/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _HASH_TABLE_TESTS_HPP
#define _HASH_TABLE_TESTS_HPP

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestAssert.h>
#include "hash_table.hpp"

namespace letin
{
  namespace vm
  {
    namespace test
    {
      class Key
      {
        int _M_i;
      public:
        Key() {}

        Key(int i) : _M_i(i) {}

        bool operator==(const Key &key) const { return _M_i == key._M_i; }
        
        std::uint64_t hash() const { return static_cast<std::uint64_t>(_M_i % 10000); }
      };
      
      struct KeyHash
      { std::uint64_t operator()(const Key &key) const { return key.hash(); } };

      struct KeyEqual
      {
        bool operator()(const priv::HashTableKeyBox<Key> &boxed_key1, const Key &key2) const
        { return boxed_key1 == key2; }
      };

      class HashTableTests : public CppUnit::TestFixture
      {
        CPPUNIT_TEST_SUITE(HashTableTests);
        CPPUNIT_TEST(test_hash_table_add_method_adds_one_pair);
        CPPUNIT_TEST(test_hash_table_add_method_adds_many_pairs);
        CPPUNIT_TEST(test_hash_table_add_method_adds_pairs_for_two_same_keys);
        CPPUNIT_TEST(test_hash_table_add_method_adds_pairs_for_two_keys_with_hash_conflict);
        CPPUNIT_TEST(test_hash_table_del_method_deletes_many_pairs);
        CPPUNIT_TEST(test_hash_table_del_method_does_not_delete_pairs_for_existent_keys);
        CPPUNIT_TEST_SUITE_END();

        Allocator *_M_alloc;
        GarbageCollector *_M_gc;
        VirtualMachineContext *_M_vm_context;
        std::mutex *_M_thread_context_mutex;
        ThreadContext *_M_thread_context;
        priv::HashTable<Key, int, KeyHash, KeyEqual> *_M_hash_table;
      public:
        void setUp();

        void tearDown();

        void test_hash_table_add_method_adds_one_pair();
        void test_hash_table_add_method_adds_many_pairs();
        void test_hash_table_add_method_adds_pairs_for_two_same_keys();
        void test_hash_table_add_method_adds_pairs_for_two_keys_with_hash_conflict();
        void test_hash_table_del_method_deletes_many_pairs();
        void test_hash_table_del_method_does_not_delete_pairs_for_existent_keys();
      };

      CPPUNIT_TEST_SUITE_REGISTRATION(HashTableTests);
    }
  }
}

#endif
