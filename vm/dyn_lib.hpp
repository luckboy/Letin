/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _DYN_LIB_HPP
#define _DYN_LIB_HPP

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      class DynamicLibrary;

      DynamicLibrary *open_dyn_lib(const char *file_name);

      void *get_dyn_lib_symbol_addr(DynamicLibrary *lib, const char *symbol_name);

      void close_dyn_lib(DynamicLibrary *lib);
    }
  }
}

#endif
