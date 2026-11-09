%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# streaming operators over more than 100 elements in the on_exit block of a cancelled thread: they check for
# cancellation every 100 elements as the AST interpreter does (issue #5641)
string sub run(string kind) {
    Queue res(); Counter started(1); Counter block(1);
    list<int> li = range(300);
    list<string> ls = map $1.toString(), li;
    int tid = background sub () {
        on_exit {
            try {
                auto r;
                switch (kind) {
                    case "take": r = (take 250, li).size(); break;
                    case "take-50": r = (take 50, li).size(); break;
                    case "drop": r = (drop 50, li).size(); break;
                    case "drop-250": r = (drop 250, li).size(); break;
                    case "drop-all": r = (drop 300, li).size(); break;
                    case "takewhile": r = (takewhile $1 < 250, li).size(); break;
                    case "takeuntil": r = (takeuntil $1 >= 250, li).size(); break;
                    case "first": r = first $1 > 250, li; break;
                    case "any": r = any $1 > 250, li; break;
                    case "all": r = all $1 < 1000, li; break;
                    case "count": r = count $1 > 10, li; break;
                    case "count-50": r = count $1 > 10, (take 50, li); break;
                    case "fused-take": r = (take 250, (map $1 * 2, li)).size(); break;
                    case "fused-count": r = count $1 > 10, (map $1 * 2, li); break;
                    case "fused-first": r = first $1 > 500, (map $1 * 2, li); break;
                    case "inner-drop": r = (map $1 * 2, (drop 150, li)).size(); break;
                    case "inner-drop-50": r = (map $1 * 2, (drop 50, li)).size(); break;
                    case "foreach-drop": { int n = 0; foreach int i in (drop 150, li) { ++n; } r = n; } break;
                    case "fused-join": r = (foldl $1 + "," + $2, (select ls, $1.size() < 3)).size(); break;
                    case "fused-join-99":
                        r = (foldl $1 + "," + $2, (select ls, $1.size() < 3 && $1 != "5")).size();
                        break;
                    case "take-100": r = (take 100, li).size(); break;
                    case "take-99": r = (take 99, li).size(); break;
                    case "fused-join-all": r = (foldl $1 + "," + $2, (map $1, ls)).size(); break;
                }
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
