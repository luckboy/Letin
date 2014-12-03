/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_VM_BASE_HPP
#define _IMPL_VM_BASE_HPP

#include <list>
#include <thread>
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
        struct Thread
        {
          std::thread thread;
          ThreadContext context;
        };
        ImplEnvironment _M_env;
        bool _M_is_entry;
        std::size_t _M_entry;
        std::list<Thread> _M_threads;

        ImplVirtualMachineBase(GarbageCollector *gc) : VirtualMachine(gc) {}
      public:
        virtual ~ImplVirtualMachineBase();

        bool load(void *ptr, std::size_t size);
      protected:
        ReturnValue start_in_thread(ThreadContext &context, std::size_t i);
      public:
        Environment &env();

        bool is_entry();

        std::size_t entry();
      };
    }
  }
}

#endif
