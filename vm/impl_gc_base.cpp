/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <letin/vm.hpp>
#include "impl_gc_base.hpp"
#include "vm.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplGarbageCollectorBase::~ImplGarbageCollectorBase() { stop(); }

      void ImplGarbageCollectorBase::add_thread_context(ThreadContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_thread_contexts.contexts.insert(context);
      }

      void ImplGarbageCollectorBase::delete_thread_context(ThreadContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_thread_contexts.contexts.erase(context);
      }

      void ImplGarbageCollectorBase::add_vm_context(VirtualMachineContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_vm_contexts.insert(context);
      }

      void ImplGarbageCollectorBase::delete_vm_context(VirtualMachineContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_vm_contexts.erase(context);
      }

      void ImplGarbageCollectorBase::start()
      {
        bool is_started;
        {
          unique_lock<mutex> lock(_M_interval_mutex);
          is_started = _M_is_started;
          _M_is_started = true;
        }
        if(!is_started) {
          _M_gc_thread = thread([this]() {
            while(true) {
              // Sleeps.
              {
                unique_lock<mutex> lock(_M_interval_mutex);
                do {
                  if(!_M_is_started) return;
                } while(_M_interval_cv.wait_for(lock, chrono::milliseconds(_M_interval_usecs)) != cv_status::timeout);
              }
              // Collects.
              collect_in_gc_thread();
            }
          });
        }
      }

      void ImplGarbageCollectorBase::stop()
      {
        {
          unique_lock<mutex> lock(_M_interval_mutex);
          _M_is_started = false;
          _M_interval_cv.notify_one();
        }
        if(_M_gc_thread.joinable()) _M_gc_thread.join();
      }

      void ImplGarbageCollectorBase::lock() { _M_gc_mutex.lock(); }

      void ImplGarbageCollectorBase::unlock() { _M_gc_mutex.unlock(); }

      thread &ImplGarbageCollectorBase::system_thread() { return _M_gc_thread; }
    }
  }
}
