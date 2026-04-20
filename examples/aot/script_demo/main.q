# AOT Phase 4 slice 10 script-mode demo — entry file.
#
# Classes and constants are defined in lib.qc; this file references
# them by bare name (no %requires, no `%include`).  Under slice 10c,
# the `qcc -c -L <dir>` invocation on main.q preloads lib.qo's
# declarations into the compile program so the parser resolves them
# via the standard namespace-tree walk.
#
# Local::Magic and LibConsts::Answer have non-literal init expressions,
# so this file exercises QORE_AOT_FEAT_CONST_PENDING: the deserializer
# wraps each pending shell in a RuntimeConstantRefNode, and the
# __const_init::*::* init-func populates the value at register time.

%modern

int sub compute() {
    Helper h();
    # 6 * 42 = 252 — correct only if Local::Magic's init-func ran and
    # populated the saved_val before compute() executes.
    int result = h.multiply(6, Local::Magic);
    if (result != 252) {
        throw "SCRIPT-TEST-ERROR",
            sprintf("compute() got %d; expected 252 "
                "(6 * Local::Magic=%d)", result, Local::Magic);
    }
    # Namespace-constant variant: LibConsts::Answer == 42.
    if (LibConsts::Answer != 42) {
        throw "SCRIPT-TEST-ERROR",
            sprintf("LibConsts::Answer got %d; expected 42",
                LibConsts::Answer);
    }
    return result;
}
