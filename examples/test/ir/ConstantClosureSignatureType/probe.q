#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of ConstantClosureSignatureType.qtest: a closure of a constant expression has the code type of its
# signature, also behind an operator and as the value of a constant (qore#6046)

%modern

const C1 = {"l": (sub () {},)};
const C2 = {"h": {"l": (sub () {},)}};
const C3 = {"h": True ? {"l": (sub () {},)} : {"l": (sub () {},)}};
const C4 = {"h": ({"l": (sub () {},)},)[0]};
const C5 = {"h": ({"l": (sub () {},)}{"l"},)};
const C6 = {"l": (int sub (SigTypeClass c, hash<SigTypeHash> h) { return c.n + h.n; },)};
const CA = sub () {};
# the default value of a closure of a constant expression refers to a constant that follows (#4903)
const CD = int sub (int x = CE) { return x; };
const CE = 2;
# types declared later: a typedef, an enum and a class in a namespace
const CF = {"l": (sub (SigAlias a, SigEnum e, SigNs::SigNsClass c) {},)};

# a generic function whose body initializes a constant declared after it: the constant's closure does not name the
# function's type parameter, which has the name of a class (qore#5733)
auto sub gen_use<GenParamX>(GenParamX v) {
    return {"l": (CG,)};
}
const CG = auto sub (GenParamX x) { return x.n; };

class GenParamX {
    public {
        int n = 7;
    }
}

typedef SigAlias = int;

enum SigEnum {
    One = 1,
}

namespace SigNs {
    class SigNsClass {
    }
}

# a closure of a class constant
class SigConstHolder {
    const C = {"l": (sub (SigTypeClass c) {},)};
}

class SigTypeClass {
    public {
        int n = 1;
    }
}

hashdecl SigTypeHash {
    int n = 2;
}

sub show(string label, auto v) {
    printf("%s: %s\n", label, v.fullType());
}

sub f() {
    show("f hash of list", {"l": (sub () {},)});
    show("f hash of hash of list", {"h": {"l": (sub () {},)}});
    show("f conditional", {"h": True ? {"l": (sub () {},)} : {"l": (sub () {},)}});
    show("f index", {"h": ({"l": (sub () {},)},)[0]});
    show("f slice", {"h": ({"l": (sub () {},)}{"l"},)});
    show("f class and hashdecl parameters",
        {"l": (int sub (SigTypeClass c, hash<SigTypeHash> h) { return c.n + h.n; },)});
    show("f closure constant in a hash", {"l": (CA,)});
    show("f later types", {"l": (sub (SigAlias a, SigEnum e, SigNs::SigNsClass c) {},)});
    show("f class constant", {"l": (sub (SigTypeClass c) {},)});
}

show("const hash of list", C1);
show("const hash of hash of list", C2);
show("const conditional", C3);
show("const index", C4);
show("const slice", C5);
show("const class and hashdecl parameters", C6);
show("const closure constant in a hash", {"l": (CA,)});
show("const later types", CF);
show("const class constant", SigConstHolder::C);
f();
printf("calls: %y %y %y\n", C6.l[0](new SigTypeClass(), <SigTypeHash>{}), CD(), CD(5));
printf("generic: %y %y\n", CG(new GenParamX()), gen_use<int>(1).l[0] === CG);
