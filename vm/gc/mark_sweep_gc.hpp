/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _GC_MARK_SWEEP_GC_HPP
#define _GC_MARK_SWEEP_GC_HPP

#include <mutex>
#include "impl_gc_base.hpp"

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      class MarkSweepGarbageCollector : public ImplGarbageCollectorBase
      {
        struct Header
        {
          Header *list_next;
          Header *stack_prev;

          Header() : list_next(nullptr), stack_prev(nullptr) {}

          bool is_marked() const { return stack_prev != nullptr; }
        };
        
        static Header _S_nil;

        Header *_M_list_first;
        Header *_M_stack_top;
        Header *_M_immortal_list_first;

        bool is_emtpy_list()
        { return _M_list_first == &_S_nil; }

        void add_header(Header *header)
        { header->list_next = _M_list_first; _M_list_first = header; }

        bool is_empty_stack()
        { return _M_stack_top == &_S_nil; }
        
        void mark_and_push_header(Header *header)
        { header->stack_prev = _M_stack_top; _M_stack_top = header; }

        Header *pop_header()
        {
          Header *saved_stack_top = _M_stack_top;
          _M_stack_top = _M_stack_top->stack_prev;
          return saved_stack_top;
        }

        void add_immortal_header(Header *header)
        { header->list_next = _M_immortal_list_first; _M_immortal_list_first = header; }

        static Header *ptr_to_header(void *ptr)
        { return reinterpret_cast<Header *>(reinterpret_cast<char *>(ptr) - sizeof(Header)); }

        static Header *object_to_header(Object *object)
        { return reinterpret_cast<Header *>(reinterpret_cast<char *>(object) - sizeof(Header)); }

        static Object *header_to_object(Header *header)
        { return reinterpret_cast<Object *>(reinterpret_cast<char *>(header) + sizeof(Header));}

        void free_list(Header *header)
        {
          while(header != &_S_nil) {
            Header *next = header->list_next;
            _M_alloc->free(reinterpret_cast<void *>(header));
            header = next;
          }
        }
      public:
        MarkSweepGarbageCollector(Allocator *alloc, unsigned int interval_usecs = 100);

        ~MarkSweepGarbageCollector();

        void collect();

        void *allocate(std::size_t size, ThreadContext *context);

        void *allocate_immortal_area(std::size_t size);

        void mark();

        void sweep();

        void mark_from_object(Object *object);

        std::size_t header_size();
      };
    }
  }
}

#endif
