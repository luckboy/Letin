/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _STRATEGY_FUN_EVAL_STRATEGY_HPP
#define _STRATEGY_FUN_EVAL_STRATEGY_HPP

#include <letin/vm.hpp>
#include "eager_eval_strategy.hpp"
#include "memo_lazy_eval_strategy.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class FunctionEvaluationStrategy : public EvaluationStrategy
      {
        EagerEvaluationStrategy _M_eager_eval_strategy;
        MemoizationLazyEvaluationStrategy _M_memo_lazy_eval_strategy;
        unsigned _M_default_fun_eval_strategy;
        EvaluationStrategy *_M_eval_strategies[MAX_EVAL_STRATEGY << 1];
      public:
        FunctionEvaluationStrategy(MemoizationCacheFactory *cache_factory, unsigned default_fun_eval_strategy = 0);

        ~FunctionEvaluationStrategy();

        bool pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        void set_fun_count(std::size_t fun_count);

        std::list<MemoizationCache *> memo_caches();
      };
    }
  }
}

#endif
