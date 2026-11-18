%modern
%disable-warning unreferenced-variable
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a cancellation that becomes pending while a pipeline evaluates its elements is raised where the AST interpreter
# raises it: at the next check of an operator after it counted an element, not at elements that a stage rejects, and
# in a foreach statement once per iteration of its body (issue #5641)

#! the number of elements that the predicate evaluated
our int n = 0;

#! makes a cancellation of the current thread pending: another thread cancels it while it waits with deferred
#! cancellation, so that the next cancellation point raises it
sub cancel_me() {
    Counter go(1);
    Counter cancelled(1);
    int tid = gettid();
    background sub () {
        go.waitForZero();
        cancel_thread(tid, "t");
        cancelled.dec();
    }();
    defer_thread_cancel(sub () {
        go.dec();
        cancelled.waitForZero();
    });
}

#! counts an element and makes the cancellation pending at element \a at; returns \a v
int sub pred(int v, int at) {
    if (++n == at) {
        cancel_me();
    }
    return v;
}

auto sub run_case(string kind, list<int> li, list<string> ls) {
    switch (kind) {
        # elements below 100 and above 150 pass the select; the cancellation becomes pending at element 120
        case "count": return count $1 >= 0, (select li, (pred($1, 120) < 100 || $1 > 150));
        case "take": return (take 250, (select li, (pred($1, 120) < 100 || $1 > 150))).size();
        case "drop": return (drop 10, (select li, (pred($1, 120) < 100 || $1 > 150))).size();
        case "inner-drop": return (map $1 * 2, (drop 150, (map pred($1, 20), li))).size();
        case "join": return (foldl $1 + "," + $2, (select ls, (pred($1.toInt(), 120) < 100 || $1.toInt() > 150)))
            .size();
        case "join-take": return (foldl $1 + "," + $2, (take 250, (select ls, (pred($1.toInt(), 120) < 100
            || $1.toInt() > 150)))).size();
        # the select passes only elements above 200; the cancellation becomes pending at element 50
        case "foreach-select": {
            int c = 0;
            foreach int i in (select li, pred($1, 50) > 200) {
                ++c;
            }
            return c;
        }
        case "foreach-map-select": {
            int c = 0;
            foreach int i in (select (map pred($1, 50), li), $1 > 200) {
                ++c;
            }
            return c;
        }
        # the drop skips 150 elements; the cancellation becomes pending at element 20
        case "foreach-drop": {
            int c = 0;
            foreach int i in (drop 150, (map pred($1, 20), li)) {
                ++c;
            }
            return c;
        }
    }
}

string sub run(string kind) {
    Queue res();
    list<int> li = range(300);
    list<string> ls = map $1.toString(), li;
    background sub () {
        n = 0;
        try {
            res.push(sprintf("ok %y n=%d", run_case(kind, li, ls), n));
        } catch (hash<ExceptionInfo> ex) {
            # the cancellation stays pending; the rest of the thread must not be cancelled
            clear_thread_cancel();
            *list<*string> l = ex.desc =~ x/^([a-z -]+?(?: loop| join| operator expression))/;
            res.push(sprintf("%s: %s n=%d", ex.err, l ? l[0] : ex.desc, n));
        }
    }();
    return res.get(60s);
}
map printf("%s -> %s\n", $1, run($1)), ("count", "take", "drop", "inner-drop", "join", "join-take", "foreach-select",
    "foreach-map-select", "foreach-drop");
