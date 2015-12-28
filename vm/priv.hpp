/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _PRIV_HPP
#define _PRIV_HPP

#include <cmath>
#include <letin/vm.hpp>
#include "util.hpp"

#ifdef _U
#define ___U                            _U
#undef _U
const int _U =                          ___U;
#undef ___U
#endif

namespace letin
{
  namespace vm
  {
    namespace priv
    {
      const int OBJECT_TYPE_HASH_TABLE = OBJECT_TYPE_INTERNAL + 0;
      const int OBJECT_TYPE_HASH_TABLE_ENTRY = OBJECT_TYPE_INTERNAL + 1;
      const int OBJECT_TYPE_ALI_HASH_TABLE_ENTRY = OBJECT_TYPE_INTERNAL + 2;
      const int OBJECT_TYPE_ALF_HASH_TABLE_ENTRY = OBJECT_TYPE_INTERNAL + 3;
      const int OBJECT_TYPE_ALR_HASH_TABLE_ENTRY = OBJECT_TYPE_INTERNAL + 4;

      template<typename _T, typename _U>
      void safely_assign_for_gc(_T &x, const _U &y);

      inline void safely_assign_for_gc(Reference &x, const Reference &y)
      { x.safely_assign_for_gc(y); }

      inline void safely_assign_for_gc(Value &x, const Value &y)
      { x.safely_assign_for_gc(y); }

      inline void safely_assign_for_gc(ReturnValue &x, const ReturnValue &y)
      { x.safely_assign_for_gc(y); }

      inline void safely_assign_for_gc(ReturnValue &x, const Value &y)
      { x.safely_assign_for_gc(y); }

      template<typename _T, typename _U>
      inline void safely_assign_for_gc(_T &x, const _U &y) { x = y; }

      template<typename _K>
      std::uint64_t hash(const _K &key);

      inline std::uint64_t hash(const int &key)
      { return static_cast<std::uint64_t>(key); }

      inline std::uint64_t hash(const std::int64_t &key)
      { return static_cast<std::uint64_t>(key); }

      inline std::uint64_t hash(const float &key)
      { return util::float_to_format_float(key).word; }

      inline std::uint64_t hash(const double &key)
      { return util::double_to_format_double(key).dword; }

      inline std::uint64_t hash(const Reference &key) { return key->hash(); }

      inline std::uint64_t hash(const Value &key) { return key.hash(); }

      inline std::uint64_t hash(const Object &key) { return key.hash(); }

      std::uint64_t hash(const ArgumentList &key);

      template<typename _T, typename _U>
      struct Equal
      { std::uint64_t operator()(const _T &x, const _U &y) const { return x == y; } };
    }
  }
}

#endif
