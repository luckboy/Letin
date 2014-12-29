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
#include <climits>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <letin/const.hpp>
#include <letin/format.hpp>
#include <letin/opcode.hpp>
#include "driver.hpp"
#include "impl_comp.hpp"
#include "impl_prog.hpp"
#include "parse_tree.hpp"
#include "parser.hpp"
#include "util.hpp"

using namespace std;
using namespace letin::opcode;
using namespace letin::util;

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      //
      // Operation descriptions.
      //

      struct OperationDescription
      {
        int32_t op;
        int arg_value_type1;
        int arg_value_type2;
      };

      static unordered_map<string, OperationDescription> op_descs {
        { "iload",      { OP_ILOAD,     VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "iload2",     { OP_ILOAD2,    VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ineg",       { OP_INEG,      VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "iadd",       { OP_IADD,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "isub",       { OP_ISUB,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "imul",       { OP_IMUL,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "idiv",       { OP_IDIV,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "imod",       { OP_IMOD,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "inot",       { OP_INOT,      VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "iand",       { OP_IAND,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ior",        { OP_IOR,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ixor",       { OP_IXOR,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ishl",       { OP_ISHL,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ishr",       { OP_ISHR,      VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ishru",      { OP_ISHRU,     VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ieq",        { OP_IEQ,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ine",        { OP_INE,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ilt",        { OP_ILT,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ige",        { OP_IGE,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "igt",        { OP_IGT,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ile",        { OP_ILE,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "fload",      { OP_FLOAD,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fload2",     { OP_FLOAD2,    VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fneg",       { OP_FNEG,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fadd",       { OP_FADD,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fsub",       { OP_FDIV,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fmul",       { OP_FMUL,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fdiv",       { OP_FDIV,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "feq",        { OP_FEQ,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fne",        { OP_FNE,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "flt",        { OP_FLT,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fge",        { OP_FGE,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fgt",        { OP_FGT,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fle",        { OP_FLE,       VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "rload",      { OP_RLOAD,     VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "req",        { OP_REQ,       VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rne",        { OP_RNE,       VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "riarray8",   { OP_RIARRAY8,  VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "riarray16",  { OP_RIARRAY16, VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "riarray32",  { OP_RIARRAY32, VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "riarray64",  { OP_RIARRAY64, VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "rsfarray",   { OP_RSFARRAY,  VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "rdfarray",   { OP_RDFARRAY,  VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "rrarray",    { OP_RRARRAY,   VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "rtuple",     { OP_RTUPLE,    VALUE_TYPE_ERROR,       VALUE_TYPE_ERROR } },
        { "rianth8",    { OP_RIANTH8,   VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rianth16",   { OP_RIANTH16,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rianth32",   { OP_RIANTH32,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rianth64",   { OP_RIANTH64,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rsfanth",    { OP_RSFANTH,   VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rdfanth",    { OP_RDFANTH,   VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rranth",     { OP_RRANTH,    VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rtnth",      { OP_RTNTH,     VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rialen8",    { OP_RIALEN8,   VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rialen16",   { OP_RIALEN16,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rialen32",   { OP_RIALEN32,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rialen64",   { OP_RIALEN64,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rsfalen",    { OP_RSFALEN,   VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rdfalen",    { OP_RDFALEN,   VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rralen",     { OP_RRALEN,    VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rtlen",      { OP_RTLEN,     VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "riacat8",    { OP_RIACAT8,   VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "riacat16",   { OP_RIACAT16,  VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "riacat32",   { OP_RIACAT32,  VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "riacat64",   { OP_RIACAT64,  VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rsfacat",    { OP_RSFACAT,   VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rdfacat",    { OP_RDFACAT,   VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rracat",     { OP_RRACAT,    VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rtcat",      { OP_RTCAT,     VALUE_TYPE_REF,         VALUE_TYPE_REF } },
        { "rtype",      { OP_RTYPE,     VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "icall",      { OP_ICALL,     VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "fcall",      { OP_FCALL,     VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "rcall",      { OP_RCALL,     VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "itof",       { OP_ITOF,      VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "ftoi",       { OP_FTOI,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "ftoi",       { OP_FTOI,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "incall",     { OP_INCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "fncall",     { OP_FNCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "rncall",     { OP_RNCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } }
      };

      //
      // Static functions.
      //

      static ParseTree *parse(const vector<Source> &sources, list<Error> &errors)
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
        list<pair<const Object *, pair<int, uint32_t>>> object_pairs;
        unordered_map<const Object *, uint32_t> object_addrs;
      };

      struct UngeneratedFunction
      {
        format::Instruction *instrs;
        unordered_map<string, uint32_t> instr_addrs;
      };

      static bool check_object_elems(const Object *object, Value::Type type, list<Error> &errors)
      {
        for(auto & elem : object->elems()) {
          if(elem.type() != Value::TYPE_INT) {
            errors.push_back(Error(elem.pos(), "incorrect value"));
            return false;
          }
        }
        return true;
      }

      static bool add_object_pairs_from_object(UngeneratedProgram &ungen_prog, const Object *object, list<Error> &errors)
      {
        size_t header_size = sizeof(format::Object) - 8;
        if(object->type() == "iarray8") {
          if(!check_object_elems(object, Value::TYPE_INT, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_IARRAY8, header_size + object->elems().size())));
        } else if(object->type() == "iarray16") {
          if(!check_object_elems(object, Value::TYPE_INT, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_IARRAY16, header_size + object->elems().size() * 2)));
        } else if(object->type() == "iarray32") {
          if(!check_object_elems(object, Value::TYPE_INT, errors) &&
              !check_object_elems(object, Value::TYPE_FUN_ADDR, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_IARRAY32, header_size + object->elems().size() * 4)));
        } else if(object->type() == "iarray64") {
          if(!check_object_elems(object, Value::TYPE_INT, errors) &&
              !check_object_elems(object, Value::TYPE_FUN_ADDR, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_IARRAY64, header_size + object->elems().size() * 8)));
        } else if(object->type() == "sfarray") {
          if(!check_object_elems(object, Value::TYPE_FLOAT, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_SFARRAY, header_size + object->elems().size() * 4)));
        } else if(object->type() == "dfarray") {
          if(!check_object_elems(object, Value::TYPE_FLOAT, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_DFARRAY, header_size + object->elems().size() * 8)));
        } else if(object->type() == "rarray") {
          for(auto &elem : object->elems()) {
            if(elem.type() != Value::TYPE_REF) {
              errors.push_back(Error(elem.pos(), "incorrect value"));
              return false;
            }
            if(!add_object_pairs_from_object(ungen_prog, elem.object(), errors)) return false;
          }
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_RARRAY, header_size + object->elems().size() * 4)));
        } else if(object->type() == "tuple") {
          for(auto &elem : object->elems()) {
            if(elem.type() == Value::TYPE_REF)
              if(!add_object_pairs_from_object(ungen_prog, elem.object(), errors)) return false;
          }
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_TUPLE, header_size + object->elems().size() * 9)));
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

      static bool get_fun_addr(const UngeneratedProgram &ungen_prog, uint32_t &addr, const string &ident, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_prog.fun_pairs.find(ident);
        if(iter == ungen_prog.fun_pairs.end()) {
          errors.push_back(Error(pos, "undefined function " + ident));
          return false;
        }
        addr = iter->second.first;
        return true;
      }

      static bool get_var_addr(const UngeneratedProgram &ungen_prog, uint32_t &addr, const string &ident, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_prog.var_pairs.find(ident);
        if(iter == ungen_prog.var_pairs.end()) {
          errors.push_back(Error(pos, "undefined variable " + ident));
          return false;
        }
        addr = iter->second.first;
        return true;
      }

      static bool get_instr_addr(const UngeneratedFunction &ungen_fun, uint32_t &addr, const string &ident, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_fun.instr_addrs.find(ident);
        if(iter == ungen_fun.instr_addrs.end()) {
          errors.push_back(Error(pos, "undefined label " + ident));
          return false;
        }
        addr = iter->second;
        return true;
      }

      static bool value_to_format_value(const UngeneratedProgram &ungen_prog, const Value &value, format::Value &format_value, list<Error> &errors)
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
            format_value.type = VALUE_TYPE_REF;
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

      static bool arg_to_format_arg(const UngeneratedProgram &ungen_prog, const Argument &arg, format::Argument &format_arg, uint32_t &format_arg_type, int value_type, const Position &instr_pos, list<Error> &errors)
      {
        if(value_type == VALUE_TYPE_ERROR) {
          errors.push_back(Error(instr_pos, "incorrect number of arguments"));
          return false;
        }
        switch(arg.type()) {
          case Argument::TYPE_IMM:
            switch(value_type) {
              case VALUE_TYPE_INT:
                if(arg.v().type() == ArgumentValue::TYPE_INT) {
                  if(arg.v().i() < INT32_MIN) {
                    errors.push_back(Error(instr_pos, "too small integer number"));
                    return false;
                  }
                  if(arg.v().i() > INT32_MAX) {
                    errors.push_back(Error(instr_pos, "too large integer number"));
                    return false;
                  }
                  format_arg.i = arg.v().i();
                } else if(arg.v().type() == ArgumentValue::TYPE_FUN_ADDR) {
                  uint32_t u;
                  if(!get_fun_addr(ungen_prog, u, arg.v().fun(), arg.pos(), errors)) return false;
                  format_arg.i = static_cast<int32_t>(u);
                } else {
                  errors.push_back(Error(arg.pos(), "incorrect argument"));
                  return false;
                }
                break;
              case VALUE_TYPE_FLOAT:
                if(arg.v().type() == ArgumentValue::TYPE_FLOAT) {
                  format_arg.f = float_to_format_float(arg.v().f());
                } else {
                  errors.push_back(Error(arg.pos(), "incorrect argument"));
                  return false;
                }
                break;
              case VALUE_TYPE_REF:
                errors.push_back(Error(arg.pos(), "incorrect argument"));
                return false;
            }
            format_arg_type = ARG_TYPE_IMM;
            return true;
          case Argument::TYPE_LVAR:
            format_arg.lvar = arg.lvar();
            format_arg_type = ARG_TYPE_LVAR;
            return true;
          case Argument::TYPE_ARG:
            format_arg.arg = arg.arg();
            format_arg_type = ARG_TYPE_ARG;
            return true;
          case Argument::TYPE_IDENT:
            if(!get_var_addr(ungen_prog, format_arg.gvar, arg.ident(), arg.pos(), errors)) return false;
            format_arg_type = ARG_TYPE_GVAR;
            return true;
        }
      }

      static bool generate_instr_with_op(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, int opcode_instr, list<Error> &errors)
      {
        if(instr.op() == nullptr) {
          errors.push_back(Error(instr.pos(), "no operation"));
          return false;
        }
        auto iter = op_descs.find(instr.op()->name());
        if(iter == op_descs.end()) {
          errors.push_back(Error(instr.pos(), "unknown operation"));
          return false;
        }
        const OperationDescription &op_desc = iter->second;
        uint32_t arg_type1 = ARG_TYPE_IMM, arg_type2 = ARG_TYPE_IMM;
        if(instr.arg1() != nullptr) {
          if(instr.arg1()->type() == Argument::TYPE_IMM && instr.arg2() == nullptr &&
              op_desc.op == OP_ILOAD2) {
            if(instr.arg1()->v().type() == ArgumentValue::TYPE_INT) {
              ungen_fun.instrs[ip].arg1.i = instr.arg1()->v().i() >> 32;
              ungen_fun.instrs[ip].arg2.i = instr.arg1()->v().i() & 0xffffffff;
            } else {
              errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
              return false;
            }
          } else if(instr.arg1()->type() == Argument::TYPE_IMM && instr.arg2() == nullptr &&
              op_desc.op == OP_FLOAD2) {
            if(instr.arg1()->v().type() == ArgumentValue::TYPE_FLOAT) {
              format::Double format_f = double_to_format_double(instr.arg1()->v().f());
              ungen_fun.instrs[ip].arg1.i = format_f.dword >> 32;
              ungen_fun.instrs[ip].arg2.i = format_f.dword & 0xffffffff;
            } else {
              errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
              return false;
            }
          } else {
            if(!arg_to_format_arg(ungen_prog, *(instr.arg1()), ungen_fun.instrs[ip].arg1, arg_type1, op_desc.arg_value_type1, instr.pos(), errors))
              return false;
          }
        } else {
          if(op_desc.arg_value_type1 != VALUE_TYPE_ERROR) {
            errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
            return false;
          }
          ungen_fun.instrs[ip].arg1.i = 0;
        }
        if(instr.arg2() != nullptr) {
          if(!arg_to_format_arg(ungen_prog, *(instr.arg2()), ungen_fun.instrs[ip].arg2, arg_type2, op_desc.arg_value_type2, instr.pos(), errors))
            return false;
        } else {
          if(op_desc.arg_value_type2 != VALUE_TYPE_ERROR) {
            errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
            return false;
          }
          ungen_fun.instrs[ip].arg2.i = 0;
        }
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(opcode_instr, op_desc.op, arg_type1, arg_type2));
        ungen_fun.instrs[ip].arg1.i = htonl(ungen_fun.instrs[ip].arg1.i);
        ungen_fun.instrs[ip].arg2.i = htonl(ungen_fun.instrs[ip].arg2.i);
        return true;
      }

      static bool generate_instr_without_op(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, int opcode_instr, list<Error> &errors)
      {
        if(instr.op() != nullptr) {
          errors.push_back(Error(instr.pos(), "instruction can't have operation"));
          return false;
        }
        if(instr.arg1() != nullptr || instr.arg2() != nullptr) {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(opcode_instr, 0, ARG_TYPE_IMM, ARG_TYPE_IMM));
        ungen_fun.instrs[ip].arg1.i = 0;
        ungen_fun.instrs[ip].arg2.i = 0;
        return true;
      }

      static bool generate_jc(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
      {
        if(instr.op() != nullptr) {
          errors.push_back(Error(instr.pos(), "instruction can't have operation"));
          return false;
        }
        uint32_t arg_type;
        if(instr.arg1() != nullptr) {
          if(!arg_to_format_arg(ungen_prog, *(instr.arg1()), ungen_fun.instrs[ip].arg1, arg_type, VALUE_TYPE_INT, instr.pos(), errors))
            return false;
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        if(instr.arg2() != nullptr) {
          if(instr.arg2()->type() == Argument::TYPE_IDENT) {
            uint32_t instr_addr;
            if(!get_instr_addr(ungen_fun, instr_addr, instr.arg2()->ident(), instr.arg2()->pos(), errors))
              return false;
            ungen_fun.instrs[ip].arg2.i = instr_addr - (ip + 1);
          }
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(INSTR_JC, 0, arg_type, ARG_TYPE_IMM));
        ungen_fun.instrs[ip].arg1.i = htonl(ungen_fun.instrs[ip].arg1.i);
        ungen_fun.instrs[ip].arg2.i = htonl(ungen_fun.instrs[ip].arg2.i);
        return true;
      }

      static bool generate_jump(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
      {
        if(instr.op() != nullptr) {
          errors.push_back(Error(instr.pos(), "instruction can't have operation"));
          return false;
        }
        if(instr.arg1() != nullptr) {
          if(instr.arg1()->type() == Argument::TYPE_IDENT) {
            uint32_t instr_addr;
            if(!get_instr_addr(ungen_fun, instr_addr, instr.arg1()->ident(), instr.arg1()->pos(), errors))
              return false;
            ungen_fun.instrs[ip].arg1.i = instr_addr - (ip + 1);
          }
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        if(instr.arg2() != nullptr) {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(INSTR_JUMP, 0, ARG_TYPE_IMM, ARG_TYPE_IMM));
        ungen_fun.instrs[ip].arg1.i = htonl(ungen_fun.instrs[ip].arg1.i);
        ungen_fun.instrs[ip].arg2.i = 0;
        return true;
      }

      static bool generate_instr(const UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
      {
        if(instr.instr() == "let") {
          return generate_instr_with_op(ungen_prog, ungen_fun, ip, instr, INSTR_LET, errors);
        } else if(instr.instr() == "in") {
          return generate_instr_without_op(ungen_prog, ungen_fun, ip, instr, INSTR_IN, errors);
        } else if(instr.instr() == "ret") {
          return generate_instr_with_op(ungen_prog, ungen_fun, ip, instr, INSTR_RET, errors);
        } else if(instr.instr() == "jc") {
          return generate_jc(ungen_prog, ungen_fun, ip, instr, errors);
        } else if(instr.instr() == "jump") {
          return generate_jump(ungen_prog, ungen_fun, ip, instr, errors);
        } else if(instr.instr() == "arg") {
          return generate_instr_with_op(ungen_prog, ungen_fun, ip, instr, INSTR_ARG, errors);
        } else if(instr.instr() == "retry") {
          return generate_instr_without_op(ungen_prog, ungen_fun, ip, instr, INSTR_RETRY, errors);
        } else {
          errors.push_back(Error(instr.pos(), "unknown instruction"));
          return false;
        }
      }

      static Program *generate_prog(const ParseTree &tree, list<Error> &errors)
      {
        UngeneratedProgram ungen_prog;
        string entry_ident;
        bool is_entry = false;
        Position entry_pos;
        size_t code_size, data_size;
        bool is_success = true;
        for(auto &def : tree.defs()) {
          FunctionDefinition *fun_def = dynamic_cast<FunctionDefinition *>(def.get());
          if(fun_def != nullptr) {
            if(ungen_prog.fun_pairs.find(fun_def->ident()) == ungen_prog.fun_pairs.end()) {
              ungen_prog.fun_pairs.insert(make_pair(fun_def->ident(), make_pair(ungen_prog.fun_pairs.size(), fun_def->fun())));
            } else {
              errors.push_back(Error(fun_def->pos(), "already defined function " + fun_def->ident()));
              is_success = false;
            }
          }
          VariableDefinition *var_def = dynamic_cast<VariableDefinition *>(def.get());
          if(var_def != nullptr) {
            if(ungen_prog.var_pairs.find(var_def->ident()) == ungen_prog.var_pairs.end()) {
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
        for(auto &pair : ungen_prog.var_pairs) {
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
        for(auto &pair : ungen_prog.fun_pairs) {
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
                is_success = false;
              }
            }
            if(line.instr() != nullptr) instr_count++;
          }
          instr_count = 0;
          for(auto line : fun.lines()) {
            if(line.instr() != nullptr) {
              Instruction instr = *(line.instr());
              if(!generate_instr(ungen_prog, ungen_fun, instr_count, instr, errors)) is_success = false;
              instr_count++;
            }
          }
          funs[pair.second.first].addr = htonl(instr_addr);
          funs[pair.second.first].arg_count = htonl(fun.arg_count());
          funs[pair.second.first].instr_count = htonl(instr_count);
          instr_addr += instr_count;
        }

        size_t i = 0;
        for(auto &pair : ungen_prog.object_pairs) {
          const Object *object = pair.first;
          format::Object *format_object = reinterpret_cast<format::Object *>(data + i);
          format_object->type = pair.second.first;
          format_object->length = object->elems().size();
          int j = 0;
          ungen_prog.object_addrs.insert(make_pair(object, i));
          switch(pair.second.first) {
            case OBJECT_TYPE_IARRAY8:
              for(auto &elem : object->elems()) {
                format_object->is8[j] = elem.i();
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY16:
              for(auto &elem : object->elems()) {
                format_object->is16[j] = htons(elem.i());
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY32:
              for(auto &elem : object->elems()) {
                format::Value format_value;
                if(value_to_format_value(ungen_prog, elem, format_value, errors))
                  format_object->is32[j] = htonl(format_value.i);
                else
                  is_success = false;
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY64:
              for(auto &elem : object->elems()) {
                format::Value format_value;
                if(value_to_format_value(ungen_prog, elem, format_value, errors))
                  format_object->is64[j] = htonll(format_value.i);
                else
                  is_success = false;
                j++;
              }
              break;
            case OBJECT_TYPE_SFARRAY:
              for(auto &elem : object->elems()) {
                format_object->sfs[j].word = htonl(float_to_format_float(elem.f()).word);
                j++;
              }
              break;
            case OBJECT_TYPE_DFARRAY:
              for(auto &elem : object->elems()) {
                format_object->dfs[j].dword = htonll(double_to_format_double(elem.f()).dword);
                j++;
              }
              break;
            case OBJECT_TYPE_RARRAY:
              for(auto &elem : object->elems()) {
                format::Value format_value;
                if(value_to_format_value(ungen_prog, elem, format_value, errors))
                  format_object->rs[j] = htonl(format_value.addr);
                else
                  is_success = false;
                j++;
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(auto &elem : object->elems()) {
                format::Value format_value;
                if(value_to_format_value(ungen_prog, elem, format_value, errors)) {
                  format_object->tes[j].i = htonll(format_value.i);
                  format_object->tuple_elem_types()[j] = format_value.type;
                } else
                  is_success = false;
                j++;
              }
              break;
          }
          format_object->type = htonl(format_object->type);
          format_object->length = htonl(format_object->length);
          i += align(pair.second.second, 8);
        }

        for(auto &pair : ungen_prog.var_pairs) {
          const Value &value = pair.second.second;
          if(value_to_format_value(ungen_prog, value, vars[pair.second.first], errors)) {
            vars[pair.second.first].type = htonl(vars[pair.second.first].type);
            vars[pair.second.first].i = htonll(vars[pair.second.first].i);
          } else
            is_success = false;
        }

        if(!is_success) return nullptr;

        Program *prog = new ImplProgram(reinterpret_cast<void *>(ptr.get()), size);
        ptr.release();
        return prog;
      }

      //
      // An ImplCompiler class.
      //

      ImplCompiler::~ImplCompiler() {}

      Program *ImplCompiler::compile(const vector<Source> &sources, list<Error> &errors)
      {
        unique_ptr<ParseTree> tree(parse(sources, errors));
        if(tree.get() == nullptr) return nullptr;
        return generate_prog(*tree, errors);
      }
    }
  }
}
