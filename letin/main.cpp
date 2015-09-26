/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <unistd.h>
#include <vector>
#include <letin/vm.hpp>
#include "path_util.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::util;

struct VirtualMachineFinalization
{
  ~VirtualMachineFinalization() { finalize_vm(); }
};

static bool find_lib(const string &lib_name, vector<string> lib_dirs, string &file_name)
{
  for(size_t i = 0; i < lib_dirs.size() + 1; i++) {
    struct stat stat_buf;
    file_name = (i < lib_dirs.size() ? lib_dirs[i] + PATH_SEP : string()) + unix_path_to_path(string(lib_name) + ".letin");
    if(stat(file_name.c_str(), &stat_buf) != -1) return true;
  }
  return false;
}

int main(int argc, char **argv)
{
  try {
    vector<string> lib_dirs;
    list<string> lib_names;
    int c;
    opterr = 0;
    while((c = getopt(argc, argv, "hl:L:")) != -1) {
      switch(c) {
        case 'h':
          cout << "Usage: " << argv[0] << " [<option> ...] <program file> [<argument> ...]" << endl;
          cout << endl;
          cout << "Options:" << endl;
          cout << "  -h                    display this text" << endl;
          cout << "  -l <library>          add the library" << endl;
          cout << "  -L <directory>        add the directory to library directories" << endl;
          break;
        case 'l':
          lib_names.push_back(string(optarg));
          break;
        case 'L':
          lib_dirs.push_back(string(optarg));
          break;
        default:
          cerr << "error: incorrect option -" << static_cast<char>(optopt) << endl;
          return 1;
      }
    }
    if(argc < optind + 1) {
      cerr << "error: no program file" << endl;
      return 1;
    }
    vector<string> file_names;
    for(auto lib_name : lib_names) {
      string file_name;
      if(!find_lib(lib_name, lib_dirs, file_name)) {
        cerr << "error: not found library " + lib_name << endl;
        return 1;
      }
      file_names.push_back(file_name);
    }
    file_names.push_back(argv[optind]);
    initialize_vm();
    VirtualMachineFinalization final;
    unique_ptr<Loader> loader(new_loader());
    unique_ptr<Allocator> alloc(new_allocator());
    unique_ptr<GarbageCollector> gc(new_garbage_collector(alloc.get()));
    unique_ptr<NativeFunctionHandler> native_fun_handler(new DefaultNativeFunctionHandler());
    unique_ptr<EvaluationStrategy> eval_strategy(new_evaluation_strategy());
    unique_ptr<VirtualMachine> vm(new_virtual_machine(loader.get(), gc.get(), native_fun_handler.get(), eval_strategy.get()));
    list<LoadingError> errors;
    if(!vm->load(file_names, &errors)) {
      for(auto error : errors)
        cerr << "error: " << file_names[error.pair_index()] << ": " << error << endl;
      return 1;
    }
    if(!vm->has_entry()) {
      cerr << "error: no entry" << endl;
      return 1;
    }
    Reference ref(vm->gc()->new_immortal_object(OBJECT_TYPE_RARRAY, argc - (optind + 1)));
    for(int i = optind + 1; i < argc; i++) {
      size_t arg_length = strlen(argv[i]);
      Reference arg_ref(vm->gc()->new_immortal_object(OBJECT_TYPE_IARRAY8, arg_length));
      for(size_t j = 0; j < arg_length; j++) arg_ref->set_elem(j, Value(argv[i][j]));
      ref->set_elem(i - (optind + 1), Value(arg_ref));
    }
    vector<Value> args;
    args.push_back(Value(ref));
    gc->start();
    bool is_unique_result = false;
    if(vm->env().fun(vm->entry()).arg_count() == 2) {
      Reference unique_io_ref(vm->gc()->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
      args.push_back(unique_io_ref);
      is_unique_result = true;
    }
    int status = 0;
    Thread thread = vm->start(args, [is_unique_result, &status](const ReturnValue & value) {
      if(!is_unique_result) {
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
      } else {
        if(value.r()->type() == (OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) && value.r()->length() == 2 &&
            value.r()->elem(0).type() == VALUE_TYPE_INT &&
        value.r()->elem(1).type() == VALUE_TYPE_REF && value.r()->elem(1).r()->type() == (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE)) {
          status = value.r()->elem(0).i();
        } else {
          if(value.error() == ERROR_SUCCESS)
            cerr << "error: result of entry function is incorrect" << endl;
          else
            cerr << "error: " << error_to_string(value.error()) << endl;
          status = 255;
        }
      }
    });
    thread.system_thread().join();
    gc->stop();
    return status;
  } catch(bad_alloc &) {
    cerr << "error: out of memory" << endl;
    return 1;
  }
}
