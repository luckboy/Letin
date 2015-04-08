/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <climits>
#include <cstring>
#include <dirent.h>
#include <string>
#include <memory>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
#endif
#include "fs_util.hpp"
#include "path_util.hpp"

using namespace std;
using namespace letin::util;

namespace letin
{
  namespace comp
  {
    namespace test
    {
      bool make_dir(const char *dir_name) 
      {
#if defined(_WIN32) || defined(_WIN64)
        return _mkdir(dir_name);
#else
        return mkdir(dir_name, 0777) != -1;
#endif
      }

      bool list_file_paths(const char *dir_name, vector<string> &paths)
      {
        DIR *dir = opendir(dir_name);
        if(dir == nullptr) return false;
        struct dirent *dirent;
        while((dirent = readdir(dir)) != nullptr) {
          if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) continue;
          paths.push_back(string(dir_name) + PATH_SEP + string(dirent->d_name));
        }
        closedir(dir);
        return true;
      }

      bool remove_file(const char *file_name)
      {
#if defined(_WIN32) || defined(_WIN64)
        return _unlink(file_name) != 1;
#else
        return unlink(file_name) != -1;
#endif        
      }

      bool remove_dir(const char *dir_name)
      {
#if defined(_WIN32) || defined(_WIN64)
        return _rmdir(dir_name) != -1;
#else
        return rmdir(dir_name) != -1;
#endif
      }

      bool change_dir(const char *dir_name)
      {
#if defined(_WIN32) || defined(_WIN64)
        return _chdir(dir_name) != -1;
#else
        return chdir(dir_name) != -1;
#endif
      }

      bool get_current_dir(string &str)
      {
        unique_ptr<char []> buf(new char[PATH_MAX + 1]);
#if defined(_WIN32) || defined(_WIN64)
        if(_getcwd(buf.get(), PATH_MAX + 1) == nullptr) return false;
#else
        if(getcwd(buf.get(), PATH_MAX + 1) == nullptr) return false;
#endif
        str.clear();
        str += buf.get();
        return true;
      }
    }
  }
}
