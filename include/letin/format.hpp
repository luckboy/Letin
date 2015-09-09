/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
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
    const std::uint32_t HEADER_FLAG_LIBRARY = 1 << 0;
    const std::uint32_t HEADER_FLAG_RELOCATABLE = 1 << 1;
    const std::uint32_t HEADER_FLAG_SYMBOLIC_NATIVE_FUNS = 1 << 2;

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
      std::uint32_t     reloc_count;
      std::uint32_t     symbol_count;
      std::uint32_t     reserved[2];
    };

    struct Float
    {
      union
      {
        std::uint8_t    bytes[4];
        std::uint32_t   word;
      };
    };

    struct Double
    {
      union
      {
        std::uint8_t    bytes[8];
        std::uint64_t   dword;
      };
    };

    struct Function
    {
      std::uint32_t     addr;
      std::uint32_t     arg_count;
      std::uint32_t     instr_count;
    };

    struct Value
    {
      std::int32_t      type;
      std::uint32_t     __pad;
      union
      {
        std::int64_t    i;
        Double          f;
        std::uint64_t   addr;
      };
    };

    union Argument
    {
      std::int32_t      i;
      Float             f;
      std::uint32_t     lvar;
      std::uint32_t     arg;
      std::uint32_t     gvar;
    };

    struct Instruction
    {
      std::uint32_t     opcode;
      Argument          arg1;
      Argument          arg2;
    };
    
    union TupleElement
    {
      std::int64_t      i;
      Double            f;
      std::uint64_t     addr;
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
        TupleElement    tes[1];
        std::int8_t     tets[1];
      };

      const std::int8_t *tuple_elem_types() const { return &(tets[length * 8]); }

      std::int8_t *tuple_elem_types() { return &(tets[length * 8]); }
    };

    const std::uint32_t RELOC_TYPE_ARG1_FUN = 0;
    const std::uint32_t RELOC_TYPE_ARG2_FUN = 1;
    const std::uint32_t RELOC_TYPE_ARG1_VAR = 2;
    const std::uint32_t RELOC_TYPE_ARG2_VAR = 3;
    const std::uint32_t RELOC_TYPE_ELEM_FUN = 4;
    const std::uint32_t RELOC_TYPE_VAR_FUN = 5;
    const std::uint32_t RELOC_TYPE_ARG1_NATIVE_FUN = 6; 
    const std::uint32_t RELOC_TYPE_ARG2_NATIVE_FUN = 7; 
    const std::uint32_t RELOC_TYPE_ELEM_NATIVE_FUN = 8; 
    const std::uint32_t RELOC_TYPE_VAR_NATIVE_FUN = 9; 
    const std::uint32_t RELOC_TYPE_SYMBOLIC = 256;

    struct Relocation
    {
      std::uint32_t     type;
      std::uint32_t     addr;
      std::uint32_t     symbol;
    };

    const std::uint8_t SYMBOL_TYPE_FUN = 0;
    const std::uint8_t SYMBOL_TYPE_VAR = 1;
    const std::uint8_t SYMBOL_TYPE_NATIVE_FUN = 2;
    const std::uint8_t SYMBOL_TYPE_DEFINED = 16;

    struct Symbol
    {
      std::uint32_t     index;
      std::uint16_t     length;
      std::uint8_t      type;
      char              name[1];
    };
  }
}

#endif
