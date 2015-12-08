/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _THREAD_STOP_CONT_HPP
#define _THREAD_STOP_CONT_HPP

#include <functional>
#include <thread>

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      class ThreadStopCont;

      void initialize_thread_stop_cont();

      void finalize_thread_stop_cont();

      void start_thread_stop_cont();

      void stop_thread_stop_cont();

      ThreadStopCont *new_thread_stop_cont();

      void delete_thread_stop_cont(ThreadStopCont *stop_cont);

      void stop_threads(ThreadStopCont *stop_cont, std::function<void (std::function<void (std::thread &)>)> fun);

      void continue_threads(ThreadStopCont *stop_cont, std::function<void (std::function<void (std::thread &)>)> fun);
    }
  }
}

#endif
