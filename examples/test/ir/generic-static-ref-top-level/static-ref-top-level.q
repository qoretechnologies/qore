#!/usr/bin/env qore
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# A reference made in top-level code to a static method of a generic class through a generic class over a class that
# adds variants to the method is resolved without a class context (it crashed at parse time); see
# generic-static-ref-top-level.qtest.

%modern

class G<T> {
    static T id(T v) { return v; }
}
class HX inherits G<float> {
    static string id(string s) { return "own " + s; }
}
class W3<V> inherits HX {}

sub show(string label, auto v) {
    printf("%s: %y %s\n", label, v, v.fullType());
}

# references made in top-level code, where no class is the context of the code
code w3_ref = \W3::id();
code hx_ref = \HX::id();

# several calls, so that the code also runs once it has IR or is compiled
for (int n = 0; n < 3; ++n) {
    show("HX::id(1)", HX::id(1));
    show("reference W3::id(1)", w3_ref(1));
    show("reference W3::id(string)", w3_ref("x"));
    show("reference HX::id(1)", hx_ref(1));
}
