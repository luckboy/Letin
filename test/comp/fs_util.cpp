/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <dirent.h>
#include <limits>
#include <memory>
#include <unistd.h>
#include "fs_util.hpp"

using namespace std;

namespace letin
{
  namespace comp
  {
    namespace test
    {
      bool make_dir(const char *dir_name) { return mkdir(dir_name, 0777) != -1; }

      bool list_file_paths(const char *dir_name, vector<string> &paths)
      {
        DIR *dir = opendir(dir_name);
        if(dir == nullptr) return false;
        struct dirent *dirent;
        while((dirent = readdir(dir)) != nullptr) {
          if(strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) continue;
          paths.push_back(string(dir_name) + "/" + string(dirent->d_name));
        }
        closedir(dir);
        return true;
      }

      bool remove_file(const char *file_name) { return unlink(file_name) != -1; }

      bool remove_dir(const char *dir_name) { return rmdir(dir_name) != -1; }

      bool change_dir(const char *dir_name) { return chdir(dir_name) != -1; }
      
      bool get_current_dir(string &str)
      {
        unique_ptr<char []> buf(new char[PATH_MAX + 1]);
        if(getcwd(buf.get(), PATH_MAX + 1) == nullptr) return false;
        str.clear();
        str += buf.get();
        return true;
      }
    }
  }
}
