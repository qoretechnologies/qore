%new-style
%require-types
%strict-args
%enable-all-warnings

# Factory that instantiates Top and asserts both cross-session
# invariants held: ctor-args chain propagated to Base, AND Base's
# `counter` member default initialized so counter.inc() ran.
sub batch_test() {
    list<string> av = ("alpha", "beta");
    BatchInherit::Top t(\av);

    # (a) Ctor-args delegation — if any BCA in the chain was lost
    # at commit time, base_argv would be NOTHING or not populated.
    if (t.base_argv != ("alpha", "beta")) {
        throw "BATCH-INHERIT-TEST", sprintf(
            "ctor-args delegation lost: base_argv=%y", t.base_argv);
    }

    # (b) Inherited member init — if importInheritedMembers
    # missed `counter`, `counter.inc()` in Base::constructor
    # would have raised <nothing>::inc().  Reaching here means
    # the member default ran; `getCount()` confirms inc() fired.
    int count = t.counter.getCount();
    if (count != 1) {
        throw "BATCH-INHERIT-TEST", sprintf(
            "counter not incremented: expected 1, got %d", count);
    }

    printf("batch-inherit-test: OK (argv=%y, counter=%d)\n",
        t.base_argv, count);
}
