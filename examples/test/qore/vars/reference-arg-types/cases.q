# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the program run by reference-arg-types.qtest in every execution mode and as qcc-compiled executables: each case
# passes a reference to a function with a reference parameter and prints the referenced value afterwards (its value,
# its full type, and whether the caller's value is the same value), or the exception that the call raised

%modern
%allow-weak-references
%disable-all-warnings

hashdecl RefArgDecl {
    int a = 1;
    string b = "x";
}

class RefArgHolder {
    public {
        hash<auto> h = {"m": 1};
        *hash<auto> oh;
        list<auto> l = (1,);
        int i = 1;
    }
}

our hash<auto> g_hash = {"g": 1};
our *hash<auto> g_ohash;
our list<auto> g_list = (1, 2);

#! prints a result line
sub out(string name, auto v) {
    printf("%s => %s %y\n", name, v.fullType(), v);
}

#! prints the exception a case raised
sub out_ex(string name, hash<ExceptionInfo> ex) {
    printf("%s => %s: %s\n", name, ex.err, (ex.desc.split("\n"))[0]);
}

#! returns a hash<string, int> built at runtime
hash<string, int> sub typed_hash() {
    hash<string, int> h = {"a": 1};
    h.b = 2;
    return h;
}

#! returns a list<int> built at runtime
list<int> sub typed_list() {
    list<int> l = (1,);
    push l, 2;
    return l;
}

# the reference parameters: each reads the value and changes it if told to
int sub hash_ref(reference<hash<auto>> r, bool write) {
    int n = r.size();
    if (write) {
        r.w = n;
    }
    return n;
}

int sub ohash_ref(reference<*hash<auto>> r, bool write) {
    int n = r.size();
    if (write) {
        r.w = n;
    }
    return n;
}

int sub list_ref(reference<list<auto>> r, bool write) {
    int n = r.size();
    if (write) {
        push r, n;
    }
    return n;
}

int sub olist_ref(reference<*list<auto>> r, bool write) {
    int n = r.size();
    if (write) {
        push r, n;
    }
    return n;
}

int sub typed_hash_ref(reference<hash<string, int>> r, bool write) {
    int n = r.size();
    if (write) {
        r.w = n;
    }
    return n;
}

int sub float_ref(reference<float> r, bool write) {
    if (write) {
        r += 0.5;
    }
    return 1;
}

int sub int_ref(reference<int> r, bool write) {
    if (write) {
        ++r;
    }
    return r;
}

int sub decl_ref(reference<hash<RefArgDecl>> r, bool write) {
    if (write) {
        r.a = 2;
    }
    return r.a;
}

# a reference to a reference: the parameter is passed on, as itself and as a reference to it
int sub pass_on_ref(reference<hash<auto>> r) {
    return hash_ref(\r, True);
}

int sub pass_on_ref_value(reference<hash<auto>> r) {
    hash<auto> copy = r;
    return hash_ref(\copy, True) + copy.size();
}

# the cases of a local of the parameter's type
sub same_type_cases() {
    {
        hash<auto> h = {"a": 1};
        hash<auto> other = h;
        hash_ref(\h, False);
        out("hash-same-type-read", h);
        printf("hash-same-type-read-shared => %y\n", h == other);
        hash_ref(\h, True);
        out("hash-same-type-write", h);
        out("hash-same-type-other", other);
    }
    {
        hash<auto> h = typed_hash();
        hash_ref(\h, False);
        out("hash-same-type-typed-value", h);
        hash_ref(\h, True);
        out("hash-same-type-typed-value-write", h);
    }
    {
        hash<auto> h = <RefArgDecl>{};
        hash_ref(\h, False);
        out("hash-same-type-hashdecl-value", h);
    }
    {
        *hash<auto> h;
        ohash_ref(\h, False);
        out("ohash-same-type-nothing", h);
        ohash_ref(\h, True);
        out("ohash-same-type-nothing-write", h);
        *hash<auto> h2 = typed_hash();
        ohash_ref(\h2, False);
        out("ohash-same-type-typed-value", h2);
    }
    {
        list<auto> l = typed_list();
        list_ref(\l, False);
        out("list-same-type-typed-value", l);
        list_ref(\l, True);
        out("list-same-type-write", l);
    }
    {
        *list<auto> l;
        olist_ref(\l, False);
        out("olist-same-type-nothing", l);
        olist_ref(\l, True);
        out("olist-same-type-nothing-write", l);
    }
    {
        hash<auto> h = {"loop": 0};
        int t = 0;
        for (int i = 0; i < 50; ++i) {
            t += hash_ref(\h, i % 10 == 0);
        }
        out("hash-same-type-loop", h);
        printf("hash-same-type-loop-sum => %d\n", t);
    }
    {
        # a call through a call reference checks the argument at runtime only
        hash<auto> h = {"cr": 1};
        code c = \hash_ref();
        c(\h, True);
        out("hash-same-type-call-ref", h);
        call_function(\hash_ref(), \h, True);
        out("hash-same-type-call-function", h);
    }
}

# a weak reference that a local holds stays weak
sub weak_cases() {
    {
        hash<auto> src = {"a": sprintf("%d", getpid())};
        hash<auto> w := src;
        hash_ref(\w, False);
        remove src;
        printf("hash-weak-stays-weak => %d\n", w.size());
    }
    {
        list<auto> src = (sprintf("%d", getpid()),);
        list<auto> w := src;
        list_ref(\w, False);
        remove src;
        printf("list-weak-stays-weak => %d\n", w.size());
    }
}

# the cases of a local whose type is not the parameter's type
sub other_type_cases() {
    {
        # an untyped local: its value is converted to the parameter's type
        auto a = {"a": 1};
        typed_hash_ref(\a, False);
        out("auto-to-typed-hash", a);
    }
    {
        auto a = 1;
        a = 1;
        float_ref(\a, False);
        out("auto-int-to-float", a);
        float_ref(\a, True);
        out("auto-int-to-float-write", a);
    }
    {
        auto a = 1;
        try {
            call_function(\hash_ref(), \a, False);
            out("auto-int-to-hash", a);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("auto-int-to-hash", ex);
        }
    }
    {
        auto a;
        hash_ref(\a, True);
        out("auto-nothing-to-hash", a);
    }
    {
        auto a = typed_list();
        list_ref(\a, True);
        out("auto-typed-list-to-list", a);
    }
    {
        int i = 1;
        try {
            call_function(\hash_ref(), \i, False);
            out("int-to-hash", i);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("int-to-hash", ex);
        }
    }
    {
        hash<string, int> h = typed_hash();
        try {
            call_function(\hash_ref(), \h, True);
            out("typed-hash-to-hash", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("typed-hash-to-hash", ex);
        }
    }
    {
        hash<auto> h = {"a": 1};
        try {
            call_function(\typed_hash_ref(), \h, True);
            out("hash-to-typed-hash", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("hash-to-typed-hash", ex);
        }
    }
    {
        *hash<auto> h = {"a": 1};
        try {
            call_function(\hash_ref(), \h, True);
            out("ohash-to-hash", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("ohash-to-hash", ex);
        }
    }
    {
        hash<auto> h = {"a": 1};
        try {
            call_function(\ohash_ref(), \h, True);
            out("hash-to-ohash", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("hash-to-ohash", ex);
        }
    }
    {
        list<auto> l = (1,);
        try {
            call_function(\hash_ref(), \l, True);
            out("list-to-hash", l);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("list-to-hash", ex);
        }
    }
    {
        hash<RefArgDecl> h();
        decl_ref(\h, True);
        out("hashdecl-same-type", h);
        try {
            call_function(\hash_ref(), \h, True);
            out("hashdecl-to-hash", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("hashdecl-to-hash", ex);
        }
    }
    {
        hash<auto!> h = typed_hash();
        hash_ref(\h, True);
        out("no-narrow-hash-to-hash", h);
    }
    {
        int i = 1;
        int_ref(\i, True);
        out("int-same-type", i);
    }
}

# the cases of an lvalue that is not a local
sub path_cases() {
    {
        RefArgHolder o();
        hash_ref(\o.h, True);
        out("member-hash", o.h);
        ohash_ref(\o.oh, True);
        out("member-ohash", o.oh);
        list_ref(\o.l, True);
        out("member-list", o.l);
        int_ref(\o.i, True);
        out("member-int", o.i);
        delete o;
        try {
            hash_ref(\o.h, True);
            out("member-deleted-object", o);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("member-deleted-object", ex);
        }
    }
    {
        hash<auto> h = {"k": {"a": 1}, "i": 1};
        hash_ref(\h.k, True);
        out("hash-key-hash", h);
        try {
            hash_ref(\h.i, True);
            out("hash-key-int", h);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("hash-key-int", ex);
        }
        hash_ref(\h.missing, True);
        out("hash-key-missing", h);
    }
    {
        list<auto> l = ({"a": 1}, 2);
        hash_ref(\l[0], True);
        out("list-element-hash", l);
    }
    {
        hash_ref(\g_hash, True);
        out("global-hash", g_hash);
        ohash_ref(\g_ohash, True);
        out("global-ohash", g_ohash);
        list_ref(\g_list, True);
        out("global-list", g_list);
    }
    {
        hash<auto> h = {"r": 1};
        pass_on_ref(\h);
        out("reference-to-reference", h);
        pass_on_ref_value(\h);
        out("reference-value-passed-on", h);
    }
}

# closure-bound variables
sub closure_cases() {
    {
        hash<auto> h = {"c": 1};
        code read = int sub () { return h.size(); };
        hash_ref(\h, True);
        out("closure-bound-local", h);
        printf("closure-bound-local-closure => %d\n", read());
    }
    {
        hash<auto> h = {"c": 1};
        code write = sub () { hash_ref(\h, True); };
        write();
        out("closure-variable-in-closure", h);
    }
    {
        list<auto> l = (1,);
        code write = sub () { list_ref(\l, True); };
        write();
        write();
        out("closure-list-in-closure", l);
    }
    {
        # a reference to a local that a thread changes
        hash<auto> h = {"t": 0};
        Counter c(1);
        background sub () {
            hash_ref(\h, True);
            c.dec();
        }();
        c.waitForZero();
        out("closure-bound-local-thread", h);
    }
}

# references to locals of another Program, which is entered to use them, also after it has been deleted
sub program_cases() {
    {
        # a reference to a local of this Program in an argument list
        hash<auto> h = {"l": 1};
        list<auto> args = (\h, True);
        call_function_args(\hash_ref(), args);
        out("args-list-local", h);
    }
    {
        Program p(PO_NEW_STYLE);
        p.importFunction("hash_ref");
        p.parse("hash<auto> sub run() { hash<auto> x = {\"a\": 1}; hash_ref(\\x, True); hash_ref(\\x, False); "
            "return x; }\n"
            "reference<hash<auto>> sub get_ref() { hash<auto> x = {\"a\": 1}; code c = sub () { x.b = 1; }; "
            "return \\x; }\n"
            "list<auto> sub get_args() { hash<auto> x = {\"a\": 1}; code c = sub () { x.b = 1; }; "
            "return (\\x, True); }\n", "child");
        out("other-program-local", p.callFunction("run"));
        reference<hash<auto>> r = p.callFunction("get_ref");
        hash_ref(\r, True);
        out("other-program-reference", r);
        list<auto> args = p.callFunction("get_args");
        call_function_args(\hash_ref(), args);
        call_function_args(\hash_ref(), args);
        printf("other-program-args-list => %d\n", call_function_args(\hash_ref(), (args[0], False)));
        delete p;
        try {
            hash_ref(\r, True);
            out("deleted-program-reference", r);
        } catch (hash<ExceptionInfo> ex) {
            out_ex("deleted-program-reference", ex);
        }
        try {
            call_function_args(\hash_ref(), args);
            printf("deleted-program-args-list => no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            out_ex("deleted-program-args-list", ex);
        }
    }
}

# a recursion that passes a reference on reaches the stack limit, which raises one exception; the thread then binds
# references as before
int sub stack_rec(reference<hash<auto>> r, int d) {
    hash<auto> h = {"d": d};
    return stack_rec(\h, d + 1);
}

int sub stack_chain(reference<hash<auto>> r, int d) {
    return stack_chain(\r, d + 1);
}

sub stack_cases() {
    foreach string name in (("stack-limit", "stack-limit-chain")) {
        try {
            hash<auto> h = {};
            if (name == "stack-limit") {
                stack_rec(\h, 0);
            } else {
                stack_chain(\h, 0);
            }
            printf("%s => no exception\n", name);
        } catch (hash<ExceptionInfo> ex) {
            # the description gives the stack size, which depends on the execution mode
            printf("%s => %s chained: %y\n", name, ex.err, exists ex.next);
        }
        hash<auto> h = {"x": 1};
        hash_ref(\h, True);
        out(name + "-after", h);
    }
}

# a reference bound in a destructor
class RefArgDtor {
    destructor() {
        hash<auto> h = {"d": 1};
        hash_ref(\h, True);
        out("destructor-local", h);
        list<auto> l = (1,);
        list_ref(\l, True);
        out("destructor-list", l);
    }
}

sub destructor_cases() {
    {
        RefArgDtor d();
    }
    RefArgDtor d2();
    delete d2;
}

same_type_cases();
weak_cases();
other_type_cases();
path_cases();
closure_cases();
program_cases();
stack_cases();
destructor_cases();
