/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <letin/const.hpp>
#include <letin/vm.hpp>
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
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
    // An Environment class.
    //

    Environment::~Environment() {}

    //
    // A VirtualMachine class.
    //

    VirtualMachine::~VirtualMachine() {}

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
    
    //
    // A ThreadContext class.
    //

    ThreadContext::ThreadContext(size_t stack_size)
    {
      _M_regs.abp = _M_regs.ac = _M_regs.lvc = _M_regs.abp2 = _M_regs.ac2 = 0;
      _M_regs.fun = nullptr;
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue();
      _M_stack = new Value[stack_size];
      _M_stack_size = stack_size;
    }

    bool ThreadContext::enter_to_fun()
    {
      if(_M_regs.abp2 + _M_regs.ac2 + 3 < _M_stack_size) {
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 0] = Value(_M_regs.abp, _M_regs.ac);
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 1] = Value(_M_regs.lvc, _M_regs.ip);
        _M_stack[_M_regs.abp2 + _M_regs.ac2 + 2] = Value(_M_regs.fun);
        _M_regs.abp = _M_regs.abp2;
        _M_regs.ac = _M_regs.ac2;
        _M_regs.abp2 = lvbp();
        _M_regs.lvc = _M_regs.ac2 = 0;
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
          _M_stack[fbp + 2].type() == VALUE_TYPE_FUN) {
        _M_regs.abp2 = _M_regs.abp;
        _M_regs.ac2 = 0;
        _M_regs.abp = _M_stack[fbp + 0].raw().p.first;
        _M_regs.ac = _M_stack[fbp + 0].raw().p.second;
        _M_regs.lvc = _M_stack[fbp + 1].raw().p.first;
        _M_regs.ip = _M_stack[fbp + 1].raw().p.second;
        _M_regs.fun = _M_stack[fbp + 2].raw().fun;
        return true;
      } else
        return false;
    }

    //
    // An VirtualMachineContext class.
    //

    VirtualMachineContext::~VirtualMachineContext() {}
  }
}
