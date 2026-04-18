# AOT Phase 4 slice 10 script-mode demo — entry file.
#
# `class Helper` is defined in lib.qc; this file references it by
# bare name (no %requires, no `%include`).  Under slice 10c, the
# `qcc -c -L <dir>` invocation on main.q preloads lib.qo's
# declarations into the compile program so the parser resolves
# `Helper` via the standard namespace-tree walk.

%modern

int sub compute() {
    Helper h();
    return h.multiply(6, 7);
}
