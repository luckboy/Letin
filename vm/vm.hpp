/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
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
      format::Relocation *_M_relocs;
      std::size_t _M_reloc_count;
      std::uint8_t *_M_symbols;
      std::size_t _M_symbol_count;
      std::vector<std::uint32_t> _M_symbol_offsets;
      std::size_t _M_fun_offset;
      std::size_t _M_var_offset;
    public:
      Program(std::uint32_t flags, std::size_t entry,
              format::Function *funs, std::size_t fun_count, format::Value *vars, std::size_t var_count,
              format::Instruction *code, std::size_t code_size, uint8_t *data, std::size_t data_size,
              std::set<std::uint32_t> data_addrs,
              format::Relocation *relocs, std::size_t reloc_count, std::uint8_t *symbols, std::size_t symbol_count,
              std::vector<std::uint32_t> symbol_offsets) :
        _M_flags(flags), _M_entry(entry),
        _M_funs(funs), _M_fun_count(fun_count), _M_vars(vars), _M_var_count(var_count),
        _M_code(code), _M_code_size(code_size), _M_data(data), _M_data_size(data_size),
        _M_data_addrs(data_addrs),
        _M_relocs(relocs), _M_reloc_count(reloc_count), _M_symbols(symbols), _M_symbol_count(symbol_count),
        _M_symbol_offsets(symbol_offsets),
        _M_fun_offset(0), _M_var_offset(0)
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

      const format::Relocation &reloc(std::size_t i) const { return _M_relocs[i]; }

      format::Relocation &reloc(std::size_t i) { return _M_relocs[i]; }

      std::size_t reloc_count() const { return _M_reloc_count; }

      format::Symbol *symbols(std::size_t i)
      { return reinterpret_cast<format::Symbol *>(_M_symbols + _M_symbol_offsets[i]); }

      std::size_t symbol_count() const { return _M_symbol_count; }

      bool relocate(std::size_t fun_offset, std::size_t var_offset,
                    const std::unordered_map<std::string, std::size_t> &fun_indexes,
                    const std::unordered_map<std::string, std::size_t> &var_indexes,
                    const std::unordered_map<std::string, int> &native_fun_indexes);
    private:
      bool relocate_index(std::size_t &index, std::size_t offset, const std::unordered_map<std::string, std::size_t> &indexes, const format::Relocation &reloc, std::uint8_t symbol_type);

      bool relocate_native_fun_index(int &index, const std::unordered_map<std::string, int> &indexes, const format::Relocation &reloc);

      bool get_elem_fun_index(std::size_t addr, std::size_t &index, format::Object *&data_object, bool is_native_fun_index = false);

      bool set_elem_fun_index(std::size_t addr, std::size_t index, format::Object *data_object);
    };

    struct Registers
    {
      std::uint32_t abp;
      std::uint32_t ac;
      std::uint32_t lvc;
      std::uint32_t abp2;
      std::uint32_t ac2;
      std::uint32_t sec;
      std::uint32_t nfbp;
      std::size_t fp;
      std::uint32_t ip;
      ReturnValue rv;
      std::size_t ai;
      void *gc_tmp_ptr;
      Reference tmp_r;
      bool after_leaving_flags[2];
      unsigned after_leaving_flag_index;
      std::uint32_t tmp_ac2;
      bool arg_instr_flag;
      bool cached_fun_result_flag;
      bool try_flag;
      std::uint32_t try_abp;
      std::uint32_t try_ac;
      Value try_arg2;
      Reference try_io_r;
      ReturnValue force_tmp_rv;
      Reference force_tmp_r;
      Reference force_tmp_r2;
    };

    class ThreadContext
    {
      GarbageCollector *_M_gc;
      NativeFunctionHandler *_M_native_fun_handler;
      std::thread _M_thread;
      Registers _M_regs;
      Value *_M_stack;
      std::size_t _M_stack_size;
      const Function *_M_funs;
      std::size_t _M_fun_count;
      const Value *_M_global_vars;
      std::size_t _M_global_var_count;
      RegisteredReference *_M_first_registered_r;
      RegisteredReference *_M_last_registered_r;
    public:
      ThreadContext(const VirtualMachineContext &vm_context, std::size_t stack_size = 32 * 1024);

      ~ThreadContext() { if(_M_gc != nullptr) _M_gc->delete_thread_context(this); delete[] _M_stack; }

      GarbageCollector *gc() { return _M_gc; }

      void set_gc(GarbageCollector *gc)
      {
        if(_M_gc != nullptr) _M_gc->delete_thread_context(this);
        _M_gc = gc;
        if(_M_gc != nullptr) _M_gc->add_thread_context(this);
      }

      NativeFunctionHandler *native_fun_handler() { return _M_native_fun_handler; }

      void set_native_fun_handler(NativeFunctionHandler *native_fun_handler) { _M_native_fun_handler = native_fun_handler; }

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

      const RegisteredReference *first_registered_r() const { return _M_first_registered_r; }

      RegisteredReference *&first_registered_r() { return _M_first_registered_r; }

      const RegisteredReference *last_registered_r() const { return _M_last_registered_r; }

      RegisteredReference *&last_registered_r() { return _M_last_registered_r; }

      std::size_t lvbp() const { return _M_regs.abp + _M_regs.ac + 3; }

      const Value &arg(std::size_t i) const { return _M_stack[_M_regs.abp + i]; }

      Value &arg(std::size_t i) { return _M_stack[_M_regs.abp + i]; }

      const Value &local_var(std::size_t i) const { return _M_stack[lvbp() + i]; }

      Value &local_var(std::size_t i) { return _M_stack[lvbp() + i]; }

      bool push_local_var(const Value &value)
      {
        if(_M_regs.abp2 < _M_stack_size) {
          _M_stack[_M_regs.abp2].safely_assign_for_gc(value);
          _M_regs.abp2++;
          _M_regs.sec++;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      bool push_arg(const Value &value)
      {
        if(_M_regs.abp2 + _M_regs.ac2 < _M_stack_size) {
          _M_stack[_M_regs.abp2 + _M_regs.ac2].safely_assign_for_gc(value);
          _M_regs.ac2++;
          _M_regs.sec++;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      void pop_args()
      {
        _M_regs.ac2 = 0;
        _M_regs.sec = _M_regs.abp2;
        std::atomic_thread_fence(std::memory_order_release);
      }

      const Value &pushed_arg(std::size_t i) const { return _M_stack[_M_regs.abp2 + i]; }

      Value &pushed_arg(std::size_t i) { return _M_stack[_M_regs.abp2 + i]; }

      void pop_args_and_local_vars() { _M_regs.abp2 = lvbp(); _M_regs.lvc = _M_regs.ac2 = 0; }

      bool enter_to_fun(std::size_t i);

      bool leave_from_fun();

      void in() { _M_regs.lvc = _M_regs.abp2 - lvbp(); }

      ReturnValue invoke_native_fun(VirtualMachine *vm, int nfi, ArgumentList &args);
    private:
      void set_error_without_try(int error, const Reference &r);
    public:
      void set_error(int error, const Reference &r = Reference());

      ArgumentList pushed_args() const { return ArgumentList(_M_stack + _M_regs.abp2, _M_regs.ac2); }

      void hide_args()
      {
        _M_regs.tmp_ac2 = _M_regs.ac2;
        _M_regs.abp2 += _M_regs.tmp_ac2;
        _M_regs.ac2 = 0;
      }

      void restore_abp2_and_ac2()
      {
        _M_regs.abp2 -= _M_regs.tmp_ac2;
        _M_regs.ac2 = _M_regs.tmp_ac2;
      }

      bool push_tmp_ac2()
      { return push_local_var(Value(static_cast<std::int64_t>(_M_regs.tmp_ac2))); }

      bool pop_tmp_ac2()
      {
        if(_M_regs.abp2 > 0 && _M_stack[_M_regs.abp2 - 1].type() == VALUE_TYPE_INT) {
          _M_regs.sec--;
          _M_regs.abp2--;
          std::atomic_thread_fence(std::memory_order_release);
          _M_regs.tmp_ac2 = _M_stack[_M_regs.abp2].raw().i;
          return true;
        } else
          return false;
      }

      bool push_tmp_ac2_and_after_leaving_flag1()
      { return push_local_var(Value(static_cast<std::uint32_t>(_M_regs.tmp_ac2), _M_regs.after_leaving_flags[0] ? 1 : 0)); }

      bool pop_tmp_ac2_and_after_leaving_flag1()
      {
        if(_M_regs.abp2 > 0 && _M_stack[_M_regs.abp2 - 1].type() == VALUE_TYPE_PAIR) {
          _M_regs.sec--;
          _M_regs.abp2--;
          std::atomic_thread_fence(std::memory_order_release);
          _M_regs.tmp_ac2 = static_cast<std::int32_t>(_M_stack[_M_regs.abp2].raw().p.first);
          _M_regs.after_leaving_flags[0] = (_M_stack[_M_regs.abp2].raw().p.second != 0);
          return true;
        } else
          return false;
      }

      void set_try_regs(const Value &arg2, const Reference &io_r)
      {
        _M_regs.try_flag = true;
        _M_regs.try_abp = _M_regs.abp2; _M_regs.try_ac = _M_regs.ac2;
        _M_regs.try_arg2.safely_assign_for_gc(arg2);
        _M_regs.try_io_r.safely_assign_for_gc(io_r);
        std::atomic_thread_fence(std::memory_order_release);
      }

      void set_try_regs_for_force()
      {
        _M_regs.try_abp = _M_regs.abp2; _M_regs.try_ac = _M_regs.ac2;
        std::atomic_thread_fence(std::memory_order_release);
      }

      bool push_try_regs()
      {
        if(!push_local_var(Value(_M_regs.try_flag ? 1 : 0))) return false;
        if(!push_local_var(Value(static_cast<std::int64_t>((static_cast<std::uint64_t>(_M_regs.try_abp) << 32) | _M_regs.try_ac)))) return false;
        if(!push_local_var(_M_regs.try_arg2)) return false;
        if(!push_local_var(Value(_M_regs.try_io_r))) return false;
        return true;
      }

      bool pop_try_regs()
      {
        uint32_t abp2 = _M_regs.abp2 - 4;
        if(_M_regs.abp2 >= 4 &&
            _M_stack[abp2 + 0].type() == VALUE_TYPE_INT &&
            _M_stack[abp2 + 1].type() == VALUE_TYPE_INT &&
            _M_stack[abp2 + 3].type() == VALUE_TYPE_REF) {
          _M_regs.sec -= 4;
          _M_regs.abp2 -= 4;
          std::atomic_thread_fence(std::memory_order_release);
          _M_regs.try_io_r.safely_assign_for_gc(_M_stack[_M_regs.abp2 + 3].raw().r);
          _M_regs.try_arg2.safely_assign_for_gc(_M_stack[_M_regs.abp2 + 2]);
          _M_regs.try_abp = _M_stack[_M_regs.abp2 + 1].raw().i >> 32;
          _M_regs.try_ac = _M_stack[_M_regs.abp2 + 1].raw().i & 0xffffffff;
          _M_regs.try_flag = (_M_stack[_M_regs.abp2 + 0].raw().i != 0);
          return true;
        } else
          return false;
      }

      bool push_ai()
      { 
        if(!push_local_var(Value(static_cast<int64_t>(_M_regs.ai)))) return false;
        return true;
      }

      bool pop_ai()
      {
        if(_M_regs.abp2 > 0 && _M_stack[_M_regs.abp2 - 1].type() == VALUE_TYPE_INT) {
          _M_regs.sec--;
          _M_regs.abp2--;
          _M_regs.ai = _M_stack[_M_regs.abp2].raw().i;
          return true;
        } else
          return false;
      }

      void traverse_root_objects(std::function<void (Object *)> fun);
      
      void safely_set_gc_tmp_ptr_for_gc(void *ptr)
      {
        std::atomic_thread_fence(std::memory_order_release);
        _M_regs.gc_tmp_ptr = ptr;
        std::atomic_thread_fence(std::memory_order_release);
      }
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

      void traverse_root_objects(std::function<void (Object *)> fun);
    };

    class MemoizationCache
    {
    protected:
      MemoizationCache() {}
    public:
      virtual ~MemoizationCache();

      virtual Value fun_result(std::size_t i, int value_type, const ArgumentList &args) const = 0;

      virtual bool add_fun_result(std::size_t i, int value_type, const ArgumentList &args, const Value &fun_result, ThreadContext &context) = 0;
    };

    namespace priv
    {
      inline bool is_ref_value_type_for_gc(int type)
      { return type == VALUE_TYPE_REF || type == VALUE_TYPE_CANCELED_REF || (type & ~VALUE_TYPE_LAZILY_CANCELED) == VALUE_TYPE_LAZY_VALUE_REF; }

      bool are_memoizable_fun_args(const ArgumentList &args);

      bool is_memoizable_fun_result(const Value &value);
    }
  }
}

#endif

