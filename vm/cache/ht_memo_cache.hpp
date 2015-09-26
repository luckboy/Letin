/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _CACHE_HT_MEMO_CACHE_HPP
#define _CACHE_HT_MEMO_CACHE_HPP

#include <letin/vm.hpp>
#include "hash_table.hpp"
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class HashTableMemoizationCache : public MemoizationCache, public ForkHandler
      {
        struct FunctionResultCache
        {
          priv::HashTable<ArgumentList, std::int64_t> is;
          priv::HashTable<ArgumentList, double> fs;
          priv::HashTable<ArgumentList, Reference> rs;
        };

        std::unique_ptr<FunctionResultCache []> _M_fun_results;
        std::size_t _M_fun_count;
        std::size_t _M_bucket_count;
      public:
        HashTableMemoizationCache(std::size_t fun_count, std::size_t bucket_count) :
          _M_fun_results(new FunctionResultCache[fun_count]), _M_fun_count(fun_count), _M_bucket_count(bucket_count) {}

        ~HashTableMemoizationCache();

        Value fun_result(std::size_t i, int value_type, const ArgumentList &args) const;

        bool add_fun_result(std::size_t i, int value_type, const ArgumentList &args, const Value &fun_result, ThreadContext &context);

        void traverse_root_objects(std::function<void (Object *)> fun);
        
        ForkHandler *fork_handler();

        void pre_fork();

        void post_fork(bool is_child);
      };

      class HashTableMemoizationCacheFactory : public MemoizationCacheFactory
      {
        std::size_t _M_bucket_count;
      public:
        HashTableMemoizationCacheFactory(std::size_t bucket_count) : _M_bucket_count(bucket_count) {}

        ~HashTableMemoizationCacheFactory();

        MemoizationCache *new_memoization_cache(std::size_t fun_count);
      };
    }
  }
}

#endif
