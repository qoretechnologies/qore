# AOT Phase 4 slice 10 script-mode demo — entry file.
#
# `class Helper` is defined in lib.qc; this file references it by
# bare name (no %requires, no `%include`).  Under slice 10c, the
# `qcc -c -L <dir>` invocation on main.q preloads lib.qo's
# declarations into the compile program so the parser resolves
# `Helper` via the standard namespace-tree walk.

%modern

# Single-file runtime-evaluated class constant — exercises slice 10f's
# executeInitFunctions hookup.  The init expression (sprintf + toInt)
# is non-literal so the parser can't fold it; it gets compiled as a
# __const_init function that must run at script-register time.
class Local {
    public {
        const Magic = sprintf("%d", 42).toInt();
    }
}

int sub compute() {
    Helper h();
    int result = h.multiply(6, Local::Magic);
    # Self-check: if slice 10f's init-func path didn't run,
    # Local::Magic is 0 and compute() returns 0 instead of 252.
    if (result != 252) {
        throw "SCRIPT-TEST-ERROR",
            sprintf("compute() got %d; expected 252 "
                "(6 * Local::Magic=%d)", result, Local::Magic);
    }
    return result;
}
