/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cmath>
#include "num_value.hpp"

using namespace std;

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      NumberValue::~NumberValue() {}

      NumberValue NumberValue::operator-() const
      {
        switch(_M_type) {
          case TYPE_INT:
            return NumberValue(-_M_i);
          case TYPE_FLOAT:
            return NumberValue(-_M_f);
          default:
            return NumberValue();
        }
      }

      NumberValue NumberValue::operator+(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return NumberValue(_M_i + value._M_i);
        else
          return NumberValue(to_double() + value.to_double());
      }

      NumberValue NumberValue::operator-(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return NumberValue(_M_i - value._M_i);
        else
          return NumberValue(to_double() - value.to_double());
      }

      NumberValue NumberValue::operator*(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return NumberValue(_M_i * value._M_i);
        else
          return NumberValue(to_double() * value.to_double());
      }

      NumberValue NumberValue::operator/(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return NumberValue(_M_i / value._M_i);
        else
          return NumberValue(to_double() / value.to_double());
      }

      NumberValue NumberValue::operator%(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return NumberValue(_M_i % value._M_i);
        else
          return NumberValue(fmod(to_double(), value.to_double()));
      }

      NumberValue NumberValue::operator~() const
      { return NumberValue(~to_int64()); }

      NumberValue NumberValue::operator&(const NumberValue &value) const
      { return NumberValue(to_int64() & value.to_int64()); }

      NumberValue NumberValue::operator|(const NumberValue &value) const
      { return NumberValue(to_int64() | value.to_int64()); }

      NumberValue NumberValue::operator^(const NumberValue &value) const
      { return NumberValue(to_int64() ^ value.to_int64()); }

      NumberValue NumberValue::operator<<(const NumberValue &value) const
      { return NumberValue(to_int64() << value.to_int64()); }

      NumberValue NumberValue::operator>>(const NumberValue &value) const
      { return NumberValue(to_int64() >> value.to_int64()); }

      bool NumberValue::operator==(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return _M_i == value._M_i;
        else
          return to_double() == value.to_double();
      }

      bool NumberValue::operator<(const NumberValue &value) const
      {
        if(_M_type == TYPE_INT && value._M_type == TYPE_INT)
          return _M_i < value._M_i;
        else
          return to_double() < value.to_double();
      }
    }
  }
}
