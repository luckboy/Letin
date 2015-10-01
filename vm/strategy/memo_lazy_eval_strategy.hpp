/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _STRATEGY_MEMO_LAZY_EVAL_STRATEGY_HPP
#define _STRATEGY_MEMO_LAZY_EVAL_STRATEGY_HPP

#include <letin/vm.hpp>
#include "lazy_eval_strategy.hpp"
#include "memo_eval_strategy.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class MemoizationLazyEvaluationStrategy : public EvaluationStrategy
      {
        LazyEvaluationStrategy _M_lazy_eval_strategy;
        MemoizationEvaluationStrategy _M_memo_eval_strategy;
      public:
        MemoizationLazyEvaluationStrategy(MemoizationCacheFactory *cache_factory) :
          EvaluationStrategy(false), _M_memo_eval_strategy(cache_factory) {}

        ~MemoizationLazyEvaluationStrategy();

        bool pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        LazyEvaluationStrategy &lazy_eval_strategy() { return _M_lazy_eval_strategy; }

        MemoizationEvaluationStrategy &memo_eval_strategy() { return _M_memo_eval_strategy; }
      };
    }
  }
}

#endif
