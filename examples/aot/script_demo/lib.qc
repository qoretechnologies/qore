# AOT Phase 4 slice 10 script-mode demo — supporting file.
#
# Compiled per-file via `qcc -c`; cross-file references from
# main.q resolve when that file is compiled with `-L <this dir>`
# because this file's `Helper` class declaration gets preloaded
# into the compile program via QoreAOTBinaryMultiDeserializer.
#
# At runtime the host links lib.qo + main.qo + -lqore, calls each
# file's `qore_<app>_<file>_script_register(pgm)` entry, then
# dispatches into compute() via the slice 8 public C API.

%modern
%strict-args
%require-types

class Helper {
    int multiply(int a, int b) {
        return a * b;
    }
}
