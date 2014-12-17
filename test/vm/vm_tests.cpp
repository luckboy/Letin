/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include "impl_loader.hpp"
#include "interp_vm.hpp"
#include "mark_sweep_gc.hpp"
#include "new_alloc.hpp"
#include "vm_tests.hpp"
#include "helper.hpp"

using namespace std;
using namespace letin::vm;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      void VirtualMachineTests::setUp()
      {
        _M_loader = new impl::ImplLoader();
        _M_alloc = new impl::NewAllocator();
        _M_gc = new impl::MarkSweepGarbageCollector(_M_alloc);
        _M_vm = new_vm(_M_loader, _M_gc);
      }
      
      void VirtualMachineTests::tearDown()
      {
        delete _M_vm;
        delete _M_gc;
        delete _M_alloc;
        delete _M_loader;
      }

      void VirtualMachineTests::test_vm_executes_simple_program()
      {
        PROG(prog_helper, 0);
        FUN(0);
        RET(IADD, IMM(2), IMM(2));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_four = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_four](const ReturnValue &value) {
          is_four = (4 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_four);
      }
      
      void VirtualMachineTests::test_vm_executes_lets_and_ins()
      {
        PROG(prog_helper, 0);
        FUN(4); // (10, 5, 2, 8) 
        LET(IADD, A(0), A(1)); // 10 + 5 = 15
        LET(IMUL, A(2), A(2)); // 2 * 2 = 4
        IN();
        LET(ISUB, LV(0), LV(1)); // 15 - 4 = 11
        LET(IDIV, A(3), IMM(2)); // 8 / 2 = 4
        IN();
        LET(IADD, LV(3), IMM(10)); // 4 + 10 = 14
        IN();
        RET(IMUL, LV(3), LV(4)); // 4 * 14 = 56
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(10));
        args.push_back(Value(5));
        args.push_back(Value(2));
        args.push_back(Value(8));
        Thread thread = _M_vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (56 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      DEF_IMPL_VM_TESTS(InterpreterVirtualMachine);
    }
  }
}
