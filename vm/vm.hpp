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
#include <unordered_map>
#include <letin/format.hpp>
#include <letin/vm.hpp>
#include "thread.hpp"

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
      Function *fun;
      std::uint32_t ip;
      ReturnValue rv;
      void *tmp_ptr;
    };

    class ThreadContext
    {
      GarbageCollector *_M_gc;
      std::thread _M_thread;
      Registers _M_regs;
      Value *_M_stack;
      std::size_t _M_stack_size;
      bool _M_is_active;
    public:
      ThreadContext(std::size_t stack_size = 32 * 1024);

      ~ThreadContext() { if(_M_gc != nullptr) _M_gc->delete_thread_context(this); delete[] _M_stack; }

      void set_gc(GarbageCollector *gc)
      {
          if(_M_gc != nullptr) _M_gc->delete_thread_context(this);
          _M_gc = gc;
          _M_gc->add_thread_context(this);
      }

      std::thread &system_thread() { return _M_thread; }

      void start(std::function<void ()> fun) { _M_thread = std::thread(fun); }

      void stop() { priv::stop_thread(_M_thread); }

      void cont() { priv::continue_thread(_M_thread); }

      const Registers &regs() const { return _M_regs; }

      Registers &regs() { return _M_regs; }

      const Value &stack_elem(std::size_t i) const { return _M_stack[i]; }

      std::size_t stack_size() const { return _M_stack_size; }

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

      bool enter_to_fun();

      bool leave_from_fun();

      void in() { _M_regs.lvc = _M_regs.abp2 - lvbp(); }
      
      bool is_active() const { _M_is_active; }
      
      void deactive() { _M_is_active = false; }
    };

    class VirtualMachineContext
    {
      VirtualMachineContext() {}
    public:
      virtual ~VirtualMachineContext();

      virtual const std::unordered_map<std::size_t, Value> &vars() const = 0;
    };
  }
}

#endif
