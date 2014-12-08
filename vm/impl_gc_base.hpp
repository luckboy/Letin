/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_GC_BASE_HPP
#define _IMPL_GC_BASE_HPP

#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>
#include <letin/vm.hpp>
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplGarbageCollectorBase : public GarbageCollector
      {
      protected:
        struct ThreadContexts
        {
          std::set<ThreadContext *> contexts;

          void lock() { for(auto context : contexts) context->stop(); }

          void unlock() { for(auto context : contexts) context->cont(); }
        };

        std::thread _M_gc_thread;
        std::recursive_mutex _M_gc_mutex;
        ThreadContexts _M_thread_contexts;
        std::set<VirtualMachineContext *> _M_vm_contexts;
      private:
        bool _M_is_started;
        unsigned int _M_interval_usecs;
        std::mutex _M_interval_mutex;
        std::condition_variable _M_interval_cv;
      public:
        ImplGarbageCollectorBase(Allocator *alloc, unsigned int interval_usecs) :
          GarbageCollector(alloc), _M_is_started(false), _M_interval_usecs(interval_usecs) {}

        ~ImplGarbageCollectorBase();
      protected:
        virtual void collect_in_gc_thread() = 0;
      public:
        void add_thread_context(ThreadContext *context);

        void delete_thread_context(ThreadContext *context);

        void add_vm_context(VirtualMachineContext *context);

        void delete_vm_context(VirtualMachineContext *context);

        void start();

        void stop();

        void lock();

        void unlock();

        std::thread &system_thread();
      };
    }
  }
}

#endif
