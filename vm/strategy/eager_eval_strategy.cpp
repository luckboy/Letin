/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "eager_eval_strategy.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      EagerEvaluationStrategy::~EagerEvaluationStrategy() {}

      bool EagerEvaluationStrategy::pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      { return true; }

      bool EagerEvaluationStrategy::post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      { return true; }

      bool EagerEvaluationStrategy::must_pre_enter_to_fun(ThreadContext *context, size_t i, int value_type)
      { return false; }

      bool EagerEvaluationStrategy::must_post_leave_from_fun(ThreadContext *context, size_t i, int value_type)
      { return false; }
    }
  }
}
