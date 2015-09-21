/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <letin/vm.hpp>
#include "priv.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    class TupleElementList
    {
      const TupleElementType *_M_tuple_elem_types;
      const TupleElement *_M_tuple_elems;
    public:
      TupleElementList(const TupleElementType *tuple_elem_types, const TupleElement *tuple_elems) :
        _M_tuple_elem_types(tuple_elem_types), _M_tuple_elems(tuple_elems) {}

      Value operator[](size_t i) const { return Value(_M_tuple_elem_types[i], _M_tuple_elems[i]); }
    };

    template<typename _T>
    static inline uint64_t next_dword(_T xs, size_t &i, size_t length)
    {
      uint64_t k = priv::hash(xs[i]);
      i++;
      return k;
    }

    static inline uint64_t next_dword(const int8_t *xs, size_t &i, size_t length)
    {
      uint64_t k = static_cast<uint64_t>(xs[i]);
      if(i + 1 < length) k |= static_cast<uint64_t>(xs[i + 1]) << 8;
      if(i + 2 < length) k |= static_cast<uint64_t>(xs[i + 2]) << 16;
      if(i + 3 < length) k |= static_cast<uint64_t>(xs[i + 3]) << 24;
      if(i + 4 < length) k |= static_cast<uint64_t>(xs[i + 4]) << 32;
      if(i + 5 < length) k |= static_cast<uint64_t>(xs[i + 5]) << 40;
      if(i + 6 < length) k |= static_cast<uint64_t>(xs[i + 6]) << 48;
      if(i + 7 < length) k |= static_cast<uint64_t>(xs[i + 7]) << 56;
      i += 8;
      return k;
    }

    static inline uint64_t next_dword(const int16_t *xs, size_t &i, size_t length)
    {
      uint64_t k = static_cast<uint64_t>(xs[i]);
      if(i + 1 < length) k |= static_cast<uint64_t>(xs[i + 1]) << 16;
      if(i + 2 < length) k |= static_cast<uint64_t>(xs[i + 2]) << 32;
      if(i + 3 < length) k |= static_cast<uint64_t>(xs[i + 3]) << 48;
      i += 4;
      return k;
    }

    static inline uint64_t next_dword(const int32_t *xs, size_t &i, size_t length)
    {
      uint64_t k = static_cast<uint64_t>(xs[i]);
      if(i + 1 < length) k |= static_cast<uint64_t>(xs[i + 1]) << 32;
      i += 2;
      return k;
    }

    static inline uint64_t next_dword(const float *xs, size_t &i, size_t length)
    {
      uint64_t k = priv::hash(xs[i]);
      if(i + 1 < length) k |= priv::hash(xs[i + 1]) << 32;
      i += 2;
      return k;
    }

    static inline uint64_t next_dword(const uint8_t *xs, size_t &i, size_t length)
    { return next_dword(reinterpret_cast<const int8_t *>(xs), i, length); }

    static inline uint64_t next_dword(const uint16_t *xs, size_t &i, size_t length)
    { return next_dword(reinterpret_cast<const int16_t *>(xs), i, length); }

    static inline uint64_t next_dword(const uint32_t *xs, size_t &i, size_t length)
    { return next_dword(reinterpret_cast<const int32_t *>(xs), i, length); }

    static inline uint64_t next_dword(const uint64_t *xs, size_t &i, size_t length)
    { return next_dword(reinterpret_cast<const int64_t *>(xs), i, length); }

    template<typename _T>
    static inline uint64_t murmur_hash64a(_T xs, size_t length, uint64_t seed = 0)
    {
      uint64_t m = 0xc6a4a7935bd1e995ULL;
      uint64_t h = seed ^ length;
      size_t i = 0;
      while(i < length) {
        uint64_t k = next_dword(xs, i, length);
        k *= m; k ^= k >> 47; k *= m;
        h *= m; h ^= k;
      }
      h ^= h >> 47; h *= m; h ^= h >> 47;
      return h;
    }

    uint64_t hash_value(const Value &key)
    {
      switch(key.type()) {
        case VALUE_TYPE_INT:
          return priv::hash(key.raw().i);
        case VALUE_TYPE_FLOAT:
          return priv::hash(key.raw().f);
        case VALUE_TYPE_REF:
          return priv::hash(key.raw().r);
        default:
          return 0;
      }
    }

    uint64_t hash_object(const Object &object)
    {
      switch(object.type()) {
        case OBJECT_TYPE_IARRAY8:
          return murmur_hash64a(object.raw().is8, object.length());
        case OBJECT_TYPE_IARRAY16:
          return murmur_hash64a(object.raw().is16, object.length());
        case OBJECT_TYPE_IARRAY32:
          return murmur_hash64a(object.raw().is32, object.length());
        case OBJECT_TYPE_IARRAY64:
          return murmur_hash64a(object.raw().is64, object.length());
        case OBJECT_TYPE_SFARRAY:
          return murmur_hash64a(object.raw().sfs, object.length());
        case OBJECT_TYPE_DFARRAY:
          return murmur_hash64a(object.raw().dfs, object.length());
        case OBJECT_TYPE_RARRAY:
          return murmur_hash64a(object.raw().rs, object.length());
        case OBJECT_TYPE_TUPLE:
          return murmur_hash64a(TupleElementList(object.raw().tuple_elem_types(), object.raw().tes), object.length());
        case OBJECT_TYPE_NATIVE_OBJECT:
          return object.raw().ntvo.hash_fun(reinterpret_cast<const void *>(object.raw().bs));
        default:
          return 0;
      }
    }

    uint64_t hash_bytes(const uint8_t *bytes, size_t length)
    { return murmur_hash64a(bytes, length); }

    uint64_t hash_hwords(const uint16_t *hwords, size_t length)
    { return murmur_hash64a(hwords, length); }

    uint64_t hash_words(const uint32_t *words, size_t length)
    { return murmur_hash64a(words, length); }

    uint64_t hash_dwords(const uint64_t *dwords, size_t length)
    { return murmur_hash64a(dwords, length); }

    namespace priv
    {
      uint64_t hash(const ArgumentList &key)
      { return murmur_hash64a(key, key.length()); }
    }
  }
}
