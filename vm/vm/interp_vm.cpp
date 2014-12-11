/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
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

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      //
      // Static inline functions.
      //

      inline static bool get_int(ThreadContext &context, int64_t &i, const Value &value)
      {
        if(value.type() != VALUE_TYPE_INT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        i = value.raw().i;
        return true;
      }

      inline static bool get_int(ThreadContext &context, int64_t &i, uint32_t arg_type, Argument arg)
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

      inline static bool get_float(ThreadContext &context, double &f, const Value &value)
      {
        if(value.type() == VALUE_TYPE_FLOAT) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        f = value.raw().f;
        return true;
      }

      inline static bool get_float(ThreadContext &context, double &f, uint32_t arg_type, Argument arg)
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

      inline static bool get_ref(ThreadContext &context, Reference &r, const Value &value)
      {
        if(value.type() != VALUE_TYPE_REF) {
          context.set_error(ERROR_INCORRECT_VALUE);
          return false;
        }
        r = value.raw().r;
        return true;
      }

      inline static bool get_ref(ThreadContext &context, Reference &r, uint32_t arg_type, Argument arg)
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
            return get_ref(context, r, context.global_var(arg.gvar));
          default:
            context.set_error(ERROR_INCORRECT_INSTR);
            return false;
        }
      }

      inline static Object *new_object(ThreadContext &context, int type, size_t length)
      {
        Object *object = context.gc()->new_object(type, length, &context);
        if(object == nullptr) {
          context.set_error(ERROR_OUT_OF_MEMORY);
          return nullptr;
        }
        return object;
      }

      inline static bool check_value_type(ThreadContext &context, const Value &value, int type)
      {
        if(value.type() != type) {
          context.set_error(ERROR_INCORRECT_OBJECT);
          return false;
        }
        return true;
      }

      inline static bool check_object_type(ThreadContext &context, const Object &object, int type)
      {
        if(object.type() != type) {
          context.set_error(ERROR_INCORRECT_OBJECT);
          return false;
        }
        return true;
      }

      inline static bool check_object_elem_index(ThreadContext &context, const Object &object, size_t i)
      {
        if(i >= object.length()) {
          context.set_error(ERROR_INDEX_OF_OUT_BOUNDS);
          return false;
        }
        return true;
      }

      inline static bool add_object_lengths(ThreadContext &context, size_t &length, size_t length1, size_t length2)
      {
        length = length1 + length2;
        if((length1 | length2) >= length) {
          context.set_error(ERROR_OUT_OF_MEMORY);
          return false;
        }
        return true;
      }

      inline static bool push_arg(ThreadContext &context, const Value value)
      {
        if(!context.push_arg(value)) {
          context.set_error(ERROR_STACK_OVERFLOW);
          return false;
        }
        return true;
      }

      //
      // An InterpretingVirtualMachine class.
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
        const Function &fun = context.fun(context.regs().fp);
        if(context.regs().ip >= fun.raw().instr_count) context.set_error(ERROR_NO_INSTR);
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
            Value value = interpret_op(context, instr);
            if(!value.is_error())
              if(!context.push_arg(value)) context.set_error(ERROR_STACK_OVERFLOW);
            context.regs().tmp_ptr = nullptr;
            break;
          }
          case INSTR_RETRY:
            if(context.regs().ac == context.regs().ac2) {
              for(size_t i = 0; i < context.regs().ac; i++) context.arg(i) = context.pushed_arg(i);
              context.pop_args_and_local_vars();
            } else
              context.set_error(ERROR_INCORRECT_ARG_COUNT);
            break;
        }
        return true;
      }

      Value InterpreterVirtualMachine::interpret_op(ThreadContext &context, const Instruction &instr)
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
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 == r2 ? 1 : 0);
          }
          case OP_RNE:
          {
            Reference r1, r2;
            if(!get_ref(context, r1, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
            if(!get_ref(context, r2, opcode_to_arg_type2(instr.opcode), instr.arg2)) return Value();
            return Value(r1 != r2 ? 1 : 0);
          }
          case OP_RIARRAY8:
          {
            Object *object = new_object(context, OBJECT_TYPE_IARRAY8, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_INT)) return Value();
              object->raw().is8[i] = context.arg(i).raw().i;
            }
            return Value(Reference(object));
          }
          case OP_RIARRAY16:
          {
            Object *object = new_object(context, OBJECT_TYPE_IARRAY16, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_INT)) return Value();
              object->raw().is16[i] = context.arg(i).raw().i;
            }
            return Value(Reference(object));
          }
          case OP_RIARRAY32:
          {
            Object *object = new_object(context, OBJECT_TYPE_IARRAY32, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_INT)) return Value();
              object->raw().is32[i] = context.arg(i).raw().i;
            }
            return Value(Reference(object));
          }
          case OP_RIARRAY64:
          {
            Object *object = new_object(context, OBJECT_TYPE_IARRAY64, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_INT)) return Value();
              object->raw().is64[i] = context.arg(i).raw().i;
            }
            return Value(Reference(object));
          }
          case OP_RSFARRAY:
          {
            Object *object = new_object(context, OBJECT_TYPE_IARRAY64, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_FLOAT)) return Value();
              object->raw().sfs[i] = context.arg(i).raw().f;
            }
            return Value(Reference(object));
          }
          case OP_RDFARRAY:
          {
            Object *object = new_object(context, OBJECT_TYPE_SFARRAY, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_FLOAT)) return Value();
              object->raw().dfs[i] = context.arg(i).raw().f;
            }
            return Value(Reference(object));
          }
          case OP_RRARRAY:
          {
            Object *object = new_object(context, OBJECT_TYPE_DFARRAY, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) {
              if(!check_value_type(context, context.arg(i), VALUE_TYPE_REF)) return Value();
              object->raw().rs[i] = context.arg(i).raw().r;
            }
            return Value(Reference(object));
          }
          case OP_RTUPLE:
          {
            Object *object = new_object(context, OBJECT_TYPE_TUPLE, context.regs().ac2);
            if(object == nullptr) return Value();
            for(size_t i = 0; context.regs().ac2; i++) object->raw().tes[i] = context.arg(i);
            return Value(Reference(object));
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
            return Value(r->raw().is8[i]);
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
            return r->raw().tes[i];
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
            Object *object = new_object(context, OBJECT_TYPE_IARRAY8, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().is8, r1->length(), object->raw().is8);
            copy_n(r2->raw().is8, r2->length(), object->raw().is8 + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_IARRAY16, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().is16, r1->length(), object->raw().is16);
            copy_n(r2->raw().is16, r2->length(), object->raw().is16 + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_IARRAY32, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().is32, r1->length(), object->raw().is32);
            copy_n(r2->raw().is32, r2->length(), object->raw().is32 + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_IARRAY64, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().is64, r1->length(), object->raw().is64);
            copy_n(r2->raw().is64, r2->length(), object->raw().is64 + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_SFARRAY, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().sfs, r1->length(), object->raw().sfs);
            copy_n(r2->raw().sfs, r2->length(), object->raw().sfs + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_DFARRAY, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().dfs, r1->length(), object->raw().dfs);
            copy_n(r2->raw().dfs, r2->length(), object->raw().dfs + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_RARRAY, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().rs, r1->length(), object->raw().rs);
            copy_n(r2->raw().rs, r2->length(), object->raw().rs + r1->length());
            return Value(Reference(object));
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
            Object *object = new_object(context, OBJECT_TYPE_TUPLE, object_length);
            if(object == nullptr) return Value();
            copy_n(r1->raw().tes, r1->length(), object->raw().tes);
            copy_n(r2->raw().tes, r2->length(), object->raw().tes + r1->length());
            return Value(Reference(object));
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
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              return Value(context.regs().rv.raw().i);
            }
          }
          case OP_FCALL:
          {
            if(!context.regs().after_leaving_flag) {
              int64_t i;
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              return Value(context.regs().rv.raw().f);
            }
          }
          case OP_RCALL:
          {
            if(!context.regs().after_leaving_flag) {
              int64_t i;
              if(!get_int(context, i, opcode_to_arg_type1(instr.opcode), instr.arg1)) return Value();
              call_fun(context, i);
              return Value();
            } else {
              context.regs().after_leaving_flag = false;
              return Value(context.regs().rv.raw().r);
            }
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
        if(fun.arg_count() != context.regs().ac) {
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
