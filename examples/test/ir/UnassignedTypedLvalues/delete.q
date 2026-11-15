#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): locals, references, hash keys and list elements cleared by
# delete and remove; its output in every execution mode and as a qcc executable must be that of the AST interpreter

%modern
sub show(string l, code c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", l, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", l, ex.err);
    }
}
sub clear(reference<int> r) { remove r; }
list<auto> sub d1() { int x = 1; delete x; return (x + x,); }
list<auto> sub d2() { int x = 1; remove x; return (x + x,); }
list<auto> sub d3() { int x = 1; clear(\x); return (x + x,); }
list<auto> sub d4() {
    list<auto> r = ();
    int x = 1;
    for (int i = 0; i < 2; ++i) {
        r += (x + x,);
        delete x;
    }
    return r;
}
list<auto> sub d5() { string s = "a"; delete s; return (s + s, s == s); }
list<auto> sub d6() { hash<string, int> h = {"a": 1}; delete h.a; return (h.a + h.a,); }
list<auto> sub d7() { list<int> l = (1, 2); delete l[0]; return (l[0] + l[0],); }
list<auto> sub d8() { hash<string, int> h = {"a": 1}; remove h.a; return (h.a + 1,); }
list<auto> sub d9() { int x = 1; reference<int> r = \x; remove r; return (x + x,); }
show("delete local", \d1()); show("remove local", \d2()); show("remove via reference arg", \d3());
show("delete in loop after read", \d4()); show("delete string local", \d5()); show("delete hash element", \d6());
show("delete list element", \d7()); show("remove hash element", \d8()); show("remove via reference local", \d9());
