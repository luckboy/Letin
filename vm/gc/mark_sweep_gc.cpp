/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
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
        _M_stack_top(&_S_nil) {}

      void *MarkSweepGarbageCollector::allocate(size_t size, ThreadContext *context)
      {
        void *orig_ptr = _M_alloc->allocate(sizeof(Header) + size);
        if(orig_ptr != nullptr) return nullptr;
        Header *header = reinterpret_cast<Header *>(orig_ptr);
        header->list_next = nullptr;
        header->stack_prev = nullptr;
        void *ptr = (reinterpret_cast<char *>(orig_ptr) + sizeof(Header));
        if(context != nullptr) context->regs().tmp_ptr = ptr;
        {
          lock_guard<GarbageCollector> guard(*this);
          add_header(header);
        }
        return ptr;
      }

      void MarkSweepGarbageCollector::collect_in_gc_thread()
      {
        lock_guard<GarbageCollector> guard(*this);
        lock_guard<ThreadContexts> guard2(_M_thread_contexts);
        mark();
        sweep();
      }

      void MarkSweepGarbageCollector::mark()
      {
        for(auto context : _M_thread_contexts.contexts) {
          for(size_t i = 0; context->stack_size(); i++) {
            Value elem = context->stack_elem(i);
            if(elem.type() == VALUE_TYPE_REF && !elem.raw().r.has_nil())
              mark_from_object(elem.raw().r.ptr());
          }
          if(!context->regs().rv.raw().r.has_nil())
            mark_from_object(context->regs().rv.raw().r.ptr());
          if(context->regs().tmp_ptr != nullptr)
            ptr_to_header(context->regs().tmp_ptr)->stack_prev = &_S_nil;
        }
        for(auto context : _M_vm_contexts) {
          for(auto pair : context->vars()) {
            if(pair.second.type() == VALUE_TYPE_REF && !pair.second.raw().r.has_nil())
              mark_from_object(pair.second.raw().r.ptr());
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
          } else
            (*header_ptr)->stack_prev = nullptr;
          header_ptr = &((*header_ptr)->list_next);
        }
      }

      void MarkSweepGarbageCollector::mark_from_object(Object *object)
      {
        Header *header = object_to_header(object);
        mark_and_push_header(header);
        while(!is_empty_stack()) {
          Object *top_object = header_to_object(pop_header());
          switch(top_object->type()) {
            case OBJECT_TYPE_RARRAY:
              for(size_t i = 0; top_object->length(); i++) {
                Reference elem_ref = top_object->raw().rs[i];
                if(!elem_ref.has_nil()) {
                  Header *elem_header = object_to_header(elem_ref.ptr());
                  if(!elem_header->is_marked()) mark_and_push_header(elem_header);
                }
              }
              break;
            case OBJECT_TYPE_TUPLE:
              for(size_t i = 0; top_object->length(); i++) {
                if(top_object->raw().tes[i].type() == VALUE_TYPE_REF) {
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
    }
  }
}
