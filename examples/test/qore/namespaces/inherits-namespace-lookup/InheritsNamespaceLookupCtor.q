# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# fixture of inherits-namespace-lookup.qtest (issue #6710): the base class constructor call of a constructor names the
# same class as the parent list, as both are resolved in the namespace of the class

%modern

namespace N1 {
    class Ctor { constructor(string s) {} string m() { return "N1::Ctor"; } }
}

namespace N2 {
    class XCtor inherits Ctor { constructor() : Ctor("x") {} }
    class Ctor { constructor(string s) {} string m() { return "N2::Ctor"; } }
}

printf("base constructor: %s\n", (new N2::XCtor()).m());
