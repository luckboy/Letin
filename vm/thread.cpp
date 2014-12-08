/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <csignal>
#include <pthread.h>
#include <thread>
#include "thread.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      static bool is_continuing = true;
      
      static void usr1_handler(int signum)
      {
        if(!is_continuing) {
          sigset_t sigmask;
          sigfillset(&sigmask);
          sigdelset(&sigmask, SIGUSR2);
          do sigpending(&sigmask); while(!is_continuing);
        }
      }

      static void usr2_handler(int signum) {}
      
      void initialize_threads()
      {
        signal(SIGUSR1, usr1_handler);
        signal(SIGUSR2, usr2_handler);
      }

      void finalize_threads() {}

      void stop_thread(std::thread &thr)
      {
        is_continuing = false;
        ::pthread_kill(thr.native_handle(), SIGUSR1);
      }

      void continue_thread(std::thread &thr)
      {
        is_continuing = true;
        ::pthread_kill(thr.native_handle(), SIGUSR2);
      }
    }
  }
}
