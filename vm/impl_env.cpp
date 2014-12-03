/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <utility>
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
      { return _M_funs.find(i) != _M_funs.end() ? _M_funs[i] : Function(); }

      Value ImplEnvironment::var(size_t i)
      { return _M_vars.find(i) != _M_vars.end() ? _M_vars[i] : Value(); }

      const unordered_map<size_t, Value> &ImplEnvironment::vars() const { return _M_vars; }

      void ImplEnvironment::add_fun(size_t i, const Function &fun)
      { _M_funs.insert(make_pair(i, fun)); }

      void ImplEnvironment::add_var(size_t i, const Value &value)
      { _M_vars.insert(make_pair(i, value)); }
    }
  }
}
