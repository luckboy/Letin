/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_ENV_HPP
#define _IMPL_ENV_HPP

#include <cstddef>
#include <unordered_map>
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
        std::unordered_map<std::size_t, Function> _M_funs;
        std::unordered_map<std::size_t, Value> _M_vars;
      public:
        ImplEnvironment();

        ~ImplEnvironment();

        const Function &fun(std::size_t i);

        const Value &var(std::size_t i);

        const std::unordered_map<std::size_t, Value> &vars() const;
      };
    }
  }
}

#endif
