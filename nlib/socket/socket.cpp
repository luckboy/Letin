/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <algorithm>
#include <cerrno>
#include <limits>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include "socket.hpp"

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace nlib
  {
    namespace socket
    {
      vector<int> system_option_names;

      void initialize_consts()
      {
        system_option_names = {
          SO_DEBUG,
          SO_ACCEPTCONN,
          SO_BROADCAST,
          SO_REUSEADDR,
          SO_KEEPALIVE,
          SO_LINGER,
          SO_OOBINLINE,
          SO_SNDBUF,
          SO_RCVBUF,
          SO_ERROR,
          SO_TYPE,
          SO_DONTROUTE,
          SO_RCVLOWAT,
          SO_RCVTIMEO,
          SO_SNDLOWAT,
          SO_SNDTIMEO
        };
      }

      ::socklen_t SocketAddress::length() const
      {
        switch(addr.sa_family) {
          case AF_UNIX:
            return sizeof(struct ::sockaddr_un);
          case AF_INET:
            return sizeof(struct ::sockaddr_in);
#if defined(HAVE_STRUCT_SOCKADDR_IN6) && defined(AF_INET6)
          case AF_INET6:
            return sizeof(struct ::sockaddr_in6);
#endif
          default:
            return 0;
        }
      }
      
      ::socklen_t OptionValue::length() const
      {
        switch(type) {
          case TYPE_INT:
            return sizeof(int);
          case TYPE_LINGER:
            return sizeof(struct ::linger);
          case TYPE_TIMEVAL:
            return sizeof(struct ::timeval);
          default:
            return 0;
        }
      }

      bool domain_to_system_domain(int64_t domain, int &system_domain)
      {
        switch(domain) {
          case 1:
            system_domain = AF_UNIX;
            return true;
          case 2:
            system_domain = AF_INET;
            return true;
#ifdef AF_INET6
          case 3:
            system_domain = AF_INET6;
            return true;
#endif
          default:
            letin_errno() = EAFNOSUPPORT;
            return false;
        }
      }

      int64_t system_type_to_type(int system_type)
      {
        switch(system_type) {
          case SOCK_STREAM:
            return 0;
          case SOCK_DGRAM:
            return 1;
          case SOCK_SEQPACKET:
            return 2;
          default:
            return -1;
        }
      }

      bool type_to_system_type(int64_t type, int &system_type)
      {
        switch(type) {
          case 0:
            system_type = SOCK_STREAM;
            return true;
          case 1:
            system_type = SOCK_DGRAM;
            return true;
          case 2:
            system_type = SOCK_SEQPACKET;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      bool protocol_to_system_protocol(int64_t protocol, int &system_protocol)
      {
        if((protocol < numeric_limits<int>::min()) || (protocol < (numeric_limits<int>::max()))) {
          letin_errno() = EPROTONOSUPPORT;
          return false;
        }
        system_protocol = protocol;
        return true;
      }

      bool how_to_system_how(int64_t how, int &system_how)
      {
        switch(how) {
          case 0:
            system_how = SHUT_RD;
            return true;
          case 1:
            system_how = SHUT_WR;
            return true;
          case 2:
            system_how = SHUT_RDWR;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      bool flags_to_system_flags(int64_t flags, int &system_flags)
      {
        if((flags & ~static_cast<int64_t>((1 << 7) - 1))) {
          letin_errno() = EINVAL;
          return false;
        }
        if((flags & (1 << 0)) != 0) system_flags |= MSG_CTRUNC;
        if((flags & (1 << 1)) != 0) system_flags |= MSG_DONTROUTE;
        if((flags & (1 << 2)) != 0) system_flags |= MSG_EOR;
        if((flags & (1 << 3)) != 0) system_flags |= MSG_OOB;
        if((flags & (1 << 4)) != 0) system_flags |= MSG_PEEK;
        if((flags & (1 << 5)) != 0) system_flags |= MSG_WAITALL;
        if((flags & (1 << 6)) != 0) system_flags |= MSG_TRUNC;
        return true;
      }

      bool sd_to_system_sd(int64_t sd, int &system_sd)
      {
        if((sd < 0) || (sd > numeric_limits<int>::max())) {
          letin_errno() = EBADF;
          return false;
        }
        system_sd = sd;
        return true;
      }

      bool length_to_system_length(int64_t length, size_t &system_length)
      {
        if((length < 0) || (length < numeric_limits<::ssize_t>::max())) {
          letin_errno() = EINVAL;
          return false;
        }
        system_length = length;
        return true;
      }

      bool arg_to_system_arg(int64_t arg, int &system_arg)
      {
        if((arg < numeric_limits<int>::min()) || (arg > numeric_limits<int>::max())) {
          letin_errno() = EINVAL;
          return false;
        }
        system_arg = arg;
        return true;
      }

      bool ref_to_system_buffer_ref(Reference buffer_r, Reference &system_buffer_r)
      {
        if(buffer_r->length() > static_cast<size_t>(numeric_limits<::ssize_t>::max())) {
          letin_errno() = EINVAL;
          return false;
        }
        system_buffer_r = buffer_r;
        return true;
      }

      bool level_to_system_level(int64_t level, int system_level)
      {
        switch(level) {
          case 0:
            system_level = IPPROTO_IP;
            return true;
          case 1:
            system_level = IPPROTO_ICMP;
            return true;
          case 2:
            system_level = IPPROTO_TCP;
            return true;
          case 3:
            system_level = IPPROTO_UDP;
            return true;
#ifdef IPPROTO_IPV6
          case 4:
            system_level = IPPROTO_IPV6;
            return true;
#endif
          case 0xffff:
            system_level = SOL_SOCKET;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      bool option_name_to_system_option_name(int64_t option_name, int &system_option_name)
      {
        if(option_name > 0 || option_name < static_cast<int>(system_option_names.size())) {
          system_option_name = system_option_names[option_name];
          return true;
        } else {
          letin_errno() = EINVAL;
          return false;
        }
      }

      bool nfds_to_system_nfds(int64_t nfds, int &system_nfds)
      {
        if((nfds < 0) || (nfds > FD_SETSIZE)) {
          letin_errno() = EBADF;
          return false;
        }
        system_nfds = nfds;
        return true;
      }

      int64_t system_poll_events_to_poll_events(short system_events)
      {
        int events = 0;
        if((system_events & POLLIN) != 0) events |= 1 << 0;
        if((system_events & POLLRDNORM) != 0) events |= 1 << 1;
        if((system_events & POLLRDBAND) != 0) events |= 1 << 2;
        if((system_events & POLLPRI) != 0) events |= 1 << 3;
        if((system_events & POLLOUT) != 0) events |= 1 << 4;
        if((system_events & POLLWRNORM) != 0) events |= 1 << 5;
        if((system_events & POLLWRBAND) != 0) events |= 1 << 6;
        if((system_events & POLLERR) != 0) events |= 1 << 7;
        if((system_events & POLLHUP) != 0) events |= 1 << 8;
        if((system_events & POLLNVAL) != 0) events |= 1 << 9;
        return events;
      }

      bool poll_events_to_system_poll_events(int64_t events, short &system_events)
      {
        if(events & ~static_cast<int64_t>((1 << 10) - 1)) {
          letin_errno() = EINVAL;
          return false;
        }
        system_events = 0;
        if((events & (1 << 0)) != 0) return POLLIN;
        if((events & (1 << 1)) != 0) return POLLRDNORM;
        if((events & (1 << 2)) != 0) return POLLRDBAND;
        if((events & (1 << 3)) != 0) return POLLPRI;
        if((events & (1 << 4)) != 0) return POLLOUT;
        if((events & (1 << 5)) != 0) return POLLWRNORM;
        if((events & (1 << 6)) != 0) return POLLWRBAND;
        if((events & (1 << 7)) != 0) return POLLERR;
        if((events & (1 << 8)) != 0) return POLLHUP;
        if((events & (1 << 9)) != 0) return POLLNVAL;
        return true;
      }

      bool size_to_system_size(int64_t size, size_t &system_size)
      {
        if((size < 0) || (size > static_cast<int64_t>(numeric_limits<size_t>::max()))) {
          letin_errno() = EINVAL;
          return true;
        }
        system_size = size;
        return true;
      }

      bool in_port_to_system_in_port(int64_t port, ::in_port_t &system_port)
      {
        if((port < 0) || (port > static_cast<int64_t>(numeric_limits<::in_port_t>::max()))) {
          letin_errno() = EINVAL;
          return true;
        }
        system_port = port;
        return true;
      }

      bool in_addr_to_system_in_addr(int64_t addr, ::in_addr_t &system_addr)
      {
        if((addr < 0) || (addr > static_cast<int64_t>(numeric_limits<::in_addr_t>::max()))) {
          letin_errno() = EINVAL;
          return true;
        }
        system_addr = addr;
        return true;
      }

      bool uint32_to_system_uint32(int64_t i, uint32_t &system_i)
      {
        if((i < 0) || (i > static_cast<int64_t>(numeric_limits<uint32_t>::max()))) {
          letin_errno() = EINVAL;
          return true;
        }
        system_i = i;
        return true;
      }

      // Functions for SocketAddressUnion.

      //
      // type sockaddr_un = (
      //     int,       // sun_family
      //     iarray8    // sun_path
      //   )
      //
      
      //
      // type sockaddr_in = (
      //     int,       // sin_family
      //     int,       // sin_port
      //     int        // sin_addr
      //   )
      //

      //
      // type sockaddr_in6 = (
      //     int,       // sin6_family
      //     int,       // sin6_port
      //     int,       // sin6_flowinfo
      //     int,       // sin6_addr_part1
      //     int,       // sin6_addr_part2
      //     int        // sin6_scope_id
      //  )
      //

      bool set_new_socket_address(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const SocketAddress &addr)
      {
        switch(addr.addr.sa_family) {
          case AF_UNIX:
          {
            const ::sockaddr_un &unix_addr = addr.unix_addr;
            tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
            if(tmp_r.is_null()) return false;
            tmp_r->set_elem(0, Value(1));
            RegisteredReference path_r(vm->gc()->new_string(unix_addr.sun_path, context), context);
            if(path_r.is_null()) return false;
            tmp_r->set_elem(1, Value(path_r));
            tmp_r.register_ref();
            return true;
          }
          case AF_INET:
          {
            const ::sockaddr_in &inet_addr = addr.inet_addr;
            tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 3, context);
            if(tmp_r.is_null()) return false;
            tmp_r->set_elem(0, Value(2));
            tmp_r->set_elem(1, Value(static_cast<int64_t>(ntohs(inet_addr.sin_port))));
            tmp_r->set_elem(2, Value(static_cast<int64_t>(ntohl(inet_addr.sin_addr.s_addr))));
            tmp_r.register_ref();
            return true;
          }
#if defined(HAVE_STRUCT_SOCKADDR_IN6) && defined(AF_INET6)
          case AF_INET6:
          {
            const ::sockaddr_in6 &inet6_addr = addr.inet6_addr;
            tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 6, context);
            if(tmp_r.is_null()) return false;
            tmp_r->set_elem(0, Value(3));
            tmp_r->set_elem(1, Value(static_cast<int64_t>(ntohs(inet6_addr.sin6_port))));
            tmp_r->set_elem(2, Value(static_cast<int64_t>(ntohl(inet6_addr.sin6_flowinfo))));
            int64_t addr_part1;
            addr_part1 = static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[0])) << 48;
            addr_part1 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[1])) << 32;
            addr_part1 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[2])) << 16;
            addr_part1 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[3])) << 0;
            tmp_r->set_elem(3, Value(addr_part1));
            int64_t addr_part2;
            addr_part2 = static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[4])) << 48;
            addr_part2 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[5])) << 32;
            addr_part2 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[6])) << 16;
            addr_part2 |= static_cast<int64_t>(ntohs(inet6_addr.sin6_addr.s6_addr16[7])) << 0;
            tmp_r->set_elem(4, Value(addr_part2));
            tmp_r->set_elem(5, Value(static_cast<int64_t>(ntohl(inet6_addr.sin6_scope_id))));
            tmp_r.register_ref();
            return true;
          }
#endif
          default:
          {
            tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 1, context);
            if(tmp_r.is_null()) return false;
            tmp_r->set_elem(0, Value(0));
            tmp_r.register_ref();
            return true;
          }
        }
      }

      bool object_to_system_socket_address(const vm::Object &object, SocketAddress &addr)
      {
        switch(object.elem(0).i()) {
          case 1:
          {
            if(object.length() + 1 >= sizeof(addr.unix_addr.sun_path)) {
              letin_errno() = ENAMETOOLONG;
              return false;
            }
            ::sockaddr_un &unix_addr = addr.unix_addr;
            unix_addr.sun_family = AF_UNIX;
            copy_n(reinterpret_cast<const char *>(object.raw().is8), object.length(), unix_addr.sun_path);
            unix_addr.sun_path[object.length() + 1] = 0;
            return true;
          }
          case 2:
          {
            ::sockaddr_in &inet_addr = addr.inet_addr;
            inet_addr.sin_family = AF_INET;
            if(!in_port_to_system_in_port(object.elem(1).i(), inet_addr.sin_port)) return false;
            inet_addr.sin_port = htons(inet_addr.sin_port);
            if(!in_addr_to_system_in_addr(object.elem(2).i(), inet_addr.sin_addr.s_addr)) return false;
            inet_addr.sin_addr.s_addr = htonl(inet_addr.sin_addr.s_addr);
            return true;
          }
#if defined(HAVE_STRUCT_SOCKADDR_IN6) && defined(AF_INET6)
          case 3:
          {
            ::sockaddr_in6 &inet6_addr = addr.inet6_addr;
            inet6_addr.sin6_family = AF_INET6;
            if(!in_port_to_system_in_port(object.elem(1).i(), inet6_addr.sin6_port)) return false;
            inet6_addr.sin6_port = htons(inet6_addr.sin6_port);
            if(!uint32_to_system_uint32(object.elem(2).i(), inet6_addr.sin6_flowinfo)) return false;
            inet6_addr.sin6_flowinfo = htons(inet6_addr.sin6_flowinfo);
            inet6_addr.sin6_addr.s6_addr16[0] = (object.elem(3).i() >> 48) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[1] = (object.elem(3).i() >> 32) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[2] = (object.elem(3).i() >> 16) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[3] = (object.elem(3).i() >> 0) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[4] = (object.elem(4).i() >> 48) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[5] = (object.elem(4).i() >> 32) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[6] = (object.elem(4).i() >> 16) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[7] = (object.elem(4).i() >> 0) & 0xffff;
            if(!uint32_to_system_uint32(object.elem(5).i(), inet6_addr.sin6_scope_id)) return false;
            inet6_addr.sin6_scope_id = htons(inet6_addr.sin6_scope_id);
            return true;
          }
#endif
          default:
            letin_errno() = EAFNOSUPPORT;
            return false;
        }
      }

      int check_socket_address(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        int error;
        if(!object.is_tuple() || object.length() <= 0) return ERROR_INCORRECT_OBJECT;
        error = vm->force_tuple_elem(context, object, 0);
        if(error != ERROR_SUCCESS) return error;
        if(!object.elem(0).is_int()) return ERROR_INCORRECT_OBJECT;
        switch(object.elem(0).i()) {
          case 0:
            if(object.length() != 1) return ERROR_INCORRECT_OBJECT;
            return ERROR_SUCCESS;
          case 1:
            if(object.length() != 2) return ERROR_INCORRECT_OBJECT;
            error = vm->force_tuple_elem(context, object, 1);
            if(error != ERROR_SUCCESS) return error;
            if(!object.elem(1).is_ref() || !object.elem(1).r()->is_iarray8()) return ERROR_INCORRECT_OBJECT;
            return ERROR_SUCCESS;
          case 2:
          case 3:
          {
            size_t n = (object.elem(0).i() == 2 ? 3 : 6);
            for(size_t i = 1; i < n; i++) {
              error = vm->force_tuple_elem(context, object, i);
              if(error != ERROR_SUCCESS) return error;
              if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
            }
            return ERROR_SUCCESS;
          }
          default:
            return ERROR_INCORRECT_OBJECT;
        }
      }

      // Functions for struct linger.

      //
      // type linger = (
      //      int,      // l_onoff
      //      int       // l_linger
      //   )
      //

      Object *new_linger(VirtualMachine *vm, ThreadContext *context, const struct ::linger &linger)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(linger.l_onoff));
        object->set_elem(1, Value(linger.l_linger));
        return object;
      }

      // Functions for struct timeval.

      //
      // type timeval = (
      //     int,       // tv_sec
      //     int        // tv_usec
      //   )
      //

      Object *new_timeval(VirtualMachine *vm, ThreadContext *context, const struct ::timeval &time_value)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(time_value.tv_sec)));
        object->set_elem(1, Value(static_cast<int64_t>(time_value.tv_usec)));
        return object;
      }

      bool object_to_system_timeval(const Object &object, struct ::timeval &time_value)
      {
        if((object.elem(0).i() > static_cast<int64_t>(numeric_limits<time_t>::min())) ||
          (object.elem(0).i() < static_cast<int64_t>(numeric_limits<time_t>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        if((object.elem(1).i() > static_cast<int64_t>(numeric_limits<suseconds_t>::min())) || 
            (object.elem(1).i() < static_cast<int64_t>(numeric_limits<suseconds_t>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        time_value.tv_sec = object.elem(0).i();
        time_value.tv_usec = object.elem(1).i();
        return true;
      }

      int check_timeval(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_tuple() || object.length() != 2) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < 2; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;
      }

      // Functions for OptionValue.

      bool object_to_system_option_value(Object &object, OptionValue &option_value)
      {
        switch(object.elem(0).i()) {
          case 0:
            option_value.u.i = object.elem(1).i();
            return true;
          case 1:
            option_value.u.linger.l_onoff = object.elem(1).r()->elem(0).i();
            option_value.u.linger.l_linger = object.elem(1).r()->elem(1).i();
            return true;
          case 2:
            option_value.u.time_value.tv_sec = object.elem(1).r()->elem(0).i();
            option_value.u.time_value.tv_usec = object.elem(1).r()->elem(1).i();
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      int check_option_value(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        int error;
        if(!object.is_tuple() || object.length() != 2) return ERROR_INCORRECT_OBJECT;
        error = vm->force_tuple_elem(context, object, 0);
        if(error != ERROR_SUCCESS) return error;
        if(!object.elem(0).is_int()) return ERROR_INCORRECT_OBJECT;
        switch(object.elem(0).i()) {
          case 0:
            error = vm->force_tuple_elem(context, object, 1);
            if(error != ERROR_SUCCESS) return error;
            if(!object.elem(1).is_int()) return ERROR_INCORRECT_OBJECT;
            return ERROR_SUCCESS;
          case 1:
          case 2:
            error = vm->force_tuple_elem(context, object, 1);
            if(error != ERROR_SUCCESS) return error;
            if(!object.elem(1).is_ref() || !object.elem(1).r()->is_tuple()) return ERROR_INCORRECT_OBJECT;
            if(object.elem(1).r()->length() != 2) return ERROR_INCORRECT_OBJECT;
            for(size_t i = 0; i < 2; i++) {
              error = vm->force_tuple_elem(context, *(object.elem(1).r().ptr()), i);
              if(error != ERROR_SUCCESS) return error;
              if(!object.elem(1).r()->elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
            }
            return ERROR_SUCCESS;
          default:
            return ERROR_INCORRECT_OBJECT;
        }
      }

      // Functions for fd_set.

      Object *new_fd_set(VirtualMachine *vm, ThreadContext *context, const ::fd_set &fds)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_IARRAY8, (FD_SETSIZE + 7) >> 3, context);
        if(object == nullptr) return nullptr;
        system_fd_set_to_object(fds, *object);
        return object;
      }

      void system_fd_set_to_object(const ::fd_set &fds, Object &object)
      {
        for(int fd = 0; fd < FD_SETSIZE; fd++) {
          if(FD_ISSET(fd, &fds))
            object.set_elem(fd >> 3, object.elem(fd >> 3).i() | (1 << (fd & 7)));
          else
            object.set_elem(fd >> 3, object.elem(fd >> 3).i() & ~(1 << (fd & 7)));
        }
      }

      bool object_to_system_fd_set(const Object &object, ::fd_set &fds)
      {
        if(object.length() != static_cast<size_t>((FD_SETSIZE + 7) >> 3)) {
          letin_errno() = EINVAL;
          return false;
        }
        FD_ZERO(&fds);
        for(int fd = 0; fd < FD_SETSIZE; fd++) {
          if((object.elem(fd >> 3).i() & (1 << (fd & 7))) != 0) FD_SET(fd, &fds);
        }
        return true;
      }

      // Functions for struct pollfd [].

      //
      // type pollfd = (
      //     int,       // fd
      //     int,       // events
      //     int        // revents
      //   )
      //

      bool set_new_pollfds(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const Array<struct ::pollfd> &fds)
      {
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_RARRAY, fds.length(), context);
        if(tmp_r.is_null()) return false;
        fill_n(tmp_r->raw().rs, fds.length(), Reference());
        tmp_r.register_ref();
        for(size_t i = 0; i < fds.length(); i++) {
          Reference fd_r(vm->gc()->new_object(OBJECT_TYPE_TUPLE, 3, context));
          if(fd_r.is_null()) return false;
          fd_r->set_elem(0, fds[i].fd);
          fd_r->set_elem(1, system_poll_events_to_poll_events(fds[i].events));
          fd_r->set_elem(2, system_poll_events_to_poll_events(fds[i].revents));
          tmp_r->set_elem(i, fd_r);
        }
        return true;
      }

      void system_pollfds_to_object(const Array<struct ::pollfd> &fds, Object &object)
      {
        for(size_t i = 0; i < object.length(); i++) {
          object.elem(i).r()->set_elem(0, fds[i].fd);
          object.elem(i).r()->set_elem(1, system_poll_events_to_poll_events(fds[i].events));
          object.elem(i).r()->set_elem(2, system_poll_events_to_poll_events(fds[i].revents));
        }
      }

      bool object_to_system_pollfds(Object &object, Array<struct ::pollfd> &fds)
      {
        fds.set_ptr_and_length(new ::pollfd[object.length()], object.length());
        for(size_t i = 0; i < object.length(); i++) {
          Reference fd_r = object.elem(i).r();
          if(!sd_to_system_sd(fd_r->elem(0).i(), fds[i].fd)) return false;
          if(!poll_events_to_system_poll_events(fd_r->elem(1).i(), fds[i].events)) return false;
          if(!poll_events_to_system_poll_events(fd_r->elem(2).i(), fds[i].revents)) return false;
        }
        return true;
      }

      static int check_pollfds(VirtualMachine *vm, ThreadContext *context, Object &object, bool is_unique)
      {
        if(!(is_unique ? object.is_unique_rarray() : object.is_rarray())) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < object.length(); i++) {
          Reference fd_r = object.elem(i).r();
          if(!(is_unique ? fd_r->is_unique_tuple() : fd_r->is_tuple())) return ERROR_INCORRECT_OBJECT;
          if(fd_r->length() != 3) return ERROR_INCORRECT_OBJECT;
          for(size_t j = 0; j < 3; j++) {
            int error = vm->force_tuple_elem(context, *fd_r, j);
            if(error != ERROR_SUCCESS) return error;
            if(!fd_r->elem(j).is_int()) return ERROR_INCORRECT_OBJECT;
          }
        }
        return ERROR_SUCCESS;
      }

      int check_pollfds(VirtualMachine *vm, ThreadContext *context, Object &object)
      { return check_pollfds(vm, context, object, false); }

      int check_unique_pollfds(VirtualMachine *vm, ThreadContext *context, Object &object)
      { return check_pollfds(vm, context, object, true); }
    }
  }
}
