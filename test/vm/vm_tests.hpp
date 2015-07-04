/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
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
  VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc,                  \
                         NativeFunctionHandler *native_fun_handler);            \
}

#define DEF_IMPL_VM_TESTS(clazz)                                                \
CPPUNIT_TEST_SUITE_REGISTRATION(clazz##Tests);                                  \
VirtualMachine *clazz##Tests::new_vm(Loader *loader, GarbageCollector *gc,      \
                                     NativeFunctionHandler *native_fun_handler) \
{ return new impl::clazz(loader, gc, native_fun_handler); }                     \
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
        CPPUNIT_TEST(test_vm_executes_simple_program);
        CPPUNIT_TEST(test_vm_executes_lets_and_ins);
        CPPUNIT_TEST(test_vm_executes_int_instructions);
        CPPUNIT_TEST(test_vm_executes_float_instructions);
        CPPUNIT_TEST(test_vm_executes_reference_instructions);
        CPPUNIT_TEST(test_vm_gets_global_variables);
        CPPUNIT_TEST(test_vm_executes_jumps);
        CPPUNIT_TEST(test_vm_invokes_functions);
        CPPUNIT_TEST(test_vm_executes_recursion);
        CPPUNIT_TEST(test_vm_executes_tail_recursion);
        CPPUNIT_TEST(test_vm_executes_many_threads);
        CPPUNIT_TEST(test_vm_complains_on_non_existent_local_variable);
        CPPUNIT_TEST(test_vm_complains_on_non_existent_argument);
        CPPUNIT_TEST(test_vm_complains_on_division_by_zero);
        CPPUNIT_TEST(test_vm_complains_on_incorrect_number_of_arguments);
        CPPUNIT_TEST(test_vm_complains_on_non_existent_global_variable);
        CPPUNIT_TEST(test_vm_executes_load2_instructions);
        CPPUNIT_TEST(test_vm_executes_instructions_for_unique_objects);
        CPPUNIT_TEST(test_vm_executes_lettuples);
        CPPUNIT_TEST(test_vm_executes_lettuples_for_shared_tuples);
        CPPUNIT_TEST(test_vm_complains_on_many_references_to_unique_object);
        CPPUNIT_TEST(test_vm_complains_on_unique_object);
        CPPUNIT_TEST(test_vm_complains_on_incorrect_local_var_count);
        CPPUNIT_TEST(test_vm_loads_program_with_many_libraries);
        CPPUNIT_TEST(test_vm_complains_on_undefined_symbols);
        CPPUNIT_TEST(test_vm_complains_on_already_defined_symbols);
        CPPUNIT_TEST(test_vm_complains_on_relocation);
        CPPUNIT_TEST(test_vm_complains_on_unrelocable_program);
        CPPUNIT_TEST(test_vm_complains_on_arguments_of_req_instruction);
        CPPUNIT_TEST(test_vm_complains_on_arguments_of_rne_instruction);
        CPPUNIT_TEST(test_vm_complains_on_rutfillr_instruction_with_unique_object);
        CPPUNIT_TEST(test_vm_executes_arg_instructions_with_invocations);
        CPPUNIT_TEST(test_vm_executes_try);
        CPPUNIT_TEST(test_vm_executes_try_and_throw);
        CPPUNIT_TEST(test_vm_executes_two_tries_and_throw);
        CPPUNIT_TEST(test_vm_throws_and_catches_system_exception);
        CPPUNIT_TEST(test_vm_throws_user_exception);
        CPPUNIT_TEST(test_vm_throws_user_exception_from_catching_function);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        Loader *_M_loader;
        Allocator *_M_alloc;
        GarbageCollector *_M_gc;
        NativeFunctionHandler *_M_native_fun_handler;
        VirtualMachine *_M_vm;
      public:
        virtual VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler) = 0;
        
        void setUp();

        void tearDown();

        void test_vm_executes_simple_program();
        void test_vm_executes_lets_and_ins();
        void test_vm_executes_int_instructions();
        void test_vm_executes_float_instructions();
        void test_vm_executes_reference_instructions();
        void test_vm_gets_global_variables();
        void test_vm_executes_jumps();
        void test_vm_invokes_functions();
        void test_vm_executes_recursion();
        void test_vm_executes_tail_recursion();
        void test_vm_executes_many_threads();
        void test_vm_complains_on_non_existent_local_variable();
        void test_vm_complains_on_non_existent_argument();
        void test_vm_complains_on_division_by_zero();
        void test_vm_complains_on_incorrect_number_of_arguments();
        void test_vm_complains_on_non_existent_global_variable();
        void test_vm_executes_load2_instructions();
        void test_vm_executes_instructions_for_unique_objects();
        void test_vm_executes_lettuples();
        void test_vm_executes_lettuples_for_shared_tuples();
        void test_vm_complains_on_many_references_to_unique_object();
        void test_vm_complains_on_unique_object();
        void test_vm_complains_on_incorrect_local_var_count();
        void test_vm_loads_program_with_many_libraries();
        void test_vm_complains_on_undefined_symbols();
        void test_vm_complains_on_already_defined_symbols();
        void test_vm_complains_on_relocation();
        void test_vm_complains_on_unrelocable_program();
        void test_vm_complains_on_arguments_of_req_instruction();
        void test_vm_complains_on_arguments_of_rne_instruction();
        void test_vm_complains_on_rutfillr_instruction_with_unique_object();
        void test_vm_executes_arg_instructions_with_invocations();
        void test_vm_executes_try();
        void test_vm_executes_try_and_throw();
        void test_vm_executes_two_tries_and_throw();
        void test_vm_throws_and_catches_system_exception();
        void test_vm_throws_user_exception();
        void test_vm_throws_user_exception_from_catching_function();
      };

      DECL_IMPL_VM_TESTS(InterpreterVirtualMachine);
    }
  }
}

#endif
