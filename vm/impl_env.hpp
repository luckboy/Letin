/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_ENV_HPP
#define _IMPL_ENV_HPP

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <letin/vm.hpp>
#include "vm.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplEnvironment : public Environment, public VirtualMachineContext
      {
        std::unique_ptr<Function []> _M_funs;
        std::size_t _M_fun_count;
        std::unique_ptr<Value []> _M_vars;
        std::size_t _M_var_count;
        std::unique_ptr<char []> _M_data_to_free;
      public:
        ImplEnvironment() : _M_funs(nullptr), _M_fun_count(0), _M_vars(nullptr), _M_var_count(0) {}

        ~ImplEnvironment();

        void set_fun_count(std::size_t fun_count)
        {
          _M_funs = std::unique_ptr<Function []>(new Function[fun_count]);
          _M_fun_count = fun_count;
        }

        void set_var_count(std::size_t var_count)
        { 
          _M_vars = std::unique_ptr<Value []>(new Value[var_count]);
          _M_var_count = var_count;
        }

        Function fun(std::size_t i);

        Value var(std::size_t i);

        const Function *funs() const;

        Function *funs();

        std::size_t fun_count() const;

        const Value *vars() const;

        Value *vars();

        std::size_t var_count() const;

        void set_fun(std::size_t i, const Function &fun);

        void set_var(std::size_t i, const Value &value);

        void set_auto_freeing(void *ptr)
        { _M_data_to_free = std::unique_ptr<char []>(reinterpret_cast<char *>(ptr)); }
      };
    }
  }
}

#endif
