/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
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
        _M_istream(new std::ifstream(file_name)), _M_has_auto_freeing(false) {}

      SourceStream(const std::string &file_name) :
        _M_istream(new std::ifstream(file_name)), _M_has_auto_freeing(false) {}

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
    
    class Error
    {
      Source _M_source;
      std::size_t _M_line;
      std::size_t _M_column;
      std::string _M_msg;
    public:
      Error(const Source &source, int line, std::size_t column, const char *msg) :
        _M_source(source), _M_line(line), _M_column(column), _M_msg(msg) {}

      Error(const Source &source, int line, int column, const std::string &msg) :
        _M_source(source), _M_line(line), _M_column(column), _M_msg(msg) {}
      
      const Source &source() const { return _M_source; }

      int line() const { return _M_line; }

      int column() const { return _M_column; }

      std::string to_str() const;
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
      Compiler();
    public:
      virtual ~Compiler();
      
      virtual Program *compile(const std::vector<Source> & sources, std::vector<Error> &errors) = 0;

      Program *compile(const char *file_name, std::vector<Error> &errors);
    };
  }
}

#endif
