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

      void CompilerTests::test_comp_compiles_simple_program()
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
        sources.push_back(Source("test.ltns", iss));
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

      DEF_IMPL_COMP_TESTS(ImplCompiler);
    }
  }
}
