/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__unix__)
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <sys/time.h>
#endif
#include <io.h>
#include <windows.h>
#else
#error "Unsupported operating system."
#endif
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <signal.h>
#include <time.h>
#include <utility>
#include <vector>
#include "posix.hpp"

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace nlib
  {
    namespace posix
    {
      static long static_system_clk_tck;
#if defined(__unix__)
      static vector<int> system_signals;
      static vector<int> signals;
      static size_t signal_offset;
      static vector<int> system_termios_cc_indexes;
      static vector<int> termios_cc_indexes;
      static size_t termios_cc_index_offset;
      static map<int64_t, ::speed_t> system_speeds;
      static map<::speed_t, int64_t> speeds;
#endif

      static NativeObjectTypeIdentity directory_type_ident;

      static void finalize_directory(const void *ptr)
      { close_dir(*reinterpret_cast<Directory * const *>(ptr)); }

      static uint64_t hash_directory(const void *ptr)
      { return reinterpret_cast<uint64_t>(*reinterpret_cast<Directory * const *>(ptr)); }

      static bool equal_directories(const void *ptr1, const void *ptr2)
      { return *reinterpret_cast<Directory * const *>(ptr1) == *reinterpret_cast<Directory * const *>(ptr2); }

      static NativeObjectFunctions directory_funs(finalize_directory, hash_directory, equal_directories);

      void initialize_consts()
      {
#if defined(__unix__)
        static_system_clk_tck = ::sysconf(_SC_CLK_TCK);
#elif defined(_WIN32) || defined(_WIN64)
        static_system_clk_tck = CLK_TCK;
#else
#error "Unsupported operating system."
#endif
#if defined(__unix__)
        system_signals = {
#ifdef SIGHUP
          SIGHUP,
#else
          0,
#endif
          SIGINT,
#ifdef SIGQUIT
          SIGQUIT,
#else
          0,
#endif
          SIGILL,
#ifdef SIGTRAP
          SIGTRAP,
#else
          0,
#endif
          SIGABRT,
#ifdef SIGBUS
          SIGBUS,
#else
          0,
#endif
          SIGFPE,
#ifdef SIGKILL
          SIGKILL,
#else
          0,
#endif
#ifdef SIGUSR1
          SIGUSR1,
#else
          0,
#endif
          SIGSEGV,
#ifdef SIGUSR2
          SIGUSR2,
#else
          0,
#endif
#ifdef SIGPIPE
          SIGPIPE,
#else
          0,
#endif
#ifdef SIGALRM
          SIGALRM,
#else
          0,
#endif
          SIGTERM,
#ifdef SIGSTKFLT
          SIGSTKFLT,
#else
          0,
#endif
#ifdef SIGCHLD
          SIGCHLD,
#else
          0,
#endif
#ifdef SIGCONT
          SIGCONT,
#else
          0,
#endif
#ifdef SIGSTOP
          SIGSTOP,
#else
          0,
#endif
#ifdef SIGTSTP
          SIGTSTP,
#else
          0,
#endif
#ifdef SIGTTIN
          SIGTTIN,
#else
          0,
#endif
#ifdef SIGTTOU
          SIGTTOU,
#else
          0,
#endif
#ifdef SIGURG
          SIGURG,
#else
          0,
#endif
#ifdef SIGXCPU
          SIGXCPU,
#else
          0,
#endif
#ifdef SIGVTALRM
          SIGVTALRM,
#else
          0,
#endif
#ifdef SIGPROF
          SIGPROF,
#else
          0,
#endif
#ifdef SIGWINCH
          SIGWINCH,
#else
          0,
#endif
#ifdef SIGPOLL
          SIGPOLL,
#else
          0,
#endif
#ifdef SIGIO
          SIGIO,
#else
          0,
#endif
#ifdef SIGPWR
          SIGPWR,
#else
          0,
#endif
#ifdef SIGSYS
          SIGSYS
#else
          0,
#endif
        };
        int min_system_signal = numeric_limits<int>::max();
        int max_system_signal = numeric_limits<int>::min();
        for(auto system_signal : system_signals) {
          min_system_signal = min(min_system_signal, system_signal);
          max_system_signal = max(max_system_signal, system_signal);
        }
        signals.resize(max_system_signal - min_system_signal + 1);
        signal_offset = min_system_signal;
        fill(signals.begin(), signals.end(), 0);
        for(int i = 0; i < static_cast<int>(system_signals.size()); i++) {
          if(system_signals[i] != 0) signals[system_signals[i] - min_system_signal] = i;
        }
        system_termios_cc_indexes = {
          VEOF,
          VEOL,
          VERASE,
          VINTR,
          VKILL,
          VMIN,
          VQUIT,
          VSTART,
          VSTOP,
          VSUSP,
          VTIME,
#ifdef VEOL2
          VEOL2,
#else
          -1,
#endif
#ifdef VERASE2
          VERASE2,
#else
          -1,
#endif
#ifdef VWERASE
          VWERASE,
#else
          -1,
#endif
#ifdef VSWTC
          VSWTC,
#else
          -1,
#endif
#ifdef VLNEXT
          VLNEXT,
#else
          -1,
#endif
#ifdef VDISCARD
          VDISCARD,
#else
          -1,
#endif
#ifdef VREPRINT
          VREPRINT
#else
          -1,
#endif
#ifdef VDSUSP
          VDSUSP,
#else
          -1,
#endif
#ifdef VSTATUS
          VSTATUS
#else
          -1
#endif
        };
        system_speeds = {
          { 0,          B0 },
          { 50,         B50 },
          { 75,         B75 },
          { 110,        B110 },
          { 134,        B134 },
          { 150,        B150 },
          { 200,        B200 },
          { 300,        B300 },
          { 600,        B600 },
          { 1200,       B1200 },
          { 1800,       B1800 },
          { 2400,       B2400 },
          { 4800,       B4800 },
#ifdef B7500
          { 7500,       B7500 },
#endif
          { 9600,       B9600 },
#ifdef B14400
          { 14400,      B14400 },
#endif
          { 19200,      B19200 },
#ifdef B28800
          { 28800,      B28800 },
#endif
          { 38400,      B38400 },
#ifdef B57600
          { 57600,      B57600 },
#endif
#ifdef B76800
          { 76800,      B76800 },
#endif
#ifdef B115200
          { 115200,     B115200 },
#endif
#ifdef B230400
          { 230400,     B230400 },
#endif
#ifdef B460800
          { 460800,     B460800 },
#endif
#ifdef B500000
          { 500000,     B500000 },
#endif
#ifdef B576000
          { 576000,     B576000 },
#endif
#ifdef B921600
          { 921600,     B921600 },
#endif
#ifdef B1000000
          { 1000000,    B1000000 },
#endif
#ifdef B1152000
          { 1152000,    B1152000 },
#endif
#ifdef B1500000
          { 1500000,    B1500000 },
#endif
#ifdef B2000000
          { 2000000,    B2000000 },
#endif
#ifdef B2500000
          { 2500000,    B2500000 },
#endif
#ifdef B3000000
          { 3000000,    B3000000 },
#endif
#ifdef B3500000
          { 3500000,    B3500000 },
#endif
#ifdef B4000000
          { 4000000,    B4000000 },
#endif
          { -1, 0 }
        };
        for(auto pair : system_speeds) {
          if(pair.first != -1) speeds.insert(make_pair(pair.second, pair.first));
        }
#endif
      }

      long int system_clk_tck() { return static_system_clk_tck; }

      int64_t system_whence_to_whence(int system_whence)
      {
        switch(system_whence) {
          case SEEK_SET:
            return 0;
          case SEEK_CUR:
            return 1;
          case SEEK_END:
            return 2;
          default:
            return -1;
        }
      }

      bool whence_to_system_whence(int64_t whence, int &system_whence)
      {
        switch(whence) {
          case 0:
            system_whence = SEEK_SET;
            return true;
          case 1:
            system_whence = SEEK_CUR;
            return true;
          case 2:
            system_whence = SEEK_END;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      int64_t system_flags_to_flags(int system_flags)
      {
        int flags = 0;
        switch(system_flags & O_ACCMODE) {
          case O_RDONLY:
            flags = 0;
            break;
          case O_RDWR:
            flags = 1;
            break;
          case O_WRONLY:
            flags = 2;
            break;
          default:
            return -1;
        }
        if((system_flags & O_CREAT) != 0) flags |= 00004;
        if((system_flags & O_EXCL) != 0) flags |= 00010;
#ifdef O_NOCTTY
        if((system_flags & O_NOCTTY) != 0) flags |= 00020;
#endif
        if((system_flags & O_TRUNC) != 0) flags |= 00040;
        if((system_flags & O_APPEND) != 0) flags |= 00100;
#ifdef O_DSYNC
        if((system_flags & O_DSYNC) != 0) flags |= 00200;
#endif
#ifdef O_NONBLOCK
        if((system_flags & O_NONBLOCK) != 0) flags |= 00400;
#endif
#ifdef O_RSYNC
        if((system_flags & O_RSYNC) != 0) flags |= 01000;
#endif
#ifdef O_SYNC
        if((system_flags & O_SYNC) != 0) flags |= 02000;
#endif
        return system_flags;
      }

      bool flags_to_system_flags(int64_t flags, int &system_flags)
      {
        if((flags & ~static_cast<int64_t>(03777)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        system_flags = 0;
        switch(flags & 3) {
          case 0:
            system_flags = O_RDONLY;
            break;
          case 1:
            system_flags = O_RDWR;
            break;
          case 2:
            system_flags = O_WRONLY;
            break;
          default:
            letin_errno() = EINVAL;
            return false;
        }
        if((flags & 00004) != 0) system_flags |= O_CREAT;
        if((flags & 00010) != 0) system_flags |= O_EXCL;
#ifdef O_NOCTTY
        if((flags & 00020) != 0) system_flags |= O_NOCTTY;
#else
        if((flags & 00020) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
        if((flags & 00040) != 0) system_flags |= O_TRUNC;
        if((flags & 00100) != 0) system_flags |= O_APPEND;
#ifdef O_DSYNC
        if((flags & 00200) != 0) system_flags |= O_DSYNC;
#else
        if((flags & 00200) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
#ifdef O_NONBLOCK
        if((flags & 00400) != 0) system_flags |= O_NONBLOCK;
#else
        if((flags & 00400) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
#ifdef O_RSYNC
        if((flags & 01000) != 0) system_flags |= O_RSYNC;
#else
        if((flags & 01000) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
#ifdef O_SYNC
        if((flags & 02000) != 0) system_flags |= O_SYNC;
#else
        if((flags & 02000) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
#endif
        return true;
      }

      bool fd_to_system_fd(int64_t fd, int &system_fd)
      {
        if((fd < 0) || (fd > numeric_limits<int>::max())) {
          letin_errno() = EBADF;
          return false;
        }
        system_fd = fd;
        return true;
      }

      bool count_to_system_count(int64_t count, size_t &system_count)
      {
        if((count < 0) || (count > numeric_limits<::ssize_t>::max())) {
          letin_errno() = EINVAL;
          return false;
        }
        system_count = count;
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

      bool long_arg_to_system_long_arg(int64_t arg, long &system_arg)
      {
        if((arg < numeric_limits<long>::min()) || (arg > numeric_limits<long>::max())) {
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

      void system_path_name_to_path_name(char *path_name)
      {
#if defined(_WIN32) || defined(_WIN64)
        for(size_t i = 0; path_name[i] != 0; i++) {
          if(path_name[i] == '\\') path_name[i] = '/';
        }
#endif
      }

      bool object_to_system_path_name(const Object &object, string &path_name)
      {
        path_name.assign(reinterpret_cast<const char *>(object.raw().is8), object.length());
#if defined(_WIN32) || defined(_WIN64)
        replace(path_name.begin(), path_name.end(), '/', '\\'); 
#endif
        return true;
      }

      bool access_mode_to_system_access_mode(int64_t mode, int &system_mode)
      {
        if((mode & ~static_cast<int64_t>(7)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        if(mode == 0) {
          system_mode = F_OK;
          return true;
        } else {
          system_mode  = 0;
          if((mode & 1) != 0) system_mode |= X_OK;
          if((mode & 2) != 0) system_mode |= W_OK;
          if((mode & 4) != 0) system_mode |= R_OK;
#if defined(_WIN32) || defined(_WIN64)
          system_mode &= ~X_OK;
#endif
          return true;
        }
      }

#if defined(__unix__)
      bool nfds_to_system_nfds(int64_t nfds, int &system_nfds)
      {
        if((nfds < 0) || (nfds > FD_SETSIZE)) {
          letin_errno() = EBADF;
          return false;
        }
        system_nfds = nfds;
        return true;
      }
#endif


      bool wait_options_to_system_wait_options(int64_t options, int &system_options)
      {
#if defined(__unix__)
        if((options & ~static_cast<int64_t>(3)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        system_options = 0;
        if((options & 1) != 0) system_options |= WNOHANG;
        if((options & 2) != 0) system_options |= WUNTRACED;
        return true;
#elif defined(_WIN32) || defined(_WIN64)
        if((options & ~static_cast<int64_t>(1)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        system_options = options;
        return true;
#else
#error "Unsupported operating system."
#endif
      }

      bool object_to_system_argv(const Object &object, Argv &argv)
      {
        argv.set_ptr(new char *[object.length() + 1]);
        for(size_t i = 0; i < object.length(); i++) {
          argv[i] = new char[object.elem(i).r()->length() + 1];
          copy_n(reinterpret_cast<char *>(object.elem(i).r()->raw().is8), object.elem(i).r()->length(), argv[i]);
          argv[i][object.elem(i).r()->length()] = 0;
        }
        argv[object.length()] = nullptr;
        return true;
      }

      int check_argv(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_rarray()) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < object.length(); i++) {
          if(!object.elem(i).r()->is_iarray8()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;
      }

#if defined(__unix__)
      int64_t system_signal_to_signal(int system_signal)
      {
        if(system_signal == 0) {
          return 0;
        } else {
          if(signals[system_signal - signal_offset] == 0) return -1;
          return signals[system_signal - signal_offset] + 1;
        }
      }

      bool signal_to_system_signal(int64_t signal, int &system_signal)
      {
        if((signal < 0) || (signal > static_cast<int>(system_signals.size()))) {
          letin_errno() = EINVAL;
          return false;
        }
        if(signal == 0) {
          system_signal = 0;
          return true;
        } else {
          if(system_signals[signal - 1] == 0) {
            letin_errno() = EINVAL;
            return false;
          }
          system_signal = system_signals[signal - 1];
          return true;
        }
      }
#endif

      bool kill_signal_to_system_kill_signal(int64_t signal, int &system_signal)
      {
        if(signal != 0 && signal != 9){
          letin_errno() = EINVAL;
          return false;
        }
        system_signal = signal;
        return true;
      }

#if defined(__unix__)
      bool which_to_system_which(int64_t which, int &system_which)
      {
        switch(which) {
          case 0:
            system_which = PRIO_PROCESS;
            return true;
          case 1:
            system_which = PRIO_PGRP;
            return true;
          case 2:
            system_which = PRIO_USER;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }
      
      bool optional_actions_to_system_optional_actions(int64_t optional_actions, int &system_optional_actions)
      {
        switch(optional_actions) {
          case 0:
            system_optional_actions = TCSANOW;
            return true;
          case 1:
            system_optional_actions = TCSADRAIN;
            return true;
          case 2:
            system_optional_actions = TCSAFLUSH;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      bool queue_selector_to_system_queue_selector(int64_t queue_selector, int &system_queue_selector)
      {
        switch(queue_selector) {
          case 0:
            system_queue_selector = TCIFLUSH;
            return true;
          case 1:
            system_queue_selector = TCOFLUSH;
            return true;
          case 2:
            system_queue_selector = TCIOFLUSH;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      bool tcflow_action_to_system_tcflow_action(int64_t action, int &system_action)
      {
        switch(action) {
          case 0:
            system_action = TCOOFF;
            return true;
          case 1:
            system_action = TCOON;
            return true;
          case 2:
            system_action = TCIOFF;
            return true;
          case 3:
            system_action = TCION;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
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

      int64_t system_flock_type_to_flock_type(short system_type)
      {
        switch(system_type) {
          case F_RDLCK:
            return 0;
          case F_WRLCK:
            return 1;
          case F_UNLCK:
            return 2;
          default:
            return -1;
        }
      }

      bool flock_type_to_system_flock_type(int64_t type, int &system_type)
      {
        switch(type) {
          case 0:
            system_type = F_RDLCK;
            return true;
          case 1:
            system_type = F_WRLCK;
            return true;
          case 2:
            system_type = F_UNLCK;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
      }

      int64_t system_termios_iflag_to_termios_iflag(::tcflag_t system_iflag)
      {
        int64_t iflag = 0;
        if((system_iflag & BRKINT) != 0) iflag |= 1 << 0;
        if((system_iflag & ICRNL) != 0) iflag |= 1 << 1;
        if((system_iflag & IGNBRK) != 0) iflag |= 1 << 2;
        if((system_iflag & IGNCR) != 0) iflag |= 1 << 3;
        if((system_iflag & IGNPAR) != 0) iflag |= 1 << 4;
        if((system_iflag & INLCR) != 0) iflag |= 1 << 5;
        if((system_iflag & INPCK) != 0) iflag |= 1 << 6;
        if((system_iflag & ISTRIP) != 0) iflag |= 1 << 7;
        if((system_iflag & IUCLC) != 0) iflag |= 1 << 8;
        if((system_iflag & IXANY) != 0) iflag |= 1 << 9;
        if((system_iflag & IXOFF) != 0) iflag |= 1 << 10;
        if((system_iflag & IXON) != 0) iflag |= 1 << 11;
        if((system_iflag & PARMRK) != 0) iflag |= 1 << 12;
        return iflag;
      }

      bool termios_iflag_to_system_termios_iflag(int64_t iflag, ::tcflag_t &system_iflag)
      {
        if((iflag & ~static_cast<int64_t>((1 << 13) - 1)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        system_iflag = 0;
        if((iflag & (1 << 0)) != 0) system_iflag |= BRKINT;
        if((iflag & (1 << 1)) != 0) system_iflag |= ICRNL;
        if((iflag & (1 << 2)) != 0) system_iflag |= IGNBRK;
        if((iflag & (1 << 3)) != 0) system_iflag |= IGNCR;
        if((iflag & (1 << 4)) != 0) system_iflag |= IGNPAR;
        if((iflag & (1 << 5)) != 0) system_iflag |= INLCR;
        if((iflag & (1 << 6)) != 0) system_iflag |= INPCK;
        if((iflag & (1 << 7)) != 0) system_iflag |= ISTRIP;
        if((iflag & (1 << 8)) != 0) system_iflag |= IUCLC;
        if((iflag & (1 << 9)) != 0) system_iflag |= IXANY;
        if((iflag & (1 << 10)) != 0) system_iflag |= IXOFF;
        if((iflag & (1 << 11)) != 0) system_iflag |= IXON;
        if((iflag & (1 << 12)) != 0) system_iflag |= PARMRK;
        return true;
      }

      int64_t system_termios_oflag_to_termios_oflag(::tcflag_t system_oflag)
      {
        int64_t oflag = 0;
        if((system_oflag & OPOST) != 0) oflag |= 1 << 0;
        if((system_oflag & OLCUC) != 0) oflag |= 1 << 1;
        if((system_oflag & ONLCR) != 0) oflag |= 1 << 2;
        if((system_oflag & OCRNL) != 0) oflag |= 1 << 3;
        if((system_oflag & ONOCR) != 0) oflag |= 1 << 4;
        if((system_oflag & ONLRET) != 0) oflag |= 1 << 5;
        if((system_oflag & OFILL) != 0) oflag |= 1 << 6;
        switch(system_oflag & NLDLY) {
          case NL0:
            break;
          case NL1:
            oflag |= 1 << 7;
            break;
          default:
            return -1;
        }
        switch(system_oflag & CRDLY) {
          case CR0:
            break;
          case CR1:
            oflag |= 1 << 8;
            break;
          case CR2:
            oflag |= 2 << 8;
            break;
          case CR3:
            oflag |= 3 << 8;
            break;
          default:
            return -1;
        }
        switch(system_oflag & TABDLY) {
          case TAB0:
            break;
          case TAB1:
            oflag |= 1 << 10;
            break;
          case TAB2:
            oflag |= 2 << 10;
            break;
          case TAB3:
            oflag |= 3 << 10;
            break;
          default:
            return -1;
        }
        switch(system_oflag & BSDLY) {
          case BS0:
            break;
          case BS1:
            oflag |= 1 << 12;
            break;
          default:
            return -1;
        }
        switch(system_oflag & VTDLY) {
          case VT0:
            break;
          case VT1:
            oflag |= 1 << 13;
            break;
          default:
            return -1;
        }
        switch(system_oflag & FFDLY) {
          case FF0:
            break;
          case FF1:
            oflag |= 1 << 14;
            break;
          default:
            return -1;
        }
        return oflag;
      }

      bool termios_oflag_to_system_termios_oflag(int64_t oflag, ::tcflag_t &system_oflag)
      {
        if((oflag & ~static_cast<int64_t>((1 << 15) - 1)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        if((system_oflag & (1 << 0)) != 0) system_oflag |= OPOST;
        if((system_oflag & (1 << 1)) != 0) system_oflag |= OLCUC;
        if((system_oflag & (1 << 2)) != 0) system_oflag |= ONLCR;
        if((system_oflag & (1 << 3)) != 0) system_oflag |= OCRNL;
        if((system_oflag & (1 << 4)) != 0) system_oflag |= ONOCR;
        if((system_oflag & (1 << 5)) != 0) system_oflag |= ONLRET;
        if((system_oflag & (1 << 6)) != 0) system_oflag |= OFILL;
        switch(system_oflag & (1 << 7)) {
          case 0 << 7:
            system_oflag |= NL0;
            break;
          case 1 << 7:
            system_oflag |= NL1;
            break;
        }
        switch(system_oflag & (3 << 8)) {
          case 0 << 8:
            system_oflag |= CR0;
            break;
          case 1 << 8:
            system_oflag |= CR1;
            break;
          case 2 << 8:
            system_oflag |= CR2;
            break;
          case 3 << 8:
            system_oflag |= CR3;
            break;
        }
        switch(system_oflag & (3 << 10)) {
          case 0 << 10:
            system_oflag |= TAB0;
            break;
          case 1 << 10:
            system_oflag |= TAB1;
            break;
          case 2 << 10:
            system_oflag |= TAB2;
            break;
          case 3 << 10:
            system_oflag |= TAB3;
            break;
        }
        switch(system_oflag & (1 << 12)) {
          case 0 << 12:
            system_oflag |= BS0;
            break;
          case 1 << 12:
            system_oflag |= BS1;
            break;
        }
        switch(system_oflag & (1 << 13)) {
          case 0 << 13:
            system_oflag |= VT0;
            break;
          case 1 << 13:
            system_oflag |= VT1;
            break;
        }
        switch(system_oflag & (1 << 14)) {
          case 0 << 14:
            system_oflag |= FF0;
            break;
          case 1 << 14:
            system_oflag |= FF1;
            break;
        }
        return true;
      }

      int64_t system_termios_cflag_to_termios_cflag(::tcflag_t system_cflag)
      {
        int64_t cflag = 0;
        switch(system_cflag & CSIZE) {
          case CS5:
            break;
          case CS6:
            cflag |= 1;
            break;
          case CS7:
            cflag |= 2;
            break;
          case CS8:
            cflag |= 3;
            break;
          default:
            return -1;
        }
        if((system_cflag & CSTOPB) != 0) cflag |= 1 << 2;
        if((system_cflag & CREAD) != 0) cflag |= 1 << 3;
        if((system_cflag & PARENB) != 0) cflag |= 1 << 4;
        if((system_cflag & PARODD) != 0) cflag |= 1 << 5;
        if((system_cflag & HUPCL) != 0) cflag |= 1 << 6;
        if((system_cflag & CLOCAL) != 0) cflag |= 1 << 7;
        return cflag;
      }

      bool termios_cflag_to_system_termios_cflag(int64_t cflag, ::tcflag_t &system_cflag)
      {
        if((cflag & ~static_cast<int64_t>((1 << 8) - 1)) != 0) {
          letin_errno() = EINVAL;
          return false;
        }
        system_cflag = 0;
        switch(cflag & 3) {
          case 0:
            system_cflag |= CS5;
            break;
          case 1:
            system_cflag |= CS6;
            break;
          case 2:
            system_cflag |= CS7;
            break;
          case 3:
            system_cflag |= CS8;
            break;
        }
        if((cflag & (1 << 2)) != 0) system_cflag |= CSTOPB;
        if((cflag & (1 << 3)) != 0) system_cflag |= CREAD;
        if((cflag & (1 << 4)) != 0) system_cflag |= PARENB;
        if((cflag & (1 << 5)) != 0) system_cflag |= PARODD;
        if((cflag & (1 << 6)) != 0) system_cflag |= HUPCL;
        if((cflag & (1 << 7)) != 0) system_cflag |= CLOCAL;
        return true;
      }

      int64_t system_termios_lflag_to_termios_lflag(::tcflag_t system_lflag)
      {
        int64_t lflag = 0;
        if((system_lflag & ECHO) != 0) lflag |= 1 << 0;
        if((system_lflag & ECHOE) != 0) lflag |= 1 << 1;
        if((system_lflag & ECHOK) != 0) lflag |= 1 << 2;
        if((system_lflag & ECHONL) != 0) lflag |= 1 << 3;
        if((system_lflag & ICANON) != 0) lflag |= 1 << 4;
        if((system_lflag & IEXTEN) != 0) lflag |= 1 << 5;
        if((system_lflag & NOFLSH) != 0) lflag |= 1 << 6;
        if((system_lflag & TOSTOP) != 0) lflag |= 1 << 7;
        if((system_lflag & XCASE) != 0) lflag |= 1 << 8;
        return lflag;
      }

      bool termios_lflag_to_system_termios_lflag(int64_t lflag, ::tcflag_t &system_lflag)
      {
        if((lflag & ~static_cast<int64_t>((1 << 9) - 1))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_lflag = 0;
        if((system_lflag & (1 << 0)) != 0) lflag |= ECHO;
        if((system_lflag & (1 << 1)) != 0) lflag |= ECHOE;
        if((system_lflag & (1 << 2)) != 0) lflag |= ECHOK;
        if((system_lflag & (1 << 3)) != 0) lflag |= ECHONL;
        if((system_lflag & (1 << 4)) != 0) lflag |= ICANON;
        if((system_lflag & (1 << 5)) != 0) lflag |= IEXTEN;
        if((system_lflag & (1 << 6)) != 0) lflag |= NOFLSH;
        if((system_lflag & (1 << 7)) != 0) lflag |= TOSTOP;
        if((system_lflag & (1 << 8)) != 0) lflag |= XCASE;
        return true;
      }

      Object *new_wait_status(VirtualMachine *vm, ThreadContext *context, int status)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(object == nullptr) return nullptr;
        if(WIFEXITED(status) != 0) {
          object->set_elem(0, Value(1));
          object->set_elem(1, Value(WEXITSTATUS(status)));
        } else if(WIFSIGNALED(status) != 0) {
          object->set_elem(0, Value(2));
          object->set_elem(1, Value(system_signal_to_signal(WTERMSIG(status))));
        } else if(WIFSTOPPED(status) != 0) {
          object->set_elem(0, Value(3));
          object->set_elem(1, Value(system_signal_to_signal(WSTOPSIG(status))));
        } else {
#ifdef WCOREDUMP
          if(WCOREDUMP(status) != 0)
            object->set_elem(0, Value(4));
          else
            object->set_elem(0, Value(0));
#else
          object->set_elem(0, Value(0));
#endif
          object->set_elem(1, Value(0));
        }
        return object;
      }
#endif

      int64_t system_mode_to_mode(::mode_t system_mode)
      {
#if defined(__unix__)
#if S_IXOTH == 00001 && S_IWOTH == 00002 && S_IROTH == 00004 && \
    S_IXGRP == 00010 && S_IWGRP == 00020 && S_IRGRP == 00040 && \
    S_IXUSR == 00100 && S_IWUSR == 00200 && S_IRUSR == 00400 && \
    S_ISVTX == 01000 && S_ISGID == 02000 && S_ISUID == 04000
        return system_mode & 07777;
#else
        int mode = 0;
        if((mode & S_IXOTH) != 0) mode |= 00001;
        if((mode & S_IWOTH) != 0) mode |= 00002;
        if((mode & S_IROTH) != 0) mode |= 00004;
        if((mode & S_IXGRP) != 0) mode |= 00010;
        if((mode & S_IWGRP) != 0) mode |= 00020;
        if((mode & S_IRGRP) != 0) mode |= 00040;
        if((mode & S_IXUSR) != 0) mode |= 00100;
        if((mode & S_IWUSR) != 0) mode |= 00200;
        if((mode & S_IRUSR) != 0) mode |= 00400;
        if((mode & S_ISVTX) != 0) mode |= 01000;
        if((mode & S_ISGID) != 0) mode |= 02000;
        if((mode & S_ISUID) != 0) mode |= 04000;
        return mode;
#endif
#elif defined(_WIN32) || defined(_WIN64)
#if _S_IXUSR == 00100 && _S_IWUSR == 00200 && _S_IRUSR == 00400
        return system_mode & 00700;
#else
        mode_t mode;
        if((mode & _S_IXUSR) != 0) mode |= 00100;
        if((mode & _S_IWUSR) != 0) mode |= 00200;
        if((mode & _S_IRUSR) != 0) mode |= 00400;
        return mode;
#endif
#else
#error "Unsupported operating system."
#endif
      }

      bool mode_to_system_mode(int64_t mode, ::mode_t &system_mode)
      {
        if((mode & ~static_cast<int64_t>(07777))) {
          letin_errno() = EINVAL;
          return false;
        }
#if defined(__unix__)
#if S_IXOTH == 00001 && S_IWOTH == 00002 && S_IROTH == 00004 && \
    S_IXGRP == 00010 && S_IWGRP == 00020 && S_IRGRP == 00040 && \
    S_IXUSR == 00100 && S_IWUSR == 00200 && S_IRUSR == 00400 && \
    S_ISVTX == 01000 && S_ISGID == 02000 && S_ISUID == 04000
        system_mode = mode;
#else
        int system_mode = 0;
        if((mode & 00001) != 0) system_mode |= S_IXOTH;
        if((mode & 00002) != 0) system_mode |= S_IWOTH;
        if((mode & 00004) != 0) system_mode |= S_IROTH;
        if((mode & 00010) != 0) system_mode |= S_IXGRP;
        if((mode & 00020) != 0) system_mode |= S_IWGRP;
        if((mode & 00040) != 0) system_mode |= S_IRGRP;
        if((mode & 00100) != 0) system_mode |= S_IXUSR;
        if((mode & 00200) != 0) system_mode |= S_IWUSR;
        if((mode & 00400) != 0) system_mode |= S_IRUSR;
        if((mode & 01000) != 0) system_mode |= S_ISVTX;
        if((mode & 02000) != 0) system_mode |= S_ISGID;
        if((mode & 04000) != 0) system_mode |= S_ISUID;
#endif
#elif defined(_WIN32) || defined(_WIN64)
#if _S_IXUSR == 00100 && _S_IWUSR == 00200 && _S_IRUSR == 00400
        system_mode = mode;
#else
        int system_mode = 0;
        if((mode & 00100) != 0) system_mode |= _S_IXUSR;
        if((mode & 00200) != 0) system_mode |= _S_IWUSR;
        if((mode & 00400) != 0) system_mode |= _S_IRUSR;
#endif
#else
#error "Unsupported operating system."
#endif
        return true;
      }

      int64_t system_stat_mode_to_stat_mode(::mode_t system_mode)
      {
        int mode = system_mode_to_mode(system_mode);
#if defined(__unix__)
        if(S_ISBLK(system_mode) != 0) return mode | 0010000;
        if(S_ISCHR(system_mode) != 0) return mode | 0020000;
        if(S_ISFIFO(system_mode) != 0) return mode | 0030000;
        if(S_ISREG(system_mode) != 0) return mode | 0040000;
        if(S_ISDIR(system_mode) != 0) return mode | 0050000;
        if(S_ISLNK(system_mode) != 0) return mode | 0060000;
#ifdef S_ISSOCK
        if(S_ISSOCK(system_mode) != 0) return mode | 0070000;
#endif
#ifdef S_ISWHT
        if(S_ISWHT(system_mode) != 0) return mode | 0100000;
#endif
#elif defined(_WIN32) || defined(_WIN64)
        if(_S_ISBLK(system_mode) != 0) return mode | 0010000;
        if(_S_ISCHR(system_mode) != 0) return mode | 0020000;
        if(_S_ISFIFO(system_mode) != 0) return mode | 0030000;
        if(_S_ISREG(system_mode) != 0) return mode | 0040000;
        if(_S_ISDIR(system_mode) != 0) return mode | 0050000;
#else
#error "Unsupported operating system."
#endif
        return mode | 0110000;
      }

      bool stat_mode_to_system_stat_mode(int64_t mode, ::mode_t &system_mode)
      {
        if(!mode_to_system_mode(mode & 07777, system_mode)) return false;
#if defined(__unix__)
        switch(mode & ~static_cast<int64_t>(07777)) {
          case 0010000:
            system_mode |= S_IFBLK;
            return true;
          case 0020000:
            system_mode |= S_IFCHR;
            return true;
          case 0030000:
            system_mode |= S_IFIFO;
            return true;
          case 0040000:
            system_mode |= S_IFREG;
            return true;
          case 0050000:
            system_mode |= S_IFDIR;
            return true;
          case 0060000:
            system_mode |= S_IFLNK;
            return true;
#ifdef S_IFSOCK
          case 0070000:
            system_mode |= S_IFSOCK;
            return true;
#endif
#ifdef S_IFWHT
          case 0100000:
            system_mode |= S_IFWHT;
            return true;
#endif
          default:
            letin_errno() = EINVAL;
            return false;
        }
#elif defined(_WIN32) || defined(_WIN64)
        switch(mode & ~static_cast<int64_t>(07777)) {
          case 0010000:
            system_mode |= _S_IFBLK;
            return true;
          case 0020000:
            system_mode |= _S_IFCHR;
            return true;
          case 0030000:
            system_mode |= _S_IFIFO;
            return true;
          case 0040000:
            system_mode |= _S_IFREG;
            return true;
          case 0050000:
            system_mode |= _S_IFDIR;
            return true;
          default:
            letin_errno() = EINVAL;
            return false;
        }
#else
#error "Unsupported operating system."
#endif
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

      bool off_to_system_off(int64_t offset, ::off_t &system_offset)
      {
        if(sizeof(int64_t) > sizeof(::off_t) &&
            ((offset < static_cast<int64_t>(numeric_limits<::off_t>::min())) ||
             (offset > static_cast<int64_t>(numeric_limits<::off_t>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_offset = offset;
        return true;
      }

#ifdef HAVE_OFF64_T
      bool off64_to_system_off64(int64_t offset, ::off64_t &system_offset)
      {
        system_offset = offset;
        return true;
      }
#endif

#if defined(__unix__)
      bool uid_to_system_uid(int64_t uid, ::uid_t &system_uid)
      {
        if(sizeof(int64_t) > sizeof(::uid_t) &&
            ((uid < static_cast<int64_t>(numeric_limits<::uid_t>::min())) ||
             (uid > static_cast<int64_t>(numeric_limits<::uid_t>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_uid = uid;
        return true;
      }

      bool gid_to_system_gid(int64_t gid, ::gid_t &system_gid)
      {
        if(sizeof(int64_t) > sizeof(::gid_t) &&
            ((gid < static_cast<int64_t>(numeric_limits<::gid_t>::min())) ||
             (gid > static_cast<int64_t>(numeric_limits<::gid_t>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_gid = gid;
        return true;
      }

      bool object_to_system_gids(const Object &object, Array<::gid_t> &gids)
      {
        gids.set_ptr_and_length(new ::gid_t[object.length()], object.length());
        for(size_t i = 0; i < gids.length(); i++) {
          if(!gid_to_system_gid(object.elem(i).i(), gids[i])) return false;
        }
        return true;
      }
#endif

      bool dev_to_system_dev(int64_t dev, ::dev_t &system_dev)
      {
        if(sizeof(int64_t) > sizeof(::dev_t) &&
            ((dev < static_cast<int64_t>(numeric_limits<::dev_t>::min())) ||
             (dev > static_cast<int64_t>(numeric_limits<::dev_t>::max())))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_dev = dev;
        return true;
      }

      bool pid_to_system_pid(int64_t pid, Pid &system_pid)
      {
        if(sizeof(int64_t) > sizeof(Pid) &&
            ((pid < static_cast<int64_t>(numeric_limits<Pid>::min())) ||
             (pid > static_cast<int64_t>(numeric_limits<Pid>::max())))) {
          letin_errno() = ESRCH;
          return false;
        }
        system_pid = pid;
        return true;
      }

      bool wait_pid_to_system_wait_pid(int64_t pid, Pid &system_pid)
      {
        if(!pid_to_system_pid(pid, system_pid)) {
          letin_errno() = ECHILD;
          return false;
        }
        return true;
      }

      bool useconds_to_system_useconds(int64_t useconds, Useconds &system_useconds)
      {
        if(sizeof(int64_t) > sizeof(Useconds) &&
            ((useconds < 0) || (useconds > numeric_limits<Useconds>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        system_useconds = useconds;
        return true;        
      }

#if defined(__unix__)
      int64_t system_speed_to_speed(::speed_t system_speed)
      {
        auto iter = speeds.find(system_speed);
        if(iter != speeds.end()) return -1;
        return iter->second;
      }

      bool speed_to_system_speed(int64_t speed, ::speed_t &system_speed)
      {
        if(speed == -1) {
          letin_errno() = EINVAL;
          return false;
        }
        auto iter = system_speeds.find(system_speed);
        if(iter != system_speeds.end()) {
          letin_errno() = EINVAL;
          return false;
        }
        system_speed = iter->second;
        return true;
      }
#endif

      // Functions for fd_set.

#if defined(__unix__)
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
#endif

      // Functions for struct timeval.

      //
      // type timeval = (
      //     int,       // tv_sec
      //     int        // tv_usec
      //   )
      //

#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
      Object *new_timeval(VirtualMachine *vm, ThreadContext *context, const struct ::timeval &time_value)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(time_value.tv_sec)));
        object->set_elem(1, Value(static_cast<int64_t>(time_value.tv_usec)));
        return object;
      }
#endif

      bool object_to_system_timeval(const Object &object, struct ::timeval &time_value)
      {
        if((object.elem(0).i() < static_cast<int64_t>(numeric_limits<time_t>::min())) ||
            (object.elem(0).i() > static_cast<int64_t>(numeric_limits<time_t>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        if((object.elem(1).i() < static_cast<int64_t>(numeric_limits<long>::min())) || 
            (object.elem(1).i() > static_cast<int64_t>(numeric_limits<long>::max()))) {
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
          if(!fd_to_system_fd(fd_r->elem(0).i(), fds[i].fd)) return false;
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

      // Functions for struct flock and struct flock64.
      
      //
      // type flock = (
      //     int,       // l_type
      //     int,       // l_whence
      //     int,       // l_start
      //     int,       // l_len
      //     int        // l_len
      //   )
      //

#if defined(__unix__)
      Object *new_flock_option(VirtualMachine *vm, ThreadContext *context, const struct ::flock &lock)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 5, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(system_flock_type_to_flock_type(lock.l_type)));
        object->set_elem(1, Value(system_whence_to_whence(lock.l_whence)));
        object->set_elem(2, Value(lock.l_start));
        object->set_elem(3, Value(lock.l_len));
        object->set_elem(4, Value(lock.l_pid));
        return object;
      }

      bool object_to_system_flock(const Object &object, struct ::flock &lock)
      {
        int type, whence;
        if(!flock_type_to_system_flock_type(object.elem(0).i(), type)) return false;
        lock.l_type = type;
        if(!whence_to_system_whence(object.elem(1).i(), whence)) return false;
        lock.l_whence = whence;
        if(!off_to_system_off(object.elem(2).i(), lock.l_start)) return false;
        if(!off_to_system_off(object.elem(3).i(), lock.l_start)) return false;
        if(!pid_to_system_pid(object.elem(4).i(), lock.l_pid)) return false;
        return true;
      }
#endif

      int check_flock(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_tuple() || object.length() != 5) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < 5; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;
      }

#if defined(__unix__) && defined(HAVE_STRUCT_FLOCK64)
      Object *new_flock64(VirtualMachine *vm, ThreadContext *context, const struct ::flock64 &lock)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 5, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(system_flock_type_to_flock_type(lock.l_type)));
        object->set_elem(1, Value(system_whence_to_whence(lock.l_whence)));
        object->set_elem(2, Value(lock.l_start));
        object->set_elem(3, Value(lock.l_len));
        object->set_elem(4, Value(lock.l_pid));
        return object;
      }

      bool object_to_system_flock64(const Object &object, struct ::flock64 &lock)
      {
        int type, whence;
        if(!flock_type_to_system_flock_type(object.elem(0).i(), type)) return false;
        lock.l_type = type;
        if(!whence_to_system_whence(object.elem(1).i(), whence)) return false;
        lock.l_whence = whence;
        if(!off64_to_system_off64(object.elem(2).i(), lock.l_start)) return false;
        if(!off64_to_system_off64(object.elem(3).i(), lock.l_start)) return false;
        if(!pid_to_system_pid(object.elem(3).i(), lock.l_pid)) return false;
        return true;
      }
#endif

      //
      // type stat = (
      //     int,       // st_dev
      //     int,       // st_ino
      //     int,       // st_mode
      //     int,       // st_nlink
      //     int,       // st_uid
      //     int,       // st_gid
      //     int,       // st_rdev
      //     int,       // st_size
      //     int,       // st_atime
      //     int,       // st_atim_nsec
      //     int,       // st_mtime
      //     int,       // st_mtim_nsec
      //     int,       // st_ctime
      //     int,       // st_ctim_nsec
      //     int,       // st_blksize
      //     int        // st_blocks
      //   )
      //

      Object *new_stat(VirtualMachine *vm, ThreadContext *context, const struct ::stat &status)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 16, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(status.st_dev)));
        object->set_elem(1, Value(static_cast<int64_t>(status.st_ino)));
        object->set_elem(2, system_stat_mode_to_stat_mode(status.st_mode));
        object->set_elem(3, Value(static_cast<int64_t>(status.st_nlink)));
        object->set_elem(4, Value(static_cast<int64_t>(status.st_uid)));
        object->set_elem(5, Value(static_cast<int64_t>(status.st_gid)));
        object->set_elem(6, Value(static_cast<int64_t>(status.st_rdev)));
        object->set_elem(7, Value(static_cast<int64_t>(status.st_size)));
#ifdef HAVE_STRUCT_STAT_ATIM
        object->set_elem(8, Value(static_cast<int64_t>(status.st_atim.tv_sec)));
        object->set_elem(9, Value(static_cast<int64_t>(status.st_atim.tv_nsec)));
#else
        object->set_elem(8, Value(static_cast<int64_t>(status.st_atime)));
        object->set_elem(9, Value(0));
#endif
#ifdef HAVE_STRUCT_STAT_MTIM
        object->set_elem(10, Value(static_cast<int64_t>(status.st_mtim.tv_sec)));
        object->set_elem(11, Value(static_cast<int64_t>(status.st_mtim.tv_nsec)));
#else
        object->set_elem(10, Value(static_cast<int64_t>(status.st_mtime)));
        object->set_elem(11, Value(0));
#endif
#ifdef HAVE_STRUCT_STAT_CTIM
        object->set_elem(12, Value(static_cast<int64_t>(status.st_ctim.tv_sec)));
        object->set_elem(13, Value(static_cast<int64_t>(status.st_ctim.tv_nsec)));
#else
        object->set_elem(12, Value(static_cast<int64_t>(status.st_ctime)));
        object->set_elem(13, Value(0));
#endif
#if defined(_WIN32) || defined(_WIN64)
        object->set_elem(14, Value(0));
        object->set_elem(15, Value(0));
#else
        object->set_elem(14, Value(static_cast<int64_t>(status.st_blksize)));
        object->set_elem(15, Value(static_cast<int64_t>(status.st_blocks)));
#endif
        return object;
      }

#if defined(__unix__)
#ifdef HAVE_STRUCT_STAT64
      Object *new_stat64(VirtualMachine *vm, ThreadContext *context, const struct ::stat64 &status)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 16, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(status.st_dev)));
        object->set_elem(1, Value(static_cast<int64_t>(status.st_ino)));
        object->set_elem(2, system_stat_mode_to_stat_mode(status.st_mode));
        object->set_elem(3, Value(static_cast<int64_t>(status.st_nlink)));
        object->set_elem(4, Value(static_cast<int64_t>(status.st_uid)));
        object->set_elem(5, Value(static_cast<int64_t>(status.st_gid)));
        object->set_elem(6, Value(static_cast<int64_t>(status.st_rdev)));
        object->set_elem(7, Value(static_cast<int64_t>(status.st_size)));
#ifdef HAVE_STRUCT_STAT64_ATIM
        object->set_elem(8, Value(static_cast<int64_t>(status.st_atim.tv_sec)));
        object->set_elem(9, Value(static_cast<int64_t>(status.st_atim.tv_nsec)));
#else
        object->set_elem(8, Value(static_cast<int64_t>(status.st_atime)));
        object->set_elem(9, Value(0));
#endif
#ifdef HAVE_STRUCT_STAT64_MTIM
        object->set_elem(10, Value(static_cast<int64_t>(status.st_mtim.tv_sec)));
        object->set_elem(11, Value(static_cast<int64_t>(status.st_mtim.tv_nsec)));
#else
        object->set_elem(10, Value(static_cast<int64_t>(status.st_mtime)));
        object->set_elem(11, Value(0));
#endif
#ifdef HAVE_STRUCT_STAT64_CTIM
        object->set_elem(12, Value(static_cast<int64_t>(status.st_ctim.tv_sec)));
        object->set_elem(13, Value(static_cast<int64_t>(status.st_ctim.tv_nsec)));
#else
        object->set_elem(12, Value(static_cast<int64_t>(status.st_ctime)));
        object->set_elem(13, Value(0));
#endif
        object->set_elem(14, Value(static_cast<int64_t>(status.st_blksize)));
        object->set_elem(15, Value(static_cast<int64_t>(status.st_blocks)));
        return object;
      }
#endif
#elif defined(_WIN32) || defined(_WIN64)
      Object *new__stat64(VirtualMachine *vm, ThreadContext *context, const struct ::_stat64 &status)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 16, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(status.st_dev)));
        object->set_elem(1, Value(static_cast<int64_t>(status.st_ino)));
        object->set_elem(2, system_stat_mode_to_stat_mode(status.st_mode));
        object->set_elem(3, Value(static_cast<int64_t>(status.st_nlink)));
        object->set_elem(4, Value(static_cast<int64_t>(status.st_uid)));
        object->set_elem(5, Value(static_cast<int64_t>(status.st_gid)));
        object->set_elem(6, Value(static_cast<int64_t>(status.st_rdev)));
        object->set_elem(7, Value(static_cast<int64_t>(status.st_size)));
        object->set_elem(8, Value(static_cast<int64_t>(status.st_atime)));
        object->set_elem(9, Value(0));
        object->set_elem(10, Value(static_cast<int64_t>(status.st_mtime)));
        object->set_elem(11, Value(0));
        object->set_elem(12, Value(static_cast<int64_t>(status.st_ctime)));
        object->set_elem(13, Value(0));
        object->set_elem(14, Value(0));
        object->set_elem(15, Value(0));
        return object;
      }
#else
#error "Unsupported operating system."
#endif

      // Functions for struct timespec.

      //
      // type timespec = (
      //     int,       // tv_sec
      //     int        // tv_nsec
      //   )
      //

#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
      Object *new_timespec(VirtualMachine *vm, ThreadContext *context, const struct ::timespec &time_value)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(static_cast<int64_t>(time_value.tv_sec)));
        object->set_elem(1, Value(static_cast<int64_t>(time_value.tv_nsec)));
        return object;
      }

      bool object_to_system_timespec(const Object &object, struct ::timespec &time_value)
      {
        if((object.elem(0).i() < static_cast<int64_t>(numeric_limits<time_t>::min())) ||
          (object.elem(0).i() > static_cast<int64_t>(numeric_limits<time_t>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        if((object.elem(1).i() < static_cast<int64_t>(numeric_limits<long>::min())) || 
            (object.elem(1).i() > static_cast<int64_t>(numeric_limits<long>::max()))) {
          letin_errno() = EINVAL;
          return false;
        }
        time_value.tv_sec = object.elem(0).i();
        time_value.tv_nsec = object.elem(1).i();
        return true;
      }
#endif

      int check_timespec(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_tuple() || object.length() != 2) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < 2; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;
      }

      // Fucntions for struct tms.

      //
      // type tms = (
      //     int,       // tms_utime
      //     int,       // tms_stime
      //     int,       // tms_cutime
      //     int        // tms_cutime
      //   )
      //

#if defined(__unix__)
      Object *new_tms(VirtualMachine *vm, ThreadContext *context, const struct ::tms &times)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 4, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, times.tms_utime);
        object->set_elem(1, times.tms_stime);
        object->set_elem(2, times.tms_cutime);
        object->set_elem(3, times.tms_cstime);
        return object;
      }
#endif

      // Functions for struct termios.
      
      //
      // type termios = (
      //     int,       // c_iflag
      //     int,       // c_oflag
      //     int,       // c_cflag
      //     int,       // c_lflag
      //     iarray32,  // c_cc
      //     int,       // c_ispeed
      //     int        // c_ospeed
      //   )
      //

#if defined(__unix__)
      bool set_new_termios(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const struct ::termios &termios)
      {
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 7, context);
        if(tmp_r.is_null()) return false;
        tmp_r->set_elem(0, Value(system_termios_iflag_to_termios_iflag(termios.c_iflag)));
        tmp_r->set_elem(1, Value(system_termios_oflag_to_termios_oflag(termios.c_oflag)));
        tmp_r->set_elem(2, Value(system_termios_cflag_to_termios_cflag(termios.c_cflag)));
        tmp_r->set_elem(3, Value(system_termios_lflag_to_termios_lflag(termios.c_lflag)));
        RegisteredReference cc_r(vm->gc()->new_object(OBJECT_TYPE_IARRAY32, system_termios_cc_indexes.size(), context), context);
        if(cc_r.is_null()) return false;
        size_t i = 0;
        for(auto system_cc_index : system_termios_cc_indexes) {
          if(system_cc_index != -1) cc_r->set_elem(i, Value(termios.c_cc[system_cc_index]));
          i++;
        }
        tmp_r->set_elem(4, Value(cc_r));
        tmp_r->set_elem(5, Value(system_speed_to_speed(cfgetispeed(&termios))));
        tmp_r->set_elem(6, Value(system_speed_to_speed(cfgetospeed(&termios))));
        tmp_r.register_ref();
        return true;
      }

      bool object_to_system_termios(const Object &object, struct ::termios &termios)
      {
        if(!termios_iflag_to_system_termios_iflag(object.elem(0).i(), termios.c_iflag)) return false;
        if(!termios_oflag_to_system_termios_oflag(object.elem(1).i(), termios.c_oflag)) return false;
        if(!termios_cflag_to_system_termios_cflag(object.elem(2).i(), termios.c_cflag)) return false;
        if(!termios_lflag_to_system_termios_lflag(object.elem(3).i(), termios.c_lflag)) return false;
        size_t i = 0;
        fill_n(termios.c_cc, NCCS, 0);
        for(auto cc_index : system_termios_cc_indexes) {
          termios.c_cc[i + termios_cc_index_offset] = object.elem(4).r()->elem(cc_index).i();
          i++;
        }
        ::speed_t input_speed;
        if(!speed_to_system_speed(object.elem(5).i(), input_speed)) return false;
        cfsetispeed(&termios, input_speed);
        ::speed_t output_speed;
        if(!speed_to_system_speed(object.elem(6).i(), output_speed)) return false;
        cfsetispeed(&termios, output_speed);
        return true;
      }
#endif

      int check_termios(VirtualMachine *vm, ThreadContext *context, Object &object)
      {
        if(!object.is_tuple() || object.length() != 7) return ERROR_INCORRECT_OBJECT;
        for(size_t i = 0; i < 4; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        int error = vm->force_tuple_elem(context, object, 4);
        if(error != ERROR_SUCCESS) return error;
        if(!object.elem(4).is_ref()) return ERROR_INCORRECT_OBJECT;
        if(!object.elem(4).r()->is_iarray32() || object.elem(4).r()->length() != 20)
          return ERROR_INCORRECT_OBJECT;
        for(size_t i = 5; i < 6; i++) {
          int error = vm->force_tuple_elem(context, object, i);
          if(error != ERROR_SUCCESS) return error;
          if(!object.elem(i).is_int()) return ERROR_INCORRECT_OBJECT;
        }
        return ERROR_SUCCESS;        
      }

      // Functions for struct utsname.

      //
      // type utsname = (
      //     iarray8,   // sysname
      //     iarray8,   // nodename
      //     iarray8,   // release
      //     iarray8,   // version
      //     iarray8    // machine
      //   )
      //

#if defined(__unix__)
      bool set_new_utsname(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const struct ::utsname &os_info)
      {
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 5, context);
        if(tmp_r.is_null()) return false;
        RegisteredReference sysname_r(vm->gc()->new_string(os_info.sysname, context), context);
        if(sysname_r.is_null()) return false;
        tmp_r->set_elem(0, Value(sysname_r));
        RegisteredReference nodename_r(vm->gc()->new_string(os_info.nodename, context), context);
        if(nodename_r.is_null()) return false;
        tmp_r->set_elem(1, Value(nodename_r));
        RegisteredReference release_r(vm->gc()->new_string(os_info.release, context), context);
        if(release_r.is_null()) return false;
        tmp_r->set_elem(2, Value(release_r));
        RegisteredReference version_r(vm->gc()->new_string(os_info.version, context), context);
        if(version_r.is_null()) return false;
        tmp_r->set_elem(3, Value(version_r));
        RegisteredReference machine_r(vm->gc()->new_string(os_info.machine, context), context);
        if(machine_r.is_null()) return false;
        tmp_r->set_elem(3, Value(machine_r));
        tmp_r.register_ref();
        return true;
      }
#endif

      // Functions for Directory *.

      Object *new_directory(VirtualMachine *vm, ThreadContext *context, Directory *dir)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_NATIVE_OBJECT | OBJECT_TYPE_UNIQUE, sizeof(Directory *), context);
        if(object == nullptr) return nullptr;
        object->raw().ntvo.type = NativeObjectType(&directory_type_ident);
        object->raw().ntvo.clazz = NativeObjectClass(&directory_funs);
        Directory **dir_ptr = reinterpret_cast<Directory **>(object->raw().ntvo.bs);
        *dir_ptr = dir;
        return object;
      }

      bool object_to_system_directory(const Object &object, Directory *&dir)
      {
        Directory * const *dir_ptr = reinterpret_cast<Directory * const *>(object.raw().ntvo.bs);
        if(*dir_ptr == nullptr) {
          letin_errno() = EBADF;
          return false;
        }
        dir = *dir_ptr;
        return true;
      }

      int check_directory(VirtualMachine *vm, vm::ThreadContext *context, Object &object)
      {
        if(!object.is_native(NativeObjectType(&directory_type_ident)))
          return ERROR_INCORRECT_OBJECT;
        return ERROR_SUCCESS;
      }

      // Functions for DirectoryEntry *.

      //
      // type dirent = (
      //     int,       // d_ino
      //     iarray8    // d_name
      //   )
      //

      bool set_new_directory_entry(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, DirectoryEntry *dir_entry)
      {
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_TUPLE, 2, context);
        if(tmp_r.is_null()) return false;
        tmp_r->set_elem(0, Value(static_cast<int64_t>(dir_entry_inode(dir_entry))));
        RegisteredReference name_r(vm->gc()->new_string(dir_entry_name(dir_entry), context), context);
        tmp_r->set_elem(1, Value(name_r));
        tmp_r.register_ref();
        return true;
      }

#if defined(_WIN32) || defined(_WIN64)
#if !defined(__MINGW32__) && !defined(__MINGW64__)
      bool object_to_system_timespec_dword(const Object &object, ::DWORD &dword)
      {
        if((object.elem(0).i() < static_cast<int64_t>(numeric_limits<::DWORD>::min() / 1000)) ||
            (object.elem(0).i() > static_cast<int64_t>(numeric_limits<::DWORD>::max() / 1000))) {
          letin_errno() = EINVAL;
          return false;
        }
        if((object.elem(1).i() < 0) || (object.elem(1).i() > static_cast<int64_t>(999999999))) {
          letin_errno() = EINVAL;
          return false;
        }
        dword = object.elem(0).i() * 1000 + object.elem(1).i() / 1000000;
        return true;
      }
#endif

      int system_error_for_windows(bool is_process_fun)
      {
        switch(::GetLastError()) {
          case ERROR_ACCESS_DENIED:
            return is_process_fun ? EPERM : EACCES;
          case ERROR_BAD_FORMAT:
            return ENOEXEC;
          case ERROR_FILE_NOT_FOUND:
            return ENOENT;
          case ERROR_IO_DEVICE:
            return EIO;
          case ERROR_CHILD_NOT_COMPLETE:
            return ECHILD;
          case ERROR_INVALID_HANDLE:
            return is_process_fun ? EINVAL : EBADF;
          case ERROR_INVALID_PARAMETER:
            return is_process_fun ? ESRCH : EINVAL;
          case ERROR_PATH_NOT_FOUND:
            return ENOENT;
          case ERROR_WAIT_NO_CHILDREN:
            return ECHILD;
          default:
            return EINVAL;
        }
      }

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
