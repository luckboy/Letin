/****************************************************************************
 *   Copyright (C) 2015-2016 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "impl_nfh_loader.hpp"

using namespace std;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplNativeFunctionHandlerLoader::~ImplNativeFunctionHandlerLoader()
      { 
        for(auto lib : _M_libs) {
          void *finish_ptr = get_dyn_lib_symbol_addr(lib, "letin_finalize");
          if(finish_ptr != nullptr) {
            auto finish_fun_ptr = reinterpret_cast<void (*)()>(finish_ptr);
            try { finish_fun_ptr(); } catch(...) {}
          }
          close_dyn_lib(lib);
        }
      }

      bool ImplNativeFunctionHandlerLoader::load(const char *file_name, function<NativeFunctionHandler *()> &fun)
      {
        DynamicLibrary *lib = open_dyn_lib(file_name);
        if(lib == nullptr) return false;
        void *init_ptr = get_dyn_lib_symbol_addr(lib, "letin_initialize");
        if(init_ptr == nullptr) {
          close_dyn_lib(lib);
          return false;
        }
        auto init_fun_ptr = reinterpret_cast<bool (*)()>(init_ptr);
        void *new_ptr = get_dyn_lib_symbol_addr(lib, "letin_new_native_function_handler");
        if(new_ptr == nullptr) {
          close_dyn_lib(lib);
          return false;
        }
        auto new_fun_ptr = reinterpret_cast<NativeFunctionHandler *(*)()>(new_ptr);
        fun = [new_fun_ptr]() -> NativeFunctionHandler * {
          try {
            return new_fun_ptr();
          } catch(...) {
            return nullptr;
          }
        };
        try {
          if(!init_fun_ptr()) {
            close_dyn_lib(lib);
            return false;
          }
          _M_libs.push_back(lib);
          return true;
        } catch(...) {
          close_dyn_lib(lib);
          return false;
        }
      }
    }
  }
}
