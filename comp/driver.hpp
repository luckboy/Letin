/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _DRIVER_HPP
#define _DRIVER_HPP

#include <unordered_map>
#include <utility>
#include <letin/comp.hpp>
#include "parse_tree.hpp"

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class Driver
      {
        Source _M_source;
        ParseTree &_M_parse_tree;
        const std::vector<std::string> &_M_include_dirs;
        std::list<Error> &_M_errors;
        std::string _M_file_name;
        std::unordered_map<std::string, NumberValue> _M_value_defs;
      public:
        Driver(ParseTree &parse_tree, const std::vector<std::string> &include_dirs,
               std::list<Error> &errors) :
          _M_parse_tree(parse_tree), _M_include_dirs(include_dirs), _M_errors(errors) {}

        virtual ~Driver();
        
        bool parse(const Source &source);

        bool parse_included_file(const std::string &file_name);

        const Source &source() const { return _M_source; }

        const std::string &file_name() const { return _M_file_name; }

        std::string &file_name() { return _M_file_name; }
        
        const ParseTree &parse_tree() const { return _M_parse_tree; }
        
        ParseTree &parse_tree() { return _M_parse_tree; }
        
        const std::list<Error> &errors() const { return _M_errors; }

        void add_error(const Error &error) { _M_errors.push_back(error); }

        bool has_value_def(const std::string &ident) const
        { return _M_value_defs.find(ident) != _M_value_defs.end(); }

        const NumberValue &value_def(const std::string &ident)
        { return _M_value_defs[ident]; }

        void add_value_def(const std::string &ident, const NumberValue &value)
        { _M_value_defs.insert(std::make_pair(ident, value)); }
      };
    }
  }
}

#endif
