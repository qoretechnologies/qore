# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The types of compiled code that the runtime resolves from their paths on each call (return, parameter and local
# variable types) are the same whether or not the path has a type parameter of the signature of the code: the same
# call site alternates the instantiations of generic code, and plain object types are checked as before (qore#7013);
# see type-path-signature-params.qtest

%modern
%enable-all-warnings

class C {
    string n() { return "C"; }
}

class D {
    string n() { return "D"; }
}

class Box<T> {
    public {
        T v;
    }

    constructor(T v) {
        self.v = v;
    }

    list<T> wrap() {
        list<T> rv = (v,);
        return rv;
    }
}

# a plain object type in the return type
*C sub get_c(auto v) {
    return v;
}

# a type parameter of the signature in the return type and in a local variable type
list<T> sub wrap<T>(T v) {
    list<T> rv = (v,);
    return rv;
}

T sub first<T>(list<T> l) {
    return l[0];
}

*list<T> sub maybe<T>(T v, bool b) {
    return b ? (v,) : NOTHING;
}

# the result of a call, or its exception
string sub try_call(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

list<string> results = ();
int sum = 0;
string cat = "";
for (int i = 0; i < 64; ++i) {
    # the same call sites alternate the instantiations of the generic code
    list<int> li = wrap<int>(i);
    list<string> ls = wrap<string>("s" + i);
    sum += first<int>(li);
    cat = first<string>(ls);
    *list<C> lc = maybe<C>(new C(), (i % 2) == 0);
    if (lc) {
        cat += lc[0].n();
    }
    list<int> bi = new Box<int>(i).wrap();
    list<string> bs = new Box<string>("b").wrap();
    sum += bi[0];
    cat += bs[0];
    *C c = get_c(new C());
    cat += c.n();
}
results += sprintf("sum: %d", sum);
results += sprintf("last: %s", cat);
results += sprintf("element types: %s %s", wrap<int>(1).fullType(), wrap<string>("x").fullType());
results += sprintf("box element types: %s %s", new Box<int>(1).wrap().fullType(),
    new Box<string>("x").wrap().fullType());
# the checks still raise their errors
results += sprintf("a D for *C: %s", try_call(auto sub () { return get_c(new D()); }));
results += sprintf("a string for an int instantiation: %s", try_call(auto sub () {
    auto l = ("x",);
    return first<int>(l);
}));
results += sprintf("nothing for *C: %s", try_call(auto sub () { return get_c(NOTHING); }));
print(join("\n", results) + "\n");
