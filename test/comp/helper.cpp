/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <netinet/in.h>
#include <letin/opcode.hpp>
#include "helper.hpp"

using namespace std;
using namespace letin::opcode;
using namespace letin::util;

namespace letin
{
  namespace comp
  {
    namespace test
    {
      bool is_fun(uint32_t arg_count, const format::Function &fun)
      { return arg_count == ntohl(fun.arg_count); }

      bool is_instr(uint32_t instr, uint32_t op, const Argument &arg1, const Argument &arg2, const format::Instruction &format_instr)
      {
        uint32_t opcode = ntohl(format_instr.opcode);
        if(instr != opcode_to_instr(opcode)) return false;
        if(arg1.type != opcode_to_arg_type1(opcode)) return false;
        if(arg2.type != opcode_to_arg_type2(opcode)) return false;
        if(arg1.format_arg.i != ntohl(format_instr.arg1.i)) return false;
        if(arg2.format_arg.i != ntohl(format_instr.arg2.i)) return false;
        return true;
      }

      bool is_object(int32_t type, uint32_t length, const format::Object &object)
      { return type == ntohl(object.type) && length == ntohl(object.length); }
      
      bool is_int_value(int64_t i, const format::Value &value)
      { return VALUE_TYPE_INT == value.type && i == ntohll(value.i); }

      bool is_float_value(double f, const format::Value &value)
      { 
        format::Double tmp_format_f2 = value.f;
        tmp_format_f2.dword = ntohll(tmp_format_f2.dword);
        double f2 = format_double_to_double(tmp_format_f2);
        return VALUE_TYPE_FLOAT == value.type && f == f2;
      }

      bool is_ref_value(uint32_t addr, const format::Value &value)
      { return VALUE_TYPE_REF == value.type && addr == ntohll(value.addr); }

      bool is_int_value_in_object(int64_t i, const format::Object &object, size_t j)
      {
        switch(ntohl(object.type)) {
          case OBJECT_TYPE_IARRAY8:
            return i == object.is8[j];
          case OBJECT_TYPE_IARRAY16:
            return i == ntohs(object.is16[j]);
          case OBJECT_TYPE_IARRAY32:
            return i == ntohl(object.is32[j]);
          case OBJECT_TYPE_IARRAY64:
            return i == ntohll(object.is64[j]);
          case OBJECT_TYPE_TUPLE:
            return VALUE_TYPE_INT == object.tets[object.length + j] && i == ntohll(object.tes[j].i);
          default:
            return false;
        }
      }

      bool is_float_value_in_object(double f, const format::Object &object, size_t j)
      {
        switch(ntohl(object.type)) {
          case OBJECT_TYPE_SFARRAY:
          {
            format::Float tmp_format_f2 = object.sfs[j];
            tmp_format_f2.word = ntohl(tmp_format_f2.word);
            double f2 = format_float_to_float(tmp_format_f2);
            return f == f2;
          }
          case OBJECT_TYPE_DFARRAY:
          {
            format::Double tmp_format_f2 = object.dfs[j];
            tmp_format_f2.dword = ntohll(tmp_format_f2.dword);
            double f2 = format_double_to_double(tmp_format_f2);
            return f == f2;
          }
          case OBJECT_TYPE_TUPLE:
          {
            if(VALUE_TYPE_INT != object.tets[object.length + j]) return false;
            format::Double tmp_format_f2 = object.tes[j].f;
            tmp_format_f2.dword = ntohll(tmp_format_f2.dword);
            double f2 = format_double_to_double(tmp_format_f2);
            return f == f2;
          }
          default:
            return false;
        }
      }

      bool is_ref_value_in_object(uint32_t addr, const format::Object &object, size_t j)
      {
        switch(ntohl(object.type)) {
          case OBJECT_TYPE_RARRAY:
            return addr == ntohl(object.rs[j]);
          case OBJECT_TYPE_TUPLE:
            return VALUE_TYPE_REF == object.tets[object.length + j] && addr == ntohll(object.tes[j].addr);
          default:
            return false;
        }
      }
    }
  }
}
