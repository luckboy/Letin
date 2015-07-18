/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <atomic>
#include "lazy_eval_strategy.hpp"
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      LazyEvaluationStrategy::~LazyEvaluationStrategy() {}

      bool LazyEvaluationStrategy::pre_enter_to_fun(ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        Reference r(context->gc()->new_object(OBJECT_TYPE_LAZY_VALUE, context->regs().ac2));
        if(r.is_null()) {
          context->set_error(ERROR_OUT_OF_MEMORY);
          is_fun_result = false;
          return false;
        }
        is_fun_result = true;
        r->raw().lzv.value_type = value_type;
        r->raw().lzv.value = Value();
        r->raw().lzv.fun = i;
        for(size_t j = 0; j < context->regs().ac2; j++) {
          r->raw().lzv.args[j] = context->pushed_arg(j);
        }
        context->regs().rv = Value::lazy_value_ref(r);
        return false;
      }

      bool LazyEvaluationStrategy::post_leave_from_fun(ThreadContext *context, size_t i, int value_type)
      { return true; }

      bool LazyEvaluationStrategy::must_pre_enter_to_fun(ThreadContext *context, size_t i, int value_type)
      { return true; }

      bool LazyEvaluationStrategy::must_post_leave_from_fun(ThreadContext *context, size_t i, int value_type)
      { return false; }
    }
  }
}
