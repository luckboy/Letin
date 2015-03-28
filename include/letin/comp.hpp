/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_COMP_HPP
#define _LETIN_COMP_HPP

#include <cstddef>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace letin
{
  namespace comp
  {
    class SourceStream
    {
      std::istream *_M_istream;
      bool _M_has_auto_freeing;
    public:
      SourceStream(std::istream &is) :
        _M_istream(&is), _M_has_auto_freeing(false) {}

      SourceStream(const char *file_name) :
        _M_istream(new std::ifstream(file_name)), _M_has_auto_freeing(true) {}

      SourceStream(const std::string &file_name) :
        _M_istream(new std::ifstream(file_name)), _M_has_auto_freeing(true) {}

      ~SourceStream() { if(_M_has_auto_freeing) delete _M_istream; }

      std::istream &istream() { return *_M_istream; }
    };

    class Source
    {
      std::string _M_file_name;
      std::istream *_M_istream;
    public:
      Source() :
        _M_file_name("<stdin>"), _M_istream(&std::cin) {}

      Source(const char *file_name) :
        _M_file_name(file_name), _M_istream(nullptr) {}

      Source(const std::string &file_name) :
        _M_file_name(file_name), _M_istream(nullptr) {}

      Source(const char *file_name, std::istream &is) :
        _M_file_name(file_name), _M_istream(&is) {}

      Source(const std::string &file_name, std::istream &is) :
        _M_file_name(file_name), _M_istream(&is) {}

      const std::string &file_name() const { return _M_file_name; }

      SourceStream open() const
      { 
        if(_M_istream != nullptr)
          return SourceStream(*_M_istream);
        else
          return SourceStream(_M_file_name);
      }
    };

    class Position
    {
      Source _M_source;
      std::size_t _M_line;
      std::size_t _M_column;
    public:
      Position() {}

      Position(const Source &source, std::size_t line, std::size_t column) :
        _M_source(source), _M_line(line), _M_column(column) {}

      const Source &source() const { return _M_source; }

      std::size_t line() const { return _M_line; }

      std::size_t column() const { return _M_column; }
    };

    class Error
    {
      Position _M_pos;
      std::string _M_msg;
    public:
      Error(const Position &pos, const char *msg) : _M_pos(pos), _M_msg(msg) {}

      Error(const Position &pos, const std::string &msg) : _M_pos(pos), _M_msg(msg) {}

      const Position &pos() const { return _M_pos; }

      const std::string &msg() const { return _M_msg; }
    };

    class Program
    {
    protected:
      Program() {}
    public:
      virtual ~Program();

      virtual const void *ptr() const = 0;

      virtual std::size_t size() const = 0;
    };

    class Compiler
    {
    protected:
      Compiler() {}
    public:
      virtual ~Compiler();

      virtual Program *compile(const std::vector<Source> &sources, std::list<Error> &errors, bool is_relocable = true) = 0;

      Program *compile(const char *file_name, std::list<Error> &errors, bool is_relocable = true);
      
      virtual void add_include_dir(const std::string &dir_name) = 0;

      void add_include_dir(const char *dir_name) { add_include_dir(std::string(dir_name)); };
    };

    Compiler *new_compiler();

    std::ostream &operator<<(std::ostream &os, const Position &pos);

    std::ostream &operator<<(std::ostream &os, const Error &error);
  }
}

#endif
