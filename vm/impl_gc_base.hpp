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

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplGarbageCollectorBase : public GarbageCollector
      {
      protected:
        std::thread _M_gc_thread;
        std::recursive_mutex _M_gc_mutex;
        std::set<ThreadContext *> _M_thread_contexts;
        std::set<VirtualMachineContext *> _M_vm_contexts;
      private:
        bool _M_is_started;
        unsigned int _M_interval_usecs;
        std::mutex _M_interval_mutex;
        std::condition_variable _M_interval_cv;
      public:
        ImplGarbageCollectorBase(Allocator *alloc) :
          GarbageCollector(alloc), _M_is_started(false), _M_interval_usecs(500) {}

        ~ImplGarbageCollectorBase();
      protected:
        virtual void collect_in_gc_thread();
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
