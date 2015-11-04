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
#include <cstddef>
#include <cstdint>
#include <climits>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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
        uint32_t op;
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
        { "fload2",     { OP_FLOAD2,    VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "fneg",       { OP_FNEG,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fadd",       { OP_FADD,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fsub",       { OP_FSUB,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
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
        { "incall",     { OP_INCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "fncall",     { OP_FNCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "rncall",     { OP_RNCALL,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "ruiafill8",  { OP_RUIAFILL8, VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "ruiafill16", { OP_RUIAFILL16, VALUE_TYPE_INT,        VALUE_TYPE_INT } },
        { "ruiafill32", { OP_RUIAFILL32, VALUE_TYPE_INT,        VALUE_TYPE_INT } },
        { "ruiafill64", { OP_RUIAFILL64, VALUE_TYPE_INT,        VALUE_TYPE_INT } },
        { "rusfafill",  { OP_RUSFAFILL, VALUE_TYPE_INT,         VALUE_TYPE_FLOAT } },
        { "rudfafill",  { OP_RUDFAFILL, VALUE_TYPE_INT,         VALUE_TYPE_FLOAT } },
        { "rurafill",   { OP_RURAFILL,  VALUE_TYPE_INT,         VALUE_TYPE_REF } },
        { "rutfilli",   { OP_RUTFILLI,  VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "rutfillf",   { OP_RUTFILLF,  VALUE_TYPE_INT,         VALUE_TYPE_FLOAT } },
        { "rutfillr",   { OP_RUTFILLR,  VALUE_TYPE_INT,         VALUE_TYPE_REF } },
        { "ruianth8",   { OP_RUIANTH8,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruianth16",  { OP_RUIANTH16, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruianth32",  { OP_RUIANTH32, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruianth64",  { OP_RUIANTH64, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rusfanth",   { OP_RUSFANTH,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rudfanth",   { OP_RUDFANTH,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruranth",    { OP_RURANTH,   VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rutnth",     { OP_RUTNTH,    VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruiasnth8",  { OP_RUIASNTH8, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruiasnth16", { OP_RUIASNTH16, VALUE_TYPE_REF,        VALUE_TYPE_INT } },
        { "ruiasnth32", { OP_RUIASNTH32, VALUE_TYPE_REF,        VALUE_TYPE_INT } },
        { "ruiasnth64", { OP_RUIASNTH64, VALUE_TYPE_REF,        VALUE_TYPE_INT } },
        { "rusfasnth",  { OP_RUSFASNTH, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rudfasnth",  { OP_RUDFASNTH, VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rurasnth",   { OP_RURASNTH,  VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "rutsnth",    { OP_RUTSNTH,   VALUE_TYPE_REF,         VALUE_TYPE_INT } },
        { "ruialen8",   { OP_RUIALEN8,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruialen16",  { OP_RUIALEN16, VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruialen32",  { OP_RUIALEN32, VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruialen64",  { OP_RUIALEN64, VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rusfalen",   { OP_RUSFALEN,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rudfalen",   { OP_RUDFALEN,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruralen",    { OP_RURALEN,   VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rutlen",     { OP_RUTLEN,    VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "rutype",     { OP_RUTYPE,    VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruiatoia8",  { OP_RUIATOIA8, VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruiatoia16", { OP_RUIATOIA16, VALUE_TYPE_REF,        VALUE_TYPE_ERROR } },
        { "ruiatoia32", { OP_RUIATOIA32, VALUE_TYPE_REF,        VALUE_TYPE_ERROR } },
        { "ruiatoia64", { OP_RUIATOIA64, VALUE_TYPE_REF,        VALUE_TYPE_ERROR } },
        { "rusfatosfa", { OP_RUSFATOSFA, VALUE_TYPE_REF,        VALUE_TYPE_ERROR } },
        { "rudfatodfa", { OP_RUDFATODFA, VALUE_TYPE_REF,        VALUE_TYPE_ERROR } },
        { "ruratora",   { OP_RURATORA,  VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "ruttot",     { OP_RUTTOT,    VALUE_TYPE_REF,         VALUE_TYPE_ERROR } },
        { "fpow",       { OP_FPOW,      VALUE_TYPE_FLOAT,       VALUE_TYPE_FLOAT } },
        { "fsqrt",      { OP_FSQRT,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fexp",       { OP_FEXP,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "flog",       { OP_FLOG,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fcos",       { OP_FCOS,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fsin",       { OP_FSIN,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "ftan",       { OP_FTAN,      VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "facos",      { OP_FACOS,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fasin",      { OP_FASIN,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fatan",      { OP_FATAN,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fceil",      { OP_FCEIL,     VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "ffloor",     { OP_FFLOOR,    VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "fround",     { OP_FROUND,    VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "ftrunc",     { OP_FTRUNC,    VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "try",        { OP_TRY,       VALUE_TYPE_INT,         VALUE_TYPE_INT } },
        { "iforce",     { OP_IFORCE,    VALUE_TYPE_INT,         VALUE_TYPE_ERROR } },
        { "fforce",     { OP_FFORCE,    VALUE_TYPE_FLOAT,       VALUE_TYPE_ERROR } },
        { "rforce",     { OP_RFORCE,    VALUE_TYPE_REF,         VALUE_TYPE_ERROR } }
      };

      struct FunctionInfo
      {
        unsigned eval_strategy;
        unsigned eval_strategy_mask;
        unsigned eval_strategy_mask2;

        FunctionInfo() : eval_strategy(0), eval_strategy_mask(~0), eval_strategy_mask2(0) {}

        FunctionInfo(unsigned eval_strategy, unsigned eval_strategy_mask, unsigned eval_strategy_mask2) :
          eval_strategy(eval_strategy), eval_strategy_mask(eval_strategy_mask), eval_strategy_mask2(eval_strategy_mask2) {}

        FunctionInfo &operator|=(const FunctionInfo &fun_info)
        {
         eval_strategy |= fun_info.eval_strategy;
         eval_strategy_mask &= fun_info.eval_strategy_mask;
         eval_strategy_mask2 |= fun_info.eval_strategy_mask2;
         return *this;
        }
      };

      static unordered_map<string, FunctionInfo> annotation_fun_infos {
        { "eager",              { 0U,                   ~EVAL_STRATEGY_LAZY,    0U } },
        { "lazy",               { EVAL_STRATEGY_LAZY,   ~0U,                    0U } },
        { "memoized",           { EVAL_STRATEGY_MEMO,   ~0U,                    0U } },
        { "unmemoized",         { 0U,                   ~EVAL_STRATEGY_MEMO,    0U } },
        { "onlylazy",           { EVAL_STRATEGY_LAZY,   ~0U,                    EVAL_STRATEGY_LAZY } },
        { "onlymemoized",       { EVAL_STRATEGY_MEMO,   ~0U,                    EVAL_STRATEGY_MEMO } }
      };

      //
      // Static functions.
      //

      static ParseTree *parse(const vector<Source> &sources, const vector<string> &include_dirs, list<Error> &errors)
      {
        ParseTree *tree = new ParseTree();
        Driver driver(*tree, include_dirs, errors);
        for(auto source : sources) {
          if(!driver.parse(source)) {
            delete tree;
            return nullptr;
          }
        }
        return tree;
      }

      struct Relocation
      {
        uint32_t type;
        uint32_t addr;
        uint32_t symbol;

        Relocation(uint32_t type, uint32_t addr, uint32_t symbol = 0) :
          type(type), addr(addr), symbol(symbol) {}
      };

      struct Symbol
      {
        uint32_t type;
        string name;
        uint32_t index;

        Symbol(uint32_t type, const string &name, uint32_t index = 0) :
          type(type), name(name), index(index) {}
      };
      
      struct UngeneratedProgram
      {
        bool is_relocatable;
        unordered_map<string, pair<uint32_t, Function>> fun_pairs;
        unordered_map<string, pair<uint32_t, Value>> var_pairs;
        list<Instruction> instrs;
        list<pair<const Object *, pair<int, uint32_t>>> object_pairs;
        unordered_map<const Object *, uint32_t> object_addrs;
        list<Relocation> relocs;
        list<Symbol> symbols;
        unordered_map<string, uint32_t> extern_fun_symbol_indexes;
        unordered_map<string, uint32_t> extern_var_symbol_indexes;
        unordered_map<string, uint32_t> extern_native_fun_symbol_indexes;
        list<pair<uint32_t, FunctionInfo>> fun_info_pairs;
      };

      struct UngeneratedFunction
      {
        uint32_t addr;
        format::Instruction *instrs;
        unordered_map<string, uint32_t> instr_addrs;
      };

      static bool check_object_elems(const Object *object, Value::Type type, list<Error> &errors)
      {
        for(auto &elem : object->elems()) {
          if(elem.type() != type) {
            errors.push_back(Error(elem.pos(), "incorrect value"));
            return false;
          }
        }
        return true;
      }

      static bool check_object_elems(const Object *object, Value::Type type1, Value::Type type2, Value::Type type3, list<Error> &errors)
      {
        for(auto &elem : object->elems()) {
          if(elem.type() != type1 && elem.type() != type2 && elem.type() != type3) {
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
          if(!check_object_elems(object, Value::TYPE_INT, Value::TYPE_FUN_INDEX, Value::TYPE_NATIVE_FUN_INDEX, errors)) return false;
          ungen_prog.object_pairs.push_back(make_pair(object, make_pair(OBJECT_TYPE_IARRAY32, header_size + object->elems().size() * 4)));
        } else if(object->type() == "iarray64") {
          if(!check_object_elems(object, Value::TYPE_INT, Value::TYPE_FUN_INDEX, Value::TYPE_NATIVE_FUN_INDEX, errors)) return false;
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

      static size_t reloc_size_from_arg(const Argument &arg)
      {
        if(arg.type() == Argument::TYPE_IDENT)
          return sizeof(format::Relocation);
        if(arg.type() == Argument::TYPE_IMM && arg.v().type() == ArgumentValue::TYPE_FUN_INDEX)
          return sizeof(format::Relocation);
        if(arg.type() == Argument::TYPE_IMM && arg.v().type() == ArgumentValue::TYPE_NATIVE_FUN_INDEX)
          return sizeof(format::Relocation);
        return 0;
      }

      static void add_symbol_name_from_arg(const UngeneratedProgram &ungen_prog, unordered_set<string> &fun_names, unordered_set<string> &var_names, unordered_set<string> &native_fun_names, const Argument &arg)
      {
        if(arg.type() == Argument::TYPE_IDENT) {
          if(ungen_prog.var_pairs.find(arg.ident()) == ungen_prog.var_pairs.end())
            var_names.insert(arg.ident());
        }
        if(arg.type() == Argument::TYPE_IMM && arg.v().type() == ArgumentValue::TYPE_FUN_INDEX) {
          if(ungen_prog.fun_pairs.find(arg.v().fun()) == ungen_prog.fun_pairs.end())
            fun_names.insert(arg.v().fun());
        }
        if(arg.type() == Argument::TYPE_IMM && arg.v().type() == ArgumentValue::TYPE_NATIVE_FUN_INDEX)
          native_fun_names.insert(arg.v().fun());
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
        if(ungen_prog.is_relocatable) {
          size_t reloc_size = 0;
          for(auto pair : ungen_prog.fun_pairs) {
            const Function &fun = pair.second.second;
            for(auto line : fun.lines()) {
              if(line.instr() != nullptr) {
                if(line.instr()->instr() == "let" || line.instr()->instr() == "arg" ||
                    line.instr()->instr() == "ret" || line.instr()->instr() == "lettuple") {
                  if(line.instr()->arg1() != nullptr)
                    reloc_size += reloc_size_from_arg(*(line.instr()->arg1()));
                  if(line.instr()->arg2() != nullptr)
                    reloc_size += reloc_size_from_arg(*(line.instr()->arg2()));
                } else if(line.instr()->instr() == "jc" || line.instr()->instr() == "throw") {
                  if(line.instr()->arg1() != nullptr)
                    reloc_size += reloc_size_from_arg(*(line.instr()->arg1()));
                }
              }
            }
          }
          for(auto pair : ungen_prog.var_pairs) {
            if(pair.second.second.type() == Value::TYPE_FUN_INDEX || pair.second.second.type() == Value::TYPE_NATIVE_FUN_INDEX)
              reloc_size += sizeof(format::Relocation);
          }
          for(auto pair : ungen_prog.object_pairs) {
            for(auto elem : pair.first->elems()) {
              if(elem.type() == Value::TYPE_FUN_INDEX || elem.type() == Value::TYPE_NATIVE_FUN_INDEX)
                reloc_size += sizeof(format::Relocation);
            }
          }
          size += align(reloc_size, 8);
          unordered_set<string> fun_symbol_names;
          unordered_set<string> var_symbol_names;
          unordered_set<string> native_fun_symbol_names;
          for(auto &symbol : ungen_prog.symbols) {
            switch((symbol.type & ~format::SYMBOL_TYPE_DEFINED)) {
              case format::SYMBOL_TYPE_FUN:
                fun_symbol_names.insert(symbol.name);
                break;
              case format::SYMBOL_TYPE_VAR:
                var_symbol_names.insert(symbol.name);
                break;
            }
          }
          for(auto pair : ungen_prog.fun_pairs) {
            const Function &fun = pair.second.second;
            for(auto line : fun.lines()) {
              if(line.instr() != nullptr) {
                if(line.instr()->instr() == "let" || line.instr()->instr() == "arg" ||
                    line.instr()->instr() == "ret" || line.instr()->instr() == "lettuple") {
                  if(line.instr()->arg1() != nullptr)
                    add_symbol_name_from_arg(ungen_prog, fun_symbol_names, var_symbol_names, native_fun_symbol_names, *(line.instr()->arg1()));
                  if(line.instr()->arg2() != nullptr)
                    add_symbol_name_from_arg(ungen_prog, fun_symbol_names, var_symbol_names, native_fun_symbol_names, *(line.instr()->arg2()));
                } else if(line.instr()->instr() == "jc" || line.instr()->instr() == "throw") {
                  if(line.instr()->arg1() != nullptr)
                    add_symbol_name_from_arg(ungen_prog, fun_symbol_names, var_symbol_names, native_fun_symbol_names, *(line.instr()->arg1()));
                }
              }
            }
          }
          for(auto pair : ungen_prog.var_pairs) {
            if(pair.second.second.type() == Value::TYPE_FUN_INDEX || pair.second.second.type() == Value::TYPE_NATIVE_FUN_INDEX)
              if(ungen_prog.fun_pairs.find(pair.second.second.fun()) == ungen_prog.fun_pairs.end())
                fun_symbol_names.insert(pair.second.second.fun());
          }
          for(auto pair : ungen_prog.object_pairs) {
            for(auto elem : pair.first->elems()) {
              if(elem.type() == Value::TYPE_FUN_INDEX || elem.type() == Value::TYPE_NATIVE_FUN_INDEX)
                if(ungen_prog.fun_pairs.find(elem.fun()) == ungen_prog.fun_pairs.end())
                  fun_symbol_names.insert(elem.fun());
            }
          }
          for(auto fun_symbol_name : fun_symbol_names)
            size += align(7 + fun_symbol_name.length(), 8);
          for(auto var_symbol_name : var_symbol_names)
            size += align(7 + var_symbol_name.length(), 8);
          for(auto native_fun_symbol_name : native_fun_symbol_names)
            size += align(7 + native_fun_symbol_name.length(), 8);
        }
        size += align(ungen_prog.fun_info_pairs.size() * sizeof(format::FunctionInfo), 8);
        return size;
      }
      
      static void add_reloc(UngeneratedProgram &ungen_prog, uint32_t type, uint32_t addr, const string symbol_name = string())
      {
        if((type & format::RELOC_TYPE_SYMBOLIC) != 0) {
          uint32_t symbol = ungen_prog.symbols.size();
          switch(type & ~format::RELOC_TYPE_SYMBOLIC) {
            case format::RELOC_TYPE_ARG1_FUN:
            case format::RELOC_TYPE_ARG2_FUN:
            case format::RELOC_TYPE_ELEM_FUN:
            case format::RELOC_TYPE_VAR_FUN:
            {
              auto iter = ungen_prog.extern_fun_symbol_indexes.find(symbol_name);
              if(iter != ungen_prog.extern_fun_symbol_indexes.end()) {
                symbol = iter->second;
              } else {
                ungen_prog.extern_fun_symbol_indexes.insert(make_pair(symbol_name, symbol));
                ungen_prog.symbols.push_back(Symbol(format::SYMBOL_TYPE_FUN, symbol_name));
              }
              break;
            }
            case format::RELOC_TYPE_ARG1_VAR:
            case format::RELOC_TYPE_ARG2_VAR:
            {
              auto iter = ungen_prog.extern_var_symbol_indexes.find(symbol_name);
              if(iter != ungen_prog.extern_var_symbol_indexes.end()) {
                symbol = iter->second;
              } else {
                ungen_prog.extern_var_symbol_indexes.insert(make_pair(symbol_name, symbol));
                ungen_prog.symbols.push_back(Symbol(format::SYMBOL_TYPE_VAR, symbol_name));
              }
              break;
            }
            case format::RELOC_TYPE_ARG1_NATIVE_FUN:
            case format::RELOC_TYPE_ARG2_NATIVE_FUN:
            case format::RELOC_TYPE_ELEM_NATIVE_FUN:
            case format::RELOC_TYPE_VAR_NATIVE_FUN:
            {
              auto iter = ungen_prog.extern_native_fun_symbol_indexes.find(symbol_name);
              if(iter != ungen_prog.extern_native_fun_symbol_indexes.end()) {
                symbol = iter->second;
              } else {
                ungen_prog.extern_native_fun_symbol_indexes.insert(make_pair(symbol_name, symbol));
                ungen_prog.symbols.push_back(Symbol(format::SYMBOL_TYPE_NATIVE_FUN, symbol_name));
              }
              break;
            }
          }
          ungen_prog.relocs.push_back(Relocation(type, addr, symbol));
        } else
          ungen_prog.relocs.push_back(Relocation(type, addr));
      }

      static bool get_fun_index(const UngeneratedProgram &ungen_prog, uint32_t &index, const string &ident, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_prog.fun_pairs.find(ident);
        if(iter == ungen_prog.fun_pairs.end()) {
          errors.push_back(Error(pos, "undefined function " + ident));
          return false;
        }
        index = iter->second.first;
        return true;
      }

      static bool get_fun_index_and_add_reloc(UngeneratedProgram &ungen_prog, uint32_t &index, const string &ident, uint32_t reloc_type, uint32_t addr, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_prog.fun_pairs.find(ident);
        if(iter == ungen_prog.fun_pairs.end()) {
          if(ungen_prog.is_relocatable) {
            add_reloc(ungen_prog, reloc_type | format::RELOC_TYPE_SYMBOLIC, addr, ident);
            index = 0;
            return true;
          } else {
            errors.push_back(Error(pos, "undefined function " + ident));
            return false;
          }
        }
        if(ungen_prog.is_relocatable) add_reloc(ungen_prog, reloc_type, addr);
        index = iter->second.first;
        return true;
      }

      static bool get_var_index_and_add_reloc(UngeneratedProgram &ungen_prog, uint32_t &index, const string &ident, uint32_t reloc_type, uint32_t addr, const Position &pos, list<Error> &errors)
      {
        auto iter = ungen_prog.var_pairs.find(ident);
        if(iter == ungen_prog.var_pairs.end()) {
          if(ungen_prog.is_relocatable) {
            add_reloc(ungen_prog, reloc_type | format::RELOC_TYPE_SYMBOLIC, addr, ident);
            index = 0;
            return true;
          } else {
            errors.push_back(Error(pos, "undefined variable " + ident));
            return false;
          }
        }
        if(ungen_prog.is_relocatable) add_reloc(ungen_prog, reloc_type, addr);
        index = iter->second.first;
        return true;
      }

      static bool get_native_fun_index_and_add_reloc(UngeneratedProgram &ungen_prog, uint32_t &index, const string &ident, uint32_t reloc_type, uint32_t addr, const Position &pos, list<Error> &errors)
      {
        if(ungen_prog.is_relocatable) {
          add_reloc(ungen_prog, reloc_type | format::RELOC_TYPE_SYMBOLIC, addr, ident);
          index = 0;
          return true;
        } else {
          errors.push_back(Error(pos, "unsupported native function symbols for unrelocable program"));
          return false;
        }
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

      static bool value_to_format_value(UngeneratedProgram &ungen_prog, const Value &value, format::Value &format_value, list<Error> &errors, uint32_t *addr = nullptr, bool is_var = false)
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
          case Value::TYPE_FUN_INDEX:
          {
            format_value.type = VALUE_TYPE_INT;
            uint32_t u;
            if(addr != nullptr) {
              uint32_t reloc_type = (is_var ? format::RELOC_TYPE_VAR_FUN : format::RELOC_TYPE_ELEM_FUN);
              if(!get_fun_index_and_add_reloc(ungen_prog, u, value.fun(), reloc_type, *addr, value.pos(), errors)) return false;
            } else {
              if(!get_fun_index(ungen_prog, u, value.fun(), value.pos(), errors)) return false;
            }
            format_value.i = static_cast<int32_t>(u);
            return true;
          }
          case Value::TYPE_NATIVE_FUN_INDEX:
          {
            format_value.type = VALUE_TYPE_INT;
            uint32_t u;
            if(addr != nullptr) {
              uint32_t reloc_type = (is_var ? format::RELOC_TYPE_VAR_NATIVE_FUN : format::RELOC_TYPE_ELEM_NATIVE_FUN);
              if(!get_native_fun_index_and_add_reloc(ungen_prog, u, value.fun(), reloc_type, *addr, value.pos(), errors)) return false;
            } else {
              errors.push_back(Error(value.pos(), "incorrect value"));
              return false;
            }
            format_value.i = static_cast<int32_t>(u);
            return true;
          }            
        }
        return false;
      }

      static bool arg_to_format_arg(UngeneratedProgram &ungen_prog, const Argument &arg, format::Argument &format_arg, uint32_t &format_arg_type, int value_type, uint32_t ip, bool is_first, const Position &instr_pos, list<Error> &errors)
      {
        if(value_type == VALUE_TYPE_ERROR) {
          errors.push_back(Error(instr_pos, "incorrect number of arguments"));
          return false;
        }
        uint32_t fun_reloc_type = (is_first ? format::RELOC_TYPE_ARG1_FUN : format::RELOC_TYPE_ARG2_FUN);
        uint32_t var_reloc_type = (is_first ? format::RELOC_TYPE_ARG1_VAR : format::RELOC_TYPE_ARG2_VAR);
        uint32_t native_fun_reloc_type = (is_first ? format::RELOC_TYPE_ARG1_NATIVE_FUN : format::RELOC_TYPE_ARG2_NATIVE_FUN);
        switch(arg.type()) {
          case Argument::TYPE_IMM:
            switch(value_type) {
              case VALUE_TYPE_INT:
                if(arg.v().type() == ArgumentValue::TYPE_INT) {
                  if(arg.v().i() < INT32_MIN) {
                    errors.push_back(Error(instr_pos, "too small integer number"));
                    return false;
                  }
                  if(arg.v().i() > static_cast<int64_t>(UINT32_MAX)) {
                    errors.push_back(Error(instr_pos, "too large integer number"));
                    return false;
                  }
                  format_arg.i = arg.v().i();
                } else if(arg.v().type() == ArgumentValue::TYPE_FUN_INDEX) {
                  uint32_t u;
                  if(!get_fun_index_and_add_reloc(ungen_prog, u, arg.v().fun(), fun_reloc_type, ip, arg.pos(), errors))
                    return false;
                  format_arg.i = static_cast<int32_t>(u);
                } else if(arg.v().type() == ArgumentValue::TYPE_NATIVE_FUN_INDEX) {
                  uint32_t u;
                  if(!get_native_fun_index_and_add_reloc(ungen_prog, u, arg.v().fun(), native_fun_reloc_type, ip, arg.pos(), errors))
                    return false;
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
            if(!get_var_index_and_add_reloc(ungen_prog, format_arg.gvar, arg.ident(), var_reloc_type, ip, arg.pos(), errors))
              return false;
            format_arg_type = ARG_TYPE_GVAR;
            return true;
        }
        return false;
      }

      static bool generate_instr_with_op(UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, int opcode_instr, list<Error> &errors)
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
        bool is_success = true;
        const OperationDescription &op_desc = iter->second;
        uint32_t arg_type1 = ARG_TYPE_IMM, arg_type2 = ARG_TYPE_IMM;
        bool is_ignored_arg2 = false;
        if(instr.arg1() != nullptr) {
          if(instr.arg1()->type() == Argument::TYPE_IMM && instr.arg2() == nullptr &&
              op_desc.op == OP_ILOAD2) {
            if(instr.arg1()->v().type() == ArgumentValue::TYPE_INT) {
              ungen_fun.instrs[ip].arg1.i = instr.arg1()->v().i() >> 32;
              ungen_fun.instrs[ip].arg2.i = instr.arg1()->v().i() & 0xffffffff;
              is_ignored_arg2 = true;
            } else {
              errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
              is_success = false;
            }
          } else if(instr.arg1()->type() == Argument::TYPE_IMM && instr.arg2() == nullptr &&
              op_desc.op == OP_FLOAD2) {
            if(instr.arg1()->v().type() == ArgumentValue::TYPE_FLOAT) {
              format::Double format_f = double_to_format_double(instr.arg1()->v().f());
              ungen_fun.instrs[ip].arg1.i = format_f.dword >> 32;
              ungen_fun.instrs[ip].arg2.i = format_f.dword & 0xffffffff;
              is_ignored_arg2 = true;
            } else {
              errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
              is_success = false;
            }
          } else {
            if(!arg_to_format_arg(ungen_prog, *(instr.arg1()), ungen_fun.instrs[ip].arg1, arg_type1, op_desc.arg_value_type1, ip + ungen_fun.addr, true, instr.pos(), errors))
              is_success = false;
          }
        } else {
          if(op_desc.arg_value_type1 != VALUE_TYPE_ERROR) {
            errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
            is_success = false;
          }
          ungen_fun.instrs[ip].arg1.i = 0;
        }
        if(!is_ignored_arg2) {
          if(instr.arg2() != nullptr) {
            if(!arg_to_format_arg(ungen_prog, *(instr.arg2()), ungen_fun.instrs[ip].arg2, arg_type2, op_desc.arg_value_type2, ip + ungen_fun.addr, false, instr.pos(), errors))
              is_success = false;
          } else {
            if(op_desc.arg_value_type2 != VALUE_TYPE_ERROR) {
              errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
              is_success = false;
            }
            ungen_fun.instrs[ip].arg2.i = 0;
          }
        }
        uint32_t local_var_count = 2;
        if(opcode_instr == INSTR_LETTUPLE) {
          if(instr.local_var_count() != nullptr) {
            local_var_count = *(instr.local_var_count());
            if(local_var_count < 2) {
              errors.push_back(Error(instr.pos(), "too small number of local variables"));
              is_success = false;
            }
            if(local_var_count > 258) {
              errors.push_back(Error(instr.pos(), "too large number of local variables"));
              is_success = false;
            }
          } else {
            errors.push_back(Error(instr.pos(), "no number of local variables"));
            is_success = false;
          }
        } else {
          if(instr.local_var_count() != nullptr) {
            errors.push_back(Error(instr.pos(), "number of local variables"));
            is_success = false;
          }
        }
        if(!is_success) return false;
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(opcode_instr, op_desc.op, arg_type1, arg_type2, local_var_count));
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

      static bool generate_jc(UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
      {
        if(instr.op() != nullptr) {
          errors.push_back(Error(instr.pos(), "instruction can't have operation"));
          return false;
        }
        bool is_success = true;
        uint32_t arg_type;
        if(instr.arg1() != nullptr) {
          if(!arg_to_format_arg(ungen_prog, *(instr.arg1()), ungen_fun.instrs[ip].arg1, arg_type, VALUE_TYPE_INT, ip + ungen_fun.addr, true, instr.pos(), errors))
            is_success = false;
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          is_success = false;
        }
        if(instr.arg2() != nullptr) {
          if(instr.arg2()->type() == Argument::TYPE_IDENT) {
            uint32_t instr_addr;
            if(!get_instr_addr(ungen_fun, instr_addr, instr.arg2()->ident(), instr.arg2()->pos(), errors))
              is_success = false;
            ungen_fun.instrs[ip].arg2.i = instr_addr - (ip + 1);
          }
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          is_success = false;
        }
        if(!is_success) return false;
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

      static bool generate_throw(UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
      {
        if(instr.op() != nullptr) {
          errors.push_back(Error(instr.pos(), "instruction can't have operation"));
          return false;
        }
        uint32_t arg_type;
        if(instr.arg1() != nullptr) {
          if(!arg_to_format_arg(ungen_prog, *(instr.arg1()), ungen_fun.instrs[ip].arg1, arg_type, VALUE_TYPE_INT, ip + ungen_fun.addr, true, instr.pos(), errors))
            return false;
        } else {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        if(instr.arg2() != nullptr) {
          errors.push_back(Error(instr.pos(), "incorrect number of arguments"));
          return false;
        }
        ungen_fun.instrs[ip].opcode = htonl(opcode::opcode(INSTR_THROW, 0, arg_type, ARG_TYPE_IMM));
        ungen_fun.instrs[ip].arg1.i = htonl(ungen_fun.instrs[ip].arg1.i);
        ungen_fun.instrs[ip].arg2.i = 0;
        return true;
      }

      static bool generate_instr(UngeneratedProgram &ungen_prog, const UngeneratedFunction &ungen_fun, uint32_t ip, const Instruction &instr, list<Error> &errors)
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
        } else if(instr.instr() == "lettuple") {
          return generate_instr_with_op(ungen_prog, ungen_fun, ip, instr, INSTR_LETTUPLE, errors);
        } else if(instr.instr() == "throw") {
          return generate_throw(ungen_prog, ungen_fun, ip, instr, errors);
        } else {
          errors.push_back(Error(instr.pos(), "unknown instruction"));
          return false;
        }
      }

      static bool annotations_to_fun_info(const list<Annotation> &annotations, FunctionInfo &fun_info, list<Error> &errors)
      {
        fun_info = FunctionInfo();
        bool is_success = true;
        for(auto annotation : annotations) {
          auto iter = annotation_fun_infos.find(annotation.name());
          if(iter != annotation_fun_infos.end()) {
            fun_info |= iter->second;
            if((iter->second.eval_strategy == EVAL_STRATEGY_LAZY ||
                iter->second.eval_strategy_mask == ~EVAL_STRATEGY_LAZY) &&
                (fun_info.eval_strategy & EVAL_STRATEGY_LAZY) != 0 &&
                (fun_info.eval_strategy_mask & EVAL_STRATEGY_LAZY) == 0) {
              errors.push_back(Error(annotation.pos(), "function can't be eager and lazy"));
              is_success = false;
            } else if((iter->second.eval_strategy == EVAL_STRATEGY_MEMO ||
                iter->second.eval_strategy_mask == ~EVAL_STRATEGY_MEMO) &&
                (fun_info.eval_strategy & EVAL_STRATEGY_MEMO) != 0 &&
                (fun_info.eval_strategy_mask & EVAL_STRATEGY_MEMO) == 0) {
              errors.push_back(Error(annotation.pos(), "function can't be unmemoized and memoized"));
              is_success = false;
            }
          } else {
            errors.push_back(Error(annotation.pos(), "unknown annotation"));
            is_success = false;
          }
        }
        return is_success;
      }

      static Program *generate_prog(const ParseTree &tree, list<Error> &errors, bool is_relocable)
      {
        UngeneratedProgram ungen_prog;
        string entry_ident;
        bool is_entry = false;
        Position entry_pos;
        size_t code_size, data_size;
        bool is_success = true;
        ungen_prog.is_relocatable = is_relocable;
        for(auto &def : tree.defs()) {
          FunctionDefinition *fun_def = dynamic_cast<FunctionDefinition *>(def.get());
          if(fun_def != nullptr) {
            if(ungen_prog.fun_pairs.find(fun_def->ident()) == ungen_prog.fun_pairs.end()) {
              uint32_t tmp_fun_count = ungen_prog.fun_pairs.size();
              ungen_prog.fun_pairs.insert(make_pair(fun_def->ident(), make_pair(tmp_fun_count, fun_def->fun())));
              FunctionInfo fun_info;
              if(annotations_to_fun_info(fun_def->annotations(), fun_info, errors)) {
                if(fun_info.eval_strategy != 0U || fun_info.eval_strategy_mask != ~0U)
                  ungen_prog.fun_info_pairs.push_back(make_pair(tmp_fun_count, fun_info));
                if(ungen_prog.is_relocatable && fun_def->modifier() == PUBLIC)
                  ungen_prog.symbols.push_back(Symbol(format::SYMBOL_TYPE_FUN | format::SYMBOL_TYPE_DEFINED, fun_def->ident(), tmp_fun_count));
              } else
                is_success = false;
            } else {
              errors.push_back(Error(fun_def->pos(), "already defined function " + fun_def->ident()));
              is_success = false;
            }
          }
          VariableDefinition *var_def = dynamic_cast<VariableDefinition *>(def.get());
          if(var_def != nullptr) {
            if(ungen_prog.var_pairs.find(var_def->ident()) == ungen_prog.var_pairs.end()) {
              uint32_t tmp_var_count = ungen_prog.var_pairs.size();
              ungen_prog.var_pairs.insert(make_pair(var_def->ident(), make_pair(tmp_var_count, var_def->value())));
              if(ungen_prog.is_relocatable && var_def->modifier() == PUBLIC)
                ungen_prog.symbols.push_back(Symbol(format::SYMBOL_TYPE_VAR | format::SYMBOL_TYPE_DEFINED, var_def->ident(), tmp_var_count));
            } else {
              errors.push_back(Error(var_def->pos(), "already defined variable " + var_def->ident()));
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
        fill_n(tmp_ptr, size, 0);
        format::Header *header = reinterpret_cast<format::Header *>(tmp_ptr);
        tmp_ptr += align(sizeof(format::Header), 8);
        copy(format::HEADER_MAGIC, format::HEADER_MAGIC + 8, header->magic);
        uint32_t relocatable_flags = format::HEADER_FLAG_RELOCATABLE | format::HEADER_FLAG_NATIVE_FUN_SYMBOLS;
        uint32_t fun_info_flags = format::HEADER_FLAG_FUN_INFOS;
        if(is_entry) {
          header->flags = htonl((ungen_prog.is_relocatable ? relocatable_flags : 0) | fun_info_flags);
          if(!get_fun_index(ungen_prog, header->entry, entry_ident, entry_pos, errors)) return nullptr;
          header->entry = htonl(header->entry);
        } else {
          header->flags = htonl(format::HEADER_FLAG_LIBRARY | (ungen_prog.is_relocatable ? relocatable_flags : 0) | fun_info_flags);
          header->entry = 0;
        }
        header->fun_count = htonl(ungen_prog.fun_pairs.size());
        header->var_count = htonl(ungen_prog.var_pairs.size());
        header->code_size = htonl(code_size);
        header->data_size = htonl(data_size);
        header->reloc_count = 0;
        header->symbol_count = 0;
        header->fun_info_count = 0;
        fill_n(header->reserved, sizeof(header->reserved) / 4, 0);

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
          ungen_fun.addr = instr_addr;
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
                if(elem.i() < INT8_MIN) {
                  errors.push_back(Error(elem.pos(), "too small integer number"));
                  is_success = false;
                }
                if(elem.i() > static_cast<int64_t>(UINT8_MAX)) {
                  errors.push_back(Error(elem.pos(), "too large integer number"));
                  is_success = false;
                }
                format_object->is8[j] = elem.i();
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY16:
              for(auto &elem : object->elems()) {
                if(elem.i() < INT16_MIN) {
                  errors.push_back(Error(elem.pos(), "too small integer number"));
                  is_success = false;
                }
                if(elem.i() > static_cast<int64_t>(UINT16_MAX)) {
                  errors.push_back(Error(elem.pos(), "too large integer number"));
                  is_success = false;
                }
                format_object->is16[j] = htons(elem.i());
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY32:
              for(auto &elem : object->elems()) {
                if(elem.type() != Value::TYPE_FUN_INDEX && elem.type() != Value::TYPE_NATIVE_FUN_INDEX) {
                  if(elem.i() < INT32_MIN) {
                    errors.push_back(Error(elem.pos(), "too small integer number"));
                    is_success = false;
                  }
                  if(elem.i() > static_cast<int64_t>(UINT32_MAX)) {
                    errors.push_back(Error(elem.pos(), "too large integer number"));
                    is_success = false;
                  }
                }
                format::Value format_value;
                uint32_t elem_addr = i + 8 + j * 4;
                if(value_to_format_value(ungen_prog, elem, format_value, errors, &elem_addr))
                  format_object->is32[j] = htonl(format_value.i);
                else
                  is_success = false;
                j++;
              }
              break;
            case OBJECT_TYPE_IARRAY64:
              for(auto &elem : object->elems()) {
                format::Value format_value;
                uint32_t elem_addr = i + 8 + j * 8;
                if(value_to_format_value(ungen_prog, elem, format_value, errors, &elem_addr))
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
                uint32_t elem_addr = i + 8 + j * 8;
                if(value_to_format_value(ungen_prog, elem, format_value, errors, &elem_addr)) {
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
          uint32_t var_addr = pair.second.first;
          if(value_to_format_value(ungen_prog, value, vars[pair.second.first], errors, &var_addr, true)) {
            vars[pair.second.first].type = htonl(vars[pair.second.first].type);
            vars[pair.second.first].i = htonll(vars[pair.second.first].i);
          } else
            is_success = false;
        }

        if(ungen_prog.is_relocatable) {
          header->reloc_count = htonl(ungen_prog.relocs.size());
          header->symbol_count = htonl(ungen_prog.symbols.size());

          format::Relocation *relocs = reinterpret_cast<format::Relocation *>(tmp_ptr);
          tmp_ptr += align(ungen_prog.relocs.size() * sizeof(format::Relocation), 8);
          uint8_t *symbols = tmp_ptr;

          i = 0;
          for(auto &reloc : ungen_prog.relocs) {
            format::Relocation *format_reloc = relocs + i;
            format_reloc->type = htonl(reloc.type);
            format_reloc->addr = htonl(reloc.addr);
            format_reloc->symbol = htonl(reloc.symbol);
            i++;
          }

          i = 0;
          for(auto &symbol : ungen_prog.symbols) {
            format::Symbol *format_symbol = reinterpret_cast<format::Symbol *>(symbols + i);
            format_symbol->index = htonl(symbol.index);
            format_symbol->length = htons(symbol.name.length());
            format_symbol->type = symbol.type;
            copy(symbol.name.begin(), symbol.name.end(), format_symbol->name);
            i += align(7 + symbol.name.length(), 8);
          }
          tmp_ptr += i;
        }

        if(!ungen_prog.fun_info_pairs.empty()) {
          header->fun_info_count = htonl(ungen_prog.fun_info_pairs.size());
          format::FunctionInfo *fun_infos = reinterpret_cast<format::FunctionInfo *>(tmp_ptr);
          tmp_ptr += align(ungen_prog.fun_info_pairs.size() * sizeof(format::FunctionInfo), 8);

          i = 0;
          for(auto pair : ungen_prog.fun_info_pairs) {
            format::FunctionInfo *format_fun_info = fun_infos + i;
            format_fun_info->fun_index = htonl(pair.first);
            format_fun_info->eval_strategy = pair.second.eval_strategy & 0xff;
            if(pair.second.eval_strategy_mask2 == 0) 
              format_fun_info->eval_strategy_mask = pair.second.eval_strategy_mask & 0xff;
            else
              format_fun_info->eval_strategy_mask = pair.second.eval_strategy_mask2 & 0xff;
            fill_n(format_fun_info->reserved, sizeof(format_fun_info->reserved), 0);
            i++;
          }
        }

        if(!is_success) return nullptr;

        Program *prog = new ImplProgram(reinterpret_cast<void *>(ptr.get()), size);
        ptr.release();
        return prog;
      }

      static void sort_errors(list<Error> &errors)
      {
        errors.sort([](const Error &error1, const Error &error2) {
          if(error1.pos().source().file_name() < error2.pos().source().file_name()) {
            return true;
          } else if(error1.pos().source().file_name() == error2.pos().source().file_name()) {
            if(error1.pos().line() < error2.pos().line())
              return true;
            else if(error1.pos().line() == error2.pos().line())
              return error1.pos().column() < error2.pos().column();
            else
              return false;
          } else
            return false;
        });
      }
      
      //
      // An ImplCompiler class.
      //

      ImplCompiler::~ImplCompiler() {}

      Program *ImplCompiler::compile(const vector<Source> &sources, list<Error> &errors, bool is_relocable)
      {
        unique_ptr<ParseTree> tree(parse(sources, _M_include_dirs, errors));
        Program *prog = nullptr;
        if(tree.get() != nullptr) prog = generate_prog(*tree, errors, is_relocable);
        sort_errors(errors);
        return prog;
      }

      void ImplCompiler::add_include_dir(const string &dir_name)
      { _M_include_dirs.push_back(dir_name); }
    }
  }
}
