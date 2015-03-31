/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
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
      bool is_fun(uint32_t arg_count, uint32_t instr_count, const format::Function &fun)
      { return arg_count == ntohl(fun.arg_count) && instr_count == ntohl(fun.instr_count); }

      bool is_instr(uint32_t instr, uint32_t op, const Argument &arg1, const Argument &arg2, uint32_t local_var_count, const format::Instruction &format_instr)
      {
        uint32_t opcode = ntohl(format_instr.opcode);
        if(instr != opcode_to_instr(opcode)) return false;
        if(op != opcode_to_op(opcode)) return false;
        if(arg1.type != opcode_to_arg_type1(opcode)) return false;
        if(arg2.type != opcode_to_arg_type2(opcode)) return false;
        if(arg1.format_arg.i != ntohl(format_instr.arg1.i)) return false;
        if(arg2.format_arg.i != ntohl(format_instr.arg2.i)) return false;
        if(local_var_count != opcode_to_local_var_count(opcode)) return false;
        return true;
      }

      bool is_object(int32_t type, uint32_t length, const format::Object &object)
      { return type == ntohl(object.type) && length == ntohl(object.length); }
      
      bool is_int_value(int64_t i, const format::Value &value)
      { return VALUE_TYPE_INT == ntohl(value.type) && i == ntohll(value.i); }

      bool is_float_value(double f, const format::Value &value)
      { 
        format::Double tmp_format_f2 = value.f;
        tmp_format_f2.dword = ntohll(tmp_format_f2.dword);
        double f2 = format_double_to_double(tmp_format_f2);
        return VALUE_TYPE_FLOAT == ntohl(value.type) && f == f2;
      }

      bool is_ref_value(const format::Value &value)
      { return VALUE_TYPE_REF == ntohl(value.type); }

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
            return VALUE_TYPE_INT == object.tets[ntohl(object.length) * 8 + j] && i == ntohll(object.tes[j].i);
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
            if(VALUE_TYPE_FLOAT != object.tets[ntohl(object.length) * 8 + j]) return false;
            format::Double tmp_format_f2 = object.tes[j].f;
            tmp_format_f2.dword = ntohll(tmp_format_f2.dword);
            double f2 = format_double_to_double(tmp_format_f2);
            return f == f2;
          }
          default:
            return false;
        }
      }

      bool is_ref_value_in_object(const format::Object &object, size_t j)
      {
        switch(ntohl(object.type)) {
          case OBJECT_TYPE_RARRAY:
            return true;
          case OBJECT_TYPE_TUPLE:
            return VALUE_TYPE_REF == object.tets[ntohl(object.length) * 8 + j];
          default:
            return false;
        }
      }
      
      size_t object_addr_in_object(const format::Object &object, size_t j)
      {
        switch(ntohl(object.type)) {
          case OBJECT_TYPE_RARRAY:
            return ntohl(object.rs[j]);
          case OBJECT_TYPE_TUPLE:
            return VALUE_TYPE_REF == object.tets[ntohl(object.length) * 8 + j] ? ntohll(object.tes[j].addr) : 0;
          default:
            return 0;
        }
      }

      bool is_reloc(uint32_t type, uint32_t addr, const std::string &symbol_name, const format::Header &header, const format::Relocation *relocs, const uint8_t *symbols)
      { 
        uint32_t reloc_count = ntohl(header.reloc_count);
        uint32_t symbol_count = ntohl(header.symbol_count);
        for(uint32_t i = 0; i < reloc_count; i++) {
          if(type == ntohl(relocs[i].type) && addr == ntohl(relocs[i].addr)) {
            if((type & format::RELOC_TYPE_SYMBOLIC) != 0) {
              for(uint32_t j = 0, k = 0; k < symbol_count; k++) {
                const format::Symbol *symbol = reinterpret_cast<const format::Symbol *>(symbols + j);
                uint16_t length = ntohs(symbol->length);
                if(htonl(relocs[i].symbol) == k) {
                  bool has_correct_type = false;
                  switch(type & ~format::RELOC_TYPE_SYMBOLIC) {
                    case format::RELOC_TYPE_ARG1_FUN:
                    case format::RELOC_TYPE_ARG2_FUN:
                    case format::RELOC_TYPE_ELEM_FUN:
                      has_correct_type = (symbol->type == format::SYMBOL_TYPE_FUN);
                      break;
                    case format::RELOC_TYPE_ARG1_VAR:
                    case format::RELOC_TYPE_ARG2_VAR:
                      has_correct_type = (symbol->type == format::SYMBOL_TYPE_VAR);
                      break;
                  }
                  return (has_correct_type && symbol_name.length() == length && equal(symbol_name.begin(), symbol_name.end(), symbol->name));
                }
                j += align(7 + length, 8);
              }
              return false;
            } else
              return true;
          }
        }
        return false;
      }

      bool is_symbol(uint32_t index, uint8_t type, const string &name, const format::Symbol &symbol)
      {
        if(index != ntohl(symbol.index)) return false;
        if(type != symbol.type) return false;
        if(name.length() != ntohs(symbol.length)) return false;
        return equal(name.begin(), name.end(), symbol.name);
      }
    }
  }
}
