/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <vector>
#include <letin/comp.hpp>

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

    Program *Compiler::compile(const char *file_name, list<Error> &errors)
    {
      vector<Source> sources;
      sources.push_back(Source(file_name));
      return compile(sources, errors);
    }
  }
}
