/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include <letin/opcode.hpp>
#include "util.hpp"

#define IMM(x)                  Argument(x)
#define LV(i)                   Argument(opcode::ARG_TYPE_LVAR, i)
#define A(i)                    Argument(opcode::ARG_TYPE_ARG, i)
#define GV(i)                   Argument(opcode::ARG_TYPE_GVAR, i)
#define NA()                    Argument(0)

#define INSTR(instr)            tmp_prog_helper.add_instr(instr)
#define LET(op, arg1, arg2)     INSTR(make_instruction(opcode::INSTR_LET, opcode::OP_##op, arg1, arg2, 2))
#define IN()                    INSTR(make_instruction(opcode::INSTR_IN, 0, Argument(0), Argument(0), 2))
#define RET(op, arg1, arg2)     INSTR(make_instruction(opcode::INSTR_RET, opcode::OP_##op, arg1, arg2, 2))
#define JC(arg, i)              INSTR(make_instruction(opcode::INSTR_JC, 0, arg, Argument(i), 2))
#define JUMP(i)                 INSTR(make_instruction(opcode::INSTR_JUMP, 0, Argument(i), Argument(0), 2))
#define ARG(op, arg1, arg2)     INSTR(make_instruction(opcode::INSTR_ARG, opcode::OP_##op, arg1, arg2, 2))
#define RETRY()                 INSTR(make_instruction(opcode::INSTR_RETRY, 0, Argument(0), Argument(0), 2))
#define LETTUPLE(op, local_var_count, arg1, arg2) INSTR(make_instruction(opcode::INSTR_LETTUPLE, opcode::OP_##, arg1, arg2, local_var_count)) 

#define FUN(arg_count)                                                          \
{                                                                               \
  uint32_t tmp_arg_count = arg_count;                                           \
  uint32_t tmp_addr = tmp_prog_helper.code_size()
#define END_FUN()                                                               \
  uint32_t tmp_instr_count = tmp_prog_helper.code_size() - tmp_addr;            \
  tmp_prog_helper.add_fun(make_function(tmp_arg_count, tmp_addr,                \
                                        tmp_instr_count));                      \
}
#define FUN_ADDR_SIZE(arg_count, addr, instr_count)     tmp_prog_helper.add_fun(make_function(arg_count, addr, instr_count))

#define VALUE(value)            tmp_object_helper.add_value(value)
#define I(i)                    VALUE(make_int_value(i))
#define F(f)                    VALUE(make_float_value(f))
#define R(addr)                 VALUE(make_ref_value(addr))

#define OBJECT(type)                                                            \
{                                                                               \
  ObjectHelper tmp_object_helper(OBJECT_TYPE_##type)
#define END_OBJECT()                                                            \
  tmp_prog_helper.add_object(tmp_object_helper.ptr(), tmp_object_helper.size());\
}

#define VAR(value)              tmp_prog_helper.add_var(value)
#define VAR_I(i)                VAR(make_int_value(i))
#define VAR_F(f)                VAR(make_float_value(f))
#define VAR_R(addr)             VAR(make_ref_value(addr))
#define VAR_O()                                                                 \
{                                                                               \
  uint32_t tmp_addr = tmp_prog_helper.data_size()
#define END_VAR_O()                                                             \
  tmp_prog_helper.add_var(make_ref_value(tmp_addr));                            \
}

#define PROG(var, entry)                                                        \
ProgramHelper var(entry);                                                       \
{                                                                               \
  ProgramHelper &tmp_prog_helper = var
#define END_PROG()                                                              \
}

namespace letin
{
  namespace vm
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

      class ObjectHelper
      {
        std::int32_t _M_type;
        std::vector<format::Value> _M_values;
      public:
        ObjectHelper(std::int32_t type) : _M_type(type) {}

        void add_value(const format::Value &value) { _M_values.push_back(value); }

        void *ptr() const;

        std::size_t size() const;
      };

      class ProgramHelper
      {
        struct ObjectPair
        {
          std::unique_ptr<uint8_t []> ptr;
          std::uint32_t size;

          ObjectPair(void *ptr, std::size_t size) :
            ptr(reinterpret_cast<std::uint8_t *>(ptr)), size(size) {}
        };

        std::vector<format::Function> _M_funs;
        std::vector<format::Value> _M_vars;
        std::vector<format::Instruction> _M_instrs;
        std::vector<ObjectPair> _M_object_pairs;
        std::size_t _M_data_size;
        std::uint32_t _M_entry;
      public:
        ProgramHelper(std::uint32_t entry) : _M_entry(entry), _M_data_size(0) {}
        
        void add_fun(const format::Function &fun) { _M_funs.push_back(fun); }

        void add_var(const format::Value &value) { _M_vars.push_back(value); }

        void add_instr(const format::Instruction &instr) { _M_instrs.push_back(instr); }

        std::size_t code_size() const { return _M_instrs.size(); }

        void add_object(void *ptr, std::size_t size)
        { _M_object_pairs.push_back(ObjectPair(ptr, size)); _M_data_size += size; }

        std::size_t data_size() const { return _M_data_size; }

        void *ptr() const;

        std::size_t size() const;
      };

      format::Function make_function(std::uint32_t arg_count, std::uint32_t addr, std::uint32_t instr_count);

      format::Instruction make_instruction(std::uint32_t instr, std::uint32_t op, const Argument &arg1, const Argument &arg2, std::uint32_t local_var_count);

      format::Value make_int_value(int i);

      format::Value make_int_value(std::int64_t i);

      format::Value make_float_value(double f);

      format::Value make_ref_value(std::uint32_t addr);
      
      struct ProgramDelete
      {
        void operator()(void *ptr) const
        { delete [] reinterpret_cast<std::uint8_t *>(ptr);}
      };
    }
  }
}

#endif
