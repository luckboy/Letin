/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_VM_BASE_HPP
#define _IMPL_VM_BASE_HPP

#include <letin/vm.hpp>
#include "vm.hpp"
#include "impl_env.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplVirtualMachineBase : public VirtualMachine
      {
      protected:
        ImplEnvironment _M_env;
        bool _M_is_entry;
        std::size_t _M_entry;

        ImplVirtualMachineBase(GarbageCollector *gc) : VirtualMachine(gc) {}
      public:
        ~ImplVirtualMachineBase();

        bool load(void *ptr, std::size_t size);

        Thread start(std::size_t i, std::function<void (const ReturnValue &)> fun);

      protected:
        ReturnValue start_in_thread(std::size_t i, ThreadContext &context);
      public:
        Environment &env();

        bool is_entry();

        std::size_t entry();
      };
    }
  }
}

#endif
