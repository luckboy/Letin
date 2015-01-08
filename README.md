# Letin

Letin is virtual machine that is designed for purely functional programming languages. This
virtual machine has features which are derived from functional programming languages. This
package also contains the compiler for this virtual machine. This compiler compiles programs in
simple programming language that is similar to assembler.

## Installation

You can install this package by invoke the following commands:

    mkdir build
    cd build
    cmake ..
    make install

## Compilation of program

You can compile program by invoke the following command: 

    letinc -o <output file> [<source file> ...]

## Running of program

You can run program by invoke the following command: 

    letin <file> [<argument> ...]