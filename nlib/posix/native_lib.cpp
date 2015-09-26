/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <limits>
#include <mutex>
#include <poll.h>
#include <string>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <letin/native.hpp>
#include "posix.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::native;
using namespace letin::nlib::posix;

extern char **environ;

static mutex getgroups_fun_mutex;

static vector<NativeFunction> native_funs;

extern "C" {
  bool letin_initialize()
  {
    try {
      initialize_errors();
      initialize_consts();
      native_funs = {

        //
        // An environment native function.
        //

        {
          "posix.environ", // () -> rarray
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            size_t n = 0;
            for(n = 0; environ[n] != nullptr; n++);
            RegisteredReference env_r(vm->gc()->new_object(OBJECT_TYPE_RARRAY, n, context), context, false);
            if(env_r.is_null()) return ReturnValue::error(ERROR_OUT_OF_MEMORY);
            fill_n(env_r->raw().rs, n, Reference());
            env_r.register_ref();
            for(size_t i = 0; environ[i] != nullptr; i++) {
              size_t env_var_len = strlen(environ[i]);
              RegisteredReference env_elem_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, env_var_len, context), context, false);
              if(env_elem_r.is_null()) return ReturnValue::error(ERROR_OUT_OF_MEMORY);
              copy_n(environ[i], env_var_len, env_elem_r->raw().is8);
              env_elem_r.register_ref();
              env_r->set_elem(i, env_elem_r);
            }
            return return_value(vm, context, vref(env_r));
          }
        },

        //
        // Error native functions.
        //

        {
          "posix.errno", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            int tmp_errno = system_error_to_error(letin_errno());
            return return_value(vm, context, vut(vint(tmp_errno), v(io_v)));
          }
        },
        {
          "posix.set_errno", // (new_errno: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            int new_errno;
            if(!convert_args(args, toerrno(new_errno)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            letin_errno() = new_errno;
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },

        //
        // IO native functions.
        //

        {
          "posix.read", // (fd: int, count: int, io: uio) -> (option (int, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            size_t count;
            if(!convert_args(args, tofd(fd), tocount(count)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, count, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
            ::ssize_t result = ::read(fd, buf_r->raw().is8, count);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r))), v(io_v)));
          }
        },
        {
          "posix.write", // (fd: int, buf: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            Reference buf_r;
            if(!convert_args(args, tofd(fd), tobufref(buf_r)))
              return return_value(vm, context, vut(vt(vint(-1)), v(io_v)));
            ::ssize_t result = ::write(fd, buf_r->raw().is8, buf_r->length());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.uread", // (fd: int, buf: uiarray8, offset: int, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[4];
            int fd;
            Reference buf_r;
            size_t offset, count;
            if(!convert_args(args, tofd(fd), toref(buf_r), tosize(offset), tocount(count)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + count >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            ::ssize_t result = ::read(fd, buf_r->raw().is8 + offset, count);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "posix.uwrite", // (fd: int, buf: uiarray8, offset: int, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[4];
            int fd;
            Reference buf_r;
            size_t offset, count;
            if(!convert_args(args, tofd(fd), toref(buf_r), tosize(offset), tocount(count)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + count >= buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
            ::ssize_t result = ::write(fd, buf_r->raw().is8 + offset, count);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "posix.lseek", // (fd: int, offset: int, whence: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int fd, whence;
#if defined(HAVE_OFF64_T) && defined(HAVE_LSEEK64)
            off64_t offset;
            if(!convert_args(args, tofd(fd), tooff64(offset), towhence(whence)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            off64_t result = ::lseek64(fd, offset, whence);
#else
            off_t offset;
            if(!convert_args(args, tofd(fd), tooff(offset), towhence(whence)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            off_t result = ::lseek(fd, offset, whence);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.open", // (path_name: iarray8, flags: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string path_name;
            int flags;
            mode_t mode;
            if(!convert_args(args, topath(path_name), toflags(flags), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#ifdef HAVE_OPEN64
            int fd = ::open64(path_name.c_str(), flags, mode);
#else
            int fd = ::open(path_name.c_str(), flags, mode);
#endif
            if(fd == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(fd), v(io_v)));
          }
        },
        {
          "posix.close", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::close(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.pipe", // (io: uio) -> (option (int, int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            int fds[2];
            int result = ::pipe(fds);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(fds[0]), vint(fds[1]))), v(io_v)));
          }
        },
        {
          "posix.dup", // (old_fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int old_fd;
            if(!convert_args(args, tofd(old_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::dup(old_fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.dup2", // (old_fd: int, int new_fd, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int old_fd, new_fd;
            if(!convert_args(args, tofd(old_fd), tofd(new_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::dup2(old_fd, new_fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.FD_SETSIZE", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            return return_value(vm, context, vint(FD_SETSIZE));
          }
        },
        {
          "posix.select", // (nfds: int, rfds: iarray8, wfds: iarray8, efds: iarray8, timeout: option tuple, io: uio) -> (option (int, iarray8, iarray8, iarray8), uio)
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
          "posix.uselect", // (nfds: int, rfds: uiarray8, wfds: uiarray8, efds: uiarray8, timeout: option tuple, io: uio) -> ((int, uiarray8, uiarray8, uiarray8), uio)
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
          "posix.poll", // (fds: rarray, timeout: int, io: uio) -> (option (int, rarray), uio)
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
          "posix.upoll", // (fds: urarray, timeout: int, io: uio) -> ((int, urarray), uio)
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
        },
        {
          "posix.fcntl_dupfd", // (fd: int, min_fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, min_fd;
            if(!convert_args(args, tofd(fd), tofd(min_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_DUPFD, min_fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.fcntl_getfd", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_GETFD);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.fcntl_setfd", // (fd: int, bit: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, bit;
            if(!convert_args(args, tofd(fd), toarg(bit)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETFD, bit);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fcntl_getfl", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int system_flags = ::fcntl(fd, F_GETFL);
            if(system_flags == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            int flags = system_flags_to_flags(system_flags);
            return return_value(vm, context, vut(vint(flags), v(io_v)));
          }
        },
        {
          "posix.fcntl_setfl", // (fd: int, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, flags;
            if(!convert_args(args, tofd(fd), toflags(flags)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETFL, flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fcntl_getlk", // (fd: int, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_FLOCK64) && defined(F_GETLK64)
            struct ::flock64 lock;
            int result = ::fcntl(fd, F_GETLK64, &lock);
#else
            struct ::flock lock;
            int result = ::fcntl(fd, F_GETLK, &lock);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_FLOCK64) && defined(F_GETLK64)
            return return_value(vm, context, vut(vsome(vflock64(lock)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vflock(lock)), v(io_v)));
#endif
          }
        },
        {
          "posix.fcntl_setlk", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cflock, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
#if defined(HAVE_STRUCT_FLOCK64) && defined(F_SETLK64)
            struct ::flock64 lock;
            if(!convert_args(args, tofd(fd), toflock64(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETLK64, &lock);
#else
            struct ::flock lock;
            if(!convert_args(args, tofd(fd), toflock(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETLK, &lock);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fcntl_setlkw", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cflock, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
#if defined(HAVE_STRUCT_FLOCK64) && defined(F_SETLKW64)
            struct ::flock64 lock;
            if(!convert_args(args, tofd(fd), toflock64(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETLKW64, &lock);
#else
            struct ::flock lock;
            if(!convert_args(args, tofd(fd), toflock(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETLKW, &lock);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },

        //
        // File system native functions.
        //

        {
          "posix.stat", // (path_name: iarray8, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string path_name;
            if(!convert_args(args, topath(path_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_STAT64)
            struct ::stat64 stat_buf;
            int result = ::stat64(path_name.c_str(), &stat_buf);
#else
            struct ::stat stat_buf;
            int result = ::stat(path_name.c_str(), &stat_buf);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_STAT64)
            return return_value(vm, context, vut(vsome(vstat64(stat_buf)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vstat(stat_buf)), v(io_v)));
#endif
          }
        },
        {
          "posix.lstat", // (path_name: iarray8, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string path_name;
            if(!convert_args(args, topath(path_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_LSTAT64)
            struct ::stat64 stat_buf;
            int result = ::lstat64(path_name.c_str(), &stat_buf);
#else
            struct ::stat stat_buf;
            int result = ::lstat(path_name.c_str(), &stat_buf);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_LSTAT64)
            return return_value(vm, context, vut(vsome(vstat64(stat_buf)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vstat(stat_buf)), v(io_v)));
#endif
          }
        },
        {
          "posix.fstat", // (fd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_FSTAT64)
            struct ::stat64 stat_buf;
            int result = ::fstat64(fd, &stat_buf);
#else
            struct ::stat stat_buf;
            int result = ::fstat(fd, &stat_buf);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_FSTAT64)
            return return_value(vm, context, vut(vsome(vstat64(stat_buf)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vstat(stat_buf)), v(io_v)));
#endif
          }
        },
        {
          "posix.access", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            int mode;
            if(!convert_args(args, topath(path_name), toamode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::access(path_name.c_str(), mode);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getcwd", // (io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            unique_ptr<char []> buf(new char[PATH_MAX + 1]);
            fill_n(buf.get(), PATH_MAX + 1, 0);
            char *result = ::getcwd(buf.get(), PATH_MAX + 1);
            if(result == nullptr)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            system_path_name_to_path_name(buf.get());
            return return_value(vm, context, vut(vsome(vcstr(buf.get())), v(io_v)));
          }
        },
        {
          "posix.chdir", // (dir_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::chdir(dir_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fchdir", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fchdir(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.mkdir", // (dir_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string dir_name;
            ::mode_t mode;
            if(!convert_args(args, topath(dir_name), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::mkdir(dir_name.c_str(), mode);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.rmdir", // (dir_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::rmdir(dir_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.link", // (old_file_name: iarray8, new_file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string old_file_name, new_file_name;
            if(!convert_args(args, topath(old_file_name), topath(new_file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::link(old_file_name.c_str(), new_file_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.unlink", // (file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string file_name;
            if(!convert_args(args, topath(file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::unlink(file_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.rename", // (old_path_name: iarray8, new_path_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string old_path_name, new_path_name;
            if(!convert_args(args, topath(old_path_name), topath(new_path_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = rename(old_path_name.c_str(), new_path_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.symlink", // (old_path_name: iarray8, new_path_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string old_file_name, new_file_name;
            if(!convert_args(args, topath(old_file_name), topath(new_file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::symlink(old_file_name.c_str(), new_file_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.readlink", // (path_name: iarray8, io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string path_name;
            unique_ptr<char []> buf(new char[PATH_MAX + 1]);
            fill_n(buf.get(), PATH_MAX + 1, 0);
            if(!convert_args(args, topath(path_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::readlink(path_name.c_str(), buf.get(), PATH_MAX + 1);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            system_path_name_to_path_name(buf.get());
            return return_value(vm, context, vut(vsome(vcstr(buf.get())), v(io_v)));
          }
        },
        {
          "posix.chmod", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            ::mode_t mode;
            if(!convert_args(args, topath(path_name), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::chmod(path_name.c_str(), mode);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fchmod", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            ::mode_t mode;
            if(!convert_args(args, tofd(fd), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fchmod(fd, mode);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.chown", // (path_name: iarray8, owner: int, group: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string path_name;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, topath(path_name), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::chown(path_name.c_str(), owner, group);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.lchown", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string path_name;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, topath(path_name), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::lchown(path_name.c_str(), owner, group);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fchown", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int fd;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, tofd(fd), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fchown(fd, owner, group);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.mknod", // (path_name: iarray8, mode: int, dev: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string path_name;
            ::mode_t mode;
            ::dev_t dev;
            if(!convert_args(args, topath(path_name), tosmode(mode), todev(dev)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            if(S_ISDIR(mode) != 0)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), EINVAL);
            int result = ::mknod(path_name.c_str(), mode, dev);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.utimes", // (path_name: iarray8, times: (tuple, tuple), io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ct(ctimeval, ctimeval), cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            struct ::timeval times[2];
            if(!convert_args(args, topath(path_name), tot(totimeval(times[0]), totimeval(times[1]))))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::utimes(path_name.c_str(), times);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.umask", // (mask: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::mode_t mask;
            if(!convert_args(args, tomode(mask)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::mode_t new_mask = ::umask(mask);
            return return_value(vm, context, vut(vint(static_cast<int>(new_mask)), v(io_v)));
          }
        },

        //
        // Process native functions.
        //

        {
          "posix.getpid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::pid_t pid = ::getpid();
            return return_value(vm, context, vut(vint(pid), v(io_v)));
          }
        },
        {
          "posix.getppid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::pid_t pid = ::getppid();
            return return_value(vm, context, vut(vint(pid), v(io_v)));
          }
        },
        {
          "posix.fork", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::pid_t pid;
            {
              ForkAround fork_around;
              pid = ::fork();
            }
            if(pid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pid), v(io_v)));
          }
        },
        {
          "posix.waitpid", // (pid: int, options: int, io: uio) -> (option (int, (int, int)), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            ::pid_t pid;
            int status;
            int options;
            if(!convert_args(args, towpid(pid), towoptions(options)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            ::pid_t result = ::waitpid(pid, &status, options);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            if(result != 0) 
              return return_value(vm, context, vut(vsome(vt(vint(result), vwstatus(status))), v(io_v)));
            else
              return return_value(vm, context, vut(vsome(vt(vint(result), vt(vint(0), vint(0)))), v(io_v)));
          }
        },
        {
          "posix.execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cargv, cargv, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string file_name;
            Argv argv, env;
            if(!convert_args(args, topath(file_name), toargv(argv), toargv(env)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::execve(file_name.c_str(), argv.ptr(), env.ptr());
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
          }
        },
        {
          "posix.fork_execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cargv, cargv, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string file_name;
            Argv argv, env;
            if(!convert_args(args, topath(file_name), toargv(argv), toargv(env)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t pid = ::fork();
            if(pid == 0) {
              ::execve(file_name.c_str(), argv.ptr(), env.ptr());
              ::_exit(-1);
            } else if(pid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pid), v(io_v)));
          }
        },
        {
          "posix.exit", // (status: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int status;
            if(!convert_args(args, toarg(status)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            exit(status);
          }
        },
        {
          "posix.kill", // (pid: int, sig: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            ::pid_t pid;
            int sig;
            if(!convert_args(args, topid(pid), tosig(sig)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::kill(pid, sig);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getpgid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::pid_t pid;
            if(!convert_args(args, topid(pid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t pgid = ::getpgid(pid);
            if(pgid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pgid), v(io_v)));
          }
        },
        {
          "posix.setpgid", // (pid: int, pgid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            ::pid_t pid, pgid;
            if(!convert_args(args, topid(pid), topid(pgid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setpgid(pid, pgid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getsid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::pid_t pid;
            if(!convert_args(args, topid(pid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t sid = ::getsid(pid);
            if(sid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sid), v(io_v)));
          }
        },
        {
          "posix.setsid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::pid_t sid = ::setsid();
            if(sid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sid), v(io_v)));
          }
        },
        {
          "posix.pause", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            int result = ::pause();
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.usleep", // (useconds: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::useconds_t useconds;
            if(!convert_args(args, touseconds(useconds)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::usleep(useconds);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.nanosleep", // (req: tuple, io: uio) -> ((int, option tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ctimespec, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            struct ::timespec req, rem;
            if(!convert_args(args, totimespec(req)))
              return return_value(vm, context, vut(vt(vint(-1), vnone), v(io_v)));
            int result = ::nanosleep(&req, &rem);
            if(result == -1) {
              if(errno == EINTR)
                return return_value_with_errno(vm, context, vut(vt(vint(-1), vsome(vtimespec(rem))), v(io_v)));
              else
                return return_value_with_errno(vm, context, vut(vt(vint(-1), vnone), v(io_v)));
            }
            return return_value(vm, context, vut(vt(vint(0), vnone), v(io_v)));
          }
        },
        {
          "posix.CLK_TCK", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return return_value(vm, context, vint(system_clk_tck()));
          }
        },
        {
          "posix.times", // (io: uio) -> (option (int, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            struct ::tms buf;
            ::clock_t result = ::times(&buf);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vtms(buf))), v(io_v)));
          }
        },
        {
          "posix.nice", // (inc: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int inc;
            if(!convert_args(args, toarg(inc)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::nice(inc);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getpriority", // (which: int, who: int, io: uio) -> (option int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int which, who;
            errno = 0;
            if(!convert_args(args, towhich(which), toarg(who)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getpriority(which, who);
            if(result == -1 && errno != 0)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vint(result)), v(io_v)));
          }
        },
        {
          "posix.setpriority", // (which: int, who: int, prio: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int which, who, prio;
            if(!convert_args(args, towhich(which), toarg(who), toarg(prio)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setpriority(which, who, prio);
            if(result == -1 && errno != 0)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },

        //
        // User native functions.
        //

        {
          "posix.getuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::uid_t uid = ::getuid();
            return return_value(vm, context, vut(vint(static_cast<int64_t>(uid)), v(io_v)));
          }
        },
        {
          "posix.setuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::uid_t uid;
            if(!convert_args(args, touid(uid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setuid(uid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getgid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::gid_t gid = ::getgid();
            return return_value(vm, context, vut(vint(static_cast<int64_t>(gid)), v(io_v)));
          }
        },
        {
          "posix.setgid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::gid_t gid;
            if(!convert_args(args, touid(gid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setgid(gid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.geteuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::uid_t uid = ::geteuid();
            return return_value(vm, context, vut(vint(static_cast<int>(uid)), v(io_v)));
          }
        },
        {
          "posix.seteuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::uid_t uid;
            if(!convert_args(args, touid(uid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::seteuid(uid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getegid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::gid_t gid = ::getegid();
            return return_value(vm, context, vut(vint(static_cast<int>(gid)), v(io_v)));
          }
        },
        {
          "posix.setegid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::gid_t gid;
            if(!convert_args(args, touid(gid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setegid(gid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getgroups", // (io: uio) -> (option iarray32, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            {
              lock_guard<mutex> guard(getgroups_fun_mutex);
              int group_count = ::getgroups(0, nullptr);
              if(group_count == -1)
                return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
              unique_ptr<::gid_t> groups(new ::gid_t[group_count]);
              int result = ::getgroups(group_count, groups.get());
              if(result == -1)
                return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
              RegisteredReference groups_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY32, group_count, context), context);
              if(groups_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
              for(int i = 0; i < group_count; i++) groups_r->set_elem(i, static_cast<int64_t>(groups.get()[i]));
              return return_value(vm, context, vut(vsome(vref(groups_r)), v(io_v)));
            }
          }
        },
        {
          "posix.setgroups", // (groups: iarray32, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray32, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            Array<::gid_t> groups;
            if(!convert_args(args, togids(groups)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setgroups(groups.length(), groups.ptr());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },

        //
        // A time native function.
        //

        {
          "posix.gettimeofday", // (io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            struct ::timeval tv;
            int result = ::gettimeofday(&tv, nullptr);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vtimeval(tv)), v(io_v)));
          }
        },

        //
        // Terminal native functions.
        //

        {
          "posix.tcgetattr", // (fd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            struct ::termios termios;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::tcgetattr(fd, &termios);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vtermios(termios)), v(io_v)));
          }
        },
        {
          "posix.tcsetattr", // (fd: int, optional_actions: int, termios: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, ctermios, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int fd, optional_actions;
            struct ::termios termios;
            if(!convert_args(args, tofd(fd), tooactions(optional_actions), totermios(termios)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcsetattr(fd, optional_actions, &termios);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(1), v(io_v)));
          }
        },
        {
          "posix.tcsendbreak", // (fd: int, duration: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, duration;
            if(!convert_args(args, tofd(fd), toarg(duration)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcsendbreak(fd, duration);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.tcdrain", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcdrain(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.tcflush", // (fd: int, queue_selector: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, queue_selector;
            if(!convert_args(args, tofd(fd), toqselector(queue_selector)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcflush(fd, queue_selector);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.tcflow", // (fd: int, action: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd, action;
            if(!convert_args(args, tofd(fd), totaction(action)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcflow(fd, action);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.tcgetpgrp", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t pgrp = ::tcgetpgrp(fd);
            if(pgrp == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pgrp), v(io_v)));
          }
        },
        {
          "posix.tcsetpgrp", // (fd: int, pgrp: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            ::pid_t pgrp;
            if(!convert_args(args, tofd(fd), topid(pgrp)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcsetpgrp(fd, pgrp);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.isatty", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::isatty(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.ttyname", // (fd: int, io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            unique_ptr<char []> buf(new char[TTY_NAME_MAX + 1]);
            fill_n(buf.get(), TTY_NAME_MAX + 1, 0);
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::ttyname_r(fd, buf.get(), TTY_NAME_MAX + 1);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vcstr(buf.get())), v(io_v)));
          }
        },

        //
        // Other native functions.
        //

        {
          "posix.sync", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            ::sync();
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fsync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fsync(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fdatasync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fdatasync(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.uname", // (io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            struct ::utsname buf;
            int result = ::uname(&buf);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vutsname(buf)), v(io_v)));
          }
        },

        //
        // Directory native functions.
        //

        {
          "posix.opendir", // (dir_name: iarray8, io: uio) -> (uoption unative, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vunone, v(io_v)));
            ::DIR *dir;
            {
              lock_guard<recursive_mutex> guard(new_mutex);
              dir = ::opendir(dir_name.c_str());
            }
            if(dir == nullptr)
              return return_value_with_errno(vm, context, vut(vunone, v(io_v)));
            return return_value(vm, context, vut(vusome(vdir(dir)), v(io_v)));
          }
        },
        {
          "posix.closedir", // (dir: unative, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[1];
            ::DIR *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            *reinterpret_cast<::DIR **>(dir_v.r()->raw().ntvo.bs) = nullptr;
            atomic_thread_fence(memory_order_release);
            int result;
            {
              lock_guard<recursive_mutex> guard(new_mutex);
              result = ::closedir(dir);
            }
            if(result == -1) 
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.readdir", // (dir: unative, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::DIR *dir;
            unique_ptr<struct ::dirent> dir_entry;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            struct ::dirent *result = nullptr;
            int tmp_errno = ::readdir_r(dir, dir_entry.get(), &result);
            if(tmp_errno != 0)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)), tmp_errno);
            if(result != nullptr)
              return return_value(vm, context, vut(vsome(vdirent(*dir_entry)), v(io_v)));
            else
              return return_value(vm, context, vut(vnone, v(io_v)));
          }
        },
        {
          "posix.rewinddir", // (dir: unative, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::DIR *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::rewinddir(dir);
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.seekdir", // (dir: unative, loc: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cint, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            ::DIR *dir;
            long loc;
            if(!convert_args(args, todir(dir), tolarg(loc)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::seekdir(dir, loc);
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.telldir", // (dir: unative, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cio);
            if(error != ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::DIR *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            long loc = ::telldir(dir);
            return return_value(vm, context, vut(vint(loc), v(io_v)));
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
