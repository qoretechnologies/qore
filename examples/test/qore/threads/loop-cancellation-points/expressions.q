%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# expressions over more than 100 elements in the on_exit block of a cancelled thread: none of them is a cancellation
# point in the AST interpreter except the string join of foldl (issue #5641)
string sub run(string kind) {
    Queue res(); Counter started(1); Counter block(1);
    list<hash<auto>> lh = map (hash<auto>{"k": $1, "j": $1}), xrange(300);
    list<int> li = range(300);
    int tid = background sub () {
        on_exit {
            try {
                auto r;
                switch (kind) {
                    case "map-key": r = (map $1.k, lh).size(); break;
                    case "map-key-offset": r = (map $1.k + 1, lh).size(); break;
                    case "hmap-two-keys": r = (map {$1.k: $1.j}, lh).size(); break;
                    case "select-key": r = (select lh, $1.k > 0).size(); break;
                    case "foldl-map-sum": r = foldl $1 + $2, (map $1 * 2, li); break;
                    case "foldl-map-sq": r = foldl $1 + $2, (map $1 * $1, li); break;
                    case "foldl-map-offset": r = foldl $1 + $2, (map $1 + 3, li); break;
                    case "map-mul": r = (map $1 * 2, li).size(); break;
                    case "list-lit": r = ((map $1, li)).size(); break;
                    case "hmap": r = (map {$1.toString(): $1}, li).size(); break;
                    case "map-select": r = (map $1 * 2, li, $1 > 10).size(); break;
                    case "foldr": r = foldr $1 + $2, li; break;
                    case "slice-remove": { list<int> l = li; r = (remove l[0..250]).size(); } break;
                    case "join": r = (foldl $1 + "," + $2, (map $1.toString(), li)).size(); break;
                    case "join-direct": {
                        list<string> ls = map $1.toString(), li;
                        r = (foldl $1 + "," + $2, ls).size();
                        break;
                    }
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
map printf("%s\n", run($1)), ("map-key", "map-key-offset", "hmap-two-keys", "select-key", "foldl-map-sum",
    "foldl-map-sq", "foldl-map-offset", "map-mul", "list-lit", "hmap", "map-select", "foldr", "slice-remove", "join",
    "join-direct");
