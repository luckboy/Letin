/****************************************************************************
 *   Copyright (C) 2015, 2019 Łukasz Szpakowski.                            *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__unix__)
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <grp.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <sys/time.h>
#include <pthread_time.h>
#include <unistd.h>
#endif
#include <io.h>
#include <process.h>
#include <sstream>
#include <utime.h>
#include <windows.h>
#include <winsock2.h>
#else
#error "Unsupported operating system."
#endif
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <signal.h>
#include <string>
#include <time.h>
#include <letin/native.hpp>
#include "posix.hpp"

using namespace std;
using namespace letin;
using namespace letin::vm;
using namespace letin::native;
using namespace letin::nlib::posix;

struct DirectoryEntryDelete
{
  void operator()(void *ptr)
  { delete_directory_entry(reinterpret_cast<DirectoryEntry *>(ptr)); }
};

extern char **environ;

#if defined(__unix__)
static mutex getgroups_fun_mutex;
#endif
static vector<NativeFunction> native_funs;
static MutexForkHandler fork_handler;

#if defined(_WIN32) || defined(_WIN64)

static void add_arg_to_cmd_line(string &cmd_line, const char *arg)
{
  for(size_t i = 0; arg[i] != 0; i++) {
    switch(arg[i]) {
      case '"':
        cmd_line += "\\\"";
        break;
      case '\\':
      {
        size_t backslash_count = 0;
        for(; arg[i] == '\\'; i++) backslash_count++;
        if(arg[i] == '"') {
          for(size_t j = 0; j < backslash_count; j++) cmd_line += "\\\\";
        } else {
          for(size_t j = 0; j < backslash_count; j++) cmd_line += "\\";
        }
        i--;
        break;
      }
      default:
        cmd_line += arg[i];
        break;
    }
  }
}

static void add_file_name_and_argv_to_cmd_line(string &cmd_line, const char *file_name, char **argv)
{ 
  add_arg_to_cmd_line(cmd_line, file_name);
  for(size_t i = 0; argv[i] != nullptr; i++) add_arg_to_cmd_line(cmd_line, argv[i]);
}

static char *env_to_env_block(char **env)
{
  size_t length = 0;
  for(size_t i = 0; env[i] != nullptr; i++) length += strlen(env[i]) + 1;
  length++;
  char *env_block = new char[length];
  char *ptr = env_block;
  for(size_t i = 0; env[i] != nullptr; i++) {
    strcpy(ptr, env[i]);
    ptr += strlen(env[i]) + 1;
  }
  *ptr = 0;
  return env_block;
}

#endif

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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            int tmp_errno = system_error_to_error(letin_errno());
            return return_value(vm, context, vut(vint(tmp_errno), v(io_v)));
          }
        },
        {
          "posix.set_errno", // (new_errno: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            size_t count;
            if(!convert_args(args, tofd(fd), tocount(count)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            RegisteredReference buf_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY8, count, context), context);
            if(buf_r.is_null()) return error_return_value(ERROR_OUT_OF_MEMORY);
            fill_n(buf_r->raw().is8, buf_r->length(), 0);
#if defined(__unix__)
            ::ssize_t result;
            {
              InterruptibleFunctionAround around(context);
              result = ::read(fd, buf_r->raw().is8, count);
            }
#elif defined(_WIN32) || defined(_WIN64)
            ::ssize_t result = ::_read(fd, buf_r->raw().is8, count);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vref(buf_r))), v(io_v)));
          }
        },
        {
          "posix.write", // (fd: int, buf: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int fd;
            Reference buf_r;
            if(!convert_args(args, tofd(fd), tobufref(buf_r)))
              return return_value(vm, context, vut(vt(vint(-1)), v(io_v)));
#if defined(__unix__)
            ::ssize_t result;
            {
              InterruptibleFunctionAround around(context);
              result = ::write(fd, buf_r->raw().is8, buf_r->length());
            }
#elif defined(_WIN32) || defined(_WIN64)
            ::ssize_t result = ::_write(fd, buf_r->raw().is8, buf_r->length());
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.uread", // (fd: int, buf: uiarray8, offset: int, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[4];
            int fd;
            Reference buf_r;
            size_t offset, count;
            if(!convert_args(args, tofd(fd), toref(buf_r), tosize(offset), tocount(count)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + count > buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
#if defined(__unix__)
            ::ssize_t result;
            {
              InterruptibleFunctionAround around(context);
              result = ::read(fd, buf_r->raw().is8 + offset, count);
            }
#elif defined(_WIN32) || defined(_WIN64)
            ::ssize_t result = ::_read(fd, buf_r->raw().is8 + offset, count);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "posix.uwrite", // (fd: int, buf: uiarray8, offset: int, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &buf_v = args[1], io_v = args[4];
            int fd;
            Reference buf_r;
            size_t offset, count;
            if(!convert_args(args, tofd(fd), toref(buf_r), tosize(offset), tocount(count)))
              return return_value(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            if(offset >= buf_r->length() || offset + count > buf_r->length())
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)), EINVAL);
#if defined(__unix__)
            ::ssize_t result;
            {
              InterruptibleFunctionAround around(context);
              result = ::write(fd, buf_r->raw().is8 + offset, count);
            }
#elif defined(_WIN32) || defined(_WIN64)
            ::ssize_t result = ::_write(fd, buf_r->raw().is8 + offset, count);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(buf_v)), v(io_v)));
            return return_value(vm, context, vut(vut(vint(result), v(buf_v)), v(io_v)));
          }
        },
        {
          "posix.lseek", // (fd: int, offset: int, whence: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            int fd, whence;
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            int64_t offset;
            if(!convert_args(args, tofd(fd), toint(offset), towhence(whence)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int64_t result = ::_lseeki64(fd, offset, whence);
#else
#error "Unsupported operating system."
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string path_name;
            int flags;
            ::mode_t mode;
            if(!convert_args(args, topath(path_name), toflags(flags), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int fd;
            {
              InterruptibleFunctionAround around(context);
#ifdef HAVE_OPEN64
              fd = ::open64(path_name.c_str(), flags, mode);
#else
              fd = ::open(path_name.c_str(), flags, mode);
#endif
            }
#elif defined(_WIN32) || defined(_WIN64)
            int fd = ::_open(path_name.c_str(), flags | O_BINARY, mode);
#else
#error "Unsupported operating system."
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::close(fd);
            }
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_close(fd);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.pipe", // (io: uio) -> (option (int, int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            int fds[2];
            int result = ::pipe(fds);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(fds[0]), vint(fds[1]))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.dup", // (old_fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int old_fd;
            if(!convert_args(args, tofd(old_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::dup(old_fd);
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_dup(old_fd);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.dup2", // (old_fd: int, int new_fd, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            int old_fd, new_fd;
            if(!convert_args(args, tofd(old_fd), tofd(new_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            int result = ::dup2(old_fd, new_fd);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::dup2(old_fd, new_fd);
            }
#endif
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_dup2(old_fd, new_fd);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.FD_SETSIZE", // () -> int
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            return return_value(vm, context, vint(FD_SETSIZE));
          }
        },
        {
          "posix.select", // (nfds: int, rfds: iarray8, wfds: iarray8, efds: iarray8, timeout: option tuple, io: uio) -> (option (int, iarray8, iarray8, iarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, ciarray8, ciarray8, ciarray8, coption(ctimeval), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value io_v = args[5];
#if defined(__unix__)
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vfd_set(rfds), vfd_set(rfds), vfd_set(rfds))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.uselect", // (nfds: int, rfds: uiarray8, wfds: uiarray8, efds: uiarray8, timeout: option tuple, io: uio) -> ((int, uiarray8, uiarray8, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuiarray8, cuiarray8, cuiarray8, coption(ctimeval), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &rfds_v = args[1], wfds_v = args[2], efds_v = args[3], io_v = args[5];
#if defined(__unix__)
            bool is_timeout;
            int nfds;
            ::fd_set rfds, wfds, efds;
            struct ::timeval timeout;
            if(!convert_args(args, tonfds(nfds), tofd_set(rfds), tofd_set(wfds), tofd_set(efds), tooption(totimeval(timeout), is_timeout)))
              return return_value(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
            int result;
            {
              InterruptibleFunctionAround around(context);
              ::select(nfds, &rfds, &wfds, &efds, (is_timeout ? &timeout : nullptr));
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
            system_fd_set_to_object(rfds, *(rfds_v.r().ptr()));
            system_fd_set_to_object(wfds, *(wfds_v.r().ptr()));
            system_fd_set_to_object(efds, *(efds_v.r().ptr()));
            return return_value(vm, context, vut(vut(vint(result), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vt(vint(-1), v(rfds_v), v(wfds_v), v(efds_v)), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.poll", // (fds: rarray, timeout: int, io: uio) -> (option (int, rarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cpollfds, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::poll(fds.ptr(), fds.length(), timeout);
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vpollfds(fds))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.upoll", // (fds: urarray, timeout: int, io: uio) -> ((int, urarray), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cupollfds, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &fds_v = args[0], io_v = args[2];
#if defined(__unix__)
            Array<struct ::pollfd> fds;
            int timeout;
            if(!convert_args(args, topollfds(fds), toarg(timeout)))
              return return_value(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::poll(fds.ptr(), fds.length(), timeout);
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)));
            if(!system_pollfds_to_object(vm, context, fds, *(fds_v.r().ptr())))
              return error_return_value(ERROR_OUT_OF_MEMORY);
            return return_value(vm, context, vut(vut(vint(result), v(fds_v)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vut(vint(-1), v(fds_v)), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_dupfd", // (fd: int, min_fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, min_fd;
            if(!convert_args(args, tofd(fd), tofd(min_fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_DUPFD, min_fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_getfd", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_GETFD);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_setfd", // (fd: int, bit: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, bit;
            if(!convert_args(args, tofd(fd), toarg(bit)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETFD, bit);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_getfl", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int system_flags = ::fcntl(fd, F_GETFL);
            if(system_flags == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            int flags = system_flags_to_flags(system_flags);
            return return_value(vm, context, vut(vint(flags), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_setfl", // (fd: int, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, flags;
            if(!convert_args(args, tofd(fd), toflags(flags)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::fcntl(fd, F_SETFL, flags);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_getlk", // (fd: int, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_setlk", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cflock, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fcntl_setlkw", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cflock, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd;
#if defined(HAVE_STRUCT_FLOCK64) && defined(F_SETLKW64)
            struct ::flock64 lock;
            if(!convert_args(args, tofd(fd), toflock64(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__linux__)
            int result = ::fcntl(fd, F_SETLKW64, &lock);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::fcntl(fd, F_SETLKW64, &lock);
            }
#endif
#else
            struct ::flock lock;
            if(!convert_args(args, tofd(fd), toflock(lock)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__linux__)
            int result = ::fcntl(fd, F_SETLKW, &lock);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::fcntl(fd, F_SETLKW, &lock);
            }
#endif
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // File system native functions.
        //

        {
          "posix.stat", // (path_name: iarray8, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string path_name;
            if(!convert_args(args, topath(path_name)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(__unix__)
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_STAT64)
            struct ::stat64 stat_buf;
            int result = ::stat64(path_name.c_str(), &stat_buf);
#else
            struct ::stat stat_buf;
            int result = ::stat(path_name.c_str(), &stat_buf);
#endif
#elif defined(_WIN32) || defined(_WIN64)
            struct ::_stat64 stat_buf;
            int result = ::_stat64(path_name.c_str(), &stat_buf);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(__unix__)
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_STAT64)
            return return_value(vm, context, vut(vsome(vstat64(stat_buf)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vstat(stat_buf)), v(io_v)));
#endif
#elif defined(_WIN32) || defined(_WIN64)
            return return_value(vm, context, vut(vsome(v_stat64(stat_buf)), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.lstat", // (path_name: iarray8, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fstat", // (fd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(__unix__)
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_FSTAT64)
            struct ::stat64 stat_buf;
            int result = ::fstat64(fd, &stat_buf);
#else
            struct ::stat stat_buf;
            int result = ::fstat(fd, &stat_buf);
#endif
#elif defined(_WIN32) || defined(_WIN64)
            struct ::_stat64 stat_buf;
            int result = ::_fstat64(fd, &stat_buf);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
#if defined(__unix__)
#if defined(HAVE_STRUCT_STAT64) && defined(HAVE_FSTAT64)
            return return_value(vm, context, vut(vsome(vstat64(stat_buf)), v(io_v)));
#else
            return return_value(vm, context, vut(vsome(vstat(stat_buf)), v(io_v)));
#endif
#elif defined(_WIN32) || defined(_WIN64)
            return return_value(vm, context, vut(vsome(v_stat64(stat_buf)), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.access", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            int mode;
            if(!convert_args(args, topath(path_name), toamode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::access(path_name.c_str(), mode);
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_access(path_name.c_str(), mode);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.getcwd", // (io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
            unique_ptr<char []> buf(new char[PATH_MAX + 1]);
            fill_n(buf.get(), PATH_MAX + 1, 0);
#if defined(__unix__)
            char *result = ::getcwd(buf.get(), PATH_MAX + 1);
#elif defined(_WIN32) || defined(_WIN64)
            char *result = ::_getcwd(buf.get(), PATH_MAX + 1);
#else
#error "Unsupported operating system."
#endif
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::chdir(dir_name.c_str());
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_chdir(dir_name.c_str());
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fchdir", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            int result = ::fchdir(fd);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::fchdir(fd);
            }
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.mkdir", // (dir_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string dir_name;
            ::mode_t mode;
            if(!convert_args(args, topath(dir_name), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::mkdir(dir_name.c_str(), mode);
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_mkdir(dir_name.c_str());
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.rmdir", // (dir_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::rmdir(dir_name.c_str());
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_rmdir(dir_name.c_str());
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.link", // (old_file_name: iarray8, new_file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            string old_file_name, new_file_name;
            if(!convert_args(args, topath(old_file_name), topath(new_file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::link(old_file_name.c_str(), new_file_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.unlink", // (file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string file_name;
            if(!convert_args(args, topath(file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::unlink(file_name.c_str());
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_unlink(file_name.c_str());
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.rename", // (old_path_name: iarray8, new_path_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            string old_file_name, new_file_name;
            if(!convert_args(args, topath(old_file_name), topath(new_file_name)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::symlink(old_file_name.c_str(), new_file_name.c_str());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.readlink", // (path_name: iarray8, io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.chmod", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            ::mode_t mode;
            if(!convert_args(args, topath(path_name), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::chmod(path_name.c_str(), mode);
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_chmod(path_name.c_str(), mode);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fchmod", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd;
            ::mode_t mode;
            if(!convert_args(args, tofd(fd), tomode(mode)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            int result = ::fchmod(fd, mode);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::fchmod(fd, mode);
            }
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.chown", // (path_name: iarray8, owner: int, group: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            string path_name;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, topath(path_name), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::chown(path_name.c_str(), owner, group);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.lchown", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            string path_name;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, topath(path_name), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::lchown(path_name.c_str(), owner, group);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fchown", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            int fd;
            ::uid_t owner;
            ::gid_t group;
            if(!convert_args(args, tofd(fd), touid(owner), togid(group)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            int result = ::fchown(fd, owner, group);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::fchown(fd, owner, group);
            }
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.mknod", // (path_name: iarray8, mode: int, dev: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.utimes", // (path_name: iarray8, times: (tuple, tuple), io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, ct(ctimeval, ctimeval), cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            string path_name;
            struct ::timeval times[2];
            if(!convert_args(args, topath(path_name), tot(totimeval(times[0]), totimeval(times[1]))))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::utimes(path_name.c_str(), times);
#elif defined(_WIN32) || defined(_WIN64)
            struct ::utimbuf tmp_times;
            tmp_times.actime = times[0].tv_sec;
            tmp_times.modtime = times[1].tv_sec;
            int result = ::utime(path_name.c_str(), &tmp_times);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.umask", // (mask: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            ::mode_t mask;
            if(!convert_args(args, tomode(mask)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            ::mode_t new_mask = ::umask(mask);
#elif defined(_WIN32) || defined(_WIN64)
            ::mode_t new_mask = ::_umask(mask);
#else
#error "Unsupported operating system."
#endif
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            Pid pid = ::getpid();
#elif defined(_WIN32) || defined(_WIN64)
            Pid pid = ::GetCurrentProcessId();
#else
#error "Unsupported operating system."
#endif
            return return_value(vm, context, vut(vint(pid), v(io_v)));
          }
        },
        {
          "posix.getppid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            Pid pid = ::getppid();
            return return_value(vm, context, vut(vint(pid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fork", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            Pid pid;
            {
              ForkAround fork_around;
              pid = ::fork();
            }
            if(pid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.waitpid", // (pid: int, options: int, io: uio) -> (option (int, (int, int)), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
            Pid pid;
            int options;
#if defined(__unix__)
            int status;
            if(!convert_args(args, towpid(pid), towoptions(options)))
              return return_value(vm, context, vut(vnone, v(io_v)));
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            Pid result = ::waitpid(pid, &status, options);
#else
            Pid result;
            {
              InterruptibleFunctionAround around(context);
              result = ::waitpid(pid, &status, options);
            }
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            if(result != 0) 
              return return_value(vm, context, vut(vsome(vt(vint(result), vwstatus(status))), v(io_v)));
            else
              return return_value(vm, context, vut(vsome(vt(vint(result), vt(vint(0), vint(0)))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            if(!convert_args(args, towpid(pid), towoptions(options)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            ::HANDLE handle = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if(handle == nullptr)
              return return_value_with_errno_for_windows(vm, context, vut(vnone, v(io_v)), true);
            if((options & 1) == 0) {
              ::DWORD wait_result = ::WaitForSingleObject(handle, INFINITY);
              if(wait_result == WAIT_FAILED)
                return return_value_with_errno_for_windows(vm, context, vut(vnone, v(io_v)), true);
            }
            ::DWORD exit_status;
            ::BOOL result = ::GetExitCodeProcess(handle, &exit_status);
            if(result == FALSE)
              return return_value_with_errno_for_windows(vm, context, vut(vnone, v(io_v)));
            ::CloseHandle(handle);
            if(exit_status != STILL_ACTIVE)
              return return_value(vm, context, vut(vsome(vt(vint(pid), vt(vint(1), vint(exit_status)))), v(io_v)));
            else
              return return_value(vm, context, vut(vsome(vt(vint(pid), vt(vint(0), vint(0)))), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cargv, cargv, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string file_name;
            Argv argv, env;
            if(!convert_args(args, topath(file_name), toargv(argv), toargv(env)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            ::execve(file_name.c_str(), argv.ptr(), env.ptr());
#elif defined(_WIN32) || defined(_WIN64)
            ::_execve(file_name.c_str(), argv.ptr(), env.ptr());
#else
#error "Unsupported operating system."
#endif
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
          }
        },
        {
          "posix.fork_execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cargv, cargv, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
            string file_name;
            Argv argv, env;
            if(!convert_args(args, topath(file_name), toargv(argv), toargv(env)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            Pid pid = ::fork();
            if(pid == 0) {
              ::execve(file_name.c_str(), argv.ptr(), env.ptr());
              ::_exit(-1);
            } else if(pid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            string cmd_line;
            add_file_name_and_argv_to_cmd_line(cmd_line, file_name.c_str(), argv.ptr());
            unique_ptr<char []> env_block(env_to_env_block(env.ptr()));
            ::STARTUPINFO startup_info;
            fill_n(reinterpret_cast<uint8_t *>(&startup_info), sizeof(::STARTUPINFO), 0);
            startup_info.cb = sizeof(::STARTUPINFO);
            startup_info.dwFlags = STARTF_USESTDHANDLES;
            startup_info.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
            startup_info.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
            startup_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
            ::PROCESS_INFORMATION process_info;
            ::BOOL result = ::CreateProcess(file_name.c_str(), const_cast<char *>(cmd_line.c_str()), nullptr, nullptr, TRUE, 0, env_block.get(), nullptr, &startup_info, &process_info);
            if(result == FALSE)
              return return_value_with_errno_for_windows(vm, context, vut(vnone, v(io_v)));
            ::CloseHandle(process_info.hThread);
            ::CloseHandle(process_info.hProcess);
            return return_value(vm, context, vut(vint(process_info.dwProcessId), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.exit", // (status: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            ::pid_t pid;
            int sig;
            if(!convert_args(args, topid(pid), tosig(sig)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::kill(pid, sig);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            ::DWORD pid;
            int sig;
            if(!convert_args(args, topid(pid), toksig(sig)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::HANDLE handle = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if(handle == nullptr)
              return return_value_with_errno_for_windows(vm, context, vut(vint(-1), v(io_v)), true);
            int result = ::TerminateProcess(handle, 0);
            if(result == -1)
              return return_value_with_errno_for_windows(vm, context, vut(vint(-1), v(io_v)), true);
            ::CloseHandle(handle);
            return return_value(vm, context, vut(vint(0), v(io_v)));
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getpgid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::pid_t pid;
            if(!convert_args(args, topid(pid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t pgid = ::getpgid(pid);
            if(pgid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pgid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setpgid", // (pid: int, pgid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            ::pid_t pid, pgid;
            if(!convert_args(args, topid(pid), topid(pgid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setpgid(pid, pgid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getsid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::pid_t pid;
            if(!convert_args(args, topid(pid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t sid = ::getsid(pid);
            if(sid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setsid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::pid_t sid = ::setsid();
            if(sid == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(sid), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.pause", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::pause();
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.usleep", // (useconds: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
            ::useconds_t useconds;
            if(!convert_args(args, touseconds(useconds)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result;
            {
              InterruptibleFunctionAround around(context);
#if defined(__NetBSD__)
              struct ::timespec req;
              req.tv_sec = useconds / 1000000;
              req.tv_nsec = (useconds % 1000000) * 1000;
              result = ::nanosleep(&req, nullptr);
#else
              result = ::usleep(useconds);
#endif
            }
#else
            struct ::timespec req;
            req.tv_sec = useconds / 1000000;
            req.tv_nsec = (useconds % 1000000) * 1000;
            int result = ::nanosleep(&req, nullptr);
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            ::DWORD useconds;
            if(!convert_args(args, touseconds(useconds)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::DWORD milliseconds = useconds / 1000;
            if(milliseconds > 0) ::Sleep(milliseconds);
#else
#error "Unsupported operating system."
#endif
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.nanosleep", // (req: tuple, io: uio) -> ((int, option tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ctimespec, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
            struct ::timespec req, rem;
            if(!convert_args(args, totimespec(req)))
              return return_value(vm, context, vut(vt(vint(-1), vnone), v(io_v)));
#if defined(__unix__)
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::nanosleep(&req, &rem);
            }
#else
            int result = ::nanosleep(&req, &rem);
#endif
            if(result == -1) {
              if(errno == EINTR)
                return return_value_with_errno(vm, context, vut(vt(vint(-1), vsome(vtimespec(rem))), v(io_v)));
              else
                return return_value_with_errno(vm, context, vut(vt(vint(-1), vnone), v(io_v)));
            }
            return return_value(vm, context, vut(vt(vint(0), vnone), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            ::DWORD req_dword;
            if(!convert_args(args, totimespecdw(req_dword)))
              return return_value(vm, context, vut(vt(vint(-1), vnone), v(io_v)));
            if(req_dword > 0) ::Sleep(req_dword);
            return return_value(vm, context, vut(vt(vint(0), vnone), v(io_v)));
#else
#error "Unsupported operating system."
#endif
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
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            struct ::tms buf;
            ::clock_t result = ::times(&buf);
            if(result == static_cast<::clock_t>(-1))
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vt(vint(result), vtms(buf))), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.nice", // (inc: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int inc;
            if(!convert_args(args, toarg(inc)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::nice(inc);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getpriority", // (which: int, who: int, io: uio) -> (option int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int which, who;
            errno = 0;
            if(!convert_args(args, towhich(which), toarg(who)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::getpriority(which, who);
            if(result == -1 && errno != 0)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vint(result)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setpriority", // (which: int, who: int, prio: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            int which, who, prio;
            if(!convert_args(args, towhich(which), toarg(who), toarg(prio)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setpriority(which, who, prio);
            if(result == -1 && errno != 0)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // User native functions.
        //

        {
          "posix.getuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::uid_t uid = ::getuid();
            return return_value(vm, context, vut(vint(static_cast<int64_t>(uid)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::uid_t uid;
            if(!convert_args(args, touid(uid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setuid(uid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getgid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::gid_t gid = ::getgid();
            return return_value(vm, context, vut(vint(static_cast<int64_t>(gid)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setgid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::gid_t gid;
            if(!convert_args(args, touid(gid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setgid(gid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.geteuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::uid_t uid = ::geteuid();
            return return_value(vm, context, vut(vint(static_cast<int>(uid)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.seteuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::uid_t uid;
            if(!convert_args(args, touid(uid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::seteuid(uid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getegid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::gid_t gid = ::getegid();
            return return_value(vm, context, vut(vint(static_cast<int>(gid)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setegid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            ::gid_t gid;
            if(!convert_args(args, touid(gid)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setegid(gid);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.getgroups", // (io: uio) -> (option iarray32, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
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
#elif defined(_WIN32) || defined(_WIN64)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.setgroups", // (groups: iarray32, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray32, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            Array<::gid_t> groups;
            if(!convert_args(args, togids(groups)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::setgroups(groups.length(), groups.ptr());
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // A time native function.
        //

        {
          "posix.gettimeofday", // (io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
            struct ::timeval tv;
            int result = ::gettimeofday(&tv, nullptr);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vtimeval(tv)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            ::SYSTEMTIME system_time;
            ::GetLocalTime(&system_time);
            struct tm time;
            time.tm_year = system_time.wYear - 1900;
            time.tm_mon = system_time.wMonth - 1;
            time.tm_wday = system_time.wDayOfWeek;
            time.tm_mday = system_time.wDay;
            time.tm_hour = system_time.wHour;
            time.tm_min = system_time.wMinute;
            time.tm_sec = system_time.wSecond;
            time.tm_isdst = -1;
            time_t result = mktime(&time);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            long useconds = static_cast<long>(system_time.wMilliseconds) * 1000;
            return return_value(vm, context, vut(vsome(vt(vint(result), vint(useconds))), v(io_v)));
#endif
          }
        },

        //
        // Terminal native functions.
        //

        {
          "posix.tcgetattr", // (fd: int, io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            struct ::termios termios;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::tcgetattr(fd, &termios);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vtermios(termios)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcsetattr", // (fd: int, optional_actions: int, termios: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, ctermios, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[3];
#if defined(__unix__)
            int fd, optional_actions;
            struct ::termios termios;
            if(!convert_args(args, tofd(fd), tooactions(optional_actions), totermios(termios)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result; 
            {
              InterruptibleFunctionAround around(context);
              result = ::tcsetattr(fd, optional_actions, &termios);
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(1), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcsendbreak", // (fd: int, duration: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, duration;
            if(!convert_args(args, tofd(fd), toarg(duration)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcsendbreak(fd, duration);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcdrain", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result;
            {
              InterruptibleFunctionAround around(context);
              result = ::tcdrain(fd);
            }
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcflush", // (fd: int, queue_selector: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, queue_selector;
            if(!convert_args(args, tofd(fd), toqselector(queue_selector)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcflush(fd, queue_selector);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcflow", // (fd: int, action: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd, action;
            if(!convert_args(args, tofd(fd), totaction(action)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcflow(fd, action);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcgetpgrp", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            ::pid_t pgrp = ::tcgetpgrp(fd);
            if(pgrp == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(pgrp), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.tcsetpgrp", // (fd: int, pgrp: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[2];
#if defined(__unix__)
            int fd;
            ::pid_t pgrp;
            if(!convert_args(args, tofd(fd), topid(pgrp)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            int result = ::tcsetpgrp(fd, pgrp);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.isatty", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
            int result = ::isatty(fd);
#elif defined(_WIN32) || defined(_WIN64)
            int result = ::_isatty(fd);
#else
#error "Unsupported operating system."
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(result), v(io_v)));
          }
        },
        {
          "posix.ttyname", // (fd: int, io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
#if defined(__unix__)
            int fd;
            unique_ptr<char []> buf(new char[system_tty_name_max() + 1]);
            fill_n(buf.get(), system_tty_name_max() + 1, 0);
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vnone, v(io_v)));
            int result = ::ttyname_r(fd, buf.get(), system_tty_name_max() + 1);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vcstr(buf.get())), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vnone, v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // Other native functions.
        //

        {
          "posix.sync", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            ::sync();
            return return_value(vm, context, vut(vint(0), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            return return_value_with_errno(vm, context, vut(vint(0), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },
        {
          "posix.fsync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            int result = ::fsync(fd);
#else
            int result;
            {
              InterruptibleFunctionAround around(context);
              ::fsync(fd);
            }
#endif
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            ::HANDLE handle = reinterpret_cast<::HANDLE>(::_get_osfhandle(fd));
            if(handle == INVALID_HANDLE_VALUE)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            ::BOOL result = ::FlushFileBuffers(handle);
            if(result == FALSE)
              return return_value_with_errno_for_windows(vm, context, vut(vint(-1), v(io_v)));
#else
#error "Unsupported operating system."
#endif
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.fdatasync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            int fd;
            if(!convert_args(args, tofd(fd)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
#if defined(__unix__)
#if !defined(__FreeBSD__)
            int result = ::fdatasync(fd);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
#else
            return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)), ENOSYS);
#endif
#elif defined(_WIN32) || defined(_WIN64)
            ::HANDLE handle = reinterpret_cast<::HANDLE>(::_get_osfhandle(fd));
            if(handle == INVALID_HANDLE_VALUE)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            ::BOOL result = ::FlushFileBuffers(handle);
            if(result == FALSE)
              return return_value_with_errno_for_windows(vm, context, vut(vint(-1), v(io_v)));
#else
#error "Unsupported operating system."
#endif
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.uname", // (io: uio) -> (option tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            struct ::utsname buf;
            int result = ::uname(&buf);
            if(result == -1)
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            return return_value(vm, context, vut(vsome(vutsname(buf)), v(io_v)));
#elif defined(_WIN32) || defined(_WIN64)
            unique_ptr<char []> buf(new char[256]);
            int result = ::gethostname(buf.get(), 255);
            if(result == -1)
              return return_value_with_errno_for_winsock2(vm, context, vut(vnone, v(io_v)));
            buf[255] = 0;
            ::SYSTEM_INFO system_info;
            ::GetSystemInfo(&system_info);
            ::OSVERSIONINFO os_version_info;
            os_version_info.dwOSVersionInfoSize = sizeof(::OSVERSIONINFO);
            ::BOOL result2 = ::GetVersionEx(&os_version_info);
            if(result2 == FALSE)
              return return_value_with_errno_for_windows(vm, context, vut(vnone, v(io_v)));
            string sysname("Windows");
            string nodename(buf.get());
            string machine;
            switch(system_info.wProcessorArchitecture) {
              case PROCESSOR_ARCHITECTURE_INTEL:
                switch(system_info.dwProcessorType) {
                  case PROCESSOR_INTEL_486:
                    machine = "i486";
                    break;
                  case PROCESSOR_INTEL_PENTIUM:
                    machine = "i586";
                    break;
                  default:
                    machine = "i386";
                    break;
                }
                break;
              case PROCESSOR_ARCHITECTURE_MIPS:
                machine = "mips";
                break;
              case PROCESSOR_ARCHITECTURE_ALPHA:
                machine = "alpha";
                break;
              case PROCESSOR_ARCHITECTURE_PPC:
                machine = "ppc";
                break;
              case PROCESSOR_ARCHITECTURE_SHX:
                machine = "shx";
                break;
              case PROCESSOR_ARCHITECTURE_ARM:
                machine = "arm";
                break;
              case PROCESSOR_ARCHITECTURE_IA64:
                machine = "ia64";
                break;
              case PROCESSOR_ARCHITECTURE_ALPHA64:
                machine = "alpha";
                break;
              case PROCESSOR_ARCHITECTURE_AMD64:
                machine = "x86_64";
                break;
              case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
                machine = "i386";
                break;
              default:
                machine = "unknown";
                break;
            }
            ostringstream release_oss;
            release_oss << os_version_info.dwMajorVersion << "." << os_version_info.dwMinorVersion;
            string release(release_oss.str());
            ostringstream version_oss;
            version_oss << os_version_info.dwBuildNumber;
            string version(version_oss.str());
            return return_value_with_errno(vm, context, vut(vsome(vt(vstr(sysname), vstr(nodename), vstr(release), vstr(version), vstr(machine))), v(io_v)), ENOSYS);
#else
#error "Unsupported operating system."
#endif
          }
        },

        //
        // Directory native functions.
        //

        {
          "posix.opendir", // (dir_name: iarray8, io: uio) -> (uoption unative, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, ciarray8, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[1];
            string dir_name;
            if(!convert_args(args, topath(dir_name)))
              return return_value(vm, context, vut(vunone, v(io_v)));
            Directory *dir = open_dir(dir_name.c_str());
            if(dir == nullptr)
              return return_value_with_errno(vm, context, vut(vunone, v(io_v)));
            return return_value(vm, context, vut(vusome(vdir(dir)), v(io_v)));
          }
        },
        {
          "posix.closedir", // (dir: unative, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[1];
            Directory *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vint(-1), v(io_v)));
            Directory **dir_ptr = reinterpret_cast<Directory **>(dir_v.r()->raw().ntvo.bs);
            *dir_ptr = nullptr;
            atomic_thread_fence(memory_order_release);
#if defined(__unix__)
            bool result;
            {
              InterruptibleFunctionAround around(context);
              result = close_dir(dir);
            }
#else
            bool result = close_dir(dir);
#endif
            if(!result)
              return return_value_with_errno(vm, context, vut(vint(-1), v(io_v)));
            return return_value(vm, context, vut(vint(0), v(io_v)));
          }
        },
        {
          "posix.readdir", // (dir: unative, io: uio) -> ((option tuple, unative), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[1];
            Directory *dir;
            unique_ptr<DirectoryEntry, DirectoryEntryDelete> dir_entry(new_directory_entry());
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vut(vnone, v(dir_v)), v(io_v)));
            DirectoryEntry *result = nullptr;
            if(!read_dir(dir, dir_entry.get(), result))
              return return_value_with_errno(vm, context, vut(vut(vnone, v(dir_v)), v(io_v)));
            if(result != nullptr)
              return return_value(vm, context, vut(vut(vsome(vdirentry(dir_entry.get())), v(dir_v)), v(io_v)));
            else
              return return_value(vm, context, vut(vut(vnone, v(dir_v)), v(io_v)));
          }
        },
        {
          "posix.rewinddir", // (dir: unative, io: uio) -> ((int, unative), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[1];
            Directory *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vut(vint(-1), v(dir_v)), v(io_v)));
            rewind_dir(dir);
            return return_value(vm, context, vut(vut(vint(0), v(dir_v)), v(io_v)));
          }
        },
        {
          "posix.seekdir", // (dir: unative, loc: int, io: uio) -> ((int, unative), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cint, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[2];
            Directory *dir;
            long loc;
            if(!convert_args(args, todir(dir), tolarg(loc)))
              return return_value(vm, context, vut(vut(vint(-1), v(dir_v)), v(io_v)));
            seek_dir(dir, loc);
            return return_value(vm, context, vut(vut(vint(0), v(dir_v)), v(io_v)));
          }
        },
        {
          "posix.telldir", // (dir: unative, io: uio) -> ((int, unative), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cdir, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &dir_v = args[0], &io_v = args[1];
            Directory *dir;
            if(!convert_args(args, todir(dir)))
              return return_value(vm, context, vut(vut(vint(-1), v(dir_v)), v(io_v)));
            long loc = tell_dir(dir);
            return return_value(vm, context, vut(vut(vint(loc), v(dir_v)), v(io_v)));
          }
        },
        
        //
        // A host name function.
        //

        {
          "posix.gethostname", // (io: uio) -> (option iarray8, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            int error = check_args(vm, context, args, cuio);
            if(error != letin::ERROR_SUCCESS) return error_return_value(error);
            Value &io_v = args[0];
#if defined(__unix__)
            size_t len = system_host_name_max() + 1;
#elif defined(_WIN32) || defined(_WIN64)
            int len = 256;
#else
#error "Unsupported operating system."
#endif
            unique_ptr<char []> buf(new char[len]);
            int result = ::gethostname(buf.get(), len);
            if(result == -1) 
              return return_value_with_errno(vm, context, vut(vnone, v(io_v)));
            buf[len - 1] = 0;
            return return_value(vm, context, vut(vsome(vcstr(buf.get())), v(io_v)));
          }
        }
      };
#if defined(__unix__)
      fork_handler.mutexes() = { &getgroups_fun_mutex };
#elif defined(_WIN32) || defined(_WIN64)
      fork_handler.mutexes().clear();
#else
#error "Unsupported operating system."
#endif
      return true;
    } catch(...) {
      return false;
    }
  }

  void letin_finalize() {}

  NativeFunctionHandler *letin_new_native_function_handler()
  {
    return new_native_library_without_throwing(native_funs, &fork_handler);
  }
}
