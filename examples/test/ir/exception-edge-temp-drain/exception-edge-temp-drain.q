%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# an exception raised in a temp scope nested in a statement's (the per-element scope of a void map, the scope of an
# expression whose temporaries are released before the rest of its statement runs) goes to a handler outside the
# statement: the temporaries of every scope that the exception leaves are released before the handler runs, as the
# AST interpreter releases them; see exception-edge-temp-drain.qtest (qore#5689)

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
#! returns n - 1 objects: h1 .. h(n - 1)
list<Holder> sub holders(Log log, int n, string prefix = "h") { return map new Holder(log, prefix + $1), xrange(1, n); }
int sub size_of(list<Holder> l) { return l.size(); }

# a void map over a temporary list whose body raises
sub void_map(Log log) { try { map fail_on($1, "h2"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the same with a body that creates blocks and one that needs the implicit argument list
sub void_map_ternary(Log log) { try { map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub void_map_context(Log log) { try { map fail_on($1, $2 ? "x" : "h2"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the statement has other temporaries: the argument of a call that the map is an argument of
sub void_map_in_call(Log log) { try { size_of(holders(log, 2, "a") + (map fail_on($1, "h2"), holders(log, 3))); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# nested void maps
sub nested_void_map(Log log) { try { map (map fail_on($1, "h2"), holders(log, 3)), holders(log, 2, "o"); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the handler is that of a block that encloses a loop
sub in_foreach(Log log) { try { foreach Holder o in (holders(log, 2, "o")) { map fail_on($1, "h2"), holders(log, 3); log.add("each " + o.n); } } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# on_error and on_exit handlers of the block
sub on_error_inner(Log log) { on_error { log.add("on_error"); } on_exit { log.add("on_exit"); } map fail_on($1, "h2"), holders(log, 3); log.add("done"); }
sub on_error_handler(Log log) { try { on_error_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub on_error_in_try(Log log) { try { on_error { log.add("on_error"); } map fail_on($1, "h2"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# no handler in the function: the exception leaves it
sub no_handler_inner(Log log) { map fail_on($1, "h2"), holders(log, 3); log.add("done"); }
sub no_handler(Log log) { try { no_handler_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# a nested try: the inner handler rethrows
sub nested_rethrow(Log log) { try { try { map fail_on($1, "h2"), holders(log, 3); } catch (hash<ExceptionInfo> ex) { log.add("inner " + ex.err); rethrow; } } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the normal path is unchanged
sub normal(Log log) { try { map fail_on($1, "none"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }

const Cases = (
    "void_map", "void_map_ternary", "void_map_context", "void_map_in_call", "nested_void_map", "in_foreach",
    "on_error_handler", "on_error_in_try", "no_handler", "nested_rethrow", "normal",
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
