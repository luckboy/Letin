/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#if defined(__unix__)
#include <dlfcn.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#error "Unsupported operating system."
#endif
#include <new>
#include "dyn_lib.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace priv
    {
#if defined(__unix__)

      //
      // An implementation for Unix-like systems.
      //

      class DynamicLibrary {};

      DynamicLibrary *open_dyn_lib(const char *file_name)
      {
        void *handle = ::dlopen(file_name, RTLD_NOW);
        if(handle == nullptr) return nullptr;
        return reinterpret_cast<DynamicLibrary *>(handle);
      }

      void *get_dyn_lib_symbol_addr(DynamicLibrary *lib, const char *symbol_name)
      { return ::dlsym(reinterpret_cast<void *>(lib), symbol_name); }

      void close_dyn_lib(DynamicLibrary *lib)
      { ::dlclose(reinterpret_cast<void *>(lib)); }

#elif defined(_WIN32) || defined(_WIN64)

      //
      // An implementation for Windows.
      //

      class DynamicLibrary {};

      DynamicLibrary *open_dyn_lib(const char *file_name)
      {
        ::HMODULE hmodule = ::LoadLibrary(file_name);
        if(hmodule == nullptr) return nullptr;
        return reinterpret_cast<DynamicLibrary *>(hmodule);
      }

      void *get_dyn_lib_symbol_addr(DynamicLibrary *lib, const char *symbol_name)
      { return reinterpret_cast<void *>(::GetProcAddress(reinterpret_cast<::HMODULE>(lib), symbol_name)); }

      void close_dyn_lib(DynamicLibrary *lib)
      { ::FreeLibrary(reinterpret_cast<::HMODULE>(lib)); }

#else
#error "Unsupported operating system."
#endif
    }
  }
}
