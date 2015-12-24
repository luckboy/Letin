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
#include <atomic>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#include <letin/const.hpp>
#include <letin/vm.hpp>
#include "alloc/new_alloc.hpp"
#include "cache/ht_memo_cache.hpp"
#include "gc/mark_sweep_gc.hpp"
#include "strategy/eager_eval_strategy.hpp"
#include "strategy/fun_eval_strategy.hpp"
#include "strategy/lazy_eval_strategy.hpp"
#include "strategy/memo_eval_strategy.hpp"
#include "strategy/memo_lazy_eval_strategy.hpp"
#include "vm/interp_vm.hpp"
#include "hash_table.hpp"
#include "impl_loader.hpp"
#include "impl_nfh_loader.hpp"
#include "priv.hpp"
#include "thread_stop_cont.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::opcode;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    //
    // Static variables.
    //

    static mutex fork_handler_list_map_mutex;
    static map<int, list<ForkHandler *>> fork_handler_list_map;
    static InternalForkHandler internal_fork_handler;
    static DefaultNativeFunctionForkHandler default_native_fun_fork_handler;
    static mutex io_stream_mutex;

    //
    // Static inline functions.
    //

    static inline size_t object_size(int type, size_t length)
    {
      size_t header_size = offsetof(ObjectRaw, is8);
      size_t elem_size;
      if(static_cast<uint64_t>(length) > static_cast<uint64_t>(numeric_limits<int64_t>::max())) return 0;
      switch(type & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          elem_size = 1;
          break;
        case OBJECT_TYPE_IARRAY16:
          elem_size = 2;
          break;
        case OBJECT_TYPE_IARRAY32:
          elem_size = 4;
          break;
        case OBJECT_TYPE_IARRAY64:
          elem_size = 8;
          break;
        case OBJECT_TYPE_SFARRAY:
          elem_size = sizeof(float);
          break;
        case OBJECT_TYPE_DFARRAY:
          elem_size = sizeof(double);
          break;
        case OBJECT_TYPE_RARRAY:
          elem_size = sizeof(Reference);
          break;
        case OBJECT_TYPE_TUPLE:
          elem_size = sizeof(TupleElement) + sizeof(TupleElementType);
          break;
        case OBJECT_TYPE_IO:
          elem_size = 0;
          break;
        case OBJECT_TYPE_LAZY_VALUE:
          header_size = offsetof(ObjectRaw, lzv.args);
          elem_size = sizeof(Value);
          break;
        case OBJECT_TYPE_NATIVE_OBJECT:
          header_size = offsetof(ObjectRaw, ntvo.bs);
          elem_size = 1;
          break;
        default:
          if((type & OBJECT_TYPE_INTERNAL) != 0)
            elem_size = 1;
          else
            return 0;
      }
      return header_size + length * elem_size;
    }

    //
    // A Reference class.
    //

    Object Reference::_S_nil;

    //
    // A Value class.
    //

    bool Value::operator==(const Value &value) const
    {
      if(_M_raw.type != value._M_raw.type) return false;
      switch(_M_raw.type) {
        case VALUE_TYPE_INT:
          return _M_raw.i == value._M_raw.i;
        case VALUE_TYPE_FLOAT:
          return _M_raw.f == value._M_raw.f;
        case VALUE_TYPE_REF:
          return _M_raw.r == value._M_raw.r;
        case VALUE_TYPE_PAIR:
          return _M_raw.p.first == value._M_raw.p.first && _M_raw.p.first == value._M_raw.p.first;
        case VALUE_TYPE_CANCELED_REF:
          return _M_raw.r == value._M_raw.r;
        case VALUE_TYPE_LAZY_VALUE_REF:
          return _M_raw.r == value._M_raw.r;
        case VALUE_TYPE_LAZY_VALUE_REF | VALUE_TYPE_LAZILY_CANCELED:
          return _M_raw.r == value._M_raw.r;
        case VALUE_TYPE_LOCKED_LAZY_VALUE_REF:
          return _M_raw.r == value._M_raw.r;
        case VALUE_TYPE_ERROR:
          return true;
        default:
          return false;
      }
    }

    //
    // A LazyValueMutex class.
    //

    thread_local size_t LazyValueMutex::_S_locked_mutex_count = 0;

    void LazyValueMutex::lock()
    {
      if(_S_locked_mutex_count == 0) lazy_value_mutex_sem.op(-1);
      _S_locked_mutex_count++;
      _M_mutex.lock();
    }

    bool LazyValueMutex::try_lock()
    {
      if(_S_locked_mutex_count == 0) lazy_value_mutex_sem.op(-1);
      _S_locked_mutex_count++;
      if(!_M_mutex.try_lock()) {
        if(_S_locked_mutex_count > 0) _S_locked_mutex_count--;
        if(_S_locked_mutex_count == 0) lazy_value_mutex_sem.op(1);
        return false;
      }
      return true;
    }

    void LazyValueMutex::unlock()
    {
      _M_mutex.unlock();
      if(_S_locked_mutex_count > 0) _S_locked_mutex_count--;
      if(_S_locked_mutex_count == 0) lazy_value_mutex_sem.op(1);
    }

    //
    // An Object class.
    //

    bool Object::operator==(const Object &object) const
    {
      if(_M_raw.type != object._M_raw.type) return false;
      if(_M_raw.length != object._M_raw.length) return false;
      switch(_M_raw.type & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          return equal(_M_raw.is8, _M_raw.is8 + _M_raw.length, object._M_raw.is8);
        case OBJECT_TYPE_IARRAY16:
          return equal(_M_raw.is16, _M_raw.is16 + _M_raw.length, object._M_raw.is16);
        case OBJECT_TYPE_IARRAY32:
          return equal(_M_raw.is32, _M_raw.is32 + _M_raw.length, object._M_raw.is32);
        case OBJECT_TYPE_IARRAY64:
          return equal(_M_raw.is64, _M_raw.is64 + _M_raw.length, object._M_raw.is64);
        case OBJECT_TYPE_SFARRAY:
          return equal(_M_raw.sfs, _M_raw.sfs + _M_raw.length, object._M_raw.sfs);
        case OBJECT_TYPE_DFARRAY:
          return equal(_M_raw.dfs, _M_raw.dfs + _M_raw.length, object._M_raw.dfs);
        case OBJECT_TYPE_RARRAY:
          return equal(_M_raw.rs, _M_raw.rs + _M_raw.length, object._M_raw.rs);
        case OBJECT_TYPE_TUPLE:
          for(size_t i = 0; i < _M_raw.length; i++) {
            Value value1(_M_raw.tuple_elem_types()[i], _M_raw.tes[i]);
            Value value2(object._M_raw.tuple_elem_types()[i], object._M_raw.tes[i]);
            if(value1 != value2) return false;
          }
          return true;
        case OBJECT_TYPE_LAZY_VALUE:
          if(_M_raw.lzv.value_type != object._M_raw.lzv.value_type) return false;
          if(_M_raw.lzv.must_be_shared != object._M_raw.lzv.must_be_shared) return false;
          if(_M_raw.lzv.value != object._M_raw.lzv.value) return false;
          if(_M_raw.lzv.fun != object._M_raw.lzv.fun) return false;
          for(size_t i = 0; i < _M_raw.length; i++) {
            if(_M_raw.lzv.args[i] != object._M_raw.lzv.args[i]) return false;
          }
          return true;
        case OBJECT_TYPE_NATIVE_OBJECT:
          if(_M_raw.ntvo.type != object._M_raw.ntvo.type) return false;
          if(_M_raw.ntvo.clazz != object._M_raw.ntvo.clazz) return false;
          return equal(_M_raw.ntvo.bs, _M_raw.ntvo.bs + _M_raw.length, object._M_raw.ntvo.bs);
        default:
          if((_M_raw.type & OBJECT_TYPE_INTERNAL) != 0)
            return equal(_M_raw.bs, _M_raw.bs + _M_raw.length, object._M_raw.bs);
          else
            return false;
      }
    }

    Value Object::elem(size_t i) const
    {
      switch(type() & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          return Value(_M_raw.is8[i]);
        case OBJECT_TYPE_IARRAY16:
          return Value(_M_raw.is16[i]);
        case OBJECT_TYPE_IARRAY32:
          return Value(_M_raw.is32[i]);
        case OBJECT_TYPE_IARRAY64:
          return Value(_M_raw.is64[i]);
        case OBJECT_TYPE_SFARRAY:
          return Value(_M_raw.sfs[i]);
        case OBJECT_TYPE_DFARRAY:
          return Value(_M_raw.dfs[i]);
        case OBJECT_TYPE_RARRAY:
          return Value(_M_raw.rs[i]);
        case OBJECT_TYPE_TUPLE:
          return Value(_M_raw.tuple_elem_types()[i], _M_raw.tes[i]);
        case OBJECT_TYPE_LAZY_VALUE:
          return _M_raw.lzv.args[i];
        default:
          return Value();
      }
    }

    bool Object::set_elem(size_t i, const Value &value)
    {
      switch(type() & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          if(value.type() != VALUE_TYPE_INT) return false;
          _M_raw.is8[i] = value.raw().i;
          return true;
        case OBJECT_TYPE_IARRAY16:
          if(value.type() != VALUE_TYPE_INT) return false;
          _M_raw.is16[i] = value.raw().i;
          return true;
        case OBJECT_TYPE_IARRAY32:
          if(value.type() != VALUE_TYPE_INT) return false;
          _M_raw.is32[i] = value.raw().i;
          return true;
        case OBJECT_TYPE_IARRAY64:
          if(value.type() != VALUE_TYPE_INT) return false;
          _M_raw.is64[i] = value.raw().i;
          return true;
        case OBJECT_TYPE_SFARRAY:
          if(value.type() != VALUE_TYPE_FLOAT) return false;
          _M_raw.sfs[i] = value.raw().f;
          return true;
        case OBJECT_TYPE_DFARRAY:
          if(value.type() != VALUE_TYPE_FLOAT) return false;
          _M_raw.dfs[i] = value.raw().f;
          return true;
        case OBJECT_TYPE_RARRAY:
          if(value.type() != VALUE_TYPE_REF) return false;
          _M_raw.rs[i] = value.raw().r;
          atomic_thread_fence(memory_order_release);
          return true;
        case OBJECT_TYPE_TUPLE:
          _M_raw.tuple_elem_types()[i] = TupleElementType(VALUE_TYPE_ERROR);
          atomic_thread_fence(memory_order_release);
          _M_raw.tes[i] = TupleElement(value.raw().i);
          atomic_thread_fence(memory_order_release);
          _M_raw.tuple_elem_types()[i] = TupleElementType(value.type());
          atomic_thread_fence(memory_order_release);
          return true;
        case OBJECT_TYPE_LAZY_VALUE:
          _M_raw.lzv.args[i].safely_assign_for_gc(value);
          atomic_thread_fence(memory_order_release);
          return true;
        default:
          return false;
      }
    }

    //
    // A ReturnValue class.
    //

    ReturnValue &ReturnValue::operator=(const Value &value)
    {
      switch(value.type()) {
        case VALUE_TYPE_INT:
          _M_raw.i = value.raw().i;
          _M_raw.f = 0.0;
          _M_raw.r = Reference();
          _M_raw.error = ERROR_SUCCESS;
          return *this;
        case VALUE_TYPE_FLOAT:
          _M_raw.i = 0;
          _M_raw.f = value.raw().f;
          _M_raw.r = Reference();
          _M_raw.error = ERROR_SUCCESS;
          return *this;
        case VALUE_TYPE_REF:
          _M_raw.i = 0;
          _M_raw.f = 0.0;
          _M_raw.r = value.raw().r;
          _M_raw.error = ERROR_SUCCESS;
          return *this;
        case VALUE_TYPE_LAZY_VALUE_REF:
        case VALUE_TYPE_LAZY_VALUE_REF | VALUE_TYPE_LAZILY_CANCELED:
          _M_raw.i = ((value.type() & VALUE_TYPE_LAZILY_CANCELED) != 0 ? 1 : 0);
          _M_raw.f = 0.0;
          _M_raw.r = value.raw().r;
          _M_raw.error = ERROR_SUCCESS;
          return *this;          
        default:
          _M_raw.i = 0;
          _M_raw.f = 0.0;
          _M_raw.r = Reference();
          _M_raw.error = ERROR_INCORRECT_VALUE;
          return *this;
      }
    }

    //
    // A Thread class.
    //

    Thread::Thread(ThreadContext *context) : _M_context(context) {}

    thread &Thread::system_thread() { return _M_context->system_thread(); }

    ThreadContext *Thread::context() { return _M_context.get(); }

    //
    // An Environment class.
    //

    Environment::~Environment() {}

    //
    // A RegisteredReference class.
    //

    RegisteredReference::~RegisteredReference()
    {
      if(_M_prev != nullptr || _M_next != nullptr) {
        lock_guard<GarbageCollector> guard(*(_M_context->gc()));
        if(_M_context->first_registered_r() == this)
          _M_context->first_registered_r() = (_M_next != this ? _M_next : nullptr);
        if(_M_context->last_registered_r() == this)
          _M_context->last_registered_r() = (_M_prev != this ? _M_prev : nullptr);
        if(_M_prev != nullptr) _M_prev->_M_next = _M_next;
        if(_M_next != nullptr) _M_next->_M_prev = _M_prev;
      }
    }

    void RegisteredReference::register_ref()
    {
      if(_M_prev == nullptr && _M_next == nullptr) {
        lock_guard<GarbageCollector> guard(*(_M_context->gc()));
        _M_prev = (_M_context->last_registered_r() != nullptr ? _M_context->last_registered_r() : this);
        _M_next = (_M_context->first_registered_r() != nullptr ? _M_context->first_registered_r() : this);
        if(_M_prev != this) _M_prev->_M_next = this;
        if(_M_next != this) _M_next->_M_prev = this;
        if(_M_context->first_registered_r() == nullptr) _M_context->first_registered_r() = this;
        _M_context->last_registered_r() = this;
      }
    }

    //
    // A ForkAround class.
    //

    ForkAround::ForkAround()
    {
#if defined(_WIN32) || defined(_WIN64)
      _M_pid = _getpid();
#else
      _M_pid = getpid();
#endif
      {
        unique_lock<mutex> lock(fork_handler_list_map_mutex);
        lock.release();
        for(auto iter = fork_handler_list_map.rbegin(); iter != fork_handler_list_map.rend(); iter++) {
          for(auto handler : iter->second) handler->pre_fork();
        }
      }
    }

    ForkAround::~ForkAround()
    {
#if defined(_WIN32) || defined(_WIN64)
      bool is_child = (_M_pid != _getpid());
#else
      bool is_child = (_M_pid != getpid());
#endif
      {
        unique_lock<mutex> lock(fork_handler_list_map_mutex, adopt_lock);
        for(auto iter = fork_handler_list_map.begin(); iter != fork_handler_list_map.end(); iter++) {
          for(auto handler : iter->second) handler->post_fork(is_child);
        }
      }
    }

    //
    // A ForkHandler class.
    //

    ForkHandler::~ForkHandler() {}

    //
    // A MutexForkHandler class.
    //

    MutexForkHandler::~MutexForkHandler() {}

    void MutexForkHandler::pre_fork()
    { for(auto iter = _M_mutexes.begin(); iter != _M_mutexes.end(); iter++) (*iter)->lock(); }

    void MutexForkHandler::post_fork(bool is_child)
    {
      if(is_child) {
        for(auto iter = _M_mutexes.begin(); iter != _M_mutexes.end(); iter++) new (*iter) mutex;
      } else {
        for(auto iter = _M_mutexes.rbegin(); iter != _M_mutexes.rend(); iter++) (*iter)->unlock();
      }
    }

    //
    // An InterruptibleFunctionAround class.
    //

    InterruptibleFunctionAround::InterruptibleFunctionAround(ThreadContext *context) :
      _M_context(context)
    {
      lock_guard<mutex> guard(_M_context->interruptible_fun_mutex());
      _M_context->interruptible_fun_flag() = true;
    }

    InterruptibleFunctionAround::~InterruptibleFunctionAround()
    {
      lock_guard<mutex> guard(_M_context->interruptible_fun_mutex());
      _M_context->interruptible_fun_flag() = false;
    }

    //
    // A VirtualMachine class.
    //

    VirtualMachine::~VirtualMachine() {}

    bool VirtualMachine::load(void *ptr, size_t size, list<LoadingError> *errors, bool is_auto_freeing)
    {
      vector<pair<void *, size_t>> pairs;
      pairs.push_back(make_pair(ptr, size));
      return load(pairs, errors, is_auto_freeing);
    }

    bool VirtualMachine::load(const vector<string> &file_names, list<LoadingError> *errors)
    {
      bool is_success = true;
      vector<pair<void *, size_t>> pairs;
      for(size_t i = 0; i < file_names.size(); i++) {
        const char *file_name = file_names[i].c_str();
        struct stat stat_buf;
        if(stat(file_name, &stat_buf) == -1) {
          if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_IO));
          is_success = false;
          continue;
        }
        if(stat_buf.st_size > numeric_limits<ssize_t>::max()) {
          if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_IO));
          is_success = false;
          continue;
        }
        size_t size = stat_buf.st_size;
        char *ptr = new char[size];
        ifstream ifs(file_name, ifstream::in | ifstream::binary);
        if(!ifs.good()) {
          if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_IO));
          is_success = false;
          continue;
        }
        ifs.read(ptr, size);
        if(ifs.gcount() != stat_buf.st_size) {
          if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_IO));
          is_success = false;
          continue;
        }
        pairs.push_back(make_pair(ptr, size));
      }
      if(!is_success) {
        for(auto pair : pairs) delete [] reinterpret_cast<char *>(pair.first);
        return false;
      }
      return load(pairs, errors, true);
    }

    bool VirtualMachine::load(const char *file_name, list<LoadingError> *errors)
    {
      vector<string> file_names;
      file_names.push_back(string(file_name));
      return load(file_names, errors);
    }

    int VirtualMachine::force_tuple_elem(ThreadContext *context, Object &object, size_t i)
    {
      if((object.type() & ~OBJECT_TYPE_UNIQUE) != OBJECT_TYPE_TUPLE) return ERROR_INCORRECT_OBJECT;
      if(object.elem(i).is_lazy()) {
        RegisteredReference tmp_r(object.elem(i).raw().r, context);
        Value tmp_value = object.elem(i);
        force(context, tmp_value);
        object.set_elem(i, tmp_value);
      }
      return ERROR_SUCCESS;
    }

    int VirtualMachine::fully_force_tuple_elem(ThreadContext *context, Object &object, size_t i)
    {
      if((object.type() & ~OBJECT_TYPE_UNIQUE) != OBJECT_TYPE_TUPLE) return ERROR_INCORRECT_OBJECT;
      if(object.elem(i).is_lazy()) {
        RegisteredReference tmp_r(object.elem(i).raw().r, context);
        Value tmp_value = object.elem(i);
        fully_force(context, tmp_value);
        object.set_elem(i, tmp_value);
      }
      return ERROR_SUCCESS;
    }

    //
    // A Loader class.
    //

    Loader::~Loader() {}

    //
    // An Allocator class.
    //

    Allocator::~Allocator() {}

    //
    // A GarbageCollector class.
    //

    GarbageCollector::~GarbageCollector() {}

    Object *GarbageCollector::new_object(int type, size_t length, ThreadContext *context)
    {
      size_t size = object_size(type, length);
      if(size != 0)
        return new(allocate(size, context)) Object(type, length);
      else
        return nullptr;
    }

    Object *GarbageCollector::new_immortal_object(int type, std::size_t length)
    {
      size_t size = object_size(type, length);
      if(size != 0)
        return new(allocate_immortal_area(size)) Object(type, length);
      else
        return nullptr;
    }

    Object *GarbageCollector::new_pair(const Value &value1, const Value &value2, ThreadContext *context)
    {
      Object *object = new_object(OBJECT_TYPE_TUPLE, 2, context);
      if(object == nullptr) return nullptr;
      object->set_elem(0, value1);
      object->set_elem(1, value2);
      return object;
    }

    Object *GarbageCollector::new_unique_pair(const Value &value1, const Value &value2, ThreadContext *context)
    {
      Object *object = new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
      if(object == nullptr) return nullptr;
      object->set_elem(0, value1);
      object->set_elem(1, value2);
      return object;
    }

    Object *GarbageCollector::new_string(const string &str, ThreadContext *context)
    {
      Object *object = new_object(OBJECT_TYPE_IARRAY8, str.size(), context);
      if(object == nullptr) return nullptr;
      copy(str.begin(), str.end(), object->raw().is8);
      return object;
    }

    Object *GarbageCollector::new_string(const char *str, ThreadContext *context)
    {
      size_t length = strlen(str);
      Object *object = new_object(OBJECT_TYPE_IARRAY8, length, context);
      if(object == nullptr) return nullptr;
      copy_n(str, length, object->raw().is8);
      return object;
    }

    //
    // A NativeFunctionHandler class.
    //

    NativeFunctionHandler::~NativeFunctionHandler() {}

    //
    // A DefualtNativeFunctionHandler class.
    //
    
    DefaultNativeFunctionHandler::DefaultNativeFunctionHandler()
    { add_fork_handler(FORK_HANDLER_PRIO_NATIVE_FUN, &default_native_fun_fork_handler); }

    DefaultNativeFunctionHandler::~DefaultNativeFunctionHandler()
    { delete_fork_handler(FORK_HANDLER_PRIO_NATIVE_FUN, &default_native_fun_fork_handler); }

    ReturnValue DefaultNativeFunctionHandler::invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args)
    {
      switch(nfi) {
        case NATIVE_FUN_ATOI:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[0].r()->type() != OBJECT_TYPE_IARRAY8)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          const char *s = reinterpret_cast<const char *>(args[0].r()->raw().is8);
          istringstream iss(string(s, args[0].r()->length()));
          int64_t i = 0;
          iss >> i;
          return ReturnValue(i, 0.0, Reference(), ERROR_SUCCESS);
        }
        case NATIVE_FUN_ITOA:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_INT)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          ostringstream oss;
          oss << args[0].i();
          string str = oss.str();
          Reference r = vm->gc()->new_object(OBJECT_TYPE_IARRAY8, str.length(), context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          copy(str.begin(), str.end(), reinterpret_cast<char *>(r->raw().is8));
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        case NATIVE_FUN_ATOF:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[0].r()->type() != OBJECT_TYPE_IARRAY8)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          const char *s = reinterpret_cast<const char *>(args[0].r()->raw().is8);
          istringstream iss(string(s, args[0].r()->length()));
          double f = 0.0;
          iss >> f;
          return ReturnValue(0, f, Reference(), ERROR_SUCCESS);
        }
        case NATIVE_FUN_FTOA:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_FLOAT)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          ostringstream oss;
          oss << args[0].f();
          string str = oss.str();
          Reference r = vm->gc()->new_object(OBJECT_TYPE_IARRAY8, str.length(), context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          copy(str.begin(), str.end(), reinterpret_cast<char *>(r->raw().is8));
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        case NATIVE_FUN_GET_CHAR:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[0].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          char c;
          {
            lock_guard<mutex> guard(io_stream_mutex);
            cin >> c;
          }
          Reference r = vm->gc()->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          r->set_elem(0, Value(c));
          r->set_elem(1, args[0]);
          args[0].cancel_ref();
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        case NATIVE_FUN_PUT_CHAR:
        {
          int error;
          if(args.length() != 2)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_INT)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          error = vm->force(context, args[1]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[1].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[1].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          {
            lock_guard<mutex> guard(io_stream_mutex);
            cout << static_cast<char>(args[0].i());
          }
          Reference r = vm->gc()->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          r->set_elem(0, args[0]);
          r->set_elem(1, args[1]);
          args[1].cancel_ref();
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        case NATIVE_FUN_GET_LINE:
        {
          int error;
          if(args.length() != 1)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[0].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          string str;
          {
            lock_guard<mutex> guard(io_stream_mutex);
            getline(cin, str);
          }
          context->regs().tmp_r.safely_assign_for_gc(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, str.length(), context));
          if(context->regs().tmp_r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          copy(str.begin(), str.end(), context->regs().tmp_r->raw().is8);
          Reference r = vm->gc()->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          r->set_elem(0, Value(context->regs().tmp_r));
          r->set_elem(1, args[0]);
          args[0].cancel_ref();
          context->regs().tmp_r.safely_assign_for_gc(r);
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        case NATIVE_FUN_PUT_STRING:
        {
          int error;
          if(args.length() != 2)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
          error = vm->force(context, args[0]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[0].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[0].r()->type() != OBJECT_TYPE_IARRAY8)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          error = vm->force(context, args[1]);
          if(error != ERROR_SUCCESS) return ReturnValue(0, 0.0, Reference(), error);
          if(args[1].type() != VALUE_TYPE_REF)
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
          if(args[1].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
            return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
          {
            lock_guard<mutex> guard(io_stream_mutex);
            cout.write(reinterpret_cast<const char *>(args[0].r()->raw().is8), args[0].r()->length());
          }
          Reference r = vm->gc()->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
          if(r.is_null())
            return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
          r->set_elem(0, Value(static_cast<int64_t>(args[0].r()->length())));
          r->set_elem(1, args[1]);
          args[1].cancel_ref();
          return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
        }
        default:
        {
          return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
        }
      }
    }

    const char *DefaultNativeFunctionHandler::native_fun_name(int nfi) const
    {
      switch(nfi) {
        case NATIVE_FUN_ATOI:
          return "atoi";
        case NATIVE_FUN_ITOA:
          return "itoa";
        case NATIVE_FUN_ATOF:
          return "atof";
        case NATIVE_FUN_FTOA:
          return "ftoa";
        case NATIVE_FUN_GET_CHAR:
          return "get_char";
        case NATIVE_FUN_PUT_CHAR:
          return "put_char";
        case NATIVE_FUN_GET_LINE:
          return "get_line";
        case NATIVE_FUN_PUT_STRING:
          return "put_string";
        default:
          return nullptr;
      }
    }

    int DefaultNativeFunctionHandler::min_native_fun_index() const
    { return 0; }

    int DefaultNativeFunctionHandler::max_native_fun_index() const
    { return MAX_DEFAULT_NATIVE_FUN_INDEX; }

    //
    // A MultiNativeFunctionHandler class.
    //

    MultiNativeFunctionHandler::MultiNativeFunctionHandler(const vector<NativeFunctionHandler *> &handlers)
    {
      _M_handler_pair_count = 0;
      for(auto handler : handlers) {
        int native_fun_count = handler->max_native_fun_index() - handler->min_native_fun_index() + 1;
        _M_handler_pair_count += static_cast<size_t>((native_fun_count + 1023) >> 10);
      }
      _M_handler_pairs = unique_ptr<HandlerPair []>(new HandlerPair[_M_handler_pair_count]);
      size_t i = 0;
      for(auto handler : handlers) {
        int native_fun_count = handler->max_native_fun_index() - handler->min_native_fun_index() + 1;
        size_t n = static_cast<size_t>((native_fun_count + 1023) >> 10);
        for(size_t j = 0; j < n; j++) {
          _M_handler_pairs[i + j].handler = unique_ptr<NativeFunctionHandler>(handler);
          _M_handler_pairs[i + j].nfi_offset = (static_cast<int>(i) << 10) - handler->min_native_fun_index();
        }
        i += n;
      }
    }

    MultiNativeFunctionHandler::~MultiNativeFunctionHandler() {}

    ReturnValue MultiNativeFunctionHandler::invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args)
    {
      size_t i = static_cast<size_t>(nfi >> 10);
      if(i < _M_handler_pair_count)
        return _M_handler_pairs[i].handler->invoke(vm, context, nfi - _M_handler_pairs[i].nfi_offset, args);
      else
        return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
    }

    const char *MultiNativeFunctionHandler::native_fun_name(int nfi) const
    {
      size_t i = static_cast<size_t>(nfi >> 10);
      if(i < _M_handler_pair_count)
        return _M_handler_pairs[i].handler->native_fun_name(nfi - _M_handler_pairs[i].nfi_offset);
      else
        return nullptr;
    }

    int MultiNativeFunctionHandler::min_native_fun_index() const
    {
      if(_M_handler_pair_count > 0)
        return _M_handler_pairs[0].handler->min_native_fun_index() + _M_handler_pairs[0].nfi_offset;
      else
        return 0;
    }

    int MultiNativeFunctionHandler::max_native_fun_index() const
    {
      if(_M_handler_pair_count > 0) {
        size_t i = _M_handler_pair_count - 1;
        return _M_handler_pairs[i].handler->max_native_fun_index() + _M_handler_pairs[i].nfi_offset;
      } else
        return 0;
    }

    //
    // An EmptyNativeFunctionHandler class.
    //

    EmptyNativeFunctionHandler::~EmptyNativeFunctionHandler() {}

    ReturnValue EmptyNativeFunctionHandler::invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args)
    { return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN); }

    const char *EmptyNativeFunctionHandler::native_fun_name(int nfi) const
    { return nullptr; }

    int EmptyNativeFunctionHandler::min_native_fun_index() const
    { return 0; }

    int EmptyNativeFunctionHandler::max_native_fun_index() const
    { return 1; }

    //
    // An EvaluationStrategy class.
    //

    EvaluationStrategy::~EvaluationStrategy() {}

    bool EvaluationStrategy::pre_enter_to_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type, bool &is_fun_result)
    { return pre_enter_to_fun(vm, context, i, value_type, is_fun_result); }

    bool EvaluationStrategy::post_leave_from_fun_for_force(VirtualMachine *vm, ThreadContext *context, size_t i, int value_type)
    { return post_leave_from_fun(vm, context, i, value_type); }

    void EvaluationStrategy::set_fun_infos_and_fun_count(const FunctionInfo *fun_infos, size_t fun_count) {}

    list<MemoizationCache *> EvaluationStrategy::memo_caches()
    { return list<MemoizationCache *>(); }

    //
    // A NativeFunctionHandlerLoader class.
    //

    NativeFunctionHandlerLoader::~NativeFunctionHandlerLoader() {}

    //
    // A NativeLibrary class.
    //

    NativeLibrary::NativeLibrary(const vector<NativeFunction> &funs, ForkHandler *fork_handler, int min_nfi) :
      _M_funs(funs), _M_fork_handler(fork_handler), _M_min_nfi(min_nfi)
    { if(_M_fork_handler != nullptr) add_fork_handler(FORK_HANDLER_PRIO_NATIVE_FUN, _M_fork_handler); }

    NativeLibrary::~NativeLibrary()
    { if(_M_fork_handler != nullptr) delete_fork_handler(FORK_HANDLER_PRIO_NATIVE_FUN, _M_fork_handler); }

    ReturnValue NativeLibrary::invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args)
    {
      if(nfi >= _M_min_nfi && nfi < static_cast<int>(_M_funs.size()) + _M_min_nfi)
        return _M_funs[nfi - _M_min_nfi].fun()(vm, context, args);
      else
        return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
    }

    const char *NativeLibrary::native_fun_name(int nfi) const
    {
      if(nfi >= _M_min_nfi && nfi < static_cast<int>(_M_funs.size()) + _M_min_nfi)
        return _M_funs[nfi - _M_min_nfi].name();
      else
        return nullptr;
    }

    int NativeLibrary::min_native_fun_index() const { return _M_min_nfi; }

    int NativeLibrary::max_native_fun_index() const { return _M_min_nfi + static_cast<int>(_M_funs.size()) - 1; }

    //
    // A MemoizationCacheFactory class.
    //

    MemoizationCacheFactory::~MemoizationCacheFactory() {}

    //
    // A Program class.
    //

    bool Program::relocate(size_t fun_offset, size_t var_offset, const unordered_map<string, size_t> &fun_indexes, const unordered_map<string, size_t> &var_indexes, const unordered_map<string, int> &native_fun_indexes)
    {
      for(size_t i = 0; i < _M_reloc_count; i++) {
        uint32_t addr = _M_relocs[i].addr;
        switch(_M_relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) {
          case format::RELOC_TYPE_ARG1_FUN:
          {
            if(addr >= _M_code_size) return false;
            if(opcode_to_arg_type1(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
            size_t index = _M_code[addr].arg1.i;
            if(!relocate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            _M_code[addr].arg1.i = index;
            break;
          }
          case format::RELOC_TYPE_ARG2_FUN:
          {
            if(addr >= _M_code_size) return false;
            if(opcode_to_arg_type2(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
            size_t index = _M_code[addr].arg2.i;
            if(!relocate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            _M_code[addr].arg2.i = index;
            break;
          }
          case format::RELOC_TYPE_ARG1_VAR:
          {
            if(addr >= _M_code_size) return false;
            if(opcode_to_arg_type1(_M_code[addr].opcode) != ARG_TYPE_GVAR) return false;
            size_t index = _M_code[addr].arg1.gvar;
            if(!relocate_index(index, var_offset, var_indexes, _M_relocs[i], format::SYMBOL_TYPE_VAR)) return false;
            _M_code[addr].arg1.gvar = index;
            break;
          }
          case format::RELOC_TYPE_ARG2_VAR:
          {
            if(addr >= _M_code_size) return false;
            if(opcode_to_arg_type2(_M_code[addr].opcode) != ARG_TYPE_GVAR) return false;
            size_t index = _M_code[addr].arg2.gvar;
            if(!relocate_index(index, var_offset, var_indexes, _M_relocs[i], format::SYMBOL_TYPE_VAR)) return false;
            _M_code[addr].arg2.gvar = index;
            break;
          }
          case format::RELOC_TYPE_ELEM_FUN:
          {
            format::Object *data_object;
            size_t index;
            if(!get_elem_fun_index(addr, index, data_object)) return false;
            if(!relocate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            if(!set_elem_fun_index(addr, index, data_object)) return false;
            break;
          }
          case format::RELOC_TYPE_VAR_FUN:
          {
            if(addr >= _M_var_count) return false;
            if(_M_vars[addr].type != VALUE_TYPE_INT) return false;
            if((_M_vars[addr].i < 0) || (_M_vars[addr].i > UINT32_MAX)) return false;
            size_t index = _M_vars[addr].i;
            if(!relocate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            _M_vars[addr].i = index;
            break;
          }
          default:
          {
            if((_M_flags & format::HEADER_FLAG_NATIVE_FUN_SYMBOLS) != 0) {
              switch(_M_relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) {
                case format::RELOC_TYPE_ARG1_NATIVE_FUN:
                {
                  if(addr >= _M_code_size) return false;
                  if(opcode_to_arg_type1(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
                  int index = _M_code[addr].arg1.i;
                  if(!relocate_native_fun_index(index, native_fun_indexes, _M_relocs[i])) return false;
                  _M_code[addr].arg1.i = index;
                  break;
                }
                case format::RELOC_TYPE_ARG2_NATIVE_FUN:
                {
                  if(addr >= _M_code_size) return false;
                  if(opcode_to_arg_type2(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
                  int index = _M_code[addr].arg2.i;
                  if(!relocate_native_fun_index(index, native_fun_indexes, _M_relocs[i])) return false;
                  _M_code[addr].arg2.i = index;
                  break;
                }
                case format::RELOC_TYPE_ELEM_NATIVE_FUN:
                {
                  format::Object *data_object;
                  size_t tmp_index;
                  int index;
                  if(!get_elem_fun_index(addr, tmp_index, data_object)) return false;
                  index = tmp_index;
                  if(!relocate_native_fun_index(index, native_fun_indexes, _M_relocs[i])) return false;
                  tmp_index = index;
                  if(!set_elem_fun_index(addr, tmp_index, data_object)) return false;
                  break;
                }
                case format::RELOC_TYPE_VAR_NATIVE_FUN:
                {
                  if(addr >= _M_var_count) return false;
                  if(_M_vars[addr].type != VALUE_TYPE_INT) return false;
                  if((_M_vars[addr].i < INT32_MIN) || (_M_vars[addr].i > INT32_MAX)) return false;
                  int index = _M_vars[addr].i;
                  if(!relocate_native_fun_index(index, native_fun_indexes, _M_relocs[i])) return false;
                  _M_vars[addr].i = index;
                  break;
                }
              }
            } else
              return false;
            break;
          }
        }
      }
      if((_M_flags & format::HEADER_FLAG_LIBRARY) == 0)
        _M_entry = _M_entry - _M_fun_offset + fun_offset;
      _M_fun_offset = fun_offset;
      _M_var_offset = var_offset;
      return true;
    }

    bool Program::relocate_index(size_t &index, size_t offset, const unordered_map<string, size_t> &indexes, const format::Relocation &reloc, uint8_t symbol_type)
    {
      if((reloc.type & format::RELOC_TYPE_SYMBOLIC) != 0) {
        format::Symbol *symbol = symbols(reloc.symbol);
        if((symbol->type & ~format::SYMBOL_TYPE_DEFINED) != symbol_type) return false;
        auto iter = indexes.find(string(symbol->name, symbol->length));
        if(iter == indexes.end()) return false;
        index = iter->second;
        return true;
      } else {
        size_t old_offset = (symbol_type == format::SYMBOL_TYPE_FUN ? _M_fun_offset : _M_var_offset);
        index = index - old_offset + offset;
        if(index > UINT32_MAX) return false;
        return true;
      }
    }

    bool Program::relocate_native_fun_index(int &index, const unordered_map<string, int> &indexes, const format::Relocation &reloc)
    {
      if((reloc.type & format::RELOC_TYPE_SYMBOLIC) != 0) {
        format::Symbol *symbol = symbols(reloc.symbol);
        if(symbol->type != format::SYMBOL_TYPE_NATIVE_FUN) return false;
        auto iter = indexes.find(string(symbol->name, symbol->length));
        if(iter == indexes.end()) return false;
        index = iter->second;
      }
      return true;
    }

    bool Program::get_elem_fun_index(size_t addr, size_t &index, format::Object *&data_object, bool is_native_fun_index)
    {
      auto data_addr_iter = _M_data_addrs.upper_bound(addr);
      if(data_addr_iter == _M_data_addrs.begin()) return false;
      data_addr_iter--;
      size_t data_object_addr = *data_addr_iter;
      data_object = reinterpret_cast<format::Object *>(_M_data + data_object_addr);
      if(data_object_addr + 8 > addr) return false;
      switch(data_object->type) {
        case OBJECT_TYPE_IARRAY32:
          if(data_object_addr + 8 + data_object->length * 4 <= addr) return false;
          if(((addr - (data_object_addr + 8)) & 3) != 0) return false;
          index = *reinterpret_cast<int32_t *>(_M_data + addr);
          return true;
        case OBJECT_TYPE_IARRAY64:
        case OBJECT_TYPE_TUPLE:
        {
          if(data_object_addr + 8 + data_object->length * 8 <= addr) return false;
          if(((addr - (data_object_addr + 8)) & 7) != 0) return false;
          if(data_object->type == OBJECT_TYPE_TUPLE) {
            size_t j = (addr - (data_object_addr + 8)) >> 3;
            if(data_object->tuple_elem_types()[j] != VALUE_TYPE_INT) return false;
          }
          int64_t tmp_index = *reinterpret_cast<int64_t *>(_M_data + addr);
          if(!is_native_fun_index) {
            if((tmp_index < 0) || (tmp_index > UINT32_MAX)) return false;
          } else {
            if((tmp_index < INT32_MIN) || (tmp_index > INT32_MAX)) return false;
          }
          index = tmp_index;
          return true;
        }
        default:
          return false;
      }
    }

    bool Program::set_elem_fun_index(size_t addr, size_t index, format::Object *data_object)
    {
      switch(data_object->type) {
        case OBJECT_TYPE_IARRAY32:
          *reinterpret_cast<int32_t *>(_M_data + addr) = index;
          return true;
        case OBJECT_TYPE_IARRAY64:
        case OBJECT_TYPE_TUPLE:
          *reinterpret_cast<int64_t *>(_M_data + addr) = index;
          return true;
        default:
          return false;
      }
    }

    //
    // A ThreadContext class.
    //

    ThreadContext::ThreadContext(const VirtualMachineContext &context, size_t stack_size) :
      _M_funs(context.funs()), _M_fun_count(context.fun_count()),
      _M_global_vars(context.vars()), _M_global_var_count(context.var_count()),
      _M_fun_infos(context.fun_infos())
    {
      _M_gc = nullptr;
      _M_native_fun_handler = nullptr;
      _M_regs.abp = _M_regs.abp2 = _M_regs.sec = _M_regs.nfbp = 0;
      _M_regs.ac = _M_regs.lvc = _M_regs.ac2 = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue();
      _M_regs.ai = 0;
      _M_regs.gc_tmp_ptr = nullptr;
      _M_regs.tmp_r = Reference();
      _M_regs.after_leaving_flags[0] = false;
      _M_regs.after_leaving_flags[1] = false;
      _M_regs.after_leaving_flag_index = 0;
      _M_regs.arg_instr_flag = false;
      _M_regs.cached_fun_result_flag = 0;
      _M_regs.try_flag = false;
      _M_regs.try_arg2 = Value();
      _M_regs.try_io_r = Reference();
      _M_regs.try_abp = _M_regs.nfbp;
      _M_regs.try_ac = 0;
      _M_regs.force_tmp_rv = ReturnValue();
      _M_regs.force_tmp_r = Reference();
      _M_regs.force_tmp_r2 = Reference();
      _M_regs.force_tmp_rv2 = ReturnValue();
      _M_first_registered_r = _M_last_registered_r = nullptr;
      _M_stack = new Value[stack_size];
      _M_stack_size = stack_size;
    }

    bool ThreadContext::enter_to_fun(size_t i)
    {
      if(_M_regs.abp2 + _M_regs.ac2 + 3 < _M_stack_size) {
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 0].safely_assign_for_gc(Value(_M_regs.abp, _M_regs.ac));
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 1].safely_assign_for_gc(Value(_M_regs.lvc, _M_regs.ip - 1));
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 2].safely_assign_for_gc(Value(static_cast<int64_t>((_M_regs.fp << 8) | (_M_regs.after_leaving_flag_index & 1) | ((_M_regs.cached_fun_result_flag & 1) << 1))));
        atomic_thread_fence(memory_order_release);
        _M_regs.abp = _M_regs.abp2;
        _M_regs.ac = _M_regs.ac2;
        _M_regs.abp2 = lvbp();
        _M_regs.lvc = _M_regs.ac2 = 0;
        _M_regs.sec = _M_regs.abp2;
        _M_regs.fp = i;
        _M_regs.ip = 0;
        _M_regs.after_leaving_flags[0] = false;
        _M_regs.after_leaving_flags[1] = false;
        _M_regs.after_leaving_flag_index = 0;
        atomic_thread_fence(memory_order_release);
        return true;
      } else
        return false;
    }

    bool ThreadContext::leave_from_fun()
    {
      auto fbp = _M_regs.abp + _M_regs.ac;
      if(fbp + 3 < _M_stack_size &&
          _M_stack[fbp + 0].type() == VALUE_TYPE_PAIR &&
          _M_stack[fbp + 1].type() == VALUE_TYPE_PAIR &&
          _M_stack[fbp + 2].type() == VALUE_TYPE_INT) {
        _M_regs.abp2 = _M_regs.abp;
        _M_regs.ac2 = _M_regs.ac;
        _M_regs.abp = _M_stack[fbp + 0].raw().p.first;
        _M_regs.ac = _M_stack[fbp + 0].raw().p.second;
        _M_regs.lvc = _M_stack[fbp + 1].raw().p.first;
        _M_regs.sec = _M_regs.abp2;
        _M_regs.ip = _M_stack[fbp + 1].raw().p.second;
        _M_regs.fp = static_cast<size_t>(_M_stack[fbp + 2].raw().i >> 8);
        _M_regs.after_leaving_flag_index = static_cast<unsigned>(_M_stack[fbp + 2].raw().i & 1);
        _M_regs.after_leaving_flags[_M_regs.after_leaving_flag_index] = true;
        _M_regs.cached_fun_result_flag = (_M_stack[fbp + 2].raw().i >> 1) & 1;
        atomic_thread_fence(memory_order_release);
        return true;
      } else
        return false;
    }

    ReturnValue ThreadContext::invoke_native_fun(VirtualMachine *vm, int nfi, ArgumentList &args)
    {
      SavedRegisters saved_regs;
      if(!save_regs_and_set_regs(saved_regs))
        return ReturnValue(0, 0.0, Reference(), ERROR_STACK_OVERFLOW);
      ReturnValue value;
      try {
        value = _M_native_fun_handler->invoke(vm, this, nfi, args);
      } catch(bad_alloc &e) {
        return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
      } catch(...) {
        return ReturnValue(0, 0.0, Reference(), ERROR_EXCEPTION);
      }
      if(!restore_regs(saved_regs))
        ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
      return value;
    }

    void ThreadContext::unlock_lazy_values(size_t new_stack_elem_count)
    {
      size_t i = _M_regs.abp2 + _M_regs.ac2;
      while(i > new_stack_elem_count) {
        i--;
        if(_M_stack[i].type() == VALUE_TYPE_LOCKED_LAZY_VALUE_REF)
          _M_stack[i].raw().r->raw().lzv.mutex.unlock();
      }
      atomic_thread_fence(memory_order_release);
    }
    
    void ThreadContext::set_error_without_try(int error, const Reference &r)
    {
      unlock_lazy_values(_M_regs.nfbp);
      _M_regs.abp = _M_regs.abp2 = _M_regs.sec = _M_regs.nfbp;
      _M_regs.ac = _M_regs.lvc = _M_regs.ac2 = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue(0, 0.0, r, error);
      _M_regs.after_leaving_flags[0] = false;
      _M_regs.after_leaving_flags[1] = false;
      atomic_thread_fence(memory_order_release);
    }

    void ThreadContext::set_error(int error, const Reference &r)
    {
      if(!_M_regs.try_flag || _M_regs.try_abp < _M_regs.nfbp) {
        set_error_without_try(error, r);
      } else {
        unlock_lazy_values(_M_regs.try_abp + _M_regs.try_ac);
        _M_regs.abp = _M_regs.try_abp;
        _M_regs.ac = _M_regs.try_ac;
        _M_regs.rv = ReturnValue(0, 0.0, r, error);
        _M_regs.after_leaving_flag_index = 0;
        if(!leave_from_fun()) set_error_without_try(error, r);
      }
    }

    void ThreadContext::traverse_root_objects(function<void (Object *)> fun)
    {
      for(size_t i = 0; i < _M_regs.sec; i++) {
        if(is_ref_value_type_for_gc(_M_stack[i].type()) && !_M_stack[i].raw().r.has_nil())
          fun(_M_stack[i].raw().r.ptr());
      }
      if(!_M_regs.rv.raw().r.has_nil())
        fun(_M_regs.rv.raw().r.ptr());
      if(!_M_regs.tmp_r.has_nil())
        fun(_M_regs.tmp_r.ptr());
      if(is_ref_value_type_for_gc(_M_regs.try_arg2.type()) && !_M_regs.try_arg2.raw().r.has_nil())
        fun(_M_regs.try_arg2.raw().r.ptr());
      if(!_M_regs.try_io_r.has_nil())
        fun(_M_regs.try_io_r.ptr());
      if(!_M_regs.force_tmp_rv.raw().r.has_nil())
        fun(_M_regs.force_tmp_rv.raw().r.ptr());
      if(!_M_regs.force_tmp_r.has_nil())
        fun(_M_regs.force_tmp_r.ptr());
      if(!_M_regs.force_tmp_r2.has_nil())
        fun(_M_regs.force_tmp_r2.ptr());
      if(!_M_regs.force_tmp_rv2.raw().r.has_nil())
        fun(_M_regs.force_tmp_rv2.raw().r.ptr());
      if(_M_first_registered_r != nullptr) {
        RegisteredReference *r = _M_first_registered_r; 
        do {
          if(!r->has_nil()) fun(r->ptr()); 
          r = r->_M_next;
        } while(r != _M_first_registered_r);
      }
    }

    bool ThreadContext::save_regs_and_set_regs(SavedRegisters &saved_regs)
    {
      saved_regs.nfbp = _M_regs.nfbp;
      saved_regs.abp = _M_regs.abp;
      saved_regs.ac = _M_regs.ac;
      saved_regs.lvc = _M_regs.lvc;
      saved_regs.abp2 = _M_regs.abp2;
      saved_regs.ac2 = _M_regs.ac2;
      saved_regs.fp = _M_regs.fp;
      saved_regs.ip = _M_regs.ip;
      saved_regs.after_leaving_flag1 = _M_regs.after_leaving_flags[0];
      saved_regs.after_leaving_flag2 = _M_regs.after_leaving_flags[1];
      saved_regs.after_leaving_flag_index = _M_regs.after_leaving_flag_index;
      saved_regs.try_flag = _M_regs.try_flag;
      saved_regs.try_abp = _M_regs.try_abp;
      saved_regs.try_ac = _M_regs.try_ac;
      saved_regs.force_tmp_rv = _M_regs.force_tmp_rv;
      saved_regs.force_tmp_rv2 = _M_regs.force_tmp_rv2;
      saved_regs.sec = _M_regs.abp2 + _M_regs.ac2;
      uint32_t sec = saved_regs.sec;
      if(sec + 6 > _M_stack_size) return false;
      _M_stack[sec + 0].safely_assign_for_gc(_M_regs.try_arg2);
      _M_stack[sec + 1].safely_assign_for_gc(Value(_M_regs.try_io_r));
      _M_stack[sec + 2].safely_assign_for_gc(Value(_M_regs.force_tmp_rv.raw().r));
      _M_stack[sec + 3].safely_assign_for_gc(Value(_M_regs.force_tmp_r));
      _M_stack[sec + 4].safely_assign_for_gc(Value(_M_regs.force_tmp_r2));
      _M_stack[sec + 5].safely_assign_for_gc(Value(_M_regs.force_tmp_rv2.raw().r));
      _M_regs.nfbp = sec + 6;
      _M_regs.abp = _M_regs.abp2 = _M_regs.sec = _M_regs.nfbp;
      _M_regs.ac = _M_regs.lvc = _M_regs.ac2 = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.after_leaving_flags[0] = false;
      _M_regs.after_leaving_flags[1] = false;
      _M_regs.after_leaving_flag_index = 0;
      _M_regs.try_flag = false;
      _M_regs.try_arg2 = Value();
      _M_regs.try_io_r = Reference();
      _M_regs.try_abp = _M_regs.nfbp;
      _M_regs.try_ac = 0;
      atomic_thread_fence(memory_order_release);
      return true;
    }

    bool ThreadContext::restore_regs(const SavedRegisters &saved_regs)
    {
      uint32_t sec = saved_regs.sec;
      bool result = true;
      _M_regs.force_tmp_rv.safely_assign_for_gc(saved_regs.force_tmp_rv);
      _M_regs.force_tmp_rv2.safely_assign_for_gc(saved_regs.force_tmp_rv2);
      if(_M_stack[sec + 5].type() == VALUE_TYPE_REF &&
          _M_stack[sec + 5].raw().r == _M_regs.force_tmp_rv2.raw().r &&
          _M_stack[sec + 4].type() == VALUE_TYPE_REF &&
          _M_stack[sec + 3].type() == VALUE_TYPE_REF &&
          _M_stack[sec + 2].type() == VALUE_TYPE_REF &&
          _M_stack[sec + 2].raw().r == _M_regs.force_tmp_rv.raw().r &&
          _M_stack[sec + 1].type() == VALUE_TYPE_REF) {
        _M_regs.force_tmp_r2.safely_assign_for_gc(_M_stack[sec + 4].raw().r);
        _M_regs.force_tmp_r.safely_assign_for_gc(_M_stack[sec + 3].raw().r);
        _M_regs.try_io_r = _M_stack[sec + 1].raw().r;
        _M_regs.try_arg2.safely_assign_for_gc(_M_stack[sec + 0]);
        _M_regs.try_ac = saved_regs.try_ac;
        _M_regs.try_abp = saved_regs.try_abp;
        _M_regs.try_flag = saved_regs.try_flag;
      } else {
        _M_regs.force_tmp_r2 = Reference();
        _M_regs.force_tmp_r = Reference();
        _M_regs.try_flag = false;
        _M_regs.try_arg2 = Value();
        _M_regs.try_io_r = Reference();
        _M_regs.try_abp = saved_regs.nfbp;
        _M_regs.try_ac = 0;
        result = false;
      }
      _M_regs.after_leaving_flag_index = saved_regs.after_leaving_flag_index;
      _M_regs.after_leaving_flags[1] = saved_regs.after_leaving_flag2;
      _M_regs.after_leaving_flags[0] = saved_regs.after_leaving_flag1;
      _M_regs.ip = saved_regs.ip;
      _M_regs.fp = saved_regs.fp;
      _M_regs.sec = saved_regs.abp2 + saved_regs.ac2;
      _M_regs.ac2 = saved_regs.ac2;
      _M_regs.abp2 = saved_regs.abp2;
      _M_regs.lvc = saved_regs.lvc;
      _M_regs.ac = saved_regs.ac;
      _M_regs.abp = saved_regs.abp;
      _M_regs.nfbp = saved_regs.nfbp;
      atomic_thread_fence(memory_order_release);
      return result;
    }

    //
    // A VirtualMachineContext class.
    //

    VirtualMachineContext::~VirtualMachineContext() {}

    void VirtualMachineContext::traverse_root_objects(function<void (Object *)> fun)
    {
      for(size_t i = 0; i < var_count(); i++) {
        if(is_ref_value_type_for_gc(vars()[i].type()) && !vars()[i].raw().r.has_nil())
          fun(vars()[i].raw().r.ptr());
      }
      for(auto iter = memo_caches().begin(); iter != memo_caches().end(); iter++) {
        (*iter)->traverse_root_objects(fun);
      }
    }

    //
    // A MemoizationCache class.
    //

    MemoizationCache::~MemoizationCache() {}

    //
    // Other fuctions.
    //

    Loader *new_loader() { return new impl::ImplLoader(); }

    Allocator *new_allocator() { return new impl::NewAllocator(); }

    GarbageCollector *new_garbage_collector(Allocator *alloc)
    { return new impl::MarkSweepGarbageCollector(alloc); }

    MemoizationCacheFactory *new_memoization_cache_factory(size_t bucket_count)
    { return new impl::HashTableMemoizationCacheFactory(bucket_count); }

    EvaluationStrategy *new_evaluation_strategy()
    { return new impl::EagerEvaluationStrategy(); }

    VirtualMachine *new_virtual_machine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy, function<void ()> exit_fun)
    { return new impl::InterpreterVirtualMachine(loader, gc, native_fun_handler, eval_strategy, exit_fun); }

    NativeFunctionHandlerLoader *new_native_function_handler_loader()
    { return new impl::ImplNativeFunctionHandlerLoader(); }

    EvaluationStrategy *new_eager_evaluation_strategy()
    { return new impl::EagerEvaluationStrategy(); }

    EvaluationStrategy *new_lazy_evaluation_strategy()
    { return new impl::LazyEvaluationStrategy(); }

    EvaluationStrategy *new_memoization_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory)
    { return new impl::MemoizationEvaluationStrategy(memo_cache_factory); }

    EvaluationStrategy *new_memoization_lazy_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory)
    { return new impl::MemoizationLazyEvaluationStrategy(memo_cache_factory); }

    EvaluationStrategy *new_function_evaluation_strategy(MemoizationCacheFactory *memo_cache_factory, unsigned default_fun_eval_strategy)
    { return new impl::FunctionEvaluationStrategy(memo_cache_factory, default_fun_eval_strategy); }

    void initialize_vm()
    {
#if defined(_WIN32) || defined(_WIN64)
      WSADATA data;
      ::WSAStartup(MAKEWORD(2, 2), &data);
#endif
      initialize_thread_stop_cont();
      add_fork_handler(FORK_HANDLER_PRIO_INTERNAL, &internal_fork_handler);
    }

    void finalize_vm()
    {
      delete_fork_handler(FORK_HANDLER_PRIO_INTERNAL, &internal_fork_handler);
      finalize_thread_stop_cont();
#if defined(_WIN32) || defined(_WIN64)
      ::WSACleanup();
#endif
    }

    void set_temporary_root_object(ThreadContext *context, Reference ref)
    { context->regs().tmp_r.safely_assign_for_gc(ref); }

    NativeLibrary *new_native_library_without_throwing(const vector<NativeFunction> &funs, ForkHandler *fork_handler, int min_nfi)
    { try { return new NativeLibrary(funs, fork_handler, min_nfi); } catch(...) { return nullptr; } }

    int &letin_errno()
    {
      static thread_local int thread_local_errno = 0;
      return thread_local_errno;
    }

    bool equal_values(const Value &value1, const Value &value2)
    {
      if(value1.type() != value2.type()) return false;
      switch(value1.type()) {
        case VALUE_TYPE_INT:
          return value1.raw().i == value2.raw().i;
        case VALUE_TYPE_FLOAT:
          return value1.raw().f == value2.raw().f;
        case VALUE_TYPE_REF:
          return value1.raw().r == value2.raw().r || equal_objects(*(value1.raw().r), *(value2.raw().r));
        default:
          return false;
      }
    }

    bool equal_objects(const Object &object1, const Object &object2)
    {
      if(object1.type() != object2.type()) return false;
      if(object1.length() != object2.length()) return false;
      switch(object1.type() & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          return equal(object1.raw().is8, object1.raw().is8 + object1.length(), object2.raw().is8);
        case OBJECT_TYPE_IARRAY16:
          return equal(object1.raw().is16, object1.raw().is16 + object1.length(), object2.raw().is16);
        case OBJECT_TYPE_IARRAY32:
          return equal(object1.raw().is32, object1.raw().is32 + object1.length(), object2.raw().is32);
        case OBJECT_TYPE_IARRAY64:
          return equal(object1.raw().is64, object1.raw().is64 + object1.length(), object2.raw().is64);
        case OBJECT_TYPE_SFARRAY:
          return equal(object1.raw().sfs, object1.raw().sfs + object1.length(), object2.raw().sfs);
        case OBJECT_TYPE_DFARRAY:
          return equal(object1.raw().dfs, object1.raw().dfs + object1.length(), object2.raw().dfs);
        case OBJECT_TYPE_RARRAY:
          for(size_t i = 0; i < object2.length(); i++) {
            if(object1.raw().rs[i] != object2.raw().rs[i] && equal_objects(*(object1.raw().rs[i]), *(object2.raw().rs[i])))
              return false;
          }
          return true;
        case OBJECT_TYPE_TUPLE:
          for(size_t i = 0; i < object2.length(); i++) {
            Value value1(object1.raw().tuple_elem_types()[i], object1.raw().tes[i]);
            Value value2(object2.raw().tuple_elem_types()[i], object2.raw().tes[i]);
            if(equal_values(value1, value2)) return false;
          }
          return true;
        case OBJECT_TYPE_NATIVE_OBJECT:
          return object1.raw().ntvo.clazz.equal_fun()(reinterpret_cast<const void *>(object1.raw().ntvo.bs), reinterpret_cast<const void *>(object2.raw().ntvo.bs));
        default:
          return false;
      }
    }

    void add_fork_handler(int prio, ForkHandler *handler)
    {
      lock_guard<mutex> guard(fork_handler_list_map_mutex);
      list<ForkHandler *> &handlers = fork_handler_list_map[prio];
      handlers.push_back(handler);
    }

    void delete_fork_handler(int prio, ForkHandler *handler)
    {
      lock_guard<mutex> guard(fork_handler_list_map_mutex);
      auto iter = fork_handler_list_map.find(prio);
      if(iter == fork_handler_list_map.end()) return;
      auto iter2 = find(iter->second.begin(), iter->second.end(), handler);
      if(iter2 != iter->second.end()) iter->second.erase(iter2);
      if(iter->second.empty()) fork_handler_list_map.erase(iter);
    }

    ostream &operator<<(ostream &os, const Value &value)
    {
      switch(value.type()) {
        case VALUE_TYPE_INT:
          return os << value.i();
        case VALUE_TYPE_FLOAT:
          return os << value.f();
        case VALUE_TYPE_REF:
          return os << *(value.r());
        case VALUE_TYPE_CANCELED_REF:
          return os << "canceled reference";
        case VALUE_TYPE_LAZY_VALUE_REF:
          return os << "lazy value reference";
        case VALUE_TYPE_LAZY_VALUE_REF | VALUE_TYPE_LAZILY_CANCELED:
          return os << "lazily canceled lazy value reference";
        case VALUE_TYPE_LOCKED_LAZY_VALUE_REF:
          return os << "locked lazy value reference";
        default:
          return os << "error";
      }
    }

    ostream &operator<<(ostream &os, const Object &object)
    {
      if(object.is_unique()) os << "unique ";
      switch(object.type() & ~OBJECT_TYPE_UNIQUE) {
        case OBJECT_TYPE_IARRAY8:
          os << "iarray8";
          break;
        case OBJECT_TYPE_IARRAY16:
          os << "iarray16";
          break;
        case OBJECT_TYPE_IARRAY32:
          os << "iarray32";
          break;
        case OBJECT_TYPE_IARRAY64:
          os << "iarray64";
          break;
        case OBJECT_TYPE_SFARRAY:
          os << "sfarray";
          break;
        case OBJECT_TYPE_DFARRAY:
          os << "dfarray";
          break;
        case OBJECT_TYPE_RARRAY:
          os << "rarray";
          break;
        case OBJECT_TYPE_TUPLE:
          break;
        case OBJECT_TYPE_IO:
          return os << "io";
        case OBJECT_TYPE_LAZY_VALUE:
          return os << "lazy value";
        case OBJECT_TYPE_NATIVE_OBJECT:
          return os << "native object";
        default:
          if((object.type() & OBJECT_TYPE_INTERNAL) != 0 && object.type() != OBJECT_TYPE_ERROR)
            return os << "internal object";
          else
            return os << "error";
      }
      os << ((object.type() & ~ OBJECT_TYPE_UNIQUE) != OBJECT_TYPE_TUPLE ? "[" : "(");
      for(size_t i = 0; i < object.length(); i++) {
        os << object.elem(i);
        if(i + 1 < object.length()) os << ", ";
      }
      return os << ((object.type() & ~ OBJECT_TYPE_UNIQUE) != OBJECT_TYPE_TUPLE ? "]" : ")");
    }

    ostream &operator<<(ostream &os, const LoadingError &error)
    {
      switch(error.error()) {
        case LOADING_ERROR_IO:
          return os << "IO error";
        case LOADING_ERROR_FORMAT:
          return os << "incorrect format";
        case LOADING_ERROR_NO_FUN_SYM:
          return os << "undefined function symbol " << error.symbol_name();
        case LOADING_ERROR_FUN_SYM:
          return os << "already function symbol " << error.symbol_name();
        case LOADING_ERROR_NO_VAR_SYM:
          return os << "undefined variable symbol " << error.symbol_name();
        case LOADING_ERROR_VAR_SYM:
          return os << "already variable symbol " << error.symbol_name();
        case LOADING_ERROR_RELOC:
          return os << "can't relocate program or library";
        case LOADING_ERROR_ENTRY:
          return os << "already defined entry";
        case LOADING_ERROR_NO_RELOC:
          return os << "unrelocatable program or library";
        case LOADING_ERROR_FUN_INDEX:
          return os << "incorrect function index " << error.symbol_name();
        case LOADING_ERROR_VAR_INDEX:
          return os << "incorrect function index " << error.symbol_name();
        case LOADING_ERROR_ALLOC:
          return os << "out of memory";
        case LOADING_ERROR_NO_NATIVE_FUN_SYM:
          return os << "undefined native function symbol " << error.symbol_name();
        default:
          return os << "unknown error";
      }
    }

    const char *error_to_string(int error)
    {
      switch(error) {
        case ERROR_SUCCESS:
          return "success";
        case ERROR_NO_INSTR:
          return "no instruction";
        case ERROR_INCORRECT_INSTR:
          return "incorrect instruction";
        case ERROR_INCORRECT_VALUE:
          return "incorrect value";
        case ERROR_INCORRECT_OBJECT:
          return "incorrect object";
        case ERROR_INCORRECT_FUN:
          return "incorrect function";
        case ERROR_EMPTY_STACK:
          return "empty stack";
        case ERROR_STACK_OVERFLOW:
          return "stack oveflow";
        case ERROR_OUT_OF_MEMORY:
          return "out of memory";
        case ERROR_NO_FUN:
          return "no function";
        case ERROR_NO_LOCAL_VAR:
          return "no local variable";
        case ERROR_NO_GLOBAL_VAR:
          return "no global variable";
        case ERROR_NO_ARG:
          return "no argument";
        case ERROR_INCORRECT_ARG_COUNT:
          return "incorrect number of arguments";
        case ERROR_DIV_BY_ZERO:
          return "division by zero";
        case ERROR_INDEX_OF_OUT_BOUNDS:
          return "index of out bounds";
        case ERROR_EXCEPTION:
          return "exception";
        case ERROR_NO_ENTRY:
          return "no entry";
        case ERROR_NO_NATIVE_FUN:
          return "no native function";
        case ERROR_UNIQUE_OBJECT:
          return "unique object";
        case ERROR_AGAIN_USED_UNIQUE:
          return "again used unique object";
        case ERROR_USER_EXCEPTION:
          return "user exception";
        default:
          return "unknown error";
      }
    }

    namespace priv
    {
      //
      // Private static functions.
      //

      static bool add_object_byte_count(const Object &object, size_t &byte_count);

      static bool add_value_byte_count(const Value &value, size_t &byte_count)
      {
        switch(value.type()) {
          case VALUE_TYPE_INT:
          case VALUE_TYPE_FLOAT:
            byte_count += 8;
            return true;
          case VALUE_TYPE_REF:
            byte_count += 8;
            if(!add_object_byte_count(*(value.raw().r), byte_count)) return false;
            return true;
          default:
            return false;
        }
      }
      
      static bool add_object_byte_count(const Object &object, size_t &byte_count)
      {
        switch(object.type()) {
          case OBJECT_TYPE_IARRAY8:
            byte_count += object.length();
            return true;
          case OBJECT_TYPE_IARRAY16:
            byte_count += object.length() * 2;
            return true;
          case OBJECT_TYPE_IARRAY32:
            byte_count += object.length() * 4;
            return true;
          case OBJECT_TYPE_IARRAY64:
            byte_count += object.length() * 8;
            return true;
          case OBJECT_TYPE_SFARRAY:
            byte_count += object.length() * 4;
            return true;
          case OBJECT_TYPE_DFARRAY:
            byte_count += object.length() * 8;
            return true;
          case OBJECT_TYPE_RARRAY:
            byte_count += object.length() * 8;
            return true;
          case OBJECT_TYPE_TUPLE:
            byte_count += object.length() * 8;
            for(size_t i = 0; i < object.length(); i++) {
              Value value(object.raw().tuple_elem_types()[i], object.raw().tes[i]);
              if(!add_value_byte_count(value, byte_count)) return false;
            }
            return true;
          case OBJECT_TYPE_NATIVE_OBJECT:
            return true;
          default:
            return false;
        }
      }

      //
      // An InternalForkHandler class.
      //

      InternalForkHandler::~InternalForkHandler() {}

      void InternalForkHandler::pre_fork()
      { lazy_value_mutex_sem.lock(); }

      void InternalForkHandler::post_fork(bool is_child)
      {
        if(is_child)
          new (&lazy_value_mutex_sem) Semaphore(1);
        else
          lazy_value_mutex_sem.unlock();
      }

      //
      // A DefaultNativeFunctionForkHandler class.
      //

      DefaultNativeFunctionForkHandler::~DefaultNativeFunctionForkHandler() {}

      void DefaultNativeFunctionForkHandler::pre_fork()
      { io_stream_mutex.lock(); }

      void DefaultNativeFunctionForkHandler::post_fork(bool is_child)
      {
        if(is_child)
          new (&io_stream_mutex) mutex;
        else
          io_stream_mutex.unlock();
      }

      //
      // Private variables.
      //

      Semaphore lazy_value_mutex_sem;

      //
      // Private functions.
      //

      bool are_memoizable_fun_args(const ArgumentList &args)
      {
        size_t byte_count = 0;
        for(size_t i = 0; i < args.length(); i++) {
          if(args[i].is_unique()) return false;
          if(!add_value_byte_count(args[i], byte_count)) return false;
        }
        if(byte_count > 256) return false;
        return true;
      }

      bool is_memoizable_fun_result(const Value &value)
      {
        if(value.is_unique()) return false;
        size_t byte_count = 0;
        if(!add_value_byte_count(value, byte_count)) return false;
        if(byte_count > 256) return false;
        return true;
      }

      void traverse_child_objects(Object &object, function<void (Object *)> fun)
      {
        switch(object.type() & ~OBJECT_TYPE_UNIQUE) {
          case OBJECT_TYPE_RARRAY:
            for(size_t i = 0; i < object.length(); i++) {
              if(!object.raw().rs[i].has_nil()) fun(object.raw().rs[i].ptr());
            }
            break;
          case OBJECT_TYPE_TUPLE:
            for(size_t i = 0; i < object.length(); i++) {
              if(is_ref_value_type_for_gc(object.raw().tuple_elem_types()[i].raw())) {
                Reference elem_ref = object.raw().tes[i].raw().r;
                if(!elem_ref.has_nil()) fun(elem_ref.ptr());
              }
            }
            break;
          case OBJECT_TYPE_LAZY_VALUE:
            if(is_ref_value_type_for_gc(object.raw().lzv.value.type())) {
              Reference value_ref = object.raw().lzv.value.raw().r;
              if(!value_ref.has_nil()) fun(value_ref.ptr());
            }
            for(size_t i = 0; i < object.length(); i++) {
              if(is_ref_value_type_for_gc(object.raw().lzv.args[i].type())) {
                Reference arg_ref = object.raw().lzv.args[i].raw().r;
                if(!arg_ref.has_nil()) fun(arg_ref.ptr());
              }
            }
            break;
          case OBJECT_TYPE_HASH_TABLE:
          {
            HashTableRaw *raw = reinterpret_cast<HashTableRaw *>(object.raw().bs);
            for(size_t i = 0; i < raw->bucket_count; i++) {
              Reference entry_ref = raw->buckets[i].first_entry_r;
              while(!entry_ref.has_nil()) {
                fun(entry_ref.ptr());
                HashTableEntryRawBase *raw = reinterpret_cast<HashTableEntryRawBase *>(entry_ref->raw().bs);
                entry_ref = raw->next_r;
              }
            }
            break;
          }
          case OBJECT_TYPE_ALI_HASH_TABLE_ENTRY:
          {
            HashTableEntryRaw<ArgumentList, int64_t> *raw = reinterpret_cast<HashTableEntryRaw<ArgumentList, int64_t> *>(object.raw().bs);
            if(!raw->key.key_ref().has_nil()) fun(raw->key.key_ref().ptr());
            break;
          }
          case OBJECT_TYPE_ALF_HASH_TABLE_ENTRY:
          {
            HashTableEntryRaw<ArgumentList, double> *raw = reinterpret_cast<HashTableEntryRaw<ArgumentList, double> *>(object.raw().bs);
            if(!raw->key.key_ref().has_nil()) fun(raw->key.key_ref().ptr());
            break;
          }
          case OBJECT_TYPE_ALR_HASH_TABLE_ENTRY:
          {
            HashTableEntryRaw<ArgumentList, Reference> *raw = reinterpret_cast<HashTableEntryRaw<ArgumentList, Reference> *>(object.raw().bs);
            if(!raw->key.key_ref().has_nil()) fun(raw->key.key_ref().ptr());
            if(!raw->value.value().has_nil()) fun(raw->value.value().ptr());
            break;
          }
        }
      }
    }
  }
}
