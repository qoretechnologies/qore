# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# assigns the unary plus of a call result that is an integer outside the inline (48-bit) range to a hash member, a list
# element, an object member and a local, and prints the values, which unary-plus-heap-element.qtest checks; the AST
# interpreter stored a freed value in the hash member (issue #6297)
%modern

class C {
    public {
        auto x;
    }
}

auto sub bigi(int i) { return (1 << 62) + i; }

list<auto> sub run() {
    hash<auto> h;
    h.x = +bigi(1);
    list<auto> l = (0,);
    l[0] = +bigi(2);
    C c();
    c.x = +bigi(3);
    auto v = +bigi(4);
    # pushed and in a literal
    list<auto> p = ();
    push p, +bigi(5);
    hash<auto> lit = {"y": +bigi(6)};
    return (h.x, l[0], c.x, v, p[0], lit.y);
}

list<auto> result;
for (int i = 0; i < 3; ++i) {
    result = run();
}
printf("%y\n", result);
