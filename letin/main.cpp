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

struct GarbageCollectionFinalization
{
  ~GarbageCollectionFinalization() { finalize_gc(); }
};

int main(int argc, char **argv)
{
  if(argc < 2) {
    cerr << "Usage: " << argv[0] << " <file> [<argument> ...]" << endl;
    return 1;
  }
  initialize_gc();
  GarbageCollectionFinalization final_gc;
  unique_ptr<Loader> loader(new_loader());
  unique_ptr<Allocator> alloc(new_allocator());
  unique_ptr<GarbageCollector> gc(new_garbage_collector(alloc.get()));
  unique_ptr<NativeFunctionHandler> native_fun_handler(new DefaultNativeFunctionHandler());
  unique_ptr<VirtualMachine> vm(new_virtual_machine(loader.get(), gc.get(), native_fun_handler.get()));
  if(!vm->load(argv[1])) {
    cerr << "error: can't open file or file format is incorrect" << endl;
    return 1;
  }
  if(!vm->has_entry()) {
    cerr << "error: no entry" << endl;
    return 1;
  }
  Reference ref(vm->gc()->new_immortal_object(OBJECT_TYPE_RARRAY, argc - 2));
  for(size_t i = 2; i < argc; i++) {
    size_t arg_length = strlen(argv[i]);
    Reference arg_ref(vm->gc()->new_immortal_object(OBJECT_TYPE_IARRAY8, arg_length));
    for(size_t j = 0; j < arg_length; j++) arg_ref->set_elem(j, Value(argv[i][j]));
    ref->set_elem(i - 2, Value(arg_ref));
  }
  vector<Value> args;
  args.push_back(Value(ref));
  gc->start();
  Thread thread = vm->start(args, [](const ReturnValue &value) {
    cout << "i=" << value.i() << endl;
    cout << "f=" << value.f() << endl;
    if(value.r()->type() == OBJECT_TYPE_IARRAY8) {
      cout << "r=\"";
      for(size_t i = 0; i < value.r()->length(); i++) {
        char c = value.r()->elem(i).i();
        switch(c) {
          case '\a':
            cout << "\\a";
            break;
          case '\b':
            cout << "\\b";
            break;
          case '\t':
            cout << "\\t";
            break;
          case '\n':
            cout << "\\n";
            break;
          case '\v':
            cout << "\\v";
            break;
          case '\f':
            cout << "\\f";
            break;
          case '\r':
            cout << "\\r";
            break;
          default:
            if(isprint(c))
              cout << c;
            else
              cout << "\\" << oct << c;
            break;
        }
      }
      cout << "\"" << endl;
    } else
      cout << "r=" << value.r() << endl;
    cout << "error=" << value.error() << " (" << error_to_string(value.error()) << ")" << endl;
  });
  thread.system_thread().join();
  return 0;
}
