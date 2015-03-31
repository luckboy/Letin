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
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <letin/const.hpp>
#include <letin/vm.hpp>
#include "alloc/new_alloc.hpp"
#include "gc/mark_sweep_gc.hpp"
#include "vm/interp_vm.hpp"
#include "impl_loader.hpp"
#include "thread_stop_cont.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::opcode;

namespace letin
{
  namespace vm
  {
    //
    // Static inline functions.
    //

    static inline size_t object_size(int type, size_t length)
    {
      size_t elem_size;
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
          elem_size =  sizeof(Reference);
          break;
        case OBJECT_TYPE_TUPLE:
          elem_size = sizeof(TupleElement) + sizeof(TupleElementType);
          break;
        case OBJECT_TYPE_IO:
          elem_size = 0;
          break;
        default:
          return 0;
      }
      return (sizeof(Object) - max(sizeof(int64_t), sizeof(double))) + length * elem_size;
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
        case VALUE_TYPE_ERROR:
          return true;
        default:
          return false;
      }
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
        default:
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
          return true;
        case OBJECT_TYPE_TUPLE:
          _M_raw.tes[i] = TupleElement(value.raw().i);
          _M_raw.tuple_elem_types()[i] = TupleElementType(value.type());
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
        ifstream ifs(file_name);
        if(!ifs.good()) {
          if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_IO));
          is_success = false;
          continue;
        }
        ifs.read(ptr, size);
        if(!ifs.good()) {
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

    //
    // A NativeFunctionHandler class.
    //

    NativeFunctionHandler::~NativeFunctionHandler() {}

    //
    // A DefualtNativeFunctionHandler class.
    //

    DefaultNativeFunctionHandler::~DefaultNativeFunctionHandler() {}

    ReturnValue DefaultNativeFunctionHandler::invoke(VirtualMachine *vm, ThreadContext *context, int nfi, ArgumentList &args)
    {
      try {
        switch(nfi) {
          case NATIVE_FUN_ATOI:
          {
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
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
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
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
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
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
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
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
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
            if(args[0].type() != VALUE_TYPE_REF)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[0].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
            char c;
            cin >> c;
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
            if(args.length() != 2)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
            if(args[0].type() != VALUE_TYPE_INT)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[1].type() != VALUE_TYPE_REF)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[1].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
            cout << static_cast<char>(args[0].i());
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
            if(args.length() != 1)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
            if(args[0].type() != VALUE_TYPE_REF)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[0].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
            string str;
            getline(cin, str);
            context->regs().tmp_r = vm->gc()->new_object(OBJECT_TYPE_IARRAY8, str.length(), context);
            if(context->regs().tmp_r.is_null())
              return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
            copy(str.begin(), str.end(), context->regs().tmp_r->raw().is8);
            Reference r = vm->gc()->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2, context);
            if(r.is_null())
              return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
            atomic_thread_fence(memory_order_release);
            r->set_elem(0, Value(context->regs().tmp_r));
            r->set_elem(1, args[0]);
            args[0].cancel_ref();
            atomic_thread_fence(memory_order_release);
            context->regs().tmp_r = r;
            return ReturnValue(0, 0.0, r, ERROR_SUCCESS);
          }
          case NATIVE_FUN_PUT_STRING:
          {
            if(args.length() != 2)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_ARG_COUNT);
            if(args[0].type() != VALUE_TYPE_REF)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[0].r()->type() != OBJECT_TYPE_IARRAY8)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
            if(args[1].type() != VALUE_TYPE_REF)
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_VALUE);
            if(args[1].r()->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE))
              return ReturnValue(0, 0.0, Reference(), ERROR_INCORRECT_OBJECT);
            cout.write(reinterpret_cast<const char *>(args[0].r()->raw().is8), args[0].r()->length());
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
      } catch(bad_alloc &e) {
        return ReturnValue(0, 0.0, Reference(), ERROR_OUT_OF_MEMORY);
      } catch(...) {
        return ReturnValue(0, 0.0, Reference(), ERROR_EXCEPTION);
      }
    }

    //
    // A Program class.
    //

    bool Program::relocate(size_t fun_offset, size_t var_offset, const unordered_map<string, size_t> &fun_indexes, const unordered_map<string, size_t> &var_indexes)
    {
      for(size_t i = 0; i < _M_reloc_count; i++) {
        uint32_t addr = _M_relocs[i].addr;
        switch(_M_relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) {
          case format::RELOC_TYPE_ARG1_FUN:
          {
            if(opcode_to_arg_type1(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
            size_t index = _M_code[addr].arg1.i;
            if(!relacate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            _M_code[addr].arg1.i = index;
            break;
          }
          case format::RELOC_TYPE_ARG2_FUN:
          {
            if(opcode_to_arg_type2(_M_code[addr].opcode) != ARG_TYPE_IMM) return false;
            size_t index = _M_code[addr].arg2.i;
            if(!relacate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            _M_code[addr].arg2.i = index;
            break;
          }
          case format::RELOC_TYPE_ARG1_VAR:
          {
            if(opcode_to_arg_type1(_M_code[addr].opcode) != ARG_TYPE_GVAR) return false;
            size_t index = _M_code[addr].arg1.gvar;
            if(!relacate_index(index, var_offset, var_indexes, _M_relocs[i], format::SYMBOL_TYPE_VAR)) return false;
            _M_code[addr].arg1.gvar = index;
            break;
          }
          case format::RELOC_TYPE_ARG2_VAR:
          {
            if(opcode_to_arg_type2(_M_code[addr].opcode) != ARG_TYPE_GVAR) return false;
            size_t index = _M_code[addr].arg2.gvar;
            if(!relacate_index(index, var_offset, var_indexes, _M_relocs[i], format::SYMBOL_TYPE_VAR)) return false;
            _M_code[addr].arg2.gvar = index;
            break;
          }
          case format::RELOC_TYPE_ELEM_FUN:
          {
            auto data_addr_iter = _M_data_addrs.upper_bound(addr);
            if(data_addr_iter == _M_data_addrs.begin()) return false;
            data_addr_iter--;
            size_t data_object_addr = *data_addr_iter;
            format::Object *data_object = reinterpret_cast<format::Object *>(_M_data + data_object_addr);
            if(data_object_addr + 8 > addr) return false;
            size_t index;
            switch(data_object->type) {
              case OBJECT_TYPE_IARRAY32:
                if(data_object_addr + 8 + data_object->length * 4 <= addr) return false;
                if(((addr - (data_object_addr + 8)) & 3) != 0) return false;
                index = *reinterpret_cast<int32_t *>(_M_data + addr);
                break;
              case OBJECT_TYPE_IARRAY64:
              case OBJECT_TYPE_TUPLE:
                if(data_object_addr + 8 + data_object->length * 8 <= addr) return false;
                if(((addr - (data_object_addr + 8)) & 7) != 0) return false;
                if(data_object->type == OBJECT_TYPE_TUPLE) {
                  size_t j = (addr - (data_object_addr + 8)) >> 3;
                  if(data_object->tuple_elem_types()[j] != VALUE_TYPE_INT) return false;
                }
                index = *reinterpret_cast<int64_t *>(_M_data + addr);
                break;
              default:
                return false;
            }
            if(!relacate_index(index, fun_offset, fun_indexes, _M_relocs[i], format::SYMBOL_TYPE_FUN)) return false;
            switch(data_object->type) {
              case OBJECT_TYPE_IARRAY32:
                *reinterpret_cast<int32_t *>(_M_data + addr) = index;
                break;
              case OBJECT_TYPE_IARRAY64:
              case OBJECT_TYPE_TUPLE:
                *reinterpret_cast<int64_t *>(_M_data + addr) = index;
                break;
              default:
                return false;
            }
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

    bool Program::relacate_index(size_t &index, size_t offset, const unordered_map<string, size_t> &indexes, const format::Relocation &reloc, uint8_t symbol_type)
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

    //
    // A ThreadContext class.
    //

    ThreadContext::ThreadContext(const VirtualMachineContext &context, size_t stack_size) :
      _M_funs(context.funs()), _M_fun_count(context.fun_count()),
      _M_global_vars(context.vars()), _M_global_var_count(context.var_count())
    {
      _M_gc = nullptr;
      _M_native_fun_handler = nullptr;
      _M_regs.abp = _M_regs.ac = _M_regs.lvc = _M_regs.abp2 = _M_regs.ac2 = _M_regs.sec = 0;
      _M_regs.fp = static_cast<size_t>(-1);
      _M_regs.ip = 0;
      _M_regs.rv = ReturnValue();
      _M_regs.tmp_ptr = nullptr;
      _M_regs.tmp_r = Reference();
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
    // A VirtualMachineContext class.
    //

    VirtualMachineContext::~VirtualMachineContext() {}

    //
    // Other fuctions.
    //

    Loader *new_loader() { return new impl::ImplLoader(); }

    Allocator *new_allocator() { return new impl::NewAllocator(); }

    GarbageCollector *new_garbage_collector(Allocator *alloc)
    { return new impl::MarkSweepGarbageCollector(alloc); }

    VirtualMachine *new_virtual_machine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler)
    { return new impl::InterpreterVirtualMachine(loader, gc, native_fun_handler); }

    void initialize_gc() { priv::initialize_thread_stop_cont(); }

    void finalize_gc() { priv::finalize_thread_stop_cont(); }

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
        default:
          return os << "error";
      }
    }

    ostream &operator<<(ostream &os, const Object &object)
    {
      if((object.type() & ~OBJECT_TYPE_UNIQUE) != 0) os << "unique ";
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
        default:
          return os << "error";
      }
      os << (object.type() != OBJECT_TYPE_TUPLE ? "[" : "(");
      for(size_t i = 0; i < object.length(); i++) {
        os << object.elem(i);
        if(i + 1 < object.length()) os << ", ";
      }
      return os << (object.type() != OBJECT_TYPE_TUPLE ? "]" : ")");
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
        default:
          return "unknown error";
      }
    }
  }
}
