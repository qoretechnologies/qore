# probes of generic-or-nothing-containers.qtest (qore#5849): an or-nothing container of a type parameter
# (*list<T>, *hash<string, T>, *softlist<T>, *reference<T>) is substituted with the type arguments of the call in
# return types, type arguments are inferred from arguments of such types (functions and constructors), and variants
# for list<T> and *list<T> stay separate; probes() returns one line per pass, and each pass runs every probe, so that
# the code runs on every tier with tiered execution

%modern

*list<T> sub list_of<T>(list<T> l) { return l; }
*list<T> sub no_list<T>(T v) { return; }
*hash<string, T> sub hash_of<T>(T v) { return {"k": v}; }
*softlist<T> sub soft_of<T>(T v) { return (v,); }
*list<T> sub bad_list<T>(auto v) { return (v,); }

# type arguments inferred from or-nothing container parameters
list<T> sub from_list<T>(*list<T> l) { list<T> r = (); r += l[0]; return r; }
list<T> sub from_hash<T>(*hash<string, T> h) { list<T> r = (); r += h.k; return r; }
T sub deref<T>(*reference<T> r) { return r; }
# separate variants for a container and an or-nothing container of the type parameter
string sub which<T>(list<T> l) { return "list"; }
string sub which<T>(*list<T> l) { return "*list"; }
*list<int> sub maybe_ints() { return (7,); }

class K<T> {
    private { T v; }
    constructor(*list<T> l) { v = l[0]; }
    T get() { return v; }
}

class Box<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    *list<T> get() { return (v,); }
    *hash<string, U> pair<U>(U u) { return {"u": u}; }
}

string sub t(code c) {
    try {
        auto v = c();
        return sprintf("%y %s", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the value only: the type of a list with an unbound type parameter differs in qcc (qore#5873)
string sub tv(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s\n", i,
            t(auto sub () { return list_of(("a", "b")); }),
            t(auto sub () { return list_of<int>((1, 2)); }),
            t(auto sub () { return no_list<int>(1); }),
            t(auto sub () { return hash_of("x"); }),
            t(auto sub () { return soft_of(1); }),
            t(auto sub () { return bad_list<int>(1); }),
            t(auto sub () { return bad_list<int>("x"); }),
            t(auto sub () { return (new Box<int>(3)).get(); }),
            t(auto sub () { return (new Box<int>(3)).pair<string>("y"); }),
            t(auto sub () { *list<string> l = list_of(("c",)); return l; }),
            t(auto sub () { return from_list((1, 2)); }),
            t(auto sub () { return from_hash({"k": "v"}); }),
            t(auto sub () { int x = 4; return deref(\x); }),
            t(auto sub () { return (new K((5,))).get(); }),
            t(auto sub () { return which((1,)); }),
            t(auto sub () { return which(NOTHING); }),
            tv(auto sub () { return from_list(NOTHING); }),
            t(auto sub () { return from_list(maybe_ints()); }));
    }
    return rv;
}
