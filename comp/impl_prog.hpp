/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_PROG_HPP
#define _IMPL_PROG_HPP

#include <memory>
#include <letin/comp.hpp>

namespace letin
{
  namespace comp
  {
    namespace impl
    {
      class ImplProgram : public Program
      {
        std::unique_ptr<uint8_t> _M_ptr;
        std::size_t _M_size;
      public:
        ImplProgram(void *ptr, std::size_t size) :
          _M_ptr(reinterpret_cast<uint8_t *>(ptr)), _M_size(size) {}
        
        ~ImplProgram(); 

        const void *ptr() const;

        std::size_t size() const;
      };
    }
  }
}

#endif
