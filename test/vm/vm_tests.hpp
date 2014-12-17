/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _VM_TESTS_HPP
#define _VM_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include <letin/vm.hpp>
#include "impl_env.hpp"
#include "vm.hpp"

#define DECL_IMPL_VM_TESTS(clazz)                                               \
class clazz##Tests : public VirtualMachineTests                                 \
{                                                                               \
  CPPUNIT_TEST_SUB_SUITE(clazz##Tests, VirtualMachineTests);                    \
  CPPUNIT_TEST_SUITE_END();                                                     \
public:                                                                         \
  VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc);                 \
}

#define DEF_IMPL_VM_TESTS(clazz)                                                \
CPPUNIT_TEST_SUITE_REGISTRATION(clazz##Tests);                                  \
VirtualMachine *clazz##Tests::new_vm(Loader *loader, GarbageCollector *gc)      \
{ return new impl::clazz(loader, gc); }                                         \
class clazz##Tests

namespace letin
{
  namespace vm
  {
    namespace test
    {
      class VirtualMachineTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(VirtualMachineTests);
        CPPUNIT_TEST(test_vm_execute_simple_program);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        Loader *_M_loader;
        Allocator *_M_alloc;
        GarbageCollector *_M_gc;
        VirtualMachine *_M_vm;
      public:
        virtual VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc) = 0;
        
        void setUp();

        void tearDown();
        
        void test_vm_execute_simple_program();
      };

      DECL_IMPL_VM_TESTS(InterpreterVirtualMachine);
    }
  }
}

#endif
