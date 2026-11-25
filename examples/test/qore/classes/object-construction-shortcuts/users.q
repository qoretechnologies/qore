%modern

const MainPgmId = get_thread_call_stack()[0].programid;

string sub frames(list<auto> cs) {
    list<string> l = ();
    foreach hash<auto> f in (cs) {
        l += sprintf("%s:%s:%s:%d:%s", f.function, f.type, basename(f.file ?? ""), f.line,
            f.programid == MainPgmId ? "main" : "other");
    }
    return l.join(" < ");
}

string sub ex(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s @ %s:%d %s", ex.err, ex.desc, basename(ex.file ?? ""), ex.line, frames(ex.callstack));
}

class A {
    public {
        int x;
        string s;
    }

    constructor(int x, string s) {
        self.x = x;
        self.s = s;
        printf("A::constructor(%y, %y) governed: %y frames: %s\n", x, s, SandboxManager::isGoverned(),
            frames(get_thread_call_stack()));
    }
}

# a parameter whose argument is converted
class B {
    constructor(softint i, *string s) {
        printf("B::constructor(%y, %y)\n", i, s);
    }
}

# a default argument
class C {
    constructor(int i = 5) {
        printf("C::constructor(%y)\n", i);
    }
}

# arguments of the base class constructor
class D inherits A {
    constructor(int y) : A(y + 1, "d") {
        printf("D::constructor(%y) x: %y\n", y, x);
    }
}

# an implicit base class constructor without arguments
class E0 {
    constructor() {
        printf("E0::constructor() frames: %s\n", frames(get_thread_call_stack()));
    }
}

class E1 inherits E0 {
}

class E2 inherits E1 {
    constructor() {
        printf("E2::constructor()\n");
    }
}

# a constructor that throws
class F {
    constructor(int i) {
        if (i) {
            throw "F-ERROR", sprintf("i: %d", i);
        }
    }

    destructor() {
        printf("F::destructor()\n");
    }
}

# a destructor that throws
class G {
    constructor(string label) {
    }

    destructor() {
        throw "G-ERROR", "destructor";
    }
}

# abstract methods implemented by a subclass
class H0 {
    abstract string m();

    constructor() {
        printf("H0::constructor() %s\n", m());
    }
}

class H1 inherits H0 {
    constructor() {
        printf("H1::constructor()\n");
    }

    string m() {
        return "H1::m()";
    }
}

# a copy method
class K {
    public {
        int v;
    }

    constructor(int v) {
        self.v = v;
    }

    copy(K old) {
        printf("K::copy() old: %y\n", old.v);
    }
}

# an object argument of a subclass, a hash argument and NOTHING for an or-nothing type
class L {
    constructor(A a, *hash<auto> h, *int i) {
        printf("L::constructor() %s %y %y\n", a.className(), h, i);
    }
}

# a class that inherits A privately: its objects are not A objects outside its class
class PA inherits private A {
    constructor() : A(9, "private") {
    }

    static L makeL(PA pa) {
        # in the class's context, the object is an A object
        return new L(pa, NOTHING, NOTHING);
    }
}

# a recursive constructor whose innermost call throws
class Q {
    constructor(int i) {
        if (i) {
            Q q(i - 1);
        } else {
            throw "Q-ERROR", "innermost";
        }
    }
}

# extra arguments
class R {
    constructor(int i) {
        printf("R::constructor(%y)\n", i);
    }
}


sub run(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> e) {
        printf("%s: %s\n", label, ex(e));
    }
}

for (int i = 0; i < 3; ++i) {
    run("A", auto sub () { A a(i, "a"); return a.x; });
    run("B", auto sub () { B b("2", "s"); return 1; });
    run("B nothing", auto sub () { B b(3); return 1; });
    run("C", auto sub () { C c(); return 1; });
    run("C arg", auto sub () { C c(7); return 1; });
    run("D", auto sub () { D d(i); return 1; });
    run("E1", auto sub () { E1 e(); return 1; });
    run("E2", auto sub () { E2 e(); return 1; });
    run("F", auto sub () { F f(1); return 1; });
    run("F ok", auto sub () { F f(0); return 1; });
    run("G scope", auto sub () { G g("scope"); return 1; });
    run("G delete", auto sub () { G g("delete"); delete g; return 1; });
    run("H1", auto sub () { H1 h(); return 1; });
    run("K", auto sub () { K k(i); K k2 = k.copy(); return k2.v; });
    run("L", auto sub () { D d(1); L l(d, NOTHING, NOTHING); return 1; });
    run("L hash", auto sub () { A a(1, "x"); L l(a, {"a": 1}, 2); return 1; });
    run("Q", auto sub () { Q q(2); return 1; });
    run("L wrong object", auto sub () { auto o = new C(1); L l(o, NOTHING, NOTHING); return 1; });
    run("L private object", auto sub () { auto o = new PA(); L l(o, NOTHING, NOTHING); return 1; });
    run("L private object in class", auto sub () { PA pa(); PA::makeL(pa); return 1; });
    run("R extra", auto sub () { object r = create_object("R", 1, 2); return 1; });
    run("R auto", auto sub () { auto v = i; R r(v); return 1; });
    run("A wrong type", auto sub () { object a = create_object("A", "x", "y"); return 1; });
    run("A auto wrong type", auto sub () { auto v = "x"; A a(v, "y"); return 1; });
}

# a class of this Program constructed by the code of a child Program that imports it, and of a sandboxed one
foreach bool sandboxed in ((False, True)) {
    Program p(PO_NEW_STYLE);
    if (sandboxed) {
        SandboxManager m();
        m.setUncontrolledApiPolicy(True);
        p.setSandboxManager(m);
    }
    p.importClass("A");
    p.importClass("E1");
    p.parse("A sub make(int i) { return new A(i, \"child\"); }
E1 sub make_e() { return new E1(); }", "child");
    for (int i = 0; i < 2; ++i) {
        A a = p.callFunction("make", i);
        printf("child sandboxed: %y: %y %y\n", sandboxed, a.x, a.s);
        p.callFunction("make_e");
    }
}
