/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _THREAD_HPP
#define _THREAD_HPP

#include <thread>

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      void initialize_threads();
      
      void finalize_threads();

      void stop_thread(std::thread &thr);

      void continue_thread(std::thread &thr);
    }
  }
}

#endif
