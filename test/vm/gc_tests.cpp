/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cstring>
#include "gc_tests.hpp"
#include "new_alloc.hpp"
#include "mark_sweep_gc.hpp"

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      void GarbageCollectorTests::setUp()
      {
        _M_alloc = new AllocatorWrapper(new impl::NewAllocator());
        _M_gc = new_gc(_M_alloc);
        _M_thread_context_mutex = new mutex();
        _M_thread_context_mutex->lock();
      }

      void GarbageCollectorTests::tearDown()
      {
        delete _M_thread_context_mutex;
        delete _M_gc;
        delete _M_alloc;
      }

      void GarbageCollectorTests::test_gc_collects_one_object()
      {
        unique_ptr<VirtualMachineContext> vm_context(new_vm_context());
        unique_ptr<ThreadContext> thread_context(new_thread_context(*vm_context));
        _M_gc->add_vm_context(vm_context.get());
        _M_gc->add_thread_context(thread_context.get());
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 5));
        thread_context->regs().rv.raw().r = ref;
        strcpy(reinterpret_cast<char *>(ref->raw().is8), "test");
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(1UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref) == _M_alloc->alloc_ops()[0]);
        thread_context->regs().rv.raw().r = Reference();
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(2UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_free(ref) == _M_alloc->alloc_ops()[1]);
        _M_thread_context_mutex->unlock();
        thread_context->system_thread().join();
      }

      DEF_IMPL_GC_TESTS(MarkSweepGarbageCollector);
    }
  }
}
