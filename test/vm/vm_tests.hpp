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

#define DECL_IMPL_VM_TESTS(prefix, clazz)                                       \
class prefix##clazz##Tests : public VirtualMachineTests                         \
{                                                                               \
  CPPUNIT_TEST_SUB_SUITE(prefix##clazz##Tests, VirtualMachineTests);            \
  CPPUNIT_TEST_SUITE_END();                                                     \
public:                                                                         \
  MemoizationCacheFactory *new_memo_cache_factory();                            \
  EvaluationStrategy *new_eval_strategy();                                      \
  VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc,                  \
                         NativeFunctionHandler *native_fun_handler,             \
                         EvaluationStrategy *eval_strategy);                    \
}

#define DEF_IMPL_VM_TESTS_FOR_EVAL_STRATEGY(prefix, clazz, eval_strategy_class) \
CPPUNIT_TEST_SUITE_REGISTRATION(prefix##clazz##Tests);                          \
MemoizationCacheFactory *prefix##clazz##Tests::new_memo_cache_factory()         \
{ return nullptr; }                                                             \
EvaluationStrategy *prefix##clazz##Tests::new_eval_strategy()                   \
{ return new impl::eval_strategy_class(); }                                     \
VirtualMachine *prefix##clazz##Tests::new_vm(Loader *loader,                    \
                                             GarbageCollector *gc,              \
                                             NativeFunctionHandler *native_fun_handler, \
                                             EvaluationStrategy *eval_strategy) \
{ return new impl::clazz(loader, gc, native_fun_handler, eval_strategy); }      \
class prefix##clazz##Tests

#define DEF_IMPL_VM_TESTS(prefix, clazz)                                        \
DEF_IMPL_VM_TESTS_FOR_EVAL_STRATEGY(prefix, clazz, prefix##EvaluationStrategy)

#define DEF_IMPL_VM_TESTS_FOR_EVAL_STRATEGY_WITH_MEMO_CACHE(prefix, clazz, eval_strategy_class, memo_cache_factory_class, memo_cache_arg) \
CPPUNIT_TEST_SUITE_REGISTRATION(prefix##clazz##Tests);                          \
MemoizationCacheFactory *prefix##clazz##Tests::new_memo_cache_factory()         \
{ return new impl::memo_cache_factory_class(memo_cache_arg); }                  \
EvaluationStrategy *prefix##clazz##Tests::new_eval_strategy()                   \
{ return new impl::eval_strategy_class(_M_memo_cache_factory); }                \
VirtualMachine *prefix##clazz##Tests::new_vm(Loader *loader,                    \
                                             GarbageCollector *gc,              \
                                             NativeFunctionHandler *native_fun_handler, \
                                             EvaluationStrategy *eval_strategy) \
{ return new impl::clazz(loader, gc, native_fun_handler, eval_strategy); }      \
class prefix##clazz##Tests

#define DECL_IMPL_VM_TESTS_WITH_MEMO_CACHE(prefix1, prefix2, clazz)             \
DECL_IMPL_VM_TESTS(prefix1##prefix2, clazz)

#define DEF_IMPL_VM_TESTS_WITH_MEMO_CACHE(prefix1, prefix2, clazz, arg)         \
DEF_IMPL_VM_TESTS_FOR_EVAL_STRATEGY_WITH_MEMO_CACHE(prefix1##prefix2, clazz, prefix2##EvaluationStrategy, prefix1##MemoizationCacheFactory, arg)

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
        CPPUNIT_TEST(test_vm_evaluates_local_variable_values_for_lazy_evaluation);
        CPPUNIT_TEST(test_vm_evaluates_functions_which_returns_passed_arguments_for_lazy_evaluation);
        CPPUNIT_TEST(test_vm_complains_on_many_references_to_unique_object_for_lazy_evaluation);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        Loader *_M_loader;
        Allocator *_M_alloc;
        GarbageCollector *_M_gc;
        NativeFunctionHandler *_M_native_fun_handler;
      protected:
        MemoizationCacheFactory *_M_memo_cache_factory;
      private:
        EvaluationStrategy *_M_eval_strategy;
        VirtualMachine *_M_vm;
      public:
        virtual MemoizationCacheFactory *new_memo_cache_factory() = 0;
        
        virtual EvaluationStrategy *new_eval_strategy() = 0;

        virtual VirtualMachine *new_vm(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy) = 0;
        
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
        void test_vm_evaluates_local_variable_values_for_lazy_evaluation();
        void test_vm_evaluates_functions_which_returns_passed_arguments_for_lazy_evaluation();
        void test_vm_complains_on_many_references_to_unique_object_for_lazy_evaluation();
      };

      DECL_IMPL_VM_TESTS(Eager, InterpreterVirtualMachine);

      DECL_IMPL_VM_TESTS(Lazy, InterpreterVirtualMachine);

      DECL_IMPL_VM_TESTS_WITH_MEMO_CACHE(HashTable, Memoization, InterpreterVirtualMachine);
    }
  }
}

#endif
