/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_NFH_LOADER_HPP
#define _IMPL_NFH_LOADER_HPP

#include <list>
#include <letin/vm.hpp>
#include "dyn_lib.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplNativeFunctionHandlerLoader : public NativeFunctionHandlerLoader
      {
        std::list<priv::DynamicLibrary *> _M_libs;
      public:
        ImplNativeFunctionHandlerLoader() {}

        ~ImplNativeFunctionHandlerLoader();

        bool load(const char *file_name, std::function<NativeFunctionHandler *()> &fun);
      };
    }
  }
}

#endif
