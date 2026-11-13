%modern
# named calls through call references and closures: the code that the call's expression gives is only known when the
# call is made, so its arguments are bound by parameter name then, as for a named call of the function, method or
# static method that the reference refers to, and to the single variant of a closure (M4 phase 3); see
# compiled-overload-errors.qtest and qore_call_ref_invoke_named()

# overloaded functions whose variants name their parameters differently
string sub f(int count, string label) { return sprintf("f(int count, string label) count=%y label=%y", count, label); }
string sub f(string label, float count) { return sprintf("f(string label, float count) label=%y count=%y", label,
    count); }

# default and optional parameters
string sub dflt(int a, string b = "db", *int c) { return sprintf("dflt(int a, string b, *int c) a=%y b=%y c=%y", a, b,
    c); }

# a reference parameter
string sub rf(string b, reference<int> a) { a = a + 1; return sprintf("rf(string b, reference<int> a) b=%y", b); }

class C {
    private {
        string tag = "obj";
    }

    string m(int count, string label) { return sprintf("C::m(int count, string label) count=%y label=%y tag=%y",
        count, label, tag); }
    string m(string label, float count) { return sprintf("C::m(string label, float count) label=%y count=%y tag=%y",
        label, count, tag); }

    static string s(int count, string label) { return sprintf("C::s(int count, string label) count=%y label=%y",
        count, label); }
    static string s(string label, float count) { return sprintf("C::s(string label, float count) label=%y count=%y",
        label, count); }

    # references created in the class: a reference to a method of the object and to a static method
    code localMethodRef() { return \m(); }
    code localStaticRef() { return \s(); }

    # a closure that uses the object
    code objectClosure() {
        return string sub (int n, string prefix = "p") { return sprintf("object closure n=%y prefix=%y tag=%y", n,
            prefix, tag); };
    }
}

# a subclass: references to methods of its objects that it inherits are resolved when the call is made
class D inherits C {
    # a reference to a method of the object that is inherited by its class
    code inheritedRef() { return \m(); }
}

#! makes the call and prints its result or its exception
sub t(string label, code call) {
    try {
        printf("%s: %s\n", label, call());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

C c();
code fr = \f();
code mr = \c.m();
code sr = \C::s();
code lmr = c.localMethodRef();
code lsr = c.localStaticRef();
code oc = c.objectClosure();
int captured = 10;
code cl = string sub (int count, string label = "dl") { return sprintf("closure count=%y label=%y captured=%y",
    count, label, captured); };

# at top level (the JIT tier in jit mode)
printf("top fr int: %s\n", fr(label: "x", count: 1));
printf("top fr float: %s\n", fr(count: 1.5, label: "y"));
printf("top mr: %s\n", mr(label: "x", count: 2));
printf("top sr: %s\n", sr(count: 3.5, label: "z"));
printf("top lmr: %s\n", lmr(label: "l", count: 4));
printf("top lsr: %s\n", lsr(label: "l", count: 5));
printf("top oc: %s\n", oc(prefix: "q", n: 6));
printf("top oc default: %s\n", oc(n: 7));
printf("top cl: %s\n", cl(label: "c", count: 8));
printf("top cl default: %s\n", cl(count: 9));
printf("top mixed: %s\n", fr(1, label: "m"));
printf("top dflt: %s\n", (\dflt())(c: 3, a: 1));
{
    int v = 1;
    code r = \rf();
    printf("top ref: %s v=%y\n", r(a: \v, b: "r"), v);
}
{
    # an immediate closure
    printf("top immediate closure: %s\n", (string sub (int x, int y) { return sprintf("x=%y y=%y", x, y); })(y: 2,
        x: 1));
}

# in a function (the IR tier in jit mode)
sub calls() {
    t("fr int", string sub () { return fr(label: "x", count: 1); });
    t("fr float", string sub () { return fr(count: 1.5, label: "y"); });
    t("mr", string sub () { return mr(label: "x", count: 2); });
    t("sr", string sub () { return sr(count: 3.5, label: "z"); });
    t("lmr", string sub () { return lmr(label: "l", count: 4); });
    t("lsr", string sub () { return lsr(label: "l", count: 5); });
    t("oc", string sub () { return oc(prefix: "q", n: 6); });
    t("cl", string sub () { return cl(label: "c", count: 8); });
    t("mixed", string sub () { return fr(1, label: "m"); });
    t("dflt", string sub () { return (\dflt())(c: 3, a: 1); });

    # errors: an unknown name, a name that no variant has with a matching type, a missing argument, a value that is
    # not a call reference
    t("unknown name", string sub () { return fr(label: "x", cnt: 1); });
    t("closure unknown name", string sub () { return cl(cnt: 1); });
    t("no variant", string sub () { return fr(label: 1, count: "x"); });
    t("missing", string sub () { return cl(label: "only"); });
    auto notcallable = 1;
    t("not callable", string sub () { return notcallable(a: 1); });

    # an inherited method, through a reference to the method of an object of a subclass and through a reference
    # created in the subclass
    D d();
    code dmr = \d.m();
    t("inherited", string sub () { return dmr(label: "d", count: 12); });
    code dir = d.inheritedRef();
    t("inherited in class", string sub () { return dir(count: 13.5, label: "e"); });

    # a reference argument in a function (the IR tier in jit mode), through a closure
    {
        int z = 1;
        code cr = string sub (reference<int> r, int add) { r += add; return sprintf("r=%d", r); };
        t("closure ref", string sub () { return cr(add: 2, r: \z); });
        printf("closure ref after: z=%d\n", z);
    }

    # builtin functions: a variant that accepts named arguments, and one that does not
    code b2s = \binary_to_string();
    t("builtin named", string sub () { return b2s(encoding: "UTF-8", value: binary("abc")); });
    code spf = \sprintf();
    t("builtin positional only", string sub () { return spf(fmt: "%s", value: "x"); });

    # a method reference to an object that has been deleted: as with positional arguments
    {
        C gone();
        code gr = \gone.m();
        delete gone;
        t("deleted object positional", string sub () { return gr(1, "x"); });
        t("deleted object named", string sub () { return gr(label: "x", count: 1); });
    }
}
calls();

# a closure created in a Program that has been deleted: as with positional arguments, the call is refused before the
# closure's signature is read
{
    code orphan;
    {
        Program child(PO_NEW_STYLE);
        child.parse("code sub make() { return string sub (int count, string label) { return sprintf(\"%d %s\", "
            "count, label); }; }", "child");
        orphan = child.callFunction("make");
        delete child;
    }
    t("orphan closure positional", string sub () { return orphan(1, "x"); });
    t("orphan closure named", string sub () { return orphan(label: "x", count: 1); });
}

# a named call in a background thread
{
    Queue q();
    background q.push(fr(label: "bg", count: 11));
    printf("background: %s\n", q.get());
    # the named call itself runs in the background thread
    code pusher = sub (Queue queue, string value) { queue.push(value); };
    background pusher(value: "bg2", queue: q);
    printf("background call: %s\n", q.get());
}
