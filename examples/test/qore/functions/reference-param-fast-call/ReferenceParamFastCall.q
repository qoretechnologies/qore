%modern
%allow-weak-references
# calls of functions and methods with reference parameters (qore#7271): the output is the AST interpreter's in every
# execution mode, on the IR tier and in programs compiled with qcc
our int g_counter = 0;
our hash<auto> g_hash = {"a": 1};

int sub set_ref(reference<int> r, int v) { r = v; return v + 1; }
int sub set_untyped(reference r, auto v) { r = v; return 1; }
auto sub get_ref(reference r) { return r; }
int sub add_ref(reference<int> r, int v) { r += v; return r; }
int sub pass1(reference<int> r, int v) { return set_ref(\r, v) + 1; }
int sub pass2(reference<int> r, int v) { return pass1(\r, v) + 1; }
int sub pass_untyped(reference r, int v) { return set_untyped(\r, v) + pass1(\r, v); }
string sub nothing_ref(*reference<int> r) { return exists r ? sprintf("set %y", r) : "nothing"; }
string sub nothing_untyped(reference r) { return sprintf("%y", r); }
int sub set_then_throw(reference<int> r, int v) { r = v; throw "ERR", sprintf("%d", r); }
int sub set_closure(reference<int> r, code c) { r = 5; int a = c(); r = 6; return a; }
int sub two_refs(reference<int> a, reference<int> b) { a = 3; int x = b; b = 4; return x * 10 + a; }
string sub hash_ref(reference<hash<auto>> h) { h.x = 1; return h.keys().join(","); }
string sub list_ref(reference<list<auto>> l) { l += 9; return sprintf("%y", l); }
string sub hash_cow(reference<hash<auto>> h, hash<auto> copy) { h.y = 2; return sprintf("%y %y", h.y, copy.y); }
string sub str_ref(reference<string> s) { s += "x"; return s; }
string sub softint_ref(reference<softint> r) { return sprintf("%y", r); }
int sub loop_ref(reference<int> r, int n) { for (int i = 0; i < n; ++i) { r += i; } return r; }
code sub capture_ref(reference<int> r) { return int sub () { r += 1; return r; }; }
int sub rethrow_level(reference<int> r, int v) { try { return set_then_throw(\r, v); } catch (hash<ExceptionInfo> ex) { r += 1; rethrow; } }
string sub weak_ref(reference r) { return r.getName(); }
string sub weak_ref_set(reference r, object o) { r := o; return r.getName(); }

class Obj {
    public { int m = 1; hash<auto> h = {"k": 1}; list<auto> l = (1, 2); }
    string getName() { return "Obj"; }
    int mref(reference<int> r, int v) { r = v; return v * 2; }
    int mself(int v) { return set_ref(\m, v); }
    static int sref(reference<int> r, int v) { r = v; return v * 3; }
    static int stmp(reference<int> r, Tmp t, string s) { r = s.size(); printf("in stmp\n"); return r; }
    int callPriv(reference<int> r, int v) { return privRef(\r, v); }
    private int privRef(reference<int> r, int v) { r = v + 1; return r; }
}

class Tmp {
    private { string name; }
    constructor(string name) { self.name = name; }
    destructor() { printf("released %s\n", name); }
}

int sub ftmp(reference<int> r, Tmp t, string s) { r = s.size(); printf("in ftmp\n"); return r; }

class Sub inherits Obj {
    int mref(reference<int> r, int v) { r = v + 100; return v; }
}

sub out(string label, auto v) {
    printf("%s: %y\n", label, v);
}

sub run(int i) {
    int x = 1;
    out("local typed", set_ref(\x, 10 + i));
    out("local after", x);

    auto u = "a";
    out("untyped", set_untyped(\u, "b" + i));
    out("untyped after", u);
    out("untyped read", get_ref(\u));

    int y = 2;
    out("add", add_ref(\y, 3));
    out("add after", y);

    int z = 0;
    out("pass2", pass2(\z, 7 + i));
    out("pass2 after", z);
    auto pu = 0;
    out("pass untyped", pass_untyped(\pu, 4));
    out("pass untyped after", pu);

    *int n;
    out("nothing typed", nothing_ref(\n));
    out("nothing arg", nothing_ref());
    out("nothing untyped", nothing_untyped(\n));

    int e = 0;
    try {
        set_then_throw(\e, 42 + i);
    } catch (hash<ExceptionInfo> ex) {
        out("exception", ex.err + " " + ex.desc);
    }
    out("exception after", e);

    int c = 0;
    out("closure", set_closure(\c, int sub () { return c; }));
    out("closure after", c);

    int a = 1;
    out("two refs same", two_refs(\a, \a));
    out("two refs same after", a);
    int b1 = 1;
    int b2 = 2;
    out("two refs", two_refs(\b1, \b2));
    out("two refs after", (b1, b2));

    hash<auto> h = {"a": 1};
    hash<auto> hcopy = h;
    out("hash", hash_ref(\h));
    out("hash after", (h, hcopy));
    hash<auto> h2 = {"a": 1};
    out("hash cow", hash_cow(\h2, h2));
    out("hash cow after", h2);

    list<auto> l = (1,);
    list<auto> lcopy = l;
    out("list", list_ref(\l));
    out("list after", (l, lcopy));

    string s = "s";
    out("string", str_ref(\s));
    out("string after", s);

    auto si = "12";
    out("softint", softint_ref(\si));
    out("softint after", si);

    int lr = 0;
    out("loop", loop_ref(\lr, 4));
    out("loop after", lr);

    # hash keys, list elements, members, globals
    hash<auto> hk = {"k": 1};
    out("hash key", set_ref(\hk.k, 5 + i));
    out("hash key after", hk);
    hash<auto> hk2 = {"k": {"n": 1}};
    out("nested key", set_ref(\hk2.k.n, 6));
    out("nested key after", hk2);
    list<auto> le = (1, 2, 3);
    out("list elem", set_ref(\le[1], 7));
    out("list elem after", le);
    Obj o();
    out("member", set_ref(\o.m, 8 + i));
    out("member after", o.m);
    out("member hash", hash_ref(\o.h));
    out("member hash after", o.h);
    out("self member", o.mself(11));
    out("self member after", o.m);
    out("global", set_ref(\g_counter, 9 + i));
    out("global after", g_counter);
    out("global hash", hash_ref(\g_hash));
    out("global hash after", g_hash);

    # methods
    int mr = 0;
    out("method", o.mref(\mr, 3));
    out("method after", mr);
    Obj so = new Sub();
    out("virtual method", so.mref(\mr, 4));
    out("virtual method after", mr);
    out("static method", Obj::sref(\mr, 5));
    out("static method after", mr);
    out("private via public", o.callPriv(\mr, 6));
    out("private via public after", mr);

    # temporary arguments given to the call with a reference
    int tr = 0;
    out("function temp", ftmp(\tr, new Tmp("function " + i), "abc" + i));
    out("static temp", Obj::stmp(\tr, new Tmp("static " + i), "abcd" + i));
    out("temp after", tr);

    # weak reference
    object wo = new Obj();
    object w := wo;
    out("weak", weak_ref(\w));
    object w2;
    out("weak set", weak_ref_set(\w2, wo));
    delete wo;
    out("weak after delete", exists w2);

    # a value of another type in an untyped lvalue
    auto av = "str";
    try {
        out("type error", set_ref(\av, 1));
    } catch (hash<ExceptionInfo> ex) {
        out("type error ex", ex.err + ": " + ex.desc);
    }
    out("type error after", av);

    # a closure that captures a reference parameter and is called after the call
    int cap = 1;
    code capc = capture_ref(\cap);
    out("capture", (capc(), capc()));
    out("capture after", cap);

    # an exception through two levels
    int rt = 0;
    try {
        rethrow_level(\rt, 60 + i);
    } catch (hash<ExceptionInfo> ex) {
        out("rethrow", ex.desc);
    }
    out("rethrow after", rt);

    # call references and named arguments
    code cr = \set_ref();
    int cx = 0;
    out("callref", cr(\cx, 12));
    out("callref after", cx);
    int nx = 0;
    out("named", set_ref(v: 13, r: \nx));
    out("named after", nx);
    int rnx = 0;
    out("runtime named", cr(v: 14, r: \rnx));
    out("runtime named after", rnx);
    code mcr = \o.mref();
    int mcx = 0;
    out("method callref", mcr(\mcx, 15));
    out("method callref after", mcx);
}

for (int i = 0; i < 3; ++i) {
    printf("-- iteration %d\n", i);
    run(i);
}

# Program boundary: a function of a child Program called with a reference to the parent's local
Program p(PO_NEW_STYLE | PO_NO_TOP_LEVEL_STATEMENTS);
p.parse("int sub child_set(reference<int> r, int v) { r = v; return v + 1; } int sub child_call(int v) { int x = 0; int rv = child_set(\\x, v); return rv * 100 + x; }", "child");
for (int i = 0; i < 3; ++i) {
    int px = 0;
    out("program callFunction", p.callFunction("child_call", 20 + i));
    code pc = p.getCallReference("child_set");
    out("program callref", pc(\px, 30 + i));
    out("program callref after", px);
}

# sandboxed Program: a call into it with a reference
Program sp(PO_NEW_STYLE | PO_NO_TOP_LEVEL_STATEMENTS | PO_NO_FILESYSTEM | PO_NO_PROCESS_CONTROL);
sp.parse("int sub sb_set(reference<int> r, int v) { r = v; return v + 2; } int sub sb_call(int v) { int x = 1; int rv = sb_set(\\x, v); return rv * 100 + x; }", "sandbox");
for (int i = 0; i < 3; ++i) {
    int sx = 0;
    out("sandbox callFunction", sp.callFunction("sb_call", 40 + i));
    code sc = sp.getCallReference("sb_set");
    out("sandbox callref", sc(\sx, 50 + i));
    out("sandbox callref after", sx);
}
