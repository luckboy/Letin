/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
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
      MemoizationEvaluationStrategy::~MemoizationEvaluationStrategy() {}

      bool MemoizationEvaluationStrategy::pre_enter_to_fun(ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        Value fun_value = _M_cache->fun_result(i, value_type, context->pushed_args());
        if(fun_value.is_error()) {
          context->regs().cached_fun_result_flag = false;
          return true;
        }
        is_fun_result = true;
        context->regs().rv = fun_value;
        context->regs().cached_fun_result_flag = true;
        return false;
      }

      bool MemoizationEvaluationStrategy::post_leave_from_fun(ThreadContext *context, size_t i, int value_type)
      {
        if(!context->regs().cached_fun_result_flag) {
          context->regs().cached_fun_result_flag = false;
          bool result = true;
          if(!context->regs().rv.raw().r->is_lazy()) {
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
          }
          if(!result) {
            context->set_error(ERROR_OUT_OF_MEMORY);
            return false;
          }
        }
        return true;
      }

      bool MemoizationEvaluationStrategy::must_pre_enter_to_fun(ThreadContext *context, size_t i, int value_type)
      { return true; }

      bool MemoizationEvaluationStrategy::must_post_leave_from_fun(ThreadContext *context, size_t i, int value_type)
      { return true; }

      void MemoizationEvaluationStrategy::set_fun_count(size_t fun_count)
      { _M_cache = unique_ptr<MemoizationCache>(_M_cache_factory->new_memoization_cache(fun_count)); }
    }
  }
}
