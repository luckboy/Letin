/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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
#include "util.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class InterpreterVirtualMachine : public ImplVirtualMachineBase
      {
      public:
        InterpreterVirtualMachine(Loader *loader, GarbageCollector *gc) :
          ImplVirtualMachineBase(loader, gc) {}

        ~InterpreterVirtualMachine();
      protected:
        ReturnValue start_in_thread(std::size_t i, const std::vector<Value> &args, ThreadContext &context);

        virtual void interpret(ThreadContext &context);

        bool interpret_instr(ThreadContext &context);
      private:
        Value interpret_op(ThreadContext &context, const Instruction &instr);
      protected:
        virtual bool enter_to_fun(ThreadContext &context, std::size_t i);

        virtual bool leave_from_fun(ThreadContext &context);
      private:
        bool call_fun(ThreadContext &context, std::size_t i);
      };
    }
  }
}

#endif
