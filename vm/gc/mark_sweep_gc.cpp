/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <atomic>
#include <mutex>
#include <letin/vm.hpp>
#include "mark_sweep_gc.hpp"

using namespace std;

namespace letin
{
  namespace vm
  {
    namespace impl
    {
      MarkSweepGarbageCollector::Header MarkSweepGarbageCollector::_S_nil;

      MarkSweepGarbageCollector::MarkSweepGarbageCollector(Allocator *alloc,
          unsigned int interval_usecs) :
        ImplGarbageCollectorBase(alloc, interval_usecs),
        _M_list_first(&_S_nil),
        _M_stack_top(&_S_nil),
        _M_immortal_list_first(&_S_nil) {}

      MarkSweepGarbageCollector::~MarkSweepGarbageCollector()
      {
        free_list(_M_list_first);
        free_list(_M_immortal_list_first);
      }

      void MarkSweepGarbageCollector::collect()
      {
        lock_guard<GarbageCollector> guard(*this);
        lock_guard<Threads> guard2(_M_threads);
        mark();
        sweep();
      }

      void *MarkSweepGarbageCollector::allocate(size_t size, ThreadContext *context)
      {
        void *orig_ptr = _M_alloc->allocate(sizeof(Header) + size);
        if(orig_ptr == nullptr) return nullptr;
        Header *header = reinterpret_cast<Header *>(orig_ptr);
        new(header) Header();
        atomic_thread_fence(memory_order_release);
        void *ptr = (reinterpret_cast<char *>(orig_ptr) + sizeof(Header));
        if(context != nullptr) context->regs().tmp_ptr = ptr;
        atomic_thread_fence(memory_order_release);
        {
          lock_guard<GarbageCollector> guard(*this);
          add_header(header);
        }
        return ptr;
      }

      void *MarkSweepGarbageCollector::allocate_immortal_area(size_t size)
      {
        void *orig_ptr = _M_alloc->allocate(sizeof(Header) + size);
        if(orig_ptr == nullptr) return nullptr;
        Header *header = reinterpret_cast<Header *>(orig_ptr);
        new(header) Header();
        void *ptr = (reinterpret_cast<char *>(orig_ptr) + sizeof(Header));
        {
          lock_guard<GarbageCollector> guard(*this);
          add_immortal_header(header);
        }
        return ptr;
      }

      void MarkSweepGarbageCollector::mark()
      {
        for(auto context : _M_thread_contexts) {
          for(size_t i = 0; i < context->regs().sec; i++) {
            Value elem = context->stack_elem(i);
            if((elem.type() == VALUE_TYPE_REF || elem.type() == VALUE_TYPE_CANCELED_REF) &&
                !elem.raw().r.has_nil())
              mark_from_object(elem.raw().r.ptr());
          }
          if(!context->regs().rv.raw().r.has_nil())
            mark_from_object(context->regs().rv.raw().r.ptr());
          if(context->regs().tmp_ptr != nullptr)
            ptr_to_header(context->regs().tmp_ptr)->stack_prev = &_S_nil;
          mark_from_object(context->regs().tmp_r.ptr());
        }
        for(auto context : _M_vm_contexts) {
          for(size_t i = 0; i < context->var_count(); i++) {
            if(context->vars()[i].type() == VALUE_TYPE_REF && !context->vars()[i].raw().r.has_nil())
              mark_from_object(context->vars()[i].raw().r.ptr());
          }
        }
      }

      void MarkSweepGarbageCollector::sweep()
      {
        Header **header_ptr = &_M_list_first;
        while(*header_ptr != &_S_nil) {
          if(!(*header_ptr)->is_marked()) {
            Header *next = (*header_ptr)->list_next;
            _M_alloc->free(reinterpret_cast<void *>(*header_ptr));
            *header_ptr = next;
          } else {
            (*header_ptr)->stack_prev = nullptr;
            header_ptr = &((*header_ptr)->list_next);
          }
        }
      }

      void MarkSweepGarbageCollector::mark_from_object(Object *object)
      {
        Header *header = object_to_header(object);
        mark_and_push_header(header);
        while(!is_empty_stack()) {
          Object *top_object = header_to_object(pop_header());
          switch(top_object->type() & ~OBJECT_TYPE_UNIQUE) {
            case OBJECT_TYPE_RARRAY:
              for(size_t i = 0; i < top_object->length(); i++) {
                Reference elem_ref = top_object->raw().rs[i];
                if(!elem_ref.has_nil()) {
                  Header *elem_header = object_to_header(elem_ref.ptr());
                  if(!elem_header->is_marked()) mark_and_push_header(elem_header);
                }
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t i = 0; i < top_object->length(); i++) {
                if(top_object->raw().tuple_elem_types()[i].raw() == VALUE_TYPE_REF ||
                    top_object->raw().tuple_elem_types()[i].raw() == VALUE_TYPE_CANCELED_REF) {
                  Reference elem_ref = top_object->raw().tes[i].raw().r;
                  if(!elem_ref.has_nil()) {
                    Header *elem_header = object_to_header(elem_ref.ptr());
                    if(!elem_header->is_marked()) mark_and_push_header(elem_header);
                  }
                }
              }
              break;
          }
        }
      }

      size_t MarkSweepGarbageCollector::header_size() { return sizeof(Header); }
    }
  }
}
