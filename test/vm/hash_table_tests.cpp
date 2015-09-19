/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "hash_table_tests.hpp"
#include "impl_env.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::vm;
using namespace letin::vm::priv;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      void HashTableTests::setUp()
      {
        _M_alloc = new_allocator();
        _M_gc = new_garbage_collector(_M_alloc);
        _M_vm_context = new impl::ImplEnvironment();
        _M_thread_context_mutex = new mutex();
        _M_thread_context_mutex->lock();
        _M_thread_context = new ThreadContext(*_M_vm_context);
        _M_thread_context->set_gc(_M_gc);
        _M_thread_context->start([this] {
          _M_thread_context_mutex->lock();
          _M_thread_context_mutex->unlock();
        });
        _M_hash_table = new HashTable<Key, int, KeyHash, KeyEqual>();
      }

      void HashTableTests::tearDown()
      {
        _M_thread_context_mutex->unlock();
        _M_thread_context->system_thread().join();
        delete _M_thread_context;
        delete _M_thread_context_mutex;
        delete _M_vm_context;
        delete _M_gc;
        delete _M_alloc;
      }

      void HashTableTests::test_hash_table_add_method_adds_one_pair()
      {
        _M_hash_table->set_bucket_count(1000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(1), 2, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(1), value));
        CPPUNIT_ASSERT_EQUAL(2, value);
      }

      void HashTableTests::test_hash_table_add_method_adds_many_pairs()
      {
        _M_hash_table->set_bucket_count(1000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(10), 1, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(20), 2, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(30), 3, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(40), 4, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(10), value));
        CPPUNIT_ASSERT_EQUAL(1, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(20), value));
        CPPUNIT_ASSERT_EQUAL(2, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(30), value));
        CPPUNIT_ASSERT_EQUAL(3, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(40), value));
        CPPUNIT_ASSERT_EQUAL(4, value);
      }

      void HashTableTests::test_hash_table_add_method_adds_pairs_for_two_same_keys()
      {
        _M_hash_table->set_bucket_count(1000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(1), 11, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(2), 22, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(3), 33, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(1), value));
        CPPUNIT_ASSERT_EQUAL(11, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(2), value));
        CPPUNIT_ASSERT_EQUAL(22, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(3), value));
        CPPUNIT_ASSERT_EQUAL(33, value);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(2), 200, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(3), 300, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->get(Key(1), value));
        CPPUNIT_ASSERT_EQUAL(11, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(2), value));
        CPPUNIT_ASSERT_EQUAL(200, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(3), value));
        CPPUNIT_ASSERT_EQUAL(300, value);
      }

      void HashTableTests::test_hash_table_add_method_adds_pairs_for_two_keys_with_hash_conflict()
      {
        _M_hash_table->set_bucket_count(10000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(1), 15, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(2), 25, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(3), 35, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(1), value));
        CPPUNIT_ASSERT_EQUAL(15, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(2), value));
        CPPUNIT_ASSERT_EQUAL(25, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(3), value));
        CPPUNIT_ASSERT_EQUAL(35, value);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(10001), 45, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(20002), 55, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->get(Key(1), value));
        CPPUNIT_ASSERT_EQUAL(15, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(2), value));
        CPPUNIT_ASSERT_EQUAL(25, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(3), value));
        CPPUNIT_ASSERT_EQUAL(35, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(10001), value));
        CPPUNIT_ASSERT_EQUAL(45, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(20002), value));
        CPPUNIT_ASSERT_EQUAL(55, value);
      }

      void HashTableTests::test_hash_table_del_method_deletes_many_pairs()
      {
        _M_hash_table->set_bucket_count(1000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(5), 1, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(6), 2, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(7), 3, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(8), 4, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(5), value));
        CPPUNIT_ASSERT_EQUAL(1, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(6), value));
        CPPUNIT_ASSERT_EQUAL(2, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(7), value));
        CPPUNIT_ASSERT_EQUAL(3, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(8), value));
        CPPUNIT_ASSERT_EQUAL(4, value);
        CPPUNIT_ASSERT(_M_hash_table->del(Key(6), *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->del(Key(8), *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->get(Key(5), value));
        CPPUNIT_ASSERT_EQUAL(1, value);
        CPPUNIT_ASSERT(!_M_hash_table->get(Key(6), value));
        CPPUNIT_ASSERT(_M_hash_table->get(Key(7), value));
        CPPUNIT_ASSERT_EQUAL(3, value);
        CPPUNIT_ASSERT(!_M_hash_table->get(Key(8), value));
      }
      
      void HashTableTests::test_hash_table_del_method_does_not_delete_pairs_for_existent_keys()
      {
        _M_hash_table->set_bucket_count(10000, *_M_thread_context);
        CPPUNIT_ASSERT(_M_hash_table->add(Key(12), 1, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(23), 2, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(34), 3, *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->add(Key(45), 4, *_M_thread_context));
        int value;
        CPPUNIT_ASSERT(_M_hash_table->get(Key(12), value));
        CPPUNIT_ASSERT_EQUAL(1, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(23), value));
        CPPUNIT_ASSERT_EQUAL(2, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(34), value));
        CPPUNIT_ASSERT_EQUAL(3, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(45), value));
        CPPUNIT_ASSERT_EQUAL(4, value);
        CPPUNIT_ASSERT(!_M_hash_table->del(Key(10012), *_M_thread_context));
        CPPUNIT_ASSERT(!_M_hash_table->del(Key(20045), *_M_thread_context));
        CPPUNIT_ASSERT(_M_hash_table->get(Key(12), value));
        CPPUNIT_ASSERT_EQUAL(1, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(23), value));
        CPPUNIT_ASSERT_EQUAL(2, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(34), value));
        CPPUNIT_ASSERT_EQUAL(3, value);
        CPPUNIT_ASSERT(_M_hash_table->get(Key(45), value));
        CPPUNIT_ASSERT_EQUAL(4, value);
      }
    }
  }
}
