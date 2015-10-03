/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _SOCKET_HPP
#define _SOCKET_HPP

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <letin/native.hpp>

namespace letin
{
  namespace nlib
  {
    namespace socket
    {
      template<typename _T>
      class Array
      {
        std::unique_ptr<_T []> _M_ptr;
        std::size_t _M_length;
      public:
        Array() {}

        void set_ptr_and_length(_T *ptr, std::size_t length)
        {
          _M_ptr = std::unique_ptr<_T []>(ptr);
          _M_length = length;
        }

        _T *ptr() const { return _M_ptr.get(); }

        std::size_t length() const { return _M_length; }

        const _T &operator[](std::size_t i) const { return _M_ptr[i]; }

        _T &operator[](std::size_t i) { return _M_ptr[i]; }
      };

      union SocketAddress
      {
        struct ::sockaddr addr;
        struct ::sockaddr_un unix_addr;
        struct ::sockaddr_in inet_addr;
#ifdef HAVE_STRUCT_SOCKADDR_IN6
        struct ::sockaddr_in6 inet6_addr;
#endif

        ::socklen_t length() const;
      };

      struct OptionValue
      {
        enum Type
        {
          TYPE_INT,
          TYPE_LINGER,
          TYPE_TIMEVAL
        };

        Type type;
        union {
          int i;
          struct ::linger linger;
          struct ::timeval time_value;
        } u;

        ::socklen_t length() const;
      };

      struct AddressInfo
      {
        struct ::addrinfo info;
        std::unique_ptr<SocketAddress> addr;
        std::string canonical_name;
      };

      typedef struct ::addrinfo *AddinfoPointer;

      void initialize_consts();

      std::int64_t system_domain_to_domain(int system_domain);
      
      bool domain_to_system_domain(std::int64_t domain, int &system_domain);
      LETIN_NATIVE_INT_CONVERTER(todomain, domain_to_system_domain, int);

      std::int64_t system_type_to_type(int system_type);

      bool type_to_system_type(std::int64_t type, int &system_type);
      LETIN_NATIVE_INT_CONVERTER(totype, type_to_system_type, int);

      bool protocol_to_system_protocol(std::int64_t protocol, int &system_protocol);
      LETIN_NATIVE_INT_CONVERTER(toprotocol, protocol_to_system_protocol, int);

      bool how_to_system_how(std::int64_t how, int &system_how);
      LETIN_NATIVE_INT_CONVERTER(tohow, how_to_system_how, int);

      bool flags_to_system_flags(std::int64_t flags, int &system_flags);
      LETIN_NATIVE_INT_CONVERTER(toflags, flags_to_system_flags, int);

      bool sd_to_system_sd(std::int64_t sd, int &system_sd);
      LETIN_NATIVE_INT_CONVERTER(tosd, sd_to_system_sd, int);

      bool length_to_system_length(std::int64_t length, std::size_t &system_length);
      LETIN_NATIVE_INT_CONVERTER(tolen, length_to_system_length, size_t);

      bool arg_to_system_arg(std::int64_t arg, int &system_arg);
      LETIN_NATIVE_INT_CONVERTER(toarg, arg_to_system_arg, int);

      bool ref_to_system_buffer_ref(vm::Reference buffer_r, vm::Reference &system_buffer_r);
      LETIN_NATIVE_REF_CONVERTER(tobufref, ref_to_system_buffer_ref, vm::Reference);

      bool level_to_system_level(std::int64_t level, int &system_level);
      LETIN_NATIVE_INT_CONVERTER(tolevel, level_to_system_level, int);

      bool option_name_to_system_option_name(std::int64_t option_name, int &system_option_name);
      LETIN_NATIVE_INT_CONVERTER(tooptname, option_name_to_system_option_name, int);

      bool nfds_to_system_nfds(std::int64_t nfds, int &system_nfds);
      LETIN_NATIVE_INT_CONVERTER(tonfds, nfds_to_system_nfds, int);

      std::int64_t system_poll_events_to_poll_events(short system_events);

      bool poll_events_to_system_poll_events(std::int64_t events, short &system_events);

      std::int64_t system_addr_info_flags_to_addr_info_flags(int system_flags);

      bool addr_info_flags_to_system_addr_info_flags(std::int64_t flags, int &system_flags);

      std::int64_t system_addr_info_error_to_addr_info_error(int system_error);

      bool name_info_flags_to_system_name_info_flags(std::int64_t flags, int &system_flags);
      LETIN_NATIVE_INT_CONVERTER(toniflags, name_info_flags_to_system_name_info_flags, int);

      bool size_to_system_size(std::int64_t size, std::size_t &system_size);
      LETIN_NATIVE_INT_CONVERTER(tosize, size_to_system_size, std::size_t);

      bool in_port_to_system_in_port(std::int64_t port, ::in_port_t &system_port);

      bool in_addr_to_system_in_addr(std::int64_t addr, ::in_addr_t &system_addr);

      bool uint32_to_system_uint32(std::int64_t i, std::uint32_t &system_i);

      // Functions for SocketAddress.

      bool set_new_socket_address(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const SocketAddress &addr_union);
      LETIN_NATIVE_REF_SETTER(vsockaddr, set_new_socket_address, SocketAddress);

      bool object_to_system_socket_address(const vm::Object &object, SocketAddress &addr_union);
      LETIN_NATIVE_OBJECT_CONVERTER(tosockaddr, object_to_system_socket_address, SocketAddress);

      int check_socket_address(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(csockaddr, check_socket_address);

      // Functions for struct linger.

      vm::Object *new_linger(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::linger &linger);
      LETIN_NATIVE_OBJECT_SETTER(vlinger, new_linger, struct ::linger);      

      // Functions for struct timeval.

      vm::Object *new_timeval(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::timeval &time_value);
      LETIN_NATIVE_OBJECT_SETTER(vtimeval, new_timeval, struct ::timeval);

      bool object_to_system_timeval(const vm::Object &object, struct ::timeval &time_value);
      LETIN_NATIVE_OBJECT_CONVERTER(totimeval, object_to_system_timeval, struct ::timeval);

      int check_timeval(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(ctimeval, check_timeval);

      // Function for OptionValue.

      bool object_to_system_option_value(vm::Object &object, OptionValue &option_value);
      LETIN_NATIVE_OBJECT_CONVERTER(tooptval, object_to_system_option_value, OptionValue);

      int check_option_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(coptval, check_option_value);

      // Functions for fd_set.

      vm::Object *new_fd_set(vm::VirtualMachine *vm, vm::ThreadContext *context, const ::fd_set &fds);
      LETIN_NATIVE_OBJECT_SETTER(vfd_set, new_fd_set, ::fd_set);

      void system_fd_set_to_object(const ::fd_set &fds, vm::Object &object);

      bool object_to_system_fd_set(const vm::Object &object, ::fd_set &fds);
      LETIN_NATIVE_OBJECT_CONVERTER(tofd_set, object_to_system_fd_set, ::fd_set);

      // Functions for struct pollfd [].

      bool set_new_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const Array<struct ::pollfd> &fds);
      LETIN_NATIVE_REF_SETTER(vpollfds, set_new_pollfds, Array<struct ::pollfd>);

      void system_pollfds_to_object(const Array<struct ::pollfd> &fds, vm::Object &object);

      bool object_to_system_pollfds(vm::Object &object, Array<struct ::pollfd> &fds);
      LETIN_NATIVE_OBJECT_CONVERTER(topollfds, object_to_system_pollfds, Array<struct ::pollfd>);

      int check_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(cpollfds, check_pollfds);

      int check_unique_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_UNIQUE_OBJECT_CHECKER(cupollfds, check_unique_pollfds);
      
      // Functions for AddressInfo.

      bool object_to_system_address_info(vm::Object &object, AddressInfo &addr_info);
      LETIN_NATIVE_OBJECT_CONVERTER(toaddrinfo, object_to_system_address_info, AddressInfo);

      bool check_address_info(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_UNIQUE_OBJECT_CHECKER(caddrinfo, check_address_info);

      // Functions for struct addrinfo.

      bool set_new_addrinfo(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const AddinfoPointer &addr_info);
      LETIN_NATIVE_REF_SETTER(vaddrinfo, set_new_addrinfo, AddinfoPointer);
    }
  }
}

#endif
