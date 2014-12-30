/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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

      bool ImplVirtualMachineBase::load(void *ptr, size_t size, bool is_auto_freeing)
      {
        unique_ptr<Program> prog(_M_loader->load(ptr, size));
        if(prog.get() == nullptr) return false;
        _M_env.set_fun_count(prog->fun_count());
        _M_env.set_var_count(prog->var_count());
        _M_env.set_auto_freeing(is_auto_freeing ? ptr : nullptr);
        lock_guard<GarbageCollector> guard(*_M_gc);
        for(size_t i = 0; i < prog->fun_count(); i++) {
          format::Function &fun = prog->fun(i);
          _M_env.set_fun(i, Function(fun.arg_count, prog->code() + fun.addr, fun.instr_count));
        }
        unordered_map<uint32_t, Object *> objects;
        for(auto data_addr : prog->data_addrs()) {
          format::Object *data_object = prog->data(data_addr);
          Object *object = _M_gc->new_object(data_object->type, data_object->length);
          if(object == nullptr) return nullptr;
          objects.insert(make_pair(data_addr, object));
        }
        for(auto pair : objects) {
          format::Object *data_object = prog->data(pair.first);
          Object *object = pair.second;
          switch(object->type()) {
            case OBJECT_TYPE_IARRAY8:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().is8[i] = data_object->is8[i];
              break;
            case OBJECT_TYPE_IARRAY16:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().is16[i] = data_object->is16[i];
              break;
            case OBJECT_TYPE_IARRAY32:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().is32[i] = data_object->is32[i];
              break;
            case OBJECT_TYPE_IARRAY64:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().is64[i] = data_object->is64[i];
              break;
            case OBJECT_TYPE_SFARRAY:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().sfs[i] = format_float_to_float(data_object->sfs[i]);
              break;
            case OBJECT_TYPE_DFARRAY:
              for(size_t i = 0; i < object->length(); i++)
                object->raw().dfs[i] = format_double_to_double(data_object->dfs[i]);
              break;
            case OBJECT_TYPE_RARRAY:
              for(size_t i = 0; i < object->length(); i++) {
                auto iter = objects.find(data_object->rs[i]);
                if(iter == objects.end()) return false;
                object->raw().rs[i] = Reference(iter->second);
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t i = 0; i < object->length(); i++) {
                switch(data_object->tuple_elem_types()[i]) {
                  case VALUE_TYPE_INT:
                    object->raw().tes[i] = TupleElement(data_object->tes[i].i);
                    break;
                  case VALUE_TYPE_FLOAT:
                    object->raw().tes[i] = TupleElement(format_double_to_double(data_object->tes[i].f));
                    break;
                  case VALUE_TYPE_REF:
                  {
                    auto iter = objects.find(data_object->tes[i].addr);
                    if(iter == objects.end()) return false;
                    object->raw().tes[i] = TupleElement(Reference(iter->second));
                    break;
                  }
                  default:
                    return false;
                }
                object->raw().tuple_elem_types()[i] = data_object->tuple_elem_types()[i];
              }
              break;
            default:
              return false;
          }
        }
        for(size_t i = 0; i < prog->var_count(); i++) {
          format::Value &var_value = prog->var(i);
          switch(var_value.type) {
            case VALUE_TYPE_INT:
              _M_env.set_var(i, Value(var_value.i));
              break;
            case VALUE_TYPE_FLOAT:
              _M_env.set_var(i, Value(format_double_to_double(var_value.f)));
              break;
            case VALUE_TYPE_REF:
            {
              auto iter = objects.find(var_value.addr);
              if(iter == objects.end()) return false;
              _M_env.set_var(i, Value(Reference(iter->second)));
              break;
            }
            default:
              return false;
          }
        }
        _M_has_entry = ((prog->flags() & format::HEADER_FLAG_LIBRARY) == 0);
        _M_entry = prog->entry();
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
