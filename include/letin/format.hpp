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
    const uint32_t HEADER_FLAG_LIBRARY = 1;

    const uint8_t HEADER_MAGIC0 = 0x33;
    const uint8_t HEADER_MAGIC1 = 'L';
    const uint8_t HEADER_MAGIC2 = 'E';
    const uint8_t HEADER_MAGIC3 = 'T';
    const uint8_t HEADER_MAGIC4 = 0x77;
    const uint8_t HEADER_MAGIC5 = 'I';
    const uint8_t HEADER_MAGIC6 = 'N';
    const uint8_t HEADER_MAGIC7 = 0xff;

    struct Header
    {
      uint8_t           magic[8];
      uint32_t          flags;
      uint32_t          entry;
      uint32_t          fun_count;
      uint32_t          var_count;
      uint32_t          code_size;
      uint32_t          data_size;
      uint32_t          reserved[4];
    };

    struct Float
    {
      union
      {
        uint8_t         bytes[4];
        uint32_t        word;
      };

      float to_float();
    };

    struct Double
    {
      union
      {
        uint8_t         bytes[8];
        uint64_t        dword;
      };

      double to_double();
    };

    struct Function
    {
      uint32_t          addr;
      uint32_t          arg_count;
    };

    struct Variable
    {
      union
      {
        uint64_t        i;
        Double          f;
        uint32_t        addr;
      };
      int32_t           type;
    };

    union Argument
    {
      int32_t           i;
      Float             f;
      uint32_t          r;
    };

    struct Instruction
    {
      uint32_t          opcode;
      Argument          arg1;
      Argument          arg2;
    };

    struct Value
    {
      union
      {
        int64_t         i;
        Double          f;
        uint32_t        r;
      };
      int32_t           type;
    };

    struct Object
    {
      int32_t           type;
      uint32_t          length;
      union
      {
        int8_t          is8[1];
        int16_t         is16[1];
        int32_t         is32[1];
        int64_t         is64[1];
        Float           sfs[1];
        Double          dfs[1];
        uint32_t        rs[1];
        Value           tes[1];
      };
    };
  }
}

#endif


