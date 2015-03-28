/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_COMP_HPP
#define _IMPL_COMP_HPP

#include <letin/comp.hpp>

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class ImplCompiler : public Compiler
      {
        std::vector<std::string> _M_include_dirs;
      public:
        ImplCompiler() {}

        ~ImplCompiler();

        Program *compile(const std::vector<Source> & sources, std::list<Error> &errors, bool is_relocable);

        void add_include_dir(const std::string &dir_name);
      };
    }
  }
}

#endif
