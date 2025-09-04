CS 300: Overview and Memory
=====================================

Type `make` to build the programs in this directory. This should work on
the department machines and in your course container (see Lab 0 for setup
instructions).

`add` and `addin`
-----------------

Run `./add 2 2` to add 2 and 2.

The `add` function’s definition is located in `addf.c`. Check out
`addf-examples.c` for some alternate definitions that work too
(amazingly).

Run `./addin FILE OFFSET A B` to add `A` and `B`, using the “add function”
located in `FILE` at byte `OFFSET`. For instance, try `./addin addf.o 64 2 2`.

Use `objdump -S addf.o` to look at the instructions in `addf.o`.
