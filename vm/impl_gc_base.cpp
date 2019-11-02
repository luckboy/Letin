/****************************************************************************
 *   Copyright (C) 2014-2015, 2019 Łukasz Szpakowski.                       *
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
#include "thread_stop_cont.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplGarbageCollectorBase::Threads::Threads(set<ThreadContext *> &contexts) :
        _M_stop_cont(priv::new_thread_stop_cont()), contexts(contexts) {}

      ImplGarbageCollectorBase::Threads::~Threads() { priv::delete_thread_stop_cont(_M_stop_cont); }

      void ImplGarbageCollectorBase::Threads::lock()
      {
        for(auto context : contexts) context->interruptible_fun_mutex().lock();
        stop_threads(_M_stop_cont, [this](function<void (thread &)> fun) {
          for(auto context : contexts) {
            if(!context->interruptible_fun_flag()) fun(context->system_thread());
          }
        });
      }

      void ImplGarbageCollectorBase::Threads::unlock()
      {
        continue_threads(_M_stop_cont, [this](function<void (thread &)> fun) {
          for(auto context : contexts) {
            if(!context->interruptible_fun_flag()) fun(context->system_thread());
          }
        });
        for(auto context : contexts) context->interruptible_fun_mutex().unlock();
      }

      ImplGarbageCollectorBase::ImplForkHandler::~ImplForkHandler() {}

      void ImplGarbageCollectorBase::ImplForkHandler::pre_fork()
      {
        _M_gc->_M_gc_thread_mutex.lock();
        {
          unique_lock<mutex> lock(_M_gc->_M_interval_mutex);
          _M_gc->_M_is_locked_gc_thread = true;
          _M_gc->_M_interval_cv.notify_one();
        }
        _M_gc->_M_other_thread_mutex.lock();
        _M_gc->_M_interval_mutex.lock();
        _M_gc->_M_gc_mutex.lock();
        _M_gc->_M_forking_thread_context = nullptr;
        for(auto context : _M_gc->_M_thread_contexts) {
          context->interruptible_fun_mutex().lock();
          if(context->system_thread().get_id() == this_thread::get_id())
            _M_gc->_M_forking_thread_context = context;
        }
      }

      void ImplGarbageCollectorBase::ImplForkHandler::post_fork(bool is_child)
      {
        if(is_child) {
          for(auto context : _M_gc->_M_thread_contexts) {
            if(context == _M_gc->_M_forking_thread_context) {
              new (&(context->interruptible_fun_mutex())) mutex;
            } else {
              context->set_gc(nullptr);
              new (&(context->system_thread())) thread;
              context->free_stack();
            }
          }
          _M_gc->_M_thread_contexts.clear();
          _M_gc->_M_thread_contexts.insert(_M_gc->_M_forking_thread_context);
        } else {
          for(auto context : _M_gc->_M_thread_contexts) {
            context->interruptible_fun_mutex().unlock();
          }
        }
        bool is_forking_thread_context = (_M_gc->_M_forking_thread_context != nullptr);
        _M_gc->_M_forking_thread_context = nullptr;
        _M_gc->_M_is_locked_gc_thread = false;
        bool is_started = _M_gc->_M_is_started;
        if(is_child) {
          _M_gc->_M_is_started = false;
          _M_gc->_M_must_stop_from_vm_thread = true;
          new (&(_M_gc->_M_interval_cv)) condition_variable;
          new (&(_M_gc->_M_gc_mutex)) recursive_mutex;
          new (&(_M_gc->_M_interval_mutex)) mutex;
          new (&(_M_gc->_M_other_thread_mutex)) mutex;
          new (&(_M_gc->_M_gc_thread_mutex)) mutex;
        } else {
          _M_gc->_M_gc_mutex.unlock();
          _M_gc->_M_interval_mutex.unlock();
          _M_gc->_M_other_thread_mutex.unlock();
          _M_gc->_M_gc_thread_mutex.unlock();
        }
        if(is_child && is_started && is_forking_thread_context) {
          new (&(_M_gc->_M_gc_thread)) thread;
          _M_gc->start_gc_thread();
        }
      }

      ImplGarbageCollectorBase::~ImplGarbageCollectorBase()
      {
        if(_M_impl_fork_handler != nullptr)
          delete_fork_handler(FORK_HANDLER_PRIO_GC, _M_impl_fork_handler);
      }

      void ImplGarbageCollectorBase::add_thread_context(ThreadContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_thread_contexts.insert(context);
      }

      void ImplGarbageCollectorBase::delete_thread_context(ThreadContext *context)
      {
        lock_guard<GarbageCollector> gaurd(*this);
        _M_thread_contexts.erase(context);
      }

      size_t ImplGarbageCollectorBase::thread_context_count()
      { return _M_thread_contexts.size(); }

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

      size_t ImplGarbageCollectorBase::vm_context_count()
      { return _M_vm_contexts.size(); }

      void ImplGarbageCollectorBase::start_gc_thread()
      {
        bool is_started;
        {
          unique_lock<mutex> lock(_M_interval_mutex);
          is_started = _M_is_started;
          _M_is_started = true;
        }
        if(!is_started) {
          _M_gc_thread = thread([this]() {
            unique_lock<mutex> other_thread_lock(_M_other_thread_mutex);
            while(true) {
              // Sleeps.
              auto rel_time = chrono::microseconds(_M_interval_usecs);
              auto abs_time1 = chrono::high_resolution_clock::now();
              do {
                unique_lock<mutex> interval_lock(_M_interval_mutex);
                do {
                  if(_M_is_locked_gc_thread) {
                    interval_lock.unlock();
                    other_thread_lock.unlock();
                    { lock_guard<mutex> guard(_M_gc_thread_mutex); }
                    other_thread_lock.lock();
                    interval_lock.lock();
                    auto abs_time2 = chrono::high_resolution_clock::now();
                    auto time_diff = abs_time2 - abs_time1;
                    if(time_diff >= chrono::microseconds(1)) {
                      auto tmp_time_diff = chrono::duration_cast<decltype(rel_time)>(time_diff);
                      if(rel_time > tmp_time_diff)
                        rel_time -= tmp_time_diff;
                      else
                        break;
                    }
                    abs_time1 = abs_time2;
                  }
                  if(!_M_is_started) return;
                } while(_M_interval_cv.wait_for(interval_lock, rel_time) != cv_status::timeout);
              } while(_M_is_locked_gc_thread);
              // Collects.
              collect();
            }
          });
        }
      }

      void ImplGarbageCollectorBase::stop_gc_thread()
      {
        {
          unique_lock<mutex> lock(_M_interval_mutex);
          _M_is_started = false;
          _M_interval_cv.notify_one();
        }
        if(_M_gc_thread.joinable()) _M_gc_thread.join();
      }

      void ImplGarbageCollectorBase::start() { start_gc_thread(); }

      void ImplGarbageCollectorBase::stop() { stop_gc_thread(); }

      void ImplGarbageCollectorBase::lock() { _M_gc_mutex.lock(); }

      void ImplGarbageCollectorBase::unlock() { _M_gc_mutex.unlock(); }

      thread &ImplGarbageCollectorBase::system_thread() { return _M_gc_thread; }

      bool ImplGarbageCollectorBase::must_stop_from_vm_thread() { return _M_must_stop_from_vm_thread; }
    }
  }
}
