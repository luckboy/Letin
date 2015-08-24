/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_VM_HPP
#define _LETIN_VM_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
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

      Reference &safely_assign_for_gc(Reference r)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_ptr = r._M_ptr;
        std::atomic_thread_fence(std::memory_order_release);
        return *this;
      }
      
      bool operator==(Reference ref) const { return _M_ptr == ref._M_ptr; }

      bool operator!=(Reference ref) const { return _M_ptr != ref._M_ptr; }

      Object &operator*() const { return *_M_ptr; }

      Object *operator->() const { return _M_ptr; }
      
      bool has_nil() const { return _M_ptr == &_S_nil; }

      bool is_null() const { return _M_ptr == nullptr; }

      const Object *ptr() const { return _M_ptr; }

      Object *ptr() { return _M_ptr; }
    };

    class TupleElementType
    {
      std::int8_t _M_raw;
    public:
      TupleElementType(int type) : _M_raw(type) {}

      TupleElementType &safely_assign_for_gc(TupleElementType type)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw = type._M_raw;
        std::atomic_thread_fence(std::memory_order_release);
        return *this;
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

      TupleElement &safely_assign_for_gc(TupleElement elem)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw = elem._M_raw;
        std::atomic_thread_fence(std::memory_order_release);
        return *this;
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
          uint32_t first;
          uint32_t second;
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
      
      bool operator==(const Value &value) const;

      bool operator!=(const Value &value) const { return !(*this == value); }

      const ValueRaw &raw() const { return _M_raw; }

      ValueRaw &raw() { return _M_raw; }

      bool is_error() const { return _M_raw.type == VALUE_TYPE_ERROR; }

      bool is_unique() const;
      
      bool is_lazy() const { return (_M_raw.type & ~VALUE_TYPE_LAZILY_CANCELED) == VALUE_TYPE_LAZY_VALUE_REF; }

      bool is_lazily_canceled() const { return (_M_raw.type & VALUE_TYPE_LAZILY_CANCELED) != 0; }

      int type() const { return _M_raw.type; }

      std::int64_t i() const { return _M_raw.type == VALUE_TYPE_INT ? _M_raw.i : 0; }

      double f() const { return _M_raw.type == VALUE_TYPE_FLOAT ? _M_raw.f : 0.0; }

      Reference r() const { return _M_raw.type == VALUE_TYPE_REF ? _M_raw.r : Reference(); }
      
      void safely_assign_for_gc(const Value &value)
      {
        _M_raw.type = VALUE_TYPE_ERROR;
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw.i = value.raw().i;
        std::atomic_thread_fence(std::memory_order_release);
        _M_raw.type = value.type();
        std::atomic_thread_fence(std::memory_order_release);
      }

      TupleElementType tuple_elem_type() const { return TupleElementType(_M_raw.type); }

      TupleElement tuple_elem() const { return TupleElement(_M_raw.i); }

      bool cancel_ref()
      {
        if(_M_raw.type != VALUE_TYPE_REF) return false;
        _M_raw.type = VALUE_TYPE_CANCELED_REF;
        return true;
      }

      void lazily_cancel_ref() { _M_raw.type |= VALUE_TYPE_LAZILY_CANCELED; }
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
          int value_type;
          Value value;
          std::size_t fun;
          Value args[1];
        } lzv;
      };

      ObjectRaw() {}

      const TupleElementType *tuple_elem_types() const
      { return &(tets[length * sizeof(TupleElement)]); }

      TupleElementType *tuple_elem_types()
      { return &(tets[length * sizeof(TupleElement)]); }
    };

    class Object
    {
      ObjectRaw _M_raw;
    public:
      Object() { _M_raw.type = OBJECT_TYPE_ERROR; _M_raw.length = 0; }

      Object(int type, std::size_t length = 0) { _M_raw.type = type; _M_raw.length = length; }

      bool operator==(const Object &object) const;

      bool operator!=(const Object &object) const { return !(*this == object); }
      
      const ObjectRaw &raw() const { return _M_raw; }

      ObjectRaw &raw() { return _M_raw; }

      bool is_error() const { return _M_raw.type == OBJECT_TYPE_ERROR; }

      bool is_unique() const { return (_M_raw.type & OBJECT_TYPE_UNIQUE) != 0 && !is_error(); }

      bool is_lazy() const { return _M_raw.type == OBJECT_TYPE_LAZY_VALUE; }
      
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

      ReturnValue &operator=(const Value &value);

      ReturnValue &safely_assign_for_gc(const ReturnValue &value)
      {
        std::atomic_thread_fence(std::memory_order_release);
        *this = value;
        std::atomic_thread_fence(std::memory_order_release);
        return *this;
      }

      ReturnValue &safely_assign_for_gc(const Value &value)
      {
        std::atomic_thread_fence(std::memory_order_release);
        *this = value; 
        std::atomic_thread_fence(std::memory_order_release);
        return *this;
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
      
      virtual Function fun(const std::string &name) = 0;
      
      virtual Value var(const std::string &name) = 0;
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
      { if(is_registered) register_ref(); }

      RegisteredReference(Reference r, ThreadContext *context, bool is_registered = true) :
        Reference(r.ptr()), _M_context(context), _M_prev(nullptr), _M_next(nullptr)
      { if(is_registered) register_ref(); }

      RegisteredReference(const RegisteredReference &r) = delete;

      ~RegisteredReference();

      RegisteredReference &operator=(const RegisteredReference &r)
      {
        safely_assign_for_gc(r);
        return *this;
      }

      RegisteredReference &operator=(const Reference &r)
      {
        safely_assign_for_gc(r);
        return *this;
      }

      void register_ref();
    };

    class VirtualMachine
    {
    protected:
      Loader *_M_loader;
      GarbageCollector *_M_gc;
      NativeFunctionHandler *_M_native_fun_handler;
      EvaluationStrategy *_M_eval_strategy;

      VirtualMachine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy) :
        _M_loader(loader), _M_gc(gc), _M_native_fun_handler(native_fun_handler), _M_eval_strategy(eval_strategy) {}
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

      virtual void add_vm_context(VirtualMachineContext *context) = 0;

      virtual void delete_vm_context(VirtualMachineContext *context) = 0;

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

      Object *new_unique_pair(const Value &value1, const Value &value2, ThreadContext *context = nullptr);

      virtual void start() = 0;

      virtual void stop() = 0;

      virtual void lock() = 0;

      virtual void unlock() = 0;

      virtual std::thread &system_thread() = 0;
      
      virtual std::size_t header_size() = 0;
    };

    class NativeFunctionHandler
    {
    protected:
      NativeFunctionHandler() {}
    public:
      virtual ~NativeFunctionHandler();

      virtual ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args) = 0;
    };

    class DefaultNativeFunctionHandler : public NativeFunctionHandler
    {
    public:
      DefaultNativeFunctionHandler() {}

      ~DefaultNativeFunctionHandler();

      ReturnValue invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args);
    };

    class EvaluationStrategy
    {
    protected:
      bool _M_is_eager;

      EvaluationStrategy(bool is_eager = true) : _M_is_eager(is_eager) {}
    public:
      virtual ~EvaluationStrategy();

      virtual bool pre_enter_to_fun(ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result) = 0;

      virtual bool post_leave_from_fun(ThreadContext *context, std::size_t i, int value_type) = 0;

      virtual bool must_pre_enter_to_fun(ThreadContext *context, std::size_t i, int value_type) = 0;

      virtual bool must_post_leave_from_fun(ThreadContext *context, std::size_t i, int value_type) = 0;

      bool is_eager() const { return _M_is_eager; }
    };

    class NativeFunctionHandlerLoader
    {
    protected:
      NativeFunctionHandlerLoader() {}
    public:
      virtual ~NativeFunctionHandlerLoader();

      virtual NativeFunctionHandler *load(const char *file_name) = 0;
    };

    Loader *new_loader();

    Allocator *new_allocator();

    GarbageCollector *new_garbage_collector(Allocator *alloc);

    EvaluationStrategy *new_evaluation_strategy();

    VirtualMachine *new_virtual_machine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy);

    NativeFunctionHandlerLoader *new_native_function_handler();

    void initialize_gc();

    void finalize_gc();

    void set_temporary_root_object(ThreadContext *context, Reference r);

    inline void set_temporary_root_object(ThreadContext *context, Object *object)
    { set_temporary_root_object(context, Reference(object)); }

    std::ostream &operator<<(std::ostream &os, const Value &value);

    std::ostream &operator<<(std::ostream &os, const Object &object);

    std::ostream &operator<<(std::ostream &os, const LoadingError &error);

    const char *error_to_string(int error);
  }
}

#endif
