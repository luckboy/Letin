/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LEXER_HPP
#define _LEXER_HPP

#ifndef yyFlexLexerOnce
#undef yyFlexLexer
#define yyFlexLexer     LetinCompImplFlexLexer
#include <FlexLexer.h>
#endif
#include "parser.hpp"

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class Lexer : public LetinCompImplFlexLexer
      {
        typedef Parser::token token;
        Parser::semantic_type *lval;
        Parser::location_type *loc;
        std::string buffer;
      public:
        Lexer(std::istream *is) : LetinCompImplFlexLexer(is) {}

        virtual ~Lexer();

        int lex(Parser::semantic_type *lval, Parser::location_type *loc)
        {
          this->lval = lval;
          this->loc = loc;
          return yylex();
        }
      private:
        int yylex(); 
      };
    }
  }
}

#endif
