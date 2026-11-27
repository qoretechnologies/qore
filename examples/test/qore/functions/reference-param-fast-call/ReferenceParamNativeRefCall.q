%modern
# calls with reference parameters (qore#7271) from code that runs on the IR tier to functions that become native code
# while it runs (tiered execution with a JIT threshold above 1): run() is called once and stays on the IR tier, and
# the functions it calls are compiled after their first calls, so that its call sites take the fast call path of a
# native callee (QORE_IR_INLINE_NATIVE_REF_CALL); the output is the AST interpreter's
int sub set_ref(reference<int> r, int v) { r += v; return r; }
int sub thr(reference<int> r, int v) { r = v; if (v % 3 == 0) { throw "T", sprintf("%d", r); } return r; }
string sub hk(reference<hash<auto>> h, int v) { h{"k" + v} = v; return h.size().toString(); }
int sub two(reference<int> a, reference<int> b) { a += 1; b += 2; return a + b; }

sub run() {
    int x = 0;
    int acc = 0;
    hash<auto> h = {};
    hash<auto> hcopy;
    int e = 0;
    int ex_count = 0;
    for (int i = 0; i < 200; ++i) {
        acc += set_ref(\x, i);
        try {
            acc += thr(\e, i);
        } catch (hash<ExceptionInfo> ex) {
            ++ex_count;
            acc += ex.desc.toInt();
        }
        if (i == 100) {
            hcopy = h;
        }
        hk(\h, i % 50);
        acc += two(\x, \x);
        acc += set_ref(\h.k1, 1);
    }
    printf("x=%y acc=%y e=%y exc=%y h=%y hcopy=%y\n", x, acc, e, ex_count, h.size(), hcopy.size());
    printf("%y\n", h.k1);
}
run();
