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

inline std::uint64_t htonll(std::uint64_t x)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return (static_cast<std::uint64_t>(htonl(x & 0xffffffffL)) << 32) & htonl(x >> 32);
#elif  __BYTE_ORDER == __BIG_ENDIAN
  return x;
#else
#error "Unsupported byte order."
#endif
}

inline std::uint64_t ntohll(std::uint64_t x)
{
  return htonll(x);
}

inline float format_float_to_float(const letin::format::Float &x)
{
#if defined(__STDC_IEC_559__) && (__SIZEOF_FLOAT__ == 4)
  return *(reinterpret_cast<const float *>(&(x.word)));
#else
  float sign = (x.word & 0x80000000) != 0 ? -1.0f : 1.0f;
  int exp = static_cast<unsigned int>((x.word >> 23) & 0xff) - 127;
  float fract = std::ldexp(static_cast<float>(x.word & 0x007fffff), -23) + 1.0f;
  return std::ldexp(sign * fract, exp);
#endif
}

inline double format_double_to_double(const letin::format::Double &x)
{
#if defined(__STDC_IEC_559__) && (__SIZEOF_DOUBLE__ == 8)
  return *(reinterpret_cast<const double *>(&(x.dword)));
#else
  double sign = (x.dword & 0x8000000000000000LL) != 0 ? -1.0 : 1.0;
  int exp = static_cast<unsigned int>((x.dword >> 52) & 0x7ff) - 1023;
  double fract = std::ldexp(static_cast<double>(x.dword & 0x000fffffffffffffLL), -52) + 1.0;
  return std::ldexp(sign * fract, exp);
#endif
}

#endif
