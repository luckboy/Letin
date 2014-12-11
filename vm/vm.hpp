/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _VM_HPP
#define _VM_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <thread>
#include <letin/format.hpp>
#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    class Program
    {
      std::uint32_t _M_flags;
      std::size_t _M_entry;
      format::Function *_M_funs;
      std::size_t _M_fun_count;
      format::Value *_M_vars;
      std::size_t _M_var_count;
      format::Instruction *_M_code;
      std::size_t _M_code_size;
      std::uint8_t *_M_data;
      std::size_t _M_data_size;
      std::set<std::uint32_t> _M_data_addrs;
    public:
      Program(std::uint32_t flags, std::size_t entry,
              format::Function *funs, std::size_t fun_count, format::Value *vars, std::size_t var_count,
              format::Instruction *code, std::size_t code_size, uint8_t *data, std::size_t data_size,
              std::set<std::uint32_t> data_addrs) :
        _M_flags(flags), _M_entry(entry),
        _M_funs(funs), _M_fun_count(fun_count), _M_vars(vars), _M_var_count(var_count),
        _M_code(code), _M_code_size(code_size), _M_data(data), _M_data_size(data_size),
        _M_data_addrs(data_addrs)
      {}

      std::uint32_t flags() const { return _M_flags; }
      
      std::size_t entry() const { return _M_entry; }

      const format::Function &fun(std::size_t i) const { return _M_funs[i]; }

      format::Function &fun(std::size_t i) { return _M_funs[i]; }

      std::size_t fun_count() const { return _M_fun_count; }

      const format::Value &var(std::size_t i) const { return _M_vars[i]; }

      format::Value &var(std::size_t i) { return _M_vars[i]; }

      std::size_t var_count() const { return _M_var_count; }

      format::Instruction *code() const { return _M_code; }

      std::size_t code_size() const { return _M_code_size; }

      format::Object *data(std::size_t i) { return reinterpret_cast<format::Object *>(_M_data + i); }

      std::size_t data_size() const { return _M_data_size; }
      
      const std::set<std::uint32_t>  &data_addrs() const { return _M_data_addrs; }
    };

    struct Registers
    {
      std::uint32_t abp;
      std::uint32_t ac;
      std::uint32_t lvc;
      std::uint32_t abp2;
      std::uint32_t ac2;
      std::size_t fp;
      std::uint32_t ip;
      ReturnValue rv;
      void *tmp_ptr;
      bool after_leaving_flag;
    };

    class ThreadContext
    {
      GarbageCollector *_M_gc;
      std::thread _M_thread;
      Registers _M_regs;
      Value *_M_stack;
      std::size_t _M_stack_size;
      const Function *_M_funs;
      std::size_t _M_fun_count;
      const Value *_M_global_vars;
      std::size_t _M_global_var_count;
    public:
      ThreadContext(const VirtualMachineContext &vm_context, std::size_t stack_size = 32 * 1024);

      ~ThreadContext() { if(_M_gc != nullptr) _M_gc->delete_thread_context(this); delete[] _M_stack; }

      GarbageCollector *gc() { return _M_gc; }

      void set_gc(GarbageCollector *gc)
      {
          if(_M_gc != nullptr) _M_gc->delete_thread_context(this);
          _M_gc = gc;
          _M_gc->add_thread_context(this);
      }

      std::thread &system_thread() { return _M_thread; }

      void start(std::function<void ()> fun) { _M_thread = std::thread(fun); }

      const Registers &regs() const { return _M_regs; }

      Registers &regs() { return _M_regs; }

      const Value &stack_elem(std::size_t i) const { return _M_stack[i]; }

      std::size_t stack_size() const { return _M_stack_size; }

      const Function &fun(std::size_t i) const { return _M_funs[i]; }

      std::size_t fun_count() const { return _M_fun_count; }

      const Value &global_var(std::size_t i) const { return _M_global_vars[i]; }

      std::size_t global_var_count() const { return _M_global_var_count; }

      std::size_t lvbp() const { return _M_regs.abp + _M_regs.ac + 3; }

      const Value &arg(std::size_t i) const { return _M_stack[_M_regs.abp + i]; }

      Value &arg(std::size_t i) { return _M_stack[_M_regs.abp + i]; }

      const Value &local_var(std::size_t i) const { _M_stack[lvbp() + i]; }

      Value &local_var(std::size_t i) { _M_stack[lvbp() + i]; }

      bool push_local_var(const Value &value)
      {
        if(_M_regs.abp2 < _M_stack_size) {
          _M_stack[_M_regs.abp2] = value;
          _M_regs.abp2++;
          return true;
        } else
          return false;
      }

      bool push_arg(const Value &value)
      {
        if(_M_regs.abp2 + _M_regs.ac2 < _M_stack_size) {
          _M_stack[_M_regs.abp2 + _M_regs.ac2] = value;
          _M_regs.ac2++;
          return true;
        } else
          return false;
      }

      void pop_args() { _M_regs.ac2 = 0; }

      const Value &pushed_arg(std::size_t i) const { return _M_stack[_M_regs.abp2 + i]; }

      Value &pushed_arg(std::size_t i) { return _M_stack[_M_regs.abp2 + i]; }
      
      void pop_args_and_local_vars() { _M_regs.abp2 = lvbp(); _M_regs.lvc = _M_regs.ac2 = 0; }

      bool enter_to_fun(std::size_t i);

      bool leave_from_fun();

      void in() { _M_regs.lvc = _M_regs.abp2 - lvbp(); }

      void set_error(int error);
    };

    class VirtualMachineContext
    {
    protected:
      VirtualMachineContext() {}
    public:
      virtual ~VirtualMachineContext();

      virtual const Function *funs() const = 0;

      virtual Function *funs() = 0;

      virtual std::size_t fun_count() const = 0;

      virtual const Value *vars() const = 0;

      virtual Value *vars() = 0;

      virtual std::size_t var_count() const = 0;
    };
  }
}

#endif
