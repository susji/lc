# lc

`lc` is a small self-hosting toy compiler. This means it can compile itself. The
language closely resembles a simplified subset of ANSI C. `lc` targets only
GNU/Linux and x86. The goal was to support just enough features to make
self-hosting a reasonable effort with close to zero emphasis on general
robustness, feature completeness, and accurate ANSI C semantics.

To see the exact subset that `lc` supports, check out [the code](lc.c). If you
do not see a feature used in the sources, most likely it is unsupported. Some
things, even if we might parse and try compiling them, will behave wrongly. As
an example, we treat all integers as word-sized.

## Bootstrapping and self-hosting

We want to produce `lc`, a version of the compiler compiled by the compiler
itself. Bootstrapping is done by using any reasonably compliant C compiler to
produce `lc0`. We self-compile the full compiler twice to ensure that `lc0` and
`lc1` produce identical output. The steps are these:

1. Compile the `lc0` executable with some existing C compiler
2. Produce `lc1.s` with `lc0`
3. Assemble and link `lc1.s` to create the `lc1` executable
4. Produce `lc.s` with `lc1`
5. Assemble and link `lc.s` to create the `lc` executable
6. Verify that `lc1.s` and `lc.s` are equal

For details, see [the Makefile](Makefile).
