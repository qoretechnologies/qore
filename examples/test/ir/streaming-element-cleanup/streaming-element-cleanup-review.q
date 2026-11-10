%modern
# the review cases of qore#5670: temporaries of predicates and map expressions on the normal path, a fused map stage
# that changes the element, fused and typed-list loops that raise, an iterator's next() that raises, destructors that
# raise, a negative take/drop count, and code with no handler in the function; see streaming-element-cleanup.qtest

class Log {
    public {
        list<string> lines;
        hash<string, int> created;
        hash<string, int> released;
    }
    add(string line) { lines += line; }
}

#! an object whose destructor logs its release; one whose name starts with "x" raises an exception there
class Holder {
    public {
        Log log;
        string n;
        bool throw_in_dtor = False;
    }
    constructor(Log l, string nn) {
        log = l;
        n = nn;
        log.created{n} = (log.created{n} ?? 0) + 1;
        if (n =~ /^x/) {
            throw_in_dtor = True;
        }
    }
    destructor() {
        log.add("released " + n);
        log.released{n} = (log.released{n} ?? 0) + 1;
        if (throw_in_dtor) {
            throw "DTOR", n;
        }
    }
}

#! an iterator whose getValue() creates a new object; next() raises at the given position
class It inherits AbstractIterator {
    public {
        Log log;
        int i = 0;
        int n;
        string pfx;
        int fail_next = -1;
    }
    constructor(Log l, int nn, string p = "it", int fn = -1) {
        log = l; n = nn; pfx = p; fail_next = fn;
    }
    destructor() { log.add("itdone " + pfx); }
    bool next() {
        ++i;
        if (i == fail_next) {
            throw "NEXT", pfx + i;
        }
        return i <= n;
    }
    auto getValue() {
        return new Holder(log, pfx + i);
    }
    bool valid() { return i > 0 && i <= n; }
}

Holder sub fail_on(Holder h, string name) { if (h.n == name) { throw "STAGE", h.n; } return h; }
bool sub sel_fail(Holder h, string name) { if (h.n == name) { throw "SELECT", h.n; } return True; }
list<Holder> sub holders(Log log, int n, string p = "h") { return map new Holder(log, p + $1), xrange(1, n); }
sub nop(auto v) {}
string sub exstr(hash<ExceptionInfo> ex) {
    string s = ex.err + "/" + ex.desc;
    auto n = ex.next;
    while (n) {
        s += " next:" + n.err + "/" + n.desc;
        n = n.next;
    }
    return s;
}

sub fmap_take(Log log) { nop((take 1, (map $1.n, new It(log, 4)))); log.add("after"); }
sub fmap_any(Log log) { log.add("any " + (any $1 == "it2", (map $1.n, new It(log, 4)))); log.add("after"); }
sub fmap_takewhile(Log log) { nop((takewhile $1 != "it3", (map $1.n, new It(log, 4)))); log.add("after"); }
sub fmap_first(Log log) { log.add("first " + (first $1 == "it2", (map $1.n, new It(log, 4)))); log.add("after"); }
sub fmap_raises(Log log) { try { nop((takewhile sel_fail($1, "it3"), (map $1, (select new It(log, 4), True)))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub fmap_all_raises_nh_inner(Log log) { nop((all sel_fail($1, "it2"), (map $1, new It(log, 4)))); }
sub fmap_all_raises_nh(Log log) { try { fmap_all_raises_nh_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub temp_pred_select(Log log) { list r = select new It(log, 3), (new Holder(log, "t" + $#)).n != "t1"; log.add("r " + r.size()); remove r; log.add("after"); }
sub temp_pred_takewhile(Log log) { list r = (takewhile (new Holder(log, "t" + $#)).n != "t1", new It(log, 3)); log.add("r " + r.size()); remove r; log.add("after"); }
sub temp_pred_count(Log log) { int c = (count (new Holder(log, "t" + $#)).n != "t1", new It(log, 3)); log.add("c " + c); log.add("after"); }
sub temp_pred_mapsel(Log log) { list r = map (new Holder(log, "m" + $#)).n, new It(log, 3), (new Holder(log, "t" + $#)).n != "t1"; log.add("r " + r.size()); log.add("after"); }
sub mapsel_stmt(Log log) { map log.add("v " + $1.n), new It(log, 3), $1.n != "it2"; log.add("after"); }
sub mapsel_stmt_raises(Log log) { try { map log.add("v " + fail_on($1, "it2").n), new It(log, 3), True; } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub nested_normal(Log log) { int c = (count (any $1.n == "in2", new It(log, 3, "in")), new It(log, 2, "out")); log.add("c " + c); log.add("after"); }
sub nested_nohandler_inner(Log log) {
    int c = (count (any sel_fail($1, "in2"), new It(log, 3, "in")), new It(log, 2, "out"));
    log.add("c " + c);
}
sub nested_nohandler(Log log) { try { nested_nohandler_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub next_raises(Log log) { try { list r = (takewhile True, new It(log, 4, "n", 3)); log.add("r " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub next_raises_count(Log log) { try { int r = (count True, new It(log, 4, "n", 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub next_raises_select(Log log) { try { list r = select new It(log, 4, "n", 3), True; log.add("r " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub next_raises_nh_inner(Log log) { list r = (takewhile True, new It(log, 4, "n", 3)); log.add("r " + r.size()); }
sub next_raises_nh(Log log) { try { next_raises_nh_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub dtor_raises_nh_inner(Log log) { list r = (takewhile sel_fail($1, "x2"), new It(log, 3, "x")); log.add("r " + r.size()); }
sub dtor_raises_nh(Log log) { try { dtor_raises_nh_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub dtor_raises_normal_any(Log log) { try { bool b = (any $1.n == "x2", new It(log, 3, "x")); log.add("b " + b); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub dtor_raises_normal_take(Log log) { try { auto b = (take 1, new It(log, 3, "x")); log.add("b " + b.size()); remove b; } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub fused_stage_raises(Log log) { try { int r = (count True, (map fail_on($1, "it2"), new It(log, 3))); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub fused_stage_raises_nh_inner(Log log) { int r = (count True, (map fail_on($1, "it2"), new It(log, 3))); log.add("r " + r); }
sub fused_stage_raises_nh(Log log) { try { fused_stage_raises_nh_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub fused_two_stage(Log log) { try { list r = (takewhile sel_fail($1, "it3"), (select (map $1, new It(log, 4)), True)); log.add("r " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub on_exit_inner(Log log) { on_exit { log.add("on_exit"); } int c = (count sel_fail($1, "it2"), new It(log, 3)); log.add("c " + c); }
sub on_exit_case(Log log) { try { on_exit_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub take_neg(Log log) { try { nop((take -1, new It(log, 2))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub drop_neg_nh_inner(Log log) { nop((drop -1, new It(log, 2))); }
sub drop_neg_nh(Log log) { try { drop_neg_nh_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub tl_map_new_raises(Log log) { try { list<Holder> r = map sel_fail($1, "h2") ? new Holder(log, "n" + $#) : NOTHING, holders(log, 3); log.add("r " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
sub tl_mapsel_raises(Log log) { try { list r = map new Holder(log, "n" + $#), holders(log, 3), sel_fail($1, "h2"); log.add("r " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + exstr(ex)); } log.add("after"); }
# predicates with temporaries on the normal path, also first, any and all
sub temp_pred_first(Log log) { auto x = (first (new Holder(log, "t" + $#)).n == "t1", new It(log, 3)); log.add("first " + x.n); remove x; log.add("after"); }
sub temp_pred_any(Log log) { bool b = (any (new Holder(log, "t" + $#)).n == "t1", new It(log, 3)); log.add("any " + b); log.add("after"); }
sub temp_pred_all(Log log) { bool b = (all (new Holder(log, "t" + $#)).n != "t1", new It(log, 3)); log.add("all " + b); log.add("after"); }
# a map-select's map expression with a temporary on the normal path
sub temp_map_expr_mapsel(Log log) { list r = map (new Holder(log, "m" + $#)).n, new It(log, 3), $1.n != "it2"; log.add("r " + r.size()); log.add("after"); }

const Cases = (
    "temp_pred_select", "temp_pred_takewhile", "temp_pred_count", "temp_pred_mapsel", "temp_pred_first",
    "temp_pred_any", "temp_pred_all", "temp_map_expr_mapsel", "nested_normal", "mapsel_stmt", "mapsel_stmt_raises",
    "fmap_take", "fmap_any", "fmap_takewhile", "fmap_first", "fmap_raises", "fmap_all_raises_nh", "fused_two_stage",
    "fused_stage_raises", "fused_stage_raises_nh", "tl_map_new_raises", "tl_mapsel_raises", "next_raises",
    "next_raises_count", "next_raises_select", "next_raises_nh", "dtor_raises_normal_any",
    "dtor_raises_normal_take", "dtor_raises_nh", "nested_nohandler", "on_exit_case", "take_neg", "drop_neg_nh",
);

#! runs a case and returns its line: the log, and every object that was not released exactly once when it returned
string sub run(string name) {
    Log log();
    try {
        call_function(name, log);
    } catch (hash<ExceptionInfo> ex) {
        log.add("ESCAPED " + exstr(ex));
    }
    list<string> errors;
    foreach string n in (keys log.created) {
        int released = log.released{n} ?? 0;
        if (released != log.created{n}) {
            errors += sprintf("%s created %d released %d", n, log.created{n}, released);
        }
    }
    string rv = sprintf("%s: %s", name, log.lines.join("; "));
    if (errors) {
        rv += " ERROR: " + errors.join(", ");
    }
    return rv;
}

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        foreach string name in (ARGV ?: Cases) {
            printf("%s\n", run(name));
        }
    }
}

main();
