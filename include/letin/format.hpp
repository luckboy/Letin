/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_FORMAT_HPP
#define _LETIN_FORMAT_HPP

#include <cstdint>

namespace letin
{
  namespace format
  {
    const std::uint32_t HEADER_FLAG_LIBRARY = 1;

    const std::uint8_t HEADER_MAGIC0 = 0x33;
    const std::uint8_t HEADER_MAGIC1 = 'L';
    const std::uint8_t HEADER_MAGIC2 = 'E';
    const std::uint8_t HEADER_MAGIC3 = 'T';
    const std::uint8_t HEADER_MAGIC4 = 0x77;
    const std::uint8_t HEADER_MAGIC5 = 'I';
    const std::uint8_t HEADER_MAGIC6 = 'N';
    const std::uint8_t HEADER_MAGIC7 = 0xff;

    const std::uint8_t HEADER_MAGIC[8] = {
      HEADER_MAGIC0,
      HEADER_MAGIC1,
      HEADER_MAGIC2,
      HEADER_MAGIC3,
      HEADER_MAGIC4,
      HEADER_MAGIC5,
      HEADER_MAGIC6,
      HEADER_MAGIC7
    };

    struct Header
    {
      std::uint8_t      magic[8];
      std::uint32_t     flags;
      std::uint32_t     entry;
      std::uint32_t     fun_count;
      std::uint32_t     var_count;
      std::uint32_t     code_size;
      std::uint32_t     data_size;
      std::uint32_t     reserved[4];
    };

    struct Float
    {
      union
      {
        std::uint8_t    bytes[4];
        std::uint32_t   word;
      };

      float to_float();
    };

    struct Double
    {
      union
      {
        std::uint8_t    bytes[8];
        std::uint64_t   dword;
      };

      double to_double();
    };

    struct Function
    {
      std::uint32_t     addr;
      std::uint32_t     arg_count;
    };

    struct Variable
    {
      union
      {
        std::int64_t    i;
        Double          f;
        std::uint32_t   addr;
      };
      std::int32_t      type;
    };

    union Argument
    {
      std::int32_t      i;
      Float             f;
      std::uint32_t     r;
    };

    struct Instruction
    {
      std::uint32_t     opcode;
      Argument          arg1;
      Argument          arg2;
    };

    struct Value
    {
      union
      {
        std::int64_t    i;
        Double          f;
        std::uint32_t   r;
      };
      std::int32_t      type;
    };

    struct Object
    {
      std::int32_t      type;
      std::uint32_t     length;
      union
      {
        std::int8_t     is8[1];
        std::int16_t    is16[1];
        std::int32_t    is32[1];
        std::int64_t    is64[1];
        Float           sfs[1];
        Double          dfs[1];
        std::uint32_t   rs[1];
        Value           tes[1];
      };
    };
  }
}

#endif


