/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <netinet/in.h>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include <letin/opcode.hpp>
#include "util.hpp"

#define IMM(x)                          Argument(x)
#define LV(i)                           Argument(opcode::ARG_TYPE_LVAR, i)
#define A(i)                            Argument(opcode::ARG_TYPE_ARG, i)
#define GV(i)                           Argument(opcode::ARG_TYPE_GVAR, i)
#define NA()                            Argument(0)

#define ASSERT_INSTR(instr, op, arg1, arg2, local_var_count, j)                 \
CPPUNIT_ASSERT(is_instr(instr, op, arg1, arg2, local_var_count, tmp_instrs[j]))
#define ASSERT_LET(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_LET, opcode::OP_##op, arg1, arg2, 2, j)
#define ASSERT_IN(j)                    ASSERT_INSTR(opcode::INSTR_IN, 0, Argument(0), Argument(0), 2, j)
#define ASSERT_RET(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_RET, opcode::OP_##op, arg1, arg2, 2, j)
#define ASSERT_JC(arg, i, j)            ASSERT_INSTR(opcode::INSTR_JC, 0, arg, Argument(i), 2, j)
#define ASSERT_JUMP(i, j)               ASSERT_INSTR(opcode::INSTR_JUMP, 0, Argument(i), Argument(0), 2, j)
#define ASSERT_ARG(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_ARG, opcode::OP_##op, arg1, arg2, 2, j)
#define ASSERT_RETRY(j)                 ASSERT_INSTR(opcode::INSTR_RETRY, 0, Argument(0), Argument(0), 2, j)
#define ASSERT_LETTUPLE(local_var_count, op, arg1, arg2, j)                     \
ASSERT_INSTR(opcode::INSTR_LETTUPLE, opcode::OP_##op, arg1, arg2, local_var_count, j)

#define ASSERT_FUN(arg_count, instr_count, fun_offset, code_offset)             \
{                                                                               \
  const format::Function *tmp_fun =                                             \
    reinterpret_cast<const format::Function *>(tmp_ptr + fun_offset);           \
  size_t tmp_instr_addr = ntohl(tmp_fun->addr);                                 \
  const format::Instruction *tmp_instrs =                                       \
    reinterpret_cast<const format::Instruction *>(tmp_ptr + code_offset) +      \
    tmp_instr_addr;                                                             \
  CPPUNIT_ASSERT(is_fun(arg_count, instr_count, *tmp_fun))
  
#define END_ASSERT_FUN()                                                        \
}

#define ASSERT_VAR_I(i, offset)                                                 \
{                                                                               \
  const format::Value *tmp_value =                                              \
    reinterpret_cast<const format::Value *>(tmp_ptr + offset);                  \
  CPPUNIT_ASSERT(is_int_value(i, *tmp_value));                                  \
}
#define ASSERT_VAR_F(f, offset)                                                 \
{                                                                               \
  const format::Value *tmp_value =                                              \
    reinterpret_cast<const format::Value *>(tmp_ptr + offset);                  \
  CPPUNIT_ASSERT(is_float_value(f, *tmp_value));                                \
}
#define ASSERT_VAR_O(type, length, var_offset, data_offset)                     \
{                                                                               \
  const format::Value *tmp_value =                                              \
    reinterpret_cast<const format::Value *>(tmp_ptr + var_offset);              \
  CPPUNIT_ASSERT(is_ref_value(*tmp_value));                                     \
  size_t tmp_object_offset = data_offset + ntohll(tmp_value->addr);             \
  ASSERT_OBJECT(type, length, tmp_object_offset)
#define END_ASSERT_VAR_O()                                                      \
  END_ASSERT_OBJECT();                                                          \
}

#define ASSERT_I(i, j)                  CPPUNIT_ASSERT(is_int_value_in_object(i, *tmp_object, j))
#define ASSERT_F(f, j)                  CPPUNIT_ASSERT(is_float_value_in_object(f, *tmp_object, j))
#define ASSERT_O(type, length, j, data_offset)                                  \
{                                                                               \
  CPPUNIT_ASSERT(is_ref_value_in_object(*tmp_object, j));                       \
  size_t tmp_object_offset = data_offset +                                      \
    object_addr_in_object(*tmp_object, j);                                      \
  ASSERT_OBJECT(type, length, tmp_object_offset)
#define END_ASSERT_O()                                                          \
  END_ASSERT_OBJECT();                                                          \
}

#define ASSERT_OBJECT(type, length, offset)                                     \
{                                                                               \
  const format::Object *tmp_object =                                            \
    reinterpret_cast<const format::Object *>(tmp_ptr + offset);                 \
  CPPUNIT_ASSERT(is_object(OBJECT_TYPE_##type, length, *tmp_object))
#define END_ASSERT_OBJECT()                                                     \
}

#define ASSERT_RELOC(type, addr, symbol, offset)                                \
{                                                                               \
  const format::Relocation *tmp_reloc =                                         \
    reinterpret_cast<const format::Relocation *>(tmp_ptr + offset);             \
  CPPUNIT_ASSERT(is_reloc(type, addr, symbol, *tmp_reloc);                      \
}

#define ASSERT_RELOC_A1F(addr, offset)  ASSERT_RELOC(format::RELOC_TYPE_ARG1_FUN, addr, 0, offset)
#define ASSERT_RELOC_A2F(addr, offset)  ASSERT_RELOC(format::RELOC_TYPE_ARG2_FUN, addr, 0, offset)
#define ASSERT_RELOC_A1V(addr, offset)  ASSERT_RELOC(format::RELOC_TYPE_ARG1_VAR, addr, 0, offset)
#define ASSERT_RELOC_A2V(addr, offset)  ASSERT_RELOC(format::RELOC_TYPE_ARG2_VAR, addr, 0, offset)
#define ASSERT_RELOC_EF(addr, offset)   ASSERT_RELOC(format::RELOC_TYPE_ELEM_FUN, addr, 0, offset)
#define ASSERT_RELOC_SA1F(addr, symbol, offset) ASSERT_RELOC(format::RELOC_TYPE_ARG1_FUN | format::RELOC_TYPE_SYMBOLIC, addr, symbol, offset)
#define ASSERT_RELOC_SA2F(addr, symbol, offset) ASSERT_RELOC(format::RELOC_TYPE_ARG2_FUN | format::RELOC_TYPE_SYMBOLIC, addr, symbol, offset)
#define ASSERT_RELOC_SA1V(addr, symbol, offset) ASSERT_RELOC(format::RELOC_TYPE_ARG1_VAR | format::RELOC_TYPE_SYMBOLIC, addr, symbol, offset)
#define ASSERT_RELOC_SA2V(addr, symbol, offset) ASSERT_RELOC(format::RELOC_TYPE_ARG2_VAR | format::RELOC_TYPE_SYMBOLIC, addr, symbol, offset)
#define ASSERT_RELOC_SEF(addr, symbol, offset) ASSERT_RELOC(format::RELOC_TYPE_ELEM_FUN | format::RELOC_TYPE_SYMBOLIC, addr, symbol, offset)

#define ASSERT_SYMBOL(type, name, index, offset)                                \
{                                                                               \
  const format::Symbol *tmp_symbol =                                            \
    reinterpret_cast<const format::Symbol *>(tmp_ptr + offset);                 \
  CPPUNIT_ASSERT(is_symbol(index, type, name, *tmp_symbol);                     \
}

#define ASSERT_SYMBOL_UF(name, offset)  ASSERT_SYMBOL(format::SYMBOL_TYPE_FUN, name, 0, offset)
#define ASSERT_SYMBOL_UV(name, offset)  ASSERT_SYMBOL(format::SYMBOL_TYPE_VAR, name, 0, offset)
#define ASSERT_SYMBOL_DF(name, index, offset) ASSERT_SYMBOL(format::SYMBOL_TYPE_FUN | format::SYMBOL_TYPE_DEFINED, name, index, offset)
#define ASSERT_SYMBOL_DV(name, index, offset) ASSERT_SYMBOL(format::SYMBOL_TYPE_VAR | format::SYMBOL_TYPE_DEFINED, name, index, offset)

#define ASSERT_HEADER_MAGIC()           CPPUNIT_ASSERT(equal(format::HEADER_MAGIC, format::HEADER_MAGIC + 8, tmp_header->magic))
#define ASSERT_HEADER_FIELD(i, field)   CPPUNIT_ASSERT_EQUAL(i, ntohl(tmp_header->field))
#define ASSERT_HEADER_FLAGS(i)          ASSERT_HEADER_FIELD(i, flags)
#define ASSERT_HEADER_ENTRY(i)          ASSERT_HEADER_FIELD(i, entry)
#define ASSERT_HEADER_FUN_COUNT(i)      ASSERT_HEADER_FIELD(i, fun_count)
#define ASSERT_HEADER_VAR_COUNT(i)      ASSERT_HEADER_FIELD(i, var_count)
#define ASSERT_HEADER_CODE_SIZE(i)      ASSERT_HEADER_FIELD(i, code_size)
#define ASSERT_HEADER_DATA_SIZE(i)      ASSERT_HEADER_FIELD(i, data_size)
#define ASSERT_HEADER_RELOC_COUNT(i)    ASSERT_HEADER_FIELD(i, reloc_count)
#define ASSERT_HEADER_SYMBOL_COUNT(i)   ASSERT_HEADER_FIELD(i, symbol_count)

#define ASSERT_PROG(prog_size, prog)                                            \
{                                                                               \
  const uint8_t *tmp_ptr = reinterpret_cast<const uint8_t *>(prog.ptr());       \
  const size_t tmp_size = prog.size();                                          \
  const format::Header *tmp_header =                                            \
    reinterpret_cast<const format::Header *>(tmp_ptr);                          \
  CPPUNIT_ASSERT_EQUAL(prog_size, tmp_size)
#define END_ASSERT_PROG()                                                       \
}

namespace letin
{
  namespace comp
  {
    namespace test
    {
      struct Argument
      {
        std::uint32_t type;
        format::Argument format_arg;

        Argument(std::int32_t i) : type(opcode::ARG_TYPE_IMM)
        { format_arg.i = i; }

        Argument(float f) : type(opcode::ARG_TYPE_IMM)
        { format_arg.f = util::float_to_format_float(f); }

        Argument(std::uint32_t type, std::uint32_t i) : type(type)
        { format_arg.lvar = i; }
      };

      bool is_fun(std::uint32_t arg_count, std::uint32_t instr_count, const format::Function &fun);

      bool is_instr(std::uint32_t instr, std::uint32_t op, const Argument &arg1, const Argument &arg2, std::uint32_t local_var_count, const format::Instruction &format_instr);

      bool is_object(std::int32_t type, std::uint32_t length, const format::Object &object);
      
      bool is_int_value(std::int64_t i, const format::Value &value);

      bool is_float_value(double f, const format::Value &value);

      bool is_ref_value(const format::Value &value);

      bool is_int_value_in_object(std::int64_t i, const format::Object &object, std::size_t j);

      bool is_float_value_in_object(double f, const format::Object &object, std::size_t j);

      bool is_ref_value_in_object(const format::Object &object, std::size_t j);

      std::size_t object_addr_in_object(const format::Object &object, std::size_t j);

      bool is_reloc(std::uint32_t type, std::uint32_t addr, std::uint32_t symbol, const format::Relocation &reloc);

      bool is_symbol(std::uint32_t index, std::uint8_t type, const std::string &name, const format::Symbol &symbol);
    }
  }
}

#endif
