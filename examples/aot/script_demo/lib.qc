# AOT Phase 4 slice 10 script-mode demo — supporting file.
#
# Compiled per-file via `qcc -c`; cross-file references from
# main.q resolve when that file is compiled with `-L <this dir>`
# because this file's declarations get preloaded into the compile
# program via QoreAOTBinaryMultiDeserializer.
#
# At runtime the host links lib.qo + main.qo + -lqore, calls each
# file's `qore_<app>_<file>_script_register(pgm)` entry, then
# dispatches into compute() via the slice 8 public C API.

%modern

class Helper {
    int multiply(int a, int b) {
        return a * b;
    }
}

# Cross-.qo runtime-evaluated class constant — exercises the
# QORE_AOT_FEAT_CONST_PENDING deserializer path.  The init expression
# is non-literal so the parser cannot fold it at compile time; the
# serialized shell carries a pending-init flag, and the deserializer
# installs a RuntimeConstantRefNode so main.q's reference defers
# evaluation until the __const_init::Local::Magic init-func runs at
# register time.
class Local {
    public {
        const Magic = sprintf("%d", 42).toInt();
    }
}

# Cross-.qo runtime-evaluated namespace constant — same story for
# top-level constants.  The sprintf+toInt idiom forces non-literal
# init so the parser can't fold it.
namespace LibConsts {
    const Answer = sprintf("%d", 42).toInt();
}
