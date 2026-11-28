#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.
#
# The scenario of weak-store-deleted-object.qtest (qore#7778): a weak (:=) and an opaque (@=) store of a value into
# typed and untyped locals, members, hash keys and list elements.  The value is a weak reference to a deleted object,
# a weak reference to a live object (which is deleted after the store), NOTHING or an int.  Each case prints the
# exception that the store raises, if any, and the value that the lvalue holds afterwards: read directly, as a member
# of a local hash built from it, and after the live object is deleted.  Each store is a function of its own, so
# that native code analyzes each lvalue apart; every function runs three times, so that tiered execution runs it on
# the AST tier, the IR tier and as native code.

%modern
%allow-weak-references
%allow-opaque-references

class WsdObj {
    public {
        int a;
    }

    constructor(int a) {
        self.a = a;
    }
}

class WsdHolder {
    public {
        int i = 0;
        *int ni;
        *object o;
        any a;
    }
}

# describes a value without converting it
string sub wsd_desc(auto v) {
    try {
        if (v.typeCode() == NT_OBJECT) {
            return sprintf("object a=%d", v.a);
        }
        if (v.typeCode() == NT_LIST) {
            return sprintf("list(%s)", (foldl $1 + "," + $2, (map wsd_desc($1), v)) ?? "");
        }
        return sprintf("%s %y", v.type(), v);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# prints the result of the store of a case
sub wsd_report(string name, *hash<ExceptionInfo> ex, string after, string copy, string later) {
    printf("%s: %s; value: %s; copy: %s; after delete: %s\n", name, ex ? ex.err + " (" + ex.desc + ")" : "ok",
        after, copy, later);
}

# weak store into int
sub wsd_case_1(int kind) {
    string name = sprintf("weak int, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    int t = 0;
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into *int
sub wsd_case_2(int kind) {
    string name = sprintf("weak *int, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    *int t;
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into string
sub wsd_case_3(int kind) {
    string name = sprintf("weak string, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    string t = "";
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into object
sub wsd_case_4(int kind) {
    string name = sprintf("weak object, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    object t = new WsdObj(0);
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into *object
sub wsd_case_5(int kind) {
    string name = sprintf("weak *object, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    *object t;
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into softlist
sub wsd_case_6(int kind) {
    string name = sprintf("weak softlist, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    softlist t = ();
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into any
sub wsd_case_7(int kind) {
    string name = sprintf("weak any, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    any t;
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into auto
sub wsd_case_8(int kind) {
    string name = sprintf("weak auto, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    auto t;
    *hash<ExceptionInfo> err;
    try {
        t := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# weak store into int member
sub wsd_case_9(int kind) {
    string name = sprintf("weak int member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.i := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.i);
    hash<auto> c = {"v": h.i};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.i));
}

# weak store into *int member
sub wsd_case_10(int kind) {
    string name = sprintf("weak *int member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.ni := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.ni);
    hash<auto> c = {"v": h.ni};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.ni));
}

# weak store into *object member
sub wsd_case_11(int kind) {
    string name = sprintf("weak *object member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.o := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.o);
    hash<auto> c = {"v": h.o};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.o));
}

# weak store into any member
sub wsd_case_12(int kind) {
    string name = sprintf("weak any member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.a := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.a);
    hash<auto> c = {"v": h.a};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.a));
}

# weak store into hash<auto> key
sub wsd_case_13(int kind) {
    string name = sprintf("weak hash<auto> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<auto> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# weak store into hash<string, int> key
sub wsd_case_14(int kind) {
    string name = sprintf("weak hash<string, int> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<string, int> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# weak store into hash<string, *object> key
sub wsd_case_15(int kind) {
    string name = sprintf("weak hash<string, *object> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<string, *object> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# weak store into list<auto> element
sub wsd_case_16(int kind) {
    string name = sprintf("weak list<auto> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<auto> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

# weak store into list<int> element
sub wsd_case_17(int kind) {
    string name = sprintf("weak list<int> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<int> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

# weak store into list<*int> element
sub wsd_case_18(int kind) {
    string name = sprintf("weak list<*int> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<*int> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] := src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

# opaque store into int
sub wsd_case_19(int kind) {
    string name = sprintf("opaque int, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    int t = 0;
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into *int
sub wsd_case_20(int kind) {
    string name = sprintf("opaque *int, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    *int t;
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into string
sub wsd_case_21(int kind) {
    string name = sprintf("opaque string, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    string t = "";
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into object
sub wsd_case_22(int kind) {
    string name = sprintf("opaque object, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    object t = new WsdObj(0);
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into *object
sub wsd_case_23(int kind) {
    string name = sprintf("opaque *object, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    *object t;
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into softlist
sub wsd_case_24(int kind) {
    string name = sprintf("opaque softlist, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    softlist t = ();
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into any
sub wsd_case_25(int kind) {
    string name = sprintf("opaque any, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    any t;
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into auto
sub wsd_case_26(int kind) {
    string name = sprintf("opaque auto, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    auto t;
    *hash<ExceptionInfo> err;
    try {
        t @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(t);
    hash<auto> c = {"v": t};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(t));
}

# opaque store into int member
sub wsd_case_27(int kind) {
    string name = sprintf("opaque int member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.i @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.i);
    hash<auto> c = {"v": h.i};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.i));
}

# opaque store into *int member
sub wsd_case_28(int kind) {
    string name = sprintf("opaque *int member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.ni @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.ni);
    hash<auto> c = {"v": h.ni};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.ni));
}

# opaque store into *object member
sub wsd_case_29(int kind) {
    string name = sprintf("opaque *object member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.o @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.o);
    hash<auto> c = {"v": h.o};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.o));
}

# opaque store into any member
sub wsd_case_30(int kind) {
    string name = sprintf("opaque any member, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    WsdHolder h();
    *hash<ExceptionInfo> err;
    try {
        h.a @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.a);
    hash<auto> c = {"v": h.a};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.a));
}

# opaque store into hash<auto> key
sub wsd_case_31(int kind) {
    string name = sprintf("opaque hash<auto> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<auto> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# opaque store into hash<string, int> key
sub wsd_case_32(int kind) {
    string name = sprintf("opaque hash<string, int> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<string, int> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# opaque store into hash<string, *object> key
sub wsd_case_33(int kind) {
    string name = sprintf("opaque hash<string, *object> key, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    hash<string, *object> h = {};
    *hash<ExceptionInfo> err;
    try {
        h.k @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(h.k);
    hash<auto> c = {"v": h.k};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(h.k));
}

# opaque store into list<auto> element
sub wsd_case_34(int kind) {
    string name = sprintf("opaque list<auto> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<auto> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

# opaque store into list<int> element
sub wsd_case_35(int kind) {
    string name = sprintf("opaque list<int> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<int> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

# opaque store into list<*int> element
sub wsd_case_36(int kind) {
    string name = sprintf("opaque list<*int> element, %s", ("deleted", "live", "NOTHING", "int")[kind]);
    any src;
    *WsdObj keep;
    switch (kind) {
        case 0: {
            WsdObj o(1);
            src := o;
            delete o;
            break;
        }
        case 1:
            keep = new WsdObj(2);
            src := keep;
            break;
        case 3:
            src = 5;
            break;
    }
    list<*int> l = ();
    *hash<ExceptionInfo> err;
    try {
        l[0] @= src;
    } catch (hash<ExceptionInfo> ex) {
        err = ex;
    }
    string after = wsd_desc(l[0]);
    hash<auto> c = {"v": l[0]};
    string copy = wsd_desc(c.v);
    delete keep;
    wsd_report(name, err, after, copy, wsd_desc(l[0]));
}

for (int i = 0; i < 3; ++i) {
    printf("run %d\n", i);
    foreach code f in ((\wsd_case_1(), \wsd_case_2(), \wsd_case_3(), \wsd_case_4(), \wsd_case_5(), \wsd_case_6(), \wsd_case_7(), \wsd_case_8(), \wsd_case_9(), \wsd_case_10(), \wsd_case_11(), \wsd_case_12(), \wsd_case_13(), \wsd_case_14(), \wsd_case_15(), \wsd_case_16(), \wsd_case_17(), \wsd_case_18(), \wsd_case_19(), \wsd_case_20(), \wsd_case_21(), \wsd_case_22(), \wsd_case_23(), \wsd_case_24(), \wsd_case_25(), \wsd_case_26(), \wsd_case_27(), \wsd_case_28(), \wsd_case_29(), \wsd_case_30(), \wsd_case_31(), \wsd_case_32(), \wsd_case_33(), \wsd_case_34(), \wsd_case_35(), \wsd_case_36())) {
        for (int kind = 0; kind < 4; ++kind) {
            f(kind);
        }
    }
}
