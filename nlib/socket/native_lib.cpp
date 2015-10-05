/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#if defined(__unix__)
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#error "Unsupported operating system."
#endif
#include <letin/native.hpp>
#include "socket.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::native;
using namespace letin::nlib::socket;

static vector<NativeFunction> native_funs;

struct AddrinfoDelete
{
  void operator()(struct ::addrinfo *addr_info) const
  { ::freeaddrinfo(addr_info); }
};

extern "C" {
  bool letin_initialize()
  {
    try {
      native_funs = {

        //
        // Socket native functions.
        //

        {
          "socket.socket", // (domain: int, type: int, protocol: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int domain, type, protocol;
            if(!convert_args(args, todomain(domain), totype(type), toprotocol(protocol)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int sd = ::socket(domain, type, protocol);
            if(sd == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sd), v(io_v)));
          }
        },
        {
          "socket.socketpair", // (domain: int, type: int, protocol: int, io: uio) -> (option (int, int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            int domain, type, protocol;
            int sv[2];
            if(!convert_args(args, todomain(domain), totype(type), toprotocol(protocol)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::socketpair(domain, type, protocol, sv);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(sv[0]), vint(sv[1]))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "socket.shutdown", // (sd: int, how: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd, how;
            if(!convert_args(args, tosd(sd), tohow(how)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::shutdown(sd, how);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.connect", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, csockaddr, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::connect(sd, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.bind", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, csockaddr, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::bind(sd, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.listen", // (sd: int, backlog: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd, backlog;
            if(!convert_args(args, tosd(sd), toarg(backlog)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::listen(sd, backlog);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.accept", // (sd: int, io: uio) -> (option (int, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            Socklen addr_len;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int accepted_sd = ::accept(sd, &(addr.addr), &addr_len);
            if(accepted_sd == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(accepted_sd), vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.recv", // (sd: int, len: int, flags: int, io: uio) -> (option (int, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            SocketSize len;
            if(!convert_args(args, tosd(sd), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, len, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
            SocketSsize result = ::recv(sd, reinterpret_cast<Pointer>(buf_r->raw().is8), len, flags);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r))), v(io_v)));
          }
        },
        {
          "socket.send", // (sd: int, buf: iarray8, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            Reference buf_r;
            if(!convert_args(args, tosd(sd), tobufref(buf_r), toflags(flags)))
              return return_value(vm, context, vut(vt(vint(-1)), v(io_v)));
            SocketSsize result = ::send(sd, reinterpret_cast<Pointer>(buf_r->raw().is8), buf_r->length(), flags);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "socket.urecv", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset;
            SocketSize len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            SocketSsize result = ::recv(sd, reinterpret_cast<Pointer>(buf_r->raw().is8 + offset), len, flags);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.usend", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset;
            SocketSize len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            SocketSsize result = ::send(sd, reinterpret_cast<Pointer>(buf_r->raw().is8 + offset), len, flags);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.recvfrom", // (sd: int, len: int, flags: int, io: uio) -> (option (int, iarray8, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, csockaddr, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            SocketSize len;
            SocketAddress addr;
            Socklen addr_len;
            if(!convert_args(args, tosd(sd), tolen(len), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, len, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
            SocketSsize result = ::recvfrom(sd, reinterpret_cast<Pointer>(buf_r->raw().is8), len, flags, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r), vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.sendto", // (sd: int, buf: iarray8, flags: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cint, csockaddr, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[4];
            int sd, flags;
            Reference buf_r;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tobufref(buf_r), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            SocketSsize result = ::sendto(sd, reinterpret_cast<Pointer>(buf_r->raw().is8), buf_r->length(), flags, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "socket.urecvfrom", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, io: uio) -> ((int, uiarray8, option tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset;
            SocketSize len;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v), vnone), v(io_v)), EINVAL);
            SocketSsize result = ::recvfrom(sd, reinterpret_cast<Pointer>(buf_r->raw().is8 + offset), len, flags, &(addr.addr), &(addr_len));
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vut(vint(-1), v(buf_v), vnone), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v), vsome(vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.usendto", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, addr: tuple, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, csockaddr, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[6];
            int sd, flags;
            Reference buf_r;
            size_t offset;
            SocketSize len;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            SocketSsize result = ::sendto(sd, reinterpret_cast<Pointer>(buf_r->raw().is8 + offset), len, flags, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.getpeername", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getpeername(sd, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vsockaddr(addr)), v(io_v)));
          }
        },
        {
          "socket.getsockname", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getsockname(sd, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vsockaddr(addr)), v(io_v)));
          }
        },
        {
          "socket.getsockopt", // (sd: int, level: int, opt_name: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, level, opt_name;
            ::socklen_t opt_len;
            if(!convert_args(args, tosd(sd), tolevel(level), tooptname(opt_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            switch(opt_name) {
              case SO_LINGER:
              {
                struct ::linger opt_val;
                int result = ::getsockopt(sd, level, opt_name, reinterpret_cast<Pointer>(&opt_val), &opt_len);
                if(result == -1)
                  return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
                return return_value(vm, context, vut(vsome(vt(vint(1), vlinger(opt_val))), v(io_v)));
              }
              case SO_RCVTIMEO:
              case SO_SNDTIMEO:
              {
                struct ::timeval opt_val;
                int result = ::getsockopt(sd, level, opt_name, reinterpret_cast<Pointer>(&opt_val), &opt_len);
                if(result == -1)
                  return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
                return return_value(vm, context, vut(vsome(vt(vint(2), vtimeval(opt_val))), v(io_v)));
              }
              default:
              {
                int opt_val;
                int result = ::getsockopt(sd, level, opt_name, reinterpret_cast<Pointer>(&opt_val), &opt_len);
                if(result == -1)
                  return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
                if(level == SO_TYPE) opt_val = system_type_to_type(opt_val);
                return return_value(vm, context, vut(vsome(vt(vint(0), vint(opt_val))), v(io_v)));
              }                
            }
          }
        },
        {
          "socket.setsockopt", // (sd: int, level: int, opt_name: int, opt_val: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[4];
            int sd, level, opt_name;
            OptionValue opt_val;
            if(!convert_args(args, tosd(sd), tolevel(level), tooptname(opt_name), tooptval(opt_val)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setsockopt(sd, level, opt_name, reinterpret_cast<ConstPointer>(&(opt_val.u)), opt_val.length());
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(-1), v(io_v)));
          }
        },

        //
        // IO multiplexing native functions.
        //

        {
          "socket.FD_SETSIZE", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            return return_value(vm, context, vint(FD_SETSIZE));
          }
        },
        {
          "socket.select", // (nfds: int, rfds: iarray8, wfds: iarray8, efds: iarray8, timeout: option tuple, io: uio) -> (option (int, iarray8, iarray8, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, ciarray8, ciarray8, coption(ctimeval), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value io_v = args[5];
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vfd_set(rfds), vfd_set(rfds), vfd_set(rfds))), v(io_v)));
          }
        },
        {
          "socket.uselect", // (nfds: int, rfds: uiarray8, wfds: uiarray8, efds: uiarray8, timeout: option tuple, io: uio) -> ((int, uiarray8, uiarray8, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cuiarray8, cuiarray8, coption(ctimeval), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &rfds_v = args[1], wfds_v = args[2], efds_v = args[3], io_v = args[5];
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
            int result = ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
            system_fd_set_to_object(rfds, *(rfds_v.r().ptr()));
            system_fd_set_to_object(wfds, *(wfds_v.r().ptr()));
            system_fd_set_to_object(efds, *(efds_v.r().ptr()));
            return return_value(vm, context, vut(vut(vint(result), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
          }
        },
        {
          "socket.poll", // (fds: rarray, timeout: int, io: uio) -> (option (int, rarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cpollfds, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::poll(fds.ptr(), fds.length(), timeout);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vpollfds(fds))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
             return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "socket.upoll", // (fds: urarray, timeout: int, io: uio) -> ((int, urarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cupollfds, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &fds_v = args[0], io_v = args[2];
#if defined(__unix__)
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            int result = ::poll(fds.ptr(), fds.length(), timeout);
            if(result == -1)
              return return_value_with_errno_for_socket(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            system_pollfds_to_object(fds, *(fds_v.r().ptr()));
            return return_value(vm, context, vut(vut(vint(result), v(fds_v)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // Address netive functions.
        //

        {
          "socket.getaddrinfo", // (node: option iarray8, service: option iarray8, hints: option tuple, io: uio) -> (either int (list tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, coption(caddrinfo), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string node, service;
            bool is_node, is_service;
            AddressInfo addr_info;
            bool is_addr_info;
            unique_ptr<struct ::addrinfo, AddrinfoDelete> res;
            int tmp_errno = letin_errno();
            letin_errno() = 0;
            if(!convert_args(args, tooption(tostr(node), is_node), tooption(tostr(service), is_service), tooption(toaddrinfo(addr_info), is_addr_info))) {
              if(letin_errno() == 0)
                return return_value(vm, context, vut(vleft(vint(8)), v(io_v)));
              else
                return return_value(vm, context, vut(vleft(vint(2)), v(io_v)));
            }
            letin_errno() = tmp_errno;
            struct ::addrinfo *tmp_res;
            int result = ::getaddrinfo((is_node ? node.c_str() : nullptr), (is_service ? service.c_str() : nullptr), &(addr_info.info), &tmp_res);
            res = unique_ptr<struct ::addrinfo, AddrinfoDelete>(tmp_res);
            if(result != 0) {
              int addr_info_error = system_addr_info_error_to_addr_info_error(result);
#if defined(__unix__)
              if(result == EAI_SYSTEM)
                return return_value_with_errno_for_socket(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
              else
                return return_value(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
              return return_value(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
#else
#error "Unsupported operating system."
#endif
            }
            return return_value(vm, context, vut(vright(vaddrinfo(res.get())), v(io_v)));
          }
        },
        {
          "socket.getnameinfo", // (sa: tuple, flags: int, io: uio) -> (either int (iarray8, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, csockaddr, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            SocketAddress addr;
            int flags, tmp_errno = letin_errno();
            unique_ptr<char []> host(new char[NI_MAXHOST]);
            unique_ptr<char []> service(new char[NI_MAXSERV]);
            letin_errno() = 0;
            if(!convert_args(args, tosockaddr(addr), toniflags(flags))) {
              if(letin_errno() == 0)
                return return_value(vm, context, vut(vleft(vint(8)), v(io_v)));
              else
                return return_value(vm, context, vut(vleft(vint(2)), v(io_v)));
            }
            letin_errno() = tmp_errno;
            int result = ::getnameinfo(&(addr.addr), addr.length(), host.get(), NI_MAXHOST, service.get(), NI_MAXSERV, flags);
            if(result != 0) {
              int addr_info_error = system_addr_info_error_to_addr_info_error(result);
#if defined(__unix__)
              if(result == EAI_SYSTEM)
                return return_value_with_errno_for_socket(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
              else
                return return_value(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
              return return_value(vm, context, vut(vleft(vint(addr_info_error)), v(io_v)));
#else
#error "Unsupported operating system."
#endif
            }
            return return_value(vm, context, vut(vright(vt(vcstr(host.get()), vcstr(service.get()))), v(io_v)));
          }
        }
      };
      return true;
    } catch(...) {
      return false;
    }
  }

  void letin_finalize() {}

  NativeFunctionHandler *letin_new_native_function_handler()
  {
    return new_native_library_without_throwing(native_funs);
  }
}
