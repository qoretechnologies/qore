# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the program run by reference-types.qtest in every execution mode and as qcc-compiled executables: each case creates
# references to lvalues of a type and prints the type that the references have at run time, as the runtime overload
# error of a function that no reference matches names it, which variant of an overloaded function the references
# select at run time, or whether a typed container or a typed reference variable takes them

%modern
%disable-all-warnings

hashdecl RefTypeDecl {
    int a = 1;
    string b = "x";
    list<int> l = (1, 2);
}

class RefTypeObj {
    public {
        int i = 1;
        *string os;
        hash<string, int> h = {"a": 1};
        list<string> l = ("a",);
        hash<RefTypeDecl> d();
    }

    #! returns the types of references to the members created in a method
    string memberTypes() {
        list<string> rv = ();
        try { call_function(\no_match(), \i); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \os); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \h); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \l); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \d); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \h.a); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \l[0]); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        try { call_function(\no_match(), \d.b); } catch (hash<ExceptionInfo> ex) { push rv, rt(ex); }
        return rv.join(", ");
    }
}

# a class whose type is first used for a reference by several threads at once
class RefTypeThreadObj {
}

our int g_int = 1;
our hash<string, list<int>> g_hash = {"a": (1,)};

#! no reference matches its only variant: its runtime overload error names the type of the reference
sub no_match(reference<binary> r) {
}

#! returns the type of a reference as the runtime overload error of no_match() named it
string sub rt(hash<ExceptionInfo> ex) {
    *list<auto> m = (ex.desc =~ x/no variant matching 'no_match\((.*)\)'/);
    return m ? m[0] : sprintf("%s: %s", ex.err, (ex.desc.split("\n"))[0]);
}

# the variants of an overloaded function, which a call through call_function() selects at run time by the reference
string sub pick(reference<int> r) {
    return "int";
}

string sub pick(reference<string> r) {
    return "string";
}

string sub pick(reference<*string> r) {
    return "*string";
}

string sub pick(reference<list<int>> r) {
    return "list<int>";
}

string sub pick(reference<hash<string, int>> r) {
    return "hash<string, int>";
}

string sub pick(reference<hash<RefTypeDecl>> r) {
    return "hash<RefTypeDecl>";
}

string sub pick(reference<RefTypeObj> r) {
    return "RefTypeObj";
}

#! prints a result line
sub out(string name, auto v) {
    printf("%s => %s\n", name, v);
}

# references to locals of each type
sub local_cases() {
    int i = 1;
    *int oi;
    string s = "s";
    *string os;
    float f = 1.0;
    number n = 1n;
    bool b = True;
    date d = 2026-01-01;
    binary bin = binary("x");
    auto a = 1;
    hash<auto> ha = {"a": 1};
    hash<string, int> hi = {"a": 1};
    *hash<string, int> ohi;
    list<int> li = (1,);
    list<list<string>> lls = (("a",),);
    hash<RefTypeDecl> hd();
    RefTypeObj o();
    *RefTypeObj oo;
    code c = sub () {};
    softint si = 1;

    try {
        call_function(\no_match(), \i);
        out("local-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-int", rt(ex));
    }
    try {
        call_function(\no_match(), \oi);
        out("local-or-nothing-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-or-nothing-int", rt(ex));
    }
    try {
        call_function(\no_match(), \s);
        out("local-string", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-string", rt(ex));
    }
    try {
        call_function(\no_match(), \os);
        out("local-or-nothing-string", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-or-nothing-string", rt(ex));
    }
    try {
        call_function(\no_match(), \f);
        out("local-float", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-float", rt(ex));
    }
    try {
        call_function(\no_match(), \n);
        out("local-number", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-number", rt(ex));
    }
    try {
        call_function(\no_match(), \b);
        out("local-bool", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-bool", rt(ex));
    }
    try {
        call_function(\no_match(), \d);
        out("local-date", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-date", rt(ex));
    }
    try {
        call_function(\no_match(), \bin);
        out("local-binary", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-binary", rt(ex));
    }
    try {
        call_function(\no_match(), \a);
        out("local-auto", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-auto", rt(ex));
    }
    try {
        call_function(\no_match(), \ha);
        out("local-hash-auto", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-hash-auto", rt(ex));
    }
    try {
        call_function(\no_match(), \hi);
        out("local-hash-string-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-hash-string-int", rt(ex));
    }
    try {
        call_function(\no_match(), \ohi);
        out("local-or-nothing-hash-string-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-or-nothing-hash-string-int", rt(ex));
    }
    try {
        call_function(\no_match(), \li);
        out("local-list-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-list-int", rt(ex));
    }
    try {
        call_function(\no_match(), \lls);
        out("local-list-list-string", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-list-list-string", rt(ex));
    }
    try {
        call_function(\no_match(), \hd);
        out("local-hashdecl", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-hashdecl", rt(ex));
    }
    try {
        call_function(\no_match(), \o);
        out("local-object", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-object", rt(ex));
    }
    try {
        call_function(\no_match(), \oo);
        out("local-or-nothing-object", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-or-nothing-object", rt(ex));
    }
    try {
        call_function(\no_match(), \c);
        out("local-code", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-code", rt(ex));
    }
    try {
        call_function(\no_match(), \si);
        out("local-softint", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("local-softint", rt(ex));
    }

    try {
        out("pick-local-int", call_function(\pick(), \i));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-int", ex.err);
    }
    try {
        out("pick-local-string", call_function(\pick(), \s));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-string", ex.err);
    }
    try {
        out("pick-local-or-nothing-string", call_function(\pick(), \os));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-or-nothing-string", ex.err);
    }
    try {
        out("pick-local-list-int", call_function(\pick(), \li));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-list-int", ex.err);
    }
    try {
        out("pick-local-hash-string-int", call_function(\pick(), \hi));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-hash-string-int", ex.err);
    }
    try {
        out("pick-local-hashdecl", call_function(\pick(), \hd));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-hashdecl", ex.err);
    }
    try {
        out("pick-local-object", call_function(\pick(), \o));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-object", ex.err);
    }
    try {
        out("pick-local-float", call_function(\pick(), \f));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-float", ex.err);
    }
    try {
        out("pick-local-auto", call_function(\pick(), \a));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-local-auto", ex.err);
    }
}

# references to members, keys and elements of typed containers, objects and hashdecls, and to globals
sub path_cases() {
    RefTypeObj o();
    try {
        call_function(\no_match(), \o.i);
        out("member-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-int", rt(ex));
    }
    try {
        call_function(\no_match(), \o.os);
        out("member-or-nothing-string", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-or-nothing-string", rt(ex));
    }
    try {
        call_function(\no_match(), \o.h);
        out("member-hash", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-hash", rt(ex));
    }
    try {
        call_function(\no_match(), \o.l);
        out("member-list", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-list", rt(ex));
    }
    try {
        call_function(\no_match(), \o.d);
        out("member-hashdecl", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-hashdecl", rt(ex));
    }
    try {
        call_function(\no_match(), \o.h.a);
        out("member-hash-key", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-hash-key", rt(ex));
    }
    try {
        call_function(\no_match(), \o.l[0]);
        out("member-list-element", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-list-element", rt(ex));
    }
    try {
        call_function(\no_match(), \o.d.b);
        out("member-hashdecl-member", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("member-hashdecl-member", rt(ex));
    }
    out("method-members", o.memberTypes());
    try {
        out("pick-member-int", call_function(\pick(), \o.i));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-member-int", ex.err);
    }
    try {
        out("pick-member-hash-key", call_function(\pick(), \o.h.a));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-member-hash-key", ex.err);
    }
    try {
        out("pick-member-list-element", call_function(\pick(), \o.l[0]));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-member-list-element", ex.err);
    }

    hash<string, int> hi = {"a": 1};
    list<list<int>> lli = ((1,),);
    hash<RefTypeDecl> hd();
    string k = "a";
    try {
        call_function(\no_match(), \hi.a);
        out("hash-key", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hash-key", rt(ex));
    }
    try {
        call_function(\no_match(), \hi{k});
        out("hash-key-dynamic", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hash-key-dynamic", rt(ex));
    }
    try {
        call_function(\no_match(), \lli[0]);
        out("list-element", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("list-element", rt(ex));
    }
    try {
        call_function(\no_match(), \lli[0][0]);
        out("list-element-element", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("list-element-element", rt(ex));
    }
    try {
        call_function(\no_match(), \hd.a);
        out("hashdecl-member-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hashdecl-member-int", rt(ex));
    }
    try {
        call_function(\no_match(), \hd.l);
        out("hashdecl-member-list", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hashdecl-member-list", rt(ex));
    }
    try {
        call_function(\no_match(), \hd.l[1]);
        out("hashdecl-member-list-element", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hashdecl-member-list-element", rt(ex));
    }
    try {
        out("pick-hash-key", call_function(\pick(), \hi.a));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-hash-key", ex.err);
    }
    try {
        out("pick-list-element", call_function(\pick(), \lli[0]));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-list-element", ex.err);
    }
    try {
        out("pick-hashdecl-member-list", call_function(\pick(), \hd.l));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-hashdecl-member-list", ex.err);
    }

    try {
        call_function(\no_match(), \g_int);
        out("global-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("global-int", rt(ex));
    }
    try {
        call_function(\no_match(), \g_hash);
        out("global-hash", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("global-hash", rt(ex));
    }
    try {
        call_function(\no_match(), \g_hash.a);
        out("global-hash-key", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("global-hash-key", rt(ex));
    }
    try {
        out("pick-global-int", call_function(\pick(), \g_int));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-global-int", ex.err);
    }
}

# references to closure-bound locals, and references to references
sub closure_cases() {
    list<int> l = (1,);
    code get = string sub () {
        try {
            call_function(\no_match(), \l);
            return "accepted";
        } catch (hash<ExceptionInfo> ex) {
            return rt(ex);
        }
    };
    try {
        call_function(\no_match(), \l);
        out("closure-bound-local", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("closure-bound-local", rt(ex));
    }
    out("closure-variable-in-closure", get());
    try {
        out("pick-closure-bound-local", call_function(\pick(), \l));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-closure-bound-local", ex.err);
    }

    hash<string, int> h = {"a": 1};
    reference<hash<string, int>> r = \h;
    try {
        call_function(\no_match(), \r);
        out("reference-variable", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("reference-variable", rt(ex));
    }
    try {
        call_function(\no_match(), \r.a);
        out("reference-variable-key", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("reference-variable-key", rt(ex));
    }
    try {
        out("pick-reference-variable", call_function(\pick(), \r));
    } catch (hash<ExceptionInfo> ex) {
        out("pick-reference-variable", ex.err);
    }
}

# typed containers and typed reference variables that take references
sub container_cases() {
    int i = 1;
    string s = "s";
    hash<string, reference<int>> h = {"i": \i};
    out("hash-of-int-references", h.i);
    try {
        auto a = \s;
        h.s = a;
        out("hash-of-int-references-string", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("hash-of-int-references-string", ex.err);
    }
    list<reference<string>> l = (\s,);
    out("list-of-string-references", l[0]);
    try {
        auto a = \i;
        push l, a;
        out("list-of-string-references-int", "accepted");
    } catch (hash<ExceptionInfo> ex) {
        out("list-of-string-references-int", ex.err);
    }
    reference<int> ri = \i;
    out("reference-to-int", ri);
    try {
        auto a = \s;
        ri = a;
        out("reference-to-int-string", ri);
    } catch (hash<ExceptionInfo> ex) {
        out("reference-to-int-string", ex.err);
    }
}

# references created many times in a loop and by several threads at once have the same type each time
sub repeat_cases() {
    hash<string, int> h = {"a": 1};
    hash<string, int> types;
    for (int n = 0; n < 100; ++n) {
        try { call_function(\no_match(), \h); } catch (hash<ExceptionInfo> ex) { ++types{rt(ex)}; }
        try { call_function(\no_match(), \h.a); } catch (hash<ExceptionInfo> ex) { ++types{rt(ex)}; }
        ++types{call_function(\pick(), \h)};
    }
    printf("loop => %y\n", types);

    Counter start(1);
    Counter done();
    Queue q();
    for (int t = 0; t < 8; ++t) {
        done.inc();
        background sub () {
            on_exit done.dec();
            RefTypeThreadObj o();
            list<RefTypeThreadObj> l = (o,);
            start.waitForZero();
            hash<string, int> seen;
            for (int n = 0; n < 20; ++n) {
                try { call_function(\no_match(), \o); } catch (hash<ExceptionInfo> ex) { ++seen{rt(ex)}; }
                try { call_function(\no_match(), \l); } catch (hash<ExceptionInfo> ex) { ++seen{rt(ex)}; }
                try { call_function(\no_match(), \l[0]); } catch (hash<ExceptionInfo> ex) { ++seen{rt(ex)}; }
            }
            q.push(seen);
        }();
    }
    start.dec();
    done.waitForZero();
    hash<string, int> all;
    for (int t = 0; t < 8; ++t) {
        map all{$1.key} += $1.value, q.get().pairIterator();
    }
    printf("threads => %y\n", all);
}

local_cases();
path_cases();
closure_cases();
container_cases();
repeat_cases();
