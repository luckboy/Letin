/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <letin/vm.hpp>

using namespace std;
using namespace letin;
using namespace letin::vm;

static vector<NativeFunction> native_funs;

extern "C" {
  bool letin_initialize()
  {
    try {
      native_funs = {

        //
        // An environment native function.
        //

        {
          "posix.environ", // () -> rarray
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // Error native functions.
        //

        {
          "posix.errno", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.set_errno", // (new_errno: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // IO native functions.
        //

        {
          "posix.read", // (fd: int, count: int, io: uio) -> ((int, iarray8) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.write", // (fd: int, buf: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.uread", // (fd: int, buf: uiarray8, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.uwrite", // (fd: int, buf: uiarray8, count: int, io: uio) -> ((int, uiarray8), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.lseek", // (fd: int, offset: int, whence: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.open", // (path_name: iarray8, flags: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.close", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.pipe", // (io: uio) -> ((int, iarray32) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.dup", // (old_fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.dup2", // (old_fd: int, int new_fd, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.select", // (nfds: int, rfds: iarray32, wfds: iarray32, efds: iarray32, timeout: tuple, io: uio) -> ((int, iarray32, iarray32, iarray32) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.poll", // (fds: rarray, int nfds, timeout: int, io: uio) -> ((int, rarray) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_dupfd", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_getfd", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_setfd", // (fd: int, bit: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_getfl", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_setfl", // (fd: int, flags: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_getlk", // (fd: int, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_setlk", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fcntl_setlkw", // (fd: int, lock: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // File system native functions.
        //

        {
          "posix.stat", // (path_name: iarray8, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.lstat", // (path_name: iarray8, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fstat", // (fd: int, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.access", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getcwd", // (io: uio) -> ((int, iarray8) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.chdir", // (dir_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fchdir", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.mkdir", // (dir_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.rmdir", // (dir_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.link", // (old_file_name: iarray8, new_file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.unlink", // (file_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.rename", // (old_path_name: iarray8, new_path_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.symlink", // (old_path_name: iarray8, new_path_name: iarray8, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.readlink", // (path_name: iarray8, io: uio) -> ((int, iarray8) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.chmod", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fchmod", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.chown", // (path_name: iarray8, owner: int, group: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.lchown", // (path_name: iarray8, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fchown", // (fd: int, mode: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.mknod", // (path_name: iarray8, mode: int, dev: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.utimes", // (path_name: iarray8, times: tuple, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.umask", // (mask: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // Process native functions.
        //

        {
          "posix.getpid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getppid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fork", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.waitpid", // (pid: int, options: int) -> ((int, (int, int)) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fork_execve", // (file_name: iarray8, argv: rarray, env: rarray, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.exit", // (status: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.kill", // (pid: int, sig: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getpgid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setpgid", // (pid: int, pgid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getsid", // (pid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }          
        },
        {
          "posix.setsid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }          
        },
        {
          "posix.pause", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.usleep", // (useconds: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.nanosleep", // (req: tuple, io: uio) -> ((int) | (int, tuple), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.times", // (io: uio) -> (tuple, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.nice", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getpriority", // (which: int, who: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setpriority", // (which: int, who: int, prio: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // User native functions.
        //

        {
          "posix.getuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getgid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setgid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.geteuid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.seteuid", // (uid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getegid", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setegid", // (gid: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.getgroups", // (io: uio) -> ((int, iarray32) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.setgroups", // (groups: iarray32, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // A time native function.
        //

        {
          "posix.gettimeofday", // (io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },

        //
        // Terminal native functions.
        //

        {
          "posix.tcgetattr", // (fd: int, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcsetattr", // (fd: int, termios: tuple, io: uio) -> ((int, tuple) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcsendbreak", // (fd: int, duration: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcdrain", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcflush", // (fd: int, queue_selector: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcflow", // (fd: int, action: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcgetpgrp", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.tcsetpgrp", // (fd: int, pgrp: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.isatty", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.ttyname", // (fd: int, io: uio) -> ((int, iarray8) | (int), uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        
        //
        // Other native functions.
        //

        {
          "posix.sync", // (io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fsync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.fdatasync", // (fd: int, io: uio) -> (int, uio)
          [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
            return ReturnValue(0, 0.0, Reference(), ERROR_NO_NATIVE_FUN);
          }
        },
        {
          "posix.uname", // (io: uio) -> ((int, tuple), uio)
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

  NativeFunctionHandler *letin_new_native_function_handler()
  {
    return new_native_library_without_throwing(native_funs);
  }
}
