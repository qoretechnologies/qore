%modern
# the cases of native-release-order.qtest: the order in which the elements of a list source and the temporaries of a
# statement are released when code in map, select, map-select or a hash map raises an exception, which native code
# (JIT, AOT) must give as the AST interpreter does (qore#5830)

# Copyright (C) 2026 Qore Technologies, s.r.o.

class Log {
    public {
        list<string> lines;
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
    }

    destructor() {
        log.add("released " + n);
    }
}

#! an iterator that logs its release
class It inherits AbstractIterator {
    public {
        Log log;
        int i = 0;
    }

    constructor(Log l) {
        log = l;
    }

    destructor() {
        log.add("itdone it");
    }

    bool next() {
        return ++i <= 3;
    }

    auto getValue() {
        return i;
    }

    bool valid() {
        return i > 0 && i <= 3;
    }
}

#! a temporary object that makes an iterator
class Fac {
    public {
        Log log;
        string n;
    }

    constructor(Log l, string nn) {
        log = l;
        n = nn;
    }

    destructor() {
        log.add("released " + n);
    }

    It getIt() {
        return new It(log);
    }
}

Holder sub fail_on(Holder h, string name) {
    if (h.n == name) {
        throw "STAGE", h.n;
    }
    return h;
}

list<Holder> sub holders(Log log, int n, string p = "h") {
    return map new Holder(log, p + $1), xrange(1, n);
}

int sub idiv(int i) {
    return i == 2 ? 0 : 1;
}

#! a value whose destructor raises for the first one
class T {
    public {
        Log log;
        int n;
    }

    constructor(Log l, int i) {
        log = l;
        n = i;
    }

    destructor() {
        log.add("dtor " + n);
        if (n == 1) {
            throw "DTOR", "t" + n;
        }
    }
}

T sub mk(Log log, int i) {
    log.add("mk " + i);
    return new T(log, i);
}

#! prints the log of a case
sub out(string name, Log log) {
    printf("%s: %s\n", name, log.lines.join("; "));
}

# code delegated to the AST interpreter (??, exists) raises in map, select, map-select and a hash map over a list
sub map_list() {
    Log log();
    try {
        list<auto> r = map exists (fail_on($1, "h2") ?? NOTHING), holders(log, 4);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("map", log);
}

sub select_list() {
    Log log();
    try {
        list<auto> r = select holders(log, 4), exists (fail_on($1, "h2") ?? NOTHING);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("select", log);
}

sub map_select_list() {
    Log log();
    try {
        list<auto> r = map $1.n, holders(log, 4), exists (fail_on($1, "h2") ?? NOTHING);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("map select", log);
}

sub hash_map_list() {
    Log log();
    try {
        hash<auto> r = map {$1.n: exists (fail_on($1, "h2") ?? NOTHING)}, holders(log, 4);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("hash map", log);
}

# a temporary of the expression of a map, select or hash map over a list of ints, in a block with a local
sub map_temp_local() {
    Log log();
    try {
        {
            Holder loc(log, "local");
            list<int> r = map (new Holder(log, "t" + $1)).n.size() + 10 / idiv($1), (1, 2, 3);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("map temp local", log);
}

sub select_temp_local() {
    Log log();
    try {
        {
            Holder loc(log, "local");
            list<int> r = select (1, 2, 3), (new Holder(log, "t" + $1)).n.size() + 10 / idiv($1) > 0;
        }
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("select temp local", log);
}

sub hash_map_temp_local() {
    Log log();
    try {
        {
            Holder loc(log, "local");
            hash<auto> r = map {string($1): (new Holder(log, "t" + $1)).n.size() + 10 / idiv($1)}, (1, 2, 3);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("hash map temp local", log);
}

# map as a statement over an iterator of a temporary object, in a block with a local
sub map_statement_iterator() {
    Log log();
    try {
        {
            Holder loc(log, "local");
            map log.add("v " + (10 / idiv($1))), (new Fac(log, "fac")).getIt();
        }
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("map statement iterator", log);
}

# no exception: the temporary of the last element is released when the map statement ends, before the next
# statement, also in a try block
sub map_statement_try() {
    Log log();
    try {
        map (new Holder(log, "n" + $1)).n.size(), (1, 2, 3);
        log.add("stmt");
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("map statement try", log);
}

# the destructor of the value of the previous element raises when the next call replaces it
sub map_dtor_raises() {
    Log log();
    try {
        list<auto> r = map mk(log, $1).n, (1, 2, 3);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("map dtor raises", log);
}

sub main() {
    map_list();
    select_list();
    map_select_list();
    hash_map_list();
    map_temp_local();
    select_temp_local();
    hash_map_temp_local();
    map_statement_iterator();
    map_statement_try();
    map_dtor_raises();
}

main();
