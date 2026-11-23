#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the calls bound once the abstract methods are resolved in a parse that fails (the binds are released with the
# rollback), in parsePending() parses committed once, and in a Program parsed at once (qore#7411); see
# ../abstract-typed-call-created-method.qtest

%modern
string classes = "class C5 { string m(int i) { return \"C5::m(int)\"; } }
class G0 { abstract string m(bool b); abstract string m(int i); }
class G3 inherits G0 { string m(bool b) { return \"G3::m(bool)\"; } }
class G6 inherits C5, G3 { string own(bool b) { return m(b); } }
";
# a parse that fails after a call was deferred: the deferred bind is released with the rollback
{
    Program p(PO_NEW_STYLE);
    try {
        p.parse(classes + "string sub t() { G6 g(); return g.m(True); }\nint sub bad() { return \"x\"; }\n", "bad");
        printf("rollback: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("rollback: %s\n", ex.err);
    }
}
# the type of a call bound once the abstract methods are resolved is known before: an assignment that the variants of
# the call do not return a value for is refused when parsing
{
    Program p(PO_NEW_STYLE);
    try {
        p.parse(classes + "int sub t() { G6 g(); int i = g.m(True); return i; }", "type");
        printf("type: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("type: %s\n", ex.err);
    }
}
# a call of a class whose abstract methods give it no method (an abstract class between) is not deferred: its type is
# checked as before
{
    Program p(PO_NEW_STYLE);
    try {
        p.parse("class A { abstract int get(); } class B inherits A {} string sub t(B b) { string s = b.get(); return s; }",
            "abstract between");
        printf("abstract between: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("abstract between: %s\n", ex.err);
    }
}
# the warnings in effect at the call: G3::m(bool) is deprecated
{
    # (G6 without the call of its own code, which is outside the region of the directives)
    string dclasses = replace(replace(classes, "class G3 inherits G0 { string m(bool b)",
        "class G3 inherits G0 { deprecated string m(bool b)"),
        "class G6 inherits C5, G3 { string own(bool b) { return m(b); } }", "class G6 inherits C5, G3 {}");
    Program p(PO_NEW_STYLE);
    *hash<ExceptionInfo> w = p.parse(dclasses + "%disable-warning deprecated\nstring sub t() { G6 g(); "
        "return g.m(True); }\n%enable-warning deprecated\n", "warnings off", WARN_ALL);
    printf("warning disabled at the call: %y\n", w.err);
    Program p2(PO_NEW_STYLE);
    w = p2.parse(dclasses + "string sub t() { G6 g(); return g.m(True); }\n%disable-warning deprecated\n",
        "warnings on", WARN_ALL);
    printf("warning enabled at the call: %y\n", w.err);
}
# a call that no variant of the method matches is refused once it is bound
{
    Program p(PO_NEW_STYLE);
    try {
        p.parse(classes + "string sub t() { G6 g(); return g.m(\"s\"); }", "no variant");
        printf("no variant: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("no variant: %s\n", ex.err);
    }
}
# parsePending() of the classes and of the calls, then one commit
{
    Program p(PO_NEW_STYLE);
    p.parsePending(classes, "classes");
    p.parsePending("string sub t() { G6 g(); return g.m(True) + \" \" + (new G6()).own(True); }", "calls");
    p.parseCommit();
    printf("parsePending: %s\n", p.callFunction("t"));
}
# the classes and the call in one parse
{
    Program p1(PO_NEW_STYLE);
    p1.parse(classes + "string sub t() { G6 g(); return g.m(True); }", "p1");
    printf("committed: %s\n", p1.callFunction("t"));
}
