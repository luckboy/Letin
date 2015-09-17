/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _HASH_TABLE_HPP
#define _HASH_TABLE_HPP

#include <exception>
#include <mutex>
#include <letin/const.hpp>
#include <letin/vm.hpp>
#include "priv.hpp"
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      template<typename _K>
      struct Hash
      { std::uint64_t operator()(const _K &key) const { return hash(key); } };

      template<typename _K>
      class HashTableKeyBox
      {
        _K _M_key;
      public:
        HashTableKeyBox() {}

        bool operator==(const _K &key) const { return _M_key == key; }

        bool set_key(const _K &key, ThreadContext &context)
        { safely_assign_for_gc(_M_key, key); return true; }
      };

      template<typename _V>
      class HashTableValueBox
      {
        _V _M_value;
      public:
        HashTableValueBox() {}

        bool operator==(const _V &value) const { return _M_value == value; }

        const _V &value() const { return _M_value; }

        _V &value() { return _M_value; }

        bool set_value(const _V &value, ThreadContext &context)
        { safely_assign_for_gc(_M_value, value); return true; }
      };

      struct HashTableEntryRawBase
      {
        Reference prev_r;
        Reference next_r;
      };
      
      template<typename _K, typename _V>
      struct HashTableEntryRaw : public HashTableEntryRawBase
      {
        HashTableKeyBox<_K> key;
        HashTableValueBox<_V> value;
      };

      template<typename _K, typename _V>
      struct HashTableEntryObjectType
      { static constexpr int value() { return OBJECT_TYPE_HASH_TABLE_ENTRY; } };

      struct HashTableBucket
      {
        Reference first_entry_r;
        Reference last_entry_r;
      };

      struct HashTableRaw
      {
        std::size_t bucket_count;
        HashTableBucket buckets[1];
      };

      template<typename _K, typename _V, typename _Hash = Hash<_K>, typename _Equal = Equal<HashTableKeyBox<_K>, _K>>
      class HashTable
      {
        union Union
        {
          HashTableRaw *raw;
          std::uint8_t *bs;
        };

        union EntryUnion
        {
          HashTableEntryRaw<_K, _V> *raw;
          std::uint8_t *bs;          
        };

        std::mutex _M_mutex;
        Reference _M_r;
        _Hash _M_hash;
        _Equal _M_equal;
      public:
        HashTable() {}

        HashTable(ThreadContext &context, std::size_t bucket_count = 1024)
        { set_bucket_count(bucket_count, context); }
      private:
        const HashTableRaw &raw(Reference r) const
        { 
          Union u;
          u.bs = r->raw().bs;
          return *(u.raw);
        }

        HashTableRaw &raw(Reference r)
        {
          Union u;
          u.bs = r->raw().bs;
          return *(u.raw);
        }

        const HashTableRaw &raw() const { return raw(_M_r); }

        HashTableRaw &raw() { return raw(_M_r); }

        const HashTableEntryRaw<_K, _V> &entry_raw(Reference entry_r) const
        {
          EntryUnion eu;
          eu.bs = entry_r->raw().bs;
          return *(eu.raw);
        }

        HashTableEntryRaw<_K, _V> &entry_raw(Reference entry_r)
        {
          EntryUnion eu;
          eu.bs = entry_r->raw().bs;
          return *(eu.raw);
        }

        Reference find_entry_and_set_bucket_index(const _K &key, std::size_t &i) const
        {
          i = static_cast<std::size_t>(_M_hash(key) % raw().bucket_count);
          Reference entry_r = raw().buckets[i].first_entry_r;
          while(!entry_r.has_nil())
          {
            if(_M_equal(entry_raw(entry_r).key, key)) break;
            entry_r = entry_raw(entry_r).next_r;
          }
          return entry_r;
        }
      public:
        _V operator[](const _K &key)
        {
          _V value;
          if(get(key, value)) return value; else return _V();
        }

        bool get(const _K &key, _V &value)
        {
          std::lock_guard<std::mutex> guard(_M_mutex);
          if(_M_r.has_nil()) return false;
          std::size_t i;
          Reference entry_r = find_entry_and_set_bucket_index(key, i);
          if(entry_r.has_nil()) return false;
          value = entry_raw(entry_r).value.value();
          return true;
        }

        bool add(const _K &key, const _V &value, ThreadContext &context)
        {
          std::lock_guard<std::mutex> mutex_guard(_M_mutex);
          if(_M_r.has_nil()) return true;
          std::size_t i;
          Reference entry_r = find_entry_and_set_bucket_index(key, i);
          if(entry_r.has_nil()) {
            Reference new_entry_r(context.gc()->new_object(HashTableEntryObjectType<_K, _V>::value(), sizeof(HashTableEntryRaw<_K, _V>), &context));
            if(new_entry_r.is_null()) return false;
            entry_raw(new_entry_r).key = HashTableKeyBox<_K>();
            entry_raw(new_entry_r).value = HashTableValueBox<_V>();
            context.regs().tmp_r.safely_assign_for_gc(new_entry_r);
            if(!entry_raw(new_entry_r).key.set_key(key, context)) {
              context.regs().tmp_r.safely_assign_for_gc(Reference());
              context.safely_set_gc_tmp_ptr_for_gc(nullptr);
              return false;
            }
            if(!entry_raw(new_entry_r).value.set_value(value, context)) {
              context.regs().tmp_r.safely_assign_for_gc(Reference());
              context.safely_set_gc_tmp_ptr_for_gc(nullptr);
              return false;
            }
            Reference &first_entry_r = raw().buckets[i].last_entry_r;
            Reference &last_entry_r = raw().buckets[i].last_entry_r;
            {
              std::lock_guard<GarbageCollector> gc_guard(*(context.gc()));
              entry_raw(new_entry_r).prev_r = last_entry_r;
              entry_raw(new_entry_r).next_r = Reference();
              if(!last_entry_r.has_nil())
                entry_raw(last_entry_r).next_r = new_entry_r;
              else
                first_entry_r = new_entry_r;
              last_entry_r = new_entry_r;
            }
            context.regs().tmp_r.safely_assign_for_gc(Reference());
            context.safely_set_gc_tmp_ptr_for_gc(nullptr);
          } else
            safely_assign_for_gc(entry_raw(entry_r).value.value(), value);
          return true;
        }

        bool del(const _K &key, ThreadContext &context)
        {
          std::lock_guard<std::mutex> mutex_guard(_M_mutex);
          if(_M_r.has_nil()) return false;
          std::size_t i;
          Reference entry_r = find_entry_and_set_bucket_index(key, i);
          if(entry_r.has_nil()) return false;
          Reference &first_entry_r = raw().buckets[i].last_entry_r;
          Reference &last_entry_r = raw().buckets[i].last_entry_r;
          {
            std::lock_guard<GarbageCollector> gc_guard(*(context.gc()));
            if(first_entry_r == entry_r) first_entry_r = entry_raw(entry_r).next_r;
            if(last_entry_r == entry_r) last_entry_r = entry_raw(entry_r).prev_r;
            Reference prev_r = entry_raw(entry_r).prev_r;
            Reference next_r = entry_raw(entry_r).next_r;
            if(!prev_r.has_nil()) entry_raw(prev_r).next_r = entry_raw(entry_r).next_r;
            if(!next_r.has_nil()) entry_raw(next_r).prev_r = entry_raw(entry_r).prev_r;
            entry_raw(entry_r).prev_r = Reference();
            entry_raw(entry_r).next_r = Reference();
          }
          return true;
        }

        Reference ref() const { return _M_r; }

        std::size_t bucket_count() const
        { return !_M_r.has_nil() ? raw().bucket_count : 0; }

        bool set_bucket_count(std::size_t bucket_count, ThreadContext &context)
        {
          if(bucket_count != 0) {
            std::size_t raw_size = offsetof(HashTableRaw, buckets) + sizeof(Reference) * bucket_count;
            Reference r(context.gc()->new_object(OBJECT_TYPE_HASH_TABLE, raw_size, &context));
            if(r.is_null()) return false;
            raw(r).bucket_count = bucket_count;
            for(std::size_t i = 0; i < raw(r).bucket_count; i++) {
              raw(r).buckets[i].first_entry_r = Reference();
              raw(r).buckets[i].last_entry_r = Reference();
            }
            _M_r.safely_assign_for_gc(r);
            context.safely_set_gc_tmp_ptr_for_gc(nullptr);
          } else
            _M_r.safely_assign_for_gc(Reference());
          return true;
        }
      };

      template<>
      class HashTableKeyBox<ArgumentList>
      {
        Reference _M_key_r;
      public:
        HashTableKeyBox() {}

        bool operator==(const ArgumentList &key) const
        {
          if(_M_key_r->type() != OBJECT_TYPE_TUPLE) return false;
          if(_M_key_r->length() != key.length()) return false;
          for(size_t i = 0; i < key.length(); i++) {
            Value value(_M_key_r->raw().tuple_elem_types()[i], _M_key_r->raw().tes[i]);
            if(!equal_values(value, key[i])) return false;
          }
          return true;
        }

        Reference key_ref() const { return _M_key_r; }

        bool set_key(const ArgumentList &key, ThreadContext &context)
        {
          Reference r(context.gc()->new_object(OBJECT_TYPE_TUPLE, key.length(), &context));
          if(r.is_null()) return false;
          for(std::size_t i = 0; i < key.length(); i++) {
            r->raw().tes[i] = key[i].raw().i;
            r->raw().tuple_elem_types()[i].raw() = key[i].type();
          }
          _M_key_r.safely_assign_for_gc(r);
          context.safely_set_gc_tmp_ptr_for_gc(nullptr);
          return true;
        }
      };

      template<>
      struct HashTableEntryObjectType<ArgumentList, std::int64_t>
      { static constexpr int value() { return OBJECT_TYPE_ALI_HASH_TABLE_ENTRY; } };

      template<>
      struct HashTableEntryObjectType<ArgumentList, double>
      { static constexpr int value() { return OBJECT_TYPE_ALF_HASH_TABLE_ENTRY; } };

      template<>
      struct HashTableEntryObjectType<ArgumentList, Reference>
      { static constexpr int value() { return OBJECT_TYPE_ALR_HASH_TABLE_ENTRY; } };
    }
  }
}

#endif
