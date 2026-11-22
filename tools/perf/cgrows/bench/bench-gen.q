%modern
# generic runtime creation loops (#6126): L=gr (generic *T local, reference returned), gc (generic closure created and
# called), cr/cc (the concrete *int controls), gn (generic call without storage), gl (generic list<T> local), gh
# (generic hash<string, T> local), gk (generic class instantiation), gm (generic class method returning softlist<T>),
# gx (cast<T> in a generic function), gv (generic hashdecl-free typed local *T assigned and returned); N iterations
int sub f(int a) { return a + 1; }
auto sub makeReference<T>() { *T value; return \value; }
code sub makeClosure<T>() { *T value; return auto sub () { return value; }; }
auto sub makeIntReference() { *int value; return \value; }
code sub makeIntClosure() { *int value; return auto sub () { return value; }; }
T sub g_ident<T>(T v) { return v; }
list<T> sub g_list<T>(T v) { list<T> l = (v,); return l; }
hash<string, T> sub g_hash<T>(T v) { hash<string, T> h = {"a": v}; return h; }
list<T> sub g_cast<T>(auto v) { return cast<list<T>>(v); }
*T sub g_local<T>(T v) { *T x = v; return x; }
class GenericBox<T> {
    public {
        *T optional;
    }
    constructor(*T v) { optional = v; }
    softlist<T> wrap(T v) { return v; }
}
int sub run_gr(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        reference r = makeReference<int>();
        if (r !== NOTHING) { throw "BAD-RESULT"; }
        ++t;
    }
    return t;
}
int sub run_gc(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        code c = makeClosure<int>();
        if (c() !== NOTHING) { throw "BAD-RESULT"; }
        ++t;
    }
    return t;
}
int sub run_cr(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        reference r = makeIntReference();
        if (r !== NOTHING) { throw "BAD-RESULT"; }
        ++t;
    }
    return t;
}
int sub run_cc(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        code c = makeIntClosure();
        if (c() !== NOTHING) { throw "BAD-RESULT"; }
        ++t;
    }
    return t;
}
int sub run_gn(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += g_ident<int>(i);
    }
    return t;
}
int sub run_gl(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += g_list<int>(i)[0];
    }
    return t;
}
int sub run_gh(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += g_hash<int>(i).a;
    }
    return t;
}
int sub run_gk(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        GenericBox<int> b(i);
        t += b.optional;
    }
    return t;
}
int sub run_gm(int n) {
    int t = 0;
    GenericBox<int> b(1);
    for (int i = 0; i < n; ++i) {
        t += b.wrap(i)[0];
    }
    return t;
}
int sub run_gx(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        auto l = (i,);
        t += g_cast<int>(l)[0];
    }
    return t;
}
int sub run_gv(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += g_local<int>(i);
    }
    return t;
}
int n = ENV.N.toInt();
int rv;
switch (ENV.L) {
    case "gr": rv = run_gr(n); break;
    case "gc": rv = run_gc(n); break;
    case "cr": rv = run_cr(n); break;
    case "cc": rv = run_cc(n); break;
    case "gn": rv = run_gn(n); break;
    case "gl": rv = run_gl(n); break;
    case "gh": rv = run_gh(n); break;
    case "gk": rv = run_gk(n); break;
    case "gm": rv = run_gm(n); break;
    case "gx": rv = run_gx(n); break;
    case "gv": rv = run_gv(n); break;
    default: throw "BAD-LOOP", ENV.L;
}
if (ENV.P) { printf("%s %d\n", ENV.L, rv); }
