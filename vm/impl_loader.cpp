/****************************************************************************
 *   Copyright (C) 2014-2015, 2017 Łukasz Szpakowski.                       *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <algorithm>
#include <set>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include "impl_loader.hpp"
#include "util.hpp"
#include "vm.hpp"

using namespace std;
using namespace letin::util;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      ImplLoader::~ImplLoader() {}

      Program *ImplLoader::load(void *ptr, size_t size)
      {
        uint8_t *tmp_ptr = reinterpret_cast<uint8_t *>(ptr);
        size_t tmp_idx = 0;
        size_t tmp_idx2;
        format::Header *header = reinterpret_cast<format::Header *>(tmp_ptr + tmp_idx);
        tmp_idx += align(sizeof(format::Header), 8);
        if(tmp_idx > size) return nullptr;
        if(!equal(header->magic, header->magic + 8, format::HEADER_MAGIC)) return nullptr;
        header->flags = ntohl(header->flags);
        header->entry = ntohl(header->entry);
        header->fun_count = ntohl(header->fun_count);
        header->var_count = ntohl(header->var_count);
        header->code_size = ntohl(header->code_size);
        header->data_size = ntohl(header->data_size);
        header->reloc_count = ntohl(header->reloc_count);
        header->symbol_count = ntohl(header->symbol_count);

        format::Function *funs = reinterpret_cast<format::Function *>(tmp_ptr + tmp_idx);
        size_t fun_count = header->fun_count;
        size_t fun_array_size = sizeof(format::Function) * fun_count;
        if(fun_array_size / sizeof(format::Function) != fun_count) return nullptr;
        if(fun_array_size != 0 && align(fun_array_size, 8) == 0) return nullptr; 
        tmp_idx2 = tmp_idx + align(fun_array_size, 8);
        if((tmp_idx | align(fun_array_size, 8)) > tmp_idx2) return nullptr;
        tmp_idx = tmp_idx2;
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < fun_count; i++) {
          funs[i].addr = ntohl(funs[i].addr);
          funs[i].arg_count = ntohl(funs[i].arg_count);
          funs[i].instr_count = ntohl(funs[i].instr_count);
          if(funs[i].addr >= header->code_size) return nullptr;
          size_t instr_end_addr = funs[i].addr + funs[i].instr_count;
          if((funs[i].addr | funs[i].instr_count) > instr_end_addr) return nullptr;
          if(instr_end_addr > header->code_size) return nullptr;
        }

        format::Value *vars = reinterpret_cast<format::Value *>(tmp_ptr + tmp_idx);
        size_t var_count = header->var_count;
        size_t var_array_size = sizeof(format::Value) * var_count;
        if(var_array_size / sizeof(format::Value) != var_count) return nullptr;
        if(var_array_size != 0 && align(var_array_size, 8) == 0) return nullptr; 
        tmp_idx2 = tmp_idx + align(var_array_size, 8);
        if((tmp_idx | align(var_array_size, 8)) > tmp_idx2) return nullptr;
        tmp_idx = tmp_idx2;
        if(tmp_idx > size) return nullptr;
        set<uint32_t> var_addrs;
        for(size_t i = 0; i < var_count; i++) {
          vars[i].type = ntohl(vars[i].type);
          vars[i].i = ntohll(vars[i].i);
          if(vars[i].type != VALUE_TYPE_INT &&
              vars[i].type != VALUE_TYPE_FLOAT &&
              vars[i].type != VALUE_TYPE_REF) return nullptr;
          if(vars[i].type == VALUE_TYPE_REF) var_addrs.insert(vars[i].addr);
        }

        format::Instruction *code = reinterpret_cast<format::Instruction *>(tmp_ptr + tmp_idx);
        size_t code_size = header->code_size;
        size_t instr_array_size = sizeof(format::Instruction) * code_size;
        if(instr_array_size / sizeof(format::Instruction) != code_size) return nullptr;
        if(instr_array_size != 0 && align(instr_array_size, 8) == 0) return nullptr;
        tmp_idx2 = tmp_idx + align(instr_array_size, 8);
        if((tmp_idx | align(instr_array_size, 8)) > tmp_idx2) return nullptr;
        tmp_idx = tmp_idx2;
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < code_size; i++) {
          code[i].opcode = ntohl(code[i].opcode);
          code[i].arg1.i = ntohl(code[i].arg1.i);
          code[i].arg2.i = ntohl(code[i].arg2.i);
        }

        uint8_t *data = tmp_ptr + tmp_idx;
        size_t data_size = header->data_size;
        set<uint32_t> data_addrs;
        if(data_size != 0 && align(data_size, 8) == 0) return nullptr;
        tmp_idx2 = tmp_idx + align(data_size, 8);
        if((tmp_idx | align(data_size, 8)) > tmp_idx2) return nullptr;
        tmp_idx = tmp_idx2;
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < data_size;) {
          size_t tmp_i, elem_size;
          if(var_addrs.find(i) != var_addrs.end()) var_addrs.erase(i);
          data_addrs.insert(i);
          format::Object *object = reinterpret_cast<format::Object *>(data + i);
          object->type = ntohl(object->type);
          object->length = ntohl(object->length);
          tmp_i = i + sizeof(format::Object) - 8;
          if((i | (sizeof(format::Object) - 8)) > tmp_i) return nullptr;
          i = tmp_i;
          switch(object->type) {
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
            default:
              return nullptr;
          }
          size_t elem_array_size = object->length * elem_size;
          if(elem_array_size / elem_size != object->length) return nullptr;
          tmp_i = i + elem_array_size;
          if((i | elem_array_size) > tmp_i) return nullptr;
          if(tmp_i > data_size) return nullptr;
          switch(object->type) {
            case OBJECT_TYPE_IARRAY8:
              break;
            case OBJECT_TYPE_IARRAY16:
              for(size_t j = 0; j < object->length; j++) object->is16[j] = ntohs(object->is16[j]);
              break;
            case OBJECT_TYPE_IARRAY32:
              for(size_t j = 0; j < object->length; j++) object->is32[j] = ntohl(object->is32[j]);
              break;
            case OBJECT_TYPE_IARRAY64:
              for(size_t j = 0; j < object->length; j++) object->is64[j] = ntohll(object->is64[j]);
              break;
            case OBJECT_TYPE_SFARRAY:
              for(size_t j = 0; j < object->length; j++) object->sfs[j].word = ntohl(object->sfs[j].word);
              break;
            case OBJECT_TYPE_DFARRAY:
              for(size_t j = 0; j < object->length; j++) object->dfs[j].dword = ntohll(object->dfs[j].dword);
              break;
            case OBJECT_TYPE_RARRAY:
              for(size_t j = 0; j < object->length; j++) object->rs[j] = ntohl(object->rs[j]);
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t j = 0; j < object->length; j++) {
                object->tes[j].i = ntohll(object->tes[j].i);
                if(object->tuple_elem_types()[j] != VALUE_TYPE_INT &&
                    object->tuple_elem_types()[j] != VALUE_TYPE_FLOAT &&
                    object->tuple_elem_types()[j] != VALUE_TYPE_REF) return nullptr;
              }
              break;
            default:
              return nullptr;
          }
          i = tmp_i;
          if(i != 0 && align(i, 8) == 0) return nullptr;
          i = align(i, 8);
        }
        if(!var_addrs.empty()) return nullptr;

        format::Relocation *relocs;
        size_t reloc_count;
        uint8_t *symbols;
        size_t symbol_count;
        vector<uint32_t> symbol_offsets;
        if((header->flags & format::HEADER_FLAG_RELOCATABLE) != 0) {
          relocs = reinterpret_cast<format::Relocation *>(tmp_ptr + tmp_idx);
          reloc_count = header->reloc_count;
          size_t reloc_array_size = sizeof(format::Relocation) * reloc_count;
          if(reloc_array_size / sizeof(format::Relocation) != reloc_count) return nullptr;
          if(reloc_array_size != 0  && align(reloc_array_size, 8) == 0) return nullptr;
          tmp_idx2 = tmp_idx + align(reloc_array_size, 8);
          if((tmp_idx | align(reloc_array_size, 8)) > tmp_idx2) return nullptr;
          tmp_idx = tmp_idx2;
          set<uint32_t> reloc_symbol_idxs;
          if(tmp_idx > size) return nullptr;
          for(size_t i = 0; i < reloc_count; i++) {
            relocs[i].type = ntohl(relocs[i].type);
            relocs[i].addr = ntohl(relocs[i].addr);
            relocs[i].symbol = ntohl(relocs[i].symbol);
            if((relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG1_FUN &&
                (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG2_FUN &&
                (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG1_VAR &&
                (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG2_VAR &&
                (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ELEM_FUN &&
                (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_VAR_FUN) {
              if((header->flags & format::HEADER_FLAG_NATIVE_FUN_SYMBOLS) != 0) {
                if(relocs[i].type != (format::RELOC_TYPE_ARG1_NATIVE_FUN | format::RELOC_TYPE_SYMBOLIC) &&
                    relocs[i].type != (format::RELOC_TYPE_ARG2_NATIVE_FUN | format::RELOC_TYPE_SYMBOLIC) &&
                    relocs[i].type != (format::RELOC_TYPE_ELEM_NATIVE_FUN | format::RELOC_TYPE_SYMBOLIC) &&
                    relocs[i].type != (format::RELOC_TYPE_VAR_NATIVE_FUN | format::RELOC_TYPE_SYMBOLIC))
                  return nullptr;
              } else
                return nullptr;
            }
            if((relocs[i].type & format::RELOC_TYPE_SYMBOLIC) != 0) reloc_symbol_idxs.insert(relocs[i].symbol);
          }

          symbols = tmp_ptr + tmp_idx;
          symbol_count = header->symbol_count;
          for(size_t i = 0, j = 0; j < symbol_count; j++) {
            format::Symbol *symbol = reinterpret_cast<format::Symbol *>(symbols + i);
            size_t symbol_size = sizeof(format::Symbol) - 1;
            size_t tmp_symbol_size, tmp_i;
            if(tmp_idx + symbol_size > size) return nullptr;
            if((symbol->type & ~format::SYMBOL_TYPE_DEFINED) != format::SYMBOL_TYPE_FUN &&
                (symbol->type & ~format::SYMBOL_TYPE_DEFINED) != format::SYMBOL_TYPE_VAR) {
              if((header->flags & format::HEADER_FLAG_NATIVE_FUN_SYMBOLS) != 0) {
                if(symbol->type != format::SYMBOL_TYPE_NATIVE_FUN) return nullptr;
              } else
                return nullptr;
            }
            if(reloc_symbol_idxs.find(j) != reloc_symbol_idxs.end()) reloc_symbol_idxs.erase(j);
            symbol_offsets.push_back(i);
            symbol->index = ntohl(symbol->index);
            symbol->length = ntohs(symbol->length);
            tmp_symbol_size = symbol_size + symbol->length;
            if((symbol_size | symbol->length) > tmp_symbol_size) return nullptr;
            symbol_size = tmp_symbol_size;
            if(j + 1 < symbol_count) {
              if(symbol_size != 0 && align(symbol_size, 8) == 0) return nullptr;
              tmp_symbol_size = align(symbol_size, 8);
            } else
              tmp_symbol_size = symbol_size;
            tmp_idx2 = tmp_idx + tmp_symbol_size;
            if((tmp_idx | tmp_symbol_size) > tmp_idx2) return nullptr;
            tmp_idx = tmp_idx2;
            if(tmp_idx > size) return nullptr;
            if(symbol_size != 0 && align(symbol_size, 8) == 0) return nullptr;
            tmp_i = i + align(symbol_size, 8);
            if((i | align(symbol_size, 8)) > tmp_i) return nullptr;
            i = tmp_i;
          }
          if(tmp_idx > size) return nullptr;
          if(!reloc_symbol_idxs.empty()) return nullptr;
        } else {
          relocs = reinterpret_cast<format::Relocation *>(tmp_ptr + tmp_idx);
          reloc_count = 0;
          symbols = tmp_ptr + tmp_idx;
          symbol_count = 0;
        }

        format::FunctionInfo *fun_infos;
        size_t fun_info_count;
        if((header->flags & format::HEADER_FLAG_FUN_INFOS) != 0) {
          if(tmp_idx != 0 && align(tmp_idx, 8) == 0) return nullptr;
          tmp_idx = align(tmp_idx, 8);
          fun_infos = reinterpret_cast<format::FunctionInfo *>(tmp_ptr + tmp_idx);
          fun_info_count = ntohl(header->fun_info_count);
          size_t fun_info_array_size = sizeof(format::FunctionInfo) * fun_info_count;
          if(fun_info_array_size / sizeof(format::FunctionInfo) != fun_info_count) return nullptr;
          if(fun_info_array_size != 0 && align(fun_info_array_size, 8) == 0) return nullptr;
          tmp_idx2 = tmp_idx + align(fun_info_array_size, 8);
          if((tmp_idx | align(fun_info_array_size, 8)) > tmp_idx2) return nullptr;
          tmp_idx = tmp_idx2;
          if(tmp_idx > size) return nullptr;
          for(size_t i = 0; i < fun_info_count; i++) {
            fun_infos[i].fun_index = ntohl(fun_infos[i].fun_index);
            if(fun_infos[i].fun_index >= fun_count) return nullptr;
          }
        } else {
          fun_infos = reinterpret_cast<format::FunctionInfo *>(tmp_ptr + tmp_idx);
          fun_info_count = 0;
        }
        return new Program(header->flags, header->entry, funs, fun_count, vars, var_count, code, code_size, data, data_size, data_addrs, relocs, reloc_count, symbols, symbol_count, symbol_offsets, fun_infos, fun_info_count);
      }
    }
  }
}
