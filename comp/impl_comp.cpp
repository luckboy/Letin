/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <netinet/in.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include "driver.hpp"
#include "impl_comp.hpp"
#include "impl_prog.hpp"
#include "parse_tree.hpp"
#include "parser.hpp"
#include "util.hpp"

using namespace std;
using namespace letin::util;

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      //
      // Static functions.
      //

      static ParseTree *parse(const vector<Source> &sources, vector<Error> &errors)
      {
        ParseTree *tree = new ParseTree();
        Driver driver(*tree, errors);
        for(auto source : sources) {
          if(!driver.parse(source)) {
            delete tree;
            return nullptr;
          }
        }
        return tree;
      }

      struct UngeneratedProgram
      {
        unordered_map<string, pair<uint32_t, Function>> fun_pairs;
        unordered_map<string, pair<uint32_t, Value>> var_pairs;
        unordered_map<const Object *, pair<int, uint32_t>> object_pairs;
        unordered_map<const Object *, uint32_t> object_addrs;
      };

      struct UngeneratedFunction
      {
        format::Instruction *instrs;
        unordered_map<string, uint32_t> instr_addrs;
      };

      static bool check_object_elems(const Object *object, Value::Type type, vector<Error> &errors)
      {
        for(auto & elem : object->elems()) {
          if(elem.type() != Value::TYPE_INT) {
            errors.push_back(Error(elem.pos(), "incorrect value"));
            return false;
          }
        }
        return true;
      }

      static bool add_object_pairs_from_object(UngeneratedProgram &ungen_prog, const Object *object, vector<Error> &errors)
      {
        size_t header_size = sizeof(format::Object) - 8;
        if(object->type() == "iarray8") {
          if(!check_object_elems(object, Value::TYPE_INT, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_IARRAY8, header_size + object->elems().size())));
        } else if(object->type() == "iarray16") {
          if(!check_object_elems(object, Value::TYPE_INT, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_IARRAY16, header_size + object->elems().size() * 2)));
        } else if(object->type() == "iarray32") {
          if(!check_object_elems(object, Value::TYPE_INT, errors) ||
              !check_object_elems(object, Value::TYPE_FUN_ADDR, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_IARRAY32, header_size + object->elems().size() * 4)));
        } else if(object->type() == "iarray64") {
          if(!check_object_elems(object, Value::TYPE_INT, errors) ||
              !check_object_elems(object, Value::TYPE_FUN_ADDR, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_IARRAY64, header_size + object->elems().size() * 8)));
        } else if(object->type() == "sfarray") {
          if(!check_object_elems(object, Value::TYPE_FLOAT, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_SFARRAY, header_size + object->elems().size() * 4)));
        } else if(object->type() == "dfarray") {
          if(!check_object_elems(object, Value::TYPE_FLOAT, errors)) return false;
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_DFARRAY, header_size + object->elems().size() * 8)));
        } else if(object->type() == "rarray") {
          for(auto & elem : object->elems()) {
            if(elem.type() != Value::TYPE_REF) {
              errors.push_back(Error(elem.pos(), "incorrect value"));
              return false;
            }
            if(!add_object_pairs_from_object(ungen_prog, elem.object(), errors)) return false;
          }
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_RARRAY, header_size + object->elems().size() * 4)));
        } else if(object->type() == "tuple") {
          for(auto & elem : object->elems()) {
            if(elem.type() == Value::TYPE_REF)
              if(!add_object_pairs_from_object(ungen_prog, elem.object(), errors)) return false;
          }
          ungen_prog.object_pairs.insert(make_pair(object, make_pair(OBJECT_TYPE_TUPLE, header_size + object->elems().size() * 9)));
        } else {
          errors.push_back(Error(object->pos(), "incorrect object type"));
          return false;
        }
        return true;
      }

      static size_t prog_size_and_other_sizes(const UngeneratedProgram &ungen_prog, size_t &code_size, size_t &data_size)
      {
        size_t size = align(sizeof(format::Header), 8);
        size += align(ungen_prog.fun_pairs.size() * sizeof(format::Function), 8);
        size += align(ungen_prog.var_pairs.size() * sizeof(format::Value), 8);
        code_size = 0;
        for(auto pair : ungen_prog.fun_pairs) {
          const Function &fun = pair.second.second;
          for(auto line : fun.lines())
            if(line.instr() != nullptr) code_size++;
        }
        size += align(code_size * sizeof(format::Instruction), 8);
        data_size = 0;
        for(auto pair : ungen_prog.object_pairs)
          data_size += align(pair.second.second, 8);
        size += data_size;
        return size;
      }

      static bool get_fun_addr(const UngeneratedProgram &ungen_prog, uint32_t &addr, const string &ident, const Position &pos, vector<Error> &errors)
      {
        auto iter = ungen_prog.fun_pairs.find(ident);
        if(iter == ungen_prog.fun_pairs.end()) {
          errors.push_back(Error(pos, "undefined function " + ident));
          return false;
        }
        addr = iter->second.first;
        return true;
      }

      static bool get_var_addr(const UngeneratedProgram &ungen_prog, uint32_t &addr, const string &ident, const Position &pos, vector<Error> &errors)
      {
        auto iter = ungen_prog.var_pairs.find(ident);
        if(iter == ungen_prog.var_pairs.end()) {
          errors.push_back(Error(pos, "undefined variable " + ident));
          return false;
        }
        addr = iter->second.first;
        return true;
      }

      static bool value_to_format_value(const UngeneratedProgram &ungen_prog, const Value &value, format::Value &format_value, vector<Error> &errors)
      {
        switch(value.type()) {
          case Value::TYPE_INT:
            format_value.type = VALUE_TYPE_INT;
            format_value.i = value.i();
            return true;
          case Value::TYPE_FLOAT:
            format_value.type = VALUE_TYPE_FLOAT;
            format_value.f = double_to_format_double(value.f());
            return true;
          case Value::TYPE_REF:
          {
            auto iter = ungen_prog.object_addrs.find(value.object());
            format_value.type - VALUE_TYPE_REF;
            format_value.addr = iter->second;
            return true;
          }
          case Value::TYPE_FUN_ADDR:
          {
            format_value.type = VALUE_TYPE_INT;
            uint32_t addr;
            if(!get_fun_addr(ungen_prog, addr, value.fun(), value.pos(), errors)) return false;
            format_value.i = addr;
            return true;
          }
        }
      }

      static bool generate_instr(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &fun, uint32_t ip, Instruction &instr, vector<Error> &errors)
      {
        return false;
      }

      static Program *generate_prog(const ParseTree &tree, vector<Error> &errors)
      {
        UngeneratedProgram ungen_prog;
        string entry_ident;
        bool is_entry = false;
        Position entry_pos;
        size_t code_size, data_size;
        bool is_success = true;
        for(auto & def : tree.defs()) {
          FunctionDefinition *fun_def = dynamic_cast<FunctionDefinition *>(def.get());
          if(fun_def != nullptr) {
            if(ungen_prog.fun_pairs.find(fun_def->ident()) != ungen_prog.fun_pairs.end()) {
              ungen_prog.fun_pairs.insert(make_pair(fun_def->ident(), make_pair(ungen_prog.fun_pairs.size(), fun_def->fun())));
            } else {
              errors.push_back(Error(fun_def->pos(), "already defined function " + fun_def->ident()));
              is_success = false;
            }
          }
          VariableDefinition *var_def = dynamic_cast<VariableDefinition *>(def.get());
          if(var_def != nullptr) {
            if(ungen_prog.var_pairs.find(fun_def->ident()) != ungen_prog.var_pairs.end()) {
              ungen_prog.var_pairs.insert(make_pair(var_def->ident(), make_pair(ungen_prog.var_pairs.size(), var_def->value())));
            } else {
              errors.push_back(Error(fun_def->pos(), "already defined variable " + fun_def->ident()));
              is_success = false;
            }
          }
          EntryDefinition *entry_def = dynamic_cast<EntryDefinition *>(def.get());
          if(entry_def != nullptr) {
            if(!is_entry) {
              entry_ident = entry_def->ident();
              is_entry = true;
              entry_pos = entry_def->pos();
            } else {
              errors.push_back(Error(entry_def->pos(), "already defined entry"));
              is_success = false;
            }
          }
        }
        for(auto pair : ungen_prog.var_pairs) {
          const Value &value = pair.second.second;
          if(value.type() == Value::TYPE_REF)
            is_success &= add_object_pairs_from_object(ungen_prog, value.object(), errors);
        }
        if(!is_success) return nullptr;

        size_t size = prog_size_and_other_sizes(ungen_prog, code_size, data_size);
        unique_ptr<uint8_t []> ptr(new uint8_t[size]);
        uint8_t *tmp_ptr = ptr.get();
        format::Header *header = reinterpret_cast<format::Header *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Header), 8);
        copy(format::HEADER_MAGIC, format::HEADER_MAGIC + 8, header->magic);
        if(is_entry) {
          header->flags = 0;
          if(!get_fun_addr(ungen_prog, header->entry, entry_ident, entry_pos, errors)) return nullptr;
        } else {
          header->flags = htonl(format::HEADER_FLAG_LIBRARY);
          header->entry = 0;
        }
        header->fun_count = htonl(ungen_prog.fun_pairs.size());
        header->var_count = htonl(ungen_prog.var_pairs.size());
        header->code_size = htonl(code_size);
        header->data_size = htonl(data_size);
        fill_n(header->reserved, 4, 0);

        format::Function *funs = reinterpret_cast<format::Function *>(tmp_ptr);
        tmp_ptr += align(ungen_prog.fun_pairs.size() * sizeof(format::Function), 8);
        format::Value *vars = reinterpret_cast<format::Value *>(tmp_ptr);
        tmp_ptr += align(ungen_prog.var_pairs.size() * sizeof(format::Value), 8);
        format::Instruction *code = reinterpret_cast<format::Instruction *>(tmp_ptr);
        tmp_ptr += align(code_size * sizeof(format::Instruction), 8);
        uint8_t *data = tmp_ptr;
        tmp_ptr += align(data_size, 8);

        size_t instr_addr = 0;
        for(auto pair : ungen_prog.fun_pairs) {
          UngeneratedFunction ungen_fun;
          ungen_fun.instr_addrs = unordered_map<string, uint32_t>();
          const Function &fun = pair.second.second;
          ungen_fun.instrs = code + instr_addr;
          size_t instr_count = 0;
          for(auto line : fun.lines()) {
            if(line.label() != nullptr) {
              const Label &label = *(line.label());
              if(ungen_fun.instr_addrs.find(label.ident()) == ungen_fun.instr_addrs.end()) {
                ungen_fun.instr_addrs.insert(make_pair(label.ident(), instr_count));
              } else {
                errors.push_back(Error(label.pos(), "already defined label " + line.label()->ident()));
                return nullptr;
              }
            }
            if(line.instr() != nullptr) instr_count++;
          }
          instr_count = 0;
          for(auto line : fun.lines()) {
            if(line.instr() != nullptr) {
              Instruction instr = *(line.instr());
              if(!generate_instr(ungen_prog, ungen_fun, instr_count, instr, errors)) return nullptr;
              instr_count++;
            }
          }
          funs[pair.second.first].addr = htonl(instr_addr);
          funs[pair.second.first].arg_count = htonl(fun.arg_count());
          funs[pair.second.first].instr_count = htonl(instr_count);
          instr_addr += instr_count;
        }

        size_t i = 0;
        for(auto pair : ungen_prog.object_pairs) {
          const Object *object = pair.first;
          format::Object *format_object = reinterpret_cast<format::Object *>(data + i);
          format_object->type = pair.second.first;
          format_object->length = object->elems().size();
          int j = 0;
          switch(pair.second.first) {
            case OBJECT_TYPE_IARRAY8:
              for(auto & elem : object->elems()) {
                format_object->is8[j] = elem.i();
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY16:
              for(auto & elem : object->elems()) {
                format_object->is16[j] = htons(elem.i());
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY32:
              for(auto & elem : object->elems()) {
                format::Value format_value;
                if(!value_to_format_value(ungen_prog, elem, format_value, errors)) return nullptr;
                format_object->is32[j] = htonl(format_value.i);
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY64:
              for(auto & elem : object->elems()) {
                format::Value format_value;
                if(!value_to_format_value(ungen_prog, elem, format_value, errors)) return nullptr;
                format_object->is64[j] = htonll(format_value.i);
                j++;
              }
              break;
            case OBJECT_TYPE_SFARRAY:
              for(auto & elem : object->elems()) {
                format_object->sfs[j].word = htonl(float_to_format_float(elem.f()).word);
                j++;
              }
              break;
            case OBJECT_TYPE_DFARRAY:
              for(auto & elem : object->elems()) {
                format_object->dfs[j].dword = htonll(double_to_format_double(elem.f()).dword);
                j++;
              }
              break;
            case OBJECT_TYPE_RARRAY:
              for(auto & elem : object->elems()) {
                format::Value format_value;
                if(!value_to_format_value(ungen_prog, elem, format_value, errors)) return nullptr;
                format_object->rs[j] = htonl(format_value.addr);
                j++;
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(auto & elem : object->elems()) {
                format::Value format_value;
                if(!value_to_format_value(ungen_prog, elem, format_value, errors)) return nullptr;
                format_object->tes[j].i = htonl(format_value.i);
                format_object->tuple_elem_types()[j] = format_value.type;
                j++;
              }
              break;
          }
          format_object->type = htonl(format_object->type);
          format_object->length = htonl(format_object->length);
          i += align(pair.second.second, 8);
        }

        for(auto pair : ungen_prog.var_pairs) {
          const Value &value = pair.second.second;
          if(!value_to_format_value(ungen_prog, value, vars[pair.second.first], errors)) return nullptr;
          vars[pair.second.first].type =  htonl(vars[pair.second.first].type);
          vars[pair.second.first].i =  htonll(vars[pair.second.first].i);
        }
        Program *prog = new ImplProgram(reinterpret_cast<void *>(ptr.get()), size);
        ptr.release();
        return prog;
      }

      //
      // An ImplCompiler class.
      //

      ImplCompiler::~ImplCompiler() {}

      Program *ImplCompiler::compile(const vector<Source> &sources, vector<Error> &errors)
      {
        unique_ptr<ParseTree> tree(parse(sources, errors));
        if(tree.get() != nullptr) return nullptr;
        return generate_prog(*tree, errors);
      }
    }
  }
}
