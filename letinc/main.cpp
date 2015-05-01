/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <new>
#include <vector>
#include <unistd.h>
#include <letin/comp.hpp>

using namespace std;
using namespace letin::comp;

int main(int argc, char **argv)
{
  try {
    const char *output_file_name = nullptr;
    list<string> include_dirs;
    int c;
    bool is_relocable = true;
    opterr = 0;
    while((c = getopt(argc, argv, "hI:o:s")) != -1) {
      switch(c) {
        case 'h':
          cout << "Usage: " << argv[0] << " [<option> ...] [<source file> ...]" << endl;
          cout << endl;
          cout << "Options:" << endl;
          cout << "  -h                    display this text" << endl;
          cout << "  -I <directory>        add the directory to include directories" << endl;
          cout << "  -o <file>             specify an output file" << endl;
          cout << "  -s                    generate an unrelocable output file" << endl;
          return 0;
        case 'I':
          include_dirs.push_back(string(optarg));
          break;
        case 'o':
          output_file_name = optarg;
          break;
        case 's':
          is_relocable = false;
          break;
        default:
          cerr << "incorrect option -" << static_cast<char>(optopt) << endl;
          return 1;
      }
    }
    if(output_file_name == nullptr) {
      cerr << "no output file" << endl;
      return 1;
    }
    vector<Source> sources;
    if(optind < argc)
      for(int i = optind; i < argc; i++) sources.push_back(Source(argv[i]));
    else
      sources.push_back(Source());
    list<Error> errors;
    unique_ptr<Compiler> comp(new_compiler());
    for(auto include_dir : include_dirs) comp->add_include_dir(include_dir);
    unique_ptr<Program> prog(comp->compile(sources, errors, is_relocable));
    if(prog.get() == nullptr) {
      for(auto error : errors) cerr << error << endl;
      return 1;
    }
    ofstream ofs(output_file_name, ofstream::out | ofstream::binary);
    if(!ofs.good()) {
      cerr << "can't create output file" << endl;
      return 1;
    }
    ofs.write(reinterpret_cast<const char *>(prog->ptr()), prog->size());
    return 0;
  } catch(bad_alloc &) {
    cerr << "out of memory" << endl;
    return 1;
  }
}
