# Letin virtual machine

## Copyright and license

Copyright (C) 2015 Łukasz Szpakowski.  
This documentation is licensed under the Creative Commons Attribution-ShareAlike 4.0
International Public License.

## General

The Letin virtual machine is designed for purely functional programming languages. A code for
this virtual machine is executed without side-effects except modifications of stack and
registers. The conception of this virtual is based on the let expression from functional
programming languages.

## Values and objects

The Letin virtual machine is a 64-bit machine virtual and operates on values and objects.
Values and objects have own types.

### Values

The Letin virtual machine can operate two kinds of values. A value of the first of these is a
value that can be used in for example variables. This value has one of the types which are
presented in the following table:

| Name  | Number | Description                                     |
|:----- |:------ |:----------------------------------------------- |
| INT   | 0      | Type of 64-bit integer number.                  |
| FLOAT | 1      | Type of double-precision floating-point number. |
| REF   | 2      | Type of reference to object.                    |
| ERROR | -1     | Type of error value.                            |

A value of the second kind is a return value that is returned by each function. This value
contains the following values:

* 64-bit integer number
* floating-point number
* reference to object
* error

An error of this value can be one of the following errors:

| Name                | Number | Description                    |
|:------------------- |:------ |:------------------------------ |
| SUCCESS             | 0      | Success (no error).            |
| NO_INSTR            | 1      | No instruction.                |
| INCORRECT_INSTR     | 2      | Incorrect instruction.         |
| INCORRECT_VALUE     | 3      | Incorrect value.               |
| INCORRECT_OBJECT    | 4      | Incorrect object.              |
| INCORRECT_FUN       | 5      | Incorrect function.            |
| EMPTY_STACK         | 6      | Empty stack.                   |
| STACK_OVERFLOW      | 7      | Stack overflow.                |
| OUT_OF_MEMORY       | 8      | Out of memory.                 |
| NO_FUN              | 9      | No function.                   |
| NO_GLOBAL_VAR       | 10     | No global variable.            |
| NO_LOCAL_VAR        | 11     | No local variable.             |
| NO_ARG              | 12     | No argument.                   |
| INCORRECT_ARG_COUNT | 13     | Incorrect number of arguments. |
| DIV_BY_ZERO         | 14     | Division by zero.              |
| INDEX_OF_OUT_BOUNDS | 15     | Index of out bounds.           |
| EXCEPTION           | 16     | Exception.                     |
| NO_ENTRY            | 17     | No entry.                      |
| NO_NATIVE_FUN       | 18     | No native function.            |
| UNIQUE_OBJECT       | 19     | Unique object.                 |
| AGAIN_USED_UNIQUE   | 20     | Again used unique object.      |

The values of the first kind will be called the values and the values of the second kind will
be called the return values.

### Objects

Objects can be considered as arrays of elements except the IO object. An object has one of
types which are presented in the following table:

| Name     | Number | Description                                               |
|:-------- |:------ |:--------------------------------------------------------- |
| IARRAY8  | 0      | Type of array of 8-bit integer numbers.                   |
| IARRAY16 | 1      | Type of array of 16-bit integer numbers.                  |
| IARRAY32 | 2      | Type of array of 32-bit integer numbers.                  |
| IARRAY64 | 3      | Type of array of 64-bit integer numbers.                  |
| SFARRAY  | 4      | Type of array of single-precision floating-point numbers. |
| DFARRAY  | 5      | Type of array of double-precision floating-point numbers. |
| RARRAY   | 6      | Type of array of references.                              |
| TUPLE    | 7      | Type of tuple.                                            | 
| IO       | 8      | Type of IO object.                                        |
| ERROR    | -1     | Type of error object.                                     |

An object of tuple is an array of values of any type. The IO object represents the world and
is used by impure native functions.

The Letin virtual machine also supports unique objects. There mustn't be more references to an
unique object than one reference. Non-unique objects will be called shared objects. A type of
unique object can be any type with the UNIQUE type flag except the type of error object. The IO
object must be an unique object. A type number of unique object is a bitwise alternative of the
type number with the number of the UNIQUE type flag. The number of the UNIQUE type flag has
value 256. If an object contains other unique object, this object has to be unique object. 

## Program

The Letin virtual machine loads and executes a program that consists of functions and global
variables. This program has to have a specified start function so that the virtual machine can
execute its.

### Functions

Each function has a specified number of arguments and instructions. Functions return a return
value. 

### Global variables

Each global variable is immutable. In other words, each global variable indeed is constant. A
global variable mustn't have a value that is a reference to an unique object.

### Start function

A start function can have one argument or two arguments. If this function has two arguments,
the second argument is the IO object and this function has to return a tuple. This tuple
contains an integer number and the IO object. The first argument of a start function is an
array of command-line arguments. An integer number of returned tuple is a status code.

## Execution

The Letin virtual machine executes instructions in a function. These instructions don't modify
any arguments and any variables except reference cancellations.

### Arguments and local variables

The Letin virtual machine allocates arguments and local variables in a stack. Each function
has access to arguments which are passed its. Also, local variables are accessible but the
function just can have access to the local variables which are allocated by this function.

Each local variable is allocated by the let instruction. An allocated variable isn't available
for the function but this variable will be available after the execution of the in instruction.
In other words, the in instruction allows for a separation between definitions of variables and
use of they. After the execution of the in instruction, the virtual machine also can allocate
local variables. After the allocation of local variable, the passed argument is popped from the
stack.

### Function invocation

Each function can be invoked by invocation instructions. An invocation instruction takes 
allocated arguments from the stack except the instruction of an argument allocation. The
instruction of an argument allocation doesn't take arguments from the stack. Each function
doesn't modify an allocated data in a stack. In other words, each functions is transparent.
A return value is converted to a value for each invocation instruction by the Letin virtual
machine.

An invocation instruction doesn't have to immediately invoke a function. The order of execution
of function invocations isn't specified. The strategy of the function evaluation also can be
any. A function doesn't have to be invoked if its result isn't necessary or its result is known
for the arguments of its invocation. Also, a time of function invocation is unspecified.

An invocation instruction allocates or returns a value that is called an invocation value. An
invocation value doesn't have to be a function result but this value can be a value that
behaves how this function result. For example, this value is the reference to the object that
has the information of the function invocation that can be used so that this function is
invoked when that is necessary. Each instruction interprets this value as the function result.

### Reference cancellation

Unique objects can be used only once in a code. The Letin virtual machine has the mechanism of
reference cancellation that prevents reuse of references to the unique objects. A reference to
an unique object is canceled if this reference was used. References is canceled by change of
value type. Therefore, the Letin virtual machine can cancel the references which are in
arguments or local variables or tuples. Reference arrays just can have references to shared
objects. If a program uses canceled reference, this program terminates with the error of
canceled reference.

## Instructions

An instruction can have arguments or an operation with or without arguments. An operation
result can be allocated or returned by an instruction. An instruction or an operation can have
two or less arguments. Each instruction argument has a specified type. If an instruction or
an operation takes an argument that hasn't a specified type, a program terminates with the
error of incorrect value type. The instruction structure is presented in the following table:

| Name   | Word bits | Description         |
|:------ |:--------- |:------------------- |
| opcode | 32        | Opcode.             |
| arg1   | 32        | First argument.     |
| arg2   | 32        | Second argument.    |

A value of the opcode field contains an instruction code and an operation code and other
information about instruction. The information in the opcode field is presented in the
following table:

| Name            | Bits  | Description                                             |
|:--------------- |:----- |:------------------------------------------------------- |
| instr           | 0-7   | Instruction code.                                       |
| op              | 8-15  | Operation code.                                         |
| arg_type1       | 16-19 | Type of first argument.                                 |
| arg_type2       | 20-23 | Type of second argument.                                |
| local_var_count | 24-31 | Number of local variables for the lettuple instruction. |

The instructions of the Letin virtual machine is presented in the following table:

| Instruction                   | Code | Description                               |
|:----------------------------- |:---- |:----------------------------------------- |
| let &lt;op&gt;                | 0x00 | Allocates local variable in stack.        |
| in                            | 0x01 | Makes allocated local variable available. |
| ret &lt;op&gt;                | 0x02 | Returns from function.                    |
| jc &lt;arg1&gt;, &lt;arg2&gt; | 0x03 | Jumps if first argument isn't zero.       |
| jump &lt;arg1&gt;             | 0x04 | Jumps.                                    |
| arg &lt;op&gt;                | 0x05 | Allocates argument in stack.              |
| retry                         | 0x06 | Again invokes current function.           |
| lettuple(n) &lt;op&gt;        | 0x07 | Allocates local variables from tuple.     |

The jump instruction jumps to an address that is sum of the next instruction address and the
argument value. The argument value is an integer number. The jump address for the jc
instruction is calculated from the second argument.

The tail recursion can be implemented by use of the retry instruction. The retry instruction
invokes a current function with an allocated arguments.

The lettuple instruction allocates local variables from a tuple that is a result of an
operation. The number of local variables is specified by the local_var_count bits in the opcode
field. If the tuple length isn't equal to the number of local variables, a program terminates
with the error of incorrect object.

### Arguments

Each instruction argument can have one of four types. These argument types specify for example
whether an instruction or an operation uses a local variable. An operation and an instruction
have the value types of arguments. The arg1 or arg2 field can be a different value for the
different argument types. These types with the accepted value types and the description of the
argument fields are presented in the following table:

| Name | Code | Accepted value types | Description of argument field | Description        |
|:---- |:---- |:-------------------- |:----------------------------- |:------------------ |
| LVAR | 0x0  | INT, FLOAT, REF      | Index of local variable.      | Local variable.    |
| ARG  | 0x1  | INT, FLOAT, REF      | Index of function argument.   | Function argument. |
| IMM  | 0x2  | INT, FLOAT           | Immediate value.              | Immediate value.   |
| GVAR | 0x3  | INT, FLOAT, REF      | Index of global variable.     | Global variable.   |

Function arguments and local variables are indexed from zero in the order of allocations. A
value of floating-point number is stored in the IEEE 754 standard for the instruction
arguments.

### Operations

The operations also can have arguments which have the accepted value types. An operation
argument can have an accepted object type. There will be used the short names of arguments
which specify an accepted value type and an accepted object type for an operation argument.
These short names are presented in the following table:

| Name  | Description                                                           |
|:----- |:--------------------------------------------------------------------- |
| i     | Integer number.                                                       |
| f     | Floating-point number.                                                |
| r     | Reference.                                                            |
| ia8   | Reference to shared array of 8-bit integer numbers.                   |
| ia16  | Reference to shared array of 16-bit integer numbers.                  |
| ia32  | Reference to shared array of 32-bit integer numbers.                  |
| ia64  | Reference to shared array of 64-bit integer numbers.                  |
| sfa   | Reference to shared array of single-precision floating-point numbers. |
| dfa   | Reference to shared array of double-precision floating-point numbers. |
| ra    | Reference to shared array of reference.                               |
| t     | Reference to shared tuple.                                            |
| uia8  | Reference to unique array of 8-bit integer numbers.                   |
| uia16 | Reference to unique array of 16-bit integer numbers.                  |
| uia32 | Reference to unique array of 32-bit integer numbers.                  |
| uia64 | Reference to unique array of 64-bit integer numbers.                  |
| usfa  | Reference to unique array of single-precision floating-point numbers. |
| udfa  | Reference to unique array of double-precision floating-point numbers. |
| ura   | Reference to unique array of reference.                               |
| ut    | Reference to unique tuple.                                            |

Some operations also take arguments which are passed in a stack. Some operations have to take
arguments which have specified object types, otherwise a program terminates with the error of
object type. The operations of the Letin virtual machine are presented in the following table:

| Operation                              | Code | Arguments  | Description                                                                               |
|:-------------------------------------- |:---- |:---------- |:----------------------------------------------------------------------------------------- |
| iload(&lt;arg&gt;)                     | 0x00 | i          | Loads integer number.                                                                     |
| iload2(&lt;arg1&gt;, &lt;arg2&gt;)     | 0x01 | i, i       | Loads integer number from (arg1 << 32) &#124; arg2.                                       |
| ineg(&lt;arg&gt;)                      | 0x02 | i          | Negates integer number.                                                                   |
| iadd(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x03 | i, i       | Adds two integer numbers.                                                                 |
| isub(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x04 | i, i       | Subtracts two integer numbers.                                                            |
| imul(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x05 | i, i       | Multiplies two integer numbers.                                                           |
| idiv(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x06 | i, i       | Divides two integer number.                                                               |
| imod(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x07 | i, i       | Calculates remainder of two integer numbers.                                              |
| inot(&lt;arg&gt;)                      | 0x08 | i          | Calculates bitwise negation.                                                              |
| iand(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x09 | i, i       | Calculates bitwise conjunction.                                                           |
| ior(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x0a | i, i       | Calculates bitwise alternative.                                                           |
| ixor(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x0b | i, i       | Calculates bitwise exclusive alternative.                                                 |
| ishl(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x0c | i, i       | Calculates left shift by second argument.                                                 |
| ishr(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x0d | i, i       | Calculates arithmetic right shift by second argument.                                     |
| ishru(&lt;arg1;&gt;, &lt;arg2&gt;)     | 0x0e | i, i       | Calculates logical right shift by second argument.                                        |
| ieq(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x0f | i, i       | Gives 1 if arg1 is equal to arg2, otherwise 0.                                            |
| ine(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x10 | i, i       | Gives 1 if arg1 isn't equal to arg2, otherwise 0.                                         |
| ilt(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x11 | i, i       | Gives 1 if arg1 is less than arg2, otherwise 0.                                           |
| ige(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x12 | i, i       | Gives 1 if arg1 is greater than or equal to arg2, otherwise 0.                            |
| igt(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x13 | i, i       | Gives 1 if arg1 is greater than arg2, otherwise 0.                                        |
| ile(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x14 | i, i       | Gives 1 if arg1 is less than or equal to arg2, otherwise 0.                               |
| fload(&lt;arg&gt;)                     | 0x15 | f          | Gives floating-point number.                                                              |
| fload2(&lt;arg1&gt;, &lt;arg2&gt;)     | 0x16 | i, i       | Gives floating-point number from (arg1 << 32) &#124; arg2.                                |
| fneg(&lt;arg&gt;)                      | 0x17 | f          | Negates floating-point number.                                                            |
| fadd(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x18 | f, f       | Adds two floating-point numbers.                                                          |
| fsub(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x19 | f, f       | Subtracts two floating-point numbers.                                                     |
| fmul(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x1a | f, f       | Multiplies two floating-point numbers.                                                    |
| fdiv(&lt;arg1&gt;, &lt;arg2&gt;)       | 0x1b | f, f       | Divides two floating-point numbers.                                                       |
| feq(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x1c | f, f       | Gives 1 if arg1 is equal to arg2, otherwise 0.                                            |
| fne(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x1d | f, f       | Gives 1 if arg1 isn't equal to arg2, otherwise 0.                                         |
| flt(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x1e | f, f       | Gives 1 if arg1 is less than arg2, otherwise 0.                                           |
| fge(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x1f | f, f       | Gives 1 if arg1 is greater than or equal to arg2, otherwise 0.                            |
| fgt(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x20 | f, f       | Gives 1 if arg1 is greater than arg2, otherwise 0.                                        |
| fle(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x21 | f, f       | Gives 1 if arg1 is less than or equal to arg2, otherwise 0.                               |
| rload(&lt;arg&gt;)                     | 0x22 | r          | Loads reference.                                                                          |
| req(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x23 | r, r       | Gives 1 if arg1 is equal to arg2, otherwise 0. Arg1 or arg2 has to be global variable.    |
| rne(&lt;arg1&gt;, &lt;arg2&gt;)        | 0x24 | r, r       | Gives 1 if arg1 isn't equal to arg2, otherwise 0. Arg1 or arg2 has to be global variable. |
| riarray8()                             | 0x25 |            | Creates shared array of 8-bit integer numbers from pushed arguments.                      |
| riarray16()                            | 0x26 |            | Creates shared array of 16-bit integer numbers from pushed arguments.                     |
| riarray32()                            | 0x27 |            | Creates shared array of 32-bit integer numbers from pushed arguments.                     |
| riarray64()                            | 0x28 |            | Creates shared array of 64-bit integer numbers from pushed arguments.                     |
| rsfarray()                             | 0x29 |            | Creates shared array of single-precision floating-point numbers from pushed arguments.    |
| rdfarray()                             | 0x2a |            | Creates shared array of double-precision floating-point numbers from pushed arguments.    |
| rrarray()                              | 0x2b |            | Creates shared array of references from pushed arguments.                                 |
| rtuple()                               | 0x2c |            | Creates shared tuple from pushed arguments.                                               |
| rianth8(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x2d | ia8, i     | Gives element of shared array of 8-bit integer numbers.                                   |
| rianth16(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x2e | ia16, i    | Gives element of shared array of 16-bit integer numbers.                                  |
| rianth32(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x2f | ia32, i    | Gives element of shared array of 32-bit integer numbers.                                  |
| rianth64(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x30 | ia64, i    | Gives element of shared array of 64-bit integer numbers.                                  |
| rsfanth(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x31 | sfa, i     | Gives element of shared array of single-precision floating-point numbers.                 |
| rdfanth(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x32 | dfa, i     | Gives element of shared array of double-precision floating-point numbers.                 |
| rranth(&lt;arg1&gt;, &lt;arg2&gt;)     | 0x33 | ra, i      | Gives element of shared array of references.                                              |
| rtnth(&lt;arg1&gt;, &lt;arg2&gt;)      | 0x34 | t, i       | Gives element of shared tuple.                                                            |
| rialen8(&lt;arg&gt;)                   | 0x35 | ia8        | Gives length of shared array of 8-bit integer numbers.                                    |
| rialen16(&lt;arg&gt;)                  | 0x36 | ia16       | Gives length of shared array of 16-bit integer numbers.                                   |
| rialen32(&lt;arg&gt;)                  | 0x37 | ia32       | Gives length of shared array of 32-bit integer numbers.                                   |
| rialen64(&lt;arg&gt;)                  | 0x38 | ia64       | Gives length of shared array of 64-bit integer numbers.                                   |
| rsfalen(&lt;arg&gt;)                   | 0x39 | sfa        | Gives length of shared array of single-precision floating-point numbers.                  |
| rdfalen(&lt;arg&gt;)                   | 0x3a | dfa        | Gives length of shared array of double-precision floating-point numbers.                  |
| rralen(&lt;arg&gt;)                    | 0x3b | ra         | Gives length of shared array of references.                                               |
| rtlen(&lt;arg&gt;)                     | 0x3c | t          | Gives length of shared tuple.                                                             |
| riacat8(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x3d | ia8, ia8   | Concatenates two shared arrays of 8-bit integer numbers.                                  |
| riacat16(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x3e | ia16, ia16 | Concatenates two shared arrays of 16-bit integer numbers.                                 |
| riacat32(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x3f | ia32, ia32 | Concatenates two shared arrays of 32-bit integer numbers.                                 |
| riacat64(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x40 | ia64, ia64 | Concatenates two shared arrays of 64-bit integer numbers.                                 |
| rsfacat(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x41 | sfa, sfa   | Concatenates two shared arrays of single-precision floating-point numbers.                |
| rdfacat(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x42 | dfa, dfa   | Concatenates two shared arrays of double-precision floating-point numbers.                |
| rracat(&lt;arg1&gt;, &lt;arg2&gt;)     | 0x43 | ra, ra     | Concatenates two shared arrays of references.                                             |
| rtcat(&lt;arg1&gt;, &lt;arg2&gt;)      | 0x44 | t, t       | Concatenates two shared tuples.                                                           |
| rtype(&lt;arg&gt;)                     | 0x45 | r          | Gives type of object.                                                                     |
| icall(&lt;arg&gt;)                     | 0x46 | i          | Invokes function of arg index and gives its result as integer number.                     |
| fcall(&lt;arg&gt;)                     | 0x47 | i          | Invokes function of arg index and gives its result as floating-point number.              |
| rcall(&lt;arg&gt;)                     | 0x48 | i          | Invokes function of arg index and gives its result as reference.                          |
| itof(&lt;arg&gt;)                      | 0x49 | i          | Converts integer number to floating-point number.                                         |
| ftoi(&lt;arg&gt;)                      | 0x4a | f          | Converts floating-point number to integer number.                                         |
| incall(&lt;arg&gt;)                    | 0x4b | i          | Invokes native function and gives its result as integer number.                           |
| fncall(&lt;arg&gt;)                    | 0x4c | i          | Invokes native function and gives its result as floating-point number.                    |
| rncall(&lt;arg&gt;)                    | 0x4d | i          | Invokes native function and gives its result as reference.                                |
| ruiafill8(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x4e | i, i       | Creates unique array of 8-bit integer numbers that is filled by arg2.                     |
| ruiafill16(&lt;arg1&gt;, &lt;arg2&gt;) | 0x4f | i, i       | Creates unique array of 16-bit integer numbers that is filled by arg2.                    |
| ruiafill32(&lt;arg1&gt;, &lt;arg2&gt;) | 0x50 | i, i       | Creates unique array of 32-bit integer numbers that is filled by arg2.                    |
| ruiafill64(&lt;arg1&gt;, &lt;arg2&gt;) | 0x51 | i, i       | Creates unique array of 64-bit integer numbers that is filled by arg2.                    |
| rusfafill(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x52 | i, f       | Creates unique array of single-precision floating-point numbers that is filled by arg2.   |
| rudfafill(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x53 | i, f       | Creates unique array of double-precision floating-point numbers that is filled by arg2.   |
| rurafill(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x54 | i, r       | Creates unique array of references that is filled by arg2.                                |
| rutfilli(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x55 | i, i       | Creates unique tuple that is filled by integer number.                                    |
| rutfillf(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x56 | i, f       | Creates unique tuple that is filled by floating-point number.                             |
| rutfillr(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x57 | i, r       | Creates unique tuple that is filled by reference.                                         |
| ruianth8(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x58 | uia8, i    | Gives element of unique array of 8-bit integer numbers and array.                         |
| ruianth16(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x59 | uia16, i   | Gives element of unique array of 16-bit integer numbers and array.                        |
| ruianth32(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x5a | uia32, i   | Gives element of unique array of 32-bit integer numbers and array.                        |
| ruianth64(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x5b | uia64, i   | Gives element of unique array of 64-bit integer numbers and array.                        |
| rusfanth(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x5c | usfa, i    | Gives element of unique array of single-precision floating-point numbers and array.       |
| rudfanth(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x5d | udfa, i    | Gives element of unique array of double-precision floating-point numbers and array.       |
| ruranth(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x5e | ura, i     | Gives element of unique array of references and array.                                    |
| rutnth(&lt;arg1&gt;, &lt;arg2&gt;)     | 0x5f | ut, i      | Gives element of unique tuple and tuple.                                                  |
| ruiasnth8(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x60 | uia8, i    | Gives unique array of 8-bit integer numbers with updated element.                         |
| ruiasnth16(&lt;arg1&gt;, &lt;arg2&gt;) | 0x61 | uia16, i   | Gives unique array of 16-bit integer numbers with updated element.                        |
| ruiasnth32(&lt;arg1&gt;, &lt;arg2&gt;) | 0x62 | uia32, i   | Gives unique array of 32-bit integer numbers with updated element.                        |
| ruiasnth64(&lt;arg1&gt;, &lt;arg2&gt;) | 0x63 | uia64, i   | Gives unique array of 64-bit integer numbers with updated element.                        |
| rusfasnth(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x64 | usfa, i    | Gives unique array of single-precision floating-point numbers with updated element.       |
| rudfasnth(&lt;arg1&gt;, &lt;arg2&gt;)  | 0x65 | udfa, i    | Gives unique array of double-precision floating-point numbers with updated element.       |
| rurasnth(&lt;arg1&gt;, &lt;arg2&gt;)   | 0x66 | ura, i     | Gives unique array of references with updated element.                                    |
| rutsnth(&lt;arg1&gt;, &lt;arg2&gt;)    | 0x67 | ut, i      | Gives unique array of tuple with updated element.                                         |
| ruialen8(&lt;arg&gt;)                  | 0x68 | uia8       | Gives length of unique array of 8-bit integer numbers and array.                          |
| ruialen16(&lt;arg&gt;)                 | 0x69 | uia16      | Gives length of unique array of 16-bit integer numbers and array.                         |
| ruialen32(&lt;arg&gt;)                 | 0x6a | uia32      | Gives length of unique array of 32-bit integer numbers and array.                         |
| ruialen64(&lt;arg&gt;)                 | 0x6b | uia64      | Gives length of unique array of 64-bit integer numbers and array.                         |
| rusfalen(&lt;arg&gt;)                  | 0x6c | usfa       | Gives length of unique array of single-precision floating-point numbers and array.        |
| rudfalen(&lt;arg&gt;)                  | 0x6d | udfa       | Gives length of unique array of double-precision floating-point numbers and array.        |
| ruralen(&lt;arg&gt;)                   | 0x6e | ura        | Gives length of unique array of references and array.                                     |
| rutlen(&lt;arg&gt;)                    | 0x6f | ut         | Gives length of unique tuple and tuple.                                                   |
| rutype(&lt;arg&gt;)                    | 0x70 | r          | Gives type of object and object.                                                          |
| ruiatoia8(&lt;arg&gt;)                 | 0x71 | uia8       | Copies unique array of 8-bit integer numbers to shared array.                             |
| ruiatoia16(&lt;arg&gt;)                | 0x72 | uia16      | Copies unique array of 16-bit integer numbers to shared array.                            |
| ruiatoia32(&lt;arg&gt;)                | 0x73 | uia32      | Copies unique array of 32-bit integer numbers to shared array.                            |
| ruiatoia64(&lt;arg&gt;)                | 0x74 | uia64      | Copies unique array of 64-bit integer numbers to shared array.                            |
| rusfatosfa(&lt;arg&gt;)                | 0x75 | usfa       | Copies unique array of single-precision floating-point numbers to shared array.           |
| rudfatodfa(&lt;arg&gt;)                | 0x76 | udfa       | Copies unique array of double-precision floating-point numbers to shared array.           |
| ruratora(&lt;arg&gt;)                  | 0x77 | ura        | Copies unique array of references to shared array.                                        |
| ruttot(&lt;arg&gt;)                    | 0x78 | ut         | Copies unique tuple to shared tuple.                                                      |
