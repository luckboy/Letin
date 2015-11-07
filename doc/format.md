# Letin file format

## Copyright and license

Copyright (C) 2015 Łukasz Szpakowski.  
This documentation is licensed under the Creative Commons Attribution-ShareAlike 4.0
International Public License.

## General

The Letin file format is a file format of a program for the Letin virtual machine. This format
contains elements of the program so that this virtual machine can load and execute the program.
This format also is portable.

## Numbers

The Letin file format uses integer numbers and floating-point numbers. Integer numbers and
floating-point numbers are stored in the big-endian system. This file format uses the IEEE 754
standard to store floating-point numbers. This file format uses the number types which are
represented in the following table:

| Name   | Bytes | Description                                     |
|:------ |:----- |:----------------------------------------------- |
| int8   | 1     | Type of 8-bit signed integer number.            |
| int16  | 2     | Type of 16-bit signed integer number.           |
| int32  | 4     | Type of 32-bit signed integer number.           |
| int64  | 8     | Type of 64-bit signed integer number.           |
| uint8  | 1     | Type of 8-bit unsigned integer number.          |
| uint16 | 2     | Type of 16-bit unsigned integer number.         |
| uint32 | 4     | Type of 32-bit unsigned integer number.         |
| uint64 | 8     | Type of 64-bit unsigned integer number.         |
| float  | 4     | Type of single-precision floating-point number. |
| double | 8     | Type of double-precision floating-point number. |

These type names will be used to the structure descriptions. A type of array of this type
elements is written as this type name with a number of elements in brackets. For example, the
array of the 8-bit unsigned integer numbers are written as: uint8\[8\].

## Format structure

The structure of the Letin file format consists of the blocks. These blocks are aligned to 8
bytes. These blocks in the order are presented in the following list:

* header
* function table
* variable table
* code
* data
* relocation table
* symbol table
* table of function information

### Header

The first block is the header that contains the signature of the Letin file format and other
information about a program. The header structure is presented in the following table:

| Name           | Type        | Description                                       |
|:-------------- |:----------- |:------------------------------------------------- |
| magic          | uint8\[8\]  | File signature.                                   |
| flags          | uint32      | Flags of program file.                            |
| entry          | uint32      | Index of start function.                          |
| fun_count      | uint32      | Number of functions.                              |
| var_count      | uint32      | Number of variables.                              |
| code_size      | uitn32      | Code size in instructions.                        |
| data_size      | uint32      | Data size in bytes.                               |
| reloc_count    | uint32      | Number of relocations.                            |
| symbol_count   | uint32      | Number of symbols.                                |
| fun_info_count | uint32      | Number of table elements of function information. |
| reserved       | uint32\[1\] | Reserved.                                         |

The Letin virtual always checks the file signature before a loading of program. The elements of
this signature are presented in the following table:

| Name   | Index | Value |
|:------ |:----- |:----- |
| MAGIC0 | 0     | 0x33  |
| MAGIC1 | 1     | 'L'   |
| MAGIC2 | 2     | 'E'   |
| MAGIC3 | 3     | 'T'   |
| MAGIC4 | 4     | 0x77  |
| MAGIC5 | 5     | 'I'   |
| MAGIC6 | 6     | 'N'   |
| MAGIC7 | 7     | 0xff  |

The flags of program file can have the flags which are presented in the following table:

| Name               | Bit   | Description                                   |
|:------------------ |:----- |:--------------------------------------------- |
| LIBRARY            | 0     | File program is library.                      |
| RELOCATABLE        | 1     | File program is relocatable.                  |
| NATIVE_FUN_SYMBOLS | 2     | File program has symbols of native functions. |
| FUN_INFOS          | 3     | File program has information of functions.    |

### Function table

The function table is a block that contains function descriptions. The function description
has the structure that is presented the following table:

| Name        | Type   | Description                               |
|:----------- |:------ |:----------------------------------------- |
| addr        | uint32 | Address of first instruction of function. |
| arg_count   | uint32 | Number of function arguments.             |
| instr_count | uint32 | Number of function instructions.          |

Functions of the function table are indexed from zero.

### Variable table

The variable table is a block that contains variable values. The structure of a variable is
presented in the following table:

| Name         | Type                    | Description                  |
|:------------ |:----------------------- |:---------------------------- |
| type         | int32                   | Number of value type.        |
|              | uint32                  | Pad field.                   |
| i / f / addr | int64 / double / uint64 | Number or address to object. |

Also, variable of the variable table are indexed from zero. 

The third field can be one of three fields. It depends from the value type. For example, the
third field is the i field if the value of the type field is the number of the INT value type.
The following table presents the relationship between the value types and these fields:

| Name of value type | Field name | Field type | Field description                       |
|:------------------ |:---------- |:---------- |:--------------------------------------- |
| INT                | i          | int64      | Integer number.                         |
| FLOAT              | f          | double     | Double-precision floating-point number. |
| REF                | addr       | uint64     | Address to object.                      |

### Code

The fourth block is the instruction table that is called the code. The code contains
instructions which are addressed from zero. Instructions also are stored in the big-endian
system.

### Data

The fifth block is the data that contains object descriptions. These object descriptions also
are aligned 8 bytes. An object address has to be a number of the first byte of the object.
Objects of this table is addressed from zero. The object structure is presented in the
following table:

| Name             | Type                                   | Description            |
|:---------------- |:-------------------------------------- |:---------------------- |
| type             | int32                                  | Number of object type. |
| length           | uint32                                 | Object length.         |
| is8 / is16 / ... | int8\[length\] / int16\[length\] / ... | Elements.              |

The third field is specified by the object type. For example, the third field is the is8 field
if the value of the type field has the value of the IARRAY8 object type. This relationship
between the object types and third field is presented in the following table:

| Name of object type | Field name | Field type                          | Field description                                             |
|:------------------- |:---------- |:----------------------------------- |:------------------------------------------------------------- |
| IARRAY8             | is8        | int8\[length\]                      | Elements of array of 8-bit integer numbers.                   |
| IARRAY16            | is16       | int16\[length\]                     | Elements of array of 16-bit integer numbers.                  |
| IARRAY32            | is32       | int32\[length\]                     | Elements of array of 32-bit integer numbers.                  |
| IARRAY64            | is64       | int64\[length\]                     | Elements of array of 64-bit integer numbers.                  |
| SFARRAY             | sfs        | float\[length\]                     | Elements of array of single-precision floating-point numbers. |
| DFARRAY             | dfs        | double\[length\]                    | Elements of array of double-precision floating-point numbers. |
| RARRAY              | rs         | uint32\[length\]                    | Elements of array of reference addresses.                     |
| TUPLE               | tes        | (int64 / double / uint64)\[length\] | Tuple elements.                                               |
| TUPLE               | tets       | uint8\[length * 8 + length\]        | Types of tuple element.                                       |

Two last fields in the above table are tuple elements and types of tuple elements. A tuple
element and a type of tuple element create a value of a tuple element. The following table
presents how this element and type of this element create this value:

| Name of value field | Expression                            |
|:------------------- |:------------------------------------- |
| type                | object.tets\[object.length * 8 + i\]  |
| i / f / addr        | object.tes\[i\]                       |

The object in the expressions of the above table is a variable of the object structure. The i
variable in the expressions of the above table is an index of the object element.

## Relocation table

The relocation table contains relocation descriptions which are used to a program relocation.
This table occurs if flags of program file have the RELOCATABLE flag. A relocation of program
is a process that changes a function indexes and/or a variable indexes. A relocation
description specifies how an instruction argument or an object element or a variable value will
be modified during a relocation. The structure of relocation description is presented in the
following table:

| Name   | Type   | Description                                               |
|:------ |:------ |:--------------------------------------------------------- |
| type   | uint32 | Number of relocation type.                                |
| addr   | uint32 | Instruction address or element address or variable index. |
| symbol | uint32 | Symbol index.                                             |

The field of relocation type specifies whether an instruction argument or an object element
will be modified during a relocation. The relocation types are presented in the following
table:

| Name            | Number | Description                                                             |
|:--------------- |:------ |:----------------------------------------------------------------------- |
| ARG1_FUN        | 0      | Relocation of first instruction argument for function index.            |
| ARG2_FUN        | 1      | Relocation of second instruction argument for function index.           |
| ARG1_VAR        | 2      | Relocation of first instruction argument for variable index.            |
| ARG2_VAR        | 3      | Relocation of second instruction argument for variable index.           |
| ELEM_FUN        | 4      | Relocation of object element for function index.                        |
| VAR_FUN         | 5      | Relocation of variable value for function index.                        |
| ARG1_NATIVE_FUN | 6      | Relocation of first instruction argument for index of native function.  |
| ARG2_NATIVE_FUN | 7      | Relocation of second instruction argument for index of native function. |
| ELEM_NATIVE_FUN | 8      | Relocation of object element for index of native function.              |
| VAR_NATIVE_FUN  | 9      | Relocation of variable value for index of native function.              |

A relocation for a function index modifies a value that has the function index. A relocation
for a variable index modifies a value that has the variable index. A relocation for an index of
native function modifies a value that has the index of native function. The relocation for the
index of native function can occur in the relocation table if flags of program file have the
NATIVE_FUN_SYMBOLS flag.

A symbolic relocation has a symbol index that refers to the symbol. This symbol has the
function index or the variable index that replaces the instruction argument or the element
object or the variable value. A type number of a symbolic relocation is a bitwise alternative
of the type number and the SYMBOLIC type flag number. The SYMBOLIC type flag has value 256.

## Symbol table

The symbol table contains symbol descriptions. These symbol descriptions also are aligned 8
bytes. If flags of program file have the RELOCATABLE flag, this table is occurs. The structure
of symbol description is presented in the following table:

| Name   | Type           | Description                       |
|:------ |:-------------- |:--------------------------------- |
| index  | uint32         | Function index or variable index. |
| length | uint16         | Length of symbol name.            |
| type   | uint8          | Number of symbol type.            |
| name   | int8\[length\] | Characters of symbol name.        |

The field of symbol type specifies whether a symbol is a function symbol or a variable symbol.
The symbol types are presented in the following table:

| Name       | Number | Description                |
|:---------- |:------ |:-------------------------- |
| FUN        | 0      | Function symbol.           |
| VAR        | 1      | Variable symbol.           |
| NATIVE_FUN | 2      | Symbol of native function. |

Symbols can be undefined or defined. Defined symbols have indexes of defined functions or
indexes of defined variables. If a symbol is defined, the type number of the symbol is a
bitwise alternative of the type number and the DEFINED type flag number. The DEFINED type flag
has value 16. Symbol of native function mustn't be defined.

## Table of function information

The table of function information contains descriptions of function information. If program
flags have the FUN_INFOS flag, this table occurs. The structure of description of function
information is presented in the following table:

| Name               | Type   | Description                              |
|:------------------ |:------ |:---------------------------------------- |
| fun_index          | uint32 | Function index.                          |
| eval_strategy      | uint8  | Features of evaluation strategy.         |
| eval_strategy_mask | uint8  | Mask of features of evaluation strategy. |
| reserved\[6\]      | uint8  | Reserved.                                |

The eval_strategy field and the eval_strategy_mask specify an evaluation strategy of a
function. The evaluation strategy of a function can be specified by features of evaluation
strategy. The features of evaluation strategy are presented in the following table:

| Name | Bit | Description      |
|:---- |:--- |:---------------- |
| LAZY | 0   | Lazy.            |
| MEMO | 1   | Memoization.     |

The features of the evaluation strategy of a function is calculated by the following
expression:

    (default_eval_strategy | fun_info.eval_strategy) & fun_info.eval_strategy_mask

The default_eval_strategy in the above expression is a variable of features of the default
evaluation strategy. The fun_info in the above expression is a variable of structure of
function information.
