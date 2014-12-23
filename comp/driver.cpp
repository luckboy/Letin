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
        Lexer lexer(&(ss.istream()));
        Parser parser(*this, lexer);
        return parser.parse() != 0;
      }
    }
  }
}
