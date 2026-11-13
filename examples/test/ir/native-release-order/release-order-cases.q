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

#! an iterator that gives a new object for each element and whose third next() raises an exception
class NextRaisesIt inherits AbstractIterator {
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
        if (i == 2) {
            throw "NEXT";
        }
        return ++i <= 3;
    }

    auto getValue() {
        return new Holder(log, "i" + i);
    }

    bool valid() {
        return i > 0 && i <= 3;
    }
}

#! an iterator that gives a new object for each element
class HolderIt inherits AbstractIterator {
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
        return new Holder(log, "i" + i);
    }

    bool valid() {
        return i > 0 && i <= 3;
    }
}

Holder sub id(Holder h) {
    return h;
}

bool sub keep(Holder h) {
    return h.n != "i2";
}

bool sub keep_list(list<auto> l) {
    return l[0].n != "i3";
}

bool sub before_i3(Holder h) {
    return h.n != "i3";
}

string sub names(list<auto> l) {
    return (map $1.n, l).join(",");
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

int sub int_fail_on(int i, int n) {
    if (i == n) {
        throw "STAGE", i;
    }
    return i;
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

# a temporary of the expression of a map over an iterator, used as an expression, in a block with a local
sub map_iterator_temp_local() {
    Log log();
    try {
        {
            Holder loc(log, "local");
            list<int> r = map (new Holder(log, "t" + $1)).n.size() + 10 / idiv($1), new It(log);
        }
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    out("map iterator temp local", log);
}

# foldl and foldr whose code is delegated to the AST interpreter raise: $1 and $2 are restored before the handler, and
# the elements are released before it
sub foldl_raises() {
    Log log();
    try {
        auto r = foldl (exists (fail_on($2, "h2") ?? NOTHING)) ? $1 : $2, holders(log, 3);
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldl raises", log);
}

sub foldr_raises() {
    Log log();
    try {
        auto r = foldr (exists (fail_on($2, "h1") ?? NOTHING)) ? $1 : $2, holders(log, 3);
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldr raises", log);
}

# a hash map and a hash map-select over an iterator release the iterator before the handler
sub hash_map_iterator() {
    Log log();
    try {
        hash<auto> r = map {string($1): 10 / idiv($1)}, new It(log);
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("hash map iterator", log);
}

sub hash_map_select_iterator() {
    Log log();
    try {
        hash<auto> r = map {string($1): 10 / idiv($1)}, new It(log), $1 > 0;
        log.add("r " + r.size());
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("hash map select iterator", log);
}

# a fold whose list comes from a pipeline (a fused fold) restores $1 and $2 before the handler
sub foldl_pipeline_raises() {
    Log log();
    try {
        auto r = foldl (exists (fail_on($2, "h2") ?? NOTHING)) ? $1 : $2, (map $1, holders(log, 3));
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldl pipeline raises", log);
}

# a fold over a pipeline (a fused fold) releases its first element when the accumulator is replaced, as the other
# elements, and not when the statement ends (qore#5883)
sub foldl_pipeline() {
    Log log();
    {
        auto r = foldl $2, (map $1, new HolderIt(log));
        log.add("r " + r.n);
    }
    log.add("after");
    out("foldl pipeline", log);
}

sub foldl_pipeline_call() {
    Log log();
    {
        auto r = foldl $2, (map id($1), new HolderIt(log));
        log.add("r " + r.n);
    }
    log.add("after");
    out("foldl pipeline call", log);
}

# a select stage keeps the accumulator for the element that it skips: native code's accumulator borrowed the element,
# which the next element released (qore#5883)
sub foldl_pipeline_select() {
    Log log();
    {
        auto r = foldl $2, (select (map $1, new HolderIt(log)), keep($1));
        log.add("r " + r.n);
    }
    log.add("after");
    out("foldl pipeline select", log);
}

# the values of an operator in a map stage before a select stage, whose instruction replaces the value of the element
# before in its slot: native code's accumulator borrowed it (qore#5883)
sub foldl_pipeline_select_string() {
    Log log();
    auto r = foldl $1 + "," + $2, (select (map $1 + "a", ("p", "q", "x", "y")), $1 != "xa");
    log.add("r " + r);
    auto r2 = foldl $2, (select (map $1 + "a", ("p", "q", "x", "y")), $1 != "ya");
    log.add("r " + r2);
    out("foldl pipeline select string", log);
}

sub foldl_pipeline_select_list() {
    Log log();
    {
        auto r = foldl ($1[0].n == "i1" ? $1 : $2), (select (map ($1,), new HolderIt(log)), keep_list($1));
        log.add("r " + names(r));
    }
    log.add("after");
    out("foldl pipeline select list", log);
}

# the fold's expression is an operator
sub foldl_pipeline_select_concat() {
    Log log();
    {
        auto r = foldl $1 + $2, (select (map ($1,), new HolderIt(log)), keep_list($1));
        log.add("r " + names(r));
    }
    log.add("after");
    out("foldl pipeline select concat", log);
}

sub foldl_pipeline_concat() {
    Log log();
    {
        auto r = foldl $1 + $2, (map ($1,), new HolderIt(log));
        log.add("r " + names(r));
    }
    log.add("after");
    out("foldl pipeline concat", log);
}

# a takewhile stage stops the pipeline at an element that the fold does not get, which is released before the iterator
sub foldl_pipeline_takewhile() {
    Log log();
    {
        auto r = foldl $2, (takewhile before_i3($1), (map id($1), new HolderIt(log)));
        log.add("r " + r.n);
    }
    log.add("after");
    out("foldl pipeline takewhile", log);
}

# the same when the iterator's next() raises: the first element is released when the accumulator is replaced, and the
# iterator before the accumulator
sub foldl_pipeline_next_raises() {
    Log log();
    try {
        auto r = foldl (exists $1) ? $2 : $1, (map $1, new NextRaisesIt(log));
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldl pipeline next raises", log);
}

# nested folds whose code is delegated to the AST interpreter: the inner fold's restoration does not keep the outer
# fold's argument list
sub foldl_nested_raises() {
    Log log();
    try {
        auto r = foldl (exists (foldl (exists (fail_on($2, "h2") ?? NOTHING)) ? $1 : $2,
            (new Holder(log, "x"), $2)) ?? NOTHING) ? $1 : $2,
            (new Holder(log, "h1"), new Holder(log, "h2"), new Holder(log, "h3"));
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldl nested raises", log);
}

# a fold over an iterator whose next() raises: the first element is released when the accumulator is replaced, and
# the iterator before the accumulator
sub foldl_next_raises() {
    Log log();
    try {
        auto r = foldl (exists $1) ? $2 : $1, new NextRaisesIt(log);
        log.add("r " + r.n);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err + " $1 " + (exists $1 ? "set" : "unset"));
    }
    log.add("after");
    out("foldl next raises", log);
}

# a fold whose code is delegated to the AST interpreter raises in a map in a function with no handler: the exit restores
# the fold's and then the map's implicit arguments, which release the map's argument list
auto sub map_fold_no_handler(Log log) {
    return map ((foldl (exists (int_fail_on($2, 2) ?? NOTHING)) ? $1 : $2, (1, 2, 3)) + ($1.n ?? "")),
        holders(log, 2, "m");
}

sub map_fold_raises_no_handler() {
    Log log();
    try {
        map_fold_no_handler(log);
    } catch (hash<ExceptionInfo> ex) {
        log.add("catch " + ex.err);
    }
    log.add("after");
    out("map fold no handler", log);
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
    map_iterator_temp_local();
    foldl_raises();
    foldr_raises();
    hash_map_iterator();
    hash_map_select_iterator();
    foldl_pipeline_raises();
    foldl_pipeline();
    foldl_pipeline_call();
    foldl_pipeline_select();
    foldl_pipeline_select_string();
    foldl_pipeline_select_list();
    foldl_pipeline_select_concat();
    foldl_pipeline_concat();
    foldl_pipeline_takewhile();
    foldl_pipeline_next_raises();
    foldl_nested_raises();
    foldl_next_raises();
    map_fold_raises_no_handler();
}

main();
