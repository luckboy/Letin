/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#include <algorithm>
#include <cstring>
#include <letin/native.hpp>
#include "native_helper_tests.hpp"
#include "helper.hpp"

#define LIA(i)                  ARG(ILOAD, A(i), NA());
#define LFA(i)                  ARG(FLOAD, A(i), NA());
#define LRA(i)                  ARG(RLOAD, A(i), NA());
#define NLA()

#define FUN_INCALL(arg_count, nfi, args)                                        \
FUN(arg_count);                                                                 \
args                                                                            \
RET(INCALL, IMM(MIN_UNRESERVED_NATIVE_FUN_INDEX + nfi), NA());                  \
END_FUN()

#define FUN_FNCALL(arg_count, nfi, args)                                        \
FUN(arg_count);                                                                 \
args                                                                            \
RET(FNCALL, IMM(MIN_UNRESERVED_NATIVE_FUN_INDEX + nfi), NA());                  \
END_FUN()

#define FUN_RNCALL(arg_count, nfi, args)                                        \
FUN(arg_count);                                                                 \
args                                                                            \
RET(RNCALL, IMM(MIN_UNRESERVED_NATIVE_FUN_INDEX + nfi), NA());                  \
END_FUN()

using namespace std;
using namespace letin::native;

namespace letin
{
  namespace vm
  {
    namespace test
    {
      static int check_test1(VirtualMachine *vm, ThreadContext *context, Reference r)
      { return ERROR_SUCCESS; }
      LETIN_NATIVE_REF_CHECKER(ctest1, check_test1);

      static int check_test2(VirtualMachine *vm, ThreadContext *context, Object &object)
      { return ERROR_SUCCESS; }
      LETIN_NATIVE_OBJECT_CHECKER(ctest2, check_test2);

      static int check_test3(VirtualMachine *vm, ThreadContext *context, Reference r)
      { return ERROR_INCORRECT_OBJECT; }
      LETIN_NATIVE_REF_CHECKER(ctest3, check_test3);

      static int check_test4(VirtualMachine *vm, ThreadContext *context, Object &object)
      { return ERROR_INCORRECT_OBJECT; }
      LETIN_NATIVE_OBJECT_CHECKER(ctest4, check_test4);

      static bool int_to_test1(int64_t i, int &test_value)
      { test_value = i; return true; }
      LETIN_NATIVE_INT_CONVERTER(totest1, int_to_test1, int);

      static bool float_to_test2(double f, int &test_value)
      { test_value = f; return true; }
      LETIN_NATIVE_FLOAT_CONVERTER(totest2, float_to_test2, int);

      static bool ref_to_test3(Reference r, int &test_value)
      { test_value = 10; return true; }
      LETIN_NATIVE_REF_CONVERTER(totest3, ref_to_test3, int);

      static bool object_to_test4(const Object &object, int &test_value)
      { test_value = 20; return true; }
      LETIN_NATIVE_OBJECT_CONVERTER(totest4, object_to_test4, int);

      static bool int_to_test5(int64_t i, int &test_value) { return false; }
      LETIN_NATIVE_INT_CONVERTER(totest5, int_to_test5, int);

      static bool float_to_test6(double f, int &test_value) { return false; }
      LETIN_NATIVE_FLOAT_CONVERTER(totest6, float_to_test6, int);

      static bool ref_to_test7(Reference r, int &test_value) { return false; }
      LETIN_NATIVE_REF_CONVERTER(totest7, ref_to_test7, int);

      static bool object_to_test8(const Object &object, int &test_value) { return false; }
      LETIN_NATIVE_OBJECT_CONVERTER(totest8, object_to_test8, int);

      static bool set_new_test1(VirtualMachine *vm, ThreadContext *context, RegisteredReference &tmp_r, const int &i)
      {
        tmp_r = vm->gc()->new_object(OBJECT_TYPE_IARRAY64, 1, context);
        if(tmp_r.is_null()) return false;
        tmp_r->set_elem(0, Value(i));
        tmp_r.register_ref();
        return true;
      }
      LETIN_NATIVE_REF_SETTER(vtest1, set_new_test1, int);

      static Object *new_test2(VirtualMachine *vm, ThreadContext *context, const int &i)
      {
        Object *object = vm->gc()->new_object(OBJECT_TYPE_IARRAY64, 1, context);
        if(object == nullptr) return nullptr;
        object->set_elem(0, Value(i));
        return object;
      }
      LETIN_NATIVE_OBJECT_SETTER(vtest2, new_test2, int);

      CPPUNIT_TEST_SUITE_REGISTRATION(NativeHelperTests);

      void NativeHelperTests::setUp()
      {
        _M_alloc = new_allocator();
        _M_loader = new_loader();
        _M_gc = new_garbage_collector(_M_alloc);
        _M_eval_strategy = new_evaluation_strategy();
      }

      void NativeHelperTests::tearDown()
      {
        delete _M_eval_strategy;
        delete _M_gc;
        delete _M_loader;
        delete _M_alloc;
      }

      void NativeHelperTests::test_native_helpers_check_values()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cint, cfloat, native::cref, cint);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(4, 0, LIA(0) LFA(1) LRA(2) LIA(3));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 5));
        strcpy(reinterpret_cast<char *>(ref->raw().is8), "test");
        vector<Value> args { Value(1), Value(2.0), Value(ref), Value(3) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && (1 == value.i()));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_check_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ciarray8, cuiarray32, crarray);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(3, 0, LRA(0) LRA(1) LRA(2));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 5));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "test");
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE, 3));
        ref2->set_elem(0, Value(10));
        ref2->set_elem(1, Value(20));
        ref2->set_elem(2, Value(30));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_IARRAY32, 2));
        ref3->set_elem(0, Value(1));
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_IARRAY32, 2));
        ref4->set_elem(0, Value(2));
        Reference ref5(_M_gc->new_object(OBJECT_TYPE_RARRAY, 2));
        ref5->set_elem(0, Value(ref3));
        ref5->set_elem(1, Value(ref4));
        vector<Value> args { Value(ref1), Value(ref2), Value(ref5) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && (1 == value.i()));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_check_tuple_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ct(cint, ciarray8, cfloat), cut(cusfarray, cint), ct());
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(3, 0, LRA(0) LRA(1) LRA(2));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 4));
        strcpy(reinterpret_cast<char *>(ref1->raw().is8), "abc");
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 3));
        ref2->set_elem(0, Value(1));
        ref2->set_elem(1, Value(ref1));
        ref2->set_elem(2, Value(2.5));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE, 4));
        ref3->set_elem(0, Value(1.1));
        ref3->set_elem(1, Value(2.2));
        ref3->set_elem(2, Value(3.3));
        ref3->set_elem(3, Value(4.4));
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2));
        ref4->set_elem(0, Value(ref3));
        ref4->set_elem(1, Value(100));
        Reference ref5(_M_gc->new_object(OBJECT_TYPE_TUPLE, 0));
        vector<Value> args { Value(ref2), Value(ref4), Value(ref5) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && (1 == value.i()));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_check_option_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, coption(cint), cuoption(cfloat), cuoption(cint), coption(cfloat));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(4, 0, LRA(0) LRA(1) LRA(2) LRA(3));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_TUPLE, 1));
        ref1->set_elem(0, Value(0));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2));
        ref2->set_elem(0, Value(1));
        ref2->set_elem(1, Value(2.25));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2));
        ref3->set_elem(0, Value(1));
        ref3->set_elem(1, Value(2));
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_TUPLE, 1));
        ref4->set_elem(0, Value(0));
        vector<Value> args { Value(ref1), Value(ref2), Value(ref3), Value(ref4) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && (1 == value.i()));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_check_either_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cueither(cint, cfloat), ceither(cfloat, cint), cueither(cint, cfloat), ceither(cfloat, cint));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(4, 0, LRA(0) LRA(1) LRA(2) LRA(3));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2));
        ref1->set_elem(0, Value(0));
        ref1->set_elem(1, Value(1));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref2->set_elem(0, Value(1));
        ref2->set_elem(1, Value(2));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE, 2));
        ref3->set_elem(0, Value(1));
        ref3->set_elem(1, Value(2.5));
        Reference ref4(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref4->set_elem(0, Value(0));
        ref4->set_elem(1, Value(4.75));
        vector<Value> args { Value(ref1), Value(ref2), Value(ref3), Value(ref4) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && (1 == value.i()));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_convert_values()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cfloat, cint, native::cref, cfloat);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              double f1;
              int64_t i2;
              Reference r3;
              double f4;
              if(!convert_args(args, tofloat(f1), toint(i2), toref(r3), tofloat(f4)))
                return return_value(vm, context, vt());
              return return_value(vm, context, vt(vfloat(f1 + f4), vint(i2 + 10), vref(r3)));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(4, 0, LFA(0) LIA(1) LRA(2) LFA(3));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 4));
        strcpy(reinterpret_cast<char *>(ref->raw().is8), "abc");
        vector<Value> args { Value(2.3), Value(10), Value(ref), Value(4.5) };
        Thread thread = vm->start(args, [&is_expected, ref](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (Value(6.8) == value.r()->elem(0));
          is_expected &= (Value(20) == value.r()->elem(1));
          is_expected &= (Value(ref) == value.r()->elem(2));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_convert_tuple_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ct(cint, cfloat), ct(), ct(cint, cint));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int64_t i1;
              double f2;
              int64_t i3, i4;
              if(!convert_args(args, tot(toint(i1), tofloat(f2)), tot(), tot(toint(i3), toint(i4))))
                return return_value(vm, context, vt(vint(0)));
              return return_value(vm, context, vt(vint(i1 + 20), vfloat(f2 - 0.25), vint(i3 * i4)));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(3, 0, LRA(0) LRA(1) LRA(2));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref1->set_elem(0, Value(2));
        ref1->set_elem(1, Value(2.5));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 0));
        Reference ref3(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref3->set_elem(0, Value(4));
        ref3->set_elem(1, Value(6));
        vector<Value> args { Value(ref1), Value(ref2), Value(ref3) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (Value(22) == value.r()->elem(0));
          is_expected &= (Value(2.25) == value.r()->elem(1));
          is_expected &= (Value(24) == value.r()->elem(2));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_convert_option_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, coption(cfloat), coption(cint));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              bool is_some1;
              double f1 = 0.0;
              bool is_some2;
              int64_t i2 = 0;
              if(!convert_args(args, tooption(tofloat(f1), is_some1), tooption(toint(i2), is_some2)))
                return return_value(vm, context, vt(vint(0)));
              return return_value(vm, context, vt(vint(is_some1 ? 1 : 0), vfloat(f1), vint(is_some2 ? 1 : 0), vint(i2)));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(2, 0, LRA(0) LRA(1));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_TUPLE, 1));
        ref1->set_elem(0, Value(0));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref2->set_elem(0, Value(1));
        ref2->set_elem(1, Value(10));
        vector<Value> args { Value(ref1), Value(ref2) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (4 == value.r()->length());
          is_expected &= (Value(0) == value.r()->elem(0));
          is_expected &= (Value(0.0) == value.r()->elem(1));
          is_expected &= (Value(1) == value.r()->elem(2));
          is_expected &= (Value(10) == value.r()->elem(3));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_convert_either_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ceither(cfloat, cint), ceither(cint, cfloat));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              bool is_right1;
              double f11 = 0.0;
              int64_t i12 = 0;
              bool is_right2;
              int64_t i21 = 0;
              double f22 = 0.0;
              if(!convert_args(args, toeither(tofloat(f11), toint(i12), is_right1), toeither(toint(i21), tofloat(f22), is_right2)))
                return return_value(vm, context, vt(vint(0)));
              return return_value(vm, context, vt(vint(is_right1 ? 1 : 0), vfloat(f11), vint(i12), vint(is_right2 ? 1 : 0), vint(i21), vfloat(f22)));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(2, 0, LRA(0) LRA(1));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref1(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref1->set_elem(0, Value(0));
        ref1->set_elem(1, Value(2.5));
        Reference ref2(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref2->set_elem(0, Value(1));
        ref2->set_elem(1, Value(4.5));
        vector<Value> args { Value(ref1), Value(ref2) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (6 == value.r()->length());
          is_expected &= (Value(0) == value.r()->elem(0));
          is_expected &= (Value(2.5) == value.r()->elem(1));
          is_expected &= (Value(0) == value.r()->elem(2));
          is_expected &= (Value(1) == value.r()->elem(3));
          is_expected &= (Value(0) == value.r()->elem(4));
          is_expected &= (Value(4.5) == value.r()->elem(5));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_return_values()
      {
        vector<NativeFunction> native_funs = {
          {
            "test1",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          },
          {
            "test2",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vfloat(2.5));
            }
          },
          {
            "test3",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              RegisteredReference ref(vm->gc()->new_object(OBJECT_TYPE_IARRAY64, 2, context), context);
              ref->set_elem(0, Value(1));
              ref->set_elem(1, Value(2));
              return return_value(vm, context, vref(ref));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN(0);
        LET(ICALL, IMM(1), NA());
        LET(FCALL, IMM(2), NA());
        LET(RCALL, IMM(3), NA());
        IN();
        ARG(ILOAD, LV(0), NA());
        ARG(FLOAD, LV(1), NA());
        ARG(RLOAD, LV(2), NA());
        RET(RTUPLE, NA(), NA());
        END_FUN();
        FUN_INCALL(0, 0, NLA());
        FUN_FNCALL(0, 1, NLA());
        FUN_RNCALL(0, 2, NLA());
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Thread thread = vm->start(vector<Value>(), [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (Value(1) == value.r()->elem(0));
          is_expected &= (Value(2.5) == value.r()->elem(1));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(2).type());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }


      void NativeHelperTests::test_native_helpers_return_tuple_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test1",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vt(vint(1), vfloat(2.0)));
            }
          },
          {
            "test2",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vt());
            }
          },
          {
            "test3",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vut(vfloat(0.5), vint(1), v(Value(2))));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN(0);
        LET(RCALL, IMM(1), NA());
        LET(RCALL, IMM(2), NA());
        LET(RCALL, IMM(3), NA());
        IN();
        LET(RUTFILLI, IMM(3), IMM(0));
        IN();
        ARG(RLOAD, LV(0), NA());
        LET(RUTSNTH, LV(3), IMM(0));
        IN();
        ARG(RLOAD, LV(1), NA());
        LET(RUTSNTH, LV(4), IMM(1));
        IN();
        ARG(RLOAD, LV(2), NA());
        RET(RUTSNTH, LV(5), IMM(2));
        END_FUN();
        FUN_RNCALL(0, 0, NLA());
        FUN_RNCALL(0, 1, NLA());
        FUN_RNCALL(0, 2, NLA());
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Thread thread = vm->start(vector<Value>(), [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1 = value.r()->elem(0).r();
          is_expected &= (OBJECT_TYPE_TUPLE == ref1->type());
          is_expected &= (2 == ref1->length());
          is_expected &= (Value(1) == ref1->elem(0));
          is_expected &= (Value(2.0) == ref1->elem(1));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(1).type());
          Reference ref2 = value.r()->elem(1).r();
          is_expected &= (OBJECT_TYPE_TUPLE == ref2->type());
          is_expected &= (0 == ref2->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(2).type());
          Reference ref3 = value.r()->elem(2).r();
          is_expected &= ((OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE) == ref3->type());
          is_expected &= (3 == ref3->length());
          is_expected &= (Value(0.5) == ref3->elem(0));
          is_expected &= (Value(1) == ref3->elem(1));
          is_expected &= (Value(2) == ref3->elem(2));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helpers_return_string_objects()
      {
        vector<NativeFunction> native_funs = {
          {
            "test1",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vstr(string("abc")));
            }
          },
          {
            "test2",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vcstr("def"));
            }
          },
          {
            "test3",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vcstr("hijklmnop", 6));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN(0);
        LET(RCALL, IMM(1), NA());
        LET(RCALL, IMM(2), NA());
        LET(RCALL, IMM(3), NA());
        IN();
        ARG(RLOAD, LV(0), NA());
        ARG(RLOAD, LV(1), NA());
        ARG(RLOAD, LV(2), NA());
        RET(RTUPLE, NA(), NA());
        END_FUN();
        FUN_RNCALL(0, 0, NLA());
        FUN_RNCALL(0, 1, NLA());
        FUN_RNCALL(0, 2, NLA());
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Thread thread = vm->start(vector<Value>(), [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_TUPLE == value.r()->type());
          is_expected &= (3 == value.r()->length());
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(0).type());
          Reference ref1 = value.r()->elem(0).r();
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref1->type());
          is_expected &= (3 == ref1->length());
          const char *expected_str1 = "abc";
          is_expected &= (equal(expected_str1, expected_str1 + 3, reinterpret_cast<char *>(ref1->raw().is8)));
          is_expected &= (VALUE_TYPE_REF == value.r()->elem(1).type());
          Reference ref2 = value.r()->elem(1).r();
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref2->type());
          is_expected &= (3 == ref2->length());
          const char *expected_str2 = "def";
          is_expected &= (equal(expected_str2, expected_str2 + 3, reinterpret_cast<char *>(ref2->raw().is8)));
          Reference ref3 = value.r()->elem(2).r();
          is_expected &= (OBJECT_TYPE_IARRAY8 == ref3->type());
          is_expected &= (6 == ref3->length());
          const char *expected_str3 = "hijklm";
          is_expected &= (equal(expected_str3, expected_str3 + 3, reinterpret_cast<char *>(ref3->raw().is8)));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_number_of_arguments()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cint, cint, cint);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(4, 0, LIA(0) LIA(1) LIA(2) LIA(3));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(1), Value(2), Value(3), Value(4) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_ARG_COUNT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cint, cfloat, native::cref);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(3, 0, LIA(0) LIA(1) LIA(2));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(1), Value(2), Value(3) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_VALUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ct(cint, cfloat));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_shared_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cuiarray8);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_unique_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ciarray8);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE, 1));
        ref->set_elem(0, Value(1234));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_tuple_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ct(cint, cfloat));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref->set_elem(0, Value(1.1));
        ref->set_elem(1, Value(2));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_VALUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_option_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, coption(cint));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref->set_elem(0, Value(1));
        ref->set_elem(1, Value(2.5));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_VALUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_native_helper_complains_on_incorrect_either_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ceither(cfloat, cint));
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_TUPLE, 2));
        ref->set_elem(0, Value(1));
        ref->set_elem(1, Value(2.5));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_VALUE == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_reference_checker_checks_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ctest1);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 1 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_object_checker_checks_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ctest2);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 1 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_int_converter_converts_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cint);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest1(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LIA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(1) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 1 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_float_converter_converts_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cfloat);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest2(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LFA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(2.5) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 2 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_reference_converter_converts_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, native::cref);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest3(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 10 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_object_converter_converts_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, native::cref);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest4(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 20 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_reference_setter_sets_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vtest1(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(0, 0, NLA());
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Thread thread = vm->start(vector<Value>(), [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_IARRAY64 == value.r()->type());
          is_expected &= (1 == value.r()->length());
          is_expected &= (Value(1) == value.r()->elem(0));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_object_setter_sets_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vtest2(2));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_RNCALL(0, 0, NLA());
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Thread thread = vm->start(vector<Value>(), [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error());
          is_expected &= (OBJECT_TYPE_IARRAY64 == value.r()->type());
          is_expected &= (1 == value.r()->length());
          is_expected &= (Value(2) == value.r()->elem(0));
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_reference_checker_conplains_on_incorrect_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ctest3);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_object_checker_conplains_on_incorrect_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, ctest4);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              return return_value(vm, context, vint(1));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_INCORRECT_OBJECT == value.error());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_int_converter_conplains_on_incorrect_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cint);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest5(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LIA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(1) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 0 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_float_converter_conplains_on_incorrect_value()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, cfloat);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest6(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LFA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        vector<Value> args { Value(2.5) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 0 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_reference_converter_conplains_on_incorrect_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, native::cref);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest7(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 0 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }

      void NativeHelperTests::test_function_object_converter_conplains_on_incorrect_object()
      {
        vector<NativeFunction> native_funs = {
          {
            "test",
            [](VirtualMachine *vm, ThreadContext *context, ArgumentList &args) {
              int error = check_args(vm, context, args, native::cref);
              if(error != ERROR_SUCCESS) return error_return_value(error);
              int i;
              if(!convert_args(args, totest8(i)))
                return return_value(vm, context, vint(0));
              return return_value(vm, context, vint(i));
            }
          }
        };
        unique_ptr<NativeLibrary> native_lib(new NativeLibrary(native_funs));
        unique_ptr<VirtualMachine> vm(new_virtual_machine(_M_loader, _M_gc, native_lib.get(), _M_eval_strategy));
        PROG(prog_helper, 0);
        FUN_INCALL(1, 0, LRA(0));
        END_PROG();
        unique_ptr<void, ProgramDelete> ptr(prog_helper.ptr());
        CPPUNIT_ASSERT(vm->load(ptr.get(), prog_helper.size()));
        bool is_expected = false;
        Reference ref(_M_gc->new_object(OBJECT_TYPE_IARRAY8, 1));
        ref->set_elem(0, Value('a'));
        vector<Value> args { Value(ref) };
        Thread thread = vm->start(args, [&is_expected](const ReturnValue &value) {
          is_expected = (ERROR_SUCCESS == value.error() && 0 == value.i());
        });
        thread.system_thread().join();
        CPPUNIT_ASSERT(is_expected);
      }
    }
  }
}
