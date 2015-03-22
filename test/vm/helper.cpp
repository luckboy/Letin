/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <netinet/in.h>
#include <algorithm>
#include <numeric>
#include <letin/opcode.hpp>
#include "helper.hpp"

using namespace std;
using namespace letin::opcode;
using namespace letin::util;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      //
      // An ObjectHelper class.
      //

      void *ObjectHelper::ptr() const
      {
        uint8_t *ptr = new uint8_t[size()];
        format::Object *object = reinterpret_cast<format::Object *>(ptr);
        object->type = _M_type;
        object->length = _M_values.size();
        switch(_M_type)
        {
          case OBJECT_TYPE_IARRAY8:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->is8[i] = ntohll(_M_values[i].i);
            break;
          case OBJECT_TYPE_IARRAY16:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->is16[i] = htons(ntohll(_M_values[i].i));
            break;
          case OBJECT_TYPE_IARRAY32:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->is32[i] = htonl(ntohll(_M_values[i].i));
            break;
          case OBJECT_TYPE_IARRAY64:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->is64[i] = _M_values[i].i;
            break;
          case OBJECT_TYPE_SFARRAY:
            for(size_t i = 0; i < _M_values.size(); i++) {
              format::Double format_f = _M_values[i].f;
              format_f.dword = ntohll(format_f.dword);
              double f = format_double_to_double(format_f);
              object->sfs[i].word = htonl(float_to_format_float(f).word);
            }
            break;
          case OBJECT_TYPE_DFARRAY:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->dfs[i] = _M_values[i].f;
            break;
          case OBJECT_TYPE_RARRAY:
            for(size_t i = 0; i < _M_values.size(); i++)
              object->rs[i] = _M_values[i].addr;
            break;
          case OBJECT_TYPE_TUPLE:
            for(size_t i = 0; i < _M_values.size(); i++) {
              object->tes[i].i = _M_values[i].i;
              object->tuple_elem_types()[i] = ntohl(_M_values[i].type);
            }
            break;
        }
        object->type = htonl(object->type);
        object->length = htonl(object->length);
        return reinterpret_cast<void *>(ptr);
      }

      size_t ObjectHelper::size() const
      {
        size_t elem_size = 0;
        switch(_M_type) {
          case OBJECT_TYPE_IARRAY8:
            elem_size = 1;
            break;
          case OBJECT_TYPE_IARRAY16:
            elem_size = 2;
            break;
          case OBJECT_TYPE_IARRAY32:
            elem_size = 4;
            break;
          case OBJECT_TYPE_IARRAY64:
            elem_size = 8;
            break;
          case OBJECT_TYPE_SFARRAY:
            elem_size = 4;
            break;
          case OBJECT_TYPE_DFARRAY:
            elem_size = 8;
            break;
          case OBJECT_TYPE_RARRAY:
            elem_size = 4;
            break;
          case OBJECT_TYPE_TUPLE:
            elem_size = 9;
            break;
        }
        return (sizeof(format::Object) - 8) + _M_values.size() * elem_size;
      }

      //
      // A SymbolHelper class.
      //

      void *SymbolHelper::ptr() const
      {
        uint8_t *tmp_ptr = new uint8_t[size()];
        format::Symbol *symbol = reinterpret_cast<format::Symbol *>(tmp_ptr);
        symbol->index = htonl(_M_index);
        symbol->length = htons(_M_name.size());
        symbol->type = _M_type;
        copy(_M_name.begin(), _M_name.end(), symbol->name);
        return tmp_ptr;
      }

      //
      // A ProgramHelper class.
      //

      void *ProgramHelper::ptr() const
      {
        uint8_t *tmp_ptr = new uint8_t[size()];
        void *ptr = reinterpret_cast<void *>(tmp_ptr);
        format::Header *header = reinterpret_cast<format::Header *>(tmp_ptr);
        copy(format::HEADER_MAGIC, format::HEADER_MAGIC + 8, header->magic);
        tmp_ptr += align(sizeof(format::Header), 8);
        header->entry = htonl(_M_entry);
        header->flags = _M_flags;
        header->fun_count = htonl(_M_funs.size());
        header->var_count = htonl(_M_vars.size());
        header->code_size = htonl(_M_instrs.size());
        header->data_size = htonl(_M_data_size);
        header->reloc_count = htonl(_M_relocs.size());
        header->symbol_count = htonl(_M_symbol_pairs.size());

        format::Function *funs = reinterpret_cast<format::Function *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Function) * _M_funs.size(), 8);
        copy(_M_funs.begin(), _M_funs.end(), funs);

        format::Value *vars = reinterpret_cast<format::Value *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Value) * _M_vars.size(), 8);
        copy(_M_vars.begin(), _M_vars.end(), vars);

        format::Instruction *code = reinterpret_cast<format::Instruction *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Instruction) * _M_instrs.size(), 8);
        copy(_M_instrs.begin(), _M_instrs.end(), code);

        for(auto &pair : _M_object_pairs) {
          copy_n(pair.ptr.get(), pair.size, tmp_ptr);
          tmp_ptr += align(pair.size, 8);
        }

        format::Relocation *relocs = reinterpret_cast<format::Relocation *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Relocation) * _M_relocs.size(), 8);
        copy(_M_relocs.begin(), _M_relocs.end(), relocs);

        for(auto &pair : _M_symbol_pairs) {
          copy_n(pair.ptr.get(), pair.size, tmp_ptr);
          tmp_ptr += align(pair.size, 8);
        }
        return ptr;
      }
      
      size_t ProgramHelper::size() const
      {
        size_t prog_size = align(sizeof(format::Header), 8) +
                           align(_M_funs.size() * sizeof(format::Function), 8) +
                           align(_M_vars.size() * sizeof(format::Value), 8) +
                           align(_M_instrs.size() * sizeof(format::Instruction), 8) +
                           accumulate(_M_object_pairs.begin(), _M_object_pairs.end(), 0, [](size_t x, const Pair & p) {
                             return x + align(p.size, 8);
                           });
        if((_M_flags & format::HEADER_FLAG_RELOCATABLE) != 0) {
          prog_size += align(_M_relocs.size() * sizeof(format::Relocation), 8) +
                       accumulate(_M_symbol_pairs.begin(), _M_symbol_pairs.end(), 0, [](size_t x, const Pair & p) {
                         return x + align(p.size, 8);
                       });
        }
        return prog_size;
      }

      //
      // Other functions.
      //

      format::Function make_function(uint32_t arg_count, uint32_t addr, uint32_t instr_count)
      {

        format::Function fun;
        fun.addr = htonl(addr);
        fun.arg_count = htonl(arg_count);
        fun.instr_count = htonl(instr_count);
        return fun;
      }

      format::Instruction make_instruction(uint32_t instr, uint32_t op, const Argument &arg1, const Argument &arg2, uint32_t local_var_count)
      {
        format::Instruction tmp_instr;
        tmp_instr.opcode = htonl(opcode::opcode(instr, op, arg1.type, arg2.type, local_var_count));
        tmp_instr.arg1.i = htonl(arg1.format_arg.i);
        tmp_instr.arg2.i = htonl(arg2.format_arg.i);
        return tmp_instr;
      }

      format::Value make_int_value(int i)
      {
        format::Value value;
        value.type = htonl(VALUE_TYPE_INT);
        value.__pad = 0;
        value.i = htonll(i);
        return value;
      }

      format::Value make_int_value(int64_t i)
      {
        format::Value value;
        value.type = htonl(VALUE_TYPE_INT);
        value.__pad = 0;
        value.i = htonll(i);
        return value;
      }

      format::Value make_float_value(double f)
      {
        format::Value value;
        value.type = htonl(VALUE_TYPE_FLOAT);
        value.f = double_to_format_double(f);
        value.__pad = 0;
        value.f.dword = htonll(value.f.dword);
        return value;
      }

      format::Value make_ref_value(uint32_t addr)
      {
        format::Value value;
        value.type = htonl(VALUE_TYPE_REF);
        value.__pad = 0;
        value.addr = htonll(addr);
        return value;
      }

      format::Relocation make_reloc(uint32_t type, uint32_t addr, uint32_t symbol)
      {
        format::Relocation reloc;
        reloc.type = htonl(type);
        reloc.addr = htonl(addr);
        reloc.symbol = htonl(symbol);
        return reloc;
      }
    }
  }
}
