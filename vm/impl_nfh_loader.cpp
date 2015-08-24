/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
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
      { for(auto lib : _M_libs) close_dyn_lib(lib); }

      NativeFunctionHandler *ImplNativeFunctionHandlerLoader::load(const char *file_name)
      {
        DynamicLibrary *lib = open_dyn_lib(file_name);
        if(lib == nullptr) return nullptr;
        void *ptr = get_dyn_lib_symbol_addr(lib, "letin_new_native_function_handler");
        if(ptr == nullptr) {
          close_dyn_lib(lib);
          return false;
        }
        auto fun_ptr = reinterpret_cast<NativeFunctionHandler *(*)()>(ptr);
        try {
          NativeFunctionHandler *native_fun_handler = fun_ptr();
          if(native_fun_handler == nullptr) {
            close_dyn_lib(lib);
            return nullptr;
          }
          _M_libs.push_back(lib);
          return native_fun_handler;
        } catch(...) {
          close_dyn_lib(lib);
          return nullptr;
        }
      }
    }
  }
}
