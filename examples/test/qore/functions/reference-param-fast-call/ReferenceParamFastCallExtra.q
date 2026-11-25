%modern
# calls with reference parameters (qore#7271): re-entrant bindings, mismatch fallbacks after conversions, exception
# call stacks, temporaries given to a call with reference parameters and threads; the output is the AST
# interpreter's in every execution mode, on the IR tier and in programs compiled with qcc

our Mutex m();
our int shared = 0;

# a binding that does not match its parameter after another parameter's binding converted its lvalue
string sub nr(*reference<int> r, int v) { r = v; return sprintf("%y", r); }
string sub conv2(reference<softint> a, reference<int> b) { a += 1; b += 1; return sprintf("%y %y", a, b); }
string sub th(reference<hash<string, int>> h) { h.q = 1; return "ok"; }
auto sub idf(auto v) { return v; }

sub mismatch() {
    printf("nr %y\n", nr(NOTHING, 3));
    auto a = "5";
    auto b = idf("x");
    try {
        printf("conv2 %y\n", conv2(\a, \b));
    } catch (hash<ExceptionInfo> ex) {
        printf("conv2 ex %s: %s\n", ex.err, ex.desc);
    }
    printf("after %y %y\n", a, b);
    hash<auto> h = idf({"a": "s"});
    try {
        printf("th %y\n", th(\h));
    } catch (hash<ExceptionInfo> ex) {
        printf("th ex %s: %s\n", ex.err, ex.desc);
    }
    printf("h %y\n", h);
    auto c = 1;
    auto d = 2;
    printf("conv2b %y %y %y\n", conv2(\c, \d), c, d);
}

# re-entrant bindings: the function with a reference parameter is called again through another function while its
# parameter is bound
int sub f(reference<int> r, int n) { r += 1; int k = n > 0 ? h(n - 1) : 0; r += 10; return r * 1000 + k % 1000; }
int sub h(int n) { int v = n; int rv = f(\v, n); return rv + v; }
int sub fx(reference<int> r, int n) {
    r += 1;
    int k = n > 0 ? hx(n - 1) : 0;
    if (n == 0) {
        throw "DEEP", sprintf("r=%d", r);
    }
    r += 10;
    return r * 1000 + k % 1000;
}
int sub hx(int n) {
    int v = n;
    int rv;
    try {
        rv = fx(\v, n);
    } catch (hash<ExceptionInfo> ex) {
        if (n % 3) {
            rethrow;
        }
        rv = v * 7;
    }
    return rv + v;
}

sub reentrant() {
    int x = 0;
    printf("f %y x=%y\n", f(\x, 5), x);
    printf("h %y\n", h(40));
    for (int s = 0; s < 8; ++s) {
        int y = 0;
        try {
            printf("fx %y y=%y\n", fx(\y, s), y);
        } catch (hash<ExceptionInfo> ex) {
            printf("fx ex %s %s y=%y\n", ex.err, ex.desc, y);
        }
    }
}

# the call stack of an exception raised in a function with a reference parameter
int sub cf(reference<int> r, int n) {
    r += 1;
    if (n == 0) {
        throw "X", "y";
    }
    return cg(\r, n);
}
int sub cg(reference<int> r, int n) { r = n; throw "G", "g"; }
int sub ct(reference<int> r, int n) {
    r = n;
    return n / (n - n);
}

sub callstacks() {
    int x = 0;
    try {
        cf(\x, 0);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s %y\n", ex.err, map sprintf("%s:%d", $1.function, $1.line), ex.callstack);
    }
    try {
        ct(\x, 2);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s %y %y\n", ex.err, map sprintf("%s:%d", $1.function, $1.line), ex.callstack, ex.line);
    }
    try {
        cf(\x, 1);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s %y\n", ex.err, map sprintf("%s:%d", $1.function, $1.line), ex.callstack);
    }
    printf("x %y\n", x);
}

# temporaries given to a call with reference parameters
int sub cat(reference<string> r, string s) { r += s; return r.size(); }
int sub catl(reference<list<auto>> r, list<auto> s) { r += s; return r.size(); }

sub temporaries(int it) {
    string s = "";
    list<auto> l = ();
    for (int i = 0; i < 5; ++i) {
        printf("%y ", cat(\s, "x" + i + it));
        printf("%y ", catl(\l, (i, "y" + i)));
    }
    printf("\n%y %y\n", s, l);
}

# calls with reference parameters in threads, with a reference to a global that the threads share
int sub set_ref(reference<int> r, int v) { r += v; return r; }
int sub locked(reference<int> r) { m.lock(); on_exit m.unlock(); r += 1; return r; }
int sub thr(reference<int> r, int v) { r = v; if (v % 7 == 0) { throw "T", "x"; } return r; }

int sub work(int t) {
    int x = 0;
    int e = 0;
    for (int i = 0; i < 500; ++i) {
        set_ref(\x, i + t);
        locked(\shared);
        try {
            thr(\e, i);
        } catch () {
        }
    }
    return x + e;
}

sub threads() {
    Counter c();
    list<int> res = ();
    Mutex rm();
    for (int t = 0; t < 8; ++t) {
        # each thread gets the value of the loop variable when it is started
        int tn = t;
        c.inc();
        background sub () {
            on_exit c.dec();
            int r = work(tn);
            rm.lock();
            on_exit rm.unlock();
            res += r;
        }();
    }
    c.waitForZero();
    printf("threads %y %y\n", sort(res), shared);
}

for (int i = 0; i < 3; ++i) {
    printf("-- iteration %d\n", i);
    mismatch();
    reentrant();
    callstacks();
    temporaries(i);
}
threads();
