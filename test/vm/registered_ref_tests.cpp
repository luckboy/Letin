/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "registered_ref_tests.hpp"
#include "impl_env.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::vm;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      CPPUNIT_TEST_SUITE_REGISTRATION(RegisteredReferenceTests);

      void RegisteredReferenceTests::setUp()
      {
        _M_alloc = new_allocator();
        _M_gc = new_garbage_collector(_M_alloc);
        _M_vm_context = new impl::ImplEnvironment();
        _M_thread_context_mutex = new mutex();
        _M_thread_context_mutex->lock();
        _M_thread_context = new ThreadContext(*_M_vm_context);
        _M_thread_context->set_gc(_M_gc);
        _M_thread_context->start([this] {
          _M_thread_context_mutex->lock();
          _M_thread_context_mutex->unlock();
        });
      }

      void RegisteredReferenceTests::tearDown()
      {
        _M_thread_context_mutex->unlock();
        _M_thread_context->system_thread().join();
        delete _M_thread_context;
        delete _M_thread_context_mutex;
        delete _M_vm_context;
        delete _M_gc;
        delete _M_alloc;
      }

      void RegisteredReferenceTests::test_registered_ref_constructor_registers()
      {
        RegisteredReference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1, _M_thread_context), _M_thread_context);
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->last_registered_ref());
        RegisteredReference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 2, _M_thread_context), _M_thread_context);
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->last_registered_ref());
        RegisteredReference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 3, _M_thread_context), _M_thread_context);
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->next_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->last_registered_ref());
        RegisteredReference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 4, _M_thread_context), _M_thread_context);
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref4, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->next_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref4, _M_thread_context->next_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->prev_registered_ref(ref4));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref4));
        CPPUNIT_ASSERT_EQUAL(&ref4, _M_thread_context->last_registered_ref());
      }

      void RegisteredReferenceTests::test_registered_ref_constructor_does_not_register()
      {
        RegisteredReference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 10, _M_thread_context), _M_thread_context, false);
        CPPUNIT_ASSERT(nullptr == _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT(nullptr == _M_thread_context->last_registered_ref());
      }

      void RegisteredReferenceTests::test_registered_ref_register_ref_method_registers()
      {
        RegisteredReference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1, _M_thread_context), _M_thread_context, false);
        RegisteredReference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 2, _M_thread_context), _M_thread_context, false);
        RegisteredReference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 3, _M_thread_context), _M_thread_context, false);
        CPPUNIT_ASSERT(nullptr == _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT(nullptr == _M_thread_context->last_registered_ref());
        ref1.register_ref();
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->last_registered_ref());
        ref2.register_ref();
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->last_registered_ref());
        ref3.register_ref();
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->prev_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->next_registered_ref(ref2));
        CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref3));
        CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->last_registered_ref());
      }

      void RegisteredReferenceTests::test_registered_ref_destructor_unregisters()
      {
        {
          RegisteredReference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1, _M_thread_context), _M_thread_context);
          {
            RegisteredReference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 2, _M_thread_context), _M_thread_context);
            {
              RegisteredReference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 3, _M_thread_context), _M_thread_context);
              {
                RegisteredReference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 4, _M_thread_context), _M_thread_context);
                ref4->set_elem(0, 'a');
              }
              CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
              CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->prev_registered_ref(ref1));
              CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
              CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
              CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->next_registered_ref(ref2));
              CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref3));
              CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref3));
              CPPUNIT_ASSERT_EQUAL(&ref3, _M_thread_context->last_registered_ref());
            }
            CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
            CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->prev_registered_ref(ref1));
            CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->next_registered_ref(ref1));
            CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->prev_registered_ref(ref2));
            CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref2));
            CPPUNIT_ASSERT_EQUAL(&ref2, _M_thread_context->last_registered_ref());
          }
          CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->first_registered_ref());
          CPPUNIT_ASSERT_EQUAL(&ref1,_M_thread_context->prev_registered_ref(ref1));
          CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->next_registered_ref(ref1));
          CPPUNIT_ASSERT_EQUAL(&ref1, _M_thread_context->last_registered_ref());
        }
        CPPUNIT_ASSERT(nullptr == _M_thread_context->first_registered_ref());
        CPPUNIT_ASSERT(nullptr == _M_thread_context->last_registered_ref());
      }
    }
  }
}
