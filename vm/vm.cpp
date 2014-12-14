/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/stat.h>
#include <atomic>
#include <fstream>
#include <limits>
#include <memory>
#include <letin/const.hpp>
#include <letin/vm.hpp>
#include "thread_stop_cont.hpp"
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    //
    // A Reference class.
    //
    
    Object Reference::_S_nil;
    
    //
    // An Object class.
    //

    Value Object::elem(size_t i) const
    {
      switch(type()) {
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
          return _M_raw.tes[i];
        default:
          return Value();
      }
    }

    bool Object::set_elem(size_t i, Value &value)
    {
      switch(type()) {
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
          return true;
        case OBJECT_TYPE_TUPLE:
          _M_raw.tes[i] = value;
          return true;
        default:
          return false;
      }
    }

    //
    // A ReturnValue class.
    //

    ReturnValue &ReturnValue::operator=(const Value value)
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

    std::thread &Thread::system_thread() { return _M_context->system_thread(); }

    //
    // An Environment class.
    //

    Environment::~Environment() {}

    //
    // A VirtualMachine class.
    //

    VirtualMachine::~VirtualMachine() {}

    bool VirtualMachine::load(const char *file_name)
    {
      struct stat stat_buf;
      if(stat(file_name, &stat_buf) == -1) return false;
      if(stat_buf.st_size > numeric_limits<size_t>::max()) return false;
      size_t size = stat_buf.st_size;
      char *ptr = new char[size];
      ifstream ifs(file_name);
      if(ifs.good()) return false;
      ifs.read(ptr, size);
      return load(reinterpret_cast<void *>(ptr), size, true);
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
      size_t elem_size;
      switch(type) {
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
          elem_size =  sizeof(Reference);
          break;
        case OBJECT_TYPE_TUPLE:
          elem_size = sizeof(Value);
          break;
        default:
          return nullptr;
      }
      return new(allocate((sizeof(Object) - sizeof(Value)) + length * elem_size, context)) Object(type, length);
    }

    //
    // A ThreadContext class.
    //

    ThreadContext::ThreadContext(const VirtualMachineContext &context, size_t stack_size) :
      _M_funs(context.funs()), _M_fun_count(context.fun_count()),
      _M_global_vars(context.vars()), _M_global_var_count(context.var_count())
    {
      _M_gc = nullptr;
      _M_regs.abp = _M_regs.ac = _M_regs.lvc = _M_regs.abp2 = _M_regs.ac2 = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue();
      _M_regs.after_leaving_flag = false;
      _M_stack = new Value[stack_size];
      _M_stack_size = stack_size;
    }

    bool ThreadContext::enter_to_fun(size_t i)
    {
      if(_M_regs.abp2 + _M_regs.ac2 + 3 < _M_stack_size) {
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 0].safely_assign_for_gc(Value(_M_regs.abp, _M_regs.ac));
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 1].safely_assign_for_gc(Value(_M_regs.lvc, _M_regs.ip - 1));
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 2].safely_assign_for_gc(Value(static_cast<int64_t>(_M_regs.fp)));
        atomic_thread_fence(memory_order_release);
        _M_regs.abp = _M_regs.abp2;
        _M_regs.ac = _M_regs.ac2;
        _M_regs.abp2 = lvbp();
        _M_regs.lvc = _M_regs.ac2 = 0;
        _M_regs.sec = _M_regs.abp2;
        _M_regs.fp = i;
        _M_regs.ip = 0;
        _M_regs.after_leaving_flag = false;
        atomic_thread_fence(memory_order_release);
        return true;
      } else
        return false;
    }

    bool ThreadContext::leave_from_fun()
    {
      auto fbp = _M_regs.abp + _M_regs.ac;
      if(fbp + 3 < _M_stack_size ||
          _M_stack[fbp + 0].type() == VALUE_TYPE_PAIR ||
          _M_stack[fbp + 1].type() == VALUE_TYPE_PAIR ||
          _M_stack[fbp + 2].type() == VALUE_TYPE_INT) {
        _M_regs.abp2 = _M_regs.abp;
        _M_regs.ac2 = 0;
        _M_regs.abp = _M_stack[fbp + 0].raw().p.first;
        _M_regs.ac = _M_stack[fbp + 0].raw().p.second;
        _M_regs.lvc = _M_stack[fbp + 1].raw().p.first;
        _M_regs.sec = _M_regs.abp2;
        _M_regs.ip = _M_stack[fbp + 1].raw().p.second;
        _M_regs.fp = static_cast<size_t>(_M_stack[fbp + 2].raw().i);
        _M_regs.after_leaving_flag = true;
        atomic_thread_fence(memory_order_release);
        return true;
      } else
        return false;
    }

    void ThreadContext::set_error(int error)
    {
      _M_regs.abp = _M_regs.ac = _M_regs.lvc = _M_regs.abp2 = _M_regs.ac2 = _M_regs.sec = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue(0, 0.0, Reference(), error);
      _M_regs.after_leaving_flag = false;
      atomic_thread_fence(memory_order_release);
    }

    //
    // An VirtualMachineContext class.
    //

    VirtualMachineContext::~VirtualMachineContext() {}
    
    //
    // Other fuctions.
    //
    
    void initialize_gc() { priv::initialize_thread_stop_cont(); }

    void finalize_gc() { priv::finalize_thread_stop_cont(); }
  }
}
