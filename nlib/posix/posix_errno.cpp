/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <cerrno>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif
#include "posix.hpp"

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

#ifndef EBADMSG
#define EBADMSG                         ((1 << 30) + 0)
#endif
#ifndef ENODATA
#define ENODATA                         ((1 << 30) + 1)
#endif
#ifndef EMULTIHOP
#define EMULTIHOP                       ((1 << 30) + 2)
#endif
#ifndef ENOLINK
#define ENOLINK                         ((1 << 30) + 3)
#endif
#ifndef ENOSR
#define ENOSR                           ((1 << 30) + 4)
#endif
#ifndef ENOSTR
#define ENOSTR                          ((1 << 30) + 5)
#endif
#ifndef ENOTRECOVERABLE
#define ENOTRECOVERABLE                 ((1 << 30) + 6)
#endif
#ifndef EOWNERDEAD
#define EOWNERDEAD                      ((1 << 30) + 7)
#endif
#ifndef EPROTO
#define EPROTO                          ((1 << 30) + 8)
#endif
#ifndef ETIME
#define ETIME                           ((1 << 30) + 9)
#endif

#elif defined(_WIN32) || defined(_WIN64)

#undef EADDRINUSE
#define EADDRINUSE                      ((1 << 29) + WSAEADDRINUSE)
#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL                   ((1 << 29) + WSAEADDRNOTAVAIL)
#undef EAFNOSUPPORT
#define EAFNOSUPPORT                    ((1 << 29) + WSAEAFNOSUPPORT)
#undef EALREADY
#define EALREADY                        ((1 << 29) + WSAEALREADY)
#undef ECONNABORTED
#define ECONNABORTED                    ((1 << 29) + WSAECONNABORTED)
#undef ECONNREFUSED
#define ECONNREFUSED                    ((1 << 29) + WSAECONNREFUSED)
#undef ECONNRESET
#define ECONNRESET                      ((1 << 29) + WSAECONNRESET)
#undef EDESTADDRREQ
#define EDESTADDRREQ                    ((1 << 29) + WSAEDESTADDRREQ)
#undef EDQUOT
#define EDQUOT                          ((1 << 29) + WSAEDQUOT)
#undef EHOSTUNREACH
#define EHOSTUNREACH                    ((1 << 29) + WSAEHOSTUNREACH)
#undef EINPROGRESS
#define EINPROGRESS                     ((1 << 29) + WSAEINPROGRESS)
#undef EISCONN
#define EISCONN                         ((1 << 29) + WSAEISCONN)
#undef ELOOP
#define ELOOP                           ((1 << 29) + WSAELOOP)
#undef EMSGSIZE
#define EMSGSIZE                        ((1 << 29) + WSAEMSGSIZE)
#undef ENETDOWN
#define ENETDOWN                        ((1 << 29) + WSAENETDOWN)
#undef ENETRESET
#define ENETRESET                       ((1 << 29) + WSAENETRESET)
#undef ENETUNREACH
#define ENETUNREACH                     ((1 << 29) + WSAENETUNREACH)
#undef ENOBUFS
#define ENOBUFS                         ((1 << 29) + WSAENOBUFS)
#undef ENOPROTOOPT
#define ENOPROTOOPT                     ((1 << 29) + WSAENOPROTOOPT)
#undef ENOTCONN
#define ENOTCONN                        ((1 << 29) + WSAENOTCONN)
#undef ENOTSOCK
#define ENOTSOCK                        ((1 << 29) + WSAENOTSOCK)
#undef EPROTONOSUPPORT
#define EPROTONOSUPPORT                 ((1 << 29) + WSAEPROTONOSUPPORT)
#undef EPROTOTYPE
#define EPROTOTYPE                      ((1 << 29) + WSAEPROTOTYPE)
#undef ESTALE
#define ESTALE                          ((1 << 29) + WSAESTALE)

#ifndef EBADMSG
#define EBADMSG                         ((1 << 30) + 0)
#endif
#ifndef ECANCELED
#define ECANCELED                       ((1 << 30) + 1)
#endif
#ifndef EIDRM
#define EIDRM                           ((1 << 30) + 2)
#endif
#ifndef EMULTIHOP
#define EMULTIHOP                       ((1 << 30) + 3)
#endif
#ifndef ENODATA
#define ENODATA                         ((1 << 30) + 4)
#endif
#ifndef ENOLINK
#define ENOLINK                         ((1 << 30) + 5)
#endif
#ifndef ENOMSG
#define ENOMSG                          ((1 << 30) + 6)
#endif
#ifndef ENOSR
#define ENOSR                           ((1 << 30) + 7)
#endif
#ifndef ENOSTR
#define ENOSTR                          ((1 << 30) + 8)
#endif
#ifndef ENOTRECOVERABLE
#define ENOTRECOVERABLE                 ((1 << 30) + 9)
#endif
#ifndef EOVERFLOW
#define EOVERFLOW                       ((1 << 30) + 10)
#endif
#ifndef EOWNERDEAD
#define EOWNERDEAD                      ((1 << 30) + 11)
#endif
#ifndef EPROTO
#define EPROTO                          ((1 << 30) + 12)
#endif
#ifndef ETIME
#define ETIME                           ((1 << 30) + 13)
#endif
#ifndef ETXTBSY
#define ETXTBSY                         ((1 << 30) + 14)
#endif

#endif

using namespace std;

namespace letin
{
  namespace nlib
  {
    namespace posix
    {
      static vector<int> system_errors;
      static vector<int> errors;
      static size_t error_offset;
#if defined(_WIN32) || defined(_WIN64)
      static vector<int> winsock2_errors;
      static size_t winsock2_error_offset;
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(_WIN32) || defined(_WIN64)
      static vector<int> other_errors;
      static size_t other_error_offset;
#endif

      void initialize_errors()
      {
        system_errors = {
          0,
          E2BIG,
          EACCES,
          EADDRINUSE,
          EADDRNOTAVAIL,
          EAFNOSUPPORT,
          EAGAIN,
          EALREADY,
          EBADF,
          EBADMSG,
          EBUSY,
          ECANCELED,
          ECHILD,
          ECONNABORTED,
          ECONNREFUSED,
          ECONNRESET,
          EDEADLK,
          EDESTADDRREQ,
          EDOM,
          EDQUOT,
          EEXIST,
          EFAULT,
          EFBIG,
          EHOSTUNREACH,
          EIDRM,
          EILSEQ,
          EINPROGRESS,
          EINTR,
          EINVAL,
          EIO,
          EISCONN,
          EISDIR,
          ELOOP,
          EMFILE,
          EMLINK,
          EMSGSIZE,
          EMULTIHOP,
          ENAMETOOLONG,
          ENETDOWN,
          ENETRESET,
          ENETUNREACH,
          ENFILE,
          ENOBUFS,
          ENODATA,
          ENODEV,
          ENOENT,
          ENOEXEC,
          ENOLCK,
          ENOLINK,
          ENOMEM,
          ENOMSG,
          ENOPROTOOPT,
          ENOSPC,
          ENOSR,
          ENOSTR,
          ENOSYS,
          ENOTCONN,
          ENOTDIR,
          ENOTEMPTY,
          ENOTRECOVERABLE,
          ENOTSOCK,
          ENOTSUP,
          ENOTTY,
          ENXIO,
          EOVERFLOW,
          EOWNERDEAD,
          EPERM,
          EPIPE,
          EPROTO,
          EPROTONOSUPPORT,
          EPROTOTYPE,
          ERANGE,
          EROFS,
          ESPIPE,
          ESRCH,
          ESTALE,
          ETIME,
          ETIMEDOUT,
          ETXTBSY,
          EXDEV
        };
        int min_system_error = numeric_limits<int>::max();
        int max_system_error = numeric_limits<int>::min();
        for(auto system_error : system_errors) {
          if(system_error < (1 << 29)) {
            min_system_error = min(min_system_error, system_error);
            max_system_error = max(max_system_error, system_error);
          }
        }
        errors.resize(max_system_error - min_system_error + 1);
        error_offset = min_system_error;
        fill(errors.begin(), errors.end(), -1);
        for(int i = 0; i < static_cast<int>(system_errors.size()); i++) {
          if(system_errors[i] < (1 << 29))
            errors[system_errors[i] - min_system_error] = i;
        }
#if defined(_WIN32) || defined(_WIN64)
        int min_winsock2_system_error = numeric_limits<int>::max();
        int max_winsock2_system_error = numeric_limits<int>::min();
        for(auto system_error : system_errors) {
          if(system_error >= (1 << 29) && system_error < (1 << 30)) {
            min_winsock2_system_error = min(min_winsock2_system_error, system_error);
            max_winsock2_system_error = max(max_winsock2_system_error, system_error);
          }
        }
        winsock2_errors.resize(max_winsock2_system_error - min_winsock2_system_error + 1);
        winsock2_error_offset = min_winsock2_system_error;
        fill(winsock2_errors.begin(), winsock2_errors.end(), -1);
        for(int i = 0; i < static_cast<int>(system_errors.size()); i++) {
          if(system_errors[i] >= (1 << 29) && system_errors[i] < (1 << 30))
            winsock2_errors[system_errors[i] - min_winsock2_system_error] = i;
        }
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(_WIN32) || defined(_WIN64)
        int min_other_system_error = numeric_limits<int>::max();
        int max_other_system_error = numeric_limits<int>::min();
        for(auto system_error : system_errors) {
          if(system_error >= (1 << 30)) {
            min_other_system_error = min(min_other_system_error, system_error);
            max_other_system_error = max(max_other_system_error, system_error);
          }
        }
        other_errors.resize(max_other_system_error - min_other_system_error + 1);
        other_error_offset = min_other_system_error;
        fill(other_errors.begin(), other_errors.end(), -1);
        for(int i = 0; i < static_cast<int>(system_errors.size()); i++) {
          if(system_errors[i] >= (1 << 30))
            other_errors[system_errors[i] - min_other_system_error] = i;
        }
#endif
      }

      int64_t system_error_to_error(int system_error)
      {
#if EAGAIN != EWOULDBLOCK
        if(system_error == EWOULDBLOCK) system_error = EAGAIN;
#endif
#if defined(__unix__) && ENOTSUP != EOPNOTSUPP
        if(system_error == EOPNOTSUPP) system_error = ENOTSUP;
#endif
        if(system_error >= static_cast<int>(error_offset) && system_error < static_cast<int>(errors.size())) {
          return errors[system_error - error_offset];
        } else {
#if defined(__unix__)
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
          if(system_error >= static_cast<int>(other_error_offset) && system_error < static_cast<int>(other_errors.size()))
            return errors[system_error - other_error_offset];
          else
            return -1;
#else
          return -1;
#endif
#elif defined(_WIN32) || defined(_WIN64)
          if(system_error >= static_cast<int>(winsock2_error_offset) && system_error < static_cast<int>(winsock2_errors.size()))
            return errors[system_error - winsock2_error_offset];
          else if(system_error >= static_cast<int>(other_error_offset) && system_error < static_cast<int>(other_errors.size()))
            return errors[system_error - other_error_offset];
          else
            return -1;
#else
#error "Unsupported operating system."
#endif
        }
      }

      bool error_to_system_error(int64_t error, int &system_error)
      { 
        if(error >= 0 && error < static_cast<int64_t>(system_errors.size())) {
          system_error = system_errors[error];
          return true;
        } else
          return false;
      }
    }
  }
}
