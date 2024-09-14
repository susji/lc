# lc

`lc` is a small self-hosting toy compiler. This means it can compile itself. The
language closely resembles a simplified subset of ANSI C. `lc` targets only
GNU/Linux and x86. The goal was to support just enough features to make
self-hosting a reasonable effort with close to zero emphasis on general
robustness, feature completeness, and accurate ANSI C semantics.

To see the exact subset that `lc` supports, check out [the code](lc.c). If you
do not see a feature used in the sources, most likely it is unsupported. Some
things, even if we might parse and try compiling them, will behave wrongly. As
an example, we treat all integers as word-sized. Also, we produce diagnostic
output for some typical errors and warnings, but very few errors actually stop
the compilation process.

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

## The implementation

I did not do any serious planning beforehand. If you browse the code starting
from the beginning, you'll see a fairly well-contained progression into lexer,
parser, tree rewriter, and code generator stages -- this is intentional. As
mentioned, I dropped every single feature and semantic concept if I could not
easily justify the effort in comparison to its benefit for self-hosting. Structs
are probably the single greatest complication but I really did not want to
simulate them with arrays even though that is a perfectly fine approach. I had
no need for floats, alternative integers, or function pointers. Adding any of
these things *individually* is easy, but the problems begin after one buys into
a *dozen* "easy things". In aggregate, they need real effort.

It would be nice to have a strict subset of ANSI C, but to me, it is not worth
the effort. Having previously implemented a subset of the classic C
preprocessor, I realized that I really did not want that side quest either. As
can be seen from the code, this means that we lose some idiomatic features like
plain macros, variadic macros, and includes. For this purpose, they are not
needed either. Handling codegen with a trivial stack machine and these absurd
"push pop push pop" chains is anything but efficient. Still, to me, all the
interesting bits and pieces are here.

In retrospect, I think the nastiest parts are some of the ad hoc simplifications
I did without much thought when implementing the parser. This uneasiness applies
especially to parsing of definitions and declarations. As a result, we have some
accidental complexity in the AST. I also attempted to simplify codegen by doing
the tree rewrites, but I think it just complicated things. I do not like the
result. It is what it is.
