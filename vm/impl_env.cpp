/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "impl_env.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplEnvironment::~ImplEnvironment() {}

      Function ImplEnvironment::fun(size_t i)
      { return (_M_funs != nullptr && i < _M_fun_count) ? _M_funs[i] : Function(); }

      Value ImplEnvironment::var(size_t i)
      { return (_M_vars != nullptr && i < _M_var_count) ? _M_vars[i] : Value(); }

      Function ImplEnvironment::fun(const string &name)
      { 
        auto iter = _M_fun_indexes.find(name);
        return iter != _M_fun_indexes.end() ? fun(iter->second) : Function();
      }

      Value ImplEnvironment::var(const string &name)
      { 
        auto iter = _M_var_indexes.find(name);
        return iter != _M_var_indexes.end() ? var(iter->second) : Value();
      }

      const Function *ImplEnvironment::funs() const { return _M_funs.get(); }

      Function *ImplEnvironment::funs() { return _M_funs.get(); }

      size_t ImplEnvironment::fun_count() const { return _M_fun_count; }

      const Value *ImplEnvironment::vars() const { return _M_vars.get(); }

      Value *ImplEnvironment::vars() { return _M_vars.get(); }

      size_t ImplEnvironment::var_count() const { return _M_var_count; }

      void ImplEnvironment::set_fun(size_t i, const Function &fun) { _M_funs[i] = fun; }

      void ImplEnvironment::set_var(size_t i, const Value &value) { _M_vars[i] = value; }

      void ImplEnvironment::reset()
      {
        _M_funs = unique_ptr<Function []>(nullptr);
        _M_fun_count = 0;
        _M_vars = unique_ptr<Value []>(nullptr);
        _M_var_count = 0;
        _M_fun_indexes = unordered_map<string, size_t>();
        _M_var_indexes = unordered_map<string, size_t>();
        _M_data_list_to_free = list<unique_ptr<char []>>();
      }
    }
  }
}
