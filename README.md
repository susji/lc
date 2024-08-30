# lc

`lc` is a small bootstrapping toy compiler. This means it can compile itself.
The language closely resembles a simplified subset of ANSI C. `lc` targets only
GNU/Linux and x86. The goal was to support just enough features to make
bootstrapping a reasonable effort with close to zero emphasis on general
robustness, feature completeness, and accurate ANSI C semantics.

To see the exact subset that `lc` supports, check out [the code](lc.c).
