%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# expressions over more than 100 elements in the on_exit block of a cancelled thread: none of them is a cancellation
# point in the AST interpreter except the string join of foldl (issue #5641)

#! evaluates a case in a function of its own, which JIT and tiered execution compile before it runs
auto sub eval_case(string kind, list<hash<auto>> lh, list<int> li, list<string> ls100, list<string> ls99) {
    switch (kind) {
        case "map-key": return (map $1.k, lh).size();
        case "map-key-offset": return (map $1.k + 1, lh).size();
        case "hmap-two-keys": return (map {$1.k: $1.j}, lh).size();
        case "select-key": return (select lh, $1.k > 0).size();
        case "foldl-map-sum": return foldl $1 + $2, (map $1 * 2, li);
        case "foldl-map-sq": return foldl $1 + $2, (map $1 * $1, li);
        case "foldl-map-offset": return foldl $1 + $2, (map $1 + 3, li);
        case "map-mul": return (map $1 * 2, li).size();
        case "list-lit": return ((map $1, li)).size();
        case "hmap": return (map {$1.toString(): $1}, li).size();
        case "map-select": return (map $1 * 2, li, $1 > 10).size();
        case "foldr": return foldr $1 + $2, li;
        case "slice-remove": { list<int> l = li; return (remove l[0..250]).size(); }
        case "join": return (foldl $1 + "," + $2, (map $1.toString(), li)).size();
        case "join-100": return (foldl $1 + "," + $2, ls100).size();
        case "join-99": return (foldl $1 + "," + $2, ls99).size();
        case "join-take": return (foldl $1 + "," + $2, (take 200, (map $1.toString(), li))).size();
        case "join-drop": return (foldl $1 + "," + $2, (drop 50, (map $1.toString(), li))).size();
        case "join-direct": {
            list<string> ls = map $1.toString(), li;
            return (foldl $1 + "," + $2, ls).size();
        }
    }
}

string sub run(string kind) {
    Queue res(); Counter started(1); Counter block(1);
    list<hash<auto>> lh = map (hash<auto>{"k": $1, "j": $1}), xrange(300);
    list<int> li = range(300);
    # lists of exactly 100 and 99 strings, made before the thread is cancelled
    list<string> ls100 = map $1.toString(), range(100);
    list<string> ls99 = map $1.toString(), range(99);
    int tid = background sub () {
        on_exit {
            try {
                auto r = eval_case(kind, lh, li, ls100, ls99);
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
    "join-direct", "join-100", "join-99", "join-take", "join-drop");
