%modern
%allow-weak-references
# the temporaries of a statement that an exception interrupts at each of its checks, released by the shared releases
# of the native code (qore#7174): each case prints the destruction of each temporary and the exception, which must be
# those of the AST interpreter

string log;

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += " ~" + n; }
    int get(auto x) { return n.size() + x; }
}

#! returns an object of untyped value
auto sub mk(string n) { return new D(n); }

#! throws at check \a k of a statement
int sub chk(int i, int k) { if (i == k) { throw "AT", sprintf("check %d", i); } return i; }

#! a statement of untyped temporaries with five checks: the exception at check k releases the temporaries made
#! before it
auto sub untyped(int k) {
    auto r = 0;
    r += mk("a").get(chk(0, k)) + mk("b").get(chk(1, k)) + mk("c").get(chk(2, k)) + mk("d").get(chk(3, k))
        + mk("e").get(chk(4, k));
    return r;
}

#! the same with typed temporaries
int sub typed(int k) {
    int r = 0;
    D a = new D("ta");
    r += a.get(chk(0, k)) + (new D("tb")).get(chk(1, k)) + (new D("tc")).get(chk(2, k));
    return r;
}

#! the statement in a loop: the checks of each iteration release the temporaries of the iteration
auto sub inLoop(int k) {
    auto r = 0;
    for (int i = 0; i < 3; ++i) {
        r += mk("l" + i).get(chk(i * 2, k)) + mk("m" + i).get(chk(i * 2 + 1, k));
    }
    return r;
}

#! a statement with the iterator of a map operator: the exception releases the iterator and the temporaries
auto sub withMap(int k) {
    auto r = 0;
    r += mk("p").get(chk(0, k)) + (map mk("q" + $1).get(chk($1 + 1, k)), (0, 1, 2)).size()
        + mk("s").get(chk(4, k));
    return r;
}

#! the value of a return that the exception interrupts
auto sub inReturn(int k) {
    return mk("ra").get(chk(0, k)) + mk("rb").get(chk(1, k)) + mk("rc").get(chk(2, k));
}

class ThrowOnRelease {
    destructor() { throw "DTOR-T", "statement ended"; }
}

# The temporary is released before control enters the next try (#7412).
sub beforeTry(int k) {
    new ThrowOnRelease();
    try { log += " entered"; } catch (hash<ExceptionInfo> ex) { log += " caught too late"; }
}

sub weakStoreBeforeTry(int k) {
    hash<auto> h = {};
    h.w := new ThrowOnRelease();
    try { log += " entered"; } catch (hash<ExceptionInfo> ex) { log += " caught too late"; }
}

sub withinTry(int k) {
    try {
        new ThrowOnRelease();
        try { log += " entered"; } catch (hash<ExceptionInfo> ex) { log += " caught too late"; }
    } catch (hash<ExceptionInfo> ex) {
        log += " enclosing " + ex.err;
    }
}

sub run(string name, code c, int checks) {
    printf("%s:\n", name);
    for (int k = 0; k <= checks; ++k) {
        log = "";
        string rv;
        try {
            rv = sprintf("= %y", c(k));
        } catch (hash<ExceptionInfo> ex) {
            rv = sprintf("! %s: %s", ex.err, ex.desc);
        }
        printf("  %d:%s %s\n", k, log, rv);
    }
}

# each function is called more than once, so that the tiered and jit modes run its native code
for (int pass = 0; pass < 2; ++pass) {
    run("untyped", \untyped(), 5);
    run("typed", \typed(), 3);
    run("loop", \inLoop(), 6);
    run("map", \withMap(), 5);
    run("return", \inReturn(), 3);
    run("before try", \beforeTry(), 0);
    run("weak store before try", \weakStoreBeforeTry(), 0);
    run("within try", \withinTry(), 0);
}
