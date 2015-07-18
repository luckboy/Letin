/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _VM_INTERP_VM_HPP
#define _VM_INTERP_VM_HPP

#include <letin/const.hpp>
#include <letin/opcode.hpp>
#include <letin/vm.hpp>
#include "impl_vm_base.hpp"
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class InterpreterVirtualMachine : public ImplVirtualMachineBase
      {
        Value (*_M_ret_value_to_int_value)(const ReturnValue &);
        Value (*_M_ret_value_to_float_value)(const ReturnValue &);
        Value (*_M_ret_value_to_ref_value)(const ReturnValue &);
      public:
        InterpreterVirtualMachine(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy);

        ~InterpreterVirtualMachine();
      protected:
        ReturnValue start_in_thread(std::size_t i, const std::vector<Value> &args, ThreadContext &context);

        virtual void interpret(ThreadContext &context);
      private:
        bool get_int(ThreadContext &context, std::int64_t &i, Value &value);

        bool get_int(ThreadContext &context, std::int64_t &i, std::uint32_t arg_type, Argument arg);

        bool get_float(ThreadContext &context, double &f, Value &value);

        bool get_float(ThreadContext &context, double &f, std::uint32_t arg_type, Argument arg);

        bool get_ref(ThreadContext &context, Reference &r, Value &value);

        bool get_ref(ThreadContext &context, Reference &r, std::uint32_t arg_type, Argument arg);
      protected:
        bool interpret_instr(ThreadContext &context);
      private:
        Value interpret_op(ThreadContext &context, const Instruction &instr);

        Value interpret_icall_for_eager_eval(ThreadContext &context, const Instruction &instr);

        Value interpret_fcall_for_eager_eval(ThreadContext &context, const Instruction &instr);

        Value interpret_rcall_for_eager_eval(ThreadContext &context, const Instruction &instr);

        Value interpret_icall_for_lazy_eval(ThreadContext &context, const Instruction &instr);

        Value interpret_fcall_for_lazy_eval(ThreadContext &context, const Instruction &instr);

        Value interpret_rcall_for_lazy_eval(ThreadContext &context, const Instruction &instr);
      protected:
        bool enter_to_fun(ThreadContext &context, std::size_t i, bool &is_fun_result);

        bool leave_from_fun(ThreadContext &context);
      private:
        bool call_fun(ThreadContext &context, std::size_t i, int value_type);

        bool call_fun_for_force(ThreadContext &context, std::size_t i);

        bool force(ThreadContext &context, Value &value);
        
        bool force(ThreadContext &context, ReturnValue &value);

        bool force_int(ThreadContext &context, std::int64_t &i, Value &value);

        bool force_float(ThreadContext &context, double &f, Value &value);

        bool force_ref(ThreadContext &context, Reference &r, Value &value);
      };
    }
  }
}

#endif
