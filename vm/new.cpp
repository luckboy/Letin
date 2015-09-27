/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cstdlib>
#include <new>
#include <letin/vm.hpp>

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace vm
  {
    recursive_mutex new_mutex;
  }
}

void *operator new(size_t size)
{
  lock_guard<recursive_mutex> guard(new_mutex);
  void *ptr = malloc(size > 0 ? size : 1);
  if(ptr == nullptr) throw bad_alloc();
  return ptr;
}

void *operator new(size_t size, const nothrow_t &nothrow) noexcept
{
  lock_guard<recursive_mutex> guard(new_mutex);
  return malloc(size > 0 ? size : 1);
}

void *operator new[](size_t size) { return operator new(size); }

void *operator new[](size_t size, const nothrow_t &nothrow) noexcept { return operator new(size, nothrow); }

void operator delete(void *ptr) noexcept
{
  lock_guard<recursive_mutex> guard(new_mutex);
  free(ptr);
}

void operator delete(void *ptr, const nothrow_t &nothrow) noexcept { operator delete(ptr); }

void operator delete[](void *ptr) noexcept { operator delete(ptr); }

void operator delete[](void *ptr, const nothrow_t &nothrow) noexcept { operator delete(ptr); }
