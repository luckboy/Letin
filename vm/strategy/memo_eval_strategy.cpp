/****************************************************************************
 *   Copyright (C) 2015, 2019 Łukasz Szpakowski.                            *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "memo_eval_strategy.hpp"
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      //
      // Static inline functions.
      //

      static inline bool fully_force(VirtualMachine *vm, ThreadContext *context, Value &value)
      {
        SavedRegisters saved_regs;
        if(!context->save_regs_and_set_regs(saved_regs)) {
          context->set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        int error = vm->fully_force(context, value);
        if(!context->restore_regs(saved_regs)) {
          context->set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(error != ERROR_SUCCESS) {
          context->set_error(error, user_exception_ref(context), false);
          return false;
        }
        return true;
      }

      static inline bool fully_force_return_value(VirtualMachine *vm, ThreadContext *context)
      {
        SavedRegisters saved_regs;
        if(!context->save_regs_and_set_regs(saved_regs)) {
          context->set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        int error = vm->fully_force_return_value(context);
        if(!context->restore_regs(saved_regs)) {
          context->set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(error != ERROR_SUCCESS) {
          context->set_error(error, user_exception_ref(context), false);
          return false;
        }
        return true;
      }

      //
      // A MemoizationEvaluationStrategy class.
      //

      void MemoizationEvaluationStrategy::ImplForkHandler::pre_fork()
      { 
        if(_M_eval_strategy->_M_cache.get() != nullptr)
          _M_eval_strategy->_M_cache->fork_handler()->pre_fork();
      }

      void MemoizationEvaluationStrategy::ImplForkHandler::post_fork(bool is_child)
      { 
        if(_M_eval_strategy->_M_cache.get() != nullptr)
          _M_eval_strategy->_M_cache->fork_handler()->post_fork(is_child);
      }

      MemoizationEvaluationStrategy::~MemoizationEvaluationStrategy()
      { delete_fork_handler(FORK_HANDLER_PRIO_EVAL_STRATEGY, &_M_impl_fork_handler); }

      bool MemoizationEvaluationStrategy::pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        ArgumentList args = context->pushed_args();
        for(size_t i = 0; i < args.length(); i++) {
          if(args[i].is_lazy()) {
            if(!fully_force(vm, context, args[i])) {
              is_fun_result = false;
              return false;
            }
          }
        }
        Value fun_result = _M_cache->fun_result(i, value_type, args);
        if(fun_result.is_error()) {
          context->regs().cached_fun_result_flag = 0;
          return true;
        }
        is_fun_result = true;
        context->regs().rv = fun_result;
        context->regs().cached_fun_result_flag = 1;
        return false;
      }

      bool MemoizationEvaluationStrategy::post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        if(context->regs().cached_fun_result_flag == 0) {
          bool result = true;
          if(context->regs().rv.raw().r->is_lazy()) {
            if(!fully_force_return_value(vm, context)) return false;
          }
          switch(value_type) {
            case VALUE_TYPE_INT:
              result = _M_cache->add_fun_result(i, value_type, context->pushed_args(), Value(context->regs().rv.raw().i), *context);
              break;
            case VALUE_TYPE_FLOAT:
              result = _M_cache->add_fun_result(i, value_type, context->pushed_args(), Value(context->regs().rv.raw().f), *context);
              break;
            case VALUE_TYPE_REF:
              result = _M_cache->add_fun_result(i, value_type, context->pushed_args(), Value(context->regs().rv.raw().r), *context);
              break;
            default:
              context->set_error(ERROR_INCORRECT_VALUE);
              return false;
          }
          if(!result) {
            context->set_error(ERROR_OUT_OF_MEMORY);
            return false;
          }
          context->regs().cached_fun_result_flag = 1;
        }
        return true;
      }

      bool MemoizationEvaluationStrategy::must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return true; }

      bool MemoizationEvaluationStrategy::must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return true; }

      void MemoizationEvaluationStrategy::set_fun_infos_and_fun_count(const FunctionInfo *fun_infos, size_t fun_count)
      { _M_cache = unique_ptr<MemoizationCache>(_M_cache_factory->new_memoization_cache(fun_count)); }

      list<MemoizationCache *> MemoizationEvaluationStrategy::memo_caches()
      {
        list<MemoizationCache *> caches;
        caches.push_back(_M_cache.get());
        return caches;
      }
    }
  }
}
