/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _STRATEGY_LAZY_EVAL_STRATEGY_HPP
#define _STRATEGY_LAZY_EVAL_STRATEGY_HPP

#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class LazyEvaluationStrategy : public EvaluationStrategy
      {
      public:
        LazyEvaluationStrategy() : EvaluationStrategy(false) {}

        ~LazyEvaluationStrategy();

        bool pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);

        bool must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, std::size_t i, int value_type);
      };
    }
  }
}

#endif
