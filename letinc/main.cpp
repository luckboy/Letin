/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <vector>
#include <unistd.h>
#include <letin/comp.hpp>

using namespace std;
using namespace letin::comp;

int main(int argc, char **argv)
{
  const char *output_file_name = nullptr;
  int c;
  while((c = getopt(argc, argv, "o:")) != -1) {
    switch(c) {
      case 'o':
        output_file_name = optarg;
        break;
      default:
        cerr << "Usage: " << argv[0] << " -o <output file> [<source file> ...]" << endl;
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
  unique_ptr<Program> prog(comp->compile(sources, errors));
  if(prog.get() == nullptr) {
    for(auto error : errors) cerr << error << endl;
    return 1;
  }
  ofstream ofs(output_file_name);
  if(!ofs.good()) {
    cerr << "can't create output file" << endl;
    return 1;
  }
  ofs.write(reinterpret_cast<const char *>(prog->ptr()), prog->size());
  return 0;
}
