/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _ALLOC_NEW_ALLOC_HPP
#define _ALLOC_NEW_ALLOC_HPP

#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class NewAllocator : public Allocator
      {
      public:
        NewAllocator() {}
        
        ~NewAllocator();
        
        void *allocate(std::size_t size);

        void free(void *ptr);

        void lock();

        void unlock();
      };
    }
  }
}

#endif
