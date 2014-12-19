/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <netinet/in.h>
#include <cstdint>
#include <cmath>
#include <endian.h>
#include <letin/format.hpp>

namespace letin
{
  namespace util
  {
    static inline std::uint64_t htonll(std::uint64_t x)
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
      return (static_cast<std::uint64_t>(htonl(x & 0xffffffffL)) << 32) | htonl(x >> 32);
#elif  __BYTE_ORDER == __BIG_ENDIAN
      return x;
#else
#error "Unsupported byte order."
#endif
    }

    static inline std::uint64_t ntohll(std::uint64_t x)
    {
      return htonll(x);
    }

    static inline float format_float_to_float(const format::Float &x)
    {
#if defined(__STDC_IEC_559__) && (__SIZEOF_FLOAT__ == 4)
      return *(reinterpret_cast<const float *>(&(x.word)));
#else
      float sign = (x.word & 0x80000000) != 0 ? -1.0f : 1.0f;
      if(((x.word >> 23) & 0xff) != 255) {
        int exp = static_cast<unsigned int>((x.word >> 23) & 0xff) - 127;
        float fract = std::ldexp(static_cast<float>(x.word & 0x007fffff), -23) + 1.0f;
        return std::ldexp(sign * fract, exp);
      } else {
        if((x.word & 0x007fffff) == 0)
          return sign * INFINITY;
        else
          return NAN;
      }
#endif
    }

    static inline double format_double_to_double(const format::Double &x)
    {
#if defined(__STDC_IEC_559__) && (__SIZEOF_DOUBLE__ == 8)
      return *(reinterpret_cast<const double *>(&(x.dword)));
#else
      double sign = (x.dword & 0x8000000000000000LL) != 0 ? -1.0 : 1.0;
      if(((x.dword >> 52) & 0x7ff) != 2047) {
        int exp = static_cast<unsigned int>((x.dword >> 52) & 0x7ff) - 1023;
        double fract = std::ldexp(static_cast<double>(x.dword & 0x000fffffffffffffLL), -52) + 1.0;
        return std::ldexp(sign * fract, exp);
      } else {
        if((x.dword & 0x000fffffffffffffLL) == 0)
          return sign * INFINITY;
        else
          return NAN;
      }
#endif
    }
    
    static inline format::Float float_to_format_float(float x)
    {
      format::Float y;
#if defined(__STDC_IEC_559__) && (__SIZEOF_FLOAT__ == 4)
      *reinterpret_cast<float *>(&(y.word)) = x;
#else
      std::uint32_t sign_bit = std::signbit(x) ? 1 : 0;
      if(std::isinf(x)) {
        y.word = (sign_bit << 31) | 0x7f800000;
      } else if(std::isnan(x)) {
        y.word = (sign_bit << 31) | 0x7fffffff;
      } else {
        int exp;
        float fract = std::frexp(x, &exp);
        std::uint32_t biased_exp = exp + 127;
        std::uint32_t matissa = static_cast<std::uint32_t>(std::ldexp(fract - 1.0f, 23));
        y.word = (sign_bit << 31) | (biased_exp << 23) | matissa;
      }
#endif
      return y;
    }

    static inline format::Double double_to_format_double(double x)
    {
      format::Double y;
#if defined(__STDC_IEC_559__) && (__SIZEOF_DOUBLE__ == 8)
      *reinterpret_cast<double *>(&(y.dword)) = x;
#else
      std::uint64_t sign_bit = std::signbit(x) ? 1 : 0;
      if(std::isinf(x)) {
        y.word = (sign_bit << 63) | 0xfff0000000000000LL;
      } else if(std::isnan(x)) {
        y.word = (sign_bit << 63) | 0x7fffffffffffffffLL;
      } else {
        int exp;
        double fract = std::frexp(x, &exp);
        std::uint64_t biased_exp = exp + 1023;
        std::uint64_t matissa = static_cast<std::uint64_t>(std::ldexp(fract - 1.0, 52));
        y.word = (sign_bit << 31) | (biased_exp << 23) | matissa;
      }
#endif
      return y;
    }

    static inline std::size_t align(std::size_t x, std::size_t y)
    {
      return (((x + (y - 1)) / y) * y);
    }
  }
}

#endif
