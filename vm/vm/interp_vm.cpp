/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <letin/const.hpp>
#include <letin/opcode.hpp>
#include <letin/vm.hpp>
#include "interp_vm.hpp"
#include "impl_vm_base.hpp"
#include "vm.hpp"
#include "util.hpp"

using namespace std;
using namespace letin::opcode;
using namespace letin::util;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      //
      // Static inline functions.
      //

      static inline bool get_int_for_const_value(ThreadContext &context, int64_t &i, const Value &value)
      {
        if(value.type() != VALUE_TYPE_INT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        i = value.raw().i;
        return true;
      }

      static inline bool get_float_for_const_value(ThreadContext &context, double &f, const Value &value)
      {
        if(value.type() != VALUE_TYPE_FLOAT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        f = value.raw().f;
        return true;
      }

      static inline bool get_ref_for_const_value(ThreadContext &context, Reference &r, const Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(value.raw().r->is_unique()) {
          context.set_error(ERROR_UNIQUE_OBJECT);
          return false;
        }
        r = value.raw().r;
        return true;
      }

      static inline bool get_int_value(ThreadContext &context, Value &out_value, const Value &value)
      {
        if(value.type() != VALUE_TYPE_INT) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_INT) {
            out_value = value;
            return true;
          } else {
            context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        out_value = Value(value.raw().i);
        return true;
      }

      static inline bool get_int_value(ThreadContext &context, Value &value, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_int_value(context, value, context.local_var(arg.lvar));
          case ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_int_value(context, value, context.arg(arg.arg));
          case ARG_TYPE_IMM:
            value = Value(arg.i);
            return true;
          case ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_int_value(context, value, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      static inline bool get_float_value(ThreadContext &context, Value &out_value, const Value &value)
      {
        if(value.type() != VALUE_TYPE_FLOAT) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_FLOAT) {
            out_value = value;
            return true;
          } else {
            context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        out_value = Value(value.raw().f);
        return true;
      }

      static inline bool get_float_value(ThreadContext &context, Value &value, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case opcode::ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_float_value(context, value, context.local_var(arg.lvar));
          case opcode::ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_float_value(context, value, context.arg(arg.arg));
          case opcode::ARG_TYPE_IMM:
            value = Value(format_float_to_float(arg.f));
            return true;
          case opcode::ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_float_value(context, value, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      static inline bool get_ref_value(ThreadContext &context, Value &out_value, Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_REF) {
            out_value = value;
            value.lazily_cancel_ref();
            return true;
          } else {
            if(value.type() == VALUE_TYPE_CANCELED_REF)
              context.set_error(ERROR_AGAIN_USED_UNIQUE);
            else
              context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        if(value.raw().r->is_unique()) value.cancel_ref();
        out_value = Value(value.raw().r);
        return true;
      }

      static inline bool get_ref_value_for_const_value(ThreadContext &context, Value &out_value, const Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          if(value.type() == VALUE_TYPE_CANCELED_REF)
            context.set_error(ERROR_AGAIN_USED_UNIQUE);
          else
            context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(value.raw().r->is_unique()) {
          context.set_error(ERROR_UNIQUE_OBJECT);
          return false;
        }
        out_value = Value(value.raw().r);
        return true;
      }

      static inline bool get_ref_value(ThreadContext &context, Value &value, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case opcode::ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_ref_value(context, value, context.local_var(arg.lvar));
          case opcode::ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_ref_value(context, value, context.arg(arg.arg));
          case opcode::ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_ref_value_for_const_value(context, value, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      static inline Object *new_object(ThreadContext &context, int type, int64_t length)
      {
        Object *object = context.gc()->new_object_for_length64(type, length, &context);
        if(object == nullptr) {
          context.set_error(ERROR_OUT_OF_MEMORY);
          return nullptr;
        }
        return object;
      }

      static inline Object *new_unique_pair(ThreadContext &context, const Value &value1, const Value &value2)
      {
        Object *object = new_object(context, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2);
        if(object == nullptr) return nullptr;
        object->raw().tes[0] = value1.tuple_elem();
        object->raw().tuple_elem_types()[0] = value1.tuple_elem_type();
        object->raw().tes[1] = value2.tuple_elem();
        object->raw().tuple_elem_types()[1] = value2.tuple_elem_type();
        return object;
      }

      static inline bool check_value_type(ThreadContext &context, const Value &value, int type)
      {
        if(value.type() != type) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        return true;
      }

      static inline bool check_shared_for_value(ThreadContext &context, const Value &value)
      {
        if(value.is_unique()) {
          context.set_error(ERROR_UNIQUE_OBJECT);
          return false;
        }
        return true;
      }

      static inline bool check_object_type(ThreadContext &context, const Object &object, int type)
      {
        if(object.type() != type) {
          context.set_error(ERROR_INCORRECT_OBJECT);
          return false;
        }
        return true;
      }

      static inline bool check_shared_for_object(ThreadContext &context, const Object &object)
      {
        if(object.is_unique()) {
          context.set_error(ERROR_UNIQUE_OBJECT);
          return false;
        }
        return true;
      }

      static inline bool check_object_elem_index(ThreadContext &context, const Object &object, int64_t i)
      {
        if(static_cast<uint64_t>(i) >= static_cast<uint64_t>(object.length())) {
          context.set_error(ERROR_INDEX_OF_OUT_BOUNDS);
          return false;
        }
        return true;
      }

      static inline bool set_lazy_values_as_shared(ThreadContext &context, const Value &value)
      {
        if(value.type() == VALUE_TYPE_LAZY_VALUE_REF) {
          Reference r = value.raw().r;
          while(r->type() == OBJECT_TYPE_LAZY_VALUE) {
            lock_guard<LazyValueMutex> guard(r->raw().lzv.mutex);
            if(r->raw().lzv.must_be_shared) break;
            if(r->raw().lzv.value.is_unique()) {
              context.set_error(ERROR_UNIQUE_OBJECT);
              return false;
            }
            r->raw().lzv.must_be_shared = true;
            if(r->raw().lzv.value.type() != VALUE_TYPE_LAZY_VALUE_REF) break;
            r = r->raw().lzv.value.raw().r;
          }
        }
        return true;
      }

      static inline bool add_object_lengths(ThreadContext &context, size_t &length, size_t length1, size_t length2)
      {
        length = length1 + length2;
        if((length1 | length2) > length) {
          context.set_error(ERROR_OUT_OF_MEMORY);
          return false;
        }
        return true;
      }

      static inline bool push_arg(ThreadContext &context, const Value value)
      {
        if(!context.push_arg(value)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool check_pushed_arg_count(ThreadContext &context, size_t count)
      {
        if(context.regs().ac2 != count) {
          context.set_error(ERROR_INCORRECT_ARG_COUNT);
          return false;
        }
        return true;
      }

      static inline bool push_tmp_ac2(ThreadContext &context)
      {
        if(!context.push_tmp_ac2()) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_tmp_ac2(ThreadContext &context)
      {
        if(!context.pop_tmp_ac2()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool push_tmp_ac2_and_after_leaving_flag1(ThreadContext &context)
      {
        if(!context.push_tmp_ac2_and_after_leaving_flag1()) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_tmp_ac2_and_after_leaving_flag1(ThreadContext &context)
      {
        if(!context.pop_tmp_ac2_and_after_leaving_flag1()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool push_try_regs(ThreadContext &context)
      {
        if(!context.push_try_regs()) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_try_regs(ThreadContext &context)
      {
        if(!context.pop_try_regs()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool push_ai(ThreadContext &context)
      {
        if(!context.push_ai()) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_ai(ThreadContext &context)
      {
        if(!context.pop_ai()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool push_locked_lazy_value_ref(ThreadContext &context, Reference r)
      {
        if(!context.push_locked_lazy_value_ref(r)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_locked_lazy_value_ref(ThreadContext &context)
      {
        if(!context.pop_locked_lazy_value_ref()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool get_locked_lazy_value_ref(ThreadContext &context, Reference &r)
      {
        if(!context.get_locked_lazy_value_ref(r)) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool push_tmp_value(ThreadContext &context, const Value &value)
      {
        if(!context.push_tmp_value(value)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      static inline bool pop_tmp_value(ThreadContext &context)
      {
        if(!context.pop_tmp_value()) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool get_tmp_value(ThreadContext &context, Value &value)
      {
        if(!context.get_tmp_value(value)) {
          context.set_error(ERROR_EMPTY_STACK);
          return false;
        }
        return true;
      }

      static inline bool check_fun(ThreadContext &context, size_t i)
      {
        if(i >= context.fun_count()) {
          context.set_error(ERROR_NO_FUN);
          return false;
        }
        const Function &fun = context.fun(i);
        if(fun.is_error()) {
          context.set_error(ERROR_INCORRECT_FUN);
          return false;
        }
        if(fun.arg_count() != context.regs().ac2) {
          context.set_error(ERROR_INCORRECT_ARG_COUNT);
          return false;
        }
        return true;
      }

      static inline bool cancel_ref_for_unique(Value &value)
      {
        if(value.is_unique()) {
          return value.cancel_ref();
        } else if(value.is_lazy()) {
          value.lazily_cancel_ref();
          return true;
        }
        return false;
      }

      static inline bool restore_and_pop_regs_for_force(ThreadContext &context)
      {
        context.regs().after_leaving_flag_index = 0;
        context.pop_args();
        if(!pop_locked_lazy_value_ref(context)) return false;
        if(!pop_ai(context)) return false;
        if(!pop_tmp_ac2_and_after_leaving_flag1(context)) return false;
        if(!context.regs().arg_instr_flag) context.restore_abp2_and_ac2();
        return true;
      }

      //
      // Static functions.
      //

      static Value return_value_to_int_value_for_eager_eval(const ReturnValue &value)
      { return Value(value.raw().i); }

      static Value return_value_to_float_value_for_eager_eval(const ReturnValue &value)
      { return Value(value.raw().f); }

      static Value return_value_to_ref_value_for_eager_eval(const ReturnValue &value)
      { return Value(value.raw().r); }

      static Value return_value_to_int_value_for_lazy_eval(const ReturnValue &value)
      {
        if(!value.raw().r->is_lazy())
          return Value(value.raw().i);
        else
          return Value::lazy_value_ref(value.raw().r, value.raw().i != 0);
      }

      static Value return_value_to_float_value_for_lazy_eval(const ReturnValue &value)
      {
        if(!value.raw().r->is_lazy())
          return Value(value.raw().f);
        else
          return Value::lazy_value_ref(value.raw().r, value.raw().i != 0);
      }

      static Value return_value_to_ref_value_for_lazy_eval(const ReturnValue &value)
      {
        if(!value.raw().r->is_lazy())
          return Value(value.raw().r);
        else
          return Value::lazy_value_ref(value.raw().r, value.raw().i != 0);
      }

      //
      // An InterpreterVirtualMachine class.
      //

      InterpreterVirtualMachine::InterpreterVirtualMachine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy, function<void ()> exit_fun) :
        ImplVirtualMachineBase(loader, gc, native_fun_handler, eval_strategy, exit_fun)
      {
        if(_M_eval_strategy->is_eager()) {
          _M_return_value_to_int_value = return_value_to_int_value_for_eager_eval;
          _M_return_value_to_float_value = return_value_to_float_value_for_eager_eval;
          _M_return_value_to_ref_value = return_value_to_ref_value_for_eager_eval;
          _M_force_pushed_args = &InterpreterVirtualMachine::force_pushed_args_for_eager_eval;
        } else {
          _M_return_value_to_int_value = return_value_to_int_value_for_lazy_eval;
          _M_return_value_to_float_value = return_value_to_float_value_for_lazy_eval;
          _M_return_value_to_ref_value = return_value_to_ref_value_for_lazy_eval;
          _M_force_pushed_args = &InterpreterVirtualMachine::force_pushed_args_for_lazy_eval;
        }
      }

      InterpreterVirtualMachine::~InterpreterVirtualMachine() {}

      int InterpreterVirtualMachine::force(ThreadContext *context, Value &value)
      {
        if(!force_value_and_interpret(*context, value)) return context->regs().rv.error();
        return ERROR_SUCCESS;
      }

      int InterpreterVirtualMachine::fully_force(ThreadContext *context, Value &value)
      {
        if(!fully_force_value(*context, value)) return context->regs().rv.error();
        return ERROR_SUCCESS;
      }

      ReturnValue InterpreterVirtualMachine::invoke_fun(ThreadContext *context, size_t i, const ArgumentList &args)
      {
        for(size_t j = 0; j < args.length(); j++) {
          if(!push_arg(*context, args[j])) return context->regs().rv;
        }
        if(!call_fun_for_force(*context, i)) {
          if(context->regs().rv.error() == ERROR_SUCCESS) interpret(*context);
        }
        return context->regs().rv;
      }

      int InterpreterVirtualMachine::fully_force_return_value(ThreadContext *context)
      {
        if(!fully_force_rv(*context)) return context->regs().rv.error();
        return ERROR_SUCCESS;
      }

      ReturnValue InterpreterVirtualMachine::start_in_thread(size_t i, const vector<Value> &args, ThreadContext &context, bool is_force)
      {
        for(auto arg : args) if(!push_arg(context, arg)) return context.regs().rv;
        if(!call_fun_for_force(context, i)) {
          if(context.regs().rv.error() == ERROR_SUCCESS) {
             interpret(context); 
             if(is_force) fully_force_rv(context);
          }
        }
        context.pop_args();
        return context.regs().rv;
      }

      void InterpreterVirtualMachine::interpret(ThreadContext &context)
      { while(interpret_instr(context)); }

      inline bool InterpreterVirtualMachine::get_int(ThreadContext &context, int64_t &i, Value &value)
      {
        if(value.type() != VALUE_TYPE_INT) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_INT) {
            return force_int(context, i, value);
          } else {
            context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        i = value.raw().i;
        return true;
      }

      inline bool InterpreterVirtualMachine::get_int(ThreadContext &context, int64_t &i, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_int(context, i, context.local_var(arg.lvar));
          case ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_int(context, i, context.arg(arg.arg));
          case ARG_TYPE_IMM:
            i = arg.i;
            return true;
          case ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_int_for_const_value(context, i, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      inline bool InterpreterVirtualMachine::get_float(ThreadContext &context, double &f, Value &value)
      {
        if(value.type() != VALUE_TYPE_FLOAT) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_FLOAT) {
            return force_float(context, f, value);
          } else {
            context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        f = value.raw().f;
        return true;
      }

      inline bool InterpreterVirtualMachine::get_float(ThreadContext &context, double &f, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case opcode::ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_float(context, f, context.local_var(arg.lvar));
          case opcode::ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_float(context, f, context.arg(arg.arg));
          case opcode::ARG_TYPE_IMM:
            f = format_float_to_float(arg.f);
            return true;
          case opcode::ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_float_for_const_value(context, f, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      inline bool InterpreterVirtualMachine::get_ref(ThreadContext &context, Reference &r, Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          if(value.is_lazy() && value.raw().r->raw().lzv.value_type == VALUE_TYPE_REF) {
            return force_ref(context, r, value);
          } else {
            if(value.type() == VALUE_TYPE_CANCELED_REF)
              context.set_error(ERROR_AGAIN_USED_UNIQUE);
            else
              context.set_error(ERROR_INCORRECT_VALUE);
            return false;
          }
        }
        if(value.raw().r->is_unique()) value.cancel_ref();
        r = value.raw().r;
        return true;
      }

      inline bool InterpreterVirtualMachine::get_ref(ThreadContext &context, Reference &r, uint32_t arg_type, Argument arg)
      {
        switch(arg_type) {
          case opcode::ARG_TYPE_LVAR:
            if(arg.lvar >= context.regs().lvc) {
              context.set_error(ERROR_NO_LOCAL_VAR);
              return false;
            }
            return get_ref(context, r, context.local_var(arg.lvar));
          case opcode::ARG_TYPE_ARG:
            if(arg.arg >= context.regs().ac) {
              context.set_error(ERROR_NO_ARG);
              return false;
            }
            return get_ref(context, r, context.arg(arg.arg));
          case opcode::ARG_TYPE_GVAR:
            if(arg.gvar >= context.global_var_count()) {
              context.set_error(ERROR_NO_GLOBAL_VAR);
              return false;
            }
            return get_ref_for_const_value(context, r, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      bool InterpreterVirtualMachine::interpret_instr(ThreadContext &context)
      {
        if(context.regs().fp == static_cast<size_t>(-1)) return false;
        const Function &fun = context.fun(context.regs().fp);
        if(context.regs().ip >= fun.raw().instr_count) {
          context.set_error(ERROR_NO_INSTR);
          return true;
        }
        const Instruction &instr = fun.raw().instrs[context.regs().ip];
        context.regs().ip++;
        if(context.regs().after_leaving_flags[1]) {
          if(context.regs().rv.raw().error == ERROR_SUCCESS) {
            Reference locked_lazy_value_r;
            if(!get_locked_lazy_value_ref(context, locked_lazy_value_r)) return false;
            if(!_M_eval_strategy->post_leave_from_fun_for_force(this, &context, locked_lazy_value_r->raw().lzv.fun, locked_lazy_value_r->raw().lzv.value_type))
              return false;
          }
          if(opcode_to_instr(instr.opcode) == INSTR_ARG) context.regs().arg_instr_flag = true;
          if(!restore_and_pop_regs_for_force(context)) return false;
          if(opcode_to_instr(instr.opcode) == INSTR_ARG) context.regs().arg_instr_flag = false;
        }
        switch(opcode_to_instr(instr.opcode)) {
          case INSTR_LET:
          {
            Value value = interpret_op(context, instr);
            context.pop_args();
            if(!value.is_error())
              if(!context.push_local_var(value)) context.set_error(ERROR_STACK_OVERFLOW);
            context.regs().rv.raw().r = Reference();
            context.regs().gc_tmp_ptr = nullptr;
            break;
          }
          case INSTR_IN:
            context.in();
            break;
          case INSTR_RET:
          {
            Value value = interpret_op(context, instr);
            if(!value.is_error()) {
              if(!leave_from_fun(context)) context.set_error(ERROR_EMPTY_STACK);
              context.regs().rv = value;
            }
            context.regs().gc_tmp_ptr = nullptr;
            break;
          }
          case INSTR_JC:
          {
            int64_t i;
            if(get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1))
              if(i != 0) context.regs().ip += instr.arg2.i;
            break;
          }
          case INSTR_JUMP:
            context.regs().ip += instr.arg1.i;
            break;
          case INSTR_ARG:
          {
            context.hide_args();
            context.regs().arg_instr_flag = true;
            Value value = interpret_op(context, instr);
            context.regs().arg_instr_flag = false;
            if(!value.is_error()) {
              context.restore_abp2_and_ac2();
              if(!context.push_arg(value)) context.set_error(ERROR_STACK_OVERFLOW);
            }
            context.regs().rv.raw().r = Reference();
            context.regs().gc_tmp_ptr = nullptr;
            break;
          }
          case INSTR_RETRY:
            if(context.regs().ac == context.regs().ac2) {
              for(size_t i = 0; i < context.regs().ac; i++)
                context.arg(i).safely_assign_for_gc(context.pushed_arg(i));
              context.pop_args_and_local_vars();
            } else
              context.set_error(ERROR_INCORRECT_ARG_COUNT);
            context.regs().ip = 0;
            break;
          case INSTR_LETTUPLE:
          {
            Value value;
            if(!context.regs().after_leaving_flags[1]) context.regs().ai = 0;
            if(!context.regs().after_leaving_flags[1] || context.regs().ai != static_cast<uint64_t>(-1)) {
              value = interpret_op(context, instr);
              context.pop_args();
              context.regs().ai = static_cast<uint64_t>(-1);
              if(!value.is_error()) {
                if(!push_tmp_value(context, value)) value = Value();
              }
            } else {
              if(!get_tmp_value(context, value)) value = Value();
            }
            if(!value.is_error()) {
              Reference r;
              if(get_ref(context, r, value)) {
                if(pop_tmp_value(context)) { 
                  if((r->type() & ~OBJECT_TYPE_UNIQUE) == OBJECT_TYPE_TUPLE) {
                    uint32_t local_var_count = opcode_to_local_var_count(instr.opcode);
                    if(r->length() == local_var_count) {
                      for(size_t i = 0; i < local_var_count; i++) {
                        Value elem_value(r->raw().tuple_elem_types()[i], r->raw().tes[i]);
                        if(!context.push_local_var(elem_value)) context.set_error(ERROR_STACK_OVERFLOW);
                      }
                    } else
                      context.set_error(ERROR_INCORRECT_OBJECT);
                  } else
                    context.set_error(ERROR_INCORRECT_OBJECT);
                }
              }
            }
            context.regs().rv.raw().r = Reference();
            context.regs().gc_tmp_ptr = nullptr;
            break;
          }
          case INSTR_THROW:
          {
            Reference r;
            if(get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1))
              context.set_error(ERROR_USER_EXCEPTION, r);
          }
        }
        atomic_thread_fence(memory_order_release);
        return true;
      }

      Value InterpreterVirtualMachine::interpret_op(ThreadContext &context, const Instruction &instr)
      {
        switch(opcode_to_op(instr.opcode)) {
          case OP_ILOAD:
          {
            Value value;
            if(!get_int_value(context, value, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return value;
          }
          case OP_ILOAD2:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value((i1 << 32) | (i2 & 0xffffffff));
          }
          case OP_INEG:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(-i);
          }
          case OP_IADD:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 + i2);
          }
          case OP_ISUB:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 - i2);
          }
          case OP_IMUL:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 * i2);
          }
          case OP_IDIV:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(i2 == 0) {
              context.set_error(ERROR_DIV_BY_ZERO);
              return Value();
            }
            return Value(i1 / i2);
          }
          case OP_IMOD:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(i2 == 0) {
              context.set_error(ERROR_DIV_BY_ZERO);
              return Value();
            }
            return Value(i1 % i2);
          }
          case OP_INOT:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(~i);
          }
          case OP_IAND:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 & i2);
          }
          case OP_IOR:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 | i2);
          }
          case OP_IXOR:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 ^ i2);
          }
          case OP_ISHL:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 << i2);
          }
          case OP_ISHR:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 >> i2);
          }
          case OP_ISHRU:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(static_cast<std::int64_t>(static_cast<std::uint64_t>(i1) >> i2));
          }
          case OP_IEQ:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 == i2 ? 1 : 0);
          }
          case OP_INE:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 != i2 ? 1 : 0);
          }
          case OP_ILT:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 < i2 ? 1 : 0);
          }
          case OP_IGE:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 >= i2 ? 1 : 0);
          }
          case OP_IGT:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 > i2 ? 1 : 0);
          }
          case OP_ILE:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(i1 <= i2 ? 1 : 0);
          }
          case OP_FLOAD:
          {
            Value value;
            if(!get_float_value(context, value, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return value;
          }
          case OP_FLOAD2:
          {
            std::int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            format::Double x;
            x.dword = (i1 << 32) | (i2 & 0xffffffff);
            return format_double_to_double(x);
          }
          case OP_FNEG:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(-f);
          }
          case OP_FADD:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 + f2);
          }
          case OP_FSUB:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 - f2);
          }
          case OP_FMUL:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 * f2);
          }
          case OP_FDIV:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 / f2);
          }
          case OP_FEQ:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 == f2 ? 1 : 0);
          }
          case OP_FNE:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 != f2 ? 1 : 0);
          }
          case OP_FLT:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 < f2 ? 1 : 0);
          }
          case OP_FGE:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 >= f2 ? 1 : 0);
          }
          case OP_FGT:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 > f2 ? 1 : 0);
          }
          case OP_FLE:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(f1 <= f2 ? 1 : 0);
          }
          case OP_RLOAD:
          {
            Value value;
            if(!get_ref_value(context, value, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return value;
          }
          case OP_REQ:
          {
            Reference r1, r2;
            if(opcode_to_arg_type1(instr.opcode) != ARG_TYPE_GVAR && opcode_to_arg_type2(instr.opcode) != ARG_TYPE_GVAR) {
              context.set_error(ERROR_INCORRECT_INSTR);
              return Value();
            }
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 == r2 ? 1 : 0);
          }
          case OP_RNE:
          {
            Reference r1, r2;
            if(opcode_to_arg_type1(instr.opcode) != ARG_TYPE_GVAR && opcode_to_arg_type2(instr.opcode) != ARG_TYPE_GVAR) {
              context.set_error(ERROR_INCORRECT_INSTR);
              return Value();
            }
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 != r2 ? 1 : 0);
          }
          case OP_RIARRAY8:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY8, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_INT)) return Value();
              r->raw().is8[i] = context.pushed_arg(i).raw().i;
            }
            return Value(r);
          }
          case OP_RIARRAY16:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY16, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_INT)) return Value();
              r->raw().is16[i] = context.pushed_arg(i).raw().i;
            }
            return Value(r);
          }
          case OP_RIARRAY32:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY32, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_INT)) return Value();
              r->raw().is32[i] = context.pushed_arg(i).raw().i;
            }
            return Value(r);
          }
          case OP_RIARRAY64:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY64, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_INT)) return Value();
              r->raw().is64[i] = context.pushed_arg(i).raw().i;
            }
            return Value(r);
          }
          case OP_RSFARRAY:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_SFARRAY, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_FLOAT)) return Value();
              r->raw().sfs[i] = context.pushed_arg(i).raw().f;
            }
            return Value(r);
          }
          case OP_RDFARRAY:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_DFARRAY, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_FLOAT)) return Value();
              r->raw().dfs[i] = context.pushed_arg(i).raw().f;
            }
            return Value(r);
          }
          case OP_RRARRAY:
          {
            if(!(this->*_M_force_pushed_args)(context)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_RARRAY, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_value_type(context, context.pushed_arg(i), VALUE_TYPE_REF)) return Value();
              if(!check_shared_for_object(context, *(context.pushed_arg(i).raw().r))) return Value();
              r->raw().rs[i] = context.pushed_arg(i).raw().r;
            }
            atomic_thread_fence(memory_order_release);
            return Value(r);
          }
          case OP_RTUPLE:
          {
            Reference r(new_object(context, OBJECT_TYPE_TUPLE, context.regs().ac2));
            if(r.is_null()) return Value();
            for(size_t i = 0; i < context.regs().ac2; i++) {
              if(!check_shared_for_value(context, context.pushed_arg(i))) return Value();
              if(!set_lazy_values_as_shared(context, context.pushed_arg(i))) return Value();
              r->raw().tes[i] = context.pushed_arg(i).tuple_elem();
              r->raw().tuple_elem_types()[i] = context.pushed_arg(i).tuple_elem_type();
            }
            atomic_thread_fence(memory_order_release);
            return Value(r);
          }
          case OP_RIANTH8:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().is8[i]);
          }
          case OP_RIANTH16:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().is16[i]);
          }
          case OP_RIANTH32:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().is32[i]);
          }
          case OP_RIANTH64:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().is64[i]);
          }
          case OP_RSFANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().sfs[i]);
          }
          case OP_RDFANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().dfs[i]);
          }
          case OP_RRANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_RARRAY)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().rs[i]);
          }
          case OP_RTNTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            return Value(r->raw().tuple_elem_types()[i], r->raw().tes[i]);
          }
          case OP_RIALEN8:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RIALEN16:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RIALEN32:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RIALEN64:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RSFALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RDFALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RRALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_RARRAY)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RTLEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE)) return Value();
            return Value(static_cast<int64_t>(r->length()));
          }
          case OP_RIACAT8:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_IARRAY8)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_IARRAY8)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY8, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().is8, r1->length(), r->raw().is8);
            copy_n(r2->raw().is8, r2->length(), r->raw().is8 + r1->length());
            return Value(r);
          }
          case OP_RIACAT16:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_IARRAY16)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_IARRAY16)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY16, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().is16, r1->length(), r->raw().is16);
            copy_n(r2->raw().is16, r2->length(), r->raw().is16 + r1->length());
            return Value(r);
          }
          case OP_RIACAT32:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_IARRAY32)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_IARRAY32)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY32, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().is32, r1->length(), r->raw().is32);
            copy_n(r2->raw().is32, r2->length(), r->raw().is32 + r1->length());
            return Value(r);
          }
          case OP_RIACAT64:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_IARRAY64)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_IARRAY64)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY64, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().is64, r1->length(), r->raw().is64);
            copy_n(r2->raw().is64, r2->length(), r->raw().is64 + r1->length());
            return Value(r);
          }
          case OP_RSFACAT:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_SFARRAY)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_SFARRAY)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_SFARRAY, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().sfs, r1->length(), r->raw().sfs);
            copy_n(r2->raw().sfs, r2->length(), r->raw().sfs + r1->length());
            return Value(r);
          }
          case OP_RDFACAT:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_DFARRAY)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_DFARRAY)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_DFARRAY, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().dfs, r1->length(), r->raw().dfs);
            copy_n(r2->raw().dfs, r2->length(), r->raw().dfs + r1->length());
            return Value(r);
          }
          case OP_RRACAT:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_RARRAY)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_RARRAY)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_RARRAY, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().rs, r1->length(), r->raw().rs);
            copy_n(r2->raw().rs, r2->length(), r->raw().rs + r1->length());
            atomic_thread_fence(memory_order_release);
            return Value(r);
          }
          case OP_RTCAT:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_TUPLE)) return Value();
            if(!check_object_type(context, *r2, OBJECT_TYPE_TUPLE)) return Value();
            size_t object_length;
            if(!add_object_lengths(context, object_length, r1->length(), r2->length())) return Value();
            Reference r(new_object(context, OBJECT_TYPE_TUPLE, object_length));
            if(r.is_null()) return Value();
            copy_n(r1->raw().tes, r1->length(), r->raw().tes);
            copy_n(r2->raw().tes, r2->length(), r->raw().tes + r1->length());
            copy_n(r1->raw().tuple_elem_types(), r1->length(), r->raw().tuple_elem_types());
            copy_n(r2->raw().tuple_elem_types(), r2->length(), r->raw().tuple_elem_types() + r1->length());
            atomic_thread_fence(memory_order_release);
            return Value(r);
          }
          case OP_RTYPE:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(r->type());
          }
          case OP_ICALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!context.regs().after_leaving_flags[0]) {
              if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
              if(!call_fun(context, static_cast<uint32_t>(i), VALUE_TYPE_INT)) return Value();
            }
            context.regs().after_leaving_flags[0] = false;
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            if(!_M_eval_strategy->post_leave_from_fun(this, &context, static_cast<uint32_t>(i), VALUE_TYPE_INT)) return Value();
            context.pop_args();
            return _M_return_value_to_int_value(context.regs().rv);
          }
          case OP_FCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!context.regs().after_leaving_flags[0]) {
              if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
              if(!call_fun(context, static_cast<uint32_t>(i), VALUE_TYPE_FLOAT)) return Value();
            }
            context.regs().after_leaving_flags[0] = false;
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            if(!_M_eval_strategy->post_leave_from_fun(this, &context, static_cast<uint32_t>(i), VALUE_TYPE_FLOAT)) return Value();
            context.pop_args();
            return _M_return_value_to_float_value(context.regs().rv);
          }
          case OP_RCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!context.regs().after_leaving_flags[0]) {
              if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
              if(!call_fun(context, static_cast<uint32_t>(i), VALUE_TYPE_REF)) return Value();
            }
            context.regs().after_leaving_flags[0] = false;
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            if(!_M_eval_strategy->post_leave_from_fun(this, &context, static_cast<uint32_t>(i), VALUE_TYPE_REF)) return Value();
            context.pop_args();
            return _M_return_value_to_ref_value(context.regs().rv);
          }
          case OP_ITOF:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(static_cast<double>(i));
          }
          case OP_FTOI:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(static_cast<int64_t>(f));
          }
          case OP_INCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.invoke_native_fun(this, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            return Value(rv.raw().i);
          }
          case OP_FNCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.invoke_native_fun(this, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            return Value(rv.raw().f);
          }
          case OP_RNCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(context.regs().arg_instr_flag) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.invoke_native_fun(this, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            if(context.regs().arg_instr_flag) if(!pop_tmp_ac2(context)) return Value();
            return Value(rv.raw().r);
          }
          case OP_RUIAFILL8:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE, i1));
            if(r.is_null()) return Value();
            fill_n(r->raw().is8, i1, i2);
            return Value(r);
          }
          case OP_RUIAFILL16:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE, i1));
            if(r.is_null()) return Value();
            fill_n(r->raw().is16, i1, i2);
            return Value(r);
          }
          case OP_RUIAFILL32:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE, i1));
            if(r.is_null()) return Value();
            fill_n(r->raw().is32, i1, i2);
            return Value(r);
          }
          case OP_RUIAFILL64:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE, i1));
            if(r.is_null()) return Value();
            fill_n(r->raw().is64, i1, i2);
            return Value(r);
          }
          case OP_RUSFAFILL:
          {
            int64_t i;
            double f;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE, i));
            if(r.is_null()) return Value();
            fill_n(r->raw().sfs, i, f);
            return Value(r);
          }
          case OP_RUDFAFILL:
          {
            int64_t i;
            double f;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE, i));
            if(r.is_null()) return Value();
            fill_n(r->raw().dfs, i, f);
            return Value(r);
          }
          case OP_RURAFILL:
          {
            int64_t i;
            Reference r;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_shared_for_object(context, *r)) return Value(); 
            Reference r2(new_object(context, OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE, i));
            if(r2.is_null()) return Value();
            fill_n(r->raw().rs, i, r);
            return Value(r2);
          }
          case OP_RUTFILLI:
          {
            int64_t i1, i2;
            if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, i1));
            if(r.is_null()) return Value();
            fill_n(r->raw().tes, i1, TupleElement(i2));
            fill_n(r->raw().tuple_elem_types(), i1, VALUE_TYPE_INT);
            return Value(r);
          }
          case OP_RUTFILLF:
          {
            int64_t i;
            double f;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r(new_object(context, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, i));
            if(r.is_null()) return Value();
            fill_n(r->raw().tes, i, TupleElement(f));
            fill_n(r->raw().tuple_elem_types(), i, VALUE_TYPE_FLOAT);
            return Value(r);
          }
          case OP_RUTFILLR:
          {
            int64_t i;
            Reference r;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, i));
            if(r2.is_null()) return Value();
            if(static_cast<size_t>(i) > 1U && !check_shared_for_object(context, *r)) {
              context.set_error(ERROR_AGAIN_USED_UNIQUE);
              return Value();
            }
            fill_n(r2->raw().tes, i, TupleElement(r));
            fill_n(r2->raw().tuple_elem_types(), i, VALUE_TYPE_REF);
            return Value(r2);
          }
          case OP_RUIANTH8:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().is8[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIANTH16:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().is16[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIANTH32:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().is32[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIANTH64:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().is64[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUSFANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().sfs[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUDFANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().dfs[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RURANTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            if(!check_shared_for_object(context, *(r->raw().rs[i]))) return Value();
            Reference r2(new_unique_pair(context, Value(r->raw().rs[i]), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUTNTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Value value(r->raw().tuple_elem_types()[i], r->raw().tes[i]);
            Reference r2(new_unique_pair(context, value, Value(r)));
            if(r2.is_null()) return Value();
            if(value.is_unique())
              r->raw().tuple_elem_types()[i].raw() = VALUE_TYPE_CANCELED_REF;
            else if(value.is_lazy())
              r->raw().tuple_elem_types()[i].raw() |= VALUE_TYPE_LAZILY_CANCELED;
            return Value(r2);
          }
          case OP_RUIASNTH8:
          {
            Reference r;
            int64_t i1, i2;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i1, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_int(context, i2, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i1)) return Value();
            r->raw().is8[i1] = i2;
            return Value(r);
          }
          case OP_RUIASNTH16:
          {
            Reference r;
            int64_t i1, i2;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i1, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_int(context, i2, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i1)) return Value();
            r->raw().is16[i1] = i2;
            return Value(r);
          }
          case OP_RUIASNTH32:
          {
            Reference r;
            int64_t i1, i2;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i1, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_int(context, i2, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i1)) return Value();
            r->raw().is32[i1] = i2;
            return Value(r);
          }
          case OP_RUIASNTH64:
          {
            Reference r;
            int64_t i1, i2;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i1, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_int(context, i2, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i1)) return Value();
            r->raw().is64[i1] = i2;
            return Value(r);
          }
          case OP_RUSFASNTH:
          {
            Reference r;
            int64_t i;
            double f;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_float(context, f, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            r->raw().sfs[i] = f;
            return Value(r);
          }
          case OP_RUDFASNTH:
          {
            Reference r;
            int64_t i;
            double f;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_float(context, f, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            r->raw().dfs[i] = f;
            return Value(r);
          }
          case OP_RURASNTH:
          {
            Reference r1, r2;
            int64_t i;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!get_ref(context, r2, context.pushed_arg(0))) return Value();
            if(!check_object_type(context, *r1, OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r1, i)) return Value();
            r1->raw().rs[i].safely_assign_for_gc(r2);
            return Value(r1);
          }
          case OP_RUTSNTH:
          {
            Reference r;
            int64_t i;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            if(!check_pushed_arg_count(context, 1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE)) return Value();
            if(!check_object_elem_index(context, *r, i)) return Value();
            Value value = context.pushed_arg(0);
            cancel_ref_for_unique(context.pushed_arg(0));
            r->raw().tuple_elem_types()[i].safely_assign_for_gc(VALUE_TYPE_ERROR);
            r->raw().tes[i].safely_assign_for_gc(value.tuple_elem());
            r->raw().tuple_elem_types()[i].safely_assign_for_gc(value.tuple_elem_type());
            return Value(r);
          }
          case OP_RUIALEN8:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIALEN16:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIALEN32:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIALEN64:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUSFALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUDFALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RURALEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUTLEN:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_unique_pair(context, Value(static_cast<int64_t>(r->length())), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUTYPE:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            Reference r2(new_unique_pair(context, Value(r->type()), Value(r)));
            if(r2.is_null()) return Value();
            return Value(r2);
          }
          case OP_RUIATOIA8:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_IARRAY8, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().is8, r->length(), r2->raw().is8);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUIATOIA16:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_IARRAY16, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().is16, r->length(), r2->raw().is16);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUIATOIA32:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_IARRAY32, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().is32, r->length(), r2->raw().is32);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUIATOIA64:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_IARRAY64, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().is64, r->length(), r2->raw().is64);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUSFATOSFA:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_SFARRAY, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().sfs, r->length(), r2->raw().sfs);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUDFATODFA:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_DFARRAY, context.regs().ac2));
            if(r2.is_null()) return Value();
            copy_n(r->raw().dfs, r->length(), r2->raw().dfs);
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RURATORA:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_RARRAY, context.regs().ac2));
            if(r2.is_null()) return Value();
            for(size_t i = 0; i < r->length(); i++) {
              if(!check_shared_for_object(context, *(r2->raw().rs[i]))) return Value();
              r2->raw().rs[i] = r->raw().rs[i];
            }
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_RUTTOT:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!check_object_type(context, *r, OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE)) return Value();
            Reference r2(new_object(context, OBJECT_TYPE_TUPLE, context.regs().ac2));
            if(r2.is_null()) return Value();
            for(size_t i = 0; i < r->length(); i++) {
              Value value(r->raw().tuple_elem_types()[i], r->raw().tes[i]);
              if(!check_shared_for_value(context, value)) return Value();
              if(!set_lazy_values_as_shared(context, value)) return Value();
              r2->raw().tes[i] = r->raw().tes[i];
              r2->raw().tuple_elem_types()[i] = r->raw().tuple_elem_types()[i];
            }
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv.safely_assign_for_gc(Value(r3));
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            return Value(r3);
          }
          case OP_FPOW:
          {
            double f1, f2;
            if(!get_float(context, f1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_float(context, f2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(pow(f1, f2));
          }
          case OP_FSQRT:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(sqrt(f));
          }
          case OP_FEXP:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(exp(f));
          }
          case OP_FLOG:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(log(f));
          }
          case OP_FCOS:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(cos(f));
          }
          case OP_FSIN:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(sin(f));
          }
          case OP_FTAN:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(tan(f));
          }
          case OP_FACOS:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(acos(f));
          }
          case OP_FASIN:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(asin(f));
          }
          case OP_FATAN:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(atan(f));
          }
          case OP_FCEIL:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(ceil(f));
          }
          case OP_FFLOOR:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(floor(f));
          }
          case OP_FROUND:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(round(f));
          }
          case OP_FTRUNC:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(trunc(f));
          }
          case OP_TRY:
          {
            if(!context.regs().after_leaving_flags[0]) {
              int64_t i1, i2;
              if(!get_int(context, i1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              if(!get_int(context, i2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
              if(!check_pushed_arg_count(context, 3)) return Value();
              Value arg1 = context.pushed_arg(0);
              Value arg2 = context.pushed_arg(1);
              Value io = context.pushed_arg(2);
              cancel_ref_for_unique(context.pushed_arg(0));
              cancel_ref_for_unique(context.pushed_arg(1));
              cancel_ref_for_unique(context.pushed_arg(2));
              if(!check_value_type(context, io, VALUE_TYPE_REF)) return Value();
              if(!check_object_type(context, *(io.raw().r), OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE)) return Value(); 
              if(!context.regs().arg_instr_flag) context.hide_args();
              if(!push_tmp_ac2(context)) return Value();
              if(!push_try_regs(context)) return Value();
              if(!push_arg(context, arg1)) return Value();
              if(!push_arg(context, io)) return Value();
              context.set_try_regs(arg2, io.raw().r);
              if(!call_fun_for_force(context, static_cast<uint32_t>(i1))) return Value();
            }
            bool must_repeat = false;
            do {
              int error = context.regs().rv.error();
              Value arg2 = context.regs().try_arg2;
              Reference io_r = context.regs().try_io_r;
              if(!force_rv(context, true)) return Value();
              context.regs().after_leaving_flags[0] = false;
              if(!pop_try_regs(context)) return Value();
              if(!pop_tmp_ac2(context)) return Value();
              if(!context.regs().arg_instr_flag) context.restore_abp2_and_ac2();
              Reference r = context.regs().rv.raw().r;
              if(error == ERROR_SUCCESS) {
                // Checks and returns the reference for the arg1 function or the arg2 function. 
                if(r->type() != (OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) ||
                    r->length() != 2 ||
                    r->elem(1).type() != VALUE_TYPE_REF ||
                    r->elem(1).raw().r->type() != (OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE)) {
                  context.set_error(ERROR_INCORRECT_OBJECT);
                  return Value();
                }
                return Value(r);
              } else {
                context.regs().rv.raw().error = ERROR_SUCCESS;
                int64_t i;
                if(!get_int(context, i, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
                if(!context.regs().arg_instr_flag) context.hide_args();
                if(!push_tmp_ac2(context)) return Value();
                if(!push_try_regs(context)) return Value();
                if(!push_arg(context, Value(error))) return Value();
                if(!push_arg(context, Value(r))) return Value();
                if(!push_arg(context, arg2)) return Value();
                if(!push_arg(context, io_r)) return Value();
                if(!call_fun_for_force(context, static_cast<uint32_t>(i))) return Value();
                must_repeat = true;
              }
            } while(must_repeat);
          }
          case OP_IFORCE:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(i);
          }
          case OP_FFORCE:
          {
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(f);
          }
          case OP_RFORCE:
          {
            Reference r;
            if(!get_ref(context, r, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(r);
          }
          default:
          {
            context.set_error(ERROR_INCORRECT_INSTR);
            return Value();
          }
        }
      }
      
      bool InterpreterVirtualMachine::enter_to_fun(ThreadContext &context, size_t i, bool &is_fun_result)
      {
        is_fun_result = false;
        return context.enter_to_fun(i);
      }

      bool InterpreterVirtualMachine::leave_from_fun(ThreadContext &context)
      { return context.leave_from_fun(); }

      bool InterpreterVirtualMachine::call_fun(ThreadContext &context, size_t i, int value_type)
      {
        if(!check_fun(context, i)) return false;
        bool is_fun_result;
        if(!_M_eval_strategy->pre_enter_to_fun(this, &context, i, value_type, is_fun_result))
          return is_fun_result;
        if(!enter_to_fun(context, i, is_fun_result)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          is_fun_result = false;
          return false;
        }
        return is_fun_result;
      }

      bool InterpreterVirtualMachine::call_fun_for_force(ThreadContext &context, size_t i)
      {
        if(!check_fun(context, i)) return false;
        bool is_fun_result;
        if(!enter_to_fun(context, i, is_fun_result)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          is_fun_result = false;
          return false;
        }
        return is_fun_result;        
      }

      bool InterpreterVirtualMachine::call_fun_for_force_with_eval_strategy(ThreadContext &context, std::size_t i, int value_type)
      {
        if(!check_fun(context, i)) return false;
        bool is_fun_result;
        if(!_M_eval_strategy->pre_enter_to_fun_for_force(this, &context, i, value_type, is_fun_result))
          return is_fun_result;
        if(!enter_to_fun(context, i, is_fun_result)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          is_fun_result = false;
          return false;
        }
        return is_fun_result;
      }

      bool InterpreterVirtualMachine::force_value(ThreadContext &context, Value &value, bool is_try)
      {
        while(value.is_lazy()) {
          Value tmp_value;
          bool tmp_must_be_shared;
          Object &object = *(value.raw().r);
          unique_lock<LazyValueMutex> lock;
          if(!context.regs().after_leaving_flags[1])
            lock = unique_lock<LazyValueMutex>(object.raw().lzv.mutex);
          else
            lock = unique_lock<LazyValueMutex>(object.raw().lzv.mutex, adopt_lock);
          if(object.raw().lzv.value.is_error()) {
            if(!context.regs().after_leaving_flags[1]) {
              if(!context.regs().arg_instr_flag) context.hide_args();
              if(!push_tmp_ac2_and_after_leaving_flag1(context)) return false;
              if(!push_ai(context)) return false;
              if(!push_locked_lazy_value_ref(context, value.raw().r)) return false;
              Value *args = object.raw().lzv.args;
              uint32_t tmp_abp2 = context.regs().abp2;
              for(size_t i = 0; i < object.length(); i++)
                if(!push_arg(context, args[i])) return false;
              context.regs().after_leaving_flag_index = 1;
              if(is_try) context.set_try_regs_for_force();
              if(!call_fun_for_force_with_eval_strategy(context, object.raw().lzv.fun, object.raw().lzv.value_type)) {
                if(context.regs().rv.raw().error == ERROR_SUCCESS || tmp_abp2 > context.regs().abp2) lock.release();
                return false;
              }
              if(!_M_eval_strategy->post_leave_from_fun_for_force(this, &context, object.raw().lzv.fun, object.raw().lzv.value_type)) {
                if(tmp_abp2 > context.regs().abp2) lock.release();
                return false;
              }
              if(!restore_and_pop_regs_for_force(context)) {
                lock.release();
                return false;
              }
            }
            context.regs().after_leaving_flags[1] = false;
            if(!context.regs().rv.raw().r->is_lazy()) {
              switch(object.raw().lzv.value_type) {
                case VALUE_TYPE_INT:
                  object.raw().lzv.value = Value(context.regs().rv.raw().i);
                  break;
                case VALUE_TYPE_FLOAT:
                  object.raw().lzv.value = Value(context.regs().rv.raw().f);
                  break;
                case VALUE_TYPE_REF:
                  object.raw().lzv.value = Value(context.regs().rv.raw().r);
                  break;
                case VALUE_TYPE_CANCELED_REF:
                  lock.release();
                  context.set_error(ERROR_AGAIN_USED_UNIQUE);
                  return false;
                default:
                  lock.release();
                  context.set_error(ERROR_INCORRECT_VALUE);
                  return false;
              }
            } else {
              if(object.raw().lzv.value_type == context.regs().rv.raw().r->raw().lzv.value_type) {
                object.raw().lzv.value = Value::lazy_value_ref(context.regs().rv.raw().r, context.regs().rv.raw().i != 0);
              } else {
                switch(object.raw().lzv.value_type) {
                  case VALUE_TYPE_INT:
                    object.raw().lzv.value = Value(0);
                    break;
                  case VALUE_TYPE_FLOAT:
                    object.raw().lzv.value = Value(0.0);
                    break;
                  case VALUE_TYPE_REF:
                    object.raw().lzv.value = Value(Reference());
                    break;
                  case VALUE_TYPE_CANCELED_REF:
                    lock.release();
                    context.set_error(ERROR_AGAIN_USED_UNIQUE);
                    return false;
                  default:
                    lock.release();
                    context.set_error(ERROR_INCORRECT_VALUE);
                    return false;
                }
              }
            }
          }
          tmp_value = object.raw().lzv.value;
          tmp_must_be_shared = object.raw().lzv.must_be_shared;
          if(object.raw().lzv.value_type == VALUE_TYPE_REF && object.raw().lzv.value.is_unique())
            object.raw().lzv.value.cancel_ref();
          lock.unlock();
          if(object.raw().lzv.value_type == VALUE_TYPE_REF && tmp_value.type() == VALUE_TYPE_CANCELED_REF) {
            lock.release();
            context.set_error(ERROR_AGAIN_USED_UNIQUE);
            return false;
          }
          if(tmp_value.is_unique()) {
            if(value.is_lazily_canceled()) {
              lock.release();
              context.set_error(ERROR_AGAIN_USED_UNIQUE);
              return false;
            }
            if(tmp_must_be_shared) {
              lock.release();
              context.set_error(ERROR_UNIQUE_OBJECT);
              return false;
            }
          }
          context.regs().tmp_r.safely_assign_for_gc(value.r());
          value.safely_assign_for_gc(tmp_value);
          context.regs().tmp_r.safely_assign_for_gc(Reference());
        }
        return true;
      }

      bool InterpreterVirtualMachine::force_rv(ThreadContext &context, bool is_try)
      {
        if(!context.regs().after_leaving_flags[1])
          context.regs().force_tmp_rv.safely_assign_for_gc(context.regs().rv);
        if(context.regs().force_tmp_rv.raw().r->is_lazy()) {
          Value tmp_value = Value::lazy_value_ref(context.regs().force_tmp_rv.raw().r);
          if(!force_value(context, tmp_value, is_try)) return false;
          context.regs().rv = tmp_value;
          context.regs().force_tmp_rv = ReturnValue();
        }
        return true;
      }

      bool InterpreterVirtualMachine::force_int(ThreadContext &context, int64_t &i, Value &value)
      {
        if(!force_value(context, value)) return false;
        i = value.i();
        return true;
      }

      bool InterpreterVirtualMachine::force_float(ThreadContext &context, double &f, Value &value)
      {
        if(!force_value(context, value)) return false;
        f = value.f();
        return true;
      }

      bool InterpreterVirtualMachine::force_ref(ThreadContext &context, Reference &r, Value &value)
      {
        if(!force_value(context, value)) return false;
        r = value.r();
        return true;
      }

      bool InterpreterVirtualMachine::force_pushed_args_for_eager_eval(ThreadContext &context)
      { return true; }
      
      bool InterpreterVirtualMachine::force_pushed_args_for_lazy_eval(ThreadContext &context)
      {
        if(!context.regs().after_leaving_flags[1]) context.regs().ai = 0;
        for(uint64_t &i = context.regs().ai; i < context.regs().ac2; i++) {
          if(!force_value(context, context.pushed_arg(static_cast<size_t>(i)))) return false;
        }
        return true;
      }

      bool InterpreterVirtualMachine::force_value_and_interpret(ThreadContext &context, Value &value)
      {
        bool saved_after_leaving_flag1 = context.regs().after_leaving_flags[0];
        bool saved_after_leaving_flag2 = context.regs().after_leaving_flags[1];
        unsigned saved_after_leaving_flag_index = context.regs().after_leaving_flag_index;
        context.regs().after_leaving_flags[0] = false;
        context.regs().after_leaving_flags[1] = false;
        context.regs().after_leaving_flag_index = 0;
        if(!force_value(context, value)) {
          if(context.regs().rv.raw().error == ERROR_SUCCESS) {
            bool tmp_result;
            do {
              interpret(context);
              tmp_result = force_value(context, value);
            } while(!tmp_result && context.regs().rv.raw().error == ERROR_SUCCESS);
          }
        }
        context.regs().after_leaving_flags[0] = saved_after_leaving_flag1;
        context.regs().after_leaving_flags[1] = saved_after_leaving_flag2;
        context.regs().after_leaving_flag_index = saved_after_leaving_flag_index;
        if(context.regs().rv.raw().error != ERROR_SUCCESS) return false;
        return true;
      }

      bool InterpreterVirtualMachine::fully_force_value(ThreadContext &context, Value &value)
      { 
        bool result = fully_force_value(context, value, [&context](Reference r) {
          context.regs().force_tmp_r.safely_assign_for_gc(r);
        });
        context.regs().force_tmp_r.safely_assign_for_gc(Reference());
        return result;
      }

      bool InterpreterVirtualMachine::fully_force_value(ThreadContext &context, Value &value, function<void (Reference)> fun)
      {
        if(!force_value_and_interpret(context, value)) return false;
        if(value.type() == VALUE_TYPE_REF) {
          switch(value.raw().r->type()) {
            case OBJECT_TYPE_TUPLE:
            {
              Reference r(new_object(context, OBJECT_TYPE_TUPLE, value.raw().r->length()));
              if(r.is_null()) return false;
              for(size_t i = 0; i < r->length(); i++) r->set_elem(i, Value());
              fun(r);
              context.safely_set_gc_tmp_ptr_for_gc(nullptr);
              for(size_t i = 0; i < r->length(); i++) {
                Value tmp_elem_value = value.raw().r->elem(i);
                if(!fully_force_value(context, tmp_elem_value, [r, i](Reference elem_r) {
                  r->set_elem(i, Value(elem_r));
                })) return false;
                r->set_elem(i, tmp_elem_value);
              }
              value.safely_assign_for_gc(Value(r));
              break;
            }
            case OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE:
            {
              for(size_t i = 0; i < value.raw().r->length(); i++) {
                Value tmp_elem_value = value.raw().r->elem(i);
                if(!fully_force_value(context, tmp_elem_value, [&context, i](Reference elem_r) {
                  context.regs().force_tmp_r2.safely_assign_for_gc(elem_r);
                })) return false;
                value.raw().r->set_elem(i, tmp_elem_value);
                context.regs().force_tmp_r2.safely_assign_for_gc(Reference()); 
              }
              break;
            }
          }
        }
        return true;
      }

      bool InterpreterVirtualMachine::fully_force_rv(ThreadContext &context)
      {
        context.regs().force_tmp_rv2.safely_assign_for_gc(context.regs().rv);
        if(context.regs().force_tmp_rv2.raw().r->is_lazy()) {
          Value tmp_value = Value::lazy_value_ref(context.regs().force_tmp_rv2.raw().r, context.regs().force_tmp_rv2.raw().i != 0);
          if(!fully_force_value(context, tmp_value)) return false;
          context.regs().rv = tmp_value;
          context.regs().force_tmp_rv2 = ReturnValue();
        } else {
          Value tmp_value = Value(context.regs().force_tmp_rv2.raw().r);
          if(!fully_force_value(context, tmp_value)) return false;
          if(!check_value_type(context, tmp_value, VALUE_TYPE_REF)) return false;
          context.regs().rv.raw().r = tmp_value.raw().r;
          context.regs().force_tmp_rv2 = ReturnValue();
        }
        return true;
      }
    }
  }
}
