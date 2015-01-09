/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _COMPILER_TESTS_HPP
#define _COMPILER_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include <iostream>
#include <sstream>
#include <letin/comp.hpp>

#define DECL_IMPL_COMP_TESTS(clazz)                                             \
class clazz##Tests : public CompilerTests                                       \
{                                                                               \
  CPPUNIT_TEST_SUB_SUITE(clazz##Tests, CompilerTests);                          \
  CPPUNIT_TEST_SUITE_END();                                                     \
public:                                                                         \
  Compiler *new_comp();                                                         \
}

#define DEF_IMPL_COMP_TESTS(clazz)                                              \
CPPUNIT_TEST_SUITE_REGISTRATION(clazz##Tests);                                  \
Compiler *clazz##Tests::new_comp()                                              \
{ return new impl::clazz(); }                                                   \
class clazz##Tests

namespace letin
{
  namespace comp
  {
    namespace test
    {
      class CompilerTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(CompilerTests);
        CPPUNIT_TEST(test_compiler_compiles_simple_program);
        CPPUNIT_TEST(test_compiler_compiles_int_instructions);
        CPPUNIT_TEST(test_compiler_compiles_float_instructions);
        CPPUNIT_TEST(test_compiler_compiles_jumps);
        CPPUNIT_TEST(test_compiler_compiles_many_functions);
        CPPUNIT_TEST(test_compiler_compiles_many_global_variables);
        CPPUNIT_TEST(test_compiler_compiles_objects);
        CPPUNIT_TEST(test_compiler_parses_program_with_comments);
        CPPUNIT_TEST(test_compiler_compiles_characters_and_strings);
        CPPUNIT_TEST(test_compiler_complain_on_syntax_error);
        CPPUNIT_TEST(test_compiler_complain_on_undefined_functions);
        CPPUNIT_TEST(test_compiler_complain_on_already_functions);
        CPPUNIT_TEST(test_compiler_complain_on_undefined_variables);
        CPPUNIT_TEST(test_compiler_complain_on_already_variables);
        CPPUNIT_TEST(test_compiler_compiles_load2_instructions);
        CPPUNIT_TEST(test_compiler_evaluates_defined_values);
        CPPUNIT_TEST(test_compiler_evaluates_instruction_argument_values);
        CPPUNIT_TEST(test_compiler_evaluates_global_variable_values);
        CPPUNIT_TEST(test_compiler_complain_on_undefined_value);
        CPPUNIT_TEST(test_compiler_complain_on_already_defined_value);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        Compiler *_M_comp;
      public:
        virtual Compiler *new_comp() = 0;
        
        void setUp();

        void tearDown();
        
        void test_compiler_compiles_simple_program();
        void test_compiler_compiles_int_instructions();
        void test_compiler_compiles_float_instructions();
        void test_compiler_compiles_jumps();
        void test_compiler_compiles_many_functions();
        void test_compiler_compiles_many_global_variables();
        void test_compiler_compiles_objects();
        void test_compiler_parses_program_with_comments();
        void test_compiler_compiles_characters_and_strings();
        void test_compiler_complain_on_syntax_error();
        void test_compiler_complain_on_undefined_functions();
        void test_compiler_complain_on_already_functions();
        void test_compiler_complain_on_undefined_variables();
        void test_compiler_complain_on_already_variables();
        void test_compiler_complain_on_incorrect_number_of_arguments();
        void test_compiler_compiles_load2_instructions();
        void test_compiler_evaluates_defined_values();
        void test_compiler_evaluates_instruction_argument_values();
        void test_compiler_evaluates_global_variable_values();
        void test_compiler_complain_on_undefined_value();
        void test_compiler_complain_on_already_defined_value();
      };

      DECL_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}

#endif
