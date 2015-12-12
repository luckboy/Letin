/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_ENV_HPP
#define _IMPL_ENV_HPP

#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <letin/vm.hpp>
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplEnvironment : public Environment, public VirtualMachineContext
      {
        std::unique_ptr<Function []> _M_funs;
        std::size_t _M_fun_count;
        std::unique_ptr<Value []> _M_vars;
        std::size_t _M_var_count;
        std::unique_ptr<FunctionInfo []> _M_fun_infos;
        std::unordered_map<std::string, std::size_t> _M_fun_indexes;
        std::unordered_map<std::string, std::size_t> _M_var_indexes;
        std::unordered_map<std::string, int> _M_native_fun_indexes;
        std::list<MemoizationCache *> _M_memo_caches;
        std::list<std::unique_ptr<char []>> _M_data_list_to_free;
      public:
        ImplEnvironment() { reset(); }

        ~ImplEnvironment();

        void set_fun_count(std::size_t fun_count)
        {
          _M_funs = std::unique_ptr<Function []>(new Function[fun_count]);
          _M_fun_infos = std::unique_ptr<FunctionInfo []>(new FunctionInfo[fun_count]);
          _M_fun_count = fun_count;
        }

        void set_var_count(std::size_t var_count)
        { 
          _M_vars = std::unique_ptr<Value []>(new Value[var_count]);
          _M_var_count = var_count;
        }

        void set_memo_caches(const std::list<MemoizationCache *> caches)
        { _M_memo_caches = caches; }

        Function fun(std::size_t i);

        Value var(std::size_t i);

        FunctionInfo fun_info(std::size_t i);

        Function fun(const std::string &name);

        Value var(const std::string &name);

        FunctionInfo fun_info(const std::string &name);
        
        const Function *funs() const;

        Function *funs();

        std::size_t fun_count() const;

        const Value *vars() const;

        Value *vars();

        std::size_t var_count() const;

        const FunctionInfo *fun_infos() const;

        FunctionInfo *fun_infos();

        const std::list<MemoizationCache *> &memo_caches() const;

        void set_fun(std::size_t i, const Function &fun);

        void set_var(std::size_t i, const Value &value);

        void set_fun_info(std::size_t i, const FunctionInfo &fun_info);

        void add_data_to_free(void *ptr)
        { _M_data_list_to_free.push_back(std::unique_ptr<char []>(reinterpret_cast<char *>(ptr))); }

        std::unordered_map<std::string, std::size_t> fun_indexes() { return _M_fun_indexes; }

        std::unordered_map<std::string, std::size_t> var_indexes() { return _M_var_indexes; }

        std::unordered_map<std::string, int> native_fun_indexes() { return _M_native_fun_indexes; }

        bool add_fun_index(const std::string &name, std::size_t i)
        {
          if(_M_fun_indexes.find(name) != _M_fun_indexes.end()) return false;
          _M_fun_indexes.insert(std::make_pair(name, i));
          return true;
        }

        bool add_var_index(const std::string &name, std::size_t i)
        {
          if(_M_var_indexes.find(name) != _M_var_indexes.end()) return false;
          _M_var_indexes.insert(std::make_pair(name, i));
          return true;
        }
        
        bool add_native_fun_index(const std::string &name, int i)
        {
          if(_M_native_fun_indexes.find(name) != _M_native_fun_indexes.end()) return false;
          _M_native_fun_indexes.insert(std::make_pair(name, i));
          return true;
        }

        void reset();

        void reset_without_native_fun_indexes();
      };
    }
  }
}

#endif
