/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _PATH_UTIL_HPP
#define _PATH_UTIL_HPP

#include <algorithm>
#include <string>

namespace letin
{
  namespace util
  {
#if defined(_WIN32) || defined(_WIN64)
    const char PATH_SEP = '\\';
#else
    const char PATH_SEP = '/';
#endif

    static inline std::string unix_path_to_path(const std::string &path_name)
    {
      std::string system_path_name(path_name);
      std::replace(system_path_name.begin(), system_path_name.end(), '/', PATH_SEP);
      return system_path_name;
    }
  }
}

#endif
