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
represents in the following table:

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

These type names will be used to the structure descriptions. A type of array of this type is
wrote as this type name with a number of elements in brackets. For example, the array of the
8-bit unsigned integer numbers is wrote as: uint8\[8\].

## Format structure

The structure of the Letin file format consists of the blocks. These blocks are aligned to 8
bytes. These blocks in the order are presented the following list:

* header
* function table
* variable table
* code
* data

### Header

The first block is the header that contains the signature of the Letin file format and other
information about a programs. The header structure is presented in the following table:

| Name      | Type        | Description                |
|:--------- |:----------- |:-------------------------- |
| magic     | uint8\[8\]  | File signature.            |
| flags     | uint32      | Flags of program file.     |
| entry     | uint32      | Index of start function.   |
| fun_count | uint32      | Number of functions.       |
| var_count | uint32      | Number of variables.       |
| code_size | uitn32      | Code size in instructions. | 
| data_size | uint32      | Data size in bytes.        |
| reserved  | uint32\[4\] | Reserved.                  |

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

The flags of program file can have the LIBRARY flag value that specifies whether the program is
a library. The LIBRARY flag value have value 1.

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

The object in expression of the above table is a variable of the object structure. The i
variable in expression of the above table is an index of the object element.
