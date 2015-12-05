/****************************************************************************
 *   Copyright (C) 2015 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_NATIVE_HPP
#define _LETIN_NATIVE_HPP

#include <letin/vm.hpp>

//
// Macro definitions for a checking.
//

#define LETIN_NATIVE_REF_CHECKER_TYPE(fun_name)                                 \
::letin::native::priv::FunctionReferenceChecker<decltype(&fun_name), false>

#define LETIN_NATIVE_REF_CHECKER(name, fun_name)                                \
static const LETIN_NATIVE_REF_CHECKER_TYPE(fun_name) name = LETIN_NATIVE_REF_CHECKER_TYPE(fun_name)(fun_name)

#define LETIN_NATIVE_OBJECT_CHECKER_TYPE(fun_name)                              \
::letin::native::priv::FunctionObjectChecker<decltype(&fun_name), false>

#define LETIN_NATIVE_OBJECT_CHECKER(name, fun_name)                             \
static const LETIN_NATIVE_OBJECT_CHECKER_TYPE(fun_name) name = LETIN_NATIVE_OBJECT_CHECKER_TYPE(fun_name)(fun_name)

#define LETIN_NATIVE_UNIQUE_REF_CHECKER_TYPE(fun_name)                          \
::letin::native::priv::FunctionReferenceChecker<decltype(&fun_name), true>

#define LETIN_NATIVE_UNIQUE_REF_CHECKER(name, fun_name)                         \
static const LETIN_NATIVE_UNIQUE_REF_CHECKER_TYPE(fun_name) name = LETIN_NATIVE_UNIQUE_REF_CHECKER_TYPE(fun_name)(fun_name)

#define LETIN_NATIVE_UNIQUE_OBJECT_CHECKER_TYPE(fun_name)                       \
::letin::native::priv::FunctionObjectChecker<decltype(&fun_name), true>

#define LETIN_NATIVE_UNIQUE_OBJECT_CHECKER(name, fun_name)                      \
static const LETIN_NATIVE_UNIQUE_OBJECT_CHECKER_TYPE(fun_name) name = LETIN_NATIVE_UNIQUE_OBJECT_CHECKER_TYPE(fun_name)(fun_name)

//
// Macro definitions for a converting.
//

#define LETINT_NATIVE_INT_CONVERTER_TYPE(fun_name, type)                        \
::letin::native::priv::FunctionIntConverter<decltype(&fun_name), type>

#define LETIN_NATIVE_INT_CONVERTER(name, fun_name, type)                        \
static inline LETINT_NATIVE_INT_CONVERTER_TYPE(fun_name, type) name(type &x)    \
{ return LETINT_NATIVE_INT_CONVERTER_TYPE(fun_name, type)(fun_name, x); }       \
static LETINT_NATIVE_INT_CONVERTER_TYPE(fun_name, type) name(type &x)

#define LETINT_NATIVE_FLOAT_CONVERTER_TYPE(fun_name, type)                      \
::letin::native::priv::FunctionFloatConverter<decltype(&fun_name), type>

#define LETIN_NATIVE_FLOAT_CONVERTER(name, fun_name, type)                      \
static inline LETINT_NATIVE_FLOAT_CONVERTER_TYPE(fun_name, type) name(type &x)  \
{ return LETINT_NATIVE_FLOAT_CONVERTER_TYPE(fun_name, type)(fun_name, x); }     \
static LETINT_NATIVE_FLOAT_CONVERTER_TYPE(fun_name, type) name(type &x)

#define LETINT_NATIVE_REF_CONVERTER_TYPE(fun_name, type)                        \
::letin::native::priv::FunctionReferenceConverter<decltype(&fun_name), type>

#define LETIN_NATIVE_REF_CONVERTER(name, fun_name, type)                        \
static inline LETINT_NATIVE_REF_CONVERTER_TYPE(fun_name, type) name(type &x)    \
{ return LETINT_NATIVE_REF_CONVERTER_TYPE(fun_name, type)(fun_name, x); }       \
static LETINT_NATIVE_REF_CONVERTER_TYPE(fun_name, type) name(type &x)

#define LETINT_NATIVE_OBJECT_CONVERTER_TYPE(fun_name, type)                     \
::letin::native::priv::FunctionObjectConverter<decltype(&fun_name), type>

#define LETIN_NATIVE_OBJECT_CONVERTER(name, fun_name, type)                     \
static inline LETINT_NATIVE_OBJECT_CONVERTER_TYPE(fun_name, type) name(type &x) \
{ return LETINT_NATIVE_OBJECT_CONVERTER_TYPE(fun_name, type)(fun_name, x); }    \
static LETINT_NATIVE_OBJECT_CONVERTER_TYPE(fun_name, type) name(type &x)

//
// Macro definitions for a setting.
//

#define LETINT_NATIVE_REF_SETTER_TYPE(fun_name, type)                           \
::letin::native::priv::FunctionReferenceSetter<decltype(&fun_name), type>

#define LETIN_NATIVE_REF_SETTER(name, fun_name, type)                           \
static inline LETINT_NATIVE_REF_SETTER_TYPE(fun_name, type) name(const type &x) \
{ return LETINT_NATIVE_REF_SETTER_TYPE(fun_name, type)(fun_name, x); }          \
static LETINT_NATIVE_REF_SETTER_TYPE(fun_name, type) name(const type &x)

#define LETINT_NATIVE_OBJECT_SETTER_TYPE(fun_name, type)                        \
::letin::native::priv::FunctionObjectSetter<decltype(&fun_name), type>

#define LETIN_NATIVE_OBJECT_SETTER(name, fun_name, type)                        \
static inline LETINT_NATIVE_OBJECT_SETTER_TYPE(fun_name, type) name(const type &x) \
{ return LETINT_NATIVE_OBJECT_SETTER_TYPE(fun_name, type)(fun_name, x); }       \
static LETINT_NATIVE_OBJECT_SETTER_TYPE(fun_name, type) name(const type &x)

namespace letin
{
  namespace native
  {
    namespace priv
    {
      //
      // Private functions and private structures.
      //

      namespace
      {
        constexpr std::size_t arg_count() { return 0; }

        template<typename _T, typename... _Ts>
        constexpr std::size_t arg_count(_T x, _Ts... xs) { return 1 + arg_count(xs...); }

        template<typename... _Ts>
        struct TemplateParamCount
        { static constexpr std::size_t value() { return 0; } };

        template<typename _T, typename... _Ts>
        struct TemplateParamCount<_T, _Ts...>
        { static constexpr std::size_t value() { return 1 + TemplateParamCount<_Ts...>::value(); } };

        template<typename... _Ts>
        struct TemplateList {};

        template<typename _T, typename... _Ts>
        struct TemplateList<_T, _Ts...>
        {
          _T head;
          TemplateList<_Ts...> tail;

          TemplateList(_T x, _Ts... xs) : head(x), tail(TemplateList<_Ts...>(xs...)) {}
        };
      }

      //
      // Private types and private functions for a checking.
      //

      typedef std::function<int (vm::VirtualMachine *, vm::ThreadContext *, vm::Value &)> CheckerFunction;

      namespace
      {
        template<typename _T>
        inline CheckerFunction bind_checker(_T &checker)
        {
          using namespace std::placeholders;
          return std::bind(&_T::check, &checker, _1, _2, _3);
        }
      }

      int check_int_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value);

      namespace
      {
        struct IntChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_int_value(vm, context, value); }
        };
      }

      int check_float_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value);

      namespace
      {
        struct FloatChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_float_value(vm, context, value); }
        };
      }

      int check_ref_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value);

      namespace
      {
        struct ReferenceChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_ref_value(vm, context, value); }
        };
      }
      
      int check_object_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, int object_type);

      namespace
      {
        template<int _ObjectType>
        struct ObjectChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_object_value(vm, context, value, _ObjectType); }
        };

        struct ForceChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return vm->force(context, value); }
        };

        struct IgnoreChecker
        {
          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return ERROR_SUCCESS; }
        };
      }

      int check_elem(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Object &object, std::size_t i, CheckerFunction fun);

      namespace
      {
        inline int check_elems_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, const TemplateList<> checkers, std::size_t i)
        { return ERROR_SUCCESS; }

        template<typename _T, typename... _Ts>
        inline int check_elems_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, const TemplateList<_T, _Ts...> checkers, std::size_t i)
        {
          int error = check_elem(vm, context, *(value.r()), i, bind_checker(checkers.head));
          if(error != ERROR_SUCCESS) return error;
          return check_elems_from(vm, context, value, checkers.tail, i + 1);
        }

        template<int _ObiectTypeFlag, typename... _Ts>
        inline int check_tuple_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, const TemplateList<_Ts...> checkers)
        {
          int error = check_object_value(vm, context, value, OBJECT_TYPE_TUPLE | _ObiectTypeFlag);
          if(error != ERROR_SUCCESS) return error;
          if(value.r()->length() != TemplateParamCount<_Ts...>::value()) return ERROR_INCORRECT_OBJECT;
          return check_elems_from(vm, context, value, checkers, 0);
        }

        template<int _ObiectTypeFlag, typename... _Ts>
        class TupleChecker
        {
          TemplateList<_Ts...> _M_checkers;
        public:
          TupleChecker(_Ts... checkers) : _M_checkers(checkers...) {}

          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_tuple_value<_ObiectTypeFlag, _Ts...>(vm, context, value, _M_checkers); }
        };
      }

      int check_option_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, CheckerFunction fun, int object_type_flag);

      namespace
      {
        template<typename _T, int _ObjectTypeFlag>
        class OptionChecker
        {
          _T _M_checker;
        public:
          OptionChecker(_T checker) : _M_checker(checker) {}

          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_option_value(vm, context, value, bind_checker(_M_checker), _ObjectTypeFlag); }
        };
      }

      int check_either_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, CheckerFunction left, CheckerFunction right, int object_type_flag);

      namespace
      {
        template<typename _T, typename _U, int _ObjectTypeFlag>
        class EitherChecker
        {
          _T _M_left; _U _M_right;
        public:
          EitherChecker(_T left, _U right) : _M_left(left), _M_right(right) {}

          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_either_value(vm, context, value, bind_checker(_M_left), bind_checker(_M_right), _ObjectTypeFlag); }
        };
      }
      
      int check_ref_value_for_fun(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, std::function<int (vm::VirtualMachine *, vm::ThreadContext *, vm::Reference)> fun, bool is_unique);

      namespace
      {
        template<typename _Fun, bool _IsUnique>
        class FunctionReferenceChecker
        {
          _Fun _M_fun;
        public:
          FunctionReferenceChecker(_Fun fun) : _M_fun(fun) {}

          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_ref_value_for_fun(vm, context, value, _M_fun, _IsUnique); }
        };
      }

      int check_object_value_for_fun(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, std::function<int (vm::VirtualMachine *, vm::ThreadContext *, vm::Object &)> fun, bool is_unique);

      namespace
      {
        template<typename _Fun, bool _IsUnique>
        class FunctionObjectChecker
        {
          _Fun _M_fun;
        public:
          FunctionObjectChecker(_Fun fun) : _M_fun(fun) {}

          int check(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value) const
          { return check_object_value_for_fun(vm, context, value, _M_fun, _IsUnique); }
        };

        inline int check_args_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::ArgumentList &args, std::size_t i)
        { return ERROR_SUCCESS; }

        template<typename _T, typename... _Ts>
        inline int check_args_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::ArgumentList &args, std::size_t i, _T checker, _Ts... checkers)
        {
          int error = checker.check(vm, context, args[i]);
          if(error != ERROR_SUCCESS) return error;
          return check_args_from(vm, context, args, i + 1, checkers...);
        }
      }

      //
      // Private types and private functions for a coverting.
      //

      typedef std::function<bool (const vm::Value &)> ConverterFunction;

      namespace
      {
        template<typename _T>
        inline ConverterFunction bind_converter(_T &converter)
        {
          using namespace std::placeholders;
          return std::bind(&_T::convert, &converter, _1);
        }

        class IntConverter
        {
          std::int64_t &_M_i;
        public:
          IntConverter(std::int64_t &i) : _M_i(i) {}

          bool convert(const vm::Value &value) const { _M_i = value.i(); return true; }
        };

        class FloatConverter
        {
          double &_M_f;
        public:
          FloatConverter(double &f) : _M_f(f) {}

          bool convert(const vm::Value &value) const { _M_f = value.f(); return true; }
        };

        class ReferenceConverter
        {
          vm::Reference &_M_r;
        public:
          ReferenceConverter(vm::Reference &r) : _M_r(r) {}

          bool convert(const vm::Value &value) const { _M_r = value.r(); return true; }
        };

        struct NothingConverter
        { bool convert(const vm::Value &value) const { return true; } };

        inline bool convert_elems_from(const vm::Value &value, const TemplateList<> converters, std::size_t i)
        { return true; }

        template<typename _T, typename... _Ts>
        inline bool convert_elems_from(const vm::Value &value, const TemplateList<_T, _Ts...> converters, std::size_t i)
        {
          if(!converters.head.convert(value.r()->elem(i))) return false;
          return convert_elems_from(value, converters.tail, i + 1);
        }

        template<typename... _Ts>
        class TupleConverter
        {
          TemplateList<_Ts...> _M_converters;
        public:
          TupleConverter(_Ts... converters) : _M_converters(converters...) {}

          bool convert(const vm::Value &value) const
          { return convert_elems_from(value, _M_converters, 0); }
        };
      }

      bool convert_option_value(const vm::Value &value, ConverterFunction fun, bool &some_flag);

      namespace
      {
        template<typename _T>
        class OptionConverter
        {
          _T _M_converter; bool &_M_some_flag;
        public:
          OptionConverter(_T converter, bool &some_flag) : _M_converter(converter), _M_some_flag(some_flag) {}

          bool convert(const vm::Value &value) const
          { return convert_option_value(value, bind_converter(_M_converter), _M_some_flag); }
        };
      }

      bool convert_either_value(const vm::Value &value, ConverterFunction left, ConverterFunction right, bool &right_flag);

      namespace
      {
        template<typename _T, typename _U>
        class EitherConverter
        {
          _T _M_left; _U _M_right; bool &_M_right_flag;
        public:
          EitherConverter(_T left, _U right, bool &right_flag) : _M_left(left), _M_right(right), _M_right_flag(right_flag) {}

          bool convert(const vm::Value &value) const
          { return convert_either_value(value, bind_converter(_M_left), bind_converter(_M_right), _M_right_flag); }
        };

        class StringConverter
        {
          std::string &_M_string;
        public:
          StringConverter(std::string &string) : _M_string(string) {}

          bool convert(const vm::Value &value) const
          {
            _M_string.assign(reinterpret_cast<const char *>(value.r()->raw().is8), value.r()->length());
            return true;
          }
        };

        template<typename _Fun, typename _T>
        class FunctionIntConverter
        {
          _Fun _M_fun; _T &_M_x;
        public:
          FunctionIntConverter(_Fun fun, _T &x) : _M_fun(fun), _M_x(x) {}

          bool convert(const vm::Value &value) const { return _M_fun(value.i(), _M_x); }
        };

        template<typename _Fun, typename _T>
        class FunctionFloatConverter
        {
          _Fun _M_fun; _T &_M_x;
        public:
          FunctionFloatConverter(_Fun fun, _T &x) : _M_fun(fun), _M_x(x) {}

          bool convert(const vm::Value &value) const { return _M_fun(value.f(), _M_x); }
        };

        template<typename _Fun, typename _T>
        class FunctionReferenceConverter
        {
          _Fun _M_fun; _T &_M_x;
        public:
          FunctionReferenceConverter(_Fun fun, _T &x) : _M_fun(fun), _M_x(x) {}

          bool convert(const vm::Value &value) const { return _M_fun(value.r(), _M_x); }
        };

        template<typename _Fun, typename _T>
        class FunctionObjectConverter
        {
          _Fun _M_fun; _T &_M_x;
        public:
          FunctionObjectConverter(_Fun fun, _T &x) : _M_fun(fun), _M_x(x) {}

          bool convert(const vm::Value &value) const { return _M_fun(*(value.r().ptr()), _M_x); }
        };

        inline bool convert_args_from(vm::ArgumentList &args, std::size_t i) { return true; }

        template<typename _T, typename... _Ts>
        inline bool convert_args_from(vm::ArgumentList &args, std::size_t i, _T converter, _Ts... converters)
        {
          if(!converter.convert(args[i])) return false;
          return convert_args_from(args, i + 1, converters...);
        }
      }

      //
      // Private types and private functions for a setting.
      //

      typedef std::function<bool (vm::VirtualMachine *, vm::ThreadContext *, vm::Value&, vm::RegisteredReference &)> SetterFunction;

      namespace
      {
        template<typename _T>
        inline SetterFunction bind_setter(_T &setter)
        {
          using namespace std::placeholders;
          return std::bind(&_T::set, &setter, _1, _2, _3, _4);
        }

        class IntSetter
        {
          const std::int64_t &_M_i;
        public:
          IntSetter(const std::int64_t &i) : _M_i(i) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { value = vm::Value(_M_i); return ERROR_SUCCESS; }
        };

        class FloatSetter
        {
          const double &_M_f;
        public:
          FloatSetter(const double &f) : _M_f(f) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { value = vm::Value(_M_f); return ERROR_SUCCESS; }
        };

        class ReferenceSetter
        {
          const vm::Reference &_M_r;
        public:
          ReferenceSetter(const vm::Reference &r) : _M_r(r) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { value = vm::Value(_M_r); return ERROR_SUCCESS; }
        };
      }

      int set_elem(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &r, SetterFunction fun, std::size_t i);

      namespace
      {
        inline int set_elems_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &r, const TemplateList<> &setters, std::size_t i)
        {
          if(!r.has_nil()) r.register_ref();
          value = vm::Value(r);
          return ERROR_SUCCESS;
        }

        template<typename _T, typename... _Ts>
        inline int set_elems_from(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &r, const TemplateList<_T, _Ts...> &setters, std::size_t i)
        {
          int error = set_elem(vm, context, value, r, bind_setter(setters.head), i);
          if(error != ERROR_SUCCESS) return error;
          return set_elems_from(vm, context, value, r, setters.tail, i + 1);
        }

        template<int _ObjectTypeFlag, typename... _Ts>
        inline int set_tuple_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r, TemplateList<_Ts...> setters)
        {
          vm::Reference r(vm->gc()->new_object(OBJECT_TYPE_TUPLE | _ObjectTypeFlag, TemplateParamCount<_Ts...>::value(), context));
          if(r.is_null()) return ERROR_OUT_OF_MEMORY;
          tmp_r = r;
          return set_elems_from(vm, context, value, tmp_r, setters, 0);
        }

        template<int _ObjectTypeFlag, typename... _Ts>
        class TupleSetter
        {
          TemplateList<_Ts...> _M_setters;
        public:
          TupleSetter(_Ts... setters) : _M_setters(TemplateList<_Ts...>(setters...)) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { return set_tuple_value<_ObjectTypeFlag, _Ts...>(vm, context, value, tmp_r, _M_setters); }
        };

        class ValueSetter
        {
          const vm::Value &_M_value;
        public:
          ValueSetter(const vm::Value &value) : _M_value(value) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { value = vm::Value(_M_value); return ERROR_SUCCESS; }
        };
      }

      int set_string_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r, const std::string &string);

      namespace
      {
        class StringSetter
        {
          const std::string &_M_string;
        public:
          StringSetter(const std::string &string) : _M_string(string) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r) const
          { return set_string_value(vm, context, value, tmp_r, _M_string); }
        };
      }

      int set_cstring_value(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r, const char *string, std::size_t length, bool is_length);

      namespace
      {
        class CStringSetter
        {
          const char *_M_string;
          std::size_t _M_length;
          bool _M_is_length;
        public:
          CStringSetter(const char *string) :
            _M_string(string), _M_length(0), _M_is_length(false) {}

          CStringSetter(const char *string, std::size_t length) :
            _M_string(string), _M_length(length), _M_is_length(true) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r) const
          { return set_cstring_value(vm, context, value, tmp_r, _M_string, _M_length, _M_is_length); }
        };
      }

      namespace
      {
        template<typename _Fun, typename _T>
        inline std::function<bool (vm::VirtualMachine *, vm::ThreadContext *, vm::RegisteredReference &)> bind_set_new_ref_fun(_Fun &fun, const _T &x)
        {
          return [&fun, &x](vm::VirtualMachine *vm, vm::ThreadContext *context, vm::RegisteredReference &r) -> bool {
            return fun(vm, context, r, x);
          };
        }
      }

      int set_ref_value_for_fun(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r, std::function<bool (vm::VirtualMachine *, vm::ThreadContext *, vm::RegisteredReference &)> fun);

      namespace
      {
        template<typename _Fun, typename _T>
        class FunctionReferenceSetter
        {
          _Fun _M_fun; const _T &_M_x;
        public:
          FunctionReferenceSetter(_Fun fun, const _T &x) : _M_fun(fun), _M_x(x) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { return set_ref_value_for_fun(vm, context, value, tmp_r, bind_set_new_ref_fun(_M_fun, _M_x)); }
        };

        template<typename _Fun, typename _T>
        inline std::function<vm::Object *(vm::VirtualMachine *, vm::ThreadContext *)> bind_new_object_fun(_Fun &fun, const _T &x)
        {
          return [&fun, &x](vm::VirtualMachine * vm, vm::ThreadContext * context) -> vm::Object * {
            return fun(vm, context, x);
          };
        }
      }

      int set_object_value_for_fun(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value& value, vm::RegisteredReference &tmp_r, std::function<vm::Object *(vm::VirtualMachine *, vm::ThreadContext *)> fun);

      namespace
      {
        template<typename _Fun, typename _T>
        class FunctionObjectSetter
        {
          _Fun _M_fun; const _T &_M_x;
        public:
          FunctionObjectSetter(_Fun fun, const _T &x) : _M_fun(fun), _M_x(x) {}

          int set(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::Value &value, vm::RegisteredReference &tmp_r) const
          { return set_object_value_for_fun(vm, context, value, tmp_r, bind_new_object_fun(_M_fun, _M_x)); }
        };
      }

      vm::ReturnValue return_value_for_setter_fun(vm::VirtualMachine *vm, vm::ThreadContext *context, SetterFunction fun);
    }

    //
    // Checkers and a checking function.
    //

    namespace
    {
      const priv::IntChecker cint = priv::IntChecker();
      const priv::FloatChecker cfloat = priv::FloatChecker();
      const priv::ReferenceChecker cref = priv::ReferenceChecker();

      const priv::ObjectChecker<OBJECT_TYPE_IARRAY8> ciarray8 = priv::ObjectChecker<OBJECT_TYPE_IARRAY8>();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY16> ciarray16 = priv::ObjectChecker<OBJECT_TYPE_IARRAY16>();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY32> ciarray32 = priv::ObjectChecker<OBJECT_TYPE_IARRAY32>();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY64> ciarray64 = priv::ObjectChecker<OBJECT_TYPE_IARRAY64>();
      const priv::ObjectChecker<OBJECT_TYPE_SFARRAY> csfarray = priv::ObjectChecker<OBJECT_TYPE_SFARRAY>();
      const priv::ObjectChecker<OBJECT_TYPE_DFARRAY> cdfarray = priv::ObjectChecker<OBJECT_TYPE_DFARRAY>();
      const priv::ObjectChecker<OBJECT_TYPE_RARRAY> crarray = priv::ObjectChecker<OBJECT_TYPE_RARRAY>();
      const priv::ObjectChecker<OBJECT_TYPE_TUPLE> ctuple = priv::ObjectChecker<OBJECT_TYPE_TUPLE>();
      const priv::ObjectChecker<OBJECT_TYPE_IO> cio = priv::ObjectChecker<OBJECT_TYPE_IO>();

      const priv::ObjectChecker<OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE> cuiarray8 = priv::ObjectChecker < OBJECT_TYPE_IARRAY8 | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE> cuiarray16 = priv::ObjectChecker < OBJECT_TYPE_IARRAY16 | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE> cuiarray32 = priv::ObjectChecker < OBJECT_TYPE_IARRAY32 | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE> cuiarray64 = priv::ObjectChecker < OBJECT_TYPE_IARRAY64 | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE> cusfarray = priv::ObjectChecker < OBJECT_TYPE_SFARRAY | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE> cudfarray = priv::ObjectChecker < OBJECT_TYPE_DFARRAY | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE> curarray = priv::ObjectChecker < OBJECT_TYPE_RARRAY | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE> cutuple = priv::ObjectChecker < OBJECT_TYPE_TUPLE | OBJECT_TYPE_UNIQUE > ();
      const priv::ObjectChecker<OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE> cuio = priv::ObjectChecker < OBJECT_TYPE_IO | OBJECT_TYPE_UNIQUE > ();

      const priv::ForceChecker cforce = priv::ForceChecker();
      const priv::IgnoreChecker cingore = priv::IgnoreChecker();

      template<typename... _Ts>
      inline priv::TupleChecker<0, _Ts...> ct(_Ts... checkers) { return priv::TupleChecker<0, _Ts...>(checkers...); }

      template<typename... _Ts>
      inline priv::TupleChecker<OBJECT_TYPE_UNIQUE, _Ts...> cut(_Ts... checkers) { return priv::TupleChecker<OBJECT_TYPE_UNIQUE, _Ts...>(checkers...); }

      template<typename _T>
      inline priv::OptionChecker<_T, 0> coption(_T checker) { return priv::OptionChecker<_T, 0>(checker); }

      template<typename _T>
      inline priv::OptionChecker<_T, OBJECT_TYPE_UNIQUE> cuoption(_T checker) { return priv::OptionChecker<_T, OBJECT_TYPE_UNIQUE>(checker); }

      template<typename _T, typename _U>
      inline priv::EitherChecker<_T, _U, 0> ceither(_T left, _U right) { return priv::EitherChecker<_T, _U, 0>(left, right); }

      template<typename _T, typename _U>
      inline priv::EitherChecker<_T, _U, OBJECT_TYPE_UNIQUE> cueither(_T left, _U right) { return priv::EitherChecker<_T, _U, OBJECT_TYPE_UNIQUE>(left, right); }

      template<typename... _Ts>
      inline int check_args(vm::VirtualMachine *vm, vm::ThreadContext *context, vm::ArgumentList &args, _Ts... checkers)
      {
        if(args.length() != priv::arg_count(checkers...)) return ERROR_INCORRECT_ARG_COUNT;
        return priv::check_args_from(vm, context, args, 0, checkers...);
      }
    }

    //
    // Converters and a converting function.
    //

    namespace
    {
      inline priv::IntConverter toint(std::int64_t &i) { return priv::IntConverter(i); }

      inline priv::FloatConverter tofloat(double &f) { return priv::FloatConverter(f); }

      inline priv::ReferenceConverter toref(vm::Reference &r) { return priv::ReferenceConverter(r); }

      const priv::NothingConverter tonothing = priv::NothingConverter();

      template<typename... Ts>
      inline priv::TupleConverter<Ts...> tot(Ts... converters) { return priv::TupleConverter<Ts...>(converters...); }

      template<typename _T>
      inline priv::OptionConverter<_T> tooption(_T converter, bool &some_flag)
      { return priv::OptionConverter<_T>(converter, some_flag); }

      template<typename _T, typename _U>
      inline priv::EitherConverter<_T, _U> toeither(_T left, _U right, bool &right_flag)
      { return priv::EitherConverter<_T, _U>(left, right, right_flag); }

      inline priv::StringConverter tostr(std::string &string) { return priv::StringConverter(string); }

      template<typename... _Ts>
      inline bool convert_args(vm::ArgumentList &args, _Ts... converters)
      { return priv::convert_args_from(args, 0, converters...); }
    }

    //
    // Setters and return value functions.
    //

    namespace
    {
      inline priv::IntSetter vint(const std::int64_t &i) { return priv::IntSetter(i); }

      inline priv::FloatSetter vfloat(const double &f) { return priv::FloatSetter(f); }

      inline priv::ReferenceSetter vref(const vm::Reference &r) { return priv::ReferenceSetter(r); }

      template<typename... _Ts>
      inline priv::TupleSetter<0, _Ts...> vt(_Ts... setters) { return priv::TupleSetter<0, _Ts...>(setters...); }

      template<typename... _Ts>
      inline priv::TupleSetter<OBJECT_TYPE_UNIQUE, _Ts...> vut(_Ts... setters) { return priv::TupleSetter<OBJECT_TYPE_UNIQUE, _Ts...>(setters...); }

      inline priv::ValueSetter v(const vm::Value &value) { return priv::ValueSetter(value); }

      inline priv::StringSetter vstr(const std::string &string) { return priv::StringSetter(string); }

      inline priv::CStringSetter vcstr(const char *string) { return priv::CStringSetter(string); }

      inline priv::CStringSetter vcstr(const char *string, std::size_t length) { return priv::CStringSetter(string, length); }

      const priv::TupleSetter<0, priv::IntSetter> vnone = priv::TupleSetter<0, priv::IntSetter>(priv::IntSetter(0));

      template<typename _T>
      inline priv::TupleSetter<0, priv::IntSetter, _T> vsome(_T setter) { return vt(vint(1), setter); }

      const priv::TupleSetter<OBJECT_TYPE_UNIQUE, priv::IntSetter> vunone = priv::TupleSetter<OBJECT_TYPE_UNIQUE, priv::IntSetter>(priv::IntSetter(0));

      template<typename _T>
      inline priv::TupleSetter<OBJECT_TYPE_UNIQUE, priv::IntSetter, _T> vusome(_T setter) { return vut(vint(1), setter); }

      template<typename _T>
      inline priv::TupleSetter<0, priv::IntSetter, _T> vleft(_T setter) { return vt(vint(0), setter); }

      template<typename _T>
      inline priv::TupleSetter<0, priv::IntSetter, _T> vright(_T setter) { return vt(vint(1), setter); }

      template<typename _T>
      inline priv::TupleSetter<OBJECT_TYPE_UNIQUE, priv::IntSetter, _T> vuleft(_T setter) { return vut(vint(0), setter); }

      template<typename _T>
      inline priv::TupleSetter<OBJECT_TYPE_UNIQUE, priv::IntSetter, _T> vuright(_T setter) { return vut(vint(1), setter); }

      template<typename _T>
      inline vm::ReturnValue return_value(vm::VirtualMachine *vm, vm::ThreadContext *context, _T setter)
      { return priv::return_value_for_setter_fun(vm, context, priv::bind_setter(setter)); }

      inline vm::ReturnValue error_return_value(int error)
      { return vm::ReturnValue::error(error); }

      template<typename _T>
      inline vm::ReturnValue return_value_with_errno(vm::VirtualMachine *vm, vm::ThreadContext *context, _T setter, int error_num)
      {
        vm::letin_errno() = error_num;
        return return_value(vm, context, setter);
      }

      template<typename _T>
      inline vm::ReturnValue return_value_with_errno(vm::VirtualMachine *vm, vm::ThreadContext *context, _T setter)
      { return return_value_with_errno(vm, context, setter, errno); }
    }
  }
}

#endif
