/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _IMPL_VM_BASE_HPP
#define _IMPL_VM_BASE_HPP

#include <letin/vm.hpp>
#include "vm.hpp"
#include "impl_env.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class ImplVirtualMachineBase : public VirtualMachine
      {
      protected:
        ImplEnvironment _M_env;
        bool _M_has_entry;
        std::size_t _M_entry;

        ImplVirtualMachineBase(Loader *loader, GarbageCollector *gc, NativeFunctionHandler *native_fun_handler, EvaluationStrategy *eval_strategy) :
          VirtualMachine(loader, gc, native_fun_handler, eval_strategy) {}
      public:
        ~ImplVirtualMachineBase();

        bool load(const std::vector<std::pair<void *, std::size_t>> &pairs, std::list<LoadingError> *errors, bool is_auto_freeing);
      private:
        bool load_prog(std::size_t i, Program *prog, std::size_t fun_offset, std::size_t var_offset, std::list<LoadingError> *errors, void *data_to_free);
      public:
        Thread start(std::size_t i, const std::vector<Value> &args, std::function<void (const ReturnValue &)> fun, bool is_force);
      protected:
        virtual ReturnValue start_in_thread(std::size_t i, const std::vector<Value> &args, ThreadContext &context, bool is_force) = 0;
      public:
        Environment &env();

        bool has_entry();

        std::size_t entry();
      };
    }
  }
}

#endif
