/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _STRATEGY_MEMO_EVAL_STRATEGY_HPP
#define _STRATEGY_MEMO_EVAL_STRATEGY_HPP

#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class MemoizationEvaluationStrategy : public EvaluationStrategy
      {
        MemoizationCacheFactory *_M_cache_factory;
        std::unique_ptr<MemoizationCache> _M_cache;
      public:
        MemoizationEvaluationStrategy(MemoizationCacheFactory *cache_factory)
          : _M_cache_factory(cache_factory) {}

        ~MemoizationEvaluationStrategy();

        bool pre_enter_to_fun(ThreadContext *context, std::size_t i, int value_type, bool &is_fun_result);

        bool post_leave_from_fun(ThreadContext *context, std::size_t i, int value_type);

        bool must_pre_enter_to_fun(ThreadContext *context, std::size_t i, int value_type);

        bool must_post_leave_from_fun(ThreadContext *context, std::size_t i, int value_type);

        void set_fun_count(std::size_t fun_count);

        std::list<MemoizationCache *> memo_caches();
      };
    }
  }
}

#endif
