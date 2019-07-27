/****************************************************************************
 *   Copyright (C) 2015, 2019 Łukasz Szpakowski.                            *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <cstring>
#include <letin/native.hpp>

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace native
  {
    namespace priv
    {
      //
      // Private types and private functions for a checking.
      //

      int check_int_value(VirtualMachine *vm, ThreadContext *context, Value &value)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        return (value.is_int() ? ERROR_SUCCESS : ERROR_INCORRECT_VALUE);
      }

      int check_float_value(VirtualMachine *vm, ThreadContext *context, Value &value)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        return (value.is_float() ? ERROR_SUCCESS : ERROR_INCORRECT_VALUE);
      }

      int check_ref_value(VirtualMachine *vm, ThreadContext *context, Value &value)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        if(!value.is_ref()) return ERROR_INCORRECT_VALUE;
        if(value.is_unique()) value.cancel_ref();
        return ERROR_SUCCESS;
      }

      int check_object_value(VirtualMachine *vm, ThreadContext *context, Value &value, int object_type)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        if(!value.is_ref()) return ERROR_INCORRECT_VALUE;
        if(value.r()->type() != object_type) return ERROR_INCORRECT_OBJECT;
        if(value.is_unique()) value.cancel_ref();
        return ERROR_SUCCESS;
      }

      int check_elem(VirtualMachine *vm, ThreadContext *context, Object &object, size_t i, CheckerFunction fun)
      {
        vm::Value tmp_value = object.elem(i);
        RegisteredReference tmp_r(context, true);
        if(tmp_value.is_lazy() || tmp_value.is_ref() || tmp_value.type() == VALUE_TYPE_CANCELED_REF)
          tmp_r = tmp_value.raw().r;
        int result = fun(vm, context, tmp_value);
        object.set_elem(i, tmp_value);
        return result;
      }

      int check_option_value(VirtualMachine *vm, ThreadContext *context, Value &value, CheckerFunction fun, int object_type_flag)
      {
        int error = check_object_value(vm, context, value, OBJECT_TYPE_TUPLE | object_type_flag);
        if(error != ERROR_SUCCESS) return error;
        if(value.r()->length() == 1 ? (value.r()->elem(0).is_int() && value.r()->elem(0).i() == 0) : false)
           return ERROR_SUCCESS;
        else if(value.r()->length() == 2 ? value.r()->elem(0).is_int() : false)
          return check_elem(vm, context, *(value.r()), 1, fun);
        else
          return ERROR_INCORRECT_OBJECT;
      }

      int check_either_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, CheckerFunction left, CheckerFunction right, int object_type_flag)
      {
        int error = check_object_value(vm, context, value, OBJECT_TYPE_TUPLE | object_type_flag);
        if(error != ERROR_SUCCESS) return error;
        if(value.r()->length() == 2 ? (value.r()->elem(0).is_int() && value.r()->elem(0).i() == 0) : false)
          return check_elem(vm, context, *(value.r()), 1, left);
        else if(value.r()->length() == 2 ? value.r()->elem(0).is_int() : false)
          return check_elem(vm, context, *(value.r()), 1, right);
        else
          return ERROR_INCORRECT_OBJECT;
      }

      int check_ref_value_for_fun(VirtualMachine *vm, ThreadContext *context, Value &value, function<int (VirtualMachine *, ThreadContext *, Reference)> fun, bool is_unique)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        if(!value.is_ref()) return ERROR_INCORRECT_VALUE;
        if((is_unique ? !value.is_unique() : value.is_unique())) return ERROR_INCORRECT_OBJECT;
        error = fun(vm, context, value.r());
        if(error != ERROR_SUCCESS) return error;
        if(value.is_unique()) value.cancel_ref();
        return ERROR_SUCCESS;
      }

      int check_object_value_for_fun(VirtualMachine *vm, ThreadContext *context, Value &value, function<int (VirtualMachine *, ThreadContext *, Object &)> fun, bool is_unique)
      {
        int error = vm->force(context, value);
        if(error != ERROR_SUCCESS) return error;
        if(!value.is_ref()) return ERROR_INCORRECT_VALUE;
        if((is_unique ? !value.is_unique() : value.is_unique())) return ERROR_INCORRECT_OBJECT;
        error = fun(vm, context, *(value.r().ptr()));
        if(error != ERROR_SUCCESS) return error;
        if(value.is_unique()) value.cancel_ref();
        return ERROR_SUCCESS;
      }

      //
      // Private functions for a coverting.
      //

      bool convert_option_value(const Value &value, ConverterFunction fun, bool &some_flag)
      {
        if(value.r()->elem(0).i() == 0) {
          some_flag = false;
          return true;
        } else {
          some_flag = true;
          return fun(value.r()->elem(1));
        }
      }

      bool convert_either_value(const Value &value, ConverterFunction left, ConverterFunction right, bool &right_flag)
      {
        if(value.r()->elem(0).i() == 0) {
          right_flag = false;
          return left(value.r()->elem(1));
        } else {
          right_flag = true;
          return right(value.r()->elem(1));
        }
      }

      //
      // Private types and private functions for a setting.
      //

      int set_elem(VirtualMachine *vm, ThreadContext *context, Value& value, RegisteredReference &r, SetterFunction fun, size_t i)
      {
        RegisteredReference tmp_elem_r(context, false);
        Value elem_value;
        int error = fun(vm, context, elem_value, tmp_elem_r);
        if(error != ERROR_SUCCESS) return error;
        r->set_elem(i, elem_value);
        return ERROR_SUCCESS;
      }

      int set_string_value(VirtualMachine *vm, ThreadContext *context, Value& value, RegisteredReference &tmp_r, const string &string)
      {
        tmp_r= vm->gc()->new_object(OBJECT_TYPE_IARRAY8, string.length(), context);
        if(tmp_r.is_null()) return ERROR_OUT_OF_MEMORY;
        copy(string.begin(), string.end(), reinterpret_cast<char *>(tmp_r->raw().is8));
        tmp_r.register_ref();
        value = Value(tmp_r);
        return ERROR_SUCCESS;
      }

      int set_cstring_value(VirtualMachine *vm, ThreadContext *context, Value& value, RegisteredReference &tmp_r, const char *string, size_t length, bool is_length)
      {
        size_t tmp_length = (is_length ? length : strlen(string));
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_IARRAY8, tmp_length, context);
        if(tmp_r.is_null()) return ERROR_OUT_OF_MEMORY;
        copy_n(string, tmp_length, reinterpret_cast<char *>(tmp_r->raw().is8));
        tmp_r.register_ref();
        value = Value(tmp_r);
        return ERROR_SUCCESS;
      }

      int set_ref_value_for_fun(VirtualMachine *vm, ThreadContext *context, Value& value, RegisteredReference &tmp_r, function<bool (VirtualMachine *, ThreadContext *, RegisteredReference &)> fun)
      {
        if(!fun(vm, context, tmp_r)) return ERROR_OUT_OF_MEMORY;
        value = Value(tmp_r);
        return ERROR_SUCCESS;
      }

      int set_object_value_for_fun(VirtualMachine *vm, ThreadContext *context, Value& value, RegisteredReference &tmp_r, function<Object *(VirtualMachine *, ThreadContext *)> fun)
      {
        tmp_r = fun(vm, context);
        if(tmp_r.is_null()) return ERROR_OUT_OF_MEMORY;
        tmp_r.register_ref();
        value = Value(tmp_r);
        return ERROR_SUCCESS;
      }

      ReturnValue return_value_for_setter_fun(VirtualMachine *vm, ThreadContext *context, SetterFunction fun)
      {
        RegisteredReference tmp_r(context, false);
        Value value;
        int error = fun(vm, context, value, tmp_r);
        if(error != ERROR_SUCCESS) return vm::ReturnValue::error(error);
        if(!tmp_r.has_nil()) vm::set_temporary_root_object(context, tmp_r);
        return ReturnValue(value);
      }
    }
  }
}
