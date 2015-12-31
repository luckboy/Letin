# Letin

Letin is virtual machine that is designed for purely functional programming languages. This
virtual machine has features which are derived from functional programming languages. This
package also contains the compiler for this virtual machine. This compiler compiles programs in
simple programming language that is similar to assembler.

This virtual machine supports eager evaluation, lazy evaluation and memoization. Letin can load
native libraries which are shared libraries. This package also contains the native libraries
for the POSIX functions and the network functions.

## Installation

You can install this package by invoke the following commands:

    mkdir build
    cd build
    cmake ..
    make install

## Compilation of program

You can compile a program by invoke the following command: 

    letinc -o <output file> [<source file> ...]

If you want to see more information about usage of the letinc program, you can invoke the
following command:

    letinc -h

## Running of program

You can run a program by invoke the following command: 

    letin <file> [<argument> ...]

If you want to see more information about usage of the letin program, you can invoke the
following command:

    letin -h

## License

This software is licensed under the GNU Lesser General Public License v3 or later. See the
LICENSE file and the GPL file for the full licensing terms.

The documentation of this software is licensed under the Creative Commons
Attribution-ShareAlike 4.0 International Public License. See the DOCLICENSE file for the full
licensing terms.
