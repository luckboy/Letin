/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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
        CPPUNIT_TEST(test_comp_compiles_simple_program);
        CPPUNIT_TEST_SUITE_END_ABSTRACT();

        Compiler *_M_comp;
      public:
        virtual Compiler *new_comp() = 0;
        
        void setUp();

        void tearDown();
        
        void test_comp_compiles_simple_program();
      };

      DECL_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}

#endif
