# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The probe of CleanupDestructorExceptions.qtest: destructor exceptions raised when a jump leaves a block, by the
# temporaries of functional loops and by the release of a variable's old value, are caught by the enclosing try
# statement as in the AST interpreter (qore#6084, qore#6244)

%modern

our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
class TT inherits T { constructor(string n) : T(n) {} destructor() { log += "dthrow"; throw "DT"; } }
class D { private { string n; } constructor(string n) { self.n = n; } destructor() { throw "DTOR", n; } }
class E {
    private { string n; }
    constructor(string n) { self.n = n; printf("new %s\n", n); }
    destructor() { printf("~%s\n", n); throw "DTOR-" + n; }
}

string sub errs(hash<ExceptionInfo> ex) {
    list<string> l = ();
    *hash<ExceptionInfo> e = ex;
    int n = 0;
    while (e) {
        l += e.err;
        if (++n > 20) {
            return "LOOP";
        }
        e = e.next;
    }
    return l.join(",");
}

auto sub first(auto a, auto b) { return a; }

# qore#6084: a return whose block's local raises in its destructor
auto sub ret_in_try() {
    T z("z");
    try {
        TT d("d");
        return first(new T("v"), new T("t"));
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return "after";
}

# qore#6084: every way out of a block whose local raises in its destructor
sub s_end() { try { { D d("end"); } printf("after block\n"); } catch (hash<ExceptionInfo> ex) { printf("s_end caught %s\n", ex.desc); } }
sub s_break() { try { while (True) { D d("break"); break; } } catch (hash<ExceptionInfo> ex) { printf("s_break caught %s\n", ex.desc); } }
sub s_continue() { try { int i = 0; while (++i < 3) { D d("cont" + i); continue; } } catch (hash<ExceptionInfo> ex) { printf("s_continue caught %s\n", ex.desc); } }
sub s_foreach_break() { try { foreach int i in (1, 2) { D d("fb" + i); break; } } catch (hash<ExceptionInfo> ex) { printf("s_foreach_break caught %s\n", ex.desc); } }
sub s_for_break() { try { for (int i = 0; i < 3; ++i) { D d("forb" + i); break; } } catch (hash<ExceptionInfo> ex) { printf("s_for_break caught %s\n", ex.desc); } }
sub s_switch_break() { try { switch (1) { case 1: { D d("sw"); break; } } } catch (hash<ExceptionInfo> ex) { printf("s_switch_break caught %s\n", ex.desc); } }
auto sub s_return() { try { D d("ret"); return 1; } catch (hash<ExceptionInfo> ex) { printf("s_return caught %s\n", ex.desc); } return "after"; }
auto sub s_return_nested() { try { { D d("retn"); return 1; } } catch (hash<ExceptionInfo> ex) { printf("s_return_nested caught %s\n", ex.desc); } return "after"; }
# a try statement in the loop body catches the exception of the block it encloses
sub s_break_inner_try() { int i = 0; while (++i < 3) { try { D d("bi" + i); break; } catch (hash<ExceptionInfo> ex) { printf("s_break_inner_try caught %s\n", ex.desc); } } printf("s_break_inner_try i=%d\n", i); }

# the locals declared in a for statement's initializer live until the loop ends
sub for_end() { try { for (D d = new D("for-end"); False; ) { } } catch (hash<ExceptionInfo> ex) { printf("for_end caught %s\n", ex.desc); } }
sub for_break() { try { for (D d = new D("for-brk"); True; ) { break; } } catch (hash<ExceptionInfo> ex) { printf("for_break caught %s\n", ex.desc); } }
auto sub for_return() { try { while (True) { for (D d = new D("for-ret"); True; ) { return 1; } } } catch (hash<ExceptionInfo> ex) { printf("for_return caught %s\n", ex.desc); } return "after"; }
sub for_outer_break() { try { while (True) { for (D d = new D("for-ob"); True; ) { break; } break; } } catch (hash<ExceptionInfo> ex) { printf("for_outer_break caught %s\n", ex.desc); } }
# an exception raised in the body releases the initializer's locals before the catch block runs
sub for_body_throw() { try { for (T t("ft"); True; ) { throw "BODY"; } } catch (hash<ExceptionInfo> ex) { log += "catch:" + ex.err; } }

# qore#6244: the temporaries of other lowering paths
int y = 5;
auto x;
list gl;
sub t7() { gl = (1,2,3,4,5,6); try { x = (map remove gl[(new E("rm" + $1), 0)[1]..((y, 1).size())], (1, 2)); } catch (hash<ExceptionInfo> ex) { printf("t7: %s\n", errs(ex)); } }
sub t7a() { gl = (1,2,3,4,5,6); try { x = (map remove gl[(new E("ra" + $1), 0)[1]], (1, 2)); } catch (hash<ExceptionInfo> ex) { printf("t7a: %s\n", errs(ex)); } }
sub t10() { try { foreach auto v in (map (new E("fe" + $1), $1).size() + (y,1).size(), (1, 2, 3)) { } } catch (hash<ExceptionInfo> ex) { printf("t10: %s\n", errs(ex)); } }
sub t10b() { try { foreach auto v in (map (new E("fb" + $1), $1)[1], (1, 2, 3)) { } } catch (hash<ExceptionInfo> ex) { printf("t10b: %s\n", errs(ex)); } }
sub t10d() { try { foreach auto v in (map (new E("fd" + $1), $1).size() + (y, 1).size(), (select (1,2,3), $1 > 0)) { } } catch (hash<ExceptionInfo> ex) { printf("t10d: %s\n", errs(ex)); } }
sub t5b() { try { x = map {"k" + $1: (new E("h" + $1), 1).size()}, (1, 2); } catch (hash<ExceptionInfo> ex) { printf("t5b: %s\n", errs(ex)); } }
sub t5c() { try { x = map {"k" + $1: (new E("hc" + $1), 1).size()}, (1, 2), $1 > 0; } catch (hash<ExceptionInfo> ex) { printf("t5c: %s\n", errs(ex)); } }
sub t11() { try { x = (foldr $1 + (new E("fr" + $2), 1).size() + (y, 2).size(), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { printf("t11: %s\n", errs(ex)); } }
sub t12() { try { x = (take 2, (map ((new E("ix" + $1), 1).size() + (y, 2).size()), (select (1, 2, 3), $1 > 0))); } catch (hash<ExceptionInfo> ex) { printf("t12: %s\n", errs(ex)); } }
sub t14() { try { x = (all $1 < 100, (map (new E("al" + $1), $1).size() + (y,1).size(), (1, 2, 3))); } catch (hash<ExceptionInfo> ex) { printf("t14: %s\n", errs(ex)); } }
sub t15() { try { x = (count (select (map (new E("co" + $1), $1).size() + (y,1).size(), (1, 2, 3)), $1 > 0)); } catch (hash<ExceptionInfo> ex) { printf("t15: %s\n", errs(ex)); } }
sub t16() { try { x = (takewhile $1 < 100, (map (new E("tw" + $1), $1).size() + (y,1).size(), (select (1, 2, 3), $1 > 0))); } catch (hash<ExceptionInfo> ex) { printf("t16: %s\n", errs(ex)); } }
# the value of a push keeps the list alive when the variable is assigned another value
sub t20() { try { auto v; x = ((v = (1, "s")), (push v, new E("c")), (v = (2, "s")), (push v, new E("d"))).size(); } catch (hash<ExceptionInfo> ex) { printf("t20: %s\n", errs(ex)); } }
# an exception that a store left in the sink reaches the handler at a push
int sub p(string s) { printf("call %s\n", s); return 1; }
sub t24() { try { auto v = (1, new E("pc")); list w = (); x = ((v = 2), (push w, 3), p("b")).size(); } catch (hash<ExceptionInfo> ex) { printf("t24: %s\n", errs(ex)); } }
# the value of push and unshift is the list
sub push_value() {
    list v = (1,);
    printf("push: %y\n", (push v, 2));
    printf("unshift: %y\n", (unshift v, 0));
    hash h = {"l": (1,)};
    printf("push member: %y\n", (push h.l, 3));
    printf("unshift member: %y\n", (unshift h.l, 0));
}

for (int i = 0; i < 2; ++i) {
    log = ();
    try {
        auto r = ret_in_try();
        log += "got:" + r;
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("ret_in_try: %y\n", log);
}
log = ();
for_body_throw();
printf("for_body_throw: %y\n", log);
foreach string t in ("for_end", "for_break", "for_return", "for_outer_break", "s_end", "s_break", "s_continue", "s_foreach_break", "s_for_break", "s_switch_break", "s_return",
        "s_return_nested", "s_break_inner_try", "t7", "t7a", "t10", "t10b", "t10d", "t5b", "t5c", "t11", "t12", "t14",
        "t15", "t16", "t20", "t24", "push_value") {
    try {
        auto r = call_function(t);
        if (exists r) {
            printf("%s -> %y\n", t, r);
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: ESCAPED %s\n", t, errs(ex));
    }
}
