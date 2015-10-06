/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _DIR_HPP
#define _DIR_HPP

#include <sys/types.h>
#include <string>

namespace letin
{
  namespace nlib
  {
    namespace posix
    {
      class Directory;

      class DirectoryEntry;

      Directory *open_dir(const char *dir_name);

      bool close_dir(Directory *dir);

      bool read_dir(Directory *dir, DirectoryEntry *entry, DirectoryEntry *&result);

      void rewind_dir(Directory *dir);

      void seek_dir(Directory *dir, long loc);

      long tell_dir(Directory *dir);

      DirectoryEntry *new_directory_entry();

      void delete_directory_entry(DirectoryEntry *dir_entry);

      ::ino_t dir_entry_inode(const DirectoryEntry *entry);

      const char *dir_entry_name(const DirectoryEntry *entry);
    }
  }
}

#endif
