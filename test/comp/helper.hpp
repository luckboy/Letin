/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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

#define ASSERT_INSTR(instr, op, arg1, arg2, j)                                  \
CPPUNIT_ASSERT(is_instr(instr, op, arg1, arg2, tmp_instrs[j]))
#define ASSERT_LET(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_LET, opcode::OP_##op, arg1, arg2, j)
#define ASSERT_IN(j)                    ASSERT_INSTR(opcode::INSTR_IN, 0, Argument(0), Argument(0), j)
#define ASSERT_RET(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_RET, opcode::OP_##op, arg1, arg2, j)
#define ASSERT_JC(arg, i, j)            ASSERT_INSTR(opcode::INSTR_JC, 0, arg, Argument(i), j)
#define ASSERT_JUMP(i, j)               ASSERT_INSTR(opcode::INSTR_JUMP, 0, Argument(i), Argument(0), j)
#define ASSERT_ARG(op, arg1, arg2, j)   ASSERT_INSTR(opcode::INSTR_ARG, opcode::OP_##op, arg1, arg2, j)
#define ASSERT_RETRY(j)                 ASSERT_INSTR(opcode::INSTR_IN, 0, Argument(0), Argument(0), j)

#define ASSERT_FUN(arg_count, fun_offset, code_offset)                          \
{                                                                               \
  const format::Function *tmp_fun =                                             \
    reinterpret_cast<const format::Function *>(tmp_ptr + fun_offset);           \
  size_t tmp_instr_addr = ntohl(tmp_fun->addr);                                 \
  const format::Instruction *tmp_instrs =                                       \
    reinterpret_cast<const format::Instruction *>(tmp_ptr + code_offset) +      \
    tmp_instr_addr;                                                             \
  CPPUNIT_ASSERT(is_fun(arg_count, *tmp_fun))
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

#define ASSERT_HEADER_MAGIC()           CPPUNIT_ASSERT(equal(format::HEADER_MAGIC, format::HEADER_MAGIC + 8, tmp_header->magic))
#define ASSERT_HEADER_FIELD(i, field)   CPPUNIT_ASSERT_EQUAL(i, ntohl(tmp_header->field))
#define ASSERT_HEADER_FLAGS(i)          ASSERT_HEADER_FIELD(i, flags)
#define ASSERT_HEADER_ENTRY(i)          ASSERT_HEADER_FIELD(i, entry)
#define ASSERT_HEADER_FUN_COUNT(i)      ASSERT_HEADER_FIELD(i, fun_count)
#define ASSERT_HEADER_VAR_COUNT(i)      ASSERT_HEADER_FIELD(i, var_count)
#define ASSERT_HEADER_CODE_SIZE(i)      ASSERT_HEADER_FIELD(i, code_size)
#define ASSERT_HEADER_DATA_SIZE(i)      ASSERT_HEADER_FIELD(i, data_size)

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

      bool is_fun(std::uint32_t arg_count, const format::Function &fun);

      bool is_instr(std::uint32_t instr, std::uint32_t op, const Argument &arg1, const Argument &arg2, const format::Instruction &format_instr);

      bool is_object(std::int32_t type, std::uint32_t length, const format::Object &object);
      
      bool is_int_value(std::int64_t i, const format::Value &value);

      bool is_float_value(double f, const format::Value &value);

      bool is_ref_value(const format::Value &value);

      bool is_int_value_in_object(std::int64_t i, const format::Object &object, std::size_t j);

      bool is_float_value_in_object(double f, const format::Object &object, std::size_t j);

      bool is_ref_value_in_object(const format::Object &object, std::size_t j);

      std::size_t object_addr_in_object(const format::Object &object, std::size_t j);
    }
  }
}

#endif
