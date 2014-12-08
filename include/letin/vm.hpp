/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_VM_HPP
#define _LETIN_VM_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
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
    class GarbageCollector;

    typedef format::Instruction Instruction;

    class Reference
    {
      static Object _S_nil;

      Object *_M_ptr;
    public:
      Reference() : _M_ptr(&_S_nil) {}

      Reference(Object *ptr) : _M_ptr(ptr) {}

      Reference &operator=(Object *ptr) { _M_ptr = ptr; return *this; }

      Object &operator*() const { return *_M_ptr; }

      Object *operator->() const { return _M_ptr; }
      
      bool has_nil() const { return _M_ptr == &_S_nil; }

      const Object *ptr() const { return _M_ptr; }

      Object *ptr() { return _M_ptr; }
    };

    struct ValueRaw
    {
      union
      {
        std::int64_t i;
        double f;
        Reference r;
        Function *fun;
        struct
        {
          uint32_t first;
          uint32_t second;
        } p;
      };
      int type;

      ValueRaw() {}
    };

    class Value
    {
      ValueRaw _M_raw;
    public:
      Value() { _M_raw.type = VALUE_TYPE_ERROR; }

      Value(int i) { _M_raw.type = VALUE_TYPE_INT; _M_raw.i = i; }

      Value(std::int64_t i) { _M_raw.type = VALUE_TYPE_INT; _M_raw.i = i; }

      Value(double f) { _M_raw.type = VALUE_TYPE_FLOAT; _M_raw.f = f; }

      Value(Reference r) { _M_raw.type = VALUE_TYPE_REF; _M_raw.r = r; }

      Value(Function *fun) { _M_raw.type = VALUE_TYPE_FUN; _M_raw.fun = fun; }

      Value(std::uint32_t first, std::uint32_t second)
      { _M_raw.type = VALUE_TYPE_PAIR; _M_raw.p.first = first; _M_raw.p.second = second; }

      const ValueRaw &raw() const { return _M_raw; }

      ValueRaw &raw() { return _M_raw; }

      bool is_error() const { return _M_raw.type == VALUE_TYPE_ERROR; }

      int type() const { return _M_raw.type; }

      std::int64_t i() const { return _M_raw.type == VALUE_TYPE_INT ? _M_raw.i : 0; }

      double f() const { return _M_raw.type == VALUE_TYPE_FLOAT ? _M_raw.f : 0.0; }

      Reference r() const { return _M_raw.type == VALUE_TYPE_REF ? _M_raw.r : Reference(); }
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
        Value tes[1];
      };

      ObjectRaw() {}
    };

    class Object
    {
      ObjectRaw _M_raw;
    public:
      Object() { _M_raw.type = OBJECT_TYPE_ERROR; _M_raw.length = 0; }

      Object(int type, std::size_t length) { _M_raw.type = type; _M_raw.length = length; }

      const ObjectRaw &raw() const { return _M_raw; }

      ObjectRaw &raw() { return _M_raw; }

      bool is_error() const { return _M_raw.type == OBJECT_TYPE_ERROR; }

      int type() const { return _M_raw.type; }

      Value elem(std::size_t i) const;

      bool set_elem(std::size_t i, Value &value);

      Value operator[](std::size_t i) const { elem(i); }

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
      { _M_raw.i = 0; _M_raw.f = 0.0; _M_raw.r = Reference(); _M_raw.error = 0; }

      ReturnValue(std::int64_t i, double f, Reference r, int error)
      { _M_raw.i = i; _M_raw.f = f; _M_raw.r = r; _M_raw.error = error; }

      const ReturnValueRaw &raw() const { return _M_raw; }

      ReturnValueRaw &raw() { return _M_raw; }

      std::int64_t i() const { return _M_raw.i; }

      double f() const { return _M_raw.f; }

      Reference r() const { return _M_raw.r; }
    };

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
    };

    class Environment
    {
    protected:
      Environment() {}
    public:
      virtual ~Environment();

      virtual Function fun(std::size_t i) = 0;

      virtual Value var(std::size_t i) = 0;
    };

    class VirtualMachine
    {
    protected:
      Loader *_M_loader;
      GarbageCollector *_M_gc;

      VirtualMachine(GarbageCollector *gc) : _M_gc(gc) {}
    public:
      virtual ~VirtualMachine();

      virtual bool load(void *ptr, std::size_t size) = 0;

      bool load(const char *file_name);

      virtual Thread start(std::size_t i, std::function<void (const ReturnValue &)> fun) = 0;

      Thread start(std::function<void (const ReturnValue &)> fun) { return start(entry(), fun); }

      virtual Environment &env() = 0;

      virtual bool is_entry();

      virtual std::size_t entry();

      GarbageCollector *gc() { return _M_gc; }
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
    protected:
      virtual void *allocate(std::size_t size, ThreadContext *context = nullptr) = 0;
    public:
      virtual void add_thread_context(ThreadContext *context) = 0;

      virtual void delete_thread_context(ThreadContext *context) = 0;

      virtual void add_vm_context(VirtualMachineContext *context) = 0;

      virtual void delete_vm_context(VirtualMachineContext *context) = 0;

      Object *new_object(int type, std::size_t length, ThreadContext *context = nullptr);

      virtual void start() = 0;

      virtual void stop() = 0;

      virtual void lock() = 0;

      virtual void unlock() = 0;

      virtual std::thread &system_thread() = 0;
    };

    Loader *new_loader();

    Allocator *new_allocator();

    GarbageCollector *new_garbage_collector(Allocator *alloc);

    VirtualMachine *new_virtual_machine(Loader *loader, GarbageCollector *gc);
  }
}

#endif
