/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "driver.hpp"
#include "lexer.hpp"
#include "parser.hpp"

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      Driver::~Driver() {}

      bool Driver::parse(const Source &source)
      {
        SourceStream ss(source.open());
        _M_source = source;
        _M_file_name = source.file_name();
        if(!ss.istream().good()) {
          _M_errors.push_back(Error(Position(source, 1, 1), "can't open file"));
          return false;
        }
        Lexer lexer(&(ss.istream()));
        Parser parser(*this, lexer);
        return parser.parse() == 0;
      }
    }
  }
}
