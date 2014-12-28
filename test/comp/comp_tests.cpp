/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "comp_tests.hpp"
#include "impl_comp.hpp"
#include "helper.hpp"

using namespace std;
using namespace letin::comp;

namespace letin
{
  namespace comp
  {
    namespace test
    {
      void CompilerTests::setUp() { _M_comp = new_comp(); }

      void CompilerTests::tearDown() { delete _M_comp; }

      void CompilerTests::test_compiler_compiles_simple_program()
      {
        istringstream iss("\n\
.entry f\n\
f(1) = {\n\
        let iload(2)\n\
        let iload(2)\n\
        in\n\
        ret iadd(lv0, lv1)\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 48), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(0U);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(4U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 48U, 48U + 16U);
        ASSERT_LET(ILOAD, IMM(2), NA(), 0);
        ASSERT_LET(ILOAD, IMM(2), NA(), 1);
        ASSERT_IN(2);
        ASSERT_RET(IADD, LV(0), LV(1), 3);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_compiles_int_instructions()
      {
        istringstream iss("\n\
.entry f\n\
f(1) = {\n\
        let iload(-2)\n\
        let iload(0x1234abcd)\n\
        in\n\
        \n\
        let iadd(07654321, -0xdef)\n\
        in\n\
        ret isub(-0127, lv2)\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 72), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(0U);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(6U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 48U, 48U + 16U);
        ASSERT_LET(ILOAD, IMM(-2), NA(), 0);
        ASSERT_LET(ILOAD, IMM(0x1234abcd), NA(), 1);
        ASSERT_IN(2);
        ASSERT_LET(IADD, IMM(07654321), IMM(-0xdef), 3);
        ASSERT_IN(4);
        ASSERT_RET(ISUB, IMM(-0127), LV(2), 5);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_compiles_float_instructions()
      {
        istringstream iss("\n\
.entry f\n\
f(1) = {\n\
        let fload 0.123\n\
        let fload 123e10\n\
        in\n\
        let fsub lv1, -14.56e-4\n\
        in\n\
        ret fadd lv2, lv0\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 72), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(0U);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(6U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 48U, 48U + 16U);
        ASSERT_LET(FLOAD, IMM(0.123f), NA(), 0);
        ASSERT_LET(FLOAD, IMM(123e10f), NA(), 1);
        ASSERT_IN(2);
        ASSERT_LET(FSUB, LV(1), IMM(-14.56e-4f), 3);
        ASSERT_IN(4);
        ASSERT_RET(FADD, LV(2), LV(0), 5);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      DEF_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}
