/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _SEMAPHORE_HPP
#define _SEMAPHORE_HPP

#include <condition_variable>
#include <mutex>

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      class Semaphore
      {
        std::mutex _M_mutex;
        std::condition_variable _M_cv;
        int _M_value;
      public:
        Semaphore() {}

        void op(int value)
        {
          if(value > 0) {
            std::unique_lock<std::mutex> lock(_M_mutex);
            _M_value += value;
            _M_cv.notify_one();
          } else if(value < 0) {
            std::unique_lock<std::mutex> lock(_M_mutex);
            while(_M_value + value <= 0) _M_cv.wait(lock);
            _M_value += value;
          }
        }

        int value()
        {
          std::unique_lock<std::mutex> lock(_M_mutex);
          return _M_value;
        }

        void set_value(int value)
        {
          std::unique_lock<std::mutex> lock(_M_mutex);
          _M_value = value;
          if(value > 0) _M_cv.notify_one();
        }

        void lock() { _M_mutex.lock(); }

        void unlock() { _M_mutex.unlock(); }
      };

      class SemaphoreGuard
      {
        Semaphore &_M_sem;
        int _M_value;
      public:
        SemaphoreGuard(Semaphore &sem, int value) : _M_sem(sem), _M_value(value)
        { _M_sem.op(-value); }

        ~SemaphoreGuard() { _M_sem.op(_M_value); }
      };
    }
  }
}

#endif
