/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <letin/native.hpp>

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::native;

static vector<NativeFunction> native_funs;

extern "C" {
  bool letin_initialize()
  {
    try {
      native_funs = {
        {
          "socket.socket", // (domain: int, type: int, protocol: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.socketpair", // (domain: int, type: int, protocol: int, io: uio) -> (option (int, int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.shutdown", // (sd: int, how: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.connect", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.bind", // (sd: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.listen", // (sd: int, backlog: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.accept", // (sd: int, io: uio) -> (option (int, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.recv", // (sd: int, len: int, flags: int, io: uio) -> (option (int, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.send", // (sd: int, buf: iarray8, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.urecv", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.usend", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.recvfrom", // (sd: int, len: int, flags: int, addr: tuple, io: uio) -> (option (int, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.sendto", // (sd: int, buf: iarray8, flags: int, addr: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.urecvfrom", // (sd: int, buf: uiarray8, offset: int, len: int, flags: int, addr: tuple, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.usendto", // (sd: int, buf: uiarray8, offset: int, lent: int, flags: int, addr: tuple, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.getpeername", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.getsockname", // (sd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.getsockopt", // (sd: int, level: int, opt_name: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.setsockopt", // (sd: int, level: int, opt_name: int, opt_val: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.FD_SETSIZE", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.select", // (nfds: int, rfds: iarray8, wfds: iarray8, efds: iarray8, timeout: option tuple, io: uio) -> (option (int, iarray8, iarray8, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.uselect", // (nfds: int, rfds: uiarray8, wfds: uiarray8, efds: uiarray8, timeout: option tuple, io: uio) -> ((int, uiarray8, uiarray8, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.poll", // (fds: rarray, timeout: int, io: uio) -> (option (int, rarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "socket.upoll", // (fds: urarray, timeout: int, io: uio) -> ((int, urarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
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
