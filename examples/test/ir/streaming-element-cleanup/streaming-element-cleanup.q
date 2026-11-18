%modern
# streaming operators in an expression (take, drop, takewhile, takeuntil, first, any, all, count) and select and
# map-select over an iterator, whose predicate, or the consumer of whose value, raises an exception, and the same
# operators on the normal path: the order in which the elements, the lists and the sources are released must be that
# of the AST interpreter; see streaming-element-cleanup.qtest (qore#5670)

#! the log of a case: the lines, and the objects created and released
class Log {
    public {
        list<string> lines;
        hash<string, int> created;
        hash<string, int> released;
    }

    add(string line) {
        lines += line;
    }
}

class Holder {
    public {
        Log log;
        string n;
    }

    constructor(Log l, string nn) {
        log = l;
        n = nn;
        log.created{n} = (log.created{n} ?? 0) + 1;
    }

    destructor() {
        log.add("released " + n);
        log.released{n} = (log.released{n} ?? 0) + 1;
    }
}

Holder sub fail_on(Holder h, string name) { if (h.n == name) { throw "STAGE", h.n; } return h; }
bool sub sel_fail(Holder h, string name) { if (h.n == name) { throw "SELECT", h.n; } return True; }
bool sub sel_tmp(Holder h, Holder tmp, string name) { if (h.n == name) { throw "SELECT", tmp.n; } return True; }
list<Holder> sub holders(Log log, int n) { return map new Holder(log, "h" + $1), xrange(1, n); }
auto sub aholders(Log log, int n) { return holders(log, n); }
sub boom(auto v) { throw "CONSUMER"; }
sub show(Log log, auto v) { log.add("show " + v.size()); }

#! an iterator whose getValue() creates a new object, so that only the current element is alive
class It inherits AbstractIterator {
    public {
        Log log;
        int i = 0;
        int n;
    }

    constructor(Log l, int nn) {
        log = l;
        n = nn;
    }

    bool next() {
        ++i;
        return i <= n;
    }

    auto getValue() {
        return new Holder(log, "it" + i);
    }

    bool valid() {
        return i > 0 && i <= n;
    }
}

# a predicate raises: list sources
sub takewhile_raises(Log log) { try { list<Holder> r = (takewhile sel_fail($1, "h3"), holders(log, 4)); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub takeuntil_raises(Log log) { try { list<Holder> r = (takeuntil !sel_fail($1, "h3"), holders(log, 4)); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub first_raises(Log log) { try { auto r = (first sel_fail($1, "h2") && False, holders(log, 3)); log.add("r " + exists r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub any_raises(Log log) { try { bool r = (any !sel_fail($1, "h2"), holders(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub all_raises(Log log) { try { bool r = (all sel_fail($1, "h2"), holders(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub count_raises(Log log) { try { int r = (count sel_fail($1, "h2"), holders(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub local_source_raises(Log log) { list<Holder> l = holders(log, 3); try { int r = (count sel_fail($1, "h2"), l); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } remove l; log.add("after"); }
# a predicate raises: iterator sources, where only the current element is alive
sub it_takewhile_raises(Log log) { try { list<Holder> r = (takewhile sel_fail($1, "it3"), new It(log, 4)); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_takeuntil_raises(Log log) { try { list<Holder> r = (takeuntil !sel_fail($1, "it3"), new It(log, 4)); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_first_raises(Log log) { try { auto r = (first sel_fail($1, "it2") && False, new It(log, 3)); log.add("r " + exists r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_any_raises(Log log) { try { bool r = (any !sel_fail($1, "it2"), new It(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_all_raises(Log log) { try { bool r = (all sel_fail($1, "it2"), new It(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_count_raises(Log log) { try { int r = (count sel_fail($1, "it2"), new It(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# a predicate that creates a temporary raises
sub it_count_temp_raises(Log log) { try { int r = (count sel_tmp($1, new Holder(log, "t" + $#), "it2"), new It(log, 3)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err + " " + ex.desc); } log.add("after"); }
# select and map-select predicates in an expression
sub select_raises(Log log) { try { list<Holder> r = select holders(log, 3), sel_fail($1, "h2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_select_raises(Log log) { try { list<Holder> r = select new It(log, 3), sel_fail($1, "it2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_map_raises(Log log) { try { list<Holder> r = map fail_on($1, "it2"), new It(log, 3); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_map_select_raises(Log log) { try { list<Holder> r = map fail_on($1, "it2"), new It(log, 3), True; log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub it_map_select_pred_raises(Log log) { try { list<Holder> r = map $1, new It(log, 3), sel_fail($1, "it2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# sources of no declared type, iterated without a typed fast path
sub auto_select_raises(Log log) { try { auto r = select aholders(log, 4), sel_fail($1, "h2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub auto_map_raises(Log log) { try { auto r = map fail_on($1, "h2"), aholders(log, 4); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub auto_map_select_raises(Log log) { try { auto r = map fail_on($1, "h2"), aholders(log, 4), True; log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub auto_count_raises(Log log) { try { auto r = (count sel_fail($1, "h2"), aholders(log, 4)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the implicit arguments of the predicate are those of the operator: $2 is NOTHING, and the function's are restored
sub implicit_args_inner(Log log, ...) { log.add(sprintf("count %d", (count !exists $2, new It(log, 2)))); try { int c = (count sel_fail($1, "it2"), new It(log, 3)); log.add("r " + c); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add(sprintf("argv %y", argv[1..])); }
sub implicit_args(Log log) { implicit_args_inner(log, "a", "b"); log.add("after"); }
# a streaming operator over a fused pipeline of stages
sub fused_takewhile_raises(Log log) { try { list<Holder> r = (takewhile sel_fail($1, "h3"), (map $1, holders(log, 4))); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub fused_count_raises(Log log) { try { int r = (count sel_fail($1, "it2"), (select new It(log, 3), True)); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub fused_it_takewhile_raises(Log log) { try { list<Holder> r = (takewhile sel_fail($1, "it3"), (map $1, new It(log, 4))); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the consumer of the operator's value raises
sub consumer_plain(Log log) { try { boom(holders(log, 3)); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_take(Log log) { try { boom((take 2, holders(log, 4))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_drop(Log log) { try { boom((drop 1, holders(log, 4))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_takewhile(Log log) { try { boom((takewhile $1.n != "h2", holders(log, 4))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_first(Log log) { try { boom((first True, holders(log, 3))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_any(Log log) { try { boom((any $1.n == "h1", holders(log, 3))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_fused_take(Log log) { try { boom((take 2, (map $1, holders(log, 4)))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub consumer_it_take(Log log) { try { boom((take 2, new It(log, 4))); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# no handler in the function: the exception leaves it
sub no_handler_inner(Log log) { list<Holder> r = (takewhile sel_fail($1, "it2"), new It(log, 3)); log.add("size " + r.size()); }
sub no_handler(Log log) { try { no_handler_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub no_handler_count_inner(Log log) { int r = (count sel_fail($1, "it2"), new It(log, 3)); log.add("r " + r); }
sub no_handler_count(Log log) { try { no_handler_count_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub on_error_inner(Log log) { on_error { log.add("on_error"); } list<Holder> r = (takewhile sel_fail($1, "it2"), new It(log, 3)); log.add("size " + r.size()); }
sub on_error_handler(Log log) { try { on_error_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the normal path: the operator releases its source, and the element it stopped at, when it returns
sub normal_take(Log log) { show(log, (take 1, holders(log, 4))); log.add("after"); }
sub normal_drop(Log log) { show(log, (drop 2, holders(log, 4))); log.add("after"); }
sub normal_first(Log log) { auto x = (first True, holders(log, 3)); log.add("first " + x.n); remove x; log.add("after"); }
sub normal_takewhile(Log log) { show(log, (takewhile $1.n != "h2", holders(log, 4))); log.add("after"); }
sub normal_it_takewhile(Log log) { show(log, (takewhile $1.n != "it2", new It(log, 3))); log.add("after"); }
sub normal_it_any(Log log) { log.add("any " + (any $1.n == "it2", new It(log, 3))); log.add("after"); }
sub normal_it_all(Log log) { log.add("all " + (all $1.n != "it2", new It(log, 3))); log.add("after"); }
sub normal_fused_take(Log log) { show(log, (take 1, (map $1, holders(log, 4)))); log.add("after"); }
sub normal_fused_it_any(Log log) { log.add("any " + (any $1.n == "it2", (map $1, new It(log, 3)))); log.add("after"); }
sub normal_map_of_take(Log log) { show(log, map $1, (take 1, holders(log, 4))); log.add("after"); }

const Cases = (
    "takewhile_raises", "takeuntil_raises", "first_raises", "any_raises", "all_raises", "count_raises",
    "local_source_raises", "it_takewhile_raises", "it_takeuntil_raises", "it_first_raises", "it_any_raises",
    "it_all_raises", "it_count_raises", "it_count_temp_raises", "select_raises", "it_select_raises", "it_map_raises",
    "it_map_select_raises", "it_map_select_pred_raises", "auto_select_raises", "auto_map_raises",
    "auto_map_select_raises", "auto_count_raises", "implicit_args", "fused_takewhile_raises", "fused_count_raises",
    "fused_it_takewhile_raises", "consumer_plain", "consumer_take", "consumer_drop", "consumer_takewhile",
    "consumer_first", "consumer_any", "consumer_fused_take", "consumer_it_take", "no_handler", "no_handler_count",
    "on_error_handler", "normal_take", "normal_drop", "normal_first", "normal_takewhile", "normal_it_takewhile",
    "normal_it_any", "normal_it_all", "normal_fused_take", "normal_fused_it_any", "normal_map_of_take",
);

#! runs a case and returns its line: the log, and every object that was not released exactly once when it returned
string sub run(string name) {
    Log log();
    call_function(name, log);
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
