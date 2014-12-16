/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sstream>
#include <string>
#include <letin/comp.hpp>

using namespace std;

namespace letin
{
  namespace comp
  {
    //
    // An Error class.
    //
    
    string Error::to_str() const
    {
      ostringstream oss;
      oss << _M_source.file_name() << ":" << _M_line << "." << _M_column << ":" << _M_msg;
      return oss.str();
    }
    
    //
    // A Program class.
    //

    Program::~Program() {}

    //
    // A Compiler class.
    //
    
    Compiler::~Compiler() {}
  }
}
