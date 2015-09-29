/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _NATIVE_HELPER_TESTS_HPP
#define _NATIVE_HELPER_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace test
    {
      class NativeHelperTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(NativeHelperTests);
        CPPUNIT_TEST(test_native_helpers_check_values);
        CPPUNIT_TEST(test_native_helpers_check_objects);
        CPPUNIT_TEST(test_native_helpers_check_tuple_objects);
        CPPUNIT_TEST(test_native_helpers_check_option_objects);
        CPPUNIT_TEST(test_native_helpers_check_either_objects);
        CPPUNIT_TEST(test_native_helpers_convert_values);
        CPPUNIT_TEST(test_native_helpers_convert_tuple_objects);
        CPPUNIT_TEST(test_native_helpers_convert_option_objects);
        CPPUNIT_TEST(test_native_helpers_convert_either_objects);
        CPPUNIT_TEST(test_native_helpers_return_values);
        CPPUNIT_TEST(test_native_helpers_return_tuple_objects);
        CPPUNIT_TEST(test_native_helpers_return_string_objects);
        CPPUNIT_TEST(test_native_helper_complains_on_incorrect_number_of_arguments);
        CPPUNIT_TEST(test_native_helper_complains_on_incorrect_value);
        CPPUNIT_TEST(test_native_helper_complains_on_incorrect_object);
        CPPUNIT_TEST(test_native_helper_complains_on_shared_object);
        CPPUNIT_TEST(test_native_helper_complains_on_unique_object);
        CPPUNIT_TEST_SUITE_END();

        Allocator *_M_alloc;
        Loader *_M_loader;
        GarbageCollector *_M_gc;
        EvaluationStrategy *_M_eval_strategy;
      public:
        void setUp();

        void tearDown();

        void test_native_helpers_check_values();
        void test_native_helpers_check_objects();
        void test_native_helpers_check_tuple_objects();
        void test_native_helpers_check_option_objects();
        void test_native_helpers_check_either_objects();
        void test_native_helpers_convert_values();
        void test_native_helpers_convert_tuple_objects();
        void test_native_helpers_convert_option_objects();
        void test_native_helpers_convert_either_objects();
        void test_native_helpers_return_values();
        void test_native_helpers_return_tuple_objects();
        void test_native_helpers_return_string_objects();
        void test_native_helper_complains_on_incorrect_number_of_arguments();
        void test_native_helper_complains_on_incorrect_value();
        void test_native_helper_complains_on_incorrect_object();
        void test_native_helper_complains_on_shared_object();
        void test_native_helper_complains_on_unique_object();
      };
    }
  }
}

#endif
