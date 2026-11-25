# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the program run by reference-reads.qtest in every execution mode and as qcc-compiled executables: each case reads
# values through references (to locals, closure-bound locals, globals, members, hash keys, list elements, references,
# locals of other Programs, weak references), and prints what it read, or the exception that the read raised

%modern
%allow-weak-references
%disable-all-warnings

class RefReadHolder {
    public {
        int i = 1;
        hash<auto> h = {"k": 1};
        list<auto> l = (1, 2);
    }

    #! reads a member through a reference created in the method
    int readMember() {
        return read_int(\i);
    }

    #! reads a local of the method through a reference: the read has no object context of the method
    string readLocal() {
        string s = "m";
        string rv;
        for (int n = 0; n < 3; ++n) {
            s += n;
            rv += read_string(\s);
        }
        return rv;
    }

    #! reads a local of the method through a reference in a method of another object
    string readLocalOther(RefReadHolder other) {
        string s = "o";
        return other.readRef(\s) + ":" + i;
    }

    #! reads a reference given to the method and a member afterwards
    string readRef(reference<string> r) {
        return r + "-" + h.k;
    }

    #! reads a hash key of a member through a reference
    int readMemberKey() {
        return read_int(\h.k);
    }
}

our int g_int = 7;
our hash<auto> g_hash = {"g": 1};

#! prints a result line
sub out(string name, auto v) {
    printf("%s => %s %y\n", name, v.fullType(), v);
}

#! prints the exception a case raised
sub out_ex(string name, hash<ExceptionInfo> ex) {
    printf("%s => %s: %s\n", name, ex.err, (ex.desc.split("\n"))[0]);
}

#! runs a case, printing its result or the exception it raised
sub run(string name, code c) {
    try {
        out(name, c());
    } catch (hash<ExceptionInfo> ex) {
        out_ex(name, ex);
    }
}

# the readers: each reads the referenced value
auto sub read_auto(reference<auto> r) {
    return r;
}

int sub read_int(reference<int> r) {
    return r;
}

string sub read_string(reference<string> r) {
    return r;
}

auto sub read_key(reference<hash<auto>> r, string k) {
    return r{k};
}

int sub read_size(reference<auto> r) {
    return r.size();
}

#! reads the value through a reference to the reference given, \a d times deep
auto sub read_chain(reference<auto> r, int d) {
    if (!d) {
        return r;
    }
    return read_chain(\r, d - 1);
}

#! reads the value through each reference of a list
list<auto> sub read_list_refs(list<auto> l) {
    list<auto> rv = ();
    for (int i = 0; i < l.size(); ++i) {
        push rv, read_auto(\l[i]);
    }
    return rv;
}

# reads of locals, in loops, so that the tiered mode compiles the readers
sub local_cases() {
    {
        int i = 1;
        int sum;
        for (int n = 0; n < 50; ++n) {
            i = n;
            sum += read_int(\i);
        }
        out("local-int-loop", sum);
    }
    {
        string s = "a";
        out("local-string", read_string(\s));
        s = "b";
        out("local-string-changed", read_string(\s));
    }
    {
        hash<auto> h = {"a": 1, "b": (1, 2)};
        out("local-hash", read_auto(\h));
        out("local-hash-key", read_key(\h, "b"));
        out("local-hash-missing-key", read_key(\h, "x"));
        out("local-hash-missing-key-not-created", h.hasKey("x"));
        out("local-hash-same", read_auto(\h) == h);
    }
    {
        list<auto> l = (1, "two", 3.0);
        out("local-list", read_auto(\l));
        out("local-list-size", read_size(\l));
    }
    {
        auto x;
        out("local-nothing", read_auto(\x));
        x = NULL;
        out("local-null", read_auto(\x));
    }
    {
        RefReadHolder o();
        RefReadHolder r = read_auto(\o);
        out("local-object-same", r == o);
        delete o;
        out("local-object-deleted", read_auto(\o));
    }
    {
        code c = int sub (int x) { return x * 2; };
        code rc = read_auto(\c);
        out("local-closure-call", rc(21));
    }
    {
        # the same reference read many times in one function
        hash<auto> h = {"k": 1};
        int n;
        for (int i = 0; i < 20; ++i) {
            n += read_key(\h, "k");
            h.k = i;
        }
        out("local-hash-key-loop", n);
    }
}

# reads of locals bound by closures, which change them between the reads
sub closure_cases() {
    {
        int i = 1;
        code inc = sub () { ++i; };
        list<int> l;
        for (int n = 0; n < 5; ++n) {
            inc();
            push l, read_int(\i);
        }
        out("closure-bound-local", l);
    }
    {
        hash<auto> h = {"c": 1};
        code read = auto sub () { return read_auto(\h); };
        h.d = 2;
        out("closure-variable-in-closure", read());
    }
    {
        string s = "x";
        # a closure that reads a local of its creator through a reference after the creator changed it
        code reader = string sub () { return read_string(\s); };
        s = "y";
        out("closure-read-after-change", reader());
    }
}

# reads of other lvalues: globals, members, hash keys, list elements
sub path_cases() {
    out("global-int", read_int(\g_int));
    out("global-hash", read_auto(\g_hash));
    out("global-hash-key", read_auto(\g_hash.g));
    {
        RefReadHolder o();
        out("member-int", read_int(\o.i));
        out("member-hash-key", read_auto(\o.h.k));
        out("member-list-element", read_auto(\o.l[1]));
        out("method-member", o.readMember());
        out("method-member-key", o.readMemberKey());
        out("method-local", o.readLocal());
        RefReadHolder o2();
        o2.h.k = 2;
        out("method-local-other-object", o.readLocalOther(o2));
    }
    {
        hash<auto> h = {"k": {"n": 1}};
        out("hash-key", read_auto(\h.k));
        out("hash-key-nested", read_auto(\h.k.n));
        out("hash-key-missing", read_auto(\h.missing));
        out("hash-key-missing-not-created", h.hasKey("missing"));
    }
    {
        list<auto> l = (1, (2, 3));
        out("list-element", read_auto(\l[1]));
        out("list-element-nested", read_auto(\l[1][0]));
        out("list-element-past-end", read_auto(\l[5]));
        out("list-element-past-end-size", l.size());
    }
}

# reads through references to references, and references held in containers
sub reference_cases() {
    {
        hash<auto> h = {"r": 1};
        out("reference-to-reference", read_chain(\h, 1));
        out("reference-chain-5", read_chain(\h, 5));
        # deeper than the references the thread keeps without an allocation
        out("reference-chain-40", read_chain(\h, 40));
        int i = 3;
        out("reference-chain-200", read_chain(\i, 200));
    }
    {
        int a = 1;
        string b = "b";
        list<auto> l = (\a, \b);
        a = 2;
        out("references-in-list", read_list_refs(l));
        hash<auto> h = {"a": \a};
        out("reference-in-hash", read_auto(\h.a));
        out("reference-in-hash-call", call_function_args(\read_auto(), (h.a,)));
    }
    {
        hash<auto> h = {"w": 1};
        reference<hash<auto>> r = \h;
        out("reference-variable", read_auto(\r));
        h.w = 2;
        out("reference-variable-changed", r);
    }
}

# reads through weak references, which keep their semantics
sub weak_cases() {
    {
        hash<auto> h = {"w": 1};
        hash<auto> wh := h;
        out("weak-hash", read_auto(\wh));
        list<auto> l = (1, 2);
        list<auto> wl := l;
        out("weak-list", read_auto(\wl));
        RefReadHolder o();
        RefReadHolder wo := o;
        out("weak-object-same", read_auto(\wo) == o);
        delete o;
        out("weak-object-deleted-is-object", read_auto(\wo) instanceof object);
    }
}

# circular references raise an exception when they are read
sub circular_cases() {
    run("circular-self", auto sub () { auto a = 1; reference r = \a; a = \r; return read_auto(\a); });
    run("circular-self-direct", auto sub () { auto a = 1; reference r = \a; a = \r; return a; });
    run("circular-self-reference", auto sub () { auto a = 1; reference r = \a; a = \r; return r; });
    run("circular-two", auto sub () {
        auto a = 1;
        auto b = 2;
        reference ra = \a;
        reference rb = \b;
        a = \rb;
        b = \ra;
        return read_auto(\a);
    });
    run("circular-through-chain", auto sub () {
        auto a = 1;
        reference r = \a;
        a = \r;
        # resolved deeper than the references kept without an allocation
        return read_chain(\a, 30);
    });
    # a reference held in a container is not resolved again when the container is read
    run("circular-in-list", auto sub () {
        list<auto!> l = (1,);
        reference r = \l;
        l[0] = \r;
        # the element is a reference to the list, which reads as the list
        auto v = read_auto(\l[0]);
        return v.fullType() + " " + v.size();
    });
    run("circular-in-hash", auto sub () {
        hash<auto!> h = {"a": 1};
        reference r = \h;
        h.a = \r;
        auto v = read_auto(\h.a);
        return v.fullType() + " " + v.size();
    });
    # the thread reads references as before after the exceptions
    {
        hash<auto> h = {"after": 1};
        out("circular-after", read_auto(\h));
        out("circular-after-chain", read_chain(\h, 20));
    }
}

# references to locals of another Program, which is entered to read them, also after it has been deleted
sub program_cases() {
    Program p(PO_NEW_STYLE);
    p.importFunction("read_auto");
    p.parse("auto sub run() { hash<auto> x = {\"a\": 1}; return read_auto(\\x); }\n"
        "reference<auto> sub get_ref() { hash<auto> x = {\"a\": 1}; code c = sub () { x.b = 1; }; "
        "return \\x; }\n"
        "auto sub read_given(reference<auto> r) { return r; }\n"
        "list<auto> sub get_args() { hash<auto> x = {\"l\": 1}; return (\\x,); }\n", "child");
    out("other-program-local", p.callFunction("run"));
    reference<auto> r = p.callFunction("get_ref");
    out("other-program-reference", read_auto(\r));
    out("other-program-reference-direct", r);
    {
        hash<auto> h = {"parent": 1};
        out("other-program-reads-parent-local", p.callFunction("read_given", \h));
    }
    list<auto> args = p.callFunction("get_args");
    out("other-program-args-list", call_function_args(\read_auto(), args));
    delete p;
    try {
        out("deleted-program-reference", read_auto(\r));
    } catch (hash<ExceptionInfo> ex) {
        out_ex("deleted-program-reference", ex);
    }
    try {
        out("deleted-program-reference-direct", r);
    } catch (hash<ExceptionInfo> ex) {
        out_ex("deleted-program-reference-direct", ex);
    }
    try {
        out("deleted-program-args-list", call_function_args(\read_auto(), args));
    } catch (hash<ExceptionInfo> ex) {
        out_ex("deleted-program-args-list", ex);
    }
}

# a recursion that reads through a reference that it passes on reaches the stack limit, which raises one exception;
# the thread then reads references as before
auto sub stack_read(reference<auto> r, int d) {
    auto v = r;
    return stack_read(\r, d + 1);
}

auto sub stack_read_local(reference<auto> r, int d) {
    hash<auto> h = {"d": d};
    auto v = r;
    return stack_read_local(\h, d + 1);
}

sub stack_cases() {
    foreach string name in (("stack-limit-chain", "stack-limit-local")) {
        try {
            hash<auto> h = {"s": 1};
            if (name == "stack-limit-chain") {
                stack_read(\h, 0);
            } else {
                stack_read_local(\h, 0);
            }
            printf("%s => no exception\n", name);
        } catch (hash<ExceptionInfo> ex) {
            # the description gives the stack size, which depends on the execution mode
            printf("%s => %s\n", name, ex.err);
        }
        hash<auto> h = {"x": 1};
        out(name + "-after", read_auto(\h));
    }
}

# reads and writes through references to a closure-bound local from several threads at once
const Threads = 4;
const Iterations = 2000;

sub reader_thread(reference<int> r, Counter start, Counter done, reference<hash<auto>> res, int id) {
    start.waitForZero();
    int last = -1;
    bool ok = True;
    for (int n = 0; n < Iterations; ++n) {
        # each read gives a value at least the one read before, as the threads only increment it; the reference given
        # is read directly and through a reference to it
        int v = (n % 2) ? r : read_int(\r);
        if (v < last) {
            ok = False;
        }
        last = v;
        r += 1;
    }
    res{"t" + id} = ok;
    done.dec();
}

sub inc(reference<int> r) {
    r += 1;
}

sub thread_cases() {
    int x = 0;
    hash<auto> res;
    Counter start(1);
    Counter done(Threads);
    for (int i = 0; i < Threads; ++i) {
        background reader_thread(\x, start, done, \res, i);
    }
    start.dec();
    done.waitForZero();
    out("threads-sum", x);
    out("threads-reads-monotonic", (foldl $1 && $2, res.values()));
    out("threads-count", res.size());

    # a closure that threads run reads and increments the variable of its creator through references
    int y = 0;
    hash<auto> res2;
    Counter start2(1);
    Counter done2(Threads);
    code worker = sub (int id) {
        start2.waitForZero();
        int last = -1;
        bool ok = True;
        for (int n = 0; n < Iterations; ++n) {
            int v = read_int(\y);
            if (v < last) {
                ok = False;
            }
            last = v;
            inc(\y);
        }
        res2{"t" + id} = ok;
        done2.dec();
    };
    for (int i = 0; i < Threads; ++i) {
        background worker(i);
    }
    start2.dec();
    done2.waitForZero();
    out("threads-closure-sum", y);
    out("threads-closure-reads-monotonic", (foldl $1 && $2, res2.values()));
}

# reads in destructors, also one that runs while an exception propagates
class RefReadDtor {
    private {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        hash<auto> h = {"d": 1};
        out(name, read_auto(\h));
        int i = 2;
        out(name + "-int", read_int(\i));
    }
}

sub destructor_cases() {
    {
        RefReadDtor d("destructor-scope");
    }
    RefReadDtor d2("destructor-delete");
    delete d2;
    try {
        RefReadDtor d3("destructor-exception");
        throw "REF-READ-TEST", "x";
    } catch (hash<ExceptionInfo> ex) {
        printf("destructor-exception-caught => %s\n", ex.err);
    }
}

local_cases();
closure_cases();
path_cases();
reference_cases();
weak_cases();
circular_cases();
program_cases();
stack_cases();
thread_cases();
destructor_cases();
