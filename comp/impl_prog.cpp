/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "impl_prog.hpp"

using namespace std;

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      ImplProgram::~ImplProgram() {}

      const void *ImplProgram::ptr() const { return reinterpret_cast<const void *>(_M_ptr.get()); }

      size_t ImplProgram::size() const { return _M_size; }
    }
  }
}
