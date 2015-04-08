/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <fstream>
#include <string>
#include "comp_tests.hpp"
#include "fs_util.hpp"
#include "impl_comp.hpp"
#include "helper.hpp"

using namespace std;
using namespace letin::comp;
using namespace letin::util;

#if defined(_WIN32) || defined(_WIN64)
#define TEST_FILE(dir_name)     TEST_DIR "\\" dir_name
#else
#define TEST_FILE(dir_name)     TEST_DIR "/" dir_name
#endif

namespace letin
{
  namespace comp
  {
    namespace test
    {
      void CompilerTests::setUp()
      {
        make_dir(TEST_DIR);
        get_current_dir(_M_saved_dir_name);
        change_dir(TEST_DIR);
        _M_comp = new_comp();
      }

      void CompilerTests::tearDown() {
        delete _M_comp;
        vector<string> paths;
        change_dir(_M_saved_dir_name.c_str());
        list_file_paths(TEST_DIR, paths);
        for(auto path : paths) remove_file(path.c_str());
        remove_dir(TEST_DIR);
      }

      void CompilerTests::test_compiler_compiles_simple_program()
      {
        istringstream iss("\n\
.entry f\n\
f(a1) = {\n\
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
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(4U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 4U, 48U, 48U + 16U);
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
f(a1) = {\n\
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
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(6U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 6U, 48U, 48U + 16U);
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
f(a1) = {\n\
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
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(6U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 6U, 48U, 48U + 16U);
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
f(a2) = {\n\
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
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(12U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(2U, 12U, 48U, 48U + 16U);
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
f(a3) = {\n\
        let iadd a0, a1\n\
        let imul a0, a2\n\
        in\n\
        ret idiv lv0, lv1\n\
}\n\
\n\
g(a4) = {\n\
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
h(a2) = {\n\
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
        ASSERT_PROG(static_cast<size_t>(48 + 40 + 232 + 40), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(3U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(19U);
        ASSERT_HEADER_DATA_SIZE(0U);
        // f
        ASSERT_FUN(3U, 4U, 48U, 48U + 40U);
        ASSERT_LET(IADD, A(0), A(1), 0);
        ASSERT_LET(IMUL, A(0), A(2), 1);
        ASSERT_IN(2);
        ASSERT_RET(IDIV, LV(0), LV(1), 3);
        END_ASSERT_FUN();
        // g
        ASSERT_FUN(4U, 11U, 48U + 12U, 48U + 40U);
        ASSERT_ARG(IADD, A(0), A(1), 0);
        ASSERT_ARG(ISUB, A(1), A(2), 1);
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
        ASSERT_FUN(2U, 4U, 48U + 24U, 48U + 40U);
        ASSERT_ARG(ILOAD, A(0), NA(), 0);
        ASSERT_ARG(ILOAD, A(1), NA(), 1);
        ASSERT_ARG(IMUL, A(0), A(1), 2);
        ASSERT_RET(ICALL, IMM(0), NA(), 3);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_compiles_many_global_variables()
      {
        istringstream iss("\n\
f(a0) = {\n\
        ret iload g1\n\
}\n\
\n\
g(a1) = {\n\
        arg fload g2\n\
        arg iload g3\n\
        ret rtuple()\n\
}\n\
\n\
g1 = 20\n\
\n\
g2 = 2.5\n\
\n\
g3 = 30\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 24 + 48 + 48 + 40), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(2U);
        ASSERT_HEADER_VAR_COUNT(3U);
        ASSERT_HEADER_CODE_SIZE(4U);
        ASSERT_HEADER_DATA_SIZE(0U);
        // f
        ASSERT_FUN(0U, 1U, 48U, 48U + 24U + 48U);
        ASSERT_RET(ILOAD, GV(0), NA(), 0);
        END_ASSERT_FUN();
        // g
        ASSERT_FUN(1U, 3U, 48U + 12U, 48U + 24U + 48U);
        ASSERT_ARG(FLOAD, GV(1), NA(), 0);
        ASSERT_ARG(ILOAD, GV(2), NA(), 1);
        ASSERT_RET(RTUPLE, NA(), NA(), 2);
        END_ASSERT_FUN();
        // g1
        ASSERT_VAR_I(20, 48U + 24U);
        // g2
        ASSERT_VAR_F(2.5, 48U + 24U + 16U);
        // g3
        ASSERT_VAR_I(30, 48U + 24U + 32U);
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_compiles_objects()
      {
        istringstream iss("\n\
g1 = [1, 2, 3, 5]\n\
\n\
g2 = (1, 4.5, 5)\n\
\n\
g3 = iarray32[19, 8, 7]\n\
\n\
g4 = tuple[\n\
        10,\n\
        [6, 7, 8],\n\
        1.5,\n\
        rarray[[1, 2], [4, 3]],\n\
        1.4\n\
]\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 0 + 64 + 0 + 256), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(0U);
        ASSERT_HEADER_VAR_COUNT(4U);
        ASSERT_HEADER_CODE_SIZE(0U);
        ASSERT_HEADER_DATA_SIZE(256U);
        // g1
        ASSERT_VAR_O(IARRAY64, 4, 48U, 48U + 64U);
        ASSERT_I(1, 0);
        ASSERT_I(2, 1);
        ASSERT_I(3, 2); 
        ASSERT_I(5, 3); 
        END_ASSERT_VAR_O();
        // g2
        ASSERT_VAR_O(TUPLE, 3, 48U + 16U, 48U + 64U);
        ASSERT_I(1, 0);
        ASSERT_F(4.5, 1);
        ASSERT_I(5, 2);
        END_ASSERT_VAR_O();
        // g3
        ASSERT_VAR_O(IARRAY32, 3, 48U + 32U, 48U + 64U);
        ASSERT_I(19, 0);
        ASSERT_I(8, 1);
        ASSERT_I(7, 2);
        END_ASSERT_VAR_O();
        // g4
        ASSERT_VAR_O(TUPLE, 5, 48U + 48U, 48U + 64U);
        ASSERT_I(10, 0);
        ASSERT_O(IARRAY64, 3, 1, 48U + 64U);
        ASSERT_I(6, 0);
        ASSERT_I(7, 1);
        ASSERT_I(8, 2);
        END_ASSERT_O();
        ASSERT_F(1.5, 2);
        ASSERT_O(RARRAY, 2, 3, 48U + 64U);
        ASSERT_O(IARRAY64, 2, 0, 48U + 64U);
        ASSERT_I(1, 0);
        ASSERT_I(2, 1);
        END_ASSERT_O();
        ASSERT_O(IARRAY64, 2, 1, 48U + 64U);
        ASSERT_I(4, 0);
        ASSERT_I(3, 1);
        END_ASSERT_O();
        END_ASSERT_O();
        ASSERT_F(1.4, 4);
        END_ASSERT_VAR_O();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_parses_program_with_comments()
      {
        istringstream iss("\n\
// line comment\n\
f(a1) = {\n\
/* some text\n\
 * c-style comment\n\
 * some text */\n\
        ret iload 1\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
      }

      void CompilerTests::test_compiler_compiles_characters_and_strings()
      {
        istringstream iss("\n\
f(a0) = {\n\
        ret iload 'a'\n\
}\n\
g1 = '\321'\n\
g2 = \"some text\n\43and something\"\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 32 + 16 + 32), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(2U);
        ASSERT_HEADER_CODE_SIZE(1U);
        ASSERT_HEADER_DATA_SIZE(32U);
        ASSERT_FUN(0U, 1U, 48U, 48U + 16U + 32U);
        ASSERT_RET(ILOAD, IMM('a'), NA(), 0);
        END_ASSERT_FUN();
        ASSERT_VAR_I('\321', 48U + 16U);
        ASSERT_VAR_O(IARRAY8, 24U, 48U + 16U + 16U, 48U + 16U + 32U + 16U);
        ASSERT_I('s', 0);
        ASSERT_I('o', 1);
        ASSERT_I('m', 2);
        ASSERT_I('e', 3);
        ASSERT_I(' ', 4);
        ASSERT_I('t', 5);
        ASSERT_I('e', 6);
        ASSERT_I('x', 7);
        ASSERT_I('t', 8);
        ASSERT_I('\n', 9);
        ASSERT_I('\43', 10);
        ASSERT_I('a', 11);
        ASSERT_I('n', 12);
        ASSERT_I('d', 13);
        ASSERT_I(' ', 14);
        ASSERT_I('s', 15);
        ASSERT_I('o', 16);
        ASSERT_I('m', 17);
        ASSERT_I('e', 18);
        ASSERT_I('t', 19);
        ASSERT_I('h', 20);
        ASSERT_I('i', 21); 
        ASSERT_I('n', 22);
        ASSERT_I('g', 23);
        END_ASSERT_VAR_O();
        END_ASSERT_PROG();
      }
      
      void CompilerTests::test_compiler_complains_on_syntax_error()
      {
        istringstream iss("f = {}");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(1 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(5 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("syntax error") == error_vector[0].msg());
      }

      void CompilerTests::test_compiler_complains_on_undefined_functions_for_unrelocatable()
      {
        istringstream iss("\n\
f(a1) = {\n\
        arg iload &g\n\
        arg iload &h\n\
        arg iload a0\n\
        ret riarray64()\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors, false));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("undefined function g") == error_vector[0].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(4 == error_vector[1].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[1].pos().column());
        CPPUNIT_ASSERT(string("undefined function h") == error_vector[1].msg());
      }

      void CompilerTests::test_compiler_complains_on_already_functions()
      {
        istringstream iss("\n\
f(a1) = {\n\
        ret iload 1\n\
}\n\
g(a1) = {\n\
        ret iload 2\n\
}\n\
h(a1) = {\n\
        ret iload 3\n\
}\n\
f(a1) = {\n\
        ret iload 4\n\
}\n\
h(a1) = {\n\
        ret iload 5\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(11 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(1 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("already defined function f") == error_vector[0].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(14 == error_vector[1].pos().line());
        CPPUNIT_ASSERT(1 == error_vector[1].pos().column());
        CPPUNIT_ASSERT(string("already defined function h") == error_vector[1].msg());
      }

      void CompilerTests::test_compiler_complains_on_undefined_variables_for_unrelocatable()
      {
        istringstream iss("\n\
f(a0) = {\n\
        arg iload g1\n\
        arg iload g2\n\
        arg iload g4\n\
        ret riarray64()\n\
}\n\
\n\
g5 = 1\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors, false));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g1") == error_vector[0].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(4 == error_vector[1].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[1].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g2") == error_vector[1].msg());
        CPPUNIT_ASSERT(5 == error_vector[2].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[2].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g4") == error_vector[2].msg());
      }

      void CompilerTests::test_compiler_complains_on_already_variables()
      {
        istringstream iss("\n\
g1 = 1\n\
g2 = 2\n\
g1 = 11\n\
g3 = 3\n\
g4 = 4\n\
g4 = 44\n\
g2 = 22\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(4 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(1 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("already defined variable g1") == error_vector[0].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(7 == error_vector[1].pos().line());
        CPPUNIT_ASSERT(1 == error_vector[1].pos().column());
        CPPUNIT_ASSERT(string("already defined variable g4") == error_vector[1].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(8 == error_vector[2].pos().line());
        CPPUNIT_ASSERT(1 == error_vector[2].pos().column());
        CPPUNIT_ASSERT(string("already defined variable g2") == error_vector[2].msg());
      }

      void CompilerTests::test_compiler_compiles_load2_instructions()
      {
istringstream iss("\n\
.entry f\n\
f(a0) = {\n\
        arg iload2(0x1122334455667788)\n\
        arg fload2(3.1415927)\n\
        ret rtuple()\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        format::Double f = double_to_format_double(3.1415927);
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 40), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(3U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(0U, 3U, 48U, 48U + 16U);
        ASSERT_ARG(ILOAD2, IMM(0x11223344), IMM(0x55667788), 0);
        ASSERT_ARG(FLOAD2, IMM(static_cast<int32_t>(f.dword >> 32)), IMM(static_cast<int32_t>(f.dword & 0xffffffff)), 1);
        ASSERT_RET(RTUPLE, NA(), NA(), 2);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_evaluates_defined_values()
      {
      }

      void CompilerTests::test_compiler_evaluates_instruction_argument_values()
      {
        istringstream iss("\n\
.define d1 1 + 2\n\
.define d2 d1 * 2\n\
\n\
f(a0) = {\n\
        let iload 1 ? 10  + 1 : 5 + 2\n\
        let iadd 5 + d1, d2 * 2\n\
        let iload((d2))\n\
        let iload d1 - 3 ? 10 * 2 : 5 * 3\n\
        in\n\
        ret iload lv1\n\
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
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(6U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(0U, 6U, 48U, 48U + 16U);
        ASSERT_LET(ILOAD, IMM(11), NA(), 0);
        ASSERT_LET(IADD, IMM(8), IMM(12), 1);
        ASSERT_LET(ILOAD, IMM(6), NA(), 2);
        ASSERT_LET(ILOAD, IMM(15), NA(), 3);
        ASSERT_IN(4);
        ASSERT_RET(ILOAD, LV(1), NA(), 5);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_evaluates_global_variable_values()
      {
        istringstream iss("\n\
.define d1 1.5 * 2\n\
.define d2 d1 * 3\n\
.define d3 10\n\
\n\
g1 = d1 + d2\n\
\n\
g2 = (10 + d3 * 4)\n\
\n\
g3 = [2 + d3, 3 * d3 + 4, 0x11 << 2]\n\
\n\
g4 = d3 == 10 ? 1 * 2 != 2 ? 1 : 2 : 3\n\
\n\
g5 = 0 ? 1 : 0 ? 2 : 3\n\
");        
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        ASSERT_PROG(static_cast<size_t>(48 + 0 + 80 + 32), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(0U);
        ASSERT_HEADER_VAR_COUNT(5U);
        ASSERT_HEADER_CODE_SIZE(0U);
        ASSERT_HEADER_DATA_SIZE(32U);
        ASSERT_VAR_F(12.0, 48U);
        ASSERT_VAR_I(50, 48U + 16U);
        ASSERT_VAR_O(IARRAY64, 3, 48U + 32U, 48U + 80U);
        ASSERT_I(12, 0);
        ASSERT_I(34, 1);
        ASSERT_I(0x44, 2);
        END_ASSERT_VAR_O();
        ASSERT_VAR_I(2, 48U + 48U);
        ASSERT_VAR_I(3, 48U + 64U);
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_complains_on_undefined_value()
      {
        istringstream iss("\n\
f(a0) = {\n\
        ret iload((d1))\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(20 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("undefined value d1") == error_vector[0].msg());
      }

      void CompilerTests::test_compiler_complains_on_already_defined_value()
      {
        istringstream iss("\n\
.define d1 1\n\
.define d2 2\n\
.define d3 3\n\
.define d2 4\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(5 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(9 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("already defined value d2") == error_vector[0].msg());
      }

      void CompilerTests::test_compiler_includes_file()
      {
        {
          ofstream ofs(TEST_FILE("inc1.letins"));
          ofs << "\n\
.define d1 1\n\
.define d2 2\n\
";
        }
        istringstream iss("\n\
.include \"inc1.letins\"\n\
\n\
.entry f\n\
\n\
f(a1) = {\n\
        let iadd a0, (d1)\n\
        in\n\
        ret iadd lv0, (d2)\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 40), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(3U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(1U, 3U, 48U, 48U + 16U);
        ASSERT_LET(IADD, A(0), IMM(1), 0);
        ASSERT_IN(1);
        ASSERT_RET(IADD, LV(0), IMM(2), 2);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }
      
      void CompilerTests::test_compiler_includes_many_files()
      {
        {
          ofstream ofs(TEST_FILE("inc1.letins"));
          ofs << "\n\
f(a0) = {\n\
          ret iload 1\n\
}\n\
\n\
.define d1 10\n\
";
        }
        {
          ofstream ofs(TEST_FILE("inc2.letins"));
          ofs << "\n\
.define d2 20\n\
.include \"inc3.letins\"\n\
.define d3 15\n\
";
        }
        {
          ofstream ofs(TEST_FILE("inc3.letins"));
          ofs << "\n\
g(a0) = {\n\
        ret iadd (d1), (d2)\n\
}\n\
";
        }
        {
          ofstream ofs(TEST_FILE("inc4.letins"));
          ofs << "\n\
.define d4 d3 * 2\n\
.define d5 40\n\
";
        }
        istringstream iss("\n\
.include \"inc1.letins\"\n\
.include \"inc2.letins\"\n\
.include \"inc4.letins\"\n\
\n\
.entry h\n\
\n\
h(a0) = {\n\
        let icall &g\n\
        let icall &f\n\
        in\n\
        let iadd lv0, d4 + 0\n\
        let iadd lv1, d5 + 0\n\
        in\n\
        ret iadd lv2, lv3\n\
}\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 40 + 112 + 24), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(2U);
        ASSERT_HEADER_FUN_COUNT(3U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(9U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(0U, 1U, 48U, 48U + 40U);
        ASSERT_RET(ILOAD, IMM(1), NA(), 0);
        END_ASSERT_FUN();
        ASSERT_FUN(0U, 1U, 48U + 12U, 48U + 40U);
        ASSERT_RET(IADD, IMM(10), IMM(20), 0);
        END_ASSERT_FUN();
        ASSERT_FUN(0U, 7U, 48U + 24U, 48U + 40U);
        ASSERT_LET(ICALL, IMM(1), NA(), 0);
        ASSERT_LET(ICALL, IMM(0), NA(), 1);
        ASSERT_IN(2);
        ASSERT_LET(IADD, LV(0), IMM(30), 3);
        ASSERT_LET(IADD, LV(1), IMM(40), 4);
        ASSERT_IN(5);
        ASSERT_RET(IADD, LV(2), LV(3), 6);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_complains_on_non_existent_included_files()
      {
        {
          ofstream ofs(TEST_FILE("inc1.letins"));
          ofs << ".define d1 10";
        }
        {
          ofstream ofs(TEST_FILE("inc2.letins"));
          ofs << ".define d2 20";
        }
        {
          ofstream ofs(TEST_FILE("inc4.letins"));
          ofs << ".define d4 40";
        }
        istringstream iss("\n\
.include \"inc1.letins\"\n\
.include \"inc2.letins\"\n\
.include \"inc3.letins\"\n\
.include \"inc4.letins\"\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(4 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(10 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("couldn't include file inc3.letins") == error_vector[0].msg());
      }
      
      void CompilerTests::test_compiler_complains_on_errors_in_included_files_for_unrelocatable()
      {
        {
          ofstream ofs(TEST_FILE("inc1.letins"));
          ofs << "\n\
f(a1) = {\n\
          ret iadd a0, g1\n\
}\n\
";
        }
        {
          ofstream ofs(TEST_FILE("inc2.letins"));
          ofs << "\n\
g(a0) = {\n\
        ret iadd g2, g3\n\
}\n\
";
        }
        {
          ofstream ofs(TEST_FILE("inc3.letins"));
          ofs << ".define d4 40";
        }
        istringstream iss("\n\
.include \"inc1.letins\"\n\
\n\
h(a0) = {\n\
        ret iload g4\n\
}\n\
\n\
.include \"inc2.letins\"\n\
.include \"inc3.letins\"\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors, false));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), errors.size());
        CPPUNIT_ASSERT(string("inc1.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(24 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g1") == error_vector[0].msg());
        CPPUNIT_ASSERT(string("inc2.letins") == error_vector[1].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[1].pos().line());
        CPPUNIT_ASSERT(18 == error_vector[1].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g2") == error_vector[1].msg());
        CPPUNIT_ASSERT(string("inc2.letins") == error_vector[2].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[2].pos().line());
        CPPUNIT_ASSERT(22 == error_vector[2].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g3") == error_vector[2].msg());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[3].pos().source().file_name());
        CPPUNIT_ASSERT(5 == error_vector[3].pos().line());
        CPPUNIT_ASSERT(19 == error_vector[3].pos().column());
        CPPUNIT_ASSERT(string("undefined variable g4") == error_vector[3].msg());
      }

      void CompilerTests::test_compiler_compiles_lettuples()
      {
        istringstream iss("\n\
f(a5) = {\n\
        arg iload a0\n\
        arg iload a1\n\
        lettuple(lv2) rtuple()\n\
        arg iadd a0, a1\n\
        arg iadd a1, a2\n\
        arg iadd a2, a3\n\
        arg iadd a3, a4\n\
        lettuple(lv4) rtuple()\n\
        lettuple(lv5) rutfilli 5, -1\n\
        lettuple(lv3) rutfilli 3, 1\n\
        in\n\
        ret iload lv5\n\
}\n\
");        
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        ASSERT_PROG(static_cast<size_t>(48 + 16 + 144), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_LIBRARY | format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(1U);
        ASSERT_HEADER_VAR_COUNT(0U);
        ASSERT_HEADER_CODE_SIZE(12U);
        ASSERT_HEADER_DATA_SIZE(0U);
        ASSERT_FUN(5U, 12U, 48U, 48U + 16U);
        ASSERT_ARG(ILOAD, A(0), NA(), 0);
        ASSERT_ARG(ILOAD, A(1), NA(), 1);
        ASSERT_LETTUPLE(2, RTUPLE, NA(), NA(), 2);
        ASSERT_ARG(IADD, A(0), A(1), 3);
        ASSERT_ARG(IADD, A(1), A(2), 4);
        ASSERT_ARG(IADD, A(2), A(3), 5);
        ASSERT_ARG(IADD, A(3), A(4), 6);
        ASSERT_LETTUPLE(4, RTUPLE, NA(), NA(), 7);
        ASSERT_LETTUPLE(5, RUTFILLI, IMM(5), IMM(-1), 8);
        ASSERT_LETTUPLE(3, RUTFILLI, IMM(3), IMM(1), 9);
        ASSERT_IN(10);
        ASSERT_RET(ILOAD, LV(5), NA(), 11);
        END_ASSERT_FUN();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_complains_on_no_number_of_local_variables()
      {
        istringstream iss("\n\
f(a0) = {\n\
        arg iload 1\n\
        arg iload 2\n\
        lettuple rtuple()\n\
        in\n\
        ret iload lv0\n\
}\n\
");        
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(5 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(9 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("no number of local variables") == error_vector[0].msg());
      }

      void CompilerTests::test_compiler_complains_on_number_of_local_variables()
      {
        istringstream iss("\n\
f(a0) = {\n\
        ret(lv2) iload a0\n\
}\n\
");        
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr == prog.get());
        vector<Error> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT(string("test.letins") == error_vector[0].pos().source().file_name());
        CPPUNIT_ASSERT(3 == error_vector[0].pos().line());
        CPPUNIT_ASSERT(9 == error_vector[0].pos().column());
        CPPUNIT_ASSERT(string("number of local variables") == error_vector[0].msg());
      }

      void CompilerTests::test_compiler_compiles_unrelocatable_program()
      {
        istringstream iss("\n\
.entry f\n\
f(a1) = {\n\
        let iload a\n\
        let iload &g\n\
        let iadd a0, a0\n\
        in\n\
        let iadd lv0, lv1\n\
        in\n\
        ret iadd lv3, lv2\n\
}\n\
\n\
g(a0) = {\n\
        ret rload b\n\
}\n\
\n\
a = 1\n\
\n\
b = [&f, &g]\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors, false));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 24 + 32 + 96 + 24), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(0U);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(2U);
        ASSERT_HEADER_VAR_COUNT(2U);
        ASSERT_HEADER_CODE_SIZE(8U);
        ASSERT_HEADER_DATA_SIZE(24U);
        ASSERT_HEADER_RELOC_COUNT(0U);
        ASSERT_HEADER_SYMBOL_COUNT(0U);
        ASSERT_FUN(1U, 7U, 48U, 48U + 24U + 32U);
        ASSERT_LET(ILOAD, GV(0), NA(), 0);
        ASSERT_LET(ILOAD, IMM(1), NA(), 1);
        ASSERT_LET(IADD, A(0), A(0), 2);
        ASSERT_IN(3);
        ASSERT_LET(IADD, LV(0), LV(1), 4);
        ASSERT_IN(5);
        ASSERT_RET(IADD, LV(3), LV(2), 6);
        END_ASSERT_FUN();
        ASSERT_FUN(0U, 1U, 48U + 12U, 48U + 24U + 32U);
        ASSERT_RET(RLOAD, GV(1), NA(), 0);
        END_ASSERT_FUN();
        ASSERT_VAR_I(1, 48U + 24U);
        ASSERT_VAR_O(IARRAY64, 2U, 48U + 24U + 16U, 48U + 24U + 32U + 96U);
        ASSERT_I(0, 0);
        ASSERT_I(1, 1);
        END_ASSERT_VAR_O();
        END_ASSERT_PROG();
      }

      void CompilerTests::test_compiler_compiles_relocatable_program()
      {
        istringstream iss("\n\
.entry f\n\
.public\n\
f(a0) = {\n\
        let iadd &f, b\n\
        let iadd a, &g\n\
        in\n\
        ret iadd lv0, lv1\n\
}\n\
\n\
.public\n\
g(a0) = {\n\
        let iadd &i, f\n\
        let iadd g, &h\n\
        in\n\
        ret iadd lv0, lv1\n\
}\n\
\n\
.public a = 1\n\
\n\
.public b = 2\n\
\n\
c = iarray32[&f, &g, &h, &i]\n\
\n\
d = [&f, &g, &h, &i]\n\
\n\
e = (&f, &g, &h, &i)\n\
");
        vector<Source> sources;
        sources.push_back(Source("test.letins", iss));
        list<Error> errors;
        unique_ptr<Program> prog(_M_comp->compile(sources, errors));
        CPPUNIT_ASSERT(nullptr != prog.get());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), errors.size());
        ASSERT_PROG(static_cast<size_t>(48 + 24 + 80 + 96 + 112 + 240 + 64), (*(prog.get())));
        ASSERT_HEADER_MAGIC();
        ASSERT_HEADER_FLAGS(format::HEADER_FLAG_RELOCATABLE);
        ASSERT_HEADER_ENTRY(0U);
        ASSERT_HEADER_FUN_COUNT(2U);
        ASSERT_HEADER_VAR_COUNT(5U);
        ASSERT_HEADER_CODE_SIZE(8U);
        ASSERT_HEADER_DATA_SIZE(112U);
        ASSERT_HEADER_RELOC_COUNT(20U);
        ASSERT_HEADER_SYMBOL_COUNT(8U);
        ASSERT_FUN(0U, 4U, 48U, 48U + 24U + 80U);
        ASSERT_LET(IADD, IMM(0), GV(1), 0);
        ASSERT_LET(IADD, GV(0), IMM(1), 1);
        ASSERT_IN(2);
        ASSERT_RET(IADD, LV(0), LV(1), 3);
        ASSERT_RELOC_A1F(0U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_A2V(0U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_A1V(1U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_A2F(1U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        END_ASSERT_FUN();
        ASSERT_FUN(0U, 4U, 48U + 12U, 48U + 24U + 80U);
        ASSERT_LET(IADD, IMM(0), GV(0), 0);
        ASSERT_LET(IADD, GV(0), IMM(0), 1);
        ASSERT_IN(2);
        ASSERT_RET(IADD, LV(0), LV(1), 3);
        ASSERT_RELOC_SA1F(0U, "i", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SA2V(0U, "f", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SA1V(1U, "g", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SA2F(1U, "h", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        END_ASSERT_FUN();
        ASSERT_VAR_I(1, 48U + 24U);
        ASSERT_VAR_I(2, 48U + 24U + 16U);
        ASSERT_VAR_O(IARRAY32, 4U, 48U + 24U + 32U, 48U + 24U + 80U + 96U);
        ASSERT_I(0, 0);
        ASSERT_I(1, 1);
        ASSERT_I(0, 2);
        ASSERT_I(0, 3);
        ASSERT_RELOC_EF(8U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_EF(12U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(16U, "h", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(20U, "i", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        END_ASSERT_VAR_O();
        ASSERT_VAR_O(IARRAY64, 4U, 48U + 24U + 48U, 48U + 24U + 80U + 96U);
        ASSERT_I(0, 0);
        ASSERT_I(1, 1);
        ASSERT_I(0, 2);
        ASSERT_I(0, 3);
        ASSERT_RELOC_EF(8U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_EF(16U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(24U, "h", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(32U, "i", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        END_ASSERT_VAR_O();
        ASSERT_VAR_O(TUPLE, 4U, 48U + 24U + 64U, 48U + 24U + 80U + 96U);
        ASSERT_I(0, 0);
        ASSERT_I(1, 1);
        ASSERT_I(0, 2);
        ASSERT_I(0, 3);
        ASSERT_RELOC_EF(8U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_EF(16U, 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(24U, "h", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_RELOC_SEF(32U, "i", 48U + 24U + 80U + 96U + 112U, 48U + 24U + 80U + 96U + 112U + 240U);
        END_ASSERT_VAR_O();
        ASSERT_SYMBOL_DF("f", 0, 48U + 24U + 80U + 96U + 112U + 240U);
        ASSERT_SYMBOL_DF("g", 1, 48U + 24U + 80U + 96U + 112U + 240U + 8U);
        ASSERT_SYMBOL_DV("a", 0, 48U + 24U + 80U + 96U + 112U + 240U + 16U);
        ASSERT_SYMBOL_DV("b", 1, 48U + 24U + 80U + 96U + 112U + 240U + 24U);
        END_ASSERT_PROG();
      }

      DEF_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}
