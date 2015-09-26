/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_GC_BASE_HPP
#define _IMPL_GC_BASE_HPP

#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <letin/vm.hpp>
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      class ThreadStopCont;
    }

    namespace impl
    {
      class ImplGarbageCollectorBase : public GarbageCollector
      {
      protected:
        class Threads
        {
          priv::ThreadStopCont *_M_stop_cont;
        public:
          std::set<ThreadContext *> &contexts;

          Threads(std::set<ThreadContext *> &contexts);

          ~Threads();

          void lock();

          void unlock();
        };

        class ImplForkHandler : public ForkHandler
        {
          ImplGarbageCollectorBase *_M_gc;
        public:
          ImplForkHandler(ImplGarbageCollectorBase *gc) {}
          
          ~ImplForkHandler();

          virtual void pre_fork();

          virtual void post_fork(bool is_child);
        };

        std::thread _M_gc_thread;
        std::recursive_mutex _M_gc_mutex;
        std::set<ThreadContext *> _M_thread_contexts;
        std::set<VirtualMachineContext *> _M_vm_contexts;
        Threads _M_threads;
      private:
        bool _M_is_started;
        unsigned int _M_interval_usecs;
        std::mutex _M_interval_mutex;
        std::condition_variable _M_interval_cv;
        std::mutex _M_other_thread_mutex;
        std::mutex _M_gc_thread_mutex;
        bool _M_is_locked_gc_thread;
        ImplForkHandler *_M_impl_fork_handler;
      public:
        ImplGarbageCollectorBase(Allocator *alloc, unsigned int interval_usecs) :
          GarbageCollector(alloc), _M_threads(_M_thread_contexts), _M_is_started(false),
          _M_interval_usecs(interval_usecs), _M_is_locked_gc_thread(false),
          _M_impl_fork_handler(nullptr) {}

        ~ImplGarbageCollectorBase();
      protected:
        void add_impl_fork_handler(ImplForkHandler *handler)
        {
          _M_impl_fork_handler = handler;
          add_fork_handler(FORK_HANDLER_PRIO_GC, _M_impl_fork_handler);
        }
      public:
        void add_thread_context(ThreadContext *context);

        void delete_thread_context(ThreadContext *context);

        void add_vm_context(VirtualMachineContext *context);

        void delete_vm_context(VirtualMachineContext *context);
      private:
        void start_gc_thread();

        void stop_gc_thread();
      public:
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
