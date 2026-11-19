# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The probe of PushUnshiftErrors.qtest: the errors of push and unshift on a value that is not a list, through a local,
# a member and an element, have the messages of the AST interpreter in every mode

%modern
sub show(string label, code c) {
    try {
        auto v = c();
        printf("%s: ok %y\n", label, v);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}
sub f() {
    auto s = "str";
    hash h = {"s": "str", "i": 1};
    list l = ("str", 2);
    show("unshift local", auto sub () { return unshift s, 1; });
    show("unshift member", auto sub () { return unshift h.s, 1; });
    show("unshift element", auto sub () { return unshift l[0], 1; });
    show("push local", auto sub () { return push s, 1; });
    show("push member", auto sub () { return push h.s, 1; });
    show("push element", auto sub () { return push l[0], 1; });
    show("push int member", auto sub () { return push h.i, 1; });
    printf("after: s=%y h=%y l=%y\n", s, h, l);
}
f();
