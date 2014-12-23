/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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
        std::list<Definition> _M_defs;
      public:
        ParseTree() {}

        virtual ~ParseTree();

        const std::list<Definition> &defs() const { return _M_defs; }

        void add_def(const Definition &def) { _M_defs.push_back(def); }
      };

      class ArgumentValue
      {
      public:
        enum Type
        {
          TYPE_INT,
          TYPE_FLOAT,
          TYPE_REF,
          TYPE_FUN_ADDR
        };
      protected:
        Type _M_type;
        union
        {
          std::int64_t _M_i;
          double _M_f;
          std::string _M_fun;
          std::unique_ptr<Object> _M_object;
        };
      public:
        ArgumentValue(int i) : _M_type(TYPE_INT) { _M_i = i; }

        ArgumentValue(std::int64_t i) : _M_type(TYPE_INT) { _M_i = i; }

        ArgumentValue(double f) : _M_type(TYPE_FLOAT) { _M_f = f; }

        ArgumentValue(const std::string &fun) : _M_type(TYPE_FUN_ADDR) { _M_fun = fun; }

        ArgumentValue(Object *object) : _M_type(TYPE_REF) { _M_object = std::unique_ptr<Object>(object); }

        ArgumentValue(const ArgumentValue &value) { *this = value; }

        virtual ~ArgumentValue();
      private:
        void copy_union(const ArgumentValue &value);

        void destruct_union();
      public:
        ArgumentValue &operator=(const ArgumentValue &value)
        {
          destruct_union();
          _M_type = value._M_type;
          copy_union(value);
          return *this;
        }

        Type type() const { return _M_type; }

        std::int64_t i() const { return _M_type == TYPE_INT ? _M_i : 0; }

        double f() const { return _M_type == TYPE_FLOAT ? _M_f : 0.0; }

        std::string fun() const { return _M_type == TYPE_FUN_ADDR ? _M_fun : std::string(); }

        Object *object() const { return _M_object.get(); }
      };

      class Value : public ArgumentValue
      {
        Position _M_pos;
      public:
        Value(int i, const Position &pos) :
          ArgumentValue(i), _M_pos(pos) {}

        Value(std::int64_t i, const Position &pos) :
          ArgumentValue(i), _M_pos(pos) {}

        Value(double f, const Position &pos) :
          ArgumentValue(f), _M_pos(pos) {}

        Value(const std::string &fun, const Position &pos) :
          ArgumentValue(fun), _M_pos(pos) {}

        Value(Object *object, const Position &pos) :
          ArgumentValue(object), _M_pos(pos) {}

        ~Value();

        const Position &pos() const { return _M_pos; }
      };


      class Object
      {
        std::string _M_type;
        std::list<Value> _M_elems;
      public:
        Object(const std::string &str) : _M_type("iarray8")
        { for(auto c : str) _M_elems.push_back(Value(static_cast<int>(c), Position())); }

        Object(const std::string &type, const std::list<Value> &elems) : _M_type(type), _M_elems(elems) {}

        virtual ~Object() {}

        std::string type() const { return _M_type; }

        const std::list<Value> elems() const { return _M_elems; }

        void add_elem(const Value &value) { _M_elems.push_back(value); }
      };

      enum IndexArgumentEnum { LVAR, ARG };

      enum IdentifierArgumentEnum { GVAR, LABEL };

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
          _M_type(TYPE_IMM), _M_pos(pos) { _M_v = v; }

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
          _M_type(TYPE_IDENT), _M_pos(pos) { _M_ident = ident; }

        Argument(const Argument &arg)  { *this = arg; }

        virtual ~Argument();
      private:
        void copy_union(const Argument &arg);

        void destruct_union();
      public:
        Argument &operator=(const Argument &arg)
        {
          destruct_union();
          _M_type = arg._M_type;
          copy_union(arg);
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
        std::string _M_ident;
        Position _M_pos;
      public:
        Operation(const std::string &ident, const Position &pos) : _M_ident(ident), _M_pos(pos) {}

        virtual ~Operation();

        const std::string &ident() const { return _M_ident; }

        const Position &pos() const { return _M_pos; }
      };

      class Instruction
      {
        std::string _M_instr;
        std::unique_ptr<Operation> _M_op;
        std::unique_ptr<Argument> _M_arg1;
        std::unique_ptr<Argument> _M_arg2;
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

        Instruction(const Instruction &instr) :
          _M_instr(instr._M_instr), _M_op(new Operation(*(instr._M_op))),
          _M_arg1(new Argument(*(instr._M_arg1))),
          _M_arg2(new Argument(*(instr._M_arg2))), _M_pos(instr._M_pos) {}

        virtual ~Instruction();

        const std::string &instr() const { return _M_instr; }

        const Operation *op() const { return _M_op.get(); }

        const Argument *arg1() const { return _M_arg1.get(); }

        const Argument *arg2() const { return _M_arg2.get(); }
      };

      class FunctionLine
      {
        std::unique_ptr<std::string> _M_label;
        std::unique_ptr<Instruction> _M_instr;
      public:
        FunctionLine() {}

        FunctionLine(const std::string &label) : _M_label(new std::string(label)) {}

        FunctionLine(const Instruction &instr) : _M_instr(new Instruction(instr)) {}

        FunctionLine(const std::string &label, const Instruction &instr) :
          _M_label(new std::string(label)), _M_instr(new Instruction(instr)) {}
          
        FunctionLine(const FunctionLine &line) :
          _M_label(new std::string(*(line._M_label))), _M_instr(new Instruction(*(line._M_instr))) {}

        const std::string *label() const { return _M_label.get(); }

        const Instruction *instr() const { return _M_instr.get(); }
      };

      class FunctionDefinition : public Definition
      {
        std::string _M_ident;
        std::uint32_t _M_arg_count;
        std::list<FunctionLine> _M_lines;
        Position _M_pos;
      public:
        FunctionDefinition(const std::string &ident, std::uint32_t arg_count, const std::list<FunctionLine> &lines, const Position &pos) :
          Definition(pos), _M_ident(ident), _M_arg_count(arg_count), _M_lines(lines) {}

        ~FunctionDefinition();

        const std::string ident() const { return _M_ident; }

        std::uint32_t arg_count() const { return _M_arg_count; }

        const std::list<FunctionLine> &lines() const { return _M_lines; }

        void add_line(const FunctionLine &line) { _M_lines.push_back(line); }
      };

      class VariableDefinition : public Definition
      {
        std::string _M_ident;
        Value _M_value;
      public:
        VariableDefinition(const std::string &ident, const Value &value, const Position &pos) :
          Definition(pos), _M_ident(ident), _M_value(value) {}

        ~VariableDefinition();

        const std::string &ident() const { return _M_ident; }

        const Value &value() const { return _M_value; }
      };
    }
  }
}

#endif
