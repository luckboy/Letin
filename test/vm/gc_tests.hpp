/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _GC_TESTS_HPP
#define _GC_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include <memory>
#include <mutex>
#include <vector>
#include <utility>
#include <letin/vm.hpp>
#include "alloc_wrapper.hpp"
#include "impl_env.hpp"
#include "vm.hpp"

#define DECL_IMPL_GC_TESTS(clazz)                                               \
class clazz##Tests : public GarbageCollectorTests                               \
{                                                                               \
  CPPUNIT_TEST_SUB_SUITE(clazz##Tests, GarbageCollectorTests);                  \
  CPPUNIT_TEST_SUITE_END();                                                     \
public:                                                                         \
  GarbageCollector *new_gc(Allocator *alloc);                                   \
}

#define DEF_IMPL_GC_TESTS(clazz)                                                \
CPPUNIT_TEST_SUITE_REGISTRATION(clazz##Tests);                                  \
GarbageCollector *clazz##Tests::new_gc(Allocator *alloc)                        \
{ return new impl::clazz(alloc); }                                              \
class clazz##Tests

namespace letin
{
  namespace vm
  {
    namespace test
    {
      class GarbageCollectorTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(GarbageCollectorTests);
        CPPUNIT_TEST(test_gc_collects_one_object);
        CPPUNIT_TEST(test_gc_collects_many_objects);
        CPPUNIT_TEST(test_gc_collects_cycle_references);
        CPPUNIT_TEST(test_gc_does_not_collect_objects_in_thread_contexts);
        CPPUNIT_TEST(test_gc_does_not_collect_objects_in_vm_contexts);
        CPPUNIT_TEST(test_gc_destructor_free_all_objects);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        AllocatorWrapper *_M_alloc;
        GarbageCollector *_M_gc;
        std::mutex *_M_thread_context_mutex;
      public:

        virtual GarbageCollector *new_gc(Allocator *alloc) = 0;

        ThreadContext *new_thread_context(const VirtualMachineContext &vm_context)
        {
          ThreadContext *context = new ThreadContext(vm_context);
          context->start([this]() {
            _M_thread_context_mutex->lock();
            _M_thread_context_mutex->unlock();
          });
          return context;
        }

        VirtualMachineContext *new_vm_context()
        { return new impl::ImplEnvironment(); }

        impl::ImplEnvironment *new_impl_env()
        { return new impl::ImplEnvironment(); }

        AllocatorOperation make_alloc(Reference ref)
        {
          void *ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ref.ptr()) - _M_gc->header_size());
          return AllocatorOperation(ALLOC, ptr);
        }

        AllocatorOperation make_free(Reference ref)
        {
          void *ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ref.ptr()) - _M_gc->header_size());
          return AllocatorOperation(FREE, ptr);
        }

        void setUp();

        void tearDown();

        void test_gc_collects_one_object();
        void test_gc_collects_many_objects();
        void test_gc_collects_cycle_references();
        void test_gc_does_not_collect_objects_in_thread_contexts();
        void test_gc_does_not_collect_objects_in_vm_contexts();
        void test_gc_destructor_free_all_objects();
      };

      DECL_IMPL_GC_TESTS(MarkSweepGarbageCollector);
    }
  }
}

#endif
