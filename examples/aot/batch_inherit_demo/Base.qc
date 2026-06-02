%new-style
%require-types
%strict-args
%enable-all-warnings

# Regression demo for AOT batch phase-sync — cross-session
# inheritance with:
#   (a) ctor-args delegation via explicit BCA, and
#   (b) inherited class-typed member with default (Counter c()).
#
# Pre-fix libqore (before 129a15d02 / bfe4e3e2e), when
# registered in REVERSE dep order via the script-context batch
# API, produces either:
#   - `RUNTIME-OVERLOAD-ERROR: no variant matching ...Base::constructor()`
#     (commitClasses ran on a derived session before this base's
#     methods were deserialized — base committed empty, BCA lost)
#   - `PSEUDO-METHOD-DOES-NOT-EXIST: no pseudo method <nothing>::inc()`
#     (importInheritedMembers ran on a derived session before this
#     base's members were registered — `counter` never propagated
#     to the derived class's `member_init_list`).
# Post-fix: both paths succeed regardless of session order.
public namespace BatchInherit {
    public class Base {
        public {
            # Class-typed member with default — exercises the
            # importInheritedMembers cross-session path.
            Counter counter();

            # Populated by the ctor via reference — exercises the
            # cross-session ctor-args delegation path.
            list<string> base_argv;
        }

        constructor(reference<list<string>> p_argv) {
            base_argv = p_argv;
            # If `counter` is NOTHING (member init failed), this
            # raises `<nothing>::inc()`.
            counter.inc();
        }
    }
}
