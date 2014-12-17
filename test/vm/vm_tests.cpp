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
        bool is_success = false;
        bool is_four = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_four](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_four = (4 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
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
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(10));
        args.push_back(Value(5));
        args.push_back(Value(2));
        args.push_back(Value(8));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (56 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_executes_int_instructions()
      {
        PROG(prog_helper, 0);
        FUN(3);
        LET(IADD, A(0), A(1)); // 9 + 10 = 19
        LET(ISUB, A(1), A(2)); // 10 - 8 = 2
        LET(IMUL, A(2), A(0)); // 8 * 9 = 72
        IN();
        LET(IADD, LV(0), LV(1)); // 19 + 2 = 21
        IN();
        RET(IADD, LV(3), LV(2)); // 21 + 72 = 93
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(9));
        args.push_back(Value(10));
        args.push_back(Value(8));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (93 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_float_instructions()
      {
        PROG(prog_helper, 0);
        FUN(6);
        LET(FADD, A(0), A(1)); // 0.5 + 1.0 = 1.5
        LET(FSUB, A(2), A(3)); // 1.25 - 1.5 = -0.25
        LET(FMUL, A(4), A(5)); // 1.75 * 2 = 3.5
        IN();
        LET(FADD, LV(0), LV(1)); // 1.5 + (-0.25) = 1.25
        IN();
        LET(FMUL, LV(3), LV(2)); // 1.25 * 3.5 = 4.375
        IN();
        RET(FADD, LV(4), IMM(0.5f)); // 4.375 + 0.5 = 4.875
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(0.5));
        args.push_back(Value(1.0));
        args.push_back(Value(1.25));
        args.push_back(Value(1.5));
        args.push_back(Value(1.75));
        args.push_back(Value(2.0));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (4.875 == value.f());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      DEF_IMPL_VM_TESTS(InterpreterVirtualMachine);
    }
  }
}
