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
      { return (_M_funs != nullptr && i < _M_fun_count) ? _M_funs[i] : Function(); }

      Value ImplEnvironment::var(size_t i)
      { return (_M_vars != nullptr && i < _M_var_count) ? _M_vars[i] : Value(); }

      const Function *ImplEnvironment::funs() const { return _M_funs.get(); }

      Function *ImplEnvironment::funs() { return _M_funs.get(); }

      size_t ImplEnvironment::fun_count() const { return _M_fun_count; }

      const Value *ImplEnvironment::vars() const { return _M_vars.get(); }

      Value *ImplEnvironment::vars() { return _M_vars.get(); }

      size_t ImplEnvironment::var_count() const { return _M_var_count; }

      void ImplEnvironment::set_fun(size_t i, const Function &fun) { _M_funs[i] = fun; }

      void ImplEnvironment::set_var(size_t i, const Value &value) { _M_vars[i] = value; }
    }
  }
}
