/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <letin/vm.hpp>

using namespace std;
using namespace letin;
using namespace letin::vm;

int main(int argc, char **argv)
{
  if(argc < 2) {
    cerr << "Usage: " << argv[0] << " <file> [<argument> ...]" << endl;
    return 1;
  }
  unique_ptr<Loader> loader(new_loader());
  unique_ptr<Allocator> alloc(new_allocator());
  unique_ptr<GarbageCollector> gc(new_garbage_collector(alloc.get()));
  unique_ptr<NativeFunctionHandler> native_fun_handler(new DefaultNativeFunctionHandler());
  unique_ptr<VirtualMachine> vm(new_virtual_machine(loader.get(), gc.get(), native_fun_handler.get()));
  if(!vm->load(argv[1])) {
    cerr << "error: no file or incorrect file format" << endl;
    return 1;
  }
  if(!vm->has_entry()) {
    cerr << "error: no entry" << endl;
    return 1;
  }
  Reference ref(vm->gc()->new_immortal_object(OBJECT_TYPE_RARRAY, argc - 1));
  for(size_t i = 1; i < argc; i++) {
    size_t arg_length = strlen(argv[i]);
    Reference arg_ref(vm->gc()->new_immortal_object(OBJECT_TYPE_IARRAY8, arg_length));
    for(size_t j = 0; j < arg_length; j++) arg_ref->set_elem(j, Value(argv[i][j]));
    ref->set_elem(i - 1, Value(arg_ref));
  }
  vector<Value> args;
  args.push_back(Value(ref));
  Thread thread = vm->start(args, [](const ReturnValue &value) {
    cout << "i=" << value.i() << endl;
    cout << "f=" << value.f() << endl;
    if(value.r()->type() == OBJECT_TYPE_IARRAY8) {
      cout << "r=\"";
      for(size_t i = 0; i < value.r()->length(); i++) {
        char c = value.r()->elem(i).i();
        switch(c) {
          case '\a':
            cout << "\\a" << endl;
            break;
          case '\b':
            cout << "\\b" << endl;
            break;
          case '\t':
            cout << "\\t" << endl;
            break;
          case '\n':
            cout << "\\n" << endl;
            break;
          case '\v':
            cout << "\\v" << endl;
            break;
          case '\f':
            cout << "\\f" << endl;
            break;
          case '\r':
            cout << "\\r" << endl;
            break;
          default:
            if(isprint(c))
              cout << c << endl;
            else
              cout << "\\" << oct << c << endl;
            break;
        }
      }
      cout << "\"" << endl;
    } else
      cout << "r=" << value.r() << endl;
    cout << "error=" << value.error() << endl;
  });
  thread.system_thread().join();
  return 0;
}