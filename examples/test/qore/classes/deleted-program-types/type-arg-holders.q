%modern
# the worker of deleted-program-type-arg-holders.qtest: a call reference with explicit type arguments and a closure
# that captures the type arguments of the generic code that created it keep the declarations of those types for as
# long as they live, also when the Program that declared them is deleted (issue #6489)

# Copyright (C) 2026 Qore Technologies, s.r.o.

const Opts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION;

#! uses its type argument in a type check, without making a type derived from it
string sub n<T>() {
    auto v = 1;
    return (v instanceof T) ? "yes" : "no";
}

#! returns a call reference with an explicit type argument of the caller
code sub getRef<U>() {
    return \n<U>();
}

#! returns a closure that captures the type argument of the caller
code sub getClosure<U>() {
    return string sub () { return n<U>(); };
}

#! returns the number of declarations of Programs that are not freed, less the retired ones, or -1 without a debug
#! build, which has no debug functions
/** A declaration that a derived type names is retired instead of freed when nothing references it, and kept until the
    library cleanup (issue #6419)
*/
int sub live() {
    if (!Option::HAVE_LIBRARY_DEBUGGING) {
        return -1;
    }
    return call_function("dbg_live_type_declarations") - call_function("dbg_retired_type_declarations");
}

sub check(string label, string func) {
    int before = live();
    code c;
    {
        Program p(Opts);
        p.importFunction(func);
        p.parse(sprintf("class Foo {} code sub run() { return %s<Foo>(); }", func), label);
        c = p.callFunction("run");
        # calls made while the Program exists, so that the call sites cache their state
        for (int i = 0; i < 5; ++i) {
            c();
        }
        delete p;
    }
    # declarations kept by the holder
    int held = live();
    # declarations of other Programs created and deleted meanwhile
    for (int i = 0; i < 10; ++i) {
        Program q(Opts);
        q.parse(sprintf("class Bar%d { public { int x; } }", i), "q");
        delete q;
    }
    string result = c();
    delete c;
    # released by the holder: freed or retired
    int after = live();
    if (before < 0) {
        printf("%s: result: %s, declarations kept while held: n/a, released: n/a\n", label, result);
    } else {
        printf("%s: result: %s, declarations kept while held: %d, released: %s\n", label, result, held - before,
            after == before ? "yes" : sprintf("no (%d left)", after - before));
    }
}

check("call reference", "getRef");
check("closure", "getClosure");
