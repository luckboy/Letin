/****************************************************************************
 *   Copyright (C) 2014 Łukasz Szpakowski.                                  *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_CONST_HPP
#define _LETIN_CONST_HPP

namespace letin
{
  const int VALUE_TYPE_INT =            0;
  const int VALUE_TYPE_FLOAT =          1;
  const int VALUE_TYPE_REF =            2;
  const int VALUE_TYPE_PAIR =           3;
  const int VALUE_TYPE_ERROR =          -1;

  const int OBJECT_TYPE_IARRAY8 =       0;
  const int OBJECT_TYPE_IARRAY16 =      1;
  const int OBJECT_TYPE_IARRAY32 =      2;
  const int OBJECT_TYPE_IARRAY64 =      3;
  const int OBJECT_TYPE_SFARRAY =       4;
  const int OBJECT_TYPE_DFARRAY =       5;
  const int OBJECT_TYPE_RARRAY =        6;
  const int OBJECT_TYPE_TUPLE =         7;
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
  
  const int NATIVE_FUN_ATOI =           0;
  const int NATIVE_FUN_ITOA =           1;
  const int NATIVE_FUN_ATOF =           2;
  const int NATIVE_FUN_FTOA =           3;
}

#endif
