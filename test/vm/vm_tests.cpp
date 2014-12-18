/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cstring>
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
      
      void VirtualMachineTests::test_vm_executes_reference_instructions()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM('a'), NA());
        ARG(ILOAD, IMM('b'), NA());
        ARG(ILOAD, IMM('c'), NA());
        LET(RIARRAY8, NA(), NA());
        ARG(ILOAD, IMM('d'), NA());
        ARG(ILOAD, IMM('f'), NA());
        LET(RIARRAY8, NA(), NA());
        IN();
        LET(RIACAT8, LV(0), LV(1));
        IN();
        ARG(ILOAD, IMM(1), NA());
        ARG(RLOAD, LV(0), NA());
        ARG(RLOAD, LV(1), NA());
        ARG(RLOAD, LV(2), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RIANTH8, LV(2), IMM(3));
        ARG(RIALEN8, LV(0), NA());
        RET(RTUPLE, NA(), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (7 == value.r()->length());
          is_expected &= (Value(1) == value.r()->elem(0));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(1).type());
          Reference ref1(value.r()->elem(1).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref1->type());
          is_expected &= (3 == ref1->length());
          is_expected &= (strncmp("abc", reinterpret_cast<char *>(ref1->raw().is8), 3) == 0);
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(2).type());
          Reference ref2(value.r()->elem(2).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref2->type());
          is_expected &= (2 == ref2->length());
          is_expected &= (strncmp("df", reinterpret_cast<char *>(ref2->raw().is8), 2) == 0);
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(3).type());
          Reference ref3(value.r()->elem(3).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref3->type());
          is_expected &= (5 == ref3->length());
          is_expected &= (strncmp("abcdf", reinterpret_cast<char *>(ref3->raw().is8), 5) == 0);
          is_expected &= (Value(2) == value.r()->elem(4));
          is_expected &= (Value('d') == value.r()->elem(5));
          is_expected &= (Value(3) == value.r()->elem(6));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_gets_global_variables()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(RLOAD, GV(0), NA());
        ARG(ILOAD, GV(1), NA());
        ARG(FLOAD, GV(2), NA());
        ARG(RLOAD, GV(3), NA());
        RET(RTUPLE, NA(), NA());
        END_FUN();
        VAR_R(0);
        VAR_I(11);
        VAR_F(0.5);
        VAR_R(14);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c'); I('d'); I('e'); I('f');
        END_OBJECT();
        OBJECT(SFARRAY);
        F(0.1); F(0.2); F(0.4);
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (4 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1(value.r()->elem(0).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref1->type());
          is_expected &= (6 == ref1->length());
          is_expected &= (strncmp("abcdef", reinterpret_cast<char *>(ref1->raw().is8), 6) == 0);
          is_expected &= (Value(11) == value.r()->elem(1));
          is_expected &= (Value(0.5) == value.r()->elem(2));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(3).type());
          Reference ref2(value.r()->elem(3).r());
          is_expected &= (OBJECT_TYPE_SFARRAY == ref2->type());
          is_expected &= (3 == ref2->length());
          is_expected &= (0.1f == ref2->raw().sfs[0]);
          is_expected &= (0.2f == ref2->raw().sfs[1]);
          is_expected &= (0.4f == ref2->raw().sfs[2]);
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_executes_jumps()
      {
        PROG(prog_helper, 0);
        FUN(4);
        JC(A(0), 4);
        LET(IADD, A(2), IMM(10)); // 2 + 10 = 12
        IN();
        LET(IMUL, A(3), IMM(5)); // 4 * 5 = 20
        JUMP(3);
        LET(IADD, A(2), IMM(5)); // 2 + 5 = 7
        IN();
        LET(IMUL, A(3), IMM(10)); // 4 * 10 = 40
        IN();
        JC(A(1), 2);
        LET(IADD, LV(0), LV(1)); // 12 + 20 = 32 or 7 + 40 = 47
        JUMP(1);
        LET(IMUL, LV(0), LV(1)); // 12 * 20 = 240 or 7 * 40 = 280
        IN();
        RET(ILOAD, LV(2), NA()); // 47
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(1));
        args.push_back(Value(0));
        args.push_back(Value(2));
        args.push_back(Value(4));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (47 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_invokes_functions()
      {
        PROG(prog_helper, 2);
        FUN(2);
        LET(IADD, A(0), A(1)); // 2 + 3 = 5
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(1), NA());
        LET(ICALL, IMM(3), NA()); // 2 * 3 = 6
        IN();
        RET(IADD, LV(0), LV(1)); // 5 + 6 = 11
        END_FUN();
        FUN(2);
        LET(RIACAT8, A(0), A(1)); // "abc" + "def" = "abcdef"
        ARG(RLOAD, A(0), NA());
        ARG(RLOAD, A(1), NA());
        LET(RCALL, IMM(4), NA()); // "def" + "abc" = "defabc"
        IN();
        RET(RIACAT8, LV(0), LV(1)); // "abcdef" + "defabc" = "abcdefdefabc"
        END_FUN();
        FUN(0);
        ARG(ILOAD, IMM(2), NA());
        ARG(ILOAD, IMM(3), NA());
        LET(ICALL, IMM(0), NA());
        ARG(RLOAD, GV(0), NA());
        ARG(RLOAD, GV(1), NA());
        LET(RCALL, IMM(1), NA());
        IN();
        ARG(ILOAD, LV(0), NA());
        ARG(RLOAD, LV(1), NA());
        RET(RTUPLE, NA(), NA()); // (30, "abcdefdefabc")
        END_FUN();
        FUN(2);
        RET(IMUL, A(0), A(1));
        END_FUN();
        FUN(2);
        RET(RIACAT8, A(1), A(0));
        END_FUN();
        VAR_R(0);
        VAR_R(11);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c');
        END_OBJECT();
        OBJECT(IARRAY8);
        I('d'); I('e'); I('f');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (Value(11) == value.r()->elem(0));
          Reference ref1(value.r()->elem(1).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref1->type());
          is_expected &= (12 == ref1->length());
          is_expected &= (strncmp("abcdefdefabc", reinterpret_cast<char *>(ref1->raw().is8), 12) == 0);
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_recursion()
      {
        PROG(prog_helper, 0);
        FUN(1);
        LET(ILE, A(0), IMM(1));
        IN();
        JC(LV(0), 6);
        ARG(ISUB, A(0), IMM(1));
        LET(ICALL, IMM(0), NA());
        ARG(ISUB, A(0), IMM(2));
        LET(ICALL, IMM(0), NA());
        IN();
        RET(IADD, LV(1), LV(2));
        RET(ILOAD, A(0), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(10));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (55 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_tail_recursion()
      {
        PROG(prog_helper, 0);
        FUN(3);
        LET(IGT, A(1), A(0));
        IN();
        JC(LV(0), 4);
        ARG(ILOAD, A(0), NA());
        ARG(IADD, A(1), IMM(1));
        ARG(IMUL, A(2), A(1));
        RETRY();
        RET(ILOAD, A(2), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(10));
        args.push_back(Value(1));
        args.push_back(Value(1));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (3628800 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_many_threads()
      {
        PROG(prog_helper, 0);
        FUN(3);
        LET(IGT, A(1), A(0));
        IN();
        JC(LV(0), 4);
        ARG(ILOAD, A(0), NA());
        ARG(IADD, A(1), IMM(1));
        ARG(IADD, A(2), A(1));
        RETRY();
        RET(ILOAD, A(2), NA());
        END_FUN();
        FUN(3);
        LET(IGT, A(1), A(0));
        IN();
        JC(LV(0), 4);
        ARG(ILOAD, A(0), NA());
        ARG(IADD, A(1), IMM(1));
        ARG(ISUB, A(2), A(1));
        RETRY();
        RET(ILOAD, A(2), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success1 = false;
        bool is_expected1 = false;
        vector<Value> args1;
        args1.push_back(Value(7500));
        args1.push_back(Value(1));
        args1.push_back(Value(0));
        Thread thread1 = _M_vm->start(0, args1, [&is_success1, &is_expected1](const ReturnValue &value) {
          is_success1 = (ERROR_SUCCESS == value.error());
          is_expected1 = (28128750 == value.i());
        });
        bool is_success2 = false;
        bool is_expected2 = false;
        vector<Value> args2;
        args2.push_back(Value(10000));
        args2.push_back(Value(1));
        args2.push_back(Value(0));
        Thread thread2 = _M_vm->start(1, args2, [&is_success2, &is_expected2](const ReturnValue &value) {
          is_success2 = (ERROR_SUCCESS == value.error());
          is_expected2 = (-50005000 == value.i());
        });
        bool is_success3 = false;
        bool is_expected3 = false;
        vector<Value> args3;
        args3.push_back(Value(5000));
        args3.push_back(Value(1));
        args3.push_back(Value(0));
        Thread thread3 = _M_vm->start(0, args3, [&is_success3, &is_expected3](const ReturnValue &value) {
          is_success3 = (ERROR_SUCCESS == value.error());
          is_expected3 = (12502500 == value.i());
        });
        thread1.system_thread().join();
        thread2.system_thread().join();
        thread3.system_thread().join();
        CPPUNIT_ASSERT(is_success1);
        CPPUNIT_ASSERT(is_expected1);
        CPPUNIT_ASSERT(is_success2);
        CPPUNIT_ASSERT(is_expected2);
        CPPUNIT_ASSERT(is_success3);
        CPPUNIT_ASSERT(is_expected3);
      }

      DEF_IMPL_VM_TESTS(InterpreterVirtualMachine);
    }
  }
}
