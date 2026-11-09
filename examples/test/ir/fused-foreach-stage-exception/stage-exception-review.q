%modern
# cases from the review of qore#5655: a map stage that creates objects from a source of ints, take/drop counts that
# raise or are negative, an iterator whose next() raises (fused and plain foreach), and streaming operators in an
# expression; see fused-foreach-stage-exception.qtest

class Holder {
    public { Queue log; string n; }
    constructor(Queue l, string nn) { log = l; n = nn; }
    destructor() { log.push("released " + n); }
}
Holder sub fail_on(Holder h, string name) { if (h.n == name) { throw "STAGE", h.n; } return h; }
bool sub sel_fail(Holder h, string name) { if (h.n == name) { throw "SELECT", h.n; } return True; }
list<Holder> sub holders(Queue log, int n) { return map new Holder(log, "h" + $1), xrange(1, n); }
string sub run(code c) {
    Queue log();
    background sub () { try { c(log); } catch (hash<ExceptionInfo> ex) { log.push("outer " + ex.err); } log.push("END"); }();
    list<string> l;
    while (True) { string v = log.get(60s); if (v == "END") { break; } l += v; }
    return l.join("; ");
}


class It inherits AbstractIterator {
    public { Queue log; int i = 0; int failat; }
    constructor(Queue l, int f) { log = l; failat = f; }
    bool next() { ++i; if (i == failat) { throw "NEXT", string(i); } return i <= 3; }
    auto getValue() { return new Holder(log, "it" + i); }
    bool valid() { return i > 0 && i <= 3; }
}
int sub neg(int v) { if (v < 0) { throw "LIMIT"; } return v; }

# int source, map creates objects, body throws
sub p1(Queue log) {
    list<int> l = (1, 2, 3);
    try {
        foreach Holder h in (map new Holder(log, "m" + $1), l) {
            log.push("iter " + h.n);
            if (h.n == "m2") { throw "BODY"; }
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# int source, map creates objects, a later stage throws
sub p2(Queue log) {
    list<int> l = (1, 2, 3);
    try {
        foreach Holder h in (select (map new Holder(log, "m" + $1), l), sel_fail($1, "m2")) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# object source, map creates new objects, a later stage throws: intermediate result
sub p3(Queue log) {
    try {
        foreach Holder h in (select (map new Holder(log, "x" + $1.n), holders(log, 3)), sel_fail($1, "xh2")) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# limit expression throws
sub p4(Queue log) {
    try {
        foreach Holder h in (map $1, (take neg(-1), holders(log, 3))) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# negative take
sub p5(Queue log) {
    int n = -1;
    try {
        foreach Holder h in (map $1, (take n, holders(log, 3))) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# iterator next throws
sub p6(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "none"), new It(log, 2)) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# nested fused loops, inner stage throws
sub p7(Queue log) {
    try {
        foreach Holder a in (map fail_on($1, "none"), holders(log, 3)) {
            list<Holder> inner = map new Holder(log, a.n + "i" + $1), xrange(1, 3);
            foreach Holder b in (map fail_on($1, "h2i2"), inner) {
                log.push("iter " + b.n);
            }
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# no handler in the function: on_error / on_exit, caller catches
sub p8_inner(Queue log) {
    on_exit { log.push("on_exit"); }
    on_error { log.push("on_error"); }
    foreach Holder h in (map fail_on($1, "h2"), holders(log, 3)) {
        log.push("iter " + h.n);
    }
}
sub p8(Queue log) {
    try { p8_inner(log); } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# on_error inside the loop body, stage throws later
sub p9(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h2"), holders(log, 3)) {
            on_exit { log.push("body on_exit " + h.n); }
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# rethrow from a catch in the body, then a stage
sub p10(Queue log) {
    try {
        try {
            foreach Holder h in (map fail_on($1, "h3"), holders(log, 3)) {
                try { throw "B"; } catch () { log.push("inner " + h.n); if (h.n == "h2") { rethrow; } }
            }
        } catch (hash<ExceptionInfo> ex) { log.push("catch1 " + ex.err); rethrow; }
    } catch (hash<ExceptionInfo> ex) { log.push("catch2 " + ex.err); }
    log.push("after");
}
# stage throws inside a try nested in the body of an outer fused loop: inner loop
sub p11(Queue log) {
    try {
        foreach Holder a in (map fail_on($1, "none"), holders(log, 3)) {
            try {
                foreach Holder b in (map fail_on($1, "q1"), (map new Holder(log, "q" + $1), xrange(1,2))) {
                    log.push("iter " + b.n);
                }
            } catch (hash<ExceptionInfo> ex) { log.push("icatch " + a.n); }
            if (a.n == "h2") { throw "OUT"; }
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# loop variable declared outside (not a new local)
sub p12(Queue log) {
    Holder h;
    try {
        foreach h in (map fail_on($1, "h2"), holders(log, 3)) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err + " h=" + h.n); }
    log.push("after");
}
# implicit-type source (auto)
sub p13(Queue log) {
    auto src = holders(log, 3);
    try {
        foreach auto h in (map fail_on($1, "h2"), src) {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    remove src;
    log.push("after");
}
# drop + select with the select throwing after a skipped element
sub p14(Queue log) {
    try {
        foreach Holder h in (select (drop 1, holders(log, 4)), sel_fail($1, "h3") && $1.n != "h2") {
            log.push("iter " + h.n);
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# break then exception after loop
sub p15(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h3"), holders(log, 3)) {
            log.push("iter " + h.n);
            if (h.n == "h2") { break; }
        }
        throw "AFTER";
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# return from body
Holder sub p16_inner(Queue log) {
    try {
        foreach Holder h in (map fail_on($1, "h3"), holders(log, 3)) {
            if (h.n == "h2") { return h; }
        }
    } catch () {}
}
sub p16(Queue log) { Holder x = p16_inner(log); log.push("got " + x.n); remove x; log.push("after"); }

# p2 with list<auto> source
sub a(Queue log) {
    list<auto> l = (1, 2, 3);
    try {
        foreach Holder h in (select (map new Holder(log, "m" + $1), l), sel_fail($1, "m2")) { log.push("iter " + h.n); }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# non-fused foreach over a throwing iterator
sub b(Queue log) {
    try {
        foreach Holder h in (new It(log, 2)) { log.push("iter " + h.n); }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# non-fused: take limit throws in a plain expression context (not foreach)
sub c(Queue log) {
    try {
        list<Holder> r = (take neg(-1), holders(log, 3));
        log.push("size " + r.size());
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# fused map pipeline in expression context, later stage throws, int source
sub d(Queue log) {
    list<int> l = (1, 2, 3);
    try {
        list<Holder> r = (select (map new Holder(log, "m" + $1), l), sel_fail($1, "m2"));
        log.push("size " + r.size());
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err); }
    log.push("after");
}
# fused foreach body raising, loop var holds map-created object from int source, with closure
sub e(Queue log) {
    list<int> l = (1, 2, 3);
    code cl;
    try {
        foreach Holder h in (map new Holder(log, "m" + $1), l) {
            cl = string sub () { return h.n; };
            if (h.n == "m2") { throw "BODY"; }
        }
    } catch (hash<ExceptionInfo> ex) { log.push("catch " + ex.err + " " + cl()); }
    remove cl;
    log.push("after");
}

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        foreach string f in (("p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "a", "b", "c", "d", "e")) {
            printf("%s: %s\n", f, run(sub (Queue q) { call_function(f, q); }));
        }
    }
}

main();
