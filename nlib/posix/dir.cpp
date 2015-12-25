/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#if defined(__unix__)
#include <cerrno>
#include <dirent.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdint>
#include <io.h>
#include <memory>
#include <mutex>
#else
#error "Unsupported operating system."
#endif
#include "dir.hpp"

using namespace std;

namespace letin
{
  namespace nlib
  {
    namespace posix
    {
#if defined(__unix__)

      //
      // An implementation for Unix-like systems.
      //

      class Directory {};

      class DirectoryEntry {};

      Directory *open_dir(const char *dir_name)
      { return reinterpret_cast<Directory *>(::opendir(dir_name)); }

      bool close_dir(Directory *dir)
      { return closedir(reinterpret_cast<::DIR *>(dir)) != -1; }

      bool read_dir(Directory *dir, DirectoryEntry *entry, DirectoryEntry *&result)
      {
        int error = ::readdir_r(reinterpret_cast<::DIR *>(dir), reinterpret_cast<struct ::dirent *>(entry), reinterpret_cast<::dirent **>(&result));
        if(error != 0) {
          errno = error;
          return false;
        }
        return true;
      }

      void rewind_dir(Directory *dir)
      { ::rewinddir(reinterpret_cast<::DIR *>(dir)); }

      void seek_dir(Directory *dir, long loc)
      { ::seekdir(reinterpret_cast<::DIR *>(dir), loc); }

      long tell_dir(Directory *dir)
      { return ::telldir(reinterpret_cast<::DIR *>(dir)); }

      DirectoryEntry *new_directory_entry()
      { return reinterpret_cast<DirectoryEntry *>(new struct ::dirent); }

      void delete_directory_entry(DirectoryEntry *dir_entry)
      { delete reinterpret_cast<struct ::dirent *>(dir_entry); }

      ::ino_t dir_entry_inode(const DirectoryEntry *entry)
      { return reinterpret_cast<const struct ::dirent *>(entry)->d_ino; }

      const char *dir_entry_name(const DirectoryEntry *entry)
      { return reinterpret_cast<const struct ::dirent *>(entry)->d_name; }

#elif defined(_WIN32) || defined(_WIN64)

      //
      // An implementation for Windows.
      //

      class Directory
      {
      public:
        recursive_mutex mutex;
        string file_spec;
        intptr_t handle;
        struct ::_finddata_t buffer;
        bool has_entry;
        long loc;
      };

      class DirectoryEntry {};

      static bool reinitialize_dir(Directory *dir)
      {
        int saved_errno = errno;
        if(dir->handle != -1) ::_findclose(dir->handle);
        dir->handle = ::_findfirst(dir->file_spec.c_str(), &(dir->buffer));
        dir->has_entry = (dir->handle != -1);
        dir->loc = 0;
        if(dir->handle == -1 && errno != ENOENT) return false;
        errno = saved_errno;
        return true;
      }

      Directory *open_dir(const char *dir_name)
      {
        struct stat status;
        if(_stat(dir_name, &status) == -1) return nullptr;
        if(_S_ISDIR(status.st_mode) == 0) {
          errno = ENOTDIR;
          return nullptr;
        }
        unique_ptr<Directory> dir(new Directory());
        dir->file_spec = string(dir_name) + "\\*";
        dir->handle = -1;
        if(!reinitialize_dir(dir.get())) return nullptr;
        return dir.release();
      }

      bool close_dir(Directory *dir)
      {
        bool is_success = true;
        {
          lock_guard<recursive_mutex> guard(dir->mutex);
          if(dir->handle != -1) is_success = (::_findclose(dir->handle) == -1);
        }
        delete dir;
        return is_success;
      }

      bool read_dir(Directory *dir, DirectoryEntry *entry, DirectoryEntry *&result)
      {
        lock_guard<recursive_mutex> guard(dir->mutex);
        int saved_errno = errno;
        if(!dir->has_entry) {
          if(dir->handle != -1) {
            if(::_findnext(dir->handle, &(dir->buffer)) == -1) {
              if(errno == ENOENT) {
                errno = 0;
                return false;
              }
              result = nullptr;
            }
          }
        }
        dir->has_entry = false;
        dir->loc++;
        struct ::_finddata_t *file_info = reinterpret_cast<struct ::_finddata_t *>(entry);
        *file_info = dir->buffer;
        result = entry;
        errno = saved_errno;
        return true;
      }

      void rewind_dir(Directory *dir)
      {
        lock_guard<recursive_mutex> guard(dir->mutex);
        int saved_errno = errno;
        reinitialize_dir(dir);
        errno = saved_errno;
      }

      void seek_dir(Directory *dir, long loc)
      {
        lock_guard<recursive_mutex> guard(dir->mutex);
        if(dir->loc > loc) rewind_dir(dir);
        while(dir->loc < loc) {
          int saved_errno = errno;
          unique_ptr<DirectoryEntry> entry;
          DirectoryEntry *result;
          if(!read_dir(dir, entry.get(), result)) {
            errno = saved_errno;
            return;
          }
          if(result == nullptr) {
            errno = saved_errno;
            return;
          }
          errno = saved_errno;
        }
      }

      long tell_dir(Directory *dir)
      {
        lock_guard<recursive_mutex> guard(dir->mutex);
        return dir->loc;
      }

      DirectoryEntry *new_directory_entry()
      { return reinterpret_cast<DirectoryEntry *>(new struct ::_finddata_t); }

      void delete_directory_entry(DirectoryEntry *dir_entry)
      { delete reinterpret_cast<struct ::_finddata_t *>(dir_entry); }

      ::ino_t dir_entry_inode(const DirectoryEntry *entry) { return 0; }

      const char *dir_entry_name(const DirectoryEntry *entry)
      { return reinterpret_cast<const struct ::_finddata_t *>(entry)->name; }

#else
#error "Unsupported operating system."
#endif
    }
  }
}
