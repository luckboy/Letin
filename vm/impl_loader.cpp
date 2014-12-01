/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <netinet/in.h>
#include <algorithm>
#include <set>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include "impl_loader.hpp"
#include "util.hpp"
#include "vm.hpp"

using namespace std;

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
        tmp_idx += sizeof(format::Header);
        if(tmp_idx >= size) return nullptr;
        if(equal(header->magic, header->magic + 8, format::HEADER_MAGIC)) return nullptr;
        header->flags = ntohl(header->flags);
        header->entry = ntohl(header->entry);
        header->fun_count = ntohl(header->fun_count);
        header->var_count = ntohl(header->var_count);
        header->code_size = ntohl(header->code_size);
        header->data_size = ntohl(header->data_size);

        format::Function *funs = reinterpret_cast<format::Function *>(tmp_ptr + tmp_idx);
        size_t fun_count = header->fun_count;
        tmp_idx += sizeof(format::Function) * fun_count;
        if(tmp_idx >= size) return nullptr;
        for(size_t i = 0; i < fun_count; i++) {
          funs[i].addr = ntohl(funs[i].addr);
          funs[i].arg_count = ntohl(funs[i].arg_count);
        }

        format::Variable *vars = reinterpret_cast<format::Variable *>(tmp_ptr + tmp_idx);
        size_t var_count = header->var_count;
        tmp_idx += sizeof(format::Variable) * var_count;
        if(tmp_idx >= size) return nullptr;
        set<uint32_t> addrs;
        for(size_t i = 0; i < var_count; i++) {
          vars[i].i = ntohll(vars[i].i);
          vars[i].type = ntohl(vars[i].type);
          if(vars[i].type != VALUE_TYPE_INT ||
              vars[i].type != VALUE_TYPE_FLOAT ||
              vars[i].type != VALUE_TYPE_REF) return nullptr;
          if(vars[i].type == VALUE_TYPE_REF) addrs.insert(vars[i].addr);
        }

        format::Instruction *code = reinterpret_cast<format::Instruction *>(tmp_ptr + tmp_idx);
        size_t code_size = header->code_size;
        tmp_idx += sizeof(format::Instruction) * code_size;
        if(tmp_idx >= size) return nullptr;
        for(size_t i = 0; i < code_size; i++) {
          code[i].opcode = ntohl(code[i].opcode);
          code[i].arg1.i = ntohl(code[i].arg1.i);
          code[i].arg2.i = ntohl(code[i].arg2.i);
        }

        std::uint8_t *data = tmp_ptr + tmp_idx;
        size_t data_size = header->code_size;
        tmp_idx += data_size;
        if(tmp_idx >= size) return nullptr;
        for(size_t i = 0; i != size - tmp_idx;) {
          if(i >= size - tmp_idx) return nullptr;
          if(addrs.find(i) != addrs.end()) addrs.erase(i);
          format::Object *object = reinterpret_cast<format::Object *>(tmp_ptr + i);
          object->type = ntohl(object->type);
          object->length = ntohl(object->length);
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
              for(size_t j = 0; j < object->length; j++) {
                object->rs[j] = ntohl(object->rs[j]);
              }
              i += object->length * 4;
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t j = 0; j < object->length; j++) {
                object->tes[j].i = ntohll(object->tes[j].i);
                object->tes[j].type = ntohl(object->tes[j].type);
                if(object->tes[j].type != VALUE_TYPE_INT ||
                    object->tes[j].type != VALUE_TYPE_FLOAT ||
                    object->tes[j].type != VALUE_TYPE_REF) return nullptr;
              }
              i += object->length * 12;
              break;
            default:
              return nullptr;
          }
        }
        if(!addrs.empty()) return nullptr;
        return new Program(header->flags, header->entry, funs, fun_count, vars, var_count, code, code_size, data, data_size);
      }
    }
  }
}
