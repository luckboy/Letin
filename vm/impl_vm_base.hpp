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
        bool _M_has_entry;
        std::size_t _M_entry;

        ImplVirtualMachineBase(Loader *loader, GarbageCollector *gc) : VirtualMachine(loader, gc) {}
      public:
        ~ImplVirtualMachineBase();

        bool load(void *ptr, std::size_t size, bool is_auto_freeing);

        Thread start(std::size_t i, const std::vector<Value> &args, std::function<void (const ReturnValue &)> fun);
      protected:
        virtual ReturnValue start_in_thread(std::size_t i, const std::vector<Value> &args, ThreadContext &context);
      public:
        Environment &env();

        bool has_entry();

        std::size_t entry();
      };
    }
  }
}

#endif
