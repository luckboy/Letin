/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "fun_eval_strategy.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      FunctionEvaluationStrategy::FunctionEvaluationStrategy(MemoizationCacheFactory *cache_factory, unsigned default_fun_eval_strategy) :
        _M_memo_lazy_eval_strategy(cache_factory),
        _M_default_fun_eval_strategy(default_fun_eval_strategy)
      {
        _M_eval_strategies[0] = &_M_eager_eval_strategy;
        _M_eval_strategies[EVAL_STRATEGY_LAZY] = &(_M_memo_lazy_eval_strategy.lazy_eval_strategy());
        _M_eval_strategies[EVAL_STRATEGY_MEMO] = &(_M_memo_lazy_eval_strategy.memo_eval_strategy());
        _M_eval_strategies[EVAL_STRATEGY_LAZY | EVAL_STRATEGY_MEMO] = &_M_memo_lazy_eval_strategy;
      }

      FunctionEvaluationStrategy::~FunctionEvaluationStrategy() {}

      bool FunctionEvaluationStrategy::pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->pre_enter_to_fun(vm, context, i, value_type, is_fun_result);
      }

      bool FunctionEvaluationStrategy::post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->post_leave_from_fun(vm, context, i, value_type);
      }

      bool FunctionEvaluationStrategy::must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->must_pre_enter_to_fun(vm, context, i, value_type);
      }

      bool FunctionEvaluationStrategy::must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->must_post_leave_from_fun(vm, context, i, value_type);
      }

      bool FunctionEvaluationStrategy::pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->pre_enter_to_fun_for_force(vm, context, i, value_type, is_fun_result);
      }

      bool FunctionEvaluationStrategy::post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        unsigned j = fun_eval_strategy(context->fun_info(i), _M_default_fun_eval_strategy);
        return _M_eval_strategies[j]->post_leave_from_fun_for_force(vm, context, i, value_type);
      }

      void FunctionEvaluationStrategy::set_fun_count(size_t fun_count)
      { _M_memo_lazy_eval_strategy.set_fun_count(fun_count); }

      list<MemoizationCache *> FunctionEvaluationStrategy::memo_caches()
      { return _M_memo_lazy_eval_strategy.memo_caches(); }
    }
  }
}
