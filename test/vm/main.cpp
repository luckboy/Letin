/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <iostream>
#include <letin/vm.hpp>

using namespace std;
using namespace letin::vm;

int main()
{
  cout << "Testing letinvm library ..." << endl;
  initialize_vm();
  CppUnit::TextUi::TestRunner runner;  
  runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
  int status = (runner.run() ? 0 : 1);
  finalize_vm();
  return status;
}
