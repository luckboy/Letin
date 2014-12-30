/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <csignal>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#include <thread>
#include "thread_stop_cont.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      class ThreadStopCont
      {
      public:
        ::sem_t stopping_sem;
        ::sem_t continuing_sem;

        ThreadStopCont()
        {
          ::sem_init(&stopping_sem, 0, 0);
          ::sem_init(&continuing_sem, 0, 0);
        }

        ~ThreadStopCont()
        {
          ::sem_destroy(&continuing_sem);
          ::sem_destroy(&stopping_sem);
        }
      };

      static mutex thread_stop_cont_mutex;
      static volatile bool is_stopping = false;
      static volatile bool is_continuing = false;
      static ThreadStopCont *volatile thread_stop_cont;

      static void usr1_handler(int signum)
      {
        if(is_stopping) {
          ThreadStopCont *tmp_thread_stop_cont = thread_stop_cont;
          ::sem_post(&(tmp_thread_stop_cont->stopping_sem));
          sigset_t sigmask;
          while(!is_continuing) {
            sigfillset(&sigmask);
            sigdelset(&sigmask, SIGUSR2);
            sigsuspend(&sigmask);
          }
          ::sem_post(&(tmp_thread_stop_cont->continuing_sem));
        }
      }

      static void usr2_handler(int signum) {}

      void initialize_thread_stop_cont()
      {
        signal(SIGUSR1, usr1_handler);
        signal(SIGUSR2, usr2_handler);
      }

      void finalize_thread_stop_cont() {}

      ThreadStopCont *new_thread_stop_cont() { return new ThreadStopCont(); }

      void delete_thread_stop_cont(ThreadStopCont *stop_cont) { delete stop_cont; }

      void stop_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        lock_guard<mutex> guard(thread_stop_cont_mutex);
        is_stopping = true;
        thread_stop_cont = stop_cont;
        fun([](thread &thr) { ::pthread_kill(thr.native_handle(), SIGUSR1); });
        fun([stop_cont](thread &thr) { ::sem_wait(&(stop_cont->stopping_sem)); });
        is_stopping = false;
      }

      void continue_threads(ThreadStopCont *stop_cont, function<void (function<void (thread &)>)> fun)
      {
        lock_guard<mutex> guard(thread_stop_cont_mutex);
        is_continuing = true;
        fun([](thread &thr) { ::pthread_kill(thr.native_handle(), SIGUSR2); });
        fun([stop_cont](thread &thr) { ::sem_wait(&(stop_cont->continuing_sem)); });
        is_stopping = false;
      }
    }
  }
}
