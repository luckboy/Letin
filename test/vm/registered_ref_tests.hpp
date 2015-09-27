/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _REGISTERED_REF_TESTS_HPP
#define _REGISTERED_REF_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace test
    {
      class RegisteredReferenceTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(RegisteredReferenceTests);
        CPPUNIT_TEST(test_registered_ref_constructor_registers);
        CPPUNIT_TEST(test_registered_ref_constructor_does_not_register);
        CPPUNIT_TEST(test_registered_ref_register_ref_method_registers);
        CPPUNIT_TEST(test_registered_ref_destructor_unregisters);
        CPPUNIT_TEST_SUITE_END();

        Allocator *_M_alloc;
        GarbageCollector *_M_gc;
        VirtualMachineContext *_M_vm_context;
        std::mutex *_M_thread_context_mutex;
        ThreadContext *_M_thread_context;
      public:
        void setUp();

        void tearDown();

        void test_registered_ref_constructor_registers();
        void test_registered_ref_constructor_does_not_register();
        void test_registered_ref_register_ref_method_registers();
        void test_registered_ref_destructor_unregisters();
      };
    }
  }
}

#endif
