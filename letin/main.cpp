/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <letin/vm.hpp>
#include "path_util.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::util;

const size_t DEFAULT_BUCKET_COUNT = 32 * 1024;

struct VirtualMachineFinalization
{
  ~VirtualMachineFinalization() { finalize_vm(); }
};

static bool find_lib(const string &lib_name, const vector<string> &lib_dirs, string &file_name)
{
  for(size_t i = 0; i < lib_dirs.size() + 1; i++) {
    struct stat stat_buf;
    file_name = (i < lib_dirs.size() ? lib_dirs[i] + PATH_SEP : string()) + unix_path_to_path(string(lib_name) + ".letin");
    if(stat(file_name.c_str(), &stat_buf) != -1) return true;
  }
  return false;
}

static bool find_native_lib(const string &native_lib_name, const vector<string> &native_lib_dirs, string &file_name)
{
  for(size_t i = 0; i < native_lib_dirs.size(); i++) {
    struct stat stat_buf;
#if defined(_WIN32) || defined(_WIN64)    
    file_name = native_lib_dirs[i] + PATH_SEP + unix_path_to_path(string(native_lib_name) + ".dll");
#else
    file_name = native_lib_dirs[i] + PATH_SEP + unix_path_to_path(string(native_lib_name) + ".so");
#endif
    if(stat(file_name.c_str(), &stat_buf) != -1) return true;
  }
  return false;
}

static void add_dirs_from_env_var(const char *var_name, vector<string> &dirs)
{
  char *var_value = getenv(var_name);
  if(var_value == nullptr) return;
  char *ptr = var_value;
  char *end = var_value + strlen(var_value);
  while(ptr != end) {
#if defined(_WIN32) || defined(_WIN64)    
    char *ptr2 = find(ptr, end, ';');
#else
    char *ptr2 = find(ptr, end, ':');
#endif
    dirs.push_back(string(ptr, ptr2));
    if(ptr2 != end) ptr = ptr2 + 1;
  }
}

static bool parse_fun_eval_strategy_string(const string &str, unsigned &fun_eval_strategy)
{
  auto iter = str.begin();
  fun_eval_strategy = 0;
  while(iter != str.end()) {
    auto iter2 = find(iter, str.end(), '+');
    if(equal(iter, iter2, "eager"))
      ;
    else if(equal(iter, iter2, "lazy"))
      fun_eval_strategy |= 1 << EVAL_STRATEGY_LAZY;
    else if(equal(iter, iter2, "memo"))
      fun_eval_strategy |= 1 << EVAL_STRATEGY_MEMO;
    else
      return false;
    if(iter2 != str.end()) iter = iter2 + 1;
  }
  return true;
}

static bool load_native_fun_handlers(NativeFunctionHandlerLoader *native_fun_handler_loader, const vector<string> &native_lib_file_names, vector<NativeFunctionHandler *> &native_fun_handlers, bool is_default_native_fun_handler)
{
  vector<function<NativeFunctionHandler *()>> native_lib_funs;
  for(auto file_name : native_lib_file_names) {
    function<NativeFunctionHandler *()> fun;
    if(!native_fun_handler_loader->load(file_name.c_str(), fun)) {
      cerr << "error: can't load native library " + file_name << endl;
      return false;
    }
  }
  if(is_default_native_fun_handler)
    native_fun_handlers.push_back(new DefaultNativeFunctionHandler());
  else
    native_fun_handlers.push_back(new EmptyNativeFunctionHandler());
  for(auto native_lib_fun : native_lib_funs) native_fun_handlers.push_back(native_lib_fun());
  return true;                      
}

EvaluationStrategy *parse_eval_strategy_string(const string &str, unique_ptr<MemoizationCacheFactory> &memo_cache_factory)
{
  auto name_begin = str.begin();
  auto name_end = find(str.begin(), str.end(), ':');
  bool are_args = (name_end != str.end());
  size_t bucket_count = DEFAULT_BUCKET_COUNT;
  unsigned default_fun_eval_strategy = 0;
  function<EvaluationStrategy *()> fun;
  bool has_args = true;
  if(equal(name_begin, name_end, "eager")) {
    fun = []() { return new_eager_evaluation_strategy(); };
    has_args = false;
  } else if(equal(name_begin, name_end, "fun")) {
    fun = [&memo_cache_factory, &bucket_count, &default_fun_eval_strategy]() {
      memo_cache_factory = unique_ptr<MemoizationCacheFactory>(new_memoization_cache_factory(bucket_count));
      return new_function_evaluation_strategy(memo_cache_factory.get(), default_fun_eval_strategy);
    };
  } else if(equal(name_begin, name_end, "lazy")) {
    fun = []() { return new_lazy_evaluation_strategy(); };
    has_args = false;
  } else if(equal(name_begin, name_end, "memo")) {
    fun = [&memo_cache_factory, &bucket_count]() {
      memo_cache_factory = unique_ptr<MemoizationCacheFactory>(new_memoization_cache_factory(bucket_count));
      return new_memoization_evaluation_strategy(memo_cache_factory.get());
    };
  } else if(equal(name_begin, name_end, "lazymemo")) {
    fun = [&memo_cache_factory, &bucket_count]() {
      memo_cache_factory = unique_ptr<MemoizationCacheFactory>(new_memoization_cache_factory(bucket_count));
      return new_memoization_lazy_evaluation_strategy(memo_cache_factory.get());
    };
  } else {
    cerr << "error: incorrect evaluation strategy" << endl;
    return nullptr;
  }
  if(are_args) {
    if(!has_args) {
      cerr << "error: evaluation strategy " << string(name_begin, name_end) << " hasn't arguments" << endl;
      return nullptr;
    }
    auto arg_list_begin = name_end + 1;
    auto arg_list_end = str.end();
    auto arg_begin = arg_list_begin;
    while(arg_begin != arg_list_end) {
      auto arg_end = find(arg_begin, arg_list_end, ',');
      auto arg_name_end = find(arg_begin, arg_end, '=');
      bool is_arg_value = (arg_name_end != arg_end);
      auto arg_value_begin = (is_arg_value ? arg_name_end + 1 : arg_name_end);
      if(equal(arg_begin, arg_name_end, "bucket_count") && is_arg_value) {
        istringstream iss(string(arg_value_begin, arg_end));
        iss >> bucket_count;
        if(iss.fail() || !iss.eof()) {
          cerr << "error: incorrect number of buckets" << endl;
          return nullptr;
        }
      } else if(equal(arg_begin, arg_name_end, "default_fes") && is_arg_value) {
        if(!parse_fun_eval_strategy_string(string(arg_value_begin, arg_end), default_fun_eval_strategy)) {
          cerr << "error: incorrect default evaluation strategy of function" << endl;
          return nullptr;
        }
      } else {
        cerr << "error: incorrect argument of evaluation strategy" << endl;
        return nullptr;
      }
      if(arg_begin != arg_list_end) arg_begin = arg_end + 1;
    }
  }
  return fun();
}

int main(int argc, char **argv)
{
  try {
    vector<string> lib_dirs;
    list<string> lib_names;
    vector<string> native_lib_dirs;
    list<string> native_lib_names;
    string eval_strategy_string;
    bool is_default_native_fun_handler = true;
    int c;
    opterr = 0;
    while((c = getopt(argc, argv, "e:hl:L:n:N:x")) != -1) {
      switch(c) {
        case 'e':
          eval_strategy_string = string(optarg);
          break;
        case 'h':
          cout << "Usage: " << argv[0] << " [<option> ...] <program file> [<argument> ...]" << endl;
          cout << endl;
          cout << "Options:" << endl;
          cout << "  -e <evaluation strategy>      set the evaluation strategy" << endl;
          cout << "  -h                            display this text" << endl;
          cout << "  -l <library>                  add the library" << endl;
          cout << "  -L <directory>                add the directory to library directories" << endl;
          cout << "  -n <native library>           add the native library" << endl;
          cout << "  -N <directory>                add the directory to native library directories" << endl;
          cout << "  -x                            don't use the default native function handler" << endl;
          cout << endl;
          cout << "Evaluation strategies:" << endl;
          cout << "  eager                         use the strategy of eager evaluation" << endl;
          cout << "  fun[:<argument>,...]          use evaluation strategies of functions (default)" << endl;
          cout << "  lazy                          use the strategy of lazy evaluation" << endl;
          cout << "  memo[:<argument>,...]         use the strategy of eager evaluation with" << endl;
          cout << "                                memoization" << endl;
          cout << "  memolazy[:<argument>,...]     use the strategy of lazy evaluation with" << endl;
          cout << "                                memoization" << endl;
          cout << endl;
          cout << "Arguments for some evaluation strategies:" << endl;
          cout << "  bucket_count=<number>         the number of buckets for a hash table of" << endl;
          cout << "                                memoization (default: " << DEFAULT_BUCKET_COUNT << ")" << endl;
          cout << "  default_fes=<feature>+...     the default evaluation strategy of functions" << endl;
          cout << "                                (default: eager)" << endl;
          cout << endl;
          cout << "Features of evaluation strategy:" << endl;
          cout << "  eager, lazy, memo" << endl;
          cout << endl;
          cout << "Environment variables:" << endl;
          cout << "  LETIN_LIB_PATH                library directories" << endl;
          cout << "  LETIN_NATIVE_LIB_PATH         native library directories" << endl;
          return 0;
        case 'l':
          lib_names.push_back(string(optarg));
          break;
        case 'L':
          lib_dirs.push_back(string(optarg));
          break;
        case 'n':
          native_lib_names.push_back(string(optarg));
          break;
        case 'N':
          native_lib_dirs.push_back(string(optarg));
          break;
        case 'x':
          is_default_native_fun_handler = false;
          break;
        default:
          cerr << "error: incorrect option -" << static_cast<char>(optopt) << endl;
          return 1;
      }
    }
    add_dirs_from_env_var("LETIN_LIB_PATH", lib_dirs);
    add_dirs_from_env_var("LETIN_NATIVE_LIB_PATH", native_lib_dirs);
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
    vector<string> native_lib_file_names;
    for(auto native_lib_name : native_lib_dirs) {
      string native_lib_file_name;
      if(!find_native_lib(native_lib_name, native_lib_dirs, native_lib_file_name)) {
        cerr << "error: not found native library " + native_lib_name << endl;
        return 1;
      }
      native_lib_file_names.push_back(native_lib_file_name);
    }
    initialize_vm();
    VirtualMachineFinalization final;
    unique_ptr<NativeFunctionHandlerLoader> native_fun_handler_loader;
    vector<NativeFunctionHandler *> native_fun_handlers;
    if(!load_native_fun_handlers(native_fun_handler_loader.get(), native_lib_file_names, native_fun_handlers, is_default_native_fun_handler)) return 1;
    unique_ptr<Loader> loader(new_loader());
    unique_ptr<Allocator> alloc(new_allocator());
    unique_ptr<GarbageCollector> gc(new_garbage_collector(alloc.get()));
    unique_ptr<NativeFunctionHandler> native_fun_handler(new MultiNativeFunctionHandler(native_fun_handlers));
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
