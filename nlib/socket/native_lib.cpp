/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <unistd.h>
#include <letin/native.hpp>
#include "socket.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::native;
using namespace letin::nlib::socket;

static vector<NativeFunction> native_funs;

extern "C" {
  bool letin_initialize()
  {
    try {
      native_funs = {
        {
          "socket.socket", // (domain: int, type: int, protocol: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int domain, type, protocol;
            if(!convert_args(args, todomain(domain), totype(type), toprotocol(protocol)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int sd = ::socket(domain, type, protocol);
            if(sd == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sd), v(io_v)));
          }
        },
        {
          "socket.socketpair", // (domain: int, type: int, protocol: int, io: uio) -> (option (int, int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int domain, type, protocol;
            int sv[2];
            if(!convert_args(args, todomain(domain), totype(type), toprotocol(protocol)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::socketpair(domain, type, protocol, sv);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(sv[0]), vint(sv[1]))), v(io_v)));
          }
        },
        {
          "socket.shutdown", // (sd: int, how: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd, how;
            if(!convert_args(args, tosd(sd), tohow(how)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::shutdown(sd, how);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.connect", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, csockaddr, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::connect(sd, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.bind", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, csockaddr, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::bind(sd, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.listen", // (sd: int, backlog: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int sd, backlog;
            if(!convert_args(args, tosd(sd), toarg(backlog)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::listen(sd, backlog);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "socket.accept", // (sd: int, io: uio) -> (option (int, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd), tosockaddr(addr)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int accepted_sd = ::accept(sd, &(addr.addr), &addr_len);
            if(accepted_sd == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(accepted_sd), vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.recv", // (sd: int, len: int, flags: int, io: uio) -> (option (int, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            size_t len;
            if(!convert_args(args, tosd(sd), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, len, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
            ::ssize_t result = ::recv(sd, buf_r->raw().is8, len, flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r))), v(io_v)));
          }
        },
        {
          "socket.send", // (sd: int, buf: iarray8, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            Reference buf_r;
            if(!convert_args(args, tosd(sd), tobufref(buf_r), toflags(flags)))
              return return_value(vm, context, vut(vt(vint(-1)), v(io_v)));
            ::ssize_t result = ::send(sd, buf_r->raw().is8, buf_r->length(), flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "socket.urecv", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset, len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            ::ssize_t result = ::recv(sd, buf_r->raw().is8 + offset, len, flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.usend", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset, len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            ::ssize_t result = ::send(sd, buf_r->raw().is8 + offset, len, flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.recvfrom", // (sd: int, len: int, flags: int, io: uio) -> (option (int, iarray8, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, csockaddr, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, flags;
            size_t len;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd), tolen(len), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, len, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
            ::ssize_t result = ::recvfrom(sd, buf_r->raw().is8, len, flags, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r), vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.sendto", // (sd: int, buf: iarray8, flags: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cint, csockaddr, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[4];
            int sd, flags;
            Reference buf_r;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), tobufref(buf_r), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::ssize_t result = ::sendto(sd, buf_r->raw().is8, buf_r->length(), flags, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "socket.urecvfrom", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, io: uio) -> ((int, uiarray8, option tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[5];
            int sd, flags;
            Reference buf_r;
            size_t offset, len;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v), vnone), v(io_v)), EINVAL);
            ::ssize_t result = ::recvfrom(sd, buf_r->raw().is8 + offset, len, flags, &(addr.addr), &(addr_len));
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v), vnone), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v), vsome(vsockaddr(addr))), v(io_v)));
          }
        },
        {
          "socket.usendto", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, addr: tuple, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cint, csockaddr, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[6];
            int sd, flags;
            Reference buf_r;
            size_t offset, len;
            SocketAddress addr;
            if(!convert_args(args, tosd(sd), toref(buf_r), tosize(offset), tolen(len), toflags(flags), tosockaddr(addr)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + len >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            ::ssize_t result = ::sendto(sd, buf_r->raw().is8 + offset, len, flags, &(addr.addr), addr.length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "socket.getpeername", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getpeername(sd, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vsockaddr(addr)), v(io_v)));
          }
        },
        {
          "socket.getsockname", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int sd;
            SocketAddress addr;
            ::socklen_t addr_len;
            if(!convert_args(args, tosd(sd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getsockname(sd, &(addr.addr), &addr_len);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vsockaddr(addr)), v(io_v)));
          }
        },
        {
          "socket.getsockopt", // (sd: int, level: int, opt_name: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int sd, level, opt_name;
            ::socklen_t opt_len;
            if(!convert_args(args, tosd(sd), tolevel(level), tooptname(opt_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            switch(opt_name) {
              case SO_LINGER:
              {
                struct ::linger opt_val;
                int result = ::getsockopt(sd, level, opt_name, &opt_val, &opt_len);
                if(result == -1)
                  return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
                return return_value(vm, context, vut(vsome(vt(vint(1), vlinger(opt_val))), v(io_v)));
              }
              case SO_RCVTIMEO:
              case SO_SNDTIMEO:
              {
                struct ::timeval opt_val;
                int result = ::getsockopt(sd, level, opt_name, &opt_val, &opt_len);
                if(result == -1)
                  return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
                return return_value(vm, context, vut(vsome(vt(vint(2), vtimeval(opt_val))), v(io_v)));
              }
              default:
              {
                int opt_val;
                int result = ::getsockopt(sd, level, opt_name, &opt_val, &opt_len);
                if(result == -1)
                  return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
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
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[4];
            int sd, level, opt_name;
            OptionValue opt_val;
            if(!convert_args(args, tosd(sd), tolevel(level), tooptname(opt_name), tooptval(opt_val)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setsockopt(sd, level, opt_name, reinterpret_cast<const void *>(&(opt_val.u)), opt_val.length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(-1), v(io_v)));
          }
        },
        {
          "socket.FD_SETSIZE", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            return return_value(vm, context, vint(FD_SETSIZE));
          }
        },
        {
          "socket.select", // (nfds: int, rfds: iarray8, wfds: iarray8, efds: iarray8, timeout: option tuple, io: uio) -> (option (int, iarray8, iarray8, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, ciarray8, ciarray8, coption(ctimeval), cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value io_v = args[5];
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vfd_set(rfds), vfd_set(rfds), vfd_set(rfds))), v(io_v)));
          }
        },
        {
          "socket.uselect", // (nfds: int, rfds: uiarray8, wfds: uiarray8, efds: uiarray8, timeout: option tuple, io: uio) -> ((int, uiarray8, uiarray8, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cuiarray8, cuiarray8, coption(ctimeval), cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &rfds_v = args[1], wfds_v = args[2], efds_v = args[3], io_v = args[5];
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
            int result = ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
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
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::poll(fds.ptr(), fds.length(), timeout);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vpollfds(fds))), v(io_v)));
          }
        },
        {
          "socket.upoll", // (fds: urarray, timeout: int, io: uio) -> ((int, urarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cupollfds, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &fds_v = args[0], io_v = args[2];
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            int result = ::poll(fds.ptr(), fds.length(), timeout);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            system_pollfds_to_object(fds, *(fds_v.r().ptr()));
            return return_value(vm, context, vut(vut(vint(result), v(fds_v)), v(io_v)));
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
