/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <letin/format.hpp>
#include "impl_vm_base.hpp"
#include "util.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::util;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplVirtualMachineBase::~ImplVirtualMachineBase() {}

      bool ImplVirtualMachineBase::load(const vector<pair<void *, size_t>> &pairs, list<LoadingError> *errors, bool is_auto_freeing)
      {
        bool is_success = true;
        size_t fun_offset, var_offset;
        size_t fun_count = 0, var_count = 0;
        vector<unique_ptr<Program>> progs;
        bool is_relocable = true;
        for(size_t i = 0; i < pairs.size(); i++) {
          Program *prog = _M_loader->load(pairs[i].first, pairs[i].second);
          if(prog == nullptr) {
            if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
            is_success = false;
            continue;
          }
          if((prog->flags() & format::HEADER_FLAG_RELOCATABLE) == 0) {
            is_relocable = false;
            if(pairs.size() > 1) {
              if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_NO_RELOC));
              is_success = false;
              continue;
            }
          }
          progs.push_back(unique_ptr<Program>(prog));
          fun_count += prog->fun_count();
          var_count += prog->var_count();
        }
        if(!is_success) {
          _M_env.reset();
          return false;
        }

        if(is_relocable) {
          fun_offset = 0;
          var_offset = 0;
          for(size_t i = 0; i < progs.size(); i++) {
            Program *prog = progs[i].get();
            for(size_t j = 0; j < prog->symbol_count(); j++) {
              const format::Symbol *symbol = prog->symbols(j);
              switch(symbol->type) {
                case format::SYMBOL_TYPE_FUN | format::SYMBOL_TYPE_DEFINED:
                {
                  string name(symbol->name, symbol->length);
                  if(symbol->index >= prog->fun_count()) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FUN_INDEX, name));
                    is_success = false;
                  } else if(!_M_env.add_fun_index(name, fun_offset + symbol->index)) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FUN_SYM, name));
                    is_success = false;
                  }
                  break;
                }
                case format::SYMBOL_TYPE_VAR | format::SYMBOL_TYPE_DEFINED:
                {
                  string name(symbol->name, symbol->length);
                  if(symbol->index >= prog->var_count()) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_VAR_INDEX, name));
                    is_success = false;
                  } else if(!_M_env.add_var_index(name, var_offset + symbol->index)) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_VAR_SYM, name));
                    is_success = false;
                  }
                  break;
                }
              }
            }
            fun_offset += prog->fun_count();
            var_offset += prog->var_count();
          }
          if(!is_success) {
            _M_env.reset();
            return false;
          }

          for(size_t i = 0; i < progs.size(); i++) {
            Program *prog = progs[i].get();
            for(size_t j = 0; j < prog->symbol_count(); j++) {
              const format::Symbol *symbol = prog->symbols(j);
              switch(symbol->type) {
                case format::SYMBOL_TYPE_FUN:
                {
                  string name(symbol->name, symbol->length);
                  if(_M_env.fun_indexes().find(name) == _M_env.fun_indexes().end()) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_NO_FUN_SYM, name));
                    is_success = false;
                  }
                  break;
                }
                case format::SYMBOL_TYPE_VAR:
                {
                  string name(symbol->name, symbol->length);
                  if(_M_env.var_indexes().find(name) == _M_env.var_indexes().end()) {
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_NO_VAR_SYM, name));
                    is_success = false;
                  }
                  break;
                }
              }
            }
          }
          if(!is_success) {
            _M_env.reset();
            return false;
          }

          fun_offset = 0;
          var_offset = 0;
          for(size_t i = 0; i < progs.size(); i++) {
            Program *prog = progs[i].get();
            if(!prog->relocate(fun_offset, var_offset, _M_env.fun_indexes(), _M_env.var_indexes())) {
              if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_RELOC));
              is_success = false;
            }
            fun_offset += prog->fun_count();
            var_offset += prog->var_count();
          }
          if(!is_success) {
            _M_env.reset();
            return false;
          }
        }
        
        _M_env.set_fun_count(fun_count);
        _M_env.set_var_count(var_count);
        _M_has_entry = false;
        fun_offset = 0;
        var_offset = 0;
        for(size_t i = 0; i < progs.size(); i++) {
          Program *prog = progs[i].get();
          if(!load_prog(i, prog, fun_offset, var_offset, errors, (is_auto_freeing ? pairs[i].first : nullptr))) is_success = false;
          fun_offset += prog->fun_count();
          var_offset += prog->var_count();
        }
        if(!is_success) {
          _M_env.reset();
          return false;
        }
        return true;
      }

      bool ImplVirtualMachineBase::load_prog(size_t i, Program *prog, std::size_t fun_offset, std::size_t var_offset, list<LoadingError> *errors, void *data_to_free)
      {
        if(data_to_free != nullptr) _M_env.add_data_to_free(data_to_free);
        lock_guard<GarbageCollector> guard(*_M_gc);
        for(size_t j = 0; j < prog->fun_count(); j++) {
          format::Function &fun = prog->fun(j);
          _M_env.set_fun(fun_offset + j, Function(fun.arg_count, prog->code() + fun.addr, fun.instr_count));
        }
        unordered_map<uint32_t, Object *> objects;
        for(auto data_addr : prog->data_addrs()) {
          format::Object *data_object = prog->data(data_addr);
          Object *object = _M_gc->new_object(data_object->type, data_object->length);
          if(object == nullptr) {
            if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
            return false;
          }
          objects.insert(make_pair(data_addr, object));
        }
        for(auto pair : objects) {
          format::Object *data_object = prog->data(pair.first);
          Object *object = pair.second;
          switch(object->type()) {
            case OBJECT_TYPE_IARRAY8:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().is8[j] = data_object->is8[j];
              break;
            case OBJECT_TYPE_IARRAY16:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().is16[j] = data_object->is16[j];
              break;
            case OBJECT_TYPE_IARRAY32:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().is32[j] = data_object->is32[j];
              break;
            case OBJECT_TYPE_IARRAY64:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().is64[j] = data_object->is64[j];
              break;
            case OBJECT_TYPE_SFARRAY:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().sfs[j] = format_float_to_float(data_object->sfs[j]);
              break;
            case OBJECT_TYPE_DFARRAY:
              for(size_t j = 0; j < object->length(); j++)
                object->raw().dfs[j] = format_double_to_double(data_object->dfs[j]);
              break;
            case OBJECT_TYPE_RARRAY:
              for(size_t j = 0; j < object->length(); j++) {
                auto iter = objects.find(data_object->rs[j]);
                if(iter == objects.end()) {
                  if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
                  return false;
                }
                object->raw().rs[j] = Reference(iter->second);
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t j = 0; j < object->length(); j++) {
                switch(data_object->tuple_elem_types()[j]) {
                  case VALUE_TYPE_INT:
                    object->raw().tes[j] = TupleElement(data_object->tes[j].i);
                    break;
                  case VALUE_TYPE_FLOAT:
                    object->raw().tes[j] = TupleElement(format_double_to_double(data_object->tes[j].f));
                    break;
                  case VALUE_TYPE_REF:
                  {
                    auto iter = objects.find(data_object->tes[j].addr);
                    if(iter == objects.end()) {
                      if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
                      return false;
                    }
                    object->raw().tes[j] = TupleElement(Reference(iter->second));
                    break;
                  }
                  default:
                    if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
                    return false;
                }
                object->raw().tuple_elem_types()[j] = data_object->tuple_elem_types()[j];
              }
              break;
            default:
              if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
              return false;
          }
        }
        for(size_t j = 0; j < prog->var_count(); j++) {
          format::Value &var_value = prog->var(j);
          switch(var_value.type) {
            case VALUE_TYPE_INT:
              _M_env.set_var(var_offset + j, Value(var_value.i));
              break;
            case VALUE_TYPE_FLOAT:
              _M_env.set_var(var_offset + j, Value(format_double_to_double(var_value.f)));
              break;
            case VALUE_TYPE_REF:
            {
              auto iter = objects.find(var_value.addr);
              if(iter == objects.end()) {
                if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
                return false;
              }
              _M_env.set_var(var_offset + j, Value(Reference(iter->second)));
              break;
            }
            default:
              if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_FORMAT));
              return false;
          }
        }
        if((prog->flags() & format::HEADER_FLAG_LIBRARY) == 0) {
          if(_M_has_entry) {
            if(errors != nullptr) errors->push_back(LoadingError(i, LOADING_ERROR_ENTRY));
            return false;
          }
          _M_has_entry = true;
          _M_entry = prog->entry();
        }
        return true;
      }

      Thread ImplVirtualMachineBase::start(size_t i, const vector<Value> &args, function<void (const ReturnValue &)> fun)
      {
        ThreadContext *context = new ThreadContext(_M_env);
        Thread thread(context);
        context->set_gc(_M_gc);
        context->set_native_fun_handler(_M_native_fun_handler);
        context->start([this, i, fun, args, &thread]() {
          Thread thread2(thread);
          try {
            fun(start_in_thread(i, args, *(thread2.context()))); 
          } catch(...) {
            fun(ReturnValue(0, 0.0, Reference(), ERROR_EXCEPTION));
          }
        });
        return thread;
      }

      Environment &ImplVirtualMachineBase::env() { return _M_env; }

      bool ImplVirtualMachineBase::has_entry() { return _M_has_entry; }

      size_t ImplVirtualMachineBase::entry() { return _M_entry; }
    }
  }
}
