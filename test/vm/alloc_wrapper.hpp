/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _ALLOC_WRAPPER_HPP
#define _ALLOC_WRAPPER_HPP

#include <cstddef>
#include <vector>
#include <letin/vm.hpp>

namespace letin
{
  namespace vm
  {
    namespace test
    {
      enum AllocatorOperationEnum { ALLOC, FREE };
      
      struct AllocatorOperation
      {
        AllocatorOperationEnum op;
        void *ptr;

        AllocatorOperation(AllocatorOperationEnum op, void *ptr) : op(op), ptr(ptr) {}

        bool operator==(const AllocatorOperation &alloc_op) const
        { return op == alloc_op.op && ptr == alloc_op.ptr; }
      };

      class AllocatorWrapper : public Allocator
      {
        std::unique_ptr<Allocator> _M_alloc;

        std::vector<AllocatorOperation> _M_alloc_ops;
      public:
        AllocatorWrapper(Allocator *alloc) : _M_alloc(alloc) {}

        virtual ~AllocatorWrapper();

        void *allocate(std::size_t size);

        void free(void *ptr);

        const std::vector<AllocatorOperation> alloc_ops() const
        { return _M_alloc_ops; }
      };
      
      inline AllocatorOperation alloc_op(Reference ref, GarbageCollector &gc)
      {
        void *ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ref.ptr()) - gc.header_size());
        return AllocatorOperation(ALLOC, ptr);
      }

      inline AllocatorOperation free_op(Reference ref, GarbageCollector &gc)
      {
        void *ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ref.ptr()) - gc.header_size());
        return AllocatorOperation(FREE, ptr);
      }
    }
  }
}

#endif
