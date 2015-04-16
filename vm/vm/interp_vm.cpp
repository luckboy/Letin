/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <atomic>
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

      static inline bool get_int(ThreadContext &context, int64_t &i, const Value &value)
      {
        if(value.type() != VALUE_TYPE_INT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        i = value.raw().i;
        return true;
      }

      static inline bool get_int(ThreadContext &context, int64_t &i, uint32_t arg_type, Argument arg)
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
            return get_int(context, i, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      static inline bool get_float(ThreadContext &context, double &f, const Value &value)
      {
        if(value.type() != VALUE_TYPE_FLOAT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        f = value.raw().f;
        return true;
      }

      static inline bool get_float(ThreadContext &context, double &f, uint32_t arg_type, Argument arg)
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
            return get_float(context, f, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      static inline bool get_ref(ThreadContext &context, Reference &r, Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          if(value.type() == VALUE_TYPE_CANCELED_REF)
            context.set_error(ERROR_AGAIN_USED_UNIQUE);
          else
            context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(value.raw().r->is_unique()) value.cancel_ref();
        r = value.raw().r;
        return true;
      }

      static inline bool get_ref_for_const_value(ThreadContext &context, Reference &r, const Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          if(value.type() == VALUE_TYPE_CANCELED_REF)
            context.set_error(ERROR_AGAIN_USED_UNIQUE);
          else
            context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        if(value.raw().r->is_unique()) context.set_error(ERROR_UNIQUE_OBJECT);
        r = value.raw().r;
        return true;
      }

      static inline bool get_ref(ThreadContext &context, Reference &r, uint32_t arg_type, Argument arg)
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

      static inline Object *new_object(ThreadContext &context, int type, size_t length)
      {
        Object *object = context.gc()->new_object(type, length, &context);
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

      static inline bool check_object_elem_index(ThreadContext &context, const Object &object, size_t i)
      {
        if(i >= object.length()) {
          context.set_error(ERROR_INDEX_OF_OUT_BOUNDS);
          return false;
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

      //
      // An InterpreterVirtualMachine class.
      //

      InterpreterVirtualMachine::~InterpreterVirtualMachine() {}

      ReturnValue InterpreterVirtualMachine::start_in_thread(size_t i, const vector<Value> &args, ThreadContext &context)
      {
        for(auto arg : args) if(!push_arg(context, arg)) return context.regs().rv;
        if(call_fun(context, i)) interpret(context);
        return context.regs().rv;
      }

      void InterpreterVirtualMachine::interpret(ThreadContext &context)
      { while(interpret_instr(context)); }

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
        switch(opcode_to_instr(instr.opcode)) {
          case INSTR_LET:
          {
            Value value = interpret_op(context, instr);
            context.pop_args();
            if(!value.is_error())
              if(!context.push_local_var(value)) context.set_error(ERROR_STACK_OVERFLOW);
            context.regs().tmp_ptr = nullptr;
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
            context.regs().tmp_ptr = nullptr;
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
            Value value = interpret_op(context, instr, true);
            if(!value.is_error()) {
              context.restore_abp2_and_ac2();
              if(!context.push_arg(value)) context.set_error(ERROR_STACK_OVERFLOW);
            }
            context.regs().tmp_ptr = nullptr;
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
            Value value = interpret_op(context, instr);
            context.pop_args();
            if(!value.is_error()) {
              Reference r;
              if(get_ref(context, r, value)) {
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
            context.regs().tmp_ptr = nullptr;
            break;
          }
        }
        atomic_thread_fence(memory_order_release);
        return true;
      }

      Value InterpreterVirtualMachine::interpret_op(ThreadContext &context, const Instruction &instr, bool is_arg_instr)
      {
        switch(opcode_to_op(instr.opcode)) {
          case OP_ILOAD:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(i);
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
            double f;
            if(!get_float(context, f, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(f);
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
            Reference r1;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            return Value(r1);
          }
          case OP_REQ:
          {
            Reference r1, r2;
            if(opcode_to_arg_type1(instr.opcode) != ARG_TYPE_GVAR && opcode_to_arg_type2(instr.opcode) != ARG_TYPE_GVAR) return Value();
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 == r2 ? 1 : 0);
          }
          case OP_RNE:
          {
            Reference r1, r2;
            if(opcode_to_arg_type1(instr.opcode) != ARG_TYPE_GVAR && opcode_to_arg_type2(instr.opcode) != ARG_TYPE_GVAR) return Value();
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 != r2 ? 1 : 0);
          }
          case OP_RIARRAY8:
          {
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
            if(!context.regs().after_leaving_flag) {
              int64_t i;
              if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
              return Value(context.regs().rv.raw().i);
            }
          }
          case OP_FCALL:
          {
            if(!context.regs().after_leaving_flag) {
              int64_t i;
              if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
              return Value(context.regs().rv.raw().f);
            }
          }
          case OP_RCALL:
          {
            if(!context.regs().after_leaving_flag) {
              int64_t i;
              if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
              return Value(context.regs().rv.raw().r);
            }
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
            if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.native_fun_handler()->invoke(this, &context, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
            if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
            return Value(rv.raw().i);
          }
          case OP_FNCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.native_fun_handler()->invoke(this, &context, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
            if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
            return Value(rv.raw().f);
          }
          case OP_RNCALL:
          {
            int64_t i;
            if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(is_arg_instr) if(!push_tmp_ac2(context)) return Value();
            ArgumentList args = context.pushed_args();
            ReturnValue rv = context.native_fun_handler()->invoke(this, &context, i, args);
            if(rv.raw().error != ERROR_SUCCESS) {
              context.set_error(rv.raw().error);
              return Value();
            }
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
            if(is_arg_instr) if(!pop_tmp_ac2(context)) return Value();
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
            if(value.is_unique()) r->raw().tuple_elem_types()[i] = VALUE_TYPE_CANCELED_REF;
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
            r1->raw().rs[i] = r2;
            atomic_thread_fence(memory_order_release);
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
            if(context.pushed_arg(0).is_unique()) context.pushed_arg(0).cancel_ref();
            r->raw().tuple_elem_types()[i] = VALUE_TYPE_ERROR;
            atomic_thread_fence(memory_order_release);
            r->raw().tes[i] = value.tuple_elem();
            atomic_thread_fence(memory_order_release);
            r->raw().tuple_elem_types()[i] = value.tuple_elem_type();
            atomic_thread_fence(memory_order_release);
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
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
              r2->raw().tes[i] = r->raw().tes[i];
              r2->raw().tuple_elem_types()[i] = r->raw().tuple_elem_types()[i];
            }
            context.regs().tmp_r = r2;
            Reference r3(new_unique_pair(context, Value(r2), Value(r)));
            if(r3.is_null()) return Value();
            context.regs().rv = Value(r3);
            atomic_thread_fence(memory_order_release);
            context.regs().tmp_r = Reference();
            return Value(r3);
          }
          default:
          {
            context.set_error(ERROR_INCORRECT_INSTR);
            return Value();
          }
        }
      }

      bool InterpreterVirtualMachine::enter_to_fun(ThreadContext &context, size_t i)
      { return context.enter_to_fun(i); }

      bool InterpreterVirtualMachine::leave_from_fun(ThreadContext &context)
      { return context.leave_from_fun(); }

      bool InterpreterVirtualMachine::call_fun(ThreadContext &context, size_t i)
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
        if(!enter_to_fun(context, i)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }
    }
  }
}
