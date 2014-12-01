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
#include <endian.h>

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

#endif