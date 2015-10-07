/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _PARSE_TREE_HPP
#define _PARSE_TREE_HPP

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <letin/comp.hpp>
#include "num_value.hpp"

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class Object;
      class Instruction;

      class Definition
      {
      protected:
        Position _M_pos;

        Definition(const Position &pos) : _M_pos(pos) {}
      public:
        virtual ~Definition();

        const Position pos() const { return _M_pos; }
      };

      class ParseTree
      {
        std::shared_ptr< std::list<std::shared_ptr<Definition>>> _M_defs;
      public:
        ParseTree() : _M_defs(new std::list<std::shared_ptr<Definition>>()) {}

        virtual ~ParseTree();

        const std::list<std::shared_ptr<Definition>> &defs() const { return *_M_defs; }

        void add_def(const std::shared_ptr<Definition> &def) { _M_defs->push_back(def); }
      };

      class ArgumentValue
      {
      public:
        enum Type
        {
          TYPE_INT,
          TYPE_FLOAT,
          TYPE_FUN_INDEX,
          TYPE_NATIVE_FUN_INDEX
        };
      private:
        Type _M_type;
        union
        {
          std::int64_t _M_i;
          double _M_f;
          std::string _M_fun;
        };
      public:
        ArgumentValue(int i) : _M_type(TYPE_INT) { _M_i = i; }

        ArgumentValue(std::int64_t i) : _M_type(TYPE_INT) { _M_i = i; }

        ArgumentValue(double f) : _M_type(TYPE_FLOAT) { _M_f = f; }

        ArgumentValue(const NumberValue &value);

        ArgumentValue(const std::string &fun, bool is_native_fun = false) :
          _M_type(is_native_fun ? TYPE_NATIVE_FUN_INDEX : TYPE_FUN_INDEX), _M_fun(fun) {}

        ArgumentValue(const ArgumentValue &value) : _M_type(value._M_type) { copy_union(value); }

        virtual ~ArgumentValue();
      private:
        void copy_union(const ArgumentValue &value);

        void destruct_union()
        { 
          using namespace std;
          if(_M_type == TYPE_FUN_INDEX) _M_fun.~string();
        }
      public:
        ArgumentValue &operator=(const ArgumentValue &value)
        {
          if(this != &value) {
            destruct_union();
            _M_type = value._M_type;
            copy_union(value);
          }
          return *this;
        }

        Type type() const { return _M_type; }

        std::int64_t i() const { return _M_type == TYPE_INT ? _M_i : 0; }

        double f() const { return _M_type == TYPE_FLOAT ? _M_f : 0.0; }

        std::string fun() const { return _M_type == TYPE_FUN_INDEX ? _M_fun : std::string(); }
      };

      class Value
      {
      public:
        enum Type
        {
          TYPE_INT,
          TYPE_FLOAT,
          TYPE_REF,
          TYPE_FUN_INDEX,
          TYPE_NATIVE_FUN_INDEX
        };
      private:
        Type _M_type;
        union
        {
          std::int64_t _M_i;
          double _M_f;
          std::string _M_fun;
          Object *_M_object;
        };
        Position _M_pos;
      public:
        Value(int i, const Position &pos) :
          _M_type(TYPE_INT), _M_pos(pos) { _M_i = i; }
        
        Value(std::int64_t i, const Position &pos) :
          _M_type(TYPE_INT), _M_pos(pos) { _M_i = i; }

        Value(double f, const Position &pos) :
          _M_type(TYPE_FLOAT), _M_pos(pos) { _M_f = f; }

        Value(const NumberValue &value, const Position &pos);

        Value(const std::string &fun, const Position &pos, bool is_native_fun_index = false) :
          _M_type(is_native_fun_index ? TYPE_NATIVE_FUN_INDEX : TYPE_FUN_INDEX), _M_fun(fun), _M_pos(pos) {}

        Value(Object *object, const Position &pos) :
          _M_type(TYPE_REF), _M_object(object), _M_pos(pos) {}

        Value(const Value &value) : _M_type(value._M_type), _M_pos(value._M_pos) { copy_union(value); }

        ~Value();
      private:
        void copy_union(const Value &value);

        void destruct_union();
      public:
        Value &operator=(const Value &value)
        {
          if(this != &value) {
            destruct_union();
            _M_type = value._M_type;
            _M_pos = value._M_pos;
            copy_union(value);
          }
          return *this;
        }

        Type type() const { return _M_type; }

        std::int64_t i() const { return _M_type == TYPE_INT ? _M_i : 0; }

        double f() const { return _M_type == TYPE_FLOAT ? _M_f : 0.0; }

        std::string fun() const { return _M_type == TYPE_FUN_INDEX ? _M_fun : std::string(); }
        
        const Object *object() const { return _M_object; }
        
        const Position &pos() const { return _M_pos; }
      };


      class Object
      {
        std::string _M_type;
        std::shared_ptr<std::list<Value>> _M_elems;
        Position _M_pos;
      public:
        Object(const std::string &str, const Position &pos) :
          _M_type("iarray8"), _M_elems(new std::list<Value>()), _M_pos(pos)
        { for(auto c : str) _M_elems->push_back(Value(static_cast<int>(c), Position())); }

        Object(const std::string &type, const std::shared_ptr<std::list<Value>> &elems, const Position &pos) :
          _M_type(type), _M_elems(elems), _M_pos(pos) {}
         
        virtual ~Object();

        const std::string &type() const { return _M_type; }

        const std::list<Value> &elems() const { return *_M_elems; }

        const Position &pos() const { return _M_pos; }
      };

      enum IndexArgumentEnum { LVAR, ARG };

      class Argument
      {
      public:
        enum Type
        {
          TYPE_IMM,
          TYPE_LVAR,
          TYPE_ARG,
          TYPE_IDENT
        };
      private:
        Type _M_type;
        union
        {
          ArgumentValue _M_v;
          std::uint32_t _M_lvar;
          std::uint32_t _M_arg;
          std::string _M_ident;
        };
        Position _M_pos;
      public:
        Argument(const ArgumentValue &v, const Position &pos) :
          _M_type(TYPE_IMM), _M_v(v), _M_pos(pos) {}

        Argument(IndexArgumentEnum arg_enum, std::uint32_t i, const Position &pos) :
          _M_pos(pos)
        {
          if(arg_enum == LVAR) {
            _M_type = TYPE_LVAR; _M_lvar = i;
          } else {
            _M_type = TYPE_ARG; _M_arg = i;
          }
        }

        Argument(const std::string  &ident, const Position &pos) :
          _M_type(TYPE_IDENT), _M_ident(ident), _M_pos(pos){}

        Argument(const Argument &arg) : _M_type(arg._M_type), _M_pos(arg._M_pos) { copy_union(arg); }

        virtual ~Argument();
      private:
        void copy_union(const Argument &arg);

        void destruct_union();
      public:
        Argument &operator=(const Argument &arg)
        {
          if(this != &arg) {
            destruct_union();
            _M_type = arg._M_type;
            copy_union(arg);
          }
          return *this;
        }

        Type type() const { return _M_type; }

        ArgumentValue v() const { return _M_type == TYPE_IMM ? _M_v : ArgumentValue(0); }

        std::uint32_t lvar() const { return _M_type == TYPE_LVAR ? _M_lvar : 0; }

        std::uint32_t arg() const { return _M_type == TYPE_ARG ? _M_arg : 0; }

        std::string ident() const { return _M_type == TYPE_IDENT ? _M_ident : std::string(); }

        const Position pos() const { return _M_pos; }
      };

      class Operation
      {
        std::string _M_name;
        Position _M_pos;
      public:
        Operation(const std::string &name, const Position &pos) : _M_name(name), _M_pos(pos) {}

        virtual ~Operation();

        const std::string &name() const { return _M_name; }

        const Position &pos() const { return _M_pos; }
      };

      class InstructionPair
      {
        std::string _M_instr;
        std::unique_ptr<std::uint32_t> _M_local_var_count;
      public:
        InstructionPair(const std::string &instr) : _M_instr(instr) {}

        InstructionPair(const std::string &instr, std::uint32_t local_var_count) :
          _M_instr(instr), _M_local_var_count(new std::uint32_t(local_var_count)) {}

        InstructionPair(const InstructionPair &instr_pair) :
          _M_instr(instr_pair._M_instr), 
          _M_local_var_count(instr_pair._M_local_var_count.get() != nullptr ? new std::uint32_t(*(instr_pair._M_local_var_count)) : nullptr) {}

        virtual ~InstructionPair();

        const std::string &instr() const { return _M_instr; }

        const std::uint32_t *local_var_count() const { return _M_local_var_count.get(); }
      };

      class Instruction
      {
        std::string _M_instr;
        std::unique_ptr<Operation> _M_op;
        std::unique_ptr<Argument> _M_arg1;
        std::unique_ptr<Argument> _M_arg2;
        std::unique_ptr<std::uint32_t> _M_local_var_count;
        Position _M_pos;
      public:
        Instruction(const std::string &instr, const Position &pos) :
          _M_instr(instr), _M_pos(pos) {}

        Instruction(const std::string &instr, const Argument &arg, const Position &pos) :
          _M_instr(instr), _M_arg1(new Argument(arg)), _M_pos(pos) {}

        Instruction(const std::string &instr, const Argument &arg1, const Argument &arg2, const Position &pos) :
          _M_instr(instr), _M_arg1(new Argument(arg1)), _M_arg2(new Argument(arg2)),
          _M_pos(pos) {}

        Instruction(const std::string &instr, const Operation &op, const Position &pos) :
          _M_instr(instr), _M_op(new Operation(op)), _M_pos(pos) {}

        Instruction(const std::string &instr, const Operation &op, const Argument &arg, const Position &pos) :
          _M_instr(instr), _M_op(new Operation(op)), _M_arg1(new Argument(arg)),
          _M_pos(pos) {}

        Instruction(const std::string &instr, const Operation &op, const Argument &arg1, const Argument &arg2, const Position &pos) :
          _M_instr(instr), _M_op(new Operation(op)), _M_arg1(new Argument(arg1)),
          _M_arg2(new Argument(arg2)), _M_pos(pos) {}

        Instruction(const std::string &instr, const Operation &op, const Argument &arg1, const Argument &arg2, std::uint32_t local_var_count, const Position &pos) :
          _M_instr(instr), _M_op(new Operation(op)), _M_arg1(new Argument(arg1)),
          _M_arg2(new Argument(arg2)), _M_local_var_count(new std::uint32_t(local_var_count)), _M_pos(pos) {}

        Instruction(const InstructionPair &instr_pair, const Operation &op, const Position &pos) :
          _M_instr(instr_pair.instr()), _M_op(new Operation(op)),
          _M_local_var_count(instr_pair.local_var_count() != nullptr ? new std::uint32_t(*(instr_pair.local_var_count())) : nullptr),
          _M_pos(pos) {}

        Instruction(const InstructionPair &instr_pair, const Operation &op, const Argument &arg, const Position &pos) :
          _M_instr(instr_pair.instr()), _M_op(new Operation(op)), _M_arg1(new Argument(arg)),
          _M_local_var_count(instr_pair.local_var_count() != nullptr ? new std::uint32_t(*(instr_pair.local_var_count())) : nullptr),
          _M_pos(pos) {}

        Instruction(const InstructionPair &instr_pair, const Operation &op, const Argument &arg1, const Argument &arg2, const Position &pos) :
          _M_instr(instr_pair.instr()), _M_op(new Operation(op)), _M_arg1(new Argument(arg1)),
          _M_arg2(new Argument(arg2)),
          _M_local_var_count(instr_pair.local_var_count() != nullptr ? new std::uint32_t(*(instr_pair.local_var_count())) : nullptr),
          _M_pos(pos) {}

        Instruction(const Instruction &instr) :
          _M_instr(instr._M_instr),
          _M_op(instr._M_op.get() != nullptr ? new Operation(*(instr._M_op)) : nullptr),
          _M_arg1(instr._M_arg1.get() != nullptr ? new Argument(*(instr._M_arg1)) : nullptr),
          _M_arg2(instr._M_arg2.get() != nullptr ? new Argument(*(instr._M_arg2)) : nullptr),
          _M_local_var_count(instr._M_local_var_count.get() != nullptr ? new std::uint32_t(*(instr._M_local_var_count)) : nullptr),
          _M_pos(instr._M_pos) {}

        virtual ~Instruction();

        const std::string &instr() const { return _M_instr; }

        const Operation *op() const { return _M_op.get(); }

        const Argument *arg1() const { return _M_arg1.get(); }

        const Argument *arg2() const { return _M_arg2.get(); }

        const std::uint32_t *local_var_count() const { return _M_local_var_count.get(); }
        
        const Position &pos() const { return _M_pos; }
      };
      
      class Label
      {
        std::string _M_ident;
        Position _M_pos;
      public:
        Label(const std::string &ident, const Position &pos) : _M_ident(ident), _M_pos(pos) {}

        virtual ~Label();

        const std::string &ident() const { return _M_ident; }

        const Position &pos() const { return _M_pos; }
      };


      class FunctionLine
      {
        std::unique_ptr<Label> _M_label;
        std::unique_ptr<Instruction> _M_instr;
      public:
        FunctionLine() {}

        FunctionLine(const Label &label) : _M_label(new Label(label)) {}

        FunctionLine(const Instruction &instr) : _M_instr(new Instruction(instr)) {}

        FunctionLine(const Label &label, const Instruction &instr) :
          _M_label(new Label(label)), _M_instr(new Instruction(instr)) {}
          
        FunctionLine(const FunctionLine &line) :
          _M_label(line._M_label != nullptr ? new Label(*(line._M_label)) : nullptr),
          _M_instr(line._M_instr != nullptr ? new Instruction(*(line._M_instr)) : nullptr) {}

        const Label *label() const { return _M_label.get(); }

        const Instruction *instr() const { return _M_instr.get(); }
      };

      class Function
      {
        std::uint32_t _M_arg_count;
        std::shared_ptr<std::list<FunctionLine>> _M_lines;
        Position _M_pos;
      public:
        Function(std::uint32_t arg_count, const std::shared_ptr<std::list<FunctionLine>> &lines, const Position &pos) :
          _M_arg_count(arg_count), _M_lines(lines) {}

        std::uint32_t arg_count() const { return _M_arg_count; }

        const std::list<FunctionLine> &lines() const { return *_M_lines; }
      };

      enum Modifier
      {
        PRIVATE,
        PUBLIC
      };

      class FunctionDefinition : public Definition
      {
        Modifier _M_modifier;
        std::string _M_ident;
        Function _M_fun;
        Position _M_pos;
      public:
        FunctionDefinition(const std::string &ident, const Function &fun, const Position &pos) :
          Definition(pos), _M_modifier(PRIVATE), _M_ident(ident), _M_fun(fun) {}

        FunctionDefinition(Modifier modifier, const std::string &ident, const Function &fun, const Position &pos) :
          Definition(pos), _M_modifier(modifier), _M_ident(ident), _M_fun(fun) {}

        ~FunctionDefinition();

        Modifier modifier() const { return _M_modifier; }

        const std::string ident() const { return _M_ident; }

        const Function &fun() const { return _M_fun; }
      };

      class VariableDefinition : public Definition
      {
        Modifier _M_modifier;
        std::string _M_ident;
        Value _M_value;
      public:
        VariableDefinition(const std::string &ident, const Value &value, const Position &pos) :
          Definition(pos), _M_modifier(PRIVATE), _M_ident(ident), _M_value(value) {}

        VariableDefinition(Modifier modifier, const std::string &ident, const Value &value, const Position &pos) :
          Definition(pos), _M_modifier(modifier), _M_ident(ident), _M_value(value) {}

        ~VariableDefinition();

        Modifier modifier() const { return _M_modifier; }

        const std::string &ident() const { return _M_ident; }

        const Value &value() const { return _M_value; }
      };
      
      class EntryDefinition : public Definition
      {
        std::string _M_ident;
      public:
        EntryDefinition(const std::string &ident, const Position &pos) : Definition(pos), _M_ident(ident) {}
        
        ~EntryDefinition();

        const std::string &ident() const { return _M_ident; }        
      };
    }
  }
}

#endif
