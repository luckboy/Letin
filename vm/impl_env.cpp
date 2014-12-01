/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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

      const Function &ImplEnvironment::fun(size_t i) { return _M_funs[i]; }

      const Value &ImplEnvironment::var(size_t i) { return _M_vars[i]; }
      
      const unordered_map<size_t, Value> &ImplEnvironment::vars() const { return _M_vars; }
    }
  }
}
