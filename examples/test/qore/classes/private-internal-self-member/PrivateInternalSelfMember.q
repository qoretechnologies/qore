# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# self.<member> for a member of a class that a base class inherits with private:internal is refused when parsing, as
# the name alone is (#6669); see private-internal-self-member.qtest

%modern
%no-child-restrictions

# parses the code in a Program of its own and prints the result: the first exception, or the value of get()
sub try_parse(string label, string code,
        int po = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION) {
    Program p(po);
    try {
        p.parse(code, label);
        printf("%s: parsed: %y\n", label, p.callFunction("get"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

const Hierarchy = "class G { public { int v = 1; } }\n"
    "class P inherits private:internal G { int pv() { return v; } }\n";

# the member read through self in a subclass of the inheriting class
try_parse("read", Hierarchy + "class C inherits P { auto get() { return self.v; } }\n"
    "auto sub get() { C c(); return c.get(); }\n");
# the member written through self in a subclass of the inheriting class
try_parse("write", Hierarchy + "class C inherits P { auto get() { self.v = 2; return 0; } }\n"
    "auto sub get() { C c(); return c.get(); }\n");
# the inheriting class itself can access the member
try_parse("inheriting class", Hierarchy + "auto sub get() { P p(); return p.pv(); }\n");
# the member read as $.<member> (without bare references) in a subclass of the inheriting class
try_parse("read with $.", "class G { public { int $.v = 1; } }\n"
    "class P inherits private:internal G {}\n"
    "class C inherits P { auto get() { return $.v; } }\n"
    "auto sub get() { C $c(); return $c.get(); }\n", PO_REQUIRE_TYPES | PO_STRICT_ARGS);
