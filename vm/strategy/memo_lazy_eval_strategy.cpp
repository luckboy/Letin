/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "memo_lazy_eval_strategy.hpp"
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      MemoizationLazyEvaluationStrategy::~MemoizationLazyEvaluationStrategy() {}

      bool MemoizationLazyEvaluationStrategy::pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      { return _M_lazy_eval_strategy.pre_enter_to_fun(vm, context, i, value_type, is_fun_result); }

      bool MemoizationLazyEvaluationStrategy::post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return _M_lazy_eval_strategy.post_leave_from_fun(vm, context, i, value_type); }

      bool MemoizationLazyEvaluationStrategy::must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return _M_lazy_eval_strategy.must_pre_enter_to_fun(vm, context, i, value_type); }

      bool MemoizationLazyEvaluationStrategy::must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return _M_lazy_eval_strategy.must_post_leave_from_fun(vm, context, i, value_type); }

      bool MemoizationLazyEvaluationStrategy::pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      { return _M_memo_eval_strategy.pre_enter_to_fun_for_force(vm, context, i, value_type, is_fun_result); }

      bool MemoizationLazyEvaluationStrategy::post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return _M_memo_eval_strategy.post_leave_from_fun_for_force(vm, context, i, value_type); }

      void MemoizationLazyEvaluationStrategy::set_fun_count(size_t fun_count)
      { _M_memo_eval_strategy.set_fun_count(fun_count); }

      list<MemoizationCache *> MemoizationLazyEvaluationStrategy::memo_caches()
      { return _M_memo_eval_strategy.memo_caches(); }
    }
  }
}
