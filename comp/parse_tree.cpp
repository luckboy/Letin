/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "parse_tree.hpp"

using namespace std;

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      //
      // A Definition class.
      //
      
      Definition::~Definition() {}
      
      //
      // A ParseTree class.
      //

      ParseTree::~ParseTree() {}

      //
      // An ArgumentValue class.
      //
      
      ArgumentValue::~ArgumentValue() { destruct_union(); }
      
      void ArgumentValue::copy_union(const ArgumentValue &value)
      {
        switch(_M_type) {
          case TYPE_INT:
            _M_i = value._M_i;
            break;
          case TYPE_FLOAT:
            _M_f = value._M_f;
            break;
          case TYPE_FUN_ADDR:
            new (&_M_fun) string(value._M_fun);
            break;
        }
      }
      
      //
      // A Value class.
      //
      
      Value::~Value() {}

      void Value::copy_union(const Value &value)
      {
        switch(_M_type) {
          case TYPE_INT:
            _M_i = value._M_i;
            break;
          case TYPE_FLOAT:
            _M_f = value._M_f;
            break;
          case TYPE_REF:
            new (&_M_object) shared_ptr<Object>(_M_object.get() != nullptr ? value._M_object.get() : nullptr);
            break;
          case TYPE_FUN_ADDR:
            new (&_M_fun) string(value._M_fun);
            break;
        }
      }

      void Value::destruct_union()
      {
        switch(_M_type) {
          case TYPE_REF:
            _M_object.~unique_ptr<Object>();
            break;
          case TYPE_FUN_ADDR:
            _M_fun.~string();
            break;
          default:
            break;
        }
      }

      //
      // An Argument class.
      //

      Argument::~Argument() { destruct_union(); }

      void Argument::copy_union(const Argument &arg)
      {
        switch(_M_type) {
          case TYPE_IMM:
            new (&_M_v) ArgumentValue(arg._M_v);
            break;
          case TYPE_LVAR:
            _M_lvar = arg._M_lvar;
            break;
          case TYPE_ARG:
            _M_arg = arg._M_arg;
            break;
          case TYPE_IDENT:
            new (&_M_ident) string(arg._M_ident);
        }
      }

      void Argument::destruct_union()
      {
        switch(_M_type) {
          case TYPE_IMM:
            _M_v.~ArgumentValue();
            break;
          case TYPE_IDENT:
            _M_ident.~string();
            break;
          default:
            break;
        }
      }
      
      //
      // An Operation class.
      //

      Operation::~Operation() {}

      //
      // A Label class.
      //
      
      Label::~Label() {}
      
      //
      // An Instruction class.
      //

      Instruction::~Instruction() {}

      //
      // A FunctionDefinition class.
      //
      
      FunctionDefinition::~FunctionDefinition() {}
      
      //
      // A VariableDefinition class.
      //
      
      VariableDefinition::~VariableDefinition() {}

      //
      // An EntryDefinition class.
      //

      EntryDefinition::~EntryDefinition() {}
    }
  }
}
