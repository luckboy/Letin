/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _NUM_VALUE_HPP
#define _NUM_VALUE_HPP

#include <cstdint>

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class NumberValue
      {
      public:
        enum Type { TYPE_INT, TYPE_FLOAT };
      private:
        Type _M_type;
        union {
          std::int64_t _M_i;
          double _M_f;
        };
      public:
        NumberValue() : _M_type(TYPE_INT), _M_i(0) {}

        NumberValue(bool b) : _M_type(TYPE_INT), _M_i(b ? 1 : 0) {}

        NumberValue(int i) : _M_type(TYPE_INT), _M_i(i) {}

        NumberValue(std::int64_t i) : _M_type(TYPE_INT), _M_i(i) {}

        NumberValue(double f) : _M_type(TYPE_FLOAT), _M_f(f) {}

        virtual ~NumberValue();

        Type type() const { return _M_type; }
        
        std::int64_t i() const { return _M_type == TYPE_INT ? _M_i : 0; }

        double f() const { return _M_type == TYPE_FLOAT ? _M_f : 0.0; }

        std::int64_t to_int64() const { return _M_type == TYPE_INT ? _M_i : f(); }

        double to_double() const { return _M_type == TYPE_FLOAT ? _M_f : i(); }

        NumberValue operator+() const { return *this; }

        NumberValue operator-() const;

        NumberValue operator+(const NumberValue &value) const;

        NumberValue operator-(const NumberValue &value) const;

        NumberValue operator*(const NumberValue &value) const;

        NumberValue operator/(const NumberValue &value) const;

        NumberValue operator%(const NumberValue &value) const;

        NumberValue operator~() const;

        NumberValue operator&(const NumberValue &value) const;

        NumberValue operator|(const NumberValue &value) const;

        NumberValue operator^(const NumberValue &value) const;

        NumberValue operator<<(const NumberValue &value) const;

        NumberValue operator>>(const NumberValue &value) const;

        bool operator==(const NumberValue &value) const;

        bool operator!=(const NumberValue &value) const { return !(*this == value); }
        
        bool operator<(const NumberValue &value) const;

        bool operator>=(const NumberValue &value) const { return !(*this < value); }

        bool operator>(const NumberValue &value) const { return value < *this; }

        bool operator<=(const NumberValue &value) const { return !(*this > value); }
      };
    }
  }
}

#endif
