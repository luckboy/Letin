/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_CONST_HPP
#define _LETIN_CONST_HPP

#ifdef ERROR_SUCCESS
#define __ERROR_SUCCESS                 ERROR_SUCCESS
#undef ERROR_SUCCESS
const int ERROR_SUCCESS =               __ERROR_SUCCESS;
#undef ERROR_SUCCESS
#undef __ERROR_SUCCESS
#endif

#ifdef ERROR_STACK_OVERFLOW
#define __ERROR_STACK_OVERFLOW          ERROR_STACK_OVERFLOW
#undef ERROR_STACK_OVERFLOW
const int ERROR_STACK_OVERFLOW =        __ERROR_STACK_OVERFLOW;
#undef ERROR_STACK_OVERFLOW
#undef __ERROR_STACK_OVERFLOW
#endif

namespace letin
{
  const int VALUE_TYPE_INT =            0;
  const int VALUE_TYPE_FLOAT =          1;
  const int VALUE_TYPE_REF =            2;
  const int VALUE_TYPE_PAIR =           3;
  const int VALUE_TYPE_CANCELED_REF =   4;
  const int VALUE_TYPE_ERROR =          -1;

  const int OBJECT_TYPE_IARRAY8 =       0;
  const int OBJECT_TYPE_IARRAY16 =      1;
  const int OBJECT_TYPE_IARRAY32 =      2;
  const int OBJECT_TYPE_IARRAY64 =      3;
  const int OBJECT_TYPE_SFARRAY =       4;
  const int OBJECT_TYPE_DFARRAY =       5;
  const int OBJECT_TYPE_RARRAY =        6;
  const int OBJECT_TYPE_TUPLE =         7;
  const int OBJECT_TYPE_IO =            8;
  const int OBJECT_TYPE_UNIQUE =        256;
  const int OBJECT_TYPE_ERROR =         -1;
  
  const int ERROR_SUCCESS =             0;
  const int ERROR_NO_INSTR =            1;
  const int ERROR_INCORRECT_INSTR =     2;
  const int ERROR_INCORRECT_VALUE =     3;
  const int ERROR_INCORRECT_OBJECT =    4;
  const int ERROR_INCORRECT_FUN =       5;
  const int ERROR_EMPTY_STACK =         6;
  const int ERROR_STACK_OVERFLOW =      7;
  const int ERROR_OUT_OF_MEMORY =       8;
  const int ERROR_NO_FUN =              9;
  const int ERROR_NO_GLOBAL_VAR =       10;
  const int ERROR_NO_LOCAL_VAR =        11;
  const int ERROR_NO_ARG =              12;
  const int ERROR_INCORRECT_ARG_COUNT = 13;
  const int ERROR_DIV_BY_ZERO =         14;
  const int ERROR_INDEX_OF_OUT_BOUNDS = 15;
  const int ERROR_EXCEPTION =           16;
  const int ERROR_NO_ENTRY =            17;
  const int ERROR_NO_NATIVE_FUN =       18;
  const int ERROR_UNIQUE_OBJECT =       19;
  const int ERROR_AGAIN_USED_UNIQUE =   20;
  const int ERROR_USER_EXCEPTION =      21;
  
  const int NATIVE_FUN_ATOI =           0;
  const int NATIVE_FUN_ITOA =           1;
  const int NATIVE_FUN_ATOF =           2;
  const int NATIVE_FUN_FTOA =           3;
  const int NATIVE_FUN_GET_CHAR =       4;
  const int NATIVE_FUN_PUT_CHAR =       5;
  const int NATIVE_FUN_GET_LINE =       6;
  const int NATIVE_FUN_PUT_STRING =     7;
  
  const int LOADING_ERROR_IO =          0;
  const int LOADING_ERROR_FORMAT =      1;
  const int LOADING_ERROR_NO_FUN_SYM =  2;
  const int LOADING_ERROR_FUN_SYM =     3;
  const int LOADING_ERROR_NO_VAR_SYM =  4;
  const int LOADING_ERROR_VAR_SYM =     5;
  const int LOADING_ERROR_RELOC =       6;
  const int LOADING_ERROR_ENTRY =       7;
  const int LOADING_ERROR_NO_RELOC =    8;
  const int LOADING_ERROR_FUN_INDEX =   9;
  const int LOADING_ERROR_VAR_INDEX =   10;
  const int LOADING_ERROR_ALLOC =       11;
}

#endif
