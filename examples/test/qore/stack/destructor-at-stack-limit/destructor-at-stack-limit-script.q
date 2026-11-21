%modern
# Copyright 2026 Qore Technologies, s.r.o.
# the script of destructor-at-stack-limit.qtest, run in each execution mode and as a qcc executable: each scenario
# runs on a thread of its own and prints a line with its results; the arguments are the path of a file to write and
# the scenarios to run: "small" (on a small thread stack) or "big"; the two sets run in
# processes of their own, as the stack of a new thread can be that of a thread that ended with a larger stack size

#! the number of objects made and destroyed by class
our hash<string, int> made;
our hash<string, int> destroyed;
our string path = ARGV[0];
our string scenarios = ARGV[1];

# the counters are incremented without a call, as a call made by a constructor at the limit fails
sub reset() { made = {}; destroyed = {}; }

class X {
    constructor() { ++made.X; }
    destructor() { ++destroyed.X; }
}

#! writes to the file and closes it in its destructor
class F {
    private { File f(); }
    constructor() { f.open2(path, O_WRONLY | O_CREAT | O_APPEND); ++made.F; }
    destructor() { f.write("x"); f.close(); ++destroyed.F; }
}

#! a destructor that recurses until the stack is exhausted and writes to the file at the deepest point
our int floor_io = 0;
sub rec(int d) {
    try {
        rec(d + 1);
    } catch (hash<ExceptionInfo> ex) {
        if (!floor_io) {
            File f();
            f.open2(path, O_WRONLY | O_CREAT | O_APPEND);
            f.write("floor");
            f.close();
            ++floor_io;
        }
    }
}
class R {
    constructor() { ++made.R; }
    destructor() { rec(0); ++destroyed.R; }
}

#! a destructor that recurses without end: it raises STACK-LIMIT-EXCEEDED, and the depth that it reaches is kept
our int depth = 0;
sub deep(int d) {
    depth = d;
    deep(d + 1);
}
our *string deep_err;
class D {
    constructor() { ++made.D; }
    destructor() {
        ++destroyed.D;
        try {
            deep(1);
        } catch (hash<ExceptionInfo> ex) {
            deep_err = ex.err;
        }
    }
}

#! each destructor deletes the next object of the chain
class N {
    public { *N next; }
    constructor() { ++made.N; }
    destructor() { ++destroyed.N; delete next; }
}

#! the value released at the deepest point of the recursion
our auto released;

#! recurses until the stack is exhausted; each frame holds an object, and the deepest one releases "released"
int sub r(list<auto> l, int d, bool io) {
    list<auto> l2 = (0, io ? new F() : new X());
    try {
        r(l2, d + 1, io);
    } catch (hash<ExceptionInfo> ex) {
        remove released;
        rethrow;
    }
    return l.size();
}

#! returns the number of exceptions in the chain that a destructor raised: a destructor that could not run because of
#! the stack raises STACK-LIMIT-EXCEEDED, which is chained to the exception being raised
int sub dtor_errors(hash<ExceptionInfo> ex) {
    int n = 0;
    *hash<ExceptionInfo> e = ex;
    while (e) {
        if ((e.callstack[0].function ?? "") =~ /::destructor$/) {
            ++n;
        }
        e = e.next;
    }
    return n;
}

string sub counts(string cls) {
    return sprintf("%s=%d/%d", cls, made{cls} ?? 0, destroyed{cls} ?? 0);
}

#! runs a scenario on a new thread and returns its exception
string sub run(code c, int stack_size) {
    set_default_thread_stack_size(stack_size);
    Queue q();
    background sub () {
        # the result is given also if the code that makes it fails
        string res = "no result";
        on_exit q.push(res);
        int size = get_stack_size();
        try {
            c();
            res = sprintf("none stack=%d", size);
        } catch (hash<ExceptionInfo> ex) {
            res = sprintf("%s dtor=%d stack=%d", ex.err, dtor_errors(ex), size);
        }
    }();
    return q.get();
}

# a small thread stack: the smallest one on which the script's own code runs on every platform (48 KB above the stack
# guard of 80 KB on x86_64 and aarch64, a size that macOS accepts, as it requires whole pages of 16 KB; glibc gives a
# thread this size for any size from the guard's up)
const Small = 128 * 1024;
const Big = 256 * 1024;

const SandboxCode = "
our *int depth;
our *string deep_err;
sub deep(int d) {
    depth = d;
    deep(d + 1);
}
class SD {
    destructor() {
        try {
            deep(1);
        } catch (hash<ExceptionInfo> ex) {
            deep_err = ex.err;
        }
    }
}
class SX {
}
our auto released;
int sub r(list<auto> l, int d) {
    list<auto> l2 = (0, new SX());
    try {
        r(l2, d + 1);
    } catch (hash<ExceptionInfo> ex) {
        remove released;
        rethrow;
    }
    return l.size();
}
sub start(*code make) {
    # the sandbox holds the only reference to the object: one of trusted code is made by a closure of the host
    released = make ? make() : new SD();
    r((0, new SX()), 0);
}
int sub get_depth() { return depth ?? 0; }
*string sub get_err() { return deep_err; }
";

string rv;
if (scenarios == "small") {
    # destructors that do I/O, on a small thread stack
    reset();
    unlink(path);
    rv = run(sub () { r((0, new F()), 0, True); }, Small);
    printf("io: %s %s file=%d\n", rv, counts("F"), hstat(path).size);
    # a destructor of an object released at the limit recurses to the floor and does I/O there
    reset();
    unlink(path);
    released = new R();
    rv = run(sub () { r((0, new X()), 0, False); }, Small);
    printf("floor: %s %s %s floor_io=%d file=%d\n", rv, counts("X"), counts("R"), floor_io, hstat(path).size);
} else {
    # objects released while the stack unwinds from the limit: their destructors run on the slice of the stack guard
    rv = run(sub () { r((0, new X()), 0, False); }, Big);
    printf("objects: %s %s\n", rv, counts("X"));
    # twice on one thread: the floor is reset when the destructor returns, so each run is the same
    reset();
    rv = run(sub () {
        for (int i = 0; i < 2; ++i) {
            hash<string, int> before = made;
            hash<string, int> before_destroyed = destroyed;
            try {
                r((0, new X()), 0, False);
            } catch (hash<ExceptionInfo> ex) {
                printf("twice %d: %s dtor=%d X=%d/%d\n", i, ex.err, dtor_errors(ex), made.X - (before.X ?? 0),
                    destroyed.X - (before_destroyed.X ?? 0));
            }
        }
    }, Big);
    printf("twice: %s %s\n", rv, counts("X"));
    # a destructor of an object released at the limit that recurses without end raises STACK-LIMIT-EXCEEDED at the floor
    reset();
    released = new D();
    rv = run(sub () { r((0, new X()), 0, False); }, Big);
    printf("recursion: %s %s %s depth=%d err=%s\n", rv, counts("X"), counts("D"), depth, deep_err);
    # a chain of destructors that delete each other, released at the limit: the slice is used up, and the rest fail
    # with STACK-LIMIT-EXCEEDED as without it
    reset();
    {
        N head();
        N n = head;
        for (int i = 1; i < 300; ++i) {
            n.next = new N();
            n = n.next;
        }
        remove n;
        released = head;
    }
    rv = run(sub () { r((0, new X()), 0, False); }, Big);
    printf("chain: %s %s %s\n", rv, counts("X"), counts("N"));

    # a sandboxed Program with a stack budget: its destructors do not get the slice, nor do those of the host's objects
    # that it releases
    foreach string what in (("sandbox", "host object in sandbox")) {
        SandboxManager sm();
        sm.setMaxStackSize(131072);
        Program p(PO_NO_CHILD_PO_RESTRICTIONS);
        p.setSandboxManager(sm);
        p.parse(SandboxCode, "sandboxed");
        reset();
        depth = 0;
        remove deep_err;
        rv = run(sub () {
            # the host's object is released by the sandboxed code at its stack budget
            p.callFunction("start", what == "sandbox" ? NOTHING : object sub () { return new D(); });
        }, Big);
        printf("%s: %s depth=%d err=%s%s\n", what, rv, what == "sandbox" ? p.callFunction("get_depth") : depth,
            what == "sandbox" ? p.callFunction("get_err") : deep_err, what == "sandbox" ? "" : " " + counts("D"));
    }
    # the host is not affected
    reset();
    depth = 0;
    remove deep_err;
    released = new D();
    rv = run(sub () { r((0, new X()), 0, False); }, Big);
    printf("host after: %s %s %s depth=%d err=%s\n", rv, counts("X"), counts("D"), depth, deep_err);
}
printf("DONE\n");
