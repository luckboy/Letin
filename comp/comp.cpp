/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <vector>
#include <letin/comp.hpp>
#include "impl_comp.hpp"

using namespace std;

namespace letin
{
  namespace comp
  {
    //
    // A Program class.
    //

    Program::~Program() {}

    //
    // A Compiler class.
    //
    
    Compiler::~Compiler() {}

    Program *Compiler::compile(const char *file_name, list<Error> &errors, bool is_relocable)
    {
      vector<Source> sources;
      sources.push_back(Source(file_name));
      return compile(sources, errors, is_relocable);
    }

    //
    // Other functions.
    //

    Compiler *new_compiler() { return new impl::ImplCompiler(); }

    ostream &operator<<(ostream &os, const Position &pos)
    {
      os << pos.source().file_name() << ": " << pos.line() << "." << pos.column();
      return os;
    }

    ostream &operator<<(ostream &os, const Error &error)
    {
      os << error.pos() << ": " << error.msg();
      return os;
    }
  }
}
