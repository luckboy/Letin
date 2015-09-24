/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "alloc_wrapper.hpp"

namespace letin
{
  namespace vm
  {
    namespace test
    {
      AllocatorWrapper::~AllocatorWrapper() {}

      void *AllocatorWrapper::allocate(size_t size)
      {
        void *ptr = _M_alloc->allocate(size);
        if(ptr == nullptr) return nullptr;
        _M_alloc_ops.push_back(AllocatorOperation(ALLOC, ptr));
        return ptr;
      }

      void AllocatorWrapper::free(void *ptr)
      {
        _M_alloc->free(ptr);
        _M_alloc_ops.push_back(AllocatorOperation(FREE, ptr));
      }

      void AllocatorWrapper::lock() { _M_alloc->lock(); }

      void AllocatorWrapper::unlock() { _M_alloc->unlock(); }
    }
  }
}
