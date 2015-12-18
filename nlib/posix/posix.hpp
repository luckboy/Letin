/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _POSIX_HPP
#define _POSIX_HPP

#include <sys/types.h>
#include <sys/stat.h>
#if defined(__unix__)
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <sys/time.h>
#endif
#include <io.h>
#include <windows.h>
#include <winsock2.h>
#else
#error "Unsupported operating system."
#endif
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <time.h>
#include <letin/native.hpp>
#include "dir.hpp"

namespace letin
{
  namespace nlib
  {
    namespace posix
    {
#if defined(__unix__)
      typedef ::pid_t Pid;
#elif defined(_WIN32) || defined(_WIN64)
      typedef ::DWORD Pid;
#else
#error "Unsupported operating system."
#endif

#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
      typedef ::useconds_t Useconds;
#elif defined(_WIN32) || defined(_WIN64)
      typedef ::DWORD Useconds;
#else
#error "Unsupported operating system."
#endif

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

      class Argv
      {
        std::unique_ptr<char *[]> _M_ptr;
      public:
        Argv() {}

        ~Argv()
        { for(std::size_t i = 0; _M_ptr.get()[i] != nullptr; i++) delete [] _M_ptr.get(); }
        
        void set_ptr(char **ptr) { _M_ptr = std::unique_ptr<char *[]>(ptr); }

        char **ptr() const { return _M_ptr.get(); }

        char *&operator[](std::size_t i) const { return _M_ptr[i]; }

        char *&operator[](std::size_t i) { return _M_ptr[i]; }
      };

      void initialize_errors();

      void initialize_consts();

      long system_clk_tck();

      std::int64_t system_error_to_error(int system_error);

      bool error_to_system_error(std::int64_t error_num, int &system_error_num);
      LETIN_NATIVE_INT_CONVERTER(toerrno, error_to_system_error, int);

      std::int64_t system_whence_to_whence(int system_whence);
      
      bool whence_to_system_whence(std::int64_t whence, int &system_whence);
      LETIN_NATIVE_INT_CONVERTER(towhence, whence_to_system_whence, int);

      std::int64_t system_flags_to_flags(int system_flags);

      bool flags_to_system_flags(std::int64_t flags, int &system_flags);
      LETIN_NATIVE_INT_CONVERTER(toflags, flags_to_system_flags, int);

      bool fd_to_system_fd(std::int64_t fd, int &system_fd);
      LETIN_NATIVE_INT_CONVERTER(tofd, fd_to_system_fd, int);

      bool count_to_system_count(std::int64_t count, std::size_t &system_count);
      LETIN_NATIVE_INT_CONVERTER(tocount, count_to_system_count, std::size_t);

      bool arg_to_system_arg(std::int64_t arg, int &system_arg);
      LETIN_NATIVE_INT_CONVERTER(toarg, arg_to_system_arg, int);

      bool long_arg_to_system_long_arg(std::int64_t arg, long &system_arg);
      LETIN_NATIVE_INT_CONVERTER(tolarg, long_arg_to_system_long_arg, long);

      bool ref_to_system_buffer_ref(vm::Reference buffer_r, vm::Reference &system_buffer_r);
      LETIN_NATIVE_REF_CONVERTER(tobufref, ref_to_system_buffer_ref, vm::Reference);

      void system_path_name_to_path_name(char *path_name);

      bool object_to_system_path_name(const vm::Object &object, std::string &path_name);
      LETIN_NATIVE_OBJECT_CONVERTER(topath, object_to_system_path_name, std::string);

      bool access_mode_to_system_access_mode(std::int64_t mode, int &system_mode);
      LETIN_NATIVE_INT_CONVERTER(toamode, access_mode_to_system_access_mode, int);

#if defined(__unix__)
      bool nfds_to_system_nfds(std::int64_t nfds, int &system_nfds);
      LETIN_NATIVE_INT_CONVERTER(tonfds, nfds_to_system_nfds, int);
#endif

      bool wait_options_to_system_wait_options(std::int64_t options, int &system_options);
      LETIN_NATIVE_INT_CONVERTER(towoptions, wait_options_to_system_wait_options, int);

      bool object_to_system_argv(const vm::Object &object, Argv &argv);
      LETIN_NATIVE_OBJECT_CONVERTER(toargv, object_to_system_argv, Argv);
      
      int check_argv(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(cargv, check_argv);

#if defined(__unix__)
      std::int64_t system_signal_to_signal(int system_signal);

      bool signal_to_system_signal(std::int64_t signal, int &system_signal);
      LETIN_NATIVE_INT_CONVERTER(tosig, signal_to_system_signal, int);
#endif

#if defined(_WIN32) || defined(_WIN64)
      bool kill_signal_to_system_kill_signal(std::int64_t signal, int &system_signal);
      LETIN_NATIVE_INT_CONVERTER(toksig, kill_signal_to_system_kill_signal, int);
#endif

#if defined(__unix__)
      bool which_to_system_which(std::int64_t which, int &system_which);
      LETIN_NATIVE_INT_CONVERTER(towhich, which_to_system_which, int);

      bool optional_actions_to_system_optional_actions(std::int64_t optional_actions, int &system_optional_actions);
      LETIN_NATIVE_INT_CONVERTER(tooactions, optional_actions_to_system_optional_actions, int);

      bool queue_selector_to_system_queue_selector(std::int64_t queue_selector, int &system_queue_selector);
      LETIN_NATIVE_INT_CONVERTER(toqselector, queue_selector_to_system_queue_selector, int);

      bool tcflow_action_to_system_tcflow_action(std::int64_t action, int &system_action);
      LETIN_NATIVE_INT_CONVERTER(totaction, tcflow_action_to_system_tcflow_action, int);

      std::int64_t system_poll_events_to_poll_events(short system_events);

      bool poll_events_to_system_poll_events(std::int64_t events, short &system_events);

      std::int64_t system_flock_type_to_flock_type(short system_type);

      bool flock_type_to_system_flock_type(std::int64_t type, int &system_type);

      std::int64_t system_termios_iflag_to_termios_iflag(::tcflag_t system_iflag);
      
      bool termios_iflag_to_system_termios_iflag(std::int64_t iflag, ::tcflag_t &system_iflag);

      std::int64_t system_termios_oflag_to_termios_oflag(::tcflag_t system_oflag);

      bool termios_oflag_to_system_termios_oflag(std::int64_t oflag, ::tcflag_t &system_oflag);

      std::int64_t system_termios_cflag_to_termios_cflag(::tcflag_t system_cflag);

      bool termios_cflag_to_system_termios_cflag(std::int64_t iflag, ::tcflag_t &system_cflag);

      std::int64_t system_termios_lflag_to_termios_lflag(::tcflag_t system_lflag);

      bool termios_lflag_to_system_termios_lflag(std::int64_t lflag, ::tcflag_t &system_lflag);

      vm::Object *new_wait_status(vm::VirtualMachine *vm, vm::ThreadContext *context, int status);
      LETIN_NATIVE_OBJECT_SETTER(vwstatus, new_wait_status, int);
#endif

      std::int64_t system_mode_to_mode(::mode_t system_mode);

      bool mode_to_system_mode(std::int64_t mode, ::mode_t &system_mode);
      LETIN_NATIVE_INT_CONVERTER(tomode, mode_to_system_mode, ::mode_t);

      std::int64_t system_stat_mode_to_stat_mode(::mode_t system_mode);

      bool stat_mode_to_system_stat_mode(std::int64_t mode, ::mode_t &system_mode);
      LETIN_NATIVE_INT_CONVERTER(tosmode, stat_mode_to_system_stat_mode, ::mode_t);

      bool size_to_system_size(std::int64_t size, std::size_t &system_size);
      LETIN_NATIVE_INT_CONVERTER(tosize, size_to_system_size, std::size_t);

      bool off_to_system_off(std::int64_t offset, ::off_t &system_offset);
      LETIN_NATIVE_INT_CONVERTER(tooff, off_to_system_off, ::off_t);

#ifdef HAVE_OFF64_T
      bool off64_to_system_off64(std::int64_t offset, ::off64_t &system_offset);
      LETIN_NATIVE_INT_CONVERTER(tooff64, off64_to_system_off64, ::off64_t);
#endif

#if defined(__unix__)
      bool uid_to_system_uid(std::int64_t uid, ::uid_t &system_uid);
      LETIN_NATIVE_INT_CONVERTER(touid, uid_to_system_uid, ::uid_t);

      bool gid_to_system_gid(std::int64_t gid, ::gid_t &system_gid);
      LETIN_NATIVE_INT_CONVERTER(togid, gid_to_system_gid, ::gid_t);

      bool object_to_system_gids(const vm::Object &object, Array<::gid_t> &gids);
      LETIN_NATIVE_OBJECT_CONVERTER(togids, object_to_system_gids, Array<::gid_t>);
#endif

      bool dev_to_system_dev(std::int64_t dev, ::dev_t &system_dev);
      LETIN_NATIVE_INT_CONVERTER(todev, dev_to_system_dev, ::dev_t);

      bool pid_to_system_pid(std::int64_t pid, Pid &system_pid);
      LETIN_NATIVE_INT_CONVERTER(topid, pid_to_system_pid, Pid);

      bool wait_pid_to_system_wait_pid(std::int64_t pid, Pid &system_pid);
      LETIN_NATIVE_INT_CONVERTER(towpid, wait_pid_to_system_wait_pid, Pid);

      bool useconds_to_system_useconds(std::int64_t useconds, Useconds &system_useconds);
      LETIN_NATIVE_INT_CONVERTER(touseconds, useconds_to_system_useconds, Useconds);

#if defined(__unix__)
      std::int64_t system_speed_to_speed(::speed_t system_speed);

      bool speed_to_system_speed(std::int64_t speed, ::speed_t &system_speed);
#endif

      // Functions for fd_set.

#if defined(__unix__)
      vm::Object *new_fd_set(vm::VirtualMachine *vm, vm::ThreadContext *context, const ::fd_set &fds);
      LETIN_NATIVE_OBJECT_SETTER(vfd_set, new_fd_set, ::fd_set);

      void system_fd_set_to_object(const ::fd_set &fds, vm::Object &object);

      bool object_to_system_fd_set(const vm::Object &object, ::fd_set &fds);
      LETIN_NATIVE_OBJECT_CONVERTER(tofd_set, object_to_system_fd_set, ::fd_set);
#endif

      // Functions for struct timeval.

#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
      vm::Object *new_timeval(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::timeval &time_value);
      LETIN_NATIVE_OBJECT_SETTER(vtimeval, new_timeval, struct ::timeval);
#endif

      bool object_to_system_timeval(const vm::Object &object, struct ::timeval &time_value);
      LETIN_NATIVE_OBJECT_CONVERTER(totimeval, object_to_system_timeval, struct ::timeval);

      int check_timeval(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(ctimeval, check_timeval);

      // Functions for struct pollfd [].

#if defined(__unix__)
      bool set_new_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const Array<struct ::pollfd> &fds);
      LETIN_NATIVE_REF_SETTER(vpollfds, set_new_pollfds, Array<struct ::pollfd>);

      void system_pollfds_to_object(const Array<struct ::pollfd> &fds, vm::Object &object);

      bool object_to_system_pollfds(vm::Object &object, Array<struct ::pollfd> &fds);
      LETIN_NATIVE_OBJECT_CONVERTER(topollfds, object_to_system_pollfds, Array<struct ::pollfd>);
#endif

      int check_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(cpollfds, check_pollfds);

      int check_unique_pollfds(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_UNIQUE_OBJECT_CHECKER(cupollfds, check_unique_pollfds);

      // Functions for struct flock and struct flock64.

#if defined(__unix__)
      vm::Object *new_flock(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::flock &lock);
      LETIN_NATIVE_OBJECT_SETTER(vflock, new_flock, struct ::flock);

      bool object_to_system_flock(const vm::Object &object, struct ::flock &lock);
      LETIN_NATIVE_OBJECT_CONVERTER(toflock, object_to_system_flock, struct ::flock);
#endif

      int check_flock(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(cflock, check_flock);

#if defined(__unix__) && defined(HAVE_STRUCT_FLOCK64)
      vm::Object *new_flock64(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::flock64 &lock);
      LETIN_NATIVE_OBJECT_SETTER(vflock64, new_flock64, struct ::flock64);

      bool object_to_system_flock64(const vm::Object &object, struct ::flock64 &lock);
      LETIN_NATIVE_OBJECT_CONVERTER(toflock64, object_to_system_flock64, struct ::flock64);
#endif

      // Functions for struct stat and struct stat64.

      vm::Object *new_stat(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::stat &status);
      LETIN_NATIVE_OBJECT_SETTER(vstat, new_stat, struct ::stat);

#if defined(__unix__)
#ifdef HAVE_STRUCT_STAT64
      vm::Object *new_stat64(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::stat64 &status);
      LETIN_NATIVE_OBJECT_SETTER(vstat64, new_stat64, struct ::stat64);
#endif
#elif defined(_WIN32) || defined(_WIN64)
      vm::Object *new__stat64(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::_stat64 &status);
      LETIN_NATIVE_OBJECT_SETTER(v_stat64, new__stat64, struct ::_stat64);
#else
#error "Unsupported operating system."
#endif

      // Functions for struct timespec.

#if defined(__unix__) || ((defined(_WIN32) || defined(_WIN64)) && (defined(__MINGW32__) || defined(__MINGW64__)))
      vm::Object *new_timespec(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::timespec &time_value);
      LETIN_NATIVE_OBJECT_SETTER(vtimespec, new_timespec, struct ::timespec);

      bool object_to_system_timespec(const vm::Object &object, struct ::timespec &time_value);
      LETIN_NATIVE_OBJECT_CONVERTER(totimespec, object_to_system_timespec, struct ::timespec);
#endif

      int check_timespec(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(ctimespec, check_timespec);

      // Functions for struct tms.

#if defined(__unix__)
      vm::Object *new_tms(vm::VirtualMachine *vm, vm::ThreadContext *context, const struct ::tms &times);
      LETIN_NATIVE_OBJECT_SETTER(vtms, new_tms, struct ::tms);
#endif

      // Functions for struct termios.

#if defined(__unix__)
      bool set_new_termios(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const struct ::termios &termios);
      LETIN_NATIVE_REF_SETTER(vtermios, set_new_termios, struct ::termios);

      bool object_to_system_termios(const vm::Object &object, struct ::termios &termios);
      LETIN_NATIVE_OBJECT_CONVERTER(totermios, object_to_system_termios, struct ::termios);
#endif

      int check_termios(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_OBJECT_CHECKER(ctermios, check_termios);

      // Functions for struct utsname.

#if defined(__unix__)
      bool set_new_utsname(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, const struct ::utsname &os_info);
      LETIN_NATIVE_REF_SETTER(vutsname, set_new_utsname, struct ::utsname);
#endif

      // Functions for DIR *.

      vm::Object *new_directory(vm::VirtualMachine *vm, vm::ThreadContext *context, Directory *dir);
      inline LETINT_NATIVE_OBJECT_SETTER_TYPE(new_directory, Directory *) vdir(Directory * const &dir)
      { return LETINT_NATIVE_OBJECT_SETTER_TYPE(new_directory, Directory *)(new_directory, dir); }

      bool object_to_system_directory(const vm::Object &object, Directory *&dir);
      LETIN_NATIVE_OBJECT_CONVERTER(todir, object_to_system_directory, Directory *);

      int check_directory(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object);
      LETIN_NATIVE_UNIQUE_OBJECT_CHECKER(cdir, check_directory);

      // Functions for DirectoryEntry *.

      bool set_new_directory_entry(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &tmp_r, DirectoryEntry *dir_entry);
      inline LETINT_NATIVE_REF_SETTER_TYPE(set_new_directory_entry, DirectoryEntry *) vdirentry(DirectoryEntry * const &dir_entry)
      { return LETINT_NATIVE_REF_SETTER_TYPE(set_new_directory_entry, DirectoryEntry *)(set_new_directory_entry, dir_entry); }

      // Functions for Windows.

#if defined(_WIN32) || defined(_WIN64)
#if !defined(__MINGW32__) && !defined(__MINGW64__)
      bool object_to_system_timespec_dword(const vm::Object &object, ::DWORD);
      LETIN_NATIVE_OBJECT_CONVERTER(totimespecdw, object_to_system_timespec_dword, ::DWORD);
#endif

      int system_error_for_windows(bool is_process_fun = false);

      int system_error_for_winsock2();

      template<typename _T>
      static inline vm::ReturnValue return_value_with_errno_for_windows(vm::VirtualMachine *vm, vm::ThreadContext *context, _T setter, bool is_process_fun = false)
      { return native::return_value_with_errno(vm, context, setter, system_error_for_windows(is_process_fun)); }

      template<typename _T>
      static inline vm::ReturnValue return_value_with_errno_for_winsock2(vm::VirtualMachine *vm, vm::ThreadContext *context, _T setter)
      { return native::return_value_with_errno(vm, context, setter, system_error_for_winsock2()); }
#endif
    }
  }
}

#endif
