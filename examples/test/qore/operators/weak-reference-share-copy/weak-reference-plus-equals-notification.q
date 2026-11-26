#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# += with an untyped parameter or local that holds a reference changes the referenced lvalue once, under its lock, as
# the AST interpreter changes it: the member notification of an object whose member it is runs once, and what it
# writes is kept; the store of the result back to the local stores nothing (#7421)

%modern
%allow-weak-references

our auto deleted_object;

#! replaces its member m in the notification of the first change that makes it a list of three elements
class Replacing {
    public {
        auto m;
        int n = 0;
        bool done;
    }

    memberNotification(string mem) {
        ++n;
        printf("notify %d %s: %y\n", n, mem, self{mem});
        if (mem == "m" && self.m.typeCode() == NT_LIST && self.m.size() == 3 && !done) {
            done = True;
            self.m = "replaced";
        }
    }
}

#! as Replacing, without output
class QuietReplacing {
    public {
        auto m;
        bool done;
    }

    memberNotification(string mem) {
        if (mem == "m" && self.m.typeCode() == NT_LIST && self.m.size() == 3 && !done) {
            done = True;
            self.m = "replaced";
        }
    }
}

#! deletes itself in the notification of the first change that makes m a list of three elements
class Deleting {
    public {
        auto m;
        bool done;
    }

    memberNotification(string mem) {
        if (mem == "m" && self.m.typeCode() == NT_LIST && self.m.size() == 3 && !done) {
            done = True;
            printf("notification deletes the object\n");
            delete ::deleted_object;
        }
    }

    destructor() {
        printf("Deleting dtor\n");
    }
}

#! counts its notifications
class Counting {
    public {
        auto m;
        auto s;
        auto i;
        int n = 0;
    }

    memberNotification(string mem) {
        ++n;
        printf("notify %d %s: %y\n", n, mem, self{mem});
    }
}

sub add_to(auto p, auto v) {
    p += v;
}

# a notification that replaces the member keeps the value that it assigns
sub notification_replaces() {
    Replacing c();
    c.m = (1, 2);
    auto w;
    w := c.m;
    add_to(\c.m, 3);
    printf("replaced through a parameter: %y, w: %y\n", c.m, w);

    Replacing c2();
    c2.m = (1, 2);
    auto p = \c2.m;
    p += 3;
    printf("replaced through a local: %y\n", c2.m);
}

# a new list, which can be allocated where the list that the notification released was
sub share_check(int v) {
    auto a = (v, 2);
    auto w;
    w := a;
    # a store of the container that the local holds already gives it a copy, and w designates nothing then
    a = a ?? ();
    printf("share check: a: %y, w: %y\n", a, w);
}

# no state of the operation that the notification released affects a later store
sub released_container() {
    QuietReplacing c();
    c.m = (1, 2);
    auto w;
    w := c.m;
    add_to(\c.m, 3);
}

# a notification that deletes the object
sub notification_deletes() {
    deleted_object = new Deleting();
    deleted_object.m = (1, 2);
    try {
        add_to(\deleted_object.m, 3);
        printf("deleted: %y\n", deleted_object);
    } catch (hash<ExceptionInfo> ex) {
        printf("deleted: %s: %s\n", ex.err, ex.desc);
    }
}

# typed lvalues designated by a reference held in an untyped parameter
sub typed_targets() {
    list<int> l = (1, 2);
    auto w;
    w := l;
    add_to(\l, 3);
    printf("typed: %y, %y\n", l, w);
    try {
        add_to(\l, "x");
        printf("typed b: %y, %y\n", l, w);
    } catch (hash<ExceptionInfo> ex) {
        printf("typed b: %s: %s; %y, %y\n", ex.err, ex.desc, l, w);
    }
    softlist<string> sl = ("a",);
    auto ws;
    ws := sl;
    try {
        add_to(\sl, 1);
    } catch (hash<ExceptionInfo> ex) {
        printf("typed c: %s\n", ex.err);
    }
    printf("typed c: %y, %y\n", sl, ws);
    *hash<auto> h;
    add_to(\h, {"a": 1});
    printf("typed d: %y\n", h);
}

# a reference to a local that holds a reference
sub reference_to_reference() {
    auto x = (1, 2);
    auto w;
    w := x;
    auto r = \x;
    add_to(\r, 3);
    printf("reference to reference: %y, %y, %y\n", x, w, r);
}

# a reference to a closure-bound local
sub closure_bound() {
    auto x = (1, 2);
    auto w;
    w := x;
    code f = sub () { x += 4; };
    add_to(\x, 3);
    f();
    printf("closure-bound: %y, %y\n", x, w);
}

# references to hash and list elements
sub element_targets() {
    hash<auto> h = {"a": (1, 2)};
    auto w;
    w := h.a;
    add_to(\h.a, 3);
    printf("hash element: %y, %y\n", h, w);
    list<auto> l = ((1, 2),);
    auto wl;
    wl := l[0];
    add_to(\l[0], 3);
    printf("list element: %y, %y\n", l, wl);
}

# operands of other types
sub other_types() {
    auto x = (1, 2);
    auto w;
    w := x;
    try {
        add_to(\x, {"a": 1});
    } catch (hash<ExceptionInfo> ex) {
        printf("list += hash: %s\n", ex.err);
    }
    printf("list += hash: %y, %y\n", x, w);
    auto y = {"a": 1};
    auto wy;
    wy := y;
    try {
        add_to(\y, (1, 2));
        printf("hash += list: %y, %y\n", y, wy);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash += list: %s: %s; %y, %y\n", ex.err, ex.desc, y, wy);
    }
    auto s = "abc";
    try {
        add_to(\s, (1,));
    } catch (hash<ExceptionInfo> ex) {
        printf("string += list: %s\n", ex.err);
    }
    printf("string += list: %y\n", s);
    auto n;
    try {
        add_to(\n, (1,));
    } catch (hash<ExceptionInfo> ex) {
        printf("nothing += list: %s\n", ex.err);
    }
    printf("nothing += list: %y\n", n);
    auto d = 1;
    try {
        add_to(\d, (1,));
    } catch (hash<ExceptionInfo> ex) {
        printf("int += list: %s\n", ex.err);
    }
    printf("int += list: %y\n", d);
}

# the value of += through a reference
sub result_used() {
    auto x = (1, 2);
    auto w;
    w := x;
    auto p = \x;
    auto r = (p += 3);
    printf("result: %y, %y, %y\n", x, w, r);
    auto q = \x;
    auto r2 = (q += (p += 5));
    printf("nested result: %y, %y, %y\n", x, w, r2);
}

# each += through a reference notifies the change once
sub notifications() {
    Counting c();
    c.i = 1;
    c.s = "a";
    c.m = {"a": 1};
    add_to(\c.i, 2);
    add_to(\c.s, "b");
    add_to(\c.m, {"b": 2});
    auto p = \c.i;
    p += 5;
    printf("notifications: %y %y %y\n", c.i, c.s, c.m);
}

#! runs a case by its name: each case is called by name, so that JIT and tiered execution run it as native code
sub run_case(string name) {
    switch (name) {
        case "notification_replaces": notification_replaces(); break;
        case "released_container": released_container(); share_check(2); break;
        case "notification_deletes": notification_deletes(); break;
        case "typed_targets": typed_targets(); break;
        case "reference_to_reference": reference_to_reference(); break;
        case "closure_bound": closure_bound(); break;
        case "element_targets": element_targets(); break;
        case "other_types": other_types(); break;
        case "result_used": result_used(); break;
        case "notifications": notifications(); break;
    }
}

const CaseNames = (
    "notification_replaces",
    "released_container",
    "notification_deletes",
    "typed_targets",
    "reference_to_reference",
    "closure_bound",
    "element_targets",
    "other_types",
    "result_used",
    "notifications",
);

# top-level code, which qcc compiles on its own (#7421): a block local and a root-level local that hold a reference
our list<auto> top_block_list = (1,);
our auto top_block_weak;
top_block_weak := top_block_list;
{
    auto p = \top_block_list;
    p += 7;
}
printf("top-level block local: %y, %y\n", top_block_list, top_block_weak);

our hash<auto> top_root_hash = {"a": 1};
our auto top_root_weak;
top_root_weak := top_root_hash;
auto top_root_ref = \top_root_hash;
top_root_ref += {"b": 2};
printf("top-level root local: %y, %y\n", top_root_hash, top_root_weak);

auto top_list = (1, 2);
auto top_list_weak;
top_list_weak := top_list;
top_list += 3;
printf("top-level root local list: %y, %y\n", top_list, top_list_weak);

# a case named on the command line runs alone
*string only = ARGV[0];
foreach string n in (CaseNames) {
    if (!only || only == n) {
        try {
            run_case(n);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s: %s\n", n, ex.err, ex.desc);
        }
    }
}
printf("done\n");
