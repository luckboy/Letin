# Letin programming language

## Copyright and license

Copyright (C) 2015-2016, 2019 Łukasz Szpakowski.  
This documentation is licensed under the Creative Commons Attribution-ShareAlike 4.0
International Public License.

## General

The Letin programming language is a simple programming language for the Letin virtual machine.
This programming language is similar to an assembler. The instructions of this programming
language directly corresponds the instructions of the virtual machine.

## Basic syntax

The Letin programming language has defined the elements of basic syntax. These elements are
presented in the following list:

* literals
* names
* identifiers
* function arguments
* local variable
* whitespaces
* comments

### Literals

Literals in the Letin programming language are numbers or strings. Numbers can be integer
numbers or floating-point numbers. Integer numbers can represented by single characters.
Integer numbers can be written in the decimal system, the octal system, the hexadecimal system.
Also, floating-point number can be written in the science notation. The syntax of number
literal is:

    <number> ::= <int> | <float>
    <integer> ::= ("1" | ... | "9") ("0" | ... | "9")*
                | "0" ("0" | ... | "7")*
                | "0" ("X" | "x") ("0" | ... | "9" | "A" | ... | "F" | "a" | ... | "f")+ 
                | "'" (<any_char_except_backslash_and_apostrophe> | <escape_seq>) "'"
    <float> ::= ("0" | ... | "9")+ "." ("0" | ... | "9")*
                [("E" | "e") ["+" | "-"] ("0" | ... | "9")+]
              | ("0" | ... | "9")+ ("E" | "e") ["+" | "-"] ("0" | ... | "9")+
    <escape_seq> ::= "\\" ("a" | "b" | "t" | "n" | "v" | "f" | "r" | "\\" | "\"" | "'")
                   | "\\" [["0" | ... | "3"] ("0" | ... | "7")] ("0" | ... | "7")

A string can represent an array of 8-bit integer numbers. The syntax of string literal is:

    <string> ::= '"' (<any_char_except_backslash_and_quote> | <escape_seq>)* '"'

### Names and identifiers

Names are used to identify instructions and object types. The names syntax is:

    <name> ::= ("A" | ... | "Z" | "a" | ... | "z")
               ("A" | ... | "Z" | "a" | ... | "z" | "0" | ... | "9")*

Identifiers are used to identify functions, global variables, macros. The identifier syntax is:

    <ident> ::= ("_" | "A" | ... | "Z" | "a" | ... | "z" | "$") 
                ("_" | "A" | ... | "Z" | "a" | ... | "z" | "0" | ... | "9" | "$" | ".")*
              | "`" <any_char_except_grave_and_newline>+ "`"

### Arguments and local variables

The Letin programming language has the special identifier for function arguments and local
variables. The first identifier will be called the function argument identifier. This keyword
consists of the a prefix and an unsigned integer number. The syntax of the argument keyword is:

    <a> ::= "a" ("0" | ... | "9")+

The second identifier consists of the lv prefix and an unsigned integer number. This identifier
will be called the local variable identifier. The syntax of local variable identifier is:

    <lv> ::= "lv" ("0" | ... | "9")+

### Pop arguments and expression values

The Letin programming language has the special identifier for pop arguments and expression
values. The first identifier will be called the pop argument identifier. The syntax of the
pop argument keyword is:

    <pp> ::= "pp"

The second identifier consists of the ev prefix and an unsigned integer number. This identifier
will be called the expression value identifier. The syntax of local variable identifier is:

    <ev> ::= "ev" ("0" | ... | "9")+

### Whitespace characters

The Letin programming language has the two kinds of the whitespace characters. Characters of
the first kind are the space, the tabulator, the carriage return. Character of the second kind
is the newline. A newline sequence is used as separator for example definitions and
instructions. A newline sequence can have other whitespace characters. The syntax of newline
sequence is:

    <nl> ::= "\n" ((" " | "\t" | "\r")* "\n")*

### Comments

The comments in the Letin programming language are derived from the C++ programming language.
The comment syntax is:

    <comment> ::= <single_line_comment> | <multi_line_comment>
    <single_line_comment> ::= "//" <any_char_except_newline>*
    <multi_line_comment> ::= "/*" <any_char>* "*/"

## Expressions

Expressions in the Letin programming language is statically evaluated by a compiler. A value of
expression can be an integer number or a floating-point number. An expression can contain an
identifier that refers to a defined macro. The identifier of expression doesn't refer a global
variable. The expression syntax is:

    <expr> ::= <unary_op> <expr>
             | <expr> <binary_op> <expr>
             | <expr> "?" <expr> ":" <expr>
             | <number>
             | <ident>
             | "(" <expr> ")"

The operators are presented in the following table:

| Operator     | Arity   | Priority | Associate | Description                    |
|:------------ |:------- |:-------- |:--------- |:------------------------------ |
| +            | unary   | 12       | left      | Unary plus.                    |
| -            | unary   | 12       | left      | Negation.                      |
| ~            | unary   | 12       | left      | Bitwise negation.              |
| !            | unary   | 12       | left      | Logical negation.              |
| *            | binary  | 11       | left      | Multiplication.                |
| /            | binary  | 11       | left      | Division.                      |
| %            | binary  | 11       | left      | Remainder.                     |
| +            | binary  | 10       | left      | Addition.                      |
| -            | binary  | 10       | left      | Subtraction.                   |
| <<           | binary  | 9        | left      | Left shift.                    |
| >>           | binary  | 9        | left      | Right shift.                   |
| <            | binary  | 8        | left      | Less than.                     |
| >=           | binary  | 8        | left      | Greater than or equal to.      |
| >            | binary  | 8        | left      | Greater than.                  |
| <=           | binary  | 8        | left      | Less than or equal to.         |
| ==           | binary  | 7        | left      | Equal to.                      |
| !=           | binary  | 7        | left      | Not equal to.                  |
| &            | binary  | 6        | left      | Bitwise conjunction.           |
| ^            | binary  | 5        | left      | Bitwise exclusive alternative. |
| &#124;       | binary  | 4        | left      | Bitwise alternative.           |
| &&           | binary  | 3        | left      | Logical conjunction.           |
| &#124;&#124; | binary  | 2        | left      | Logical alternative.           |
| ? :          | ternary | 1        | right     | Condition operator.            |

## Functions

Each function has to have a specified body that contains instructions of virtual machine. The
function also has to have a specified number of arguments. The number of arguments has to be
specified by a number of an function argument identifier. Functions also have identifiers. The
syntax of function definition is:

    <fun_def> ::= [<annotations> [<nl>]] [".public" [<nl>]] <ident> "(" <a> ")" "=" [<nl>] "{"
                  <nl> <fun_lines> <nl> "}"
    <fun_lines> ::= <fun_lines> <nl> <fun_line>
                  | <fun_line>
    <fun_line> ::= <ident> ":" <instr>
                 | <ident> ":"
                 | <instr>

The example function definition is:

    f(a3) = {
            let iadd(a0, a1)
            in
            ret imul(lv0, a2)
    }

Functions can be public or private. Public functions are visible for other programs and other
libraries if the program or the library with the public functions is compiled as relocatable.
Private functions aren't visible. Functions implicitly are private. If a function is defined
with the .public modifier, a function is public. Functions also can have annotations.

### Instructions

An instruction can have an operation that can have arguments. An instruction without an
operation also can have arguments. An instruction or an operation can have one argument or two
arguments. The last argument of the jump instruction or the jc instruction is a label that can
be defined in a function body. The instruction syntax is:

    <instr> ::= <name> ["(" <lv>" )"] <op> <arg> "," <arg>
              | <name> ["(" <lv> ")"] <op> <arg>
              | <name> ["(" <lv> ")"] <op> "(" <arg> "," <arg> ")"
              | <name> ["(" <lv> ")"] <op> "(" <arg> ")"
              | <name> ["(" <lv> ")"] <op> "(" ")"
              | <name> <arg> "," <arg>
              | <name> <arg>
              | <name>

The lettuple instruction also has to have a specified number of local variables that is
specified by the local variable identifier. The example instructions are:

    let iadd(lv0, a)
    lettuple(lv2) tuple()
    jump label
    jc a0, label

The iload2 instruction and the fload2 instruction can have two arguments or one an argument as
immediate value. This immediate value can be a 64-bit integer number or a double-precision
floating-point number.

### Instruction arguments

An instruction argument can be used in an instruction or an operation. A number value of
instruction argument can be written by use of an expression. The syntax of instruction argument
is:

    <arg> ::= <number_arg> | <fun_index_arg> | <fun_index_arg> | <a_arg> | <lv_arg> |
              <ident_arg> | <pp_arg> | <ev_arg>
    <number_arg> ::= <expr>
    <fun_index_arg> ::= "&" <ident>
    <native_fun_index_arg> ::= "&#" <ident>
    <a_arg> ::= <a>
    <lv_arg> ::= <lv>
    <ident_arg> ::= <ident>
    <pp_arg> ::= <pp>
    <ev_arg> ::= <ev>

An instruction argument can have one of six types. The relationship between the syntax of
instruction argument and the types of instruction arguments is presented in the following
table:

| Syntax rule          | Argument type | Description                                  |
|:-------------------- |:------------- |:-------------------------------------------- |
| number_arg           | IMM           | Number as immediate value.                   |
| fun_index_arg        | IMM           | Function index as immediate value.           |
| native_fun_index_arg | IMM           | Index of native function as immediate value. |
| a_arg                | ARG           | Function argument.                           |
| lv_arg               | LVAR          | Local variable.                              |
| ident_arg            | GVAR          | Global variable.                             |
| pp_arg               | POP           | Pop argument.                                |
| ev_arg               | EVAL          | Expression value.                            |

The ident_arg syntax rule also applies to the last argument of the jump instruction or the
jc instruction.

The ev_arg syntax rule also applies to the argument of the pop instruction.

### Annotations

Annotations specify an additional information about a functions. Each function can have many
annotations. An annotation just have name. The syntax of annotations is:

    <annotations> := <annotations> <nl> <annotation>
                   | <annotation>
    <annotation> := "@" <name>

Annotations can specify whether a function can be lazily evaluated. The annotations are
presented in the following table:

| Name         | Evaluation strategy                        | Description                                            |
|:------------ |:------------------------------------------ |:------------------------------------------------------ |
| eager        | default_eval_strategy & ~LAZY              | Evaluation strategy of function is eager.              |
| lazy         | default_eval_strategy &#124; LAZY          | Evaluation strategy of function is lazy.               |
| memoized     | default_eval_strategy & ~MEMO              | Evaluation strategy of function with memoization.      |
| unmemoized   | default_eval_strategy &#124; MEMO          | Evaluation strategy of function without memoization.   |
| onlyeager    | 0                                          | Evaluation strategy of function is only eager.         |
| onlylazy     | (default_eval_strategy &#124; LAZY) & LAZY | Evaluation strategy of function is only lazy.          |
| onlymemoized | (default_eval_strategy &#124; MEMO) & MEMO | Evaluation strategy of function with only memoization. |

The default_eval_strategy in the above table is a variable of features of the default
evaluation strategy. If the annotations with the only prefix are occurs together, features of
evaluation strategy of a function are all features of evaluation strategy of these
annotations.

## Global variables

Each global variable has to have a value that can be for example a number. Also, global
variables can be public or private how functions. Also, it can be specified by the .public
keyword. The syntax of definition of global variable is:

    <var_def> ::= [".public"] [<nl>] <ident> [<nl>] "=" <value>

The example definitions of global variables are:

    a = 1
    b = 1.5
    c = [1, 2, 3]
    d = (4, 5, 6)

### Values

A value can be an integer number or a floating-point number or a function index or an index of
native function or a reference to an object. A reference of the value refers to an object of
the value. The syntax of value is:

    <value> ::= <expr>
              | "&" <ident>
              | "#&" <ident>
              | <object>

Also, a value can be written by use of a expression for numbers.

### Objects

Each object has a type that specifies for example whether this object is an array or a tuple.
An object in the Letin programming language consists of a name of object type and a list of
values. Also, an object can be written without a name of object type. The object syntax is:

    <object> ::= <iarray8> | <array> | <iarray64> | <tuple>
    <iarray8> ::= <string>
    <array> ::= <name> "[" [<nl>] <values> [<nl>] "]"
    <iarray64> ::= "[" [<nl>] <values> [<nl>] "]"
    <tuple> ::= "(" [<nl>] <values> [<nl>] ")"
    <values> ::= <values2> | ""
    <values2> ::= <values2> [<nl>] "," [<nl>] <value>
                | <value>

The names of object types with the corresponding syntax rules are presented in the following
table:

| Name     | Syntax rule | Description                                       |
|:-------- |:----------- |:------------------------------------------------- |
| iarray8  | iarray8     | Array of 8-bit integer numbers.                   |
| iarray16 |             | Array of 16-bit integer numbers.                  |
| iarray32 |             | Array of 32-bit integer numbers.                  |
| iarray64 | iarray64    | Array of 64-bit integer numbers.                  |
| sfarray  |             | Array of single-precision floating-point numbers. |
| dfarray  |             | Array of double-precision floating-point numbers. |
| rarray   |             | Array of references.                              |
| tuple    | tuple       | Tuple.                                            |

An array of 8-bit integer numbers can be represented by a string. If an object type is the
iarray32 object type or the iarray64 object type or the tuple object type, an element of the
object can be a function index.

## Entry

Each program has to have a specified start function. It is specified by the entry definition.
The entry definition has one argument that is a function identifier. This identifier has to be
defined in a program. The syntax of entry definition is:

    <entry_def> ::= ".entry" ident

If the entry definition doesn't occurs in any compiled source file, the compilation result is a
library. A library hasn't a specified start function.

## Macros

The Letin programming language allows a programmer to define macros. A macro definition has to
contain an identifier and an expression. Each macro has a value that is statically evaluated by
a compiler. Macros can't have arguments. The syntax of macro definition is:

    <value_def> ::= ".define" <ident> <expr>

The example macro definitions are:

    .define PI  3.14
    .define a   2
    .define a   3
    .define c   a + b

## File inclusions

Files also can be included in other source files. These included files also will be compiled by
a compiler. Each directive of file inclusion has a string that refers the included file. The
syntax of file inclusion is:

    <include> ::= ".include" <string>
