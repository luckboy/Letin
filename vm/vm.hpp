/****************************************************************************
 *   Copyright (C) 2014-2015, 2019 Łukasz Szpakowski.                       *
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
#include "sem.hpp"

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
      format::FunctionInfo *_M_fun_infos;
      std::size_t _M_fun_info_count;
      std::size_t _M_fun_offset;
      std::size_t _M_var_offset;
    public:
      Program(std::uint32_t flags, std::size_t entry,
              format::Function *funs, std::size_t fun_count, format::Value *vars, std::size_t var_count,
              format::Instruction *code, std::size_t code_size, uint8_t *data, std::size_t data_size,
              std::set<std::uint32_t> data_addrs,
              format::Relocation *relocs, std::size_t reloc_count, std::uint8_t *symbols, std::size_t symbol_count,
              std::vector<std::uint32_t> symbol_offsets,
              format::FunctionInfo *fun_infos, std::size_t fun_info_count) :
        _M_flags(flags), _M_entry(entry),
        _M_funs(funs), _M_fun_count(fun_count), _M_vars(vars), _M_var_count(var_count),
        _M_code(code), _M_code_size(code_size), _M_data(data), _M_data_size(data_size),
        _M_data_addrs(data_addrs),
        _M_relocs(relocs), _M_reloc_count(reloc_count), _M_symbols(symbols), _M_symbol_count(symbol_count),
        _M_symbol_offsets(symbol_offsets),
        _M_fun_infos(fun_infos), _M_fun_info_count(fun_info_count),
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

      const format::FunctionInfo &fun_info(std::size_t i) const { return _M_fun_infos[i]; }

      format::FunctionInfo &fun_info(std::size_t i) { return _M_fun_infos[i]; }

      std::size_t fun_info_count() const { return _M_fun_info_count; }

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
      std::uint32_t ebp;
      std::uint32_t ec;
      std::uint32_t esec;
      std::uint32_t nfbp;
      std::uint32_t enfbp;
      std::size_t fp;
      std::uint32_t ip;
      ReturnValue rv;
      std::uint64_t ai;
      void *gc_tmp_ptr;
      Reference tmp_r;
      bool after_leaving_flags[2];
      unsigned after_leaving_flag_index;
      std::uint32_t tmp_ac2;
      bool arg_instr_flag;
      unsigned cached_fun_result_flag;
      bool try_flag;
      std::uint32_t try_abp;
      std::uint32_t try_ac;
      Value try_arg2;
      Reference try_io_r;
      ReturnValue force_tmp_rv;
      Reference force_tmp_r;
      Reference force_tmp_r2;
      ReturnValue force_tmp_rv2;
      Value tmp_exprs[2];
    };

    struct SavedRegisters
    {
      std::uint32_t nfbp;
      std::uint32_t enfbp;
      std::uint32_t abp;
      std::uint32_t ac;
      std::uint32_t lvc;
      std::uint32_t abp2;
      std::uint32_t ac2;
      std::size_t fp;
      std::uint32_t ip;
      bool after_leaving_flag1;
      bool after_leaving_flag2;
      unsigned after_leaving_flag_index;
      bool try_flag;
      std::uint32_t try_abp;
      std::uint32_t try_ac;
      ReturnValue force_tmp_rv;
      ReturnValue force_tmp_rv2;
      std::uint32_t sec;
      std::uint32_t ebp;
      std::uint32_t ec;
    };

    class ThreadContext
    {
      GarbageCollector *_M_gc;
      NativeFunctionHandler *_M_native_fun_handler;
      std::thread _M_thread;
      Registers _M_regs;
      Value *_M_stack;
      std::size_t _M_stack_size;
      Value * _M_expr_stack;
      std::size_t _M_expr_stack_size;
      const Function *_M_funs;
      std::size_t _M_fun_count;
      const Value *_M_global_vars;
      std::size_t _M_global_var_count;
      const FunctionInfo *_M_fun_infos;
      RegisteredReference *_M_first_registered_r;
      RegisteredReference *_M_last_registered_r;
      std::mutex _M_interruptible_fun_mutex;
      bool _M_interruptible_fun_flag;
    public:
      ThreadContext(const VirtualMachineContext &vm_context, std::size_t stack_size = 32 * 1024, std::size_t expr_stack_size = 16 * 1024);

      ~ThreadContext()
      {
        if(_M_gc != nullptr) _M_gc->delete_thread_context(this);
        if(_M_stack != nullptr) delete[] _M_stack;
        if(_M_expr_stack != nullptr) delete[] _M_expr_stack;
      }

      GarbageCollector *gc() { return _M_gc; }

      void set_gc(GarbageCollector *gc) { _M_gc = gc; }

      NativeFunctionHandler *native_fun_handler() { return _M_native_fun_handler; }

      void set_native_fun_handler(NativeFunctionHandler *native_fun_handler) { _M_native_fun_handler = native_fun_handler; }

      std::thread &system_thread() { return _M_thread; }

      void start(std::function<void ()> fun) { _M_thread = std::thread(fun); }

      const Registers &regs() const { return _M_regs; }

      Registers &regs() { return _M_regs; }

      const Value &stack_elem(std::size_t i) const { return _M_stack[i]; }

      std::size_t stack_size() const { return _M_stack_size; }

      const Value &expr_stack_elem(std::size_t i) const { return _M_expr_stack[i]; }

      std::size_t expr_stack_size() const { return _M_expr_stack_size; }

      const Function &fun(std::size_t i) const { return _M_funs[i]; }

      std::size_t fun_count() const { return _M_fun_count; }

      const Value &global_var(std::size_t i) const { return _M_global_vars[i]; }

      std::size_t global_var_count() const { return _M_global_var_count; }

      const FunctionInfo &fun_info(std::size_t i) const { return _M_fun_infos[i]; }

      const RegisteredReference *first_registered_r() const { return _M_first_registered_r; }

      RegisteredReference *&first_registered_r() { return _M_first_registered_r; }

      const RegisteredReference *last_registered_r() const { return _M_last_registered_r; }

      RegisteredReference *&last_registered_r() { return _M_last_registered_r; }

      std::size_t lvbp() const { return _M_regs.abp + _M_regs.ac + 4; }

      const Value &arg(std::size_t i) const { return _M_stack[_M_regs.abp + i]; }

      Value &arg(std::size_t i) { return _M_stack[_M_regs.abp + i]; }

      const Value &local_var(std::size_t i) const { return _M_stack[lvbp() + i]; }

      Value &local_var(std::size_t i) { return _M_stack[lvbp() + i]; }

      bool push_local_var(const Value &value)
      {
        if(_M_regs.abp2 < _M_stack_size) {
          _M_stack[_M_regs.abp2].safely_assign_for_gc(value);
          _M_regs.abp2++;
          _M_regs.ac2 = 0;
          std::atomic_thread_fence(std::memory_order_release);
          _M_regs.sec = _M_regs.abp2;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      bool push_arg(const Value &value)
      {
        if(_M_regs.abp2 + _M_regs.ac2 < _M_stack_size) {
          _M_stack[_M_regs.abp2 + _M_regs.ac2].safely_assign_for_push(value);
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
      void try_lock_and_unlock_lazy_values(std::size_t stack_elem_count);

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
        _M_regs.sec = _M_regs.abp2 + _M_regs.ac2;
        std::atomic_thread_fence(std::memory_order_release);
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
          _M_regs.ai = static_cast<uint64_t>(_M_stack[_M_regs.abp2].raw().i);
          return true;
        } else
          return false;
      }

      bool push_locked_lazy_value_ref(Reference r)
      { return push_local_var(Value::locked_lazy_value_ref(r)); }

      bool pop_locked_lazy_value_ref()
      {
        if(_M_regs.abp2 > 0 && _M_stack[_M_regs.abp2 - 1].type() == VALUE_TYPE_LOCKED_LAZY_VALUE_REF) {
          _M_regs.sec--;
          _M_regs.abp2--;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      bool get_locked_lazy_value_ref(Reference &r)
      {
        if(_M_regs.abp2 > 0 && _M_stack[_M_regs.abp2 - 1].type() == VALUE_TYPE_LOCKED_LAZY_VALUE_REF) {
          r.safely_assign_for_gc(_M_stack[_M_regs.abp2 - 1].raw().r);
          return true;
        } else
          return false;
      }

      bool push_tmp_value(const Value &value)
      { return push_local_var(value); }

      bool pop_tmp_value()
      {
        if(_M_regs.abp2 > 0) {
          _M_regs.sec--;
          _M_regs.abp2--;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      bool get_tmp_value(Value &value)
      {
        if(_M_regs.abp2 > 0) {
          value.safely_assign_for_gc(_M_stack[_M_regs.abp2 - 1]);
          return true;
        } else
          return false;
      }

      bool push_expr(Value &value)
      {
        if(_M_regs.ebp + _M_regs.ec < _M_expr_stack_size) {
          _M_expr_stack[_M_regs.ebp + _M_regs.ec].safely_assign_for_push(value);
          _M_regs.ec++;
          _M_regs.esec++;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;        
      }

      bool pop_exprs(std::size_t n)
      {
        if(_M_regs.ec >= n) {
          _M_regs.ec -= n;
          _M_regs.esec -= n;
          std::atomic_thread_fence(std::memory_order_release);
          return true;
        } else
          return false;
      }

      bool get_expr(std::size_t i, Value &value)
      {
        if(_M_regs.ec > i) {
          value.safely_assign_for_gc(_M_expr_stack[_M_regs.ebp + _M_regs.ec - i - 1]);
          std::atomic_thread_fence(std::memory_order_release);
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

      bool save_regs_and_set_regs(SavedRegisters &saved_regs);

      bool restore_regs(const SavedRegisters &saved_regs);

      RegisteredReference *first_registered_ref()
      { return _M_first_registered_r; }

      RegisteredReference *last_registered_ref()
      { return _M_last_registered_r; }
      
      RegisteredReference *prev_registered_ref(const RegisteredReference &r)
      { return r._M_prev; }

      RegisteredReference *next_registered_ref(const RegisteredReference &r)
      { return r._M_next; }

      std::mutex &interruptible_fun_mutex() { return _M_interruptible_fun_mutex; }

      bool &interruptible_fun_flag() { return _M_interruptible_fun_flag; }

      void free_stack()
      {
        if(_M_stack != nullptr) {
          delete[] _M_stack;
          _M_stack = nullptr;
        }
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

      virtual const FunctionInfo *fun_infos() const = 0;

      virtual FunctionInfo *fun_infos() = 0;

      virtual const std::list<MemoizationCache *> &memo_caches() const = 0;

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

      virtual void traverse_root_objects(std::function<void (Object *)> fun) = 0;
      
      virtual ForkHandler *fork_handler() = 0;
    };

    namespace priv
    {
      class InternalForkHandler : public ForkHandler
      {
      public:
        InternalForkHandler() {}

        ~InternalForkHandler();

        void pre_fork();

        void post_fork(bool is_child);
      };

      class DefaultNativeFunctionForkHandler : public ForkHandler
      {
      public:
        DefaultNativeFunctionForkHandler() {}

        ~DefaultNativeFunctionForkHandler();

        void pre_fork();

        void post_fork(bool is_child);
      };
      
      extern Semaphore lazy_value_mutex_sem;
      
      inline bool is_ref_value_type_for_gc(int type)
      { return type == VALUE_TYPE_REF || type == VALUE_TYPE_CANCELED_REF || (type & ~VALUE_TYPE_LAZILY_CANCELED) == VALUE_TYPE_LAZY_VALUE_REF || type == VALUE_TYPE_LOCKED_LAZY_VALUE_REF; }

      inline unsigned fun_eval_strategy(const FunctionInfo &fun_info, unsigned default_fun_eval_strategy)
      { return (default_fun_eval_strategy | fun_info.eval_strategy()) & fun_info.eval_strategy_mask() & ((MAX_EVAL_STRATEGY << 1) - 1); }
      
      bool are_memoizable_fun_args(const ArgumentList &args);

      bool is_memoizable_fun_result(const Value &value);

      void traverse_child_objects(Object &object, std::function<void (Object *)> fun);
    }
  }
}

#endif
