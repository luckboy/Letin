/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#if defined(__unix__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#error "Unsupported operating system."
#endif
#include <algorithm>
#include <cerrno>
#include <limits>
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

      Socklen SocketAddress::length() const
      {
        switch(addr.sa_family) {
#if defined(__unix__)
          case AF_UNIX:
            return sizeof(struct ::sockaddr_un);
#endif
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

      Socklen OptionValue::length() const
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

      int64_t system_domain_to_domain(int system_domain)
      {
        switch(system_domain) {
          case AF_UNIX:
            return 1;
          case AF_INET:
            return 2;
#ifdef AF_INET6
          case AF_INET6:
            return 3;
#endif
          default:
            return -1;
        }
      }

      bool domain_to_system_domain(int64_t domain, int &system_domain)
      {
        switch(domain) {
#if defined(__unix__)
          case 1:
            system_domain = AF_UNIX;
            return true;
#endif
          case 2:
            system_domain = AF_INET;
            return true;
#ifdef AF_INET6
          case 3:
            system_domain = AF_INET6;
            return true;
#endif
          default:
#if defined(__unix__)
            letin_errno() = EAFNOSUPPORT;
#elif defined(_WIN32) || defined(_WIN64)
            letin_errno() = (1 << 29) + WSAEAFNOSUPPORT;
#else
#error "Unsupported operating system."
#endif
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
#if defined(__unix__)
          letin_errno() = EPROTONOSUPPORT;
#elif defined(_WIN32) || defined(_WIN64)
          letin_errno() = (1 << 29) + WSAEPROTONOSUPPORT;
#else
#error "Unsupported operating system."
#endif
          return false;
        }
        system_protocol = protocol;
        return true;
      }

      bool how_to_system_how(int64_t how, int &system_how)
      {
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
        switch(how) {
          case 0:
            system_how = SD_RECEIVE;
            return true;
          case 1:
            system_how = SD_SEND;
            return true;
          case 2:
            system_how = SD_BOTH;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
#else
#error "Unsupported operating system."
#endif
      }

      bool flags_to_system_flags(int64_t flags, int &system_flags)
      {
        if((flags & ~static_cast<int64_t>((1 << 7) - 1)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#ifdef MSG_CTRUNC
        if((flags & (1 << 0)) != 0) system_flags |= MSG_CTRUNC;
#else
        if((flags & (1 << 0)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
        if((flags & (1 << 1)) != 0) system_flags |= MSG_DONTROUTE;
#ifdef MSG_EOR
        if((flags & (1 << 2)) != 0) system_flags |= MSG_EOR;
#else
        if((flags & (1 << 2)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
        if((flags & (1 << 3)) != 0) system_flags |= MSG_OOB;
        if((flags & (1 << 4)) != 0) system_flags |= MSG_PEEK;
#ifdef MSG_WAITALL
        if((flags & (1 << 5)) != 0) system_flags |= MSG_WAITALL;
#else
        if((flags & (1 << 5)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
#ifdef MSG_TRUNC
        if((flags & (1 << 6)) != 0) system_flags |= MSG_TRUNC;
#else
        if((flags & (1 << 6)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
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

      bool length_to_system_length(int64_t length, SocketSize &system_length)
      {
        if((length < 0) || (length < numeric_limits<SocketSsize>::max())) {
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
        if(buffer_r->length() > static_cast<size_t>(numeric_limits<SocketSsize>::max())) {
          letin_errno() = EINVAL;
          return false;
        }
        system_buffer_r = buffer_r;
        return true;
      }

      bool level_to_system_level(int64_t level, int &system_level)
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

#if defined(__unix__)
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
        if((events & ~static_cast<int64_t>((1 << 10) - 1)) != 0) {
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
#endif

      int64_t system_addr_info_flags_to_addr_info_flags(int system_flags)
      {
        int64_t flags = 0;
        if((system_flags & AI_PASSIVE) != 0) flags |= 1 << 0;
        if((system_flags & AI_CANONNAME) != 0) flags |= 1 << 1;
        if((system_flags & AI_NUMERICHOST) != 0) flags |= 1 << 2;
#ifdef AI_NUMERICSERV
        if((system_flags & AI_NUMERICSERV) != 0) flags |= 1 << 3;
#endif
#ifdef AI_V4MAPPED
        if((system_flags & AI_V4MAPPED) != 0) flags |= 1 << 4;
#endif
#ifdef AI_ALL
        if((system_flags & AI_ALL) != 0) flags |= 1 << 5;
#endif
#ifdef AI_ADDRCONFIG
        if((system_flags & AI_ADDRCONFIG) != 0) flags |= 1 << 6;
#endif
        return flags;
      }

      bool addr_info_flags_to_system_addr_info_flags(int64_t flags, int &system_flags)
      {
        if((flags & ~static_cast<int64_t>((1 << 7) - 1)) != 0) return false;
        system_flags = 0;
        if((flags & (1 << 0)) != 0) system_flags |= AI_PASSIVE;
        if((flags & (1 << 1)) != 0) system_flags |= AI_CANONNAME;
        if((flags & (1 << 2)) != 0) system_flags |= AI_NUMERICHOST;
#ifdef AI_NUMERICSERV
        if((flags & (1 << 3)) != 0) system_flags |= AI_NUMERICSERV;
#else
        if((flags & (1 << 3)) != 0) return false;
#endif
#ifdef AI_V4MAPPED
        if((flags & (1 << 4)) != 0) system_flags |= AI_V4MAPPED;
#else
        if((flags & (1 << 4)) != 0) return false;
#endif
#ifdef AI_ALL
        if((flags & (1 << 5)) != 0) system_flags |= AI_ALL;
#else
        if((flags & (1 << 5)) != 0) return false;
#endif
#ifdef AI_ADDRCONFIG
        if((flags & (1 << 6)) != 0) system_flags |= AI_ADDRCONFIG;
#else
        if((flags & (1 << 6)) != 0) return false;
#endif
        return false;
      }

      int64_t system_addr_info_error_to_addr_info_error(int system_error)
      {
        switch(system_error) {
          case 0:
            return 0;
          case EAI_AGAIN:
            return 1;
          case EAI_BADFLAGS:
            return 2;
          case EAI_FAIL:
            return 3;
          case EAI_FAMILY:
            return 4;
          case EAI_MEMORY:
            return 5;
          case EAI_NONAME:
            return 6;
          case EAI_SERVICE:
            return 7;
#ifdef EAI_SYSTEM
          case EAI_SYSTEM:
            return 8;
#endif
#ifdef EAI_OVERFLOW
          case EAI_OVERFLOW:
            return 9;
#endif
          default:
            return -1;
        }
      }

      bool name_info_flags_to_system_name_info_flags(int64_t flags, int &system_flags)
      {
        if((flags & ~static_cast<int64_t>((1 << 5) - 1)) != 0) return false;
        system_flags = 0;
        if((flags & (1 << 0)) != 0) system_flags |= NI_NOFQDN;
        if((flags & (1 << 1)) != 0) system_flags |= NI_NUMERICHOST;
        if((flags & (1 << 2)) != 0) system_flags |= NI_NAMEREQD;
        if((flags & (1 << 3)) != 0) system_flags |= NI_NUMERICSERV;
        if((flags & (1 << 4)) != 0) system_flags |= NI_DGRAM;
        return true;
      }

      bool size_to_system_size(int64_t size, size_t &system_size)
      {
        if(sizeof(int64_t) > sizeof(size_t) &&
            ((size < 0) || (size > static_cast<int64_t>(numeric_limits<size_t>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_size = size;
        return true;
      }

      bool in_port_to_system_in_port(int64_t port, InPort &system_port)
      {
        if(sizeof(int64_t) > sizeof(InPort) &&
            ((port < 0) || (port > static_cast<int64_t>(numeric_limits<InPort>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_port = port;
        return true;
      }

      bool in_addr_to_system_in_addr(int64_t addr, InAddr &system_addr)
      {
        if(sizeof(int64_t) > sizeof(InAddr) &&
            ((addr < 0) || (addr > static_cast<int64_t>(numeric_limits<InAddr>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_addr = addr;
        return true;
      }

#if defined(__unix__)
      bool uint32_to_system_uint32(int64_t i, uint32_t &system_i)
      {
        if((i < 0) || (i > static_cast<int64_t>(numeric_limits<uint32_t>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_i = i;
        return true;
      }
#elif defined(_WIN32) || defined(_WIN64)
      bool ulong_to_system_ulong(int64_t i, ::ULONG &system_i)
      {
        if(sizeof(int64_t) > sizeof(InAddr) &&
            ((i < 0) || (i > static_cast<int64_t>(numeric_limits<::ULONG>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_i = i;
        return true;
      }
#else
#error "Unsupported operating system."
#endif

      // Functions for SocketAddress.

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
#if defined(__unix__)
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
#endif
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

      bool object_to_system_socket_address(const Object &object, SocketAddress &addr)
      {
        switch(object.elem(0).i()) {
#if defined(__unix__)
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
#endif
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
#if defined(__unix__)
            if(!uint32_to_system_uint32(object.elem(2).i(), inet6_addr.sin6_flowinfo)) return false;
#elif defined(_WIN32) || defined(_WIN64)
            if(!ulong_to_system_ulong(object.elem(2).i(), inet6_addr.sin6_flowinfo)) return false;
#else
#error "Unsupported operating system."
#endif
            inet6_addr.sin6_flowinfo = htons(inet6_addr.sin6_flowinfo);
            inet6_addr.sin6_addr.s6_addr16[0] = (object.elem(3).i() >> 48) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[1] = (object.elem(3).i() >> 32) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[2] = (object.elem(3).i() >> 16) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[3] = (object.elem(3).i() >> 0) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[4] = (object.elem(4).i() >> 48) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[5] = (object.elem(4).i() >> 32) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[6] = (object.elem(4).i() >> 16) & 0xffff;
            inet6_addr.sin6_addr.s6_addr16[7] = (object.elem(4).i() >> 0) & 0xffff;
#if defined(__unix__)
            if(!uint32_to_system_uint32(object.elem(5).i(), inet6_addr.sin6_scope_id)) return false;
#elif defined(_WIN32) || defined(_WIN64)
            if(!ulong_to_system_ulong(object.elem(5).i(), inet6_addr.sin6_scope_id)) return false;
#else
#error "Unsupported operating system."
#endif
            inet6_addr.sin6_scope_id = htons(inet6_addr.sin6_scope_id);
            return true;
          }
#endif
          default:
#if defined(__unix__)
            letin_errno() = EAFNOSUPPORT;
#elif defined(_WIN32) || defined(_WIN64)
            letin_errno() = (1 << 29) + WSAEAFNOSUPPORT;
#else
#error "Unsupported operating system."
#endif
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
        if((object.elem(1).i() > static_cast<int64_t>(numeric_limits<long>::min())) || 
            (object.elem(1).i() < static_cast<int64_t>(numeric_limits<long>::max()))) {
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

#if defined(__unix__)
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
#endif

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

      // Functions for AddressInfo.
      
      //
      // type addrinfo = (
      //     int,       // ai_flags
      //     int,       // ai_family
      //     int,       // ai_socktype
      //     int,       // ai_protocol
      //     option sockaddr, // ai_addr
      //     option iarray8 // ai_cononname
      //   )
      //

      bool object_to_system_address_info(Object &object, AddressInfo &addr_info)
      {
        if(!addr_info_flags_to_system_addr_info_flags(object.elem(0).i(), addr_info.info.ai_flags)) return false;
        if(!domain_to_system_domain(object.elem(1).i(), addr_info.info.ai_family)) return false;
        if(!type_to_system_type(object.elem(2).i(), addr_info.info.ai_socktype)) return false;
        if(!protocol_to_system_protocol(object.elem(3).i(), addr_info.info.ai_protocol)) return false;
        if(object.elem(4).r()->elem(0).i() != 0) {
          addr_info.addr = unique_ptr<SocketAddress>(new SocketAddress);
          if(!object_to_system_socket_address(*(object.elem(4).r()->elem(1).r()), *(addr_info.addr))) return false;
        } else
          addr_info.addr = unique_ptr<SocketAddress>();
        if(object.elem(4).r()->elem(0).i() != 0) {
          addr_info.canonical_name.assign(reinterpret_cast<char *>(object.elem(4).r()->elem(1).r()->raw().is8), object.elem(4).r()->elem(1).r()->length());
          addr_info.info.ai_canonname = const_cast<char *>(addr_info.canonical_name.c_str());
        } else
          addr_info.canonical_name = string();
        addr_info.info.ai_next = nullptr;
        return true;
      }

      bool check_address_info(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_tuple() || object.length() != 2) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < 4; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        int error = vm->force_tuple_elem(context, object, 4);
        if(error != ERROR_SUCCESS) return error;
        if(!object.elem(4).is_ref()) return ERROR_INCORRECT_OBJECT;
        if(!object.elem(4).r()->is_tuple()) return ERROR_INCORRECT_OBJECT;
        if(object.elem(4).r()->length() < 1) return ERROR_INCORRECT_OBJECT;
        if(object.elem(4).r()->elem(0).i() == 0) {
          if(object.elem(4).r()->length() != 1) return ERROR_INCORRECT_OBJECT;
        } else {
          if(object.elem(4).r()->length() != 2) return ERROR_INCORRECT_OBJECT;
          error = check_socket_address(vm, context, *(object.elem(4).r()));
          if(error != ERROR_SUCCESS) return error;
        }
        if(!object.elem(5).is_ref()) return ERROR_INCORRECT_OBJECT;
        if(!object.elem(5).r()->is_tuple()) return ERROR_INCORRECT_OBJECT;
        if(object.elem(5).r()->length() < 1) return ERROR_INCORRECT_OBJECT;
        if(object.elem(5).r()->elem(0).i() == 0) {
          if(object.elem(5).r()->length() != 1) return ERROR_INCORRECT_OBJECT;
        } else {
          if(object.elem(5).r()->length() != 2) return ERROR_INCORRECT_OBJECT;
          if(!object.elem(5).r()->is_iarray8()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;
      }

      // Functions for struct addrinfo.

      bool set_new_addrinfo(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const AddinfoPointer &addr_info)
      {
        struct ::addrinfo *tmp_addr_info = addr_info;
        Reference prev_r;
        while(true) {
          size_t tuple_length = (tmp_addr_info != nullptr ? 3 : 1);
          RegisteredReference r(vm->gc()->new_object(OBJECT_TYPE_TUPLE, tuple_length, context), context, false);
          if(r.is_null()) return false;
          r->set_elem(0, Value(tmp_addr_info != nullptr ? 1 : 0));
          for(size_t i = 1; i < tuple_length; i++) r->set_elem(i, Value());
          r.register_ref();
          if(!prev_r.is_null())
            prev_r->set_elem(2, Value(r));
          else
            tmp_r = r;
          if(tmp_addr_info == nullptr) break;
          RegisteredReference addr_info_r(vm->gc()->new_object(OBJECT_TYPE_TUPLE, 6, context), context, false);
          if(addr_info_r.is_null()) return false;
          for(size_t i = 0; i < 6; i++) addr_info_r->set_elem(i, Value());
          addr_info_r.register_ref();
          addr_info_r->set_elem(0, Value(system_addr_info_flags_to_addr_info_flags(tmp_addr_info->ai_flags)));
          addr_info_r->set_elem(1, Value(system_domain_to_domain(tmp_addr_info->ai_family)));
          addr_info_r->set_elem(2, Value(system_type_to_type(tmp_addr_info->ai_socktype)));
          addr_info_r->set_elem(3, Value(tmp_addr_info->ai_protocol));
          SocketAddress *addr = reinterpret_cast<SocketAddress *>(tmp_addr_info->ai_addr);
          RegisteredReference tmp_r2(context, false);
          if(!set_new_socket_address(vm, context, tmp_r2, *addr)) return false;
          addr_info_r->set_elem(4, Value(tmp_r2));
          size_t canonical_name_length = strlen(tmp_addr_info->ai_canonname);
          RegisteredReference tmp_r3(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, canonical_name_length, context), context);
          if(tmp_r3.is_null()) return false;
          copy_n(reinterpret_cast<int8_t *>(tmp_addr_info->ai_canonname), canonical_name_length, tmp_r3->raw().is8);
          addr_info_r->set_elem(5, Value(tmp_r3));
          r->set_elem(1, Value(addr_info_r));
          tmp_addr_info = tmp_addr_info->ai_next;
          prev_r = r;
        }
        return true;
      }

      // Functions for Windows.

#if defined(_WIN32) || defined(_WIN64)
      int system_error_for_winsock2()
      {
        int error = ::WSAGetLastError();
        switch(error) {
          case WSAEACCES:
            return EACCES;
          case WSAEBADF:
            return EBADF;
          case WSAEFAULT:
            return EFAULT;
          case WSAEINTR:
            return EINTR;
          case WSAEINVAL:
            return EINVAL;
          case WSAEMFILE:
            return EMFILE;
          case WSAENAMETOOLONG:
            return ENAMETOOLONG;
          case WSAEOPNOTSUPP:
            return ENOTSUP;
          case WSAEWOULDBLOCK:
            return EAGAIN;
          default:
            return error + (1 << 29);
        }
      }
#endif
    }
  }
}
