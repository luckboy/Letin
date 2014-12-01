/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_LOADER_HPP
#define _IMPL_LOADER_HPP

#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplLoader : public Loader
      {
      public:
        ImplLoader() {}
        
        ~ImplLoader();
        
        Program *load(void *ptr, std::size_t size);
      };
    }
  }
}

#endif
