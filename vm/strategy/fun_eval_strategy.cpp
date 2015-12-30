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
        EvaluationStrategy(false),
        _M_memo_lazy_eval_strategy(cache_factory),
        _M_default_fun_eval_strategy(default_fun_eval_strategy)
      {
        _M_eval_strategies[0] = &_M_eager_eval_strategy;
        _M_eval_strategies[EVAL_STRATEGY_LAZY] = &_M_lazy_eval_strategy;
        _M_eval_strategies[EVAL_STRATEGY_MEMO] = &(_M_memo_lazy_eval_strategy.memo_eval_strategy());
        _M_eval_strategies[EVAL_STRATEGY_LAZY | EVAL_STRATEGY_MEMO] = &_M_memo_lazy_eval_strategy;
      }

      FunctionEvaluationStrategy::~FunctionEvaluationStrategy() {}

      bool FunctionEvaluationStrategy::pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->pre_enter_to_fun(vm, context, j, value_type, is_fun_result);
      }

      bool FunctionEvaluationStrategy::post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->post_leave_from_fun(vm, context, j, value_type);
      }

      bool FunctionEvaluationStrategy::must_pre_enter_to_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->must_pre_enter_to_fun(vm, context, j, value_type);
      }

      bool FunctionEvaluationStrategy::must_post_leave_from_fun(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->must_post_leave_from_fun(vm, context, j, value_type);
      }

      bool FunctionEvaluationStrategy::pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index_for_force;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->pre_enter_to_fun_for_force(vm, context, j, value_type, is_fun_result);
      }

      bool FunctionEvaluationStrategy::post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
      {
        size_t j = _M_fun_triples.get()[i].eval_strategy_fun_index_for_force;
        unsigned k = _M_fun_triples.get()[i].eval_strategy;
        return _M_eval_strategies[k]->post_leave_from_fun_for_force(vm, context, j, value_type);
      }

      void FunctionEvaluationStrategy::set_fun_infos_and_fun_count(const FunctionInfo *fun_infos, size_t fun_count)
      { 
        _M_fun_triples = unique_ptr<FunctionTriple []>(new FunctionTriple[fun_count]);
        size_t memoized_fun_count = 0;
        for(size_t i = 0; i < fun_count; i++) {
          unsigned eval_strategy = fun_eval_strategy(fun_infos[i], _M_default_fun_eval_strategy);
          if((eval_strategy & EVAL_STRATEGY_MEMO) == 0) {
            _M_fun_triples.get()[i].eval_strategy_fun_index = i;
            _M_fun_triples.get()[i].eval_strategy_fun_index_for_force = i;
          } else {
            _M_fun_triples.get()[i].eval_strategy_fun_index = ((eval_strategy & EVAL_STRATEGY_LAZY) == 0 ? memoized_fun_count : i);
            _M_fun_triples.get()[i].eval_strategy_fun_index_for_force = memoized_fun_count;
            memoized_fun_count++;
          }
          _M_fun_triples.get()[i].eval_strategy = eval_strategy;
        }
        _M_eager_eval_strategy.set_fun_infos_and_fun_count(nullptr, 0);
        _M_lazy_eval_strategy.set_fun_infos_and_fun_count(nullptr, 0);
        _M_memo_lazy_eval_strategy.set_fun_infos_and_fun_count(nullptr, memoized_fun_count);
      }

      list<MemoizationCache *> FunctionEvaluationStrategy::memo_caches()
      { return _M_memo_lazy_eval_strategy.memo_caches(); }
    }
  }
}
