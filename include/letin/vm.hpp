/****************************************************************************
 *   Copyright (C) 2014-2015, 2019 Łukasz Szpakowski.                       *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_VM_HPP
#define _LETIN_VM_HPP

#include <sys/types.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <letin/const.hpp>
#include <letin/format.hpp>

namespace letin
{
  namespace vm
  {
    class Object;
    class Function;
    class Program;
    class Loader;
    class ThreadContext;
    class VirtualMachineContext;
    class VirtualMachine;
    class GarbageCollector;
    class NativeFunctionHandler;
    class EvaluationStrategy;
    class MemoizationCache;

    typedef format::Argument Argument;
    typedef format::Instruction Instruction;

    class Reference
    {
      static Object _S_nil;

      Object *_M_ptr;
    public:
      Reference() : _M_ptr(&_S_nil) {}

      Reference(Object *ptr) : _M_ptr(ptr) {}

      Reference &operator=(Object *ptr) { _M_ptr = ptr; return *this; }

      void safely_assign_for_gc(Reference ref)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_ptr = ref._M_ptr;
        std::atomic_thread_fence(std::memory_order_release);
      }
      
      bool operator==(Reference ref) const { return _M_ptr == ref._M_ptr; }

      bool operator!=(Reference ref) const { return _M_ptr != ref._M_ptr; }

      Object &operator*() const { return *_M_ptr; }

      Object *operator->() const { return _M_ptr; }
      
      bool has_nil() const { return _M_ptr == &_S_nil; }

      bool is_null() const { return _M_ptr == nullptr; }

      Object *ptr() const { return _M_ptr; }
    };

    class TupleElementType
    {
      std::int8_t _M_raw;
    public:
      TupleElementType(int type) : _M_raw(type) {}

      void  safely_assign_for_gc(TupleElementType type)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw = type._M_raw;
        std::atomic_thread_fence(std::memory_order_release);
      }

      const std::int8_t &raw() const { return _M_raw; }

      std::int8_t &raw() { return _M_raw; }
    };

    union TupleElementRaw
    {
      std::int64_t i;
      double f;
      Reference r;

      TupleElementRaw() {}
    };

    class TupleElement
    {
      TupleElementRaw _M_raw;
    public:
      TupleElement(int i) { _M_raw.i = i; }

      TupleElement(std::int64_t i) { _M_raw.i = i; }

      TupleElement(double f) { _M_raw.f = f; }

      TupleElement(Reference r) { _M_raw.r = r; }

      void safely_assign_for_gc(TupleElement elem)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw = elem._M_raw;
        std::atomic_thread_fence(std::memory_order_release);
      }
      
      const TupleElementRaw &raw() const { return _M_raw; }

      TupleElementRaw &raw() { return _M_raw; }
    };

    struct ValueRaw
    {
      int type;
      union
      {
        std::int64_t i;
        double f;
        Reference r;
        struct
        {
          std::uint32_t first;
          std::uint32_t second;
        } p;
      };

      ValueRaw() {}
    };

    class Value
    {
      ValueRaw _M_raw;

      Value(int type, Reference r) { _M_raw.type = type; _M_raw.r = r; }
    public:
      Value() { _M_raw.type = VALUE_TYPE_ERROR; }

      Value(int i) { _M_raw.type = VALUE_TYPE_INT; _M_raw.i = i; }

      Value(std::int64_t i) { _M_raw.type = VALUE_TYPE_INT; _M_raw.i = i; }

      Value(double f) { _M_raw.type = VALUE_TYPE_FLOAT; _M_raw.f = f; }

      Value(Reference r) { _M_raw.type = VALUE_TYPE_REF; _M_raw.r = r; }

      Value(std::uint32_t first, std::uint32_t second)
      { _M_raw.type = VALUE_TYPE_PAIR; _M_raw.p.first = first; _M_raw.p.second = second; }

      Value(TupleElementType elem_type, TupleElement elem)
      { _M_raw.type = elem_type.raw(); _M_raw.i = elem.raw().i; }

      static Value lazy_value_ref(Reference r, bool is_lazily_canceled = false)
      { return Value(VALUE_TYPE_LAZY_VALUE_REF | (is_lazily_canceled ? VALUE_TYPE_LAZILY_CANCELED : 0), r); }

      static Value locked_lazy_value_ref(Reference r)
      { return Value(VALUE_TYPE_LOCKED_LAZY_VALUE_REF, r); }
      
      bool operator==(const Value &value) const;

      bool operator!=(const Value &value) const { return !(*this == value); }

      const ValueRaw &raw() const { return _M_raw; }

      ValueRaw &raw() { return _M_raw; }

      bool is_int() const { return _M_raw.type == VALUE_TYPE_INT; }

      bool is_float() const { return _M_raw.type == VALUE_TYPE_FLOAT; }

      bool is_ref() const { return _M_raw.type == VALUE_TYPE_REF; }

      bool is_error() const { return _M_raw.type == VALUE_TYPE_ERROR; }

      bool is_unique() const;
      
      bool is_canceled_ref() { return _M_raw.type == VALUE_TYPE_CANCELED_REF; }
      
      bool is_lazy() const { return (_M_raw.type & ~VALUE_TYPE_LAZILY_CANCELED) == VALUE_TYPE_LAZY_VALUE_REF; }

      bool is_lazily_canceled() const { return (_M_raw.type & VALUE_TYPE_LAZILY_CANCELED) != 0; }

      int type() const { return _M_raw.type; }

      std::int64_t i() const { return _M_raw.type == VALUE_TYPE_INT ? _M_raw.i : 0; }

      double f() const { return _M_raw.type == VALUE_TYPE_FLOAT ? _M_raw.f : 0.0; }

      Reference r() const
      { return _M_raw.type == VALUE_TYPE_REF || _M_raw.type == VALUE_TYPE_CANCELED_REF ? _M_raw.r : Reference(); }
      
      void safely_assign_for_gc(const Value &value)
      {
        if(_M_raw.r != value._M_raw.r) _M_raw.type = VALUE_TYPE_ERROR;
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw.i = value.raw().i;
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw.type = value.type();
        std::atomic_thread_fence(std::memory_order_release);
      }

      void safely_assign_for_push(const Value &value)
      { *this = value; std::atomic_thread_fence(std::memory_order_release); }

      TupleElementType tuple_elem_type() const { return TupleElementType(_M_raw.type); }

      TupleElement tuple_elem() const { return TupleElement(_M_raw.i); }

      bool cancel_ref()
      {
        if(_M_raw.type != VALUE_TYPE_REF) return false;
        _M_raw.type = VALUE_TYPE_CANCELED_REF;
        return true;
      }

      void lazily_cancel_ref() { _M_raw.type |= VALUE_TYPE_LAZILY_CANCELED; }

      std::uint64_t hash() const;
    };

    class NativeObjectTypeIdentity
    {
      int _M_x;
    public:
      NativeObjectTypeIdentity() {}
    };

    class NativeObjectType
    {
      const NativeObjectTypeIdentity *_M_ident;
    public:
      NativeObjectType(const NativeObjectTypeIdentity *ident) : _M_ident(ident) {}

      bool operator==(NativeObjectType type) const { return _M_ident == type._M_ident; }

      bool operator!=(NativeObjectType type) const { return _M_ident != type._M_ident; }
    };

    class NativeObjectFinalizator
    {
      void (*_M_fun)(const void *);
    public:
      NativeObjectFinalizator() : _M_fun(nullptr) {}

      NativeObjectFinalizator(void (*fun)(const void *)) : _M_fun(fun) {}

      void operator()(const void *ptr) const { if(_M_fun != nullptr) _M_fun(ptr); }
    };

    class NativeObjectHashFunction
    {
      std::uint64_t (*_M_fun)(const void *);
    public:
      NativeObjectHashFunction() : _M_fun(nullptr) {}

      NativeObjectHashFunction(std::uint64_t (*fun)(const void *)) : _M_fun(fun) {}

      bool operator()(const void *ptr) const { return _M_fun != nullptr ?_M_fun(ptr) : 0; }
    };

    class NativeObjectEqualFunction
    {
      bool (*_M_fun)(const void *, const void *);
    public:
      NativeObjectEqualFunction() : _M_fun(nullptr) {}

      NativeObjectEqualFunction(bool (*fun)(const void *, const void *)) : _M_fun(fun) {}

      bool operator()(const void *ptr1, const void *ptr2) const
      { return _M_fun != nullptr ? _M_fun(ptr1, ptr2) : false; }
    };

    class NativeObjectFunctions
    {
      NativeObjectFinalizator _M_finalizator;
      NativeObjectHashFunction _M_hash_fun;
      NativeObjectEqualFunction _M_equal_fun;
    public:
      NativeObjectFunctions(void (*finalizator)(const void *), std::uint64_t (*hash_fun)(const void *), bool (*equal_fun)(const void *, const void *)) :
        _M_finalizator(NativeObjectFinalizator(finalizator)),
        _M_hash_fun(NativeObjectHashFunction(hash_fun)),
        _M_equal_fun(NativeObjectEqualFunction(equal_fun)) {}

      NativeObjectFunctions(NativeObjectFinalizator finalizator, NativeObjectHashFunction hash_fun, NativeObjectEqualFunction equal_fun) :
        _M_finalizator(finalizator), _M_hash_fun(hash_fun), _M_equal_fun(equal_fun) {}

      NativeObjectFinalizator finalizator() const { return _M_finalizator; }

      NativeObjectHashFunction hash_fun() const { return _M_hash_fun; }

      NativeObjectEqualFunction equal_fun() const { return _M_equal_fun; }
    };

    class NativeObjectClass
    {
      const NativeObjectFunctions *_M_funs;
    public:
      NativeObjectClass() : _M_funs(nullptr) {}

      NativeObjectClass(const NativeObjectFunctions *funs) : _M_funs(funs) {}

      bool operator==(NativeObjectClass clazz) const { return _M_funs == clazz._M_funs; }

      bool operator!=(NativeObjectClass clazz) const { return _M_funs != clazz._M_funs; }

      NativeObjectFinalizator finalizator() const
      { return _M_funs != nullptr ? _M_funs->finalizator() : NativeObjectFinalizator(); }

      NativeObjectHashFunction hash_fun() const
      { return _M_funs != nullptr ? _M_funs->hash_fun() : NativeObjectHashFunction(); }

      NativeObjectEqualFunction equal_fun() const
      { return _M_funs != nullptr ? _M_funs->equal_fun() : NativeObjectEqualFunction(); }
    };

    class LazyValueMutex
    {
      static thread_local std::size_t _S_locked_mutex_count;
      
      std::mutex _M_mutex;
    public:
      LazyValueMutex() {}

      void lock();

      bool try_lock();

      void unlock();
    };
    
    struct ObjectRaw
    {
      int type;
      std::size_t length;
      union
      {
        std::int8_t is8[1];
        std::int16_t is16[1];
        std::int32_t is32[1];
        std::int64_t is64[1];
        float sfs[1];
        double dfs[1];
        Reference rs[1];
        TupleElement tes[1];
        TupleElementType tets[1];
        struct {
          LazyValueMutex mutex;
          short value_type;
          bool must_be_shared;
          Value value;
          std::size_t fun;
          Value args[1];
        } lzv;
        struct {
          NativeObjectType type;
          NativeObjectClass clazz;
          std::uint8_t bs[1];
        } ntvo;
        std::uint8_t bs[1];
      };

      ObjectRaw() {}

      const TupleElementType *tuple_elem_types() const
      { return &(tets[length * sizeof(TupleElement)]); }

      TupleElementType *tuple_elem_types()
      { return &(tets[length * sizeof(TupleElement)]); }
    };

    class Object
    {
      union
      {
        struct
        {
          int _M_type;
          std::size_t _M_length;
        };
        ObjectRaw _M_raw;
      };
    public:
      constexpr Object() : _M_type(OBJECT_TYPE_ERROR), _M_length(0) {}

      Object(int type, std::size_t length = 0) { _M_raw.type = type; _M_raw.length = length; }

      bool operator==(const Object &object) const;

      bool operator!=(const Object &object) const { return !(*this == object); }
      
      const ObjectRaw &raw() const { return _M_raw; }

      ObjectRaw &raw() { return _M_raw; }

      bool is_iarray8() const { return _M_raw.type == OBJECT_TYPE_IARRAY8; }

      bool is_iarray16() const { return _M_raw.type == OBJECT_TYPE_IARRAY16; }

      bool is_iarray32() const { return _M_raw.type == OBJECT_TYPE_IARRAY32; }

      bool is_iarray64() const { return _M_raw.type == OBJECT_TYPE_IARRAY64; }

      bool is_sfiarray() const { return _M_raw.type == OBJECT_TYPE_SFARRAY; }

      bool is_dfiarray() const { return _M_raw.type == OBJECT_TYPE_DFARRAY; }

      bool is_rarray() const { return _M_raw.type == OBJECT_TYPE_RARRAY; }

      bool is_tuple() const { return _M_raw.type == OBJECT_TYPE_TUPLE; }

      bool is_io() const { return _M_raw.type == OBJECT_TYPE_IO; }

      bool is_unique_iarray8() const { return _M_raw.type == (OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE); }

      bool is_unique_iarray16() const { return _M_raw.type == (OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE); }

      bool is_unique_iarray32() const { return _M_raw.type == (OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE); }

      bool is_unique_iarray64() const { return _M_raw.type == (OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE); }

      bool is_unique_sfiarray() const { return _M_raw.type == (OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE); }

      bool is_unique_dfiarray() const { return _M_raw.type == (OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE); }

      bool is_unique_rarray() const { return _M_raw.type == (OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE); }

      bool is_unique_tuple() const { return _M_raw.type == (OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE); }

      bool is_unique_io() const { return _M_raw.type == (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE); }
      
      bool is_error() const { return _M_raw.type == OBJECT_TYPE_ERROR; }

      bool is_unique() const { return (_M_raw.type & OBJECT_TYPE_UNIQUE) != 0 && !is_error(); }

      bool is_lazy() const { return _M_raw.type == OBJECT_TYPE_LAZY_VALUE; }

      bool is_native(NativeObjectType type) const
      { return (_M_raw.type & ~OBJECT_TYPE_UNIQUE) == OBJECT_TYPE_NATIVE_OBJECT ? _M_raw.ntvo.type == type : false; }

      int type() const { return _M_raw.type; }

      Value elem(std::size_t i) const;

      bool set_elem(std::size_t i, const Value &value);

      Value elem_for_i64(std::int64_t i) const
      {
        if(static_cast<std::uint64_t>(i) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return Value();
        return elem(i);
      }

      bool set_elem_for_i64(std::int64_t i, const Value &value)
      {
        if(static_cast<std::uint64_t>(i) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return false;
        return set_elem(i, value);
      }

      Value operator[](std::size_t i) const { return elem(i); }

      std::size_t length() const { return _M_raw.length; }

      std::uint64_t hash() const;
    };

    struct ReturnValueRaw
    {
      int64_t i;
      double f;
      Reference r;
      int error;
    };

    class ReturnValue
    {
      ReturnValueRaw _M_raw;
    public:
      ReturnValue()
      { _M_raw.i = 0; _M_raw.f = 0.0; _M_raw.r = Reference(); _M_raw.error = ERROR_SUCCESS; }

      ReturnValue(std::int64_t i, double f, Reference r, int error)
      { _M_raw.i = i; _M_raw.f = f; _M_raw.r = r; _M_raw.error = error; }

      ReturnValue(const Value &value) { *this = value; }

      static ReturnValue error(int error) { return ReturnValue(0, 0.0, Reference(), error); }
      
      static ReturnValue error(int error, Reference r) { return ReturnValue(0, 0.0, r, error); }

      ReturnValue &operator=(const Value &value);

      void safely_assign_for_gc(const ReturnValue &value)
      {
        std::atomic_thread_fence(std::memory_order_release);
        *this = value;
        std::atomic_thread_fence(std::memory_order_release);
      }

      void safely_assign_for_gc(const Value &value)
      {
        std::atomic_thread_fence(std::memory_order_release);
        *this = value; 
        std::atomic_thread_fence(std::memory_order_release);
      }

      bool operator==(const ReturnValue &value) const
      { return _M_raw.i == value._M_raw.i && _M_raw.f == value._M_raw.f && _M_raw.r == value._M_raw.r && _M_raw.error == value._M_raw.error; }

      bool operator!=(const ReturnValue &value) const { return !(*this == value); }

      const ReturnValueRaw &raw() const { return _M_raw; }

      ReturnValueRaw &raw() { return _M_raw; }

      std::int64_t i() const { return _M_raw.i; }

      double f() const { return _M_raw.f; }

      Reference r() const { return _M_raw.r; }
      
      int error() const { return _M_raw.error; }
    };

    inline bool Value::is_unique() const
    { return _M_raw.type == VALUE_TYPE_REF && _M_raw.r->is_unique(); }

    struct FunctionRaw
    {
      std::size_t arg_count;
      Instruction *instrs;
      std::size_t instr_count;
    };

    class Function
    {
      FunctionRaw _M_raw;
    public:
      Function() { _M_raw.arg_count = 0; _M_raw.instrs = nullptr; _M_raw.instr_count = 0; }

      Function(std::size_t arg_count, Instruction *instrs, std::size_t instr_count)
      { _M_raw.arg_count =  arg_count; _M_raw.instrs = instrs; _M_raw.instr_count = instr_count; }

      const FunctionRaw &raw() const { return _M_raw; }

      FunctionRaw &raw() { return _M_raw; }

      bool is_error() const { return _M_raw.instrs == nullptr; }

      std::size_t arg_count() const { return _M_raw.arg_count; }

      const Instruction &instr(std::size_t i) const { return _M_raw.instrs[i]; }

      std::size_t instr_count() const { return _M_raw.instr_count; }
    };

    struct FunctionInfoRaw
    {
      unsigned eval_strategy;
      unsigned eval_strategy_mask;
    };
    
    class FunctionInfo
    {
      FunctionInfoRaw _M_raw;
    public:
      FunctionInfo() { _M_raw.eval_strategy = 0; _M_raw.eval_strategy_mask = 0xff; }

      FunctionInfo(unsigned eval_strategy, unsigned eval_strategy_mask)
      { _M_raw.eval_strategy = eval_strategy; _M_raw.eval_strategy_mask = eval_strategy_mask; }

      const FunctionInfoRaw &raw() const { return _M_raw; }

      FunctionInfoRaw &raw() { return _M_raw; }

      unsigned eval_strategy() const { return _M_raw.eval_strategy; }

      unsigned eval_strategy_mask() const { return _M_raw.eval_strategy_mask; }
    };

    class Thread
    {
      std::shared_ptr<ThreadContext> _M_context;
    public:
      Thread(ThreadContext *context);

      std::thread &system_thread();
      
      ThreadContext *context();
    };
    
    class ArgumentList
    {
      Value *_M_args;
      std::size_t _M_length;
    public:
      ArgumentList(Value *args, std::size_t length) : _M_args(args), _M_length(length) {}

      ArgumentList(std::vector<Value> &args) : _M_args(args.data()), _M_length(args.size()) {}

      const Value &operator[](std::size_t i) const { return _M_args[i]; }

      Value &operator[](std::size_t i) { return _M_args[i]; }

      std::size_t length() const { return _M_length; }
    };

    class Environment
    {
    protected:
      Environment() {}
    public:
      virtual ~Environment();

      virtual Function fun(std::size_t i) = 0;

      virtual Value var(std::size_t i) = 0;

      virtual FunctionInfo fun_info(std::size_t i) = 0;

      virtual Function fun(const std::string &name) = 0;

      virtual Value var(const std::string &name) = 0;

      virtual FunctionInfo fun_info(const std::string &name) = 0;
    };

    class LoadingError
    {
      std::size_t _M_pair_index;
      int _M_error;
      std::string _M_symbol_name;
    public:
      LoadingError(std::size_t pair_index, int error) :
        _M_pair_index(pair_index), _M_error(error) {}

      LoadingError(std::size_t pair_index, int error, const char *symbol_name) :
        _M_pair_index(pair_index), _M_error(error), _M_symbol_name(symbol_name) {}

      LoadingError(std::size_t pair_index, int error, const std::string &symbol_name) :
        _M_pair_index(pair_index), _M_error(error), _M_symbol_name(symbol_name) {}

      std::size_t pair_index() const { return _M_pair_index; }

      int error() const { return _M_error; }

      const std::string &symbol_name() const { return _M_symbol_name; }
    };

    class RegisteredReference : public Reference
    {
      friend ThreadContext;
      ThreadContext *_M_context;
      RegisteredReference *_M_prev;
      RegisteredReference *_M_next;
    public:
      RegisteredReference(ThreadContext *context, bool is_registered = true) :
        Reference(), _M_context(context), _M_prev(nullptr), _M_next(nullptr)
      { if(is_registered) register_ref(); }

      RegisteredReference(Object *ptr, ThreadContext *context, bool is_registered = true) :
        Reference(ptr), _M_context(context), _M_prev(nullptr), _M_next(nullptr)
      { if(is_registered && this->ptr() != nullptr) register_ref(); }

      RegisteredReference(Reference ref, ThreadContext *context, bool is_registered = true) :
        Reference(ref.ptr()), _M_context(context), _M_prev(nullptr), _M_next(nullptr)
      { if(is_registered && ptr() != nullptr) register_ref(); }

      RegisteredReference(const RegisteredReference &ref) = delete;

      ~RegisteredReference();

      RegisteredReference &operator=(const RegisteredReference &ref)
      {
        safely_assign_for_gc(ref);
        return *this;
      }

      RegisteredReference &operator=(const Reference &ref)
      {
        safely_assign_for_gc(ref);
        return *this;
      }

      void register_ref();
    };

    class ForkAround
    {
      ::pid_t _M_pid;
    public:
      ForkAround();

      ~ForkAround();
    };

    class ForkHandler
    {
    protected:
      ForkHandler() {}
    public:
      virtual ~ForkHandler();

      virtual void pre_fork() = 0;

      virtual void post_fork(bool is_child = false) = 0;
    };

    class MutexForkHandler : public ForkHandler
    {
      std::vector<std::mutex *> _M_mutexes;
    public:
      MutexForkHandler() {}

      ~MutexForkHandler();

      virtual void pre_fork();

      virtual void post_fork(bool is_child);
      
      const std::vector<std::mutex *> &mutexes() const { return _M_mutexes; }

      std::vector<std::mutex *> &mutexes() { return _M_mutexes; }
    };

    class InterruptibleFunctionAround
    {
      ThreadContext *_M_context;
    public:
      InterruptibleFunctionAround(ThreadContext *context);

      ~InterruptibleFunctionAround();
    };

    class VirtualMachine
    {
    protected:
      Loader *_M_loader;
      GarbageCollector *_M_gc;
      NativeFunctionHandler *_M_native_fun_handler;
      EvaluationStrategy *_M_eval_strategy;
      std::function<void ()> _M_exit_fun;

      VirtualMachine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy, std::function<void ()> exit_fun) :
        _M_loader(loader), _M_gc(gc), _M_native_fun_handler(native_fun_handler), _M_eval_strategy(eval_strategy), _M_exit_fun(exit_fun) {}
    public:
      virtual ~VirtualMachine();

      virtual bool load(const std::vector<std::pair<void *, std::size_t>> &pairs, std::list<LoadingError> *errors = nullptr, bool is_auto_freeing = false) = 0;

      bool load(void *ptr, std::size_t size, std::list<LoadingError> *errors = nullptr, bool is_auto_freeing = false);

      bool load(const std::vector<std::string> &file_names, std::list<LoadingError> *errors = nullptr);

      bool load(const char *file_name, std::list<LoadingError> *errors = nullptr);
      
      virtual Thread start(std::size_t i, const std::vector<Value> &args, std::function<void (const ReturnValue &)> fun, bool is_force = true) = 0;

      Thread start(const std::vector<Value> &args, std::function<void (const ReturnValue &)> fun, bool is_force = true) { return start(entry(), args, fun); }

      virtual Environment &env() = 0;

      virtual bool has_entry() = 0;

      virtual std::size_t entry() = 0;

      GarbageCollector *gc() { return _M_gc; }

      virtual int force(ThreadContext *context, Value &value) = 0;

      virtual int fully_force(ThreadContext *context, Value &value) = 0;

      virtual ReturnValue invoke_fun(ThreadContext *context, std::size_t i, const ArgumentList &args) = 0;

      virtual int fully_force_return_value(ThreadContext *context) = 0;

      int force_tuple_elem(ThreadContext *context, Object &object, std::size_t i);

      int fully_force_tuple_elem(ThreadContext *context, Object &object, std::size_t i);
    };

    class Loader
    {
    protected:
      Loader() {}
    public:
      virtual ~Loader();

      virtual Program *load(void *ptr, std::size_t size) = 0;
    };

    class Allocator
    {
    protected:
      Allocator() {}
    public:
      virtual ~Allocator();

      virtual void *allocate(std::size_t size) = 0;

      virtual void free(void *ptr) = 0;
    };

    class GarbageCollector
    {
    protected:
      Allocator *_M_alloc;

      GarbageCollector(Allocator *alloc) : _M_alloc(alloc) {}
    public:
      virtual ~GarbageCollector();

      virtual void collect() = 0;
    protected:
      virtual void *allocate(std::size_t size, ThreadContext *context = nullptr) = 0;

      virtual void *allocate_immortal_area(std::size_t size) = 0;
    public:
      virtual void add_thread_context(ThreadContext *context) = 0;

      virtual void delete_thread_context(ThreadContext *context) = 0;

      virtual std::size_t thread_context_count() = 0;

      virtual void add_vm_context(VirtualMachineContext *context) = 0;

      virtual void delete_vm_context(VirtualMachineContext *context) = 0;

      virtual std::size_t vm_context_count() = 0;

      Object *new_object(int type, std::size_t length, ThreadContext *context = nullptr);

      Object *new_immortal_object(int type, std::size_t length);

      Object *new_object_for_length64(int type, std::int64_t length, ThreadContext *context = nullptr)
      {
        if(static_cast<std::uint64_t>(length) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return nullptr;
        return new_object(type, length, context);
      }

      Object *new_immortal_object_for_length64(int type, std::int64_t length)
      {
        if(static_cast<std::uint64_t>(length) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return nullptr;
        return new_immortal_object(type, length);
      }

      Object *new_pair(const Value &value1, const Value &value2, ThreadContext *context = nullptr);

      Object *new_unique_pair(const Value &value1, const Value &value2, ThreadContext *context = nullptr);

      Object *new_string(const std::string &str, ThreadContext *context = nullptr);

      Object *new_string(const char *str, ThreadContext *context = nullptr);

      virtual void start() = 0;

      virtual void stop() = 0;

      virtual void lock() = 0;

      virtual void unlock() = 0;

      virtual std::thread &system_thread() = 0;
      
      virtual std::size_t header_size() = 0;

      virtual bool must_stop_from_vm_thread() = 0;
    };

    class NativeFunctionHandler
    {
    protected:
      NativeFunctionHandler() {}
    public:
      virtual ~NativeFunctionHandler();

      virtual ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args) = 0;

      virtual const char *native_fun_name(int nfi) const = 0;

      virtual int min_native_fun_index() const = 0;

      virtual int max_native_fun_index() const = 0;
    };

    class DefaultNativeFunctionHandler : public NativeFunctionHandler
    {
    public:
      DefaultNativeFunctionHandler();

      ~DefaultNativeFunctionHandler();

      ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args);

      const char *native_fun_name(int nfi) const;

      int min_native_fun_index() const;

      int max_native_fun_index() const;
    };

    class MultiNativeFunctionHandler : public NativeFunctionHandler
    {
    protected:
      struct HandlerPair
      {
        std::unique_ptr<NativeFunctionHandler> handler;
        int nfi_offset;

        HandlerPair() : handler(nullptr), nfi_offset(0) {}
      };

      std::unique_ptr<HandlerPair []> _M_handler_pairs;
      std::size_t _M_handler_pair_count;
    public:
      MultiNativeFunctionHandler(const std::vector<NativeFunctionHandler *> &handlers);

      ~MultiNativeFunctionHandler();

      ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args);

      const char *native_fun_name(int nfi) const;

      int min_native_fun_index() const;

      int max_native_fun_index() const;
    };

    class EmptyNativeFunctionHandler : public NativeFunctionHandler
    {
    public:
      EmptyNativeFunctionHandler() {}

      ~EmptyNativeFunctionHandler();

      ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args);

      const char *native_fun_name(int nfi) const;

      int min_native_fun_index() const;

      int max_native_fun_index() const;
    };

    class EvaluationStrategy
    {
    protected:
      bool _M_is_eager;

      EvaluationStrategy(bool is_eager = true) : _M_is_eager(is_eager) {}
    public:
      virtual ~EvaluationStrategy();

      virtual bool pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result) = 0;

      virtual bool post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type) = 0;

      virtual bool must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type) = 0;

      virtual bool must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type) = 0;

      virtual bool pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

      virtual bool post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

      bool is_eager() const { return _M_is_eager; }

      virtual void set_fun_infos_and_fun_count(const FunctionInfo *fun_infos, std::size_t fun_count);

      virtual std::list<MemoizationCache *> memo_caches();
    };

    class NativeFunctionHandlerLoader
    {
    protected:
      NativeFunctionHandlerLoader() {}
    public:
      virtual ~NativeFunctionHandlerLoader();

      virtual bool load(const char *file_name, std::function<NativeFunctionHandler *()> &fun) = 0;
    };

    class NativeFunction
    {
      const char *_M_name;
      std::function<ReturnValue (VirtualMachine *, ThreadContext *, ArgumentList &)> _M_fun;
    public:
      NativeFunction(const char *name, std::function<ReturnValue (VirtualMachine *, ThreadContext *, ArgumentList &)> fun) :
        _M_name(name), _M_fun(fun) {}

      const char *name() const { return _M_name; }

      std::function<ReturnValue (VirtualMachine *, ThreadContext *, ArgumentList &)> fun() const
      { return _M_fun; }
    };

    class NativeLibrary : public NativeFunctionHandler
    {
      const std::vector<NativeFunction> &_M_funs;
      ForkHandler *_M_fork_handler;
      int _M_min_nfi;
    public:
      NativeLibrary(const std::vector<NativeFunction> &funs, ForkHandler *fork_handler = nullptr, int min_nfi = MIN_UNRESERVED_NATIVE_FUN_INDEX);

      ~NativeLibrary();

      ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args);

      const char *native_fun_name(int nfi) const;

      int min_native_fun_index() const;

      int max_native_fun_index() const;
    };

    class MemoizationCacheFactory
    {
    protected:
      MemoizationCacheFactory() {}
    public:
      virtual ~MemoizationCacheFactory();

      virtual MemoizationCache *new_memoization_cache(std::size_t fun_count) = 0;
    };
    
    Loader *new_loader();

    Allocator *new_allocator();

    GarbageCollector *new_garbage_collector(Allocator *alloc);

    MemoizationCacheFactory *new_memoization_cache_factory(std::size_t bucket_count);

    EvaluationStrategy *new_evaluation_strategy();

    VirtualMachine *new_virtual_machine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy, std::function<void ()> exit_fun = []() {});

    NativeFunctionHandlerLoader *new_native_function_handler_loader();

    EvaluationStrategy *new_eager_evaluation_strategy();

    EvaluationStrategy *new_lazy_evaluation_strategy();

    EvaluationStrategy *new_memoization_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory);

    EvaluationStrategy *new_memoization_lazy_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory);

    EvaluationStrategy *new_function_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory, unsigned default_fun_eval_strategy = 0);

    void initialize_vm();

    void finalize_vm();

    void set_temporary_root_object(ThreadContext *context, Reference ref);

    inline void set_temporary_root_object(ThreadContext *context, Object *object)
    { set_temporary_root_object(context, Reference(object)); }

    NativeLibrary *new_native_library_without_throwing(const std::vector<NativeFunction> &funs, ForkHandler *fork_handler = nullptr, int min_nfi  = MIN_UNRESERVED_NATIVE_FUN_INDEX);

    int &letin_errno();

    std::uint64_t hash_value(const Value &value);

    inline std::uint64_t Value::hash() const { return hash_value(*this); }

    std::uint64_t hash_object(const Object &object);

    inline std::uint64_t Object::hash() const { return hash_object(*this); }

    bool equal_values(const Value &value1, const Value &value2);

    bool equal_objects(const Object &object1, const Object &object2);

    std::uint64_t hash_bytes(const std::uint8_t *bytes, std::size_t length);

    std::uint64_t hash_hwords(const std::uint16_t *hwords, std::size_t length);

    std::uint64_t hash_words(const std::uint32_t *words, std::size_t length);

    std::uint64_t hash_dwords(const std::uint64_t *dwords, std::size_t length);

    void add_fork_handler(int prio, ForkHandler *handler);

    void delete_fork_handler(int prio, ForkHandler *handler);

    std::ostream &operator<<(std::ostream &os, const Value &value);

    std::ostream &operator<<(std::ostream &os, const Object &object);

    std::ostream &operator<<(std::ostream &os, const LoadingError &error);

    const char *error_to_string(int error);

    Reference user_exception_ref(ThreadContext *context);
  }
}

#endif
