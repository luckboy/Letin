/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "new_alloc.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      NewAllocator::~NewAllocator() {}
        
      void *NewAllocator::allocate(size_t size)
      { try { return reinterpret_cast<void *>(new char[size]); } catch(...) { return nullptr; } }

      void NewAllocator::free(void *ptr)
      { delete[] (reinterpret_cast<char *>(ptr)); }
    }
  }
}
