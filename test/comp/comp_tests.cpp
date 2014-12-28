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
      
      void CompilerTests::test_compiler_compiles_jumps()
      {
         istringstream iss("\n\
f(2) = {\n\
        let ieq a0, 1\n\
        in\n\
        jc lv0, label1\n\
label3: ret iload 2\n\
label2:\n\
        let ieq lv1, 20\n\
        in\n\
        jc lv3, label3\n\
        ret iload lv1\n\
label1: let iadd a0, 10\n\
        let isub a1, 20\n\
        in\n\
        jump label2\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 144), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(12U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(2U, 48U, 48U + 16U);
        ASSERT_LET(IEQ, A(0), IMM(1), 0);
        ASSERT_IN(1);
        ASSERT_JC(LV(0), 5, 2);
        // label3:
        ASSERT_RET(ILOAD, IMM(2), NA(), 3);
        // label2:
        ASSERT_LET(IEQ, LV(1), IMM(20), 4);
        ASSERT_IN(5);
        ASSERT_JC(LV(3), -4, 6);
        ASSERT_RET(ILOAD, LV(1), NA(), 7);
        // label1:
        ASSERT_LET(IADD, A(0), IMM(10), 8);
        ASSERT_LET(ISUB, A(1), IMM(20), 9);
        ASSERT_IN(10);
        ASSERT_JUMP(-8, 11);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }
      
      void CompilerTests::test_compiler_compiles_many_functions()
      {
        istringstream iss("\n\
f(3) = {\n\
        let iadd a0, a1\n\
        let imul a0, a2\n\
        in\n\
        ret idiv lv0, lv1\n\
}\n\
\n\
g(4) = {\n\
        arg iadd a0, a1\n\
        arg isub a1, a2\n\
        arg ineg a2\n\
        let icall &f\n\
        arg iload a2\n\
        arg iload a3\n\
        let icall &h\n\
        in\n\
        arg iload lv0\n\
        arg iload lv1\n\
        ret rtuple()\n\
}\n\
\n\
h(2) = {\n\
        arg iload a0\n\
        arg iload a1\n\
        arg imul a0, a1\n\
        ret icall &f\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 40 + 232), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(3U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(19U);
        ASSERT_HEADER_DATA_SIZE(0U);
        // f
        ASSERT_FUN(3U, 48U, 48U + 40U);
        ASSERT_LET(IADD, A(0), A(1), 0);
        ASSERT_LET(IMUL, A(0), A(2), 1);
        ASSERT_IN(2);
        ASSERT_RET(IDIV, LV(0), LV(1), 3);
        END_ASSERT_FUN();
        // g
        ASSERT_FUN(4U, 48U + 12U, 48U + 40U);
        ASSERT_ARG(IADD, A(0), A(1), 0);
        ASSERT_ARG(IADD, A(1), A(2), 1);
        ASSERT_ARG(INEG, A(2), NA(), 2);
        ASSERT_LET(ICALL, IMM(0), NA(), 3);
        ASSERT_ARG(ILOAD, A(2), NA(), 4);
        ASSERT_ARG(ILOAD, A(3), NA(), 5);
        ASSERT_LET(ICALL, IMM(2), NA(), 6);
        ASSERT_IN(7);
        ASSERT_ARG(ILOAD, LV(0), NA(), 8);
        ASSERT_ARG(ILOAD, LV(1), NA(), 9);
        ASSERT_RET(RTUPLE, NA(), NA(), 10);
        END_ASSERT_FUN();
        // h
        ASSERT_FUN(2U, 48U + 24U, 48U + 40U);
        ASSERT_ARG(ILOAD, A(0), NA(), 0);
        ASSERT_ARG(ILOAD, A(1), NA(), 1);
        ASSERT_ARG(IMUL, A(0), A(1), 2);
        ASSERT_RET(ICALL, IMM(0), NA(), 3);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      DEF_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}
