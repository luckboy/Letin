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
#include "posix.hpp"

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
          min_system_error = max(min_system_error, system_error);
          max_system_error = max(max_system_error, system_error);
        }
        errors.resize(max_system_error - min_system_error + 1);
        error_offset = min_system_error;
        fill(errors.begin(), errors.end(), -1);
        for(int i = 0; i < static_cast<int>(system_errors.size()); i++) {
          errors[system_errors[i] - min_system_error] = i;
        }
      }

      int64_t system_error_to_error(int system_error)
      {
#if EAGAIN != EWOULDBLOCK
        if(system_error == EWOULDBLOCK) system_error = EAGAIN;
#endif
#if ENOTSUP != EOPNOTSUPP
        if(system_error == EOPNOTSUPP) system_error = ENOTSUP;
#endif
        if(system_error >= static_cast<int>(error_offset) && system_error < static_cast<int>(errors.size()))
          return errors[system_error - error_offset];
        else
          return -1;
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
