/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <cstring>
#include "impl_loader.hpp"
#include "eager_eval_strategy.hpp"
#include "ht_memo_cache.hpp"
#include "interp_vm.hpp"
#include "lazy_eval_strategy.hpp"
#include "mark_sweep_gc.hpp"
#include "memo_eval_strategy.hpp"
#include "memo_lazy_eval_strategy.hpp"
#include "new_alloc.hpp"
#include "util.hpp"
#include "vm_tests.hpp"
#include "helper.hpp"

using namespace std;
using namespace letin::vm;
using namespace letin::util;

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
        _M_native_fun_handler = new DefaultNativeFunctionHandler();
        _M_memo_cache_factory = new_memo_cache_factory();
        _M_eval_strategy = new_eval_strategy();
        _M_vm = new_vm(_M_loader, _M_gc, _M_native_fun_handler, _M_eval_strategy);
      }
      
      void VirtualMachineTests::tearDown()
      {
        delete _M_vm;
        delete _M_eval_strategy;
        if(_M_memo_cache_factory != nullptr) delete _M_memo_cache_factory;
        delete _M_native_fun_handler;
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
        VAR_R(16);
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
        VAR_R(16);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c');
        END_OBJECT();
        OBJECT(IARRAY8);
        I('d'); I('e'); I('f');
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
          is_expected &= (2 == value.r()->length());
          is_expected &= (Value(11) == value.r()->elem(0));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(1).type());
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

      void VirtualMachineTests::test_vm_complains_on_non_existent_local_variable()
      {
        PROG(prog_helper, 0);
        FUN(2);
        LET(IADD, A(0), A(1));
        LET(IMUL, A(0), A(1));
        IN();
        LET(IADD, LV(0), LV(1));
        LET(IADD, LV(1), LV(2));
        RET(ILOAD, LV(2), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        vector<Value> args;
        args.push_back(Value(1));
        args.push_back(Value(2));
        Thread thread = _M_vm->start(args, [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_NO_LOCAL_VAR == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_non_existent_argument()
      {
        PROG(prog_helper, 0);
        FUN(2);
        LET(IADD, A(0), A(1));
        LET(IMUL, A(1), A(2));
        LET(IDIV, A(2), A(0));
        IN();
        RET(IADD, LV(0), LV(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        vector<Value> args;
        args.push_back(Value(1));
        args.push_back(Value(2));
        Thread thread = _M_vm->start(args, [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_NO_ARG == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_division_by_zero()
      {
        PROG(prog_helper, 0);
        FUN(2);
        RET(IDIV, A(0), A(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        vector<Value> args;
        args.push_back(Value(10));
        args.push_back(Value(0));
        Thread thread = _M_vm->start(args, [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_DIV_BY_ZERO == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_incorrect_number_of_arguments()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(ILOAD, IMM(3), NA());
        LET(ICALL, IMM(1), NA());
        RET(IADD, LV(0), IMM(10));
        END_FUN();
        FUN(2);
        RET(IADD, A(0), A(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_INCORRECT_ARG_COUNT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }
      
      void VirtualMachineTests::test_vm_complains_on_non_existent_global_variable()
      {
        PROG(prog_helper, 0);
        FUN(0);
        RET(ILOAD, GV(10), NA());
        END_FUN();
        VAR_I(10);
        VAR_F(0.25);
        VAR_R(0);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c'); I('d'); I('e'); I('f');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_NO_GLOBAL_VAR == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_executes_load2_instructions()
      {
        double x = 0.123456789;
        format::Double format_x = double_to_format_double(x);
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD2, IMM(0x11223344), IMM(0x55667788));
        ARG(FLOAD2, IMM(static_cast<int32_t>(format_x.dword >> 32)), IMM(static_cast<int32_t>(format_x.dword & 0xffffffffL)));
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
          is_expected &= (2 == value.r()->length());
          is_expected &= (Value(static_cast<int64_t>(0x1122334455667788LL)) == value.r()->elem(0));
          is_expected &= (Value(0.123456789) == value.r()->elem(1));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_executes_instructions_for_unique_objects()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(RUIAFILL8, IMM(4), IMM('1'));
        IN();
        ARG(ILOAD, IMM('a'), NA());
        LET(RUIASNTH8, LV(0), IMM(0));
        IN();
        ARG(ILOAD, IMM('b'), NA());
        LET(RUIASNTH8, LV(1), IMM(1));
        IN();
        ARG(ILOAD, IMM('c'), NA());
        LET(RUIASNTH8, LV(2), IMM(2));
        IN();
        LET(RUIAFILL8, IMM(3), IMM('2'));
        IN();
        ARG(ILOAD, IMM('d'), NA());
        LET(RUIASNTH8, LV(4), IMM(0));
        IN();
        ARG(ILOAD, IMM('e'), NA());
        LET(RUIASNTH8, LV(5), IMM(1));
        IN();
        LET(RUTFILLI, IMM(3), IMM(0));
        IN();
        ARG(RLOAD, LV(3), NA());
        LET(RUTSNTH, LV(7), IMM(0));
        IN();
        ARG(RLOAD, LV(6), NA());
        RET(RUTSNTH, LV(8), IMM(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          is_expected &= ((OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE) == value.r()->elem(0).r()->type());
          is_expected &= (4 == value.r()->elem(0).r()->length());
          is_expected &= (Value('a') == value.r()->elem(0).r()->elem(0));
          is_expected &= (Value('b') == value.r()->elem(0).r()->elem(1));
          is_expected &= (Value('c') == value.r()->elem(0).r()->elem(2));
          is_expected &= (Value('1') == value.r()->elem(0).r()->elem(3));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(1).type());
          is_expected &= ((OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE) == value.r()->elem(1).r()->type());
          is_expected &= (3 == value.r()->elem(1).r()->length());
          is_expected &= (Value('d') == value.r()->elem(1).r()->elem(0));
          is_expected &= (Value('e') == value.r()->elem(1).r()->elem(1));
          is_expected &= (Value('2') == value.r()->elem(1).r()->elem(2));
          is_expected &= (Value(0) == value.r()->elem(2));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);        
      }

      void VirtualMachineTests::test_vm_executes_lettuples()
      {
        PROG(prog_helper, 0);
        FUN(3);
        LET(RUIAFILL32, IMM(4), A(0)); // a0
        IN();
        ARG(IADD, A(0), IMM(10)); // a0 + 10
        LET(RUIASNTH32, LV(0), IMM(0));
        IN();
        ARG(IADD, A(0), IMM(20)); // a0 + 20
        LET(RUIASNTH32, LV(1), IMM(1));
        IN();
        ARG(IADD, A(0), IMM(30)); // a0 + 30
        LET(RUIASNTH32, LV(2), IMM(2));
        IN();
        LETTUPLE(RUIANTH32, 2, LV(3), A(1));
        IN();
        LETTUPLE(RUIANTH32, 2, LV(5), A(2));
        IN();
        LET(ILOAD, LV(4), NA());
        IN();
        RET(IMUL, LV(6), LV(8));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(5));
        args.push_back(Value(0));
        args.push_back(Value(3));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (75 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);        
      }

      void VirtualMachineTests::test_vm_executes_lettuples_for_shared_tuples()
      {
        PROG(prog_helper, 0);
        FUN(3);
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(1), NA());
        ARG(ILOAD, A(2), NA());
        LETTUPLE(RTUPLE, 3, NA(), NA());
        IN();
        ARG(IADD, LV(0), LV(1)); // 1 + 2 = 3
        ARG(IADD, LV(1), LV(2)); // 2 + 3 = 5
        LETTUPLE(RTUPLE, 2, NA(), NA());
        IN();
        RET(IMUL, LV(3), LV(4)); // 3 * 5 = 15
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(1));
        args.push_back(Value(2));
        args.push_back(Value(3));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (15 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_complains_on_many_references_to_unique_object()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(RUIAFILL32, IMM(4), IMM(0));
        IN();
        LET(RLOAD, LV(0), NA());
        LET(ILOAD, IMM(10), NA());
        IN();
        RET(RLOAD, LV(0), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_AGAIN_USED_UNIQUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }
      
      void VirtualMachineTests::test_vm_complains_on_unique_object()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM(0), NA());
        ARG(RUIAFILL8, IMM(10), IMM(' '));
        ARG(ILOAD, IMM(0), NA());
        RET(RTUPLE, NA(), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_UNIQUE_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_incorrect_local_var_count()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(ILOAD, IMM(3), NA());
        ARG(ILOAD, IMM(4), NA());
        LETTUPLE(RTUPLE, 3, NA(), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_loads_program_with_many_libraries()
      {
        LIB(prog_helper1);
        FUN(1);
        RET(ILOAD, IMM(1), NA());
        END_FUN();
        FUN(2);
        RET(ILOAD, IMM(2), NA());
        END_FUN();
        VAR_I(1);
        VAR_I(2);
        SYMBOL_DF("f1", 0);
        SYMBOL_DF("f2", 1);
        SYMBOL_DV("v1", 0);
        SYMBOL_DV("v2", 1);
        END_LIB();

        PROG(prog_helper2, 6);
        FUN(3);
        RET(IADD, IMM(1), IMM(2));
        END_FUN();
        FUN(4);
        RET(IADD, IMM(0), GV(0));
        END_FUN();
        FUN(5);
        RET(ISUB, GV(0), IMM(0));
        END_FUN();
        FUN(6);
        RET(IMUL, GV(0), GV(0));
        END_FUN();
        FUN(7);
        RET(IMUL, IMM(0), IMM(0));
        END_FUN();
        FUN(8);
        RET(IADD, IMM(0), GV(0));
        END_FUN();
        FUN(9);
        RET(ILOAD, IMM(1234), NA());
        END_FUN();
        VAR_R(0);
        VAR_R(24);
        VAR_R(56);
        OBJECT(IARRAY32);
        I(0); I(0); I(0);
        END_OBJECT();
        OBJECT(IARRAY64);
        I(0); I(1); I(0);
        END_OBJECT();
        OBJECT(TUPLE);
        I(0); I(0); I(2); I(0);
        END_OBJECT();
        RELOC_A1F(0);
        RELOC_A2F(0);
        RELOC_SA1F(1, 0);
        RELOC_SA2V(1, 1);
        RELOC_SA1V(2, 2);
        RELOC_SA2F(2, 3);
        RELOC_SA1V(3, 4);
        RELOC_SA2V(3, 5);
        RELOC_SA1F(4, 6);   
        RELOC_SA2F(4, 7);
        RELOC_SA1F(5, 8);
        RELOC_SA2V(5, 9);
        RELOC_EF(8);
        RELOC_SEF(12, 0); 
        RELOC_SEF(16, 3);
        RELOC_SEF(32, 0);
        RELOC_EF(40);
        RELOC_SEF(48, 3);
        RELOC_SEF(64, 0);  
        RELOC_SEF(72, 3);
        RELOC_EF(80);
        RELOC_SEF(88, 6);
        SYMBOL_UF("f1");
        SYMBOL_UV("v1");
        SYMBOL_UV("v2");
        SYMBOL_UF("f2");
        SYMBOL_UV("v3");
        SYMBOL_UV("v4");
        SYMBOL_UF("f3");
        SYMBOL_UF("f4");
        SYMBOL_UF("f5");
        SYMBOL_UV("v5");
        SYMBOL_DF("f6", 2);
        SYMBOL_DV("v6", 1);
        END_PROG();

        LIB(prog_helper3);
        FUN(10);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        FUN(11);
        RET(RLOAD, GV(0), NA());
        END_FUN();
        FUN(12);
        RET(ILOAD, IMM(5), NA());
        END_FUN();
        VAR_I(3);
        VAR_I(4);
        VAR_I(5);
        VAR_I(1);
        VAR_I(0);
        RELOC_SA1F(0, 6);
        RELOC_SA1V(1, 7);
        RELOC_VF(3);
        RELOC_SVF(4, 6);
        SYMBOL_DF("f3", 0);
        SYMBOL_DF("f4", 1);
        SYMBOL_DF("f5", 2);
        SYMBOL_DV("v3", 0);
        SYMBOL_DV("v4", 1);
        SYMBOL_DV("v5", 2);
        SYMBOL_UF("f6");
        SYMBOL_UV("v6");
        END_LIB();

        unique_ptr<void, ProgramDelete> ptr1(prog_helper1.ptr());
        unique_ptr<void, ProgramDelete> ptr2(prog_helper2.ptr());
        unique_ptr<void, ProgramDelete> ptr3(prog_helper3.ptr());
        vector<pair<void *, size_t>> pairs;
        pairs.push_back(make_pair(prog_helper1.ptr(), prog_helper1.size()));
        pairs.push_back(make_pair(prog_helper2.ptr(), prog_helper2.size()));
        pairs.push_back(make_pair(prog_helper3.ptr(), prog_helper3.size()));
        bool is_loaded = _M_vm->load(pairs);
        CPPUNIT_ASSERT(is_loaded);
        // prog_helper1
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _M_vm->env().fun(0).arg_count());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), _M_vm->env().fun("f1").arg_count());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _M_vm->env().fun(1).arg_count());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), _M_vm->env().fun("f2").arg_count());
        CPPUNIT_ASSERT_EQUAL(Value(1), _M_vm->env().var(0));
        CPPUNIT_ASSERT_EQUAL(Value(1), _M_vm->env().var("v1"));
        CPPUNIT_ASSERT_EQUAL(Value(2), _M_vm->env().var(1));
        CPPUNIT_ASSERT_EQUAL(Value(2), _M_vm->env().var("v2"));
        // prog_helper2
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _M_vm->env().fun(2).arg_count()); // 0
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(2).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(3, _M_vm->env().fun(2).instr(0).arg1.i);
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type2(_M_vm->env().fun(2).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(4, _M_vm->env().fun(2).instr(0).arg2.i);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), _M_vm->env().fun(3).arg_count()); // 1
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(3).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(0, _M_vm->env().fun(3).instr(0).arg1.i); // f1
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type2(_M_vm->env().fun(3).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(0U, _M_vm->env().fun(3).instr(0).arg2.gvar); // v1
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), _M_vm->env().fun(4).arg_count()); // 2
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), _M_vm->env().fun("f6").arg_count()); // 2
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type1(_M_vm->env().fun(4).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(1U, _M_vm->env().fun(4).instr(0).arg1.gvar); // v2
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type2(_M_vm->env().fun(4).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(1, _M_vm->env().fun(4).instr(0).arg2.i); // f2
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(6), _M_vm->env().fun(5).arg_count()); // 3
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type1(_M_vm->env().fun(5).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(5U, _M_vm->env().fun(5).instr(0).arg1.gvar); // v3
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type2(_M_vm->env().fun(5).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(6U, _M_vm->env().fun(5).instr(0).arg2.gvar); // v4
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(7), _M_vm->env().fun(6).arg_count()); // 4
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(6).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(9, _M_vm->env().fun(6).instr(0).arg1.i); // f3
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type2(_M_vm->env().fun(6).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(10, _M_vm->env().fun(6).instr(0).arg2.i); // f4
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(8), _M_vm->env().fun(7).arg_count()); // 5
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(7).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(11, _M_vm->env().fun(7).instr(0).arg1.i); // f5
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type2(_M_vm->env().fun(7).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(7U, _M_vm->env().fun(7).instr(0).arg2.gvar); // v5
        CPPUNIT_ASSERT_EQUAL(VALUE_TYPE_REF, _M_vm->env().var(2).type());
        CPPUNIT_ASSERT_EQUAL(OBJECT_TYPE_IARRAY32, _M_vm->env().var(2).r()->type());
        CPPUNIT_ASSERT_EQUAL(Value(2), _M_vm->env().var(2).r()->elem(0));
        CPPUNIT_ASSERT_EQUAL(Value(0), _M_vm->env().var(2).r()->elem(1));
        CPPUNIT_ASSERT_EQUAL(Value(1), _M_vm->env().var(2).r()->elem(2));
        CPPUNIT_ASSERT_EQUAL(VALUE_TYPE_REF, _M_vm->env().var(3).type());
        CPPUNIT_ASSERT_EQUAL(VALUE_TYPE_REF, _M_vm->env().var("v6").type());
        CPPUNIT_ASSERT_EQUAL(OBJECT_TYPE_IARRAY64, _M_vm->env().var(3).r()->type());
        CPPUNIT_ASSERT_EQUAL(OBJECT_TYPE_IARRAY64, _M_vm->env().var("v6").r()->type());
        CPPUNIT_ASSERT_EQUAL(Value(0), _M_vm->env().var(3).r()->elem(0));
        CPPUNIT_ASSERT_EQUAL(Value(3), _M_vm->env().var(3).r()->elem(1));
        CPPUNIT_ASSERT_EQUAL(Value(1), _M_vm->env().var(3).r()->elem(2));
        CPPUNIT_ASSERT_EQUAL(VALUE_TYPE_REF, _M_vm->env().var(4).type());
        CPPUNIT_ASSERT_EQUAL(OBJECT_TYPE_TUPLE, _M_vm->env().var(4).r()->type());
        CPPUNIT_ASSERT_EQUAL(Value(0), _M_vm->env().var(4).r()->elem(0));
        CPPUNIT_ASSERT_EQUAL(Value(1), _M_vm->env().var(4).r()->elem(1));
        CPPUNIT_ASSERT_EQUAL(Value(4), _M_vm->env().var(4).r()->elem(2));
        CPPUNIT_ASSERT_EQUAL(Value(9), _M_vm->env().var(4).r()->elem(3));
        CPPUNIT_ASSERT_EQUAL(true, _M_vm->has_entry());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(8), _M_vm->entry());
        // prog_helper3
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(10), _M_vm->env().fun(9).arg_count()); // 0
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(9).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(4, _M_vm->env().fun(9).instr(0).arg1.i); // f6
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(11), _M_vm->env().fun(10).arg_count()); // 1
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_GVAR, opcode::opcode_to_arg_type1(_M_vm->env().fun(10).instr(0).opcode));
        CPPUNIT_ASSERT_EQUAL(3U, _M_vm->env().fun(10).instr(0).arg1.gvar); // v6
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(12), _M_vm->env().fun(11).arg_count()); // 2
        CPPUNIT_ASSERT_EQUAL(Value(3), _M_vm->env().var(5));
        CPPUNIT_ASSERT_EQUAL(Value(3), _M_vm->env().var("v3"));
        CPPUNIT_ASSERT_EQUAL(Value(4), _M_vm->env().var(6));
        CPPUNIT_ASSERT_EQUAL(Value(4), _M_vm->env().var("v4"));
        CPPUNIT_ASSERT_EQUAL(Value(5), _M_vm->env().var(7));
        CPPUNIT_ASSERT_EQUAL(Value(5), _M_vm->env().var("v5"));
        CPPUNIT_ASSERT_EQUAL(Value(10), _M_vm->env().var(8));
        CPPUNIT_ASSERT_EQUAL(Value(4), _M_vm->env().var(9));
      }

      void VirtualMachineTests::test_vm_complains_on_undefined_symbols()
      {
        PROG(prog_helper1, 0);
        FUN(1);
        LET(IADD, IMM(0), GV(0));
        LET(IADD, GV(0), IMM(0));
        IN();
        RET(IADD, IMM(0), GV(0));
        END_FUN();
        RELOC_SA1F(0, 0);
        RELOC_SA1V(0, 3);
        RELOC_SA1V(1, 4);
        RELOC_SA1F(1, 1);
        RELOC_SA1F(2, 2);
        RELOC_SA1V(2, 5);
        SYMBOL_UF("f");
        SYMBOL_UF("g");
        SYMBOL_UF("h");
        SYMBOL_UV("v");
        SYMBOL_UV("w");
        SYMBOL_UV("x");
        END_PROG();

        LIB(prog_helper2);
        FUN(0);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        VAR_I(0);
        SYMBOL_DF("g", 0);
        SYMBOL_DV("w", 0);
        END_LIB();

        unique_ptr<void, ProgramDelete> ptr1(prog_helper1.ptr());
        unique_ptr<void, ProgramDelete> ptr2(prog_helper2.ptr());
        vector<pair<void *, size_t>> pairs;
        pairs.push_back(make_pair(prog_helper1.ptr(), prog_helper1.size()));
        pairs.push_back(make_pair(prog_helper2.ptr(), prog_helper2.size()));
        list<LoadingError> errors;
        bool is_loaded = _M_vm->load(pairs, &errors);
        vector<LoadingError> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT(!is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), errors.size());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), error_vector[0].pair_index());
        CPPUNIT_ASSERT_EQUAL(LOADING_ERROR_NO_FUN_SYM, error_vector[0].error());
        CPPUNIT_ASSERT("f" == error_vector[0].symbol_name());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), error_vector[1].pair_index());
        CPPUNIT_ASSERT_EQUAL(LOADING_ERROR_NO_FUN_SYM, error_vector[1].error());
        CPPUNIT_ASSERT("h" == error_vector[1].symbol_name());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), error_vector[2].pair_index());
        CPPUNIT_ASSERT_EQUAL(LOADING_ERROR_NO_VAR_SYM, error_vector[2].error());
        CPPUNIT_ASSERT("v" == error_vector[2].symbol_name());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), error_vector[3].pair_index());
        CPPUNIT_ASSERT_EQUAL(LOADING_ERROR_NO_VAR_SYM, error_vector[3].error());
        CPPUNIT_ASSERT("x" == error_vector[3].symbol_name());
      }
      
      void VirtualMachineTests::test_vm_complains_on_already_defined_symbols()
      {
        LIB(prog_helper1);
        FUN(0);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        VAR_I(0);
        SYMBOL_DF("f", 0);
        SYMBOL_DV("v", 0);
        END_LIB();

        LIB(prog_helper2);
        FUN(0);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        VAR_I(0);
        SYMBOL_DF("f", 0);
        SYMBOL_DV("v", 0);
        END_LIB();

        unique_ptr<void, ProgramDelete> ptr1(prog_helper1.ptr());
        unique_ptr<void, ProgramDelete> ptr2(prog_helper2.ptr());
        vector<pair<void *, size_t>> pairs;
        pairs.push_back(make_pair(prog_helper1.ptr(), prog_helper1.size()));
        pairs.push_back(make_pair(prog_helper2.ptr(), prog_helper2.size()));
        list<LoadingError> errors;
        bool is_loaded = _M_vm->load(pairs, &errors);
        vector<LoadingError> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT(!is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), errors.size());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), error_vector[0].pair_index());
        CPPUNIT_ASSERT(LOADING_ERROR_FUN_SYM == error_vector[0].error());
        CPPUNIT_ASSERT("f" == error_vector[0].symbol_name());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), error_vector[1].pair_index());
        CPPUNIT_ASSERT(LOADING_ERROR_VAR_SYM == error_vector[1].error());
        CPPUNIT_ASSERT("v" == error_vector[1].symbol_name());
      }

      void VirtualMachineTests::test_vm_complains_on_relocation()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(ILOAD, IMM(0), NA());
        IN();
        RET(IADD, IMM(1), IMM(0));
        END_FUN();
        VAR_I(1);
        VAR_I(2);
        RELOC_A1F(0);
        RELOC_A2V(2);
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr1(prog_helper.ptr());
        list<LoadingError> errors;
        bool is_loaded = _M_vm->load(prog_helper.ptr(), prog_helper.size(), &errors);
        vector<LoadingError> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT(!is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), error_vector[0].pair_index());
        CPPUNIT_ASSERT_EQUAL(LOADING_ERROR_RELOC, error_vector[0].error());
        CPPUNIT_ASSERT("" == error_vector[0].symbol_name());
      }

      void VirtualMachineTests::test_vm_complains_on_unrelocable_program()
      {
        LIB(prog_helper1);
        FUN(0);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        RELOC_A1F(0);
        END_LIB();

        PROG(prog_helper2, 0);
        FUN(0);
        RET(ILOAD, IMM(0), NA());
        END_FUN();
        END_PROG();

        unique_ptr<void, ProgramDelete> ptr1(prog_helper1.ptr());
        unique_ptr<void, ProgramDelete> ptr2(prog_helper2.ptr());
        vector<pair<void *, size_t>> pairs;
        pairs.push_back(make_pair(prog_helper1.ptr(), prog_helper1.size()));
        pairs.push_back(make_pair(prog_helper2.ptr(), prog_helper2.size()));
        list<LoadingError> errors;
        bool is_loaded = _M_vm->load(pairs, &errors);
        vector<LoadingError> error_vector;
        for(auto error : errors) error_vector.push_back(error);
        CPPUNIT_ASSERT(!is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errors.size());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), error_vector[0].pair_index());
        CPPUNIT_ASSERT(LOADING_ERROR_NO_RELOC == error_vector[0].error());
        CPPUNIT_ASSERT("" == error_vector[0].symbol_name());
      }

      void VirtualMachineTests::test_vm_complains_on_arguments_of_req_instruction()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        LET(RIARRAY32, NA(), NA());
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        LET(RIARRAY32, NA(), NA());
        IN();
        RET(REQ, LV(0), LV(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_INCORRECT_INSTR == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_arguments_of_rne_instruction()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        LET(RIARRAY32, NA(), NA());
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        LET(RIARRAY32, NA(), NA());
        IN();
        RET(RNE, LV(0), LV(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_INCORRECT_INSTR == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_rutfillr_instruction_with_unique_object()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(RUIAFILL32, IMM(10), IMM(5));
        IN();
        RET(RUTFILLR, IMM(2), LV(0));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_AGAIN_USED_UNIQUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_executes_arg_instructions_with_invocations()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ICALL, IMM(1), NA());
        ARG(ICALL, IMM(2), NA());
        RET(RIARRAY8, NA(), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(10), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(20), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (OBJECT_TYPE_IARRAY8 == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (Value(10) == value.r()->elem(0));
          is_expected &= (Value(20) == value.r()->elem(1));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_try()
      {
        PROG(prog_helper, 0);
        FUN(1);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RLOAD, A(0), NA());
        LET(TRY, IMM(1), IMM(2));
        IN();
        RET(RLOAD, LV(0), NA());
        END_FUN();
        FUN(2);
        LET(RUTFILLI, IMM(2), A(0));
        IN();
        ARG(RLOAD, A(1), NA());
        RET(RUTSNTH, LV(0), IMM(1));
        END_FUN();
        FUN(4);
        LET(IADD, A(0), IMM(1000));
        IN();
        LET(RUTFILLI, IMM(2), LV(0));
        IN();
        ARG(RLOAD, A(3), NA());
        RET(RUTSNTH, LV(1), IMM(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        Reference unique_io_ref(_M_gc->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
        args.push_back(unique_io_ref);
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (Value(1) == value.r()->elem(0));
          Reference ref2(value.r()->elem(1).r());
          is_expected &= ((OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE) == ref2->type());          
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_executes_try_and_throw()
      {
        PROG(prog_helper, 0);
        FUN(1);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RLOAD, A(0), NA());
        LET(TRY, IMM(1), IMM(2));
        IN();
        RET(RLOAD, LV(0), NA());
        END_FUN();
        FUN(2);
        THROW(GV(0));
        END_FUN();
        FUN(4);
        ARG(ILOAD, A(0), NA());
        ARG(RLOAD, A(1), NA());
        ARG(ILOAD, A(2), NA());
        LET(RTUPLE, NA(), NA());
        IN();
        LET(RUTFILLR, IMM(2), LV(0));
        IN();
        ARG(RLOAD, A(3), NA());
        RET(RUTSNTH, LV(1), IMM(1));
        END_FUN();
        VAR_R(0);
        OBJECT(IARRAY8);
        I('a'); I('1'); I('b'); I('2'); I('c'); I('3');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        Reference unique_io_ref(_M_gc->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
        args.push_back(unique_io_ref);
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1(value.r()->elem(0).r());
          is_expected &= (OBJECT_TYPE_TUPLE == ref1->type());
          is_expected &= (Value(ERROR_USER_EXCEPTION) == ref1->elem(0));
          Reference ref2(ref1->elem(1).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref2->type());
          is_expected &= (6 == ref2->length());
          is_expected &= (strncmp("a1b2c3", reinterpret_cast<const char *>(ref2->raw().is8), 6) == 0);
          is_expected &= (Value(2) == ref1->elem(2));
          Reference ref3(value.r()->elem(1).r());
          is_expected &= ((OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE) == ref3->type());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_executes_two_tries_and_throw()
      {
        PROG(prog_helper, 0);
        FUN(1);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RLOAD, A(0), NA());
        LET(TRY, IMM(1), IMM(2));
        IN();
        RET(RLOAD, LV(0), NA());
        END_FUN();
        FUN(2);
        ARG(ILOAD, IMM(3), NA());
        ARG(ILOAD, IMM(4), NA());
        ARG(RLOAD, A(1), NA());
        RET(TRY, IMM(3), IMM(5));
        END_FUN();
        FUN(4);
        ARG(ILOAD, A(0), NA());
        ARG(RLOAD, A(1), NA());
        ARG(ILOAD, A(2), NA());
        LET(RTUPLE, NA(), NA());
        IN();
        LET(RUTFILLR, IMM(2), LV(0));
        IN();
        ARG(RLOAD, A(3), NA());
        RET(RUTSNTH, LV(1), IMM(1));
        END_FUN();
        FUN(2);
        RET(RCALL, IMM(4), NA());
        END_FUN();
        FUN(0);
        THROW(GV(0));
        END_FUN();
        FUN(4);
        THROW(GV(1));
        END_FUN();        
        VAR_R(0);
        VAR_R(16);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c');
        END_OBJECT();
        OBJECT(IARRAY8);
        I('d'); I('e'); I('f');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        Reference unique_io_ref(_M_gc->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
        args.push_back(unique_io_ref);
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1(value.r()->elem(0).r());
          is_expected &= (OBJECT_TYPE_TUPLE == ref1->type());
          is_expected &= (Value(ERROR_USER_EXCEPTION) == ref1->elem(0));
          Reference ref2(ref1->elem(1).r());
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref2->type());
          is_expected &= (3 == ref2->length());
          is_expected &= (strncmp("def", reinterpret_cast<const char *>(ref2->raw().is8), 3) == 0);
          is_expected &= (Value(2) == ref1->elem(2));
          Reference ref3(value.r()->elem(1).r());
          is_expected &= ((OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE) == ref3->type());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }
      
      void VirtualMachineTests::test_vm_throws_and_catches_system_exception()
      {
        PROG(prog_helper, 0);
        FUN(1);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RLOAD, A(0), NA());
        LET(TRY, IMM(1), IMM(2));
        IN();
        RET(RLOAD, LV(0), NA());
        END_FUN();
        FUN(2);
        RET(IDIV, IMM(0), IMM(0));
        END_FUN();
        FUN(4);
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(2), NA());
        LET(RTUPLE, NA(), NA());
        IN();
        LET(RUTFILLR, IMM(2), LV(0));
        IN();
        ARG(RLOAD, A(3), NA());
        RET(RUTSNTH, LV(1), IMM(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        Reference unique_io_ref(_M_gc->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
        args.push_back(unique_io_ref);
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (2 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1(value.r()->elem(0).r());
          is_expected &= (OBJECT_TYPE_TUPLE == ref1->type());
          is_expected &= (Value(ERROR_DIV_BY_ZERO) == ref1->elem(0));
          is_expected &= (Value(2) == ref1->elem(1));
          Reference ref2(value.r()->elem(1).r());
          is_expected &= ((OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE) == ref2->type());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_throws_user_exception()
      {
        PROG(prog_helper, 0);
        FUN(0);
        RET(ICALL, IMM(1), NA());
        END_FUN();
        FUN(0);
        THROW(GV(0));
        END_FUN();
        VAR_R(0);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error, &is_expected](const ReturnValue &value) {
          is_expected_error = (ERROR_USER_EXCEPTION == value.error());
          is_expected = (OBJECT_TYPE_IARRAY8 == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (strncmp("abc", reinterpret_cast<const char *>(value.r()->raw().is8), 3) == 0);
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_throws_user_exception_from_catching_function()
      {
        PROG(prog_helper, 0);
        FUN(1);
        ARG(ILOAD, IMM(1), NA());
        ARG(ILOAD, IMM(2), NA());
        ARG(RLOAD, A(0), NA());
        RET(TRY, IMM(1), IMM(2));
        END_FUN();
        FUN(2);
        THROW(GV(0));
        END_FUN();
        FUN(4);
        THROW(GV(1));
        END_FUN();
        VAR_R(0);
        VAR_R(16);
        OBJECT(IARRAY8);
        I('a'); I('b'); I('c');
        END_OBJECT();
        OBJECT(IARRAY8);
        I('d'); I('e'); I('f');
        END_OBJECT();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        bool is_expected = false;
        vector<Value> args;
        Reference unique_io_ref(_M_gc->new_immortal_object(OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE, 0));
        args.push_back(unique_io_ref);
        Thread thread = _M_vm->start(args, [&is_expected_error, &is_expected](const ReturnValue &value) {
          is_expected_error = (ERROR_USER_EXCEPTION == value.error());
          is_expected = (OBJECT_TYPE_IARRAY8 == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (strncmp("def", reinterpret_cast<const char *>(value.r()->raw().is8), 3) == 0);
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_evaluates_local_variable_values_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(2);
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(1), NA());
        LET(ICALL, IMM(1), NA());
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(1), NA());
        LET(ICALL, IMM(2), NA());
        IN();
        RET(IADD, LV(0), LV(1)); // 610 + 66 = 676
        END_FUN();
        FUN(2);
        LET(IMUL, A(0), A(1)); // 55 * 11 = 605
        LET(IDIV, A(0), A(1)); // 55 / 11 = 5
        IN();
        RET(IADD, LV(0), LV(1)); // 605 + 5 = 610
        END_FUN();
        FUN(2);
        LET(IAND, A(0), A(1)); // 55 & 11 = 11
        LET(IOR, A(0), A(1)); // 55 | 11 = 55
        IN();
        RET(IADD, LV(0), LV(1)); // 11 + 55 = 66 
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(55));
        args.push_back(Value(11));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (Value(676) == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_evaluates_functions_which_returns_passed_arguments_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(4);
        ARG(ILOAD, A(0), NA());
        ARG(ILOAD, A(1), NA());
        LET(ICALL, IMM(1), NA());
        ARG(ILOAD, A(2), NA());
        ARG(ILOAD, A(3), NA());
        LET(ICALL, IMM(2), NA());
        IN();
        ARG(ILOAD, LV(0), NA());
        LET(ICALL, IMM(3), NA());
        ARG(ILOAD, LV(1), NA());
        LET(ICALL, IMM(4), NA());
        IN();
        RET(IADD, LV(2), LV(3)); // 6 + 200 = 206
        END_FUN();
        FUN(2);
        RET(IADD, A(0), A(1)); // 1 + 5 = 6
        END_FUN();
        FUN(2);
        RET(IMUL, A(0), A(1)); // 10 * 20 = 200
        END_FUN();
        FUN(1);
        RET(ILOAD, A(0), NA());
        END_FUN();
        FUN(1);
        RET(ILOAD, A(0), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        vector<Value> args;
        args.push_back(Value(1));
        args.push_back(Value(5));
        args.push_back(Value(10));
        args.push_back(Value(20));
        Thread thread = _M_vm->start(args, [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (Value(206) == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);
        CPPUNIT_ASSERT(is_expected);
      }

      void VirtualMachineTests::test_vm_complains_on_many_references_to_unique_object_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM('a'), NA());
        LET(RCALL, IMM(1), NA());
        IN();
        ARG(RLOAD, LV(0), NA());
        LET(RCALL, IMM(2), NA());
        ARG(RLOAD, LV(0), NA());
        LET(RCALL, IMM(2), NA());
        IN();
        ARG(ILOAD, IMM('b'), NA());
        LET(RUIASNTH8, LV(1), IMM(0));
        ARG(ILOAD, IMM('c'), NA());
        LET(RUIASNTH8, LV(2), IMM(0));
        IN();
        LETTUPLE(RUIANTH8, 2, LV(3), IMM(0));
        LETTUPLE(RUIANTH8, 2, LV(4), IMM(0));
        IN();
        RET(IADD, LV(5), LV(7));
        END_FUN();
        FUN(1);
        RET(RUIAFILL8, IMM(5), A(0));
        END_FUN();
        FUN(1);
        RET(RLOAD, A(0), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_AGAIN_USED_UNIQUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);
      }

      void VirtualMachineTests::test_vm_complains_on_unique_reference_in_shared_tuple_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(0);
        ARG(ILOAD, IMM('a'), NA());
        LET(RCALL, IMM(1), NA());
        IN();
        ARG(RLOAD, LV(0), NA());
        LET(RTUPLE, NA(), NA());
        IN();
        LET(RUTFILLI, IMM(2), IMM(0));
        IN();
        ARG(RTNTH, LV(1), IMM(0));
        RET(RUTSNTH, LV(2), IMM(0));
        END_FUN();
        FUN(1);
        RET(RUIAFILL8, IMM(2), A(0));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_expected_error = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_expected_error](const ReturnValue &value) {
          is_expected_error = (ERROR_UNIQUE_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected_error);        
      }

      void VirtualMachineTests::test_vm_loads_program_with_native_function_symbols()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(RLOAD, GV(0), NA());
        ARG(ILOAD, IMM(0), NA());
        ARG(IADD, IMM(1000), IMM(0));
        ARG(ILOAD, GV(1), NA());
        ARG(RTNTH, LV(0), IMM(1));
        RET(RTUPLE, NA(), NA());
        END_FUN();
        VAR_R(0);
        VAR_I(0);
        OBJECT(TUPLE);
        I(1); I(0); I(2);
        END_OBJECT();
        RELOC_SA1NF(1, 0);
        RELOC_SA2NF(2, 1);
        RELOC_SVNF(1, 2);
        RELOC_SENF(16, 3);
        SYMBOL_UNF("atof");
        SYMBOL_UNF("ftoa");
        SYMBOL_UNF("atoi");
        SYMBOL_UNF("itoa");
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), _M_vm->env().fun(0).arg_count());
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type1(_M_vm->env().fun(0).instr(1).opcode));
        CPPUNIT_ASSERT_EQUAL(NATIVE_FUN_ATOF, _M_vm->env().fun(0).instr(1).arg1.i);
        CPPUNIT_ASSERT_EQUAL(opcode::ARG_TYPE_IMM, opcode::opcode_to_arg_type2(_M_vm->env().fun(0).instr(2).opcode));
        CPPUNIT_ASSERT_EQUAL(NATIVE_FUN_FTOA, _M_vm->env().fun(0).instr(2).arg2.i);
        CPPUNIT_ASSERT_EQUAL(VALUE_TYPE_REF, _M_vm->env().var(0).type());
        CPPUNIT_ASSERT_EQUAL(OBJECT_TYPE_TUPLE, _M_vm->env().var(0).r()->type());
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), _M_vm->env().var(0).r()->length());
        CPPUNIT_ASSERT_EQUAL(Value(NATIVE_FUN_ITOA), _M_vm->env().var(0).r()->elem(1));
        CPPUNIT_ASSERT_EQUAL(Value(NATIVE_FUN_ATOI), _M_vm->env().var(1));
      }

      void VirtualMachineTests::test_vm_loads_program_with_function_infos()
      {
        PROG(prog_helper, 0);
        FUN(0);
        RET(ILOAD, IMM(1), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(2), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(3), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(4), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(5), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(6), NA());
        END_FUN();
        FUN_INFO(0, 0, ~(EVAL_STRATEGY_LAZY | EVAL_STRATEGY_MEMO) & 0xff);
        FUN_INFO(1, EVAL_STRATEGY_LAZY, ~EVAL_STRATEGY_MEMO & 0xff);
        FUN_INFO(4, EVAL_STRATEGY_MEMO, ~EVAL_STRATEGY_LAZY & 0xff);
        FUN_INFO(5, EVAL_STRATEGY_MEMO | EVAL_STRATEGY_LAZY, 0xff);
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0), _M_vm->env().fun_info(0).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(~(EVAL_STRATEGY_LAZY | EVAL_STRATEGY_MEMO) & 0xff), _M_vm->env().fun_info(0).eval_strategy_mask());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(EVAL_STRATEGY_LAZY), _M_vm->env().fun_info(1).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(~EVAL_STRATEGY_MEMO & 0xff), _M_vm->env().fun_info(1).eval_strategy_mask());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0), _M_vm->env().fun_info(2).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0xff), _M_vm->env().fun_info(2).eval_strategy_mask());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0), _M_vm->env().fun_info(3).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0xff), _M_vm->env().fun_info(3).eval_strategy_mask());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(EVAL_STRATEGY_MEMO), _M_vm->env().fun_info(4).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(~EVAL_STRATEGY_LAZY & 0xff), _M_vm->env().fun_info(4).eval_strategy_mask());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(EVAL_STRATEGY_MEMO | EVAL_STRATEGY_LAZY), _M_vm->env().fun_info(5).eval_strategy());
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(0xff), _M_vm->env().fun_info(5).eval_strategy_mask());
      }

      void VirtualMachineTests::test_vm_evaluates_lettuple_with_rcall_operation_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(0);
        LET(RCALL, IMM(1), NA());
        IN();
        ARG(RLOAD, LV(0), NA());
        LETTUPLE(RCALL, 2, IMM(2), NA());
        IN();
        RET(IADD, LV(1), LV(2));
        END_FUN();
        FUN(0);
        RET(RUTFILLI, IMM(2), IMM(10));
        END_FUN();
        FUN(1);
        LET(ILOAD, IMM(20), NA());
        IN();
        ARG(IADD, LV(0), IMM(50));
        RET(RUTSNTH, A(0), IMM(1));
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (80 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);        
        CPPUNIT_ASSERT(is_expected);        
      }

      void VirtualMachineTests::test_vm_evaluates_nested_invocations_with_value_type_changes_for_lazy_evaluation()
      {
        PROG(prog_helper, 0);
        FUN(0);
        RET(ICALL, IMM(1), NA());
        END_FUN();
        FUN(0);
        RET(FCALL, IMM(2), NA());
        END_FUN();
        FUN(0);
        RET(ICALL, IMM(3), NA());
        END_FUN();
        FUN(0);
        RET(ILOAD, IMM(10), NA());
        END_FUN();
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        bool is_loaded = _M_vm->load(ptr.get(), prog_helper.size());
        CPPUNIT_ASSERT(is_loaded);
        bool is_success = false;
        bool is_expected = false;
        Thread thread = _M_vm->start(vector<Value>(), [&is_success, &is_expected](const ReturnValue &value) {
          is_success = (ERROR_SUCCESS == value.error());
          is_expected = (0 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_success);        
        CPPUNIT_ASSERT(is_expected);        
      }

      DEF_IMPL_VM_TESTS(Eager, InterpreterVirtualMachine);

      DEF_IMPL_VM_TESTS(Lazy, InterpreterVirtualMachine);

      DEF_IMPL_VM_TESTS_WITH_MEMO_CACHE(HashTable, Memoization, InterpreterVirtualMachine, 32 * 1024);

      DEF_IMPL_VM_TESTS_WITH_MEMO_CACHE(HashTable, MemoizationLazy, InterpreterVirtualMachine, 32 * 1024);
    }
  }
}
