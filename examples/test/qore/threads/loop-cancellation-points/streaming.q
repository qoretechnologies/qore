%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# streaming operators over more than 100 elements in the on_exit block of a cancelled thread: they check for
# cancellation every 100 elements as the AST interpreter does (issue #5641)
#! evaluates a case in a function of its own, which JIT and tiered execution compile before it runs
auto sub eval_case(string kind, list<int> li, list<string> ls) {
    switch (kind) {
        case "take": return (take 250, li).size();
        case "take-50": return (take 50, li).size();
        case "drop": return (drop 50, li).size();
        case "drop-250": return (drop 250, li).size();
        case "drop-all": return (drop 300, li).size();
        case "takewhile": return (takewhile $1 < 250, li).size();
        case "takeuntil": return (takeuntil $1 >= 250, li).size();
        case "first": return first $1 > 250, li;
        case "any": return any $1 > 250, li;
        case "all": return all $1 < 1000, li;
        case "count": return count $1 > 10, li;
        case "count-50": return count $1 > 10, (take 50, li);
        case "fused-take": return (take 250, (map $1 * 2, li)).size();
        case "fused-count": return count $1 > 10, (map $1 * 2, li);
        case "fused-first": return first $1 > 500, (map $1 * 2, li);
        case "inner-drop": return (map $1 * 2, (drop 150, li)).size();
        case "inner-drop-50": return (map $1 * 2, (drop 50, li)).size();
        case "foreach-drop": { int n = 0; foreach int i in (drop 150, li) { ++n; } return n; }
        case "fused-join": return (foldl $1 + "," + $2, (select ls, $1.size() < 3)).size();
        case "fused-join-99":
            return (foldl $1 + "," + $2, (select ls, $1.size() < 3 && $1 != "5")).size();
        case "take-100": return (take 100, li).size();
        case "take-99": return (take 99, li).size();
        case "fused-join-all": return (foldl $1 + "," + $2, (map $1, ls)).size();
    }
}

string sub run(string kind) {
    Queue res(); Counter started(1); Counter block(1);
    list<int> li = range(300);
    list<string> ls = map $1.toString(), li;
    int tid = background sub () {
        on_exit {
            try {
                auto r = eval_case(kind, li, ls);
                res.push(sprintf("ok %y", r));
            } catch (hash<ExceptionInfo> ex) { res.push(ex.err + ": " + ex.desc); }
        }
        started.dec();
        block.waitForZero();
    }();
    started.waitForZero();
    cancel_thread(tid, "t");
    string r = res.get(60s);
    thread_join(tid);
    r =~ s/thread [0-9]+ //;
    return kind + " -> " + r;
}
map printf("%s\n", run($1)), ("take", "take-50", "drop", "drop-250", "drop-all", "takewhile", "takeuntil", "first",
    "any", "all", "count", "count-50", "fused-take", "fused-count", "fused-first", "inner-drop", "inner-drop-50",
    "foreach-drop", "fused-join", "fused-join-99", "take-100", "take-99", "fused-join-all");
