/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _FS_UTIL_HPP
#define _FS_UTIL_HPP

#include <string>
#include <vector>

namespace letin
{
  namespace comp
  {
    namespace test
    {
      bool make_dir(const char *dir_name);

      bool list_file_paths(const char *dir_name, std::vector<std::string> &paths);

      std::string concatenate_path(const char *dir_name, const char *name);

      bool remove_file(const char *file_name);

      bool remove_dir(const char *dir_name);
    }
  }
}

#endif
