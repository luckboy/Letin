/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
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
        tmp_idx += align(sizeof(format::Function) * fun_count, 8);
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < fun_count; i++) {
          funs[i].addr = ntohl(funs[i].addr);
          funs[i].arg_count = ntohl(funs[i].arg_count);
          funs[i].instr_count = ntohl(funs[i].instr_count);
        }

        format::Value *vars = reinterpret_cast<format::Value *>(tmp_ptr + tmp_idx);
        size_t var_count = header->var_count;
        tmp_idx += align(sizeof(format::Value) * var_count, 8);
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
        tmp_idx += align(sizeof(format::Instruction) * code_size, 8);
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < code_size; i++) {
          code[i].opcode = ntohl(code[i].opcode);
          code[i].arg1.i = ntohl(code[i].arg1.i);
          code[i].arg2.i = ntohl(code[i].arg2.i);
        }

        uint8_t *data = tmp_ptr + tmp_idx;
        size_t data_size = header->data_size;
        set<uint32_t> data_addrs;
        tmp_idx += align(data_size, 8);
        if(tmp_idx > size) return nullptr;
        for(size_t i = 0; i < data_size;) {
          if(var_addrs.find(i) != var_addrs.end()) var_addrs.erase(i);
          data_addrs.insert(i);
          format::Object *object = reinterpret_cast<format::Object *>(data + i);
          object->type = ntohl(object->type);
          object->length = ntohl(object->length);
          i += sizeof(format::Object) - 8;
          switch(object->type) {
            case OBJECT_TYPE_IARRAY8:
              i += object->length;
              break;
            case OBJECT_TYPE_IARRAY16:
              for(size_t j = 0; j < object->length; j++) object->is16[j] = ntohs(object->is16[j]);
              i += object->length * 2;
              break;
            case OBJECT_TYPE_IARRAY32:
              for(size_t j = 0; j < object->length; j++) object->is32[j] = ntohl(object->is32[j]);
              i += object->length * 4;
              break;
            case OBJECT_TYPE_IARRAY64:
              for(size_t j = 0; j < object->length; j++) object->is64[j] = ntohll(object->is64[j]);
              i += object->length * 8;
              break;
            case OBJECT_TYPE_SFARRAY:
              for(size_t j = 0; j < object->length; j++) object->sfs[j].word = ntohl(object->sfs[j].word);
              i += object->length * 4;
              break;
            case OBJECT_TYPE_DFARRAY:
              for(size_t j = 0; j < object->length; j++) object->dfs[j].dword = ntohll(object->dfs[j].dword);
              i += object->length * 8;
              break;
            case OBJECT_TYPE_RARRAY:
              for(size_t j = 0; j < object->length; j++) object->rs[j] = ntohl(object->rs[j]);
              i += object->length * 4;
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t j = 0; j < object->length; j++) {
                object->tes[j].i = ntohll(object->tes[j].i);
                if(object->tuple_elem_types()[j] != VALUE_TYPE_INT &&
                    object->tuple_elem_types()[j] != VALUE_TYPE_FLOAT &&
                    object->tuple_elem_types()[j] != VALUE_TYPE_REF) return nullptr;
              }
              i += object->length * 9;
              break;
            default:
              return nullptr;
          }
          if(i > data_size) return nullptr;
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
          tmp_idx += align(sizeof(format::Relocation) * reloc_count, 8);
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
                if((relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG1_NATIVE_FUN &&
                    (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ARG2_NATIVE_FUN &&
                    (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_ELEM_NATIVE_FUN &&
                    (relocs[i].type & ~format::RELOC_TYPE_SYMBOLIC) != format::RELOC_TYPE_VAR_NATIVE_FUN)
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
            if(tmp_idx + symbol_size > size) return nullptr;
            if((symbol->type & ~format::SYMBOL_TYPE_DEFINED) != format::SYMBOL_TYPE_FUN &&
                (symbol->type & ~format::SYMBOL_TYPE_DEFINED) != format::SYMBOL_TYPE_VAR) {
              if((header->flags & format::HEADER_FLAG_NATIVE_FUN_SYMBOLS) != 0) {
                if((symbol->type & ~format::SYMBOL_TYPE_DEFINED) != format::SYMBOL_TYPE_NATIVE_FUN)
                  return nullptr;
              } else
                return nullptr;
            }
            if(reloc_symbol_idxs.find(j) != reloc_symbol_idxs.end()) reloc_symbol_idxs.erase(j);
            symbol_offsets.push_back(i);
            symbol->index = ntohl(symbol->index);
            symbol->length = ntohs(symbol->length);
            symbol_size += symbol->length;
            tmp_idx += (j + 1 < symbol_count ? align(symbol_size, 8) : symbol_size);
            if(tmp_idx > size) return nullptr;
            i += align(symbol_size, 8);
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
          tmp_idx = align(tmp_idx, 8);
          fun_infos = reinterpret_cast<format::FunctionInfo *>(tmp_ptr + tmp_idx);
          fun_info_count = header->fun_info_count;
          tmp_idx += align(sizeof(format::FunctionInfo) * fun_info_count, 8);
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
