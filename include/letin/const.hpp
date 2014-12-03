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
  const int VALUE_TYPE_FUN =            3;
  const int VALUE_TYPE_PAIR =           4;
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
  
  const int ERROR_OUT_OF_MEM =          1;
  const int ERROR_INVALID_OPCODE =      2;
  const int ERROR_INVALID_ARG =         3;
}

#endif
