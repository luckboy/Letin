/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <cstring>
#include "gc_tests.hpp"
#include "mark_sweep_gc.hpp"
#include "new_alloc.hpp"

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
        CPPUNIT_ASSERT_EQUAL(1UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref) == _M_alloc->alloc_ops()[0]);
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(1UL, _M_alloc->alloc_ops().size());
        thread_context->regs().rv.raw().r = Reference();
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(2UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_free(ref) == _M_alloc->alloc_ops()[1]);
        _M_thread_context_mutex[0].unlock();
        thread_context->system_thread().join();
      }
      
      void GarbageCollectorTests::test_gc_collects_many_objects()
      {
        unique_ptr<VirtualMachineContext> vm_context(new_vm_context());
        unique_ptr<ThreadContext> thread_context1(new_thread_context(*vm_context));
        unique_ptr<ThreadContext> thread_context2(new_thread_context(*vm_context));
        _M_gc->add_vm_context(vm_context.get());
        _M_gc->add_thread_context(thread_context1.get());
        _M_gc->add_thread_context(thread_context2.get());
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "test1");
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref2->raw().is8), "test2");
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref3->raw().is8), "test3");
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref4->raw().is8), "test4");
        Reference ref5(_M_gc->new_object(OBJECT_TYPE_RARRAY, 3));
        ref5->raw().rs[0] = ref1;
        ref5->raw().rs[1] = ref2;
        ref5->raw().rs[2] = ref2;
        Reference ref6(_M_gc->new_object(OBJECT_TYPE_TUPLE, 4));
        ref6->set_elem(0, Value(ref5));
        ref6->set_elem(1, Value(1));
        ref6->set_elem(2, Value(ref5));
        ref6->set_elem(3, Value(1.0));
        Reference ref7(_M_gc->new_object(OBJECT_TYPE_RARRAY, 2));
        ref7->raw().rs[0] = ref3;
        ref7->raw().rs[1] = ref4;
        Reference ref8(_M_gc->new_object(OBJECT_TYPE_TUPLE, 5));
        ref8->set_elem(0, Value(1));
        ref8->set_elem(1, Value(ref3));
        ref8->set_elem(2, Value(1.0));
        ref8->set_elem(3, Value(ref7));
        ref8->set_elem(4, Value(2.0));
        thread_context1->push_local_var(Value(ref6));
        thread_context1->push_arg(Value(ref5));
        thread_context1->push_arg(Value(ref4));
        thread_context2->push_local_var(Value(1));
        thread_context2->push_arg(Value(ref1));
        thread_context2->push_arg(Value(ref7));
        thread_context2->regs().rv.raw().r = ref8;
        CPPUNIT_ASSERT_EQUAL(8UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref1) == _M_alloc->alloc_ops()[0]);
        CPPUNIT_ASSERT(make_alloc(ref2) == _M_alloc->alloc_ops()[1]);
        CPPUNIT_ASSERT(make_alloc(ref3) == _M_alloc->alloc_ops()[2]);
        CPPUNIT_ASSERT(make_alloc(ref4) == _M_alloc->alloc_ops()[3]);
        CPPUNIT_ASSERT(make_alloc(ref5) == _M_alloc->alloc_ops()[4]);
        CPPUNIT_ASSERT(make_alloc(ref6) == _M_alloc->alloc_ops()[5]);
        CPPUNIT_ASSERT(make_alloc(ref7) == _M_alloc->alloc_ops()[6]);
        CPPUNIT_ASSERT(make_alloc(ref8) == _M_alloc->alloc_ops()[7]);
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(8UL, _M_alloc->alloc_ops().size());
        thread_context1->pop_args();
        thread_context2->pop_args();
        thread_context2->regs().rv.raw().r = Reference();
        _M_gc->collect();
        const vector<AllocatorOperation> &alloc_ops = _M_alloc->alloc_ops();
        CPPUNIT_ASSERT_EQUAL(12UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref8)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref7)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref3)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref4)) == 1);
        _M_thread_context_mutex->unlock();
        thread_context1->system_thread().join();
        thread_context2->system_thread().join();
      }

      void GarbageCollectorTests::test_gc_collects_cycle_references()
      {
        unique_ptr<VirtualMachineContext> vm_context(new_vm_context());
        unique_ptr<ThreadContext> thread_context(new_thread_context(*vm_context));
        _M_gc->add_vm_context(vm_context.get());
        _M_gc->add_thread_context(thread_context.get());
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_RARRAY, 1));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_RARRAY, 1));
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref1->raw().rs[0] = ref4;
        ref2->set_elem(0, Value(ref1));
        ref2->set_elem(1, Value(1));
        ref3->raw().rs[0] = ref2;
        ref4->set_elem(0, Value(2));
        ref4->set_elem(1, Value(ref3));
        thread_context->regs().rv.raw().r = ref4;
        CPPUNIT_ASSERT_EQUAL(4UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref1) == _M_alloc->alloc_ops()[0]);
        CPPUNIT_ASSERT(make_alloc(ref2) == _M_alloc->alloc_ops()[1]);
        CPPUNIT_ASSERT(make_alloc(ref3) == _M_alloc->alloc_ops()[2]);
        CPPUNIT_ASSERT(make_alloc(ref4) == _M_alloc->alloc_ops()[3]);
        _M_gc->collect();
        thread_context->regs().rv.raw().r = Reference();
        CPPUNIT_ASSERT_EQUAL(4UL, _M_alloc->alloc_ops().size());
        _M_gc->collect();
        const vector<AllocatorOperation> &alloc_ops = _M_alloc->alloc_ops();
        CPPUNIT_ASSERT_EQUAL(8UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref1)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref2)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref3)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref4)) == 1);
        _M_thread_context_mutex->unlock();
        thread_context->system_thread().join();
      }

      void GarbageCollectorTests::test_gc_does_not_collect_objects_in_thread_contexts()
      {
        unique_ptr<VirtualMachineContext> vm_context(new_vm_context());
        unique_ptr<ThreadContext> thread_context1(new_thread_context(*vm_context));
        unique_ptr<ThreadContext> thread_context2(new_thread_context(*vm_context));
        unique_ptr<ThreadContext> thread_context3(new_thread_context(*vm_context));
        _M_gc->add_vm_context(vm_context.get());
        _M_gc->add_thread_context(thread_context1.get());
        _M_gc->add_thread_context(thread_context2.get());
        _M_gc->add_thread_context(thread_context3.get());
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "test1");
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref2->raw().is8), "test2");
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref3->raw().is8), "test3");
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6, thread_context3.get()));
        strcpy(reinterpret_cast<char *>(ref4->raw().is8), "test4");
        Reference ref5(_M_gc->new_object(OBJECT_TYPE_RARRAY, 1));
        ref5->raw().rs[0] = ref2;
        Reference ref6(_M_gc->new_object(OBJECT_TYPE_TUPLE, 4));
        ref6->set_elem(0, Value(ref3));
        ref6->set_elem(1, Value(2));
        ref6->set_elem(2, Value(ref3));
        ref6->set_elem(3, Value(2.0));
        thread_context1->push_local_var(Value(ref2));
        thread_context1->push_arg(Value(ref5));
        thread_context2->push_local_var(Value(ref6));
        thread_context3->regs().rv.raw().r = ref4;
        CPPUNIT_ASSERT_EQUAL(6UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref1) == _M_alloc->alloc_ops()[0]);
        CPPUNIT_ASSERT(make_alloc(ref2) == _M_alloc->alloc_ops()[1]);
        CPPUNIT_ASSERT(make_alloc(ref3) == _M_alloc->alloc_ops()[2]);
        CPPUNIT_ASSERT(make_alloc(ref4) == _M_alloc->alloc_ops()[3]);
        CPPUNIT_ASSERT(make_alloc(ref5) == _M_alloc->alloc_ops()[4]);
        CPPUNIT_ASSERT(make_alloc(ref6) == _M_alloc->alloc_ops()[5]);
        _M_gc->collect();
        CPPUNIT_ASSERT_EQUAL(7UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_free(ref1) == _M_alloc->alloc_ops()[6]);
        _M_thread_context_mutex->unlock();
        thread_context1->system_thread().join();
        thread_context2->system_thread().join();
        thread_context3->system_thread().join();
      }
      
      void GarbageCollectorTests::test_gc_does_not_collect_objects_in_vm_contexts()
      {
        unique_ptr<impl::ImplEnvironment> impl_env1(new_impl_env());
        unique_ptr<ThreadContext> thread_context1(new_thread_context(*impl_env1));
        unique_ptr<impl::ImplEnvironment> impl_env2(new_impl_env());
        unique_ptr<ThreadContext> thread_context2(new_thread_context(*impl_env2));
        _M_gc->add_vm_context(impl_env1.get());
        _M_gc->add_thread_context(thread_context1.get());
        _M_gc->add_vm_context(impl_env2.get());
        _M_gc->add_thread_context(thread_context2.get());
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "test1");
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref2->raw().is8), "test2");
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref3->raw().is8), "test3");
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref4->raw().is8), "test4");
        Reference ref5(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref5->raw().is8), "test5");
        Reference ref6(_M_gc->new_object(OBJECT_TYPE_RARRAY, 2));
        ref6->raw().rs[0] = ref2;
        ref6->raw().rs[1] = ref3;
        Reference ref7(_M_gc->new_object(OBJECT_TYPE_TUPLE, 3));
        ref7->set_elem(0, Value(1));
        ref7->set_elem(1, Value(ref3));
        ref7->set_elem(2, Value(ref4));
        impl_env1->set_var_count(3);
        impl_env1->set_var(0, Value(ref2));
        impl_env1->set_var(1, Value(ref4));
        impl_env1->set_var(2, Value(ref6));
        impl_env2->set_var_count(2);
        impl_env2->set_var(0, Value(ref6));
        impl_env2->set_var(1, Value(ref7));
        CPPUNIT_ASSERT_EQUAL(7UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref1) == _M_alloc->alloc_ops()[0]);
        CPPUNIT_ASSERT(make_alloc(ref2) == _M_alloc->alloc_ops()[1]);
        CPPUNIT_ASSERT(make_alloc(ref3) == _M_alloc->alloc_ops()[2]);
        CPPUNIT_ASSERT(make_alloc(ref4) == _M_alloc->alloc_ops()[3]);
        CPPUNIT_ASSERT(make_alloc(ref5) == _M_alloc->alloc_ops()[4]);
        CPPUNIT_ASSERT(make_alloc(ref6) == _M_alloc->alloc_ops()[5]);
        CPPUNIT_ASSERT(make_alloc(ref7) == _M_alloc->alloc_ops()[6]);
        _M_gc->collect();
        const vector<AllocatorOperation> &alloc_ops = _M_alloc->alloc_ops();
        CPPUNIT_ASSERT_EQUAL(9UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref1)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref5)) == 1);
        _M_thread_context_mutex->unlock();
        thread_context1->system_thread().join();
        thread_context2->system_thread().join();
      }

      void GarbageCollectorTests::test_gc_destructor_free_all_objects()
      {
        unique_ptr<impl::ImplEnvironment> impl_env(new_impl_env());
        unique_ptr<ThreadContext> thread_context(new_thread_context(*impl_env));
        GarbageCollector *gc = new_gc(_M_alloc);
        gc->add_vm_context(impl_env.get());
        gc->add_thread_context(thread_context.get());
        Reference ref1(gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "test1");
        Reference ref2(gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref2->raw().is8), "test2");
        Reference ref3(gc->new_object(OBJECT_TYPE_IARRAY8, 6));
        strcpy(reinterpret_cast<char *>(ref3->raw().is8), "test3");
        Reference ref4(gc->new_object(OBJECT_TYPE_TUPLE, 3));
        ref4->set_elem(0, Value(1.0));
        ref4->set_elem(1, Value(ref1));
        ref4->set_elem(2, Value(2.0));
        impl_env->set_var_count(2);
        impl_env->set_var(0, Value(ref2));
        thread_context->push_local_var(Value(ref4));
        thread_context->push_local_var(Value(ref2));
        CPPUNIT_ASSERT_EQUAL(4UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(make_alloc(ref1) == _M_alloc->alloc_ops()[0]);
        CPPUNIT_ASSERT(make_alloc(ref2) == _M_alloc->alloc_ops()[1]);
        CPPUNIT_ASSERT(make_alloc(ref3) == _M_alloc->alloc_ops()[2]);
        CPPUNIT_ASSERT(make_alloc(ref4) == _M_alloc->alloc_ops()[3]);
        delete gc;
        const vector<AllocatorOperation> &alloc_ops = _M_alloc->alloc_ops();
        CPPUNIT_ASSERT_EQUAL(8UL, _M_alloc->alloc_ops().size());
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref1)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref2)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref3)) == 1);
        CPPUNIT_ASSERT(count(alloc_ops.begin(), alloc_ops.end(), make_free(ref4)) == 1);
        _M_thread_context_mutex->unlock();
        thread_context->system_thread().join();
      }

      DEF_IMPL_GC_TESTS(MarkSweepGarbageCollector);
    }
  }
}
