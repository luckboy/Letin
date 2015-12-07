/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "ht_memo_cache.hpp"

using namespace std;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      //
      // A HashTableMemoizationCache class.
      //

      HashTableMemoizationCache::~HashTableMemoizationCache() {}

      Value HashTableMemoizationCache::fun_result(size_t i, int value_type, const ArgumentList &args) const
      {
        if(i >= _M_fun_count) return Value();
        if(!are_memoizable_fun_args(args)) return Value();
        switch(value_type) {
          case VALUE_TYPE_INT:
          {
            int64_t j;
            if(!_M_fun_results.get()[i].is.get(args, j)) return Value();
            return Value(j);
          }
          case VALUE_TYPE_FLOAT:
          {
            double f;
            if(!_M_fun_results.get()[i].fs.get(args, f)) return Value();
            return Value(f);
          }
          case VALUE_TYPE_REF:
          {
            Reference r;
            if(!_M_fun_results.get()[i].rs.get(args, r)) return Value();
            return Value(r);
          }
          default:
          {
            return Value();
          }
        }
      }

      bool HashTableMemoizationCache::add_fun_result(size_t i, int value_type, const ArgumentList &args, const Value &fun_result, ThreadContext &context)
      {
        if(i >= _M_fun_count) return true;
        if(value_type != fun_result.type()) return true;
        if(!are_memoizable_fun_args(args)) return true;
        if(!is_memoizable_fun_result(fun_result)) return true;
        switch(value_type) {
          case VALUE_TYPE_INT:
            if(!_M_fun_results.get()[i].is.set_bucket_count_for_nil_ref(_M_bucket_count, context)) return false;
            return _M_fun_results.get()[i].is.add(args, fun_result.raw().i, context);
          case VALUE_TYPE_FLOAT:
            if(!_M_fun_results.get()[i].fs.set_bucket_count_for_nil_ref(_M_bucket_count, context)) return false;
            return _M_fun_results.get()[i].fs.add(args, fun_result.raw().f, context);
          case VALUE_TYPE_REF:
            if(!_M_fun_results.get()[i].rs.set_bucket_count_for_nil_ref(_M_bucket_count, context)) return false;
            return _M_fun_results.get()[i].rs.add(args, fun_result.raw().r, context);
          default:
            return true;
        }
      }

      void HashTableMemoizationCache::traverse_root_objects(function<void (Object *)> fun)
      {
        for(size_t i = 0; i < _M_fun_count; i++) {
          if(!_M_fun_results.get()[i].is.unsafe_ref().has_nil())
            fun(_M_fun_results.get()[i].is.unsafe_ref().ptr());
          if(!_M_fun_results.get()[i].fs.unsafe_ref().has_nil())
            fun(_M_fun_results.get()[i].fs.unsafe_ref().ptr());
          if(!_M_fun_results.get()[i].rs.unsafe_ref().has_nil())
            fun(_M_fun_results.get()[i].rs.unsafe_ref().ptr());
        }
      }

      ForkHandler *HashTableMemoizationCache::fork_handler() { return this; }

      void HashTableMemoizationCache::pre_fork()
      {
        for(size_t i = 0; i < _M_fun_count; i++) {
          _M_fun_results.get()[i].is.lock();
          _M_fun_results.get()[i].fs.lock();
          _M_fun_results.get()[i].rs.lock();
        }
      }

      void HashTableMemoizationCache::post_fork(bool is_child)
      {
        size_t i = _M_fun_count; 
        while(i > 0) {
          i--;
          _M_fun_results.get()[i].rs.lock();
          _M_fun_results.get()[i].fs.lock();
          _M_fun_results.get()[i].is.lock();
        }
      }

      //
      // A HashTableMemoizationCacheFactory class.
      //

      HashTableMemoizationCacheFactory::~HashTableMemoizationCacheFactory() {}

      MemoizationCache *HashTableMemoizationCacheFactory::new_memoization_cache(size_t fun_count)
      { return new HashTableMemoizationCache(_M_bucket_count, fun_count); }
    }
  }
}
