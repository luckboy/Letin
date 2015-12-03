/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#if defined(__unix__)
#include <atomic>
#include <csignal>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#elif defined(_WIN32) || defined(_WIN64)
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <pthread.h>
#endif
#include <windows.h>
#else
#error "Unsupported operating system."
#endif
#include <thread>
#include "thread_stop_cont.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace priv
    {
#if defined(__unix__)

      //
      // An implementation for Unix-like systems.
      //

      class ThreadStopCont
      {
      public:
        ::sem_t stopping_sem;
        ::sem_t continuing_sem;
        volatile bool has_stopping;

        ThreadStopCont()
        {
          ::sem_init(&stopping_sem, 0, 0);
          ::sem_init(&continuing_sem, 0, 0);
          has_stopping = false;
        }

        ~ThreadStopCont()
        {
          ::sem_destroy(&continuing_sem);
          ::sem_destroy(&stopping_sem);
        }
      };

      static mutex thread_stop_cont_mutex;
      static ThreadStopCont *volatile thread_stop_cont;

      static void usr1_handler(int signum)
      {
        ThreadStopCont *tmp_thread_stop_cont = thread_stop_cont;
        if(tmp_thread_stop_cont->has_stopping) {
          ::sem_post(&(tmp_thread_stop_cont->stopping_sem));
          sigset_t sigmask;
          while(tmp_thread_stop_cont->has_stopping) {
            sigfillset(&sigmask);
            sigdelset(&sigmask, SIGUSR2);
            sigsuspend(&sigmask);
            atomic_thread_fence(memory_order_acquire);
          }
          ::sem_post(&(tmp_thread_stop_cont->continuing_sem));
        }
      }

      static void usr2_handler(int signum) {}

      void initialize_thread_stop_cont()
      {
        struct sigaction usr1_sigaction;
        usr1_sigaction.sa_handler = usr1_handler;
        sigemptyset(&(usr1_sigaction.sa_mask));
        usr1_sigaction.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &usr1_sigaction, nullptr);

        struct sigaction usr2_sigaction;
        usr2_sigaction.sa_handler = usr2_handler;
        sigemptyset(&(usr2_sigaction.sa_mask));
        usr2_sigaction.sa_flags = SA_RESTART;
        sigaction(SIGUSR2, &usr2_sigaction, nullptr);
      }

      void finalize_thread_stop_cont() {}

      void start_thread_stop_cont()
      {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGUSR1);
        ::pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr);
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGUSR2);
        ::pthread_sigmask(SIG_BLOCK, &sigmask, nullptr);
      }

      void stop_thread_stop_cont() {}

      ThreadStopCont *new_thread_stop_cont() { return new ThreadStopCont(); }

      void delete_thread_stop_cont(ThreadStopCont *stop_cont) { delete stop_cont; }

      void stop_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        lock_guard<mutex> guard(thread_stop_cont_mutex);
        thread_stop_cont = stop_cont;
        stop_cont->has_stopping = true;
        atomic_thread_fence(memory_order_release);
        fun([stop_cont](thread &thr) {
          if(::pthread_kill(thr.native_handle(), SIGUSR1) != 0)
            ::sem_post(&(stop_cont->stopping_sem));
        });
        fun([stop_cont](thread &thr) { ::sem_wait(&(stop_cont->stopping_sem)); });
      }

      void continue_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        stop_cont->has_stopping = false;
        atomic_thread_fence(memory_order_release);
        fun([stop_cont](thread &thr) { 
          if(::pthread_kill(thr.native_handle(), SIGUSR2) != 0)
            ::sem_post(&(stop_cont->continuing_sem));
        });
        fun([stop_cont](thread &thr) { ::sem_wait(&(stop_cont->continuing_sem)); });
      }

#elif defined(_WIN32) || defined(_WIN64)

      //
      // An implementation for Windows.
      //
      
      class ThreadStopCont {};

      void initialize_thread_stop_cont() {}

      void finalize_thread_stop_cont() {}

      void start_thread_stop_cont() {}

      void stop_thread_stop_cont() {}

      ThreadStopCont *new_thread_stop_cont() { return nullptr; }

      void delete_thread_stop_cont(ThreadStopCont *stop_cont) {}

      void stop_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        fun([](thread &thr) {
#if defined(__MINGW32__) || defined(__MINGW64__)
          ::HANDLE handle = reinterpret_cast<::HANDLE>(::pthread_gethandle(thr.native_handle()));
#else
          ::HANDLE handle = thr.native_handle();
#endif
          ::SuspendThread(handle);
        });
      }

      void continue_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        fun([](thread &thr) {
#if defined(__MINGW32__) || defined(__MINGW64__)
          ::HANDLE handle = reinterpret_cast<::HANDLE>(::pthread_gethandle(thr.native_handle()));
#else
          ::HANDLE handle = thr.native_handle();
#endif
          ::ResumeThread(handle);
        });
      }

#else
#error "Unsupported operating system."
#endif
    }
  }
}
