/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _UTIL_HPP
#define _UTIL_HPP

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <cstdint>
#include <cmath>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/endian.h>
#else
#if !defined(_WIN32) && !defined(_WIN64)
#include <endian.h>
#endif
#endif
#include <limits>
#include <letin/format.hpp>

#if !defined(__BYTE_ORDER) && !defined(_BYTE_ORDER)
#if defined(_WIN32) || defined(_WIN64)
#define __LITTLE_ENDIAN         1234
#define __BIG_ENDIAN            4321
#define __PDP_ENDIAN            3412
#define __BYTE_ORDER            __LITTLE_ENDIAN
#else
#error "Undefined byte order."
#endif
#endif

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN         _LITTLE_ENDIAN
#define __BIG_ENDIAN            _BIG_ENDIAN
#define __PDP_ENDIAN            _PDP_ENDIAN
#define __BYTE_ORDER            _BYTE_ORDER
#endif

namespace letin
{
  namespace util
  {
    static inline std::uint64_t htonll(std::uint64_t x)
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
      return (static_cast<std::uint64_t>(htonl(x & 0xffffffffL)) << 32) | htonl(x >> 32);
#elif __BYTE_ORDER == __BIG_ENDIAN
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
      if(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4) {
        return *(reinterpret_cast<const float *>(&(x.word)));
      } else {
        float sign = (x.word & 0x80000000) != 0 ? -1.0f : 1.0f;
        if(((x.word >> 23) & 0xff) != 255) {
          int exp = static_cast<unsigned int>((x.word >> 23) & 0xff) - 127;
          float fract = std::ldexp(static_cast<float>(x.word & 0x007fffff), -23) + 1.0f;
          return std::ldexp(sign * fract, exp);
        } else {
          if((x.word & 0x007fffff) == 0)
            return sign * std::numeric_limits<float>::infinity();
          else
            return std::numeric_limits<float>::quiet_NaN();
        }
      }
    }

    static inline double format_double_to_double(const format::Double &x)
    {
      if(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8) {
        return *(reinterpret_cast<const double *>(&(x.dword)));
      } else {
        double sign = (x.dword & 0x8000000000000000LL) != 0 ? -1.0 : 1.0;
        if(((x.dword >> 52) & 0x7ff) != 2047) {
          int exp = static_cast<unsigned int>((x.dword >> 52) & 0x7ff) - 1023;
          double fract = std::ldexp(static_cast<double>(x.dword & 0x000fffffffffffffLL), -52) + 1.0;
          return std::ldexp(sign * fract, exp);
        } else {
          if((x.dword & 0x000fffffffffffffLL) == 0)
            return sign * std::numeric_limits<double>::infinity();
          else
            return std::numeric_limits<double>::quiet_NaN();
        }
      }
    }
    
    static inline format::Float float_to_format_float(float x)
    {
      format::Float y;
      if(std::numeric_limits<float>::is_iec559 && sizeof(double) == 8) {
        *reinterpret_cast<float *>(&(y.word)) = x;
      } else {
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
      }
      return y;
    }

    static inline format::Double double_to_format_double(double x)
    {
      format::Double y;
      if(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8) {
        *reinterpret_cast<double *>(&(y.dword)) = x;
      } else {
        std::uint64_t sign_bit = std::signbit(x) ? 1 : 0;
        if(std::isinf(x)) {
          y.dword = (sign_bit << 63) | 0xfff0000000000000LL;
        } else if(std::isnan(x)) {
          y.dword = (sign_bit << 63) | 0x7fffffffffffffffLL;
        } else {
          int exp;
          double fract = std::frexp(x, &exp);
          std::uint64_t biased_exp = exp + 1023;
          std::uint64_t matissa = static_cast<std::uint64_t>(std::ldexp(fract - 1.0, 52));
          y.dword = (sign_bit << 31) | (biased_exp << 23) | matissa;
        }
      }
      return y;
    }

    static inline std::size_t align(std::size_t x, std::size_t y)
    {
      return (((x + (y - 1)) / y) * y);
    }
  }
}

#endif
