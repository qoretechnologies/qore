%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# map and fused map stages whose body raises, reads the implicit arguments ($1, $2, $#) with and without operands
# that the AST interpreter evaluates at run time, holds closures and call references, or creates blocks (?:, a
# ternary, &&) that hold temporaries: the release order of the elements, the lists and the sources, and the values of
# the implicit arguments must be those of the AST interpreter; see map-implicit-context.qtest (qore#5680)

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
list<Holder> sub holders(Log log, int n) { return map new Holder(log, "h" + $1), xrange(1, n); }
auto sub aholders(Log log, int n) { return holders(log, n); }
auto sub avalues(...) { return argv; }
string sub fmt(auto v) { return sprintf("%y", v); }

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

#! an iterator over strings
class StrIt inherits AbstractIterator {
    public {
        int i = 0;
        list<string> l;
    }

    constructor(list<string> ll) {
        l = ll;
    }

    bool next() {
        ++i;
        return i <= l.size();
    }

    auto getValue() {
        return l[i - 1];
    }

    bool valid() {
        return i > 0 && i <= l.size();
    }
}

# the body has a delegated expression (a regular expression match) and raises: the element and the list built so
# far are released before the handler
sub it_map_raises(Log log) { try { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub list_map_raises(Log log) { try { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 3); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub auto_map_raises(Log log) { try { auto r = map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), aholders(log, 3); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub local_map_raises(Log log) { list<Holder> l = holders(log, 3); try { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), l; log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } remove l; log.add("after"); }
sub it_map_index_raises(Log log) { try { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "it" + ($# + 1)), new It(log, 3); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the value of the map is not used
sub it_map_void_raises(Log log) { try { map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub list_map_void_raises(Log log) { try { map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 3); log.add("done"); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# no handler in the function: the exception leaves it
sub no_handler_inner(Log log) { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3); log.add("size " + r.size()); }
sub no_handler(Log log) { try { no_handler_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub no_handler_list_inner(Log log) { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 3); log.add("size " + r.size()); }
sub no_handler_list(Log log) { try { no_handler_list_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub on_error_inner(Log log) { on_error { log.add("on_error"); } list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3); log.add("size " + r.size()); }
sub on_error_handler(Log log) { try { on_error_inner(log); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# nested maps whose inner body raises
sub nested_raises(Log log) { try { list r = map (map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 3)), (1, 2); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# fused map stages whose body raises
sub fused_take_raises(Log log) { try { list<Holder> r = (take 3, (map fail_on($1, ($1.n =~ /x/) ? "x" : "h2"), holders(log, 4))); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub fused_it_count_raises(Log log) { try { int r = (count True, (map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3))); log.add("r " + r); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub fused_foreach_raises(Log log) { try { foreach Holder h in (map fail_on($1, ($1.n =~ /x/) ? "x" : "it2"), new It(log, 3)) { log.add("each " + h.n); } } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
# the normal path
sub it_map_normal(Log log) { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "none"), new It(log, 2); log.add("size " + r.size()); remove r; log.add("after"); }
sub list_map_normal(Log log) { list<Holder> r = map fail_on($1, ($1.n =~ /x/) ? "x" : "none"), holders(log, 3); log.add("size " + r.size()); remove r; log.add("after"); }

# the normal path, where the body creates blocks (?:, ternary, &&) that hold a call's temporary: the temporaries
# are released when the statement ends, before the code after it runs
sub layout_list_ternary(Log log) { list r = map fail_on($1, ($1.n =~ /x/) ? "x" : "none"), holders(log, 3); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_list_elvis(Log log) { list r = map ((($1.n =~ /x/) ? $1 : NOTHING) ?: fail_on($1, "none")), holders(log, 3); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_list_and(Log log) { list r = map (($1.n =~ /h/) && fail_on($1, "none")), holders(log, 3); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_it_ternary(Log log) { list r = map fail_on($1, ($1.n =~ /x/) ? "x" : "none"), new It(log, 2); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_it_elvis(Log log) { list r = map ((($1.n =~ /x/) ? $1 : NOTHING) ?: fail_on($1, "none")), new It(log, 2); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_it_and(Log log) { list r = map (($1.n =~ /i/) && fail_on($1, "none")), new It(log, 2); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_fused_ternary(Log log) { list r = (take 2, (map fail_on($1, ($1.n =~ /x/) ? "x" : "none"), holders(log, 4))); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_fused_elvis(Log log) { list r = (take 2, (map ((($1.n =~ /x/) ? $1 : NOTHING) ?: fail_on($1, "none")), holders(log, 4))); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_fused_and(Log log) { list<Holder> src = holders(log, 4); list r = (take 2, (map (($1.n =~ /h/) && fail_on($1, "none")), src)); log.add("size " + r.size()); remove src; log.add("removed"); remove r; log.add("after"); }
# the same with a body that needs the implicit argument list ($2)
sub layout_ctx_list_ternary(Log log) { list r = map fail_on($1, $2 ? "x" : "none"), holders(log, 3); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_ctx_it_and(Log log) { list r = map (!$2 && fail_on($1, "none")), new It(log, 2); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_ctx_fused_elvis(Log log) { list r = (take 2, (map ($2 ?: fail_on($1, "none")), holders(log, 4))); log.add("size " + r.size()); remove r; log.add("after"); }
# a fused stage whose body creates blocks raises
sub fused_ternary_raises(Log log) { try { list<Holder> r = (take 3, (map fail_on($1, $2 ? "x" : "h2"), holders(log, 4))); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }

# select and map-select over a typed list whose predicate needs the implicit argument list and raises, or whose
# predicate creates blocks that hold a call's temporary
bool sub sel_fail(Holder h, string name) { if (h.n == name) { throw "SELECT", h.n; } return True; }
sub select_ctx_raises(Log log) { try { list<Holder> r = select holders(log, 3), sel_fail(new hash<string, Holder>({"h": $1}).h, "h2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub map_select_ctx_raises(Log log) { try { list<Holder> r = map $1, holders(log, 3), sel_fail(new hash<string, Holder>({"h": $1}).h, "h2"); log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub map_select_ctx_map_raises(Log log) { try { list<Holder> r = map fail_on(new hash<string, Holder>({"h": $1}).h, "h2"), holders(log, 3), True; log.add("size " + r.size()); } catch (hash<ExceptionInfo> ex) { log.add("catch " + ex.err); } log.add("after"); }
sub layout_select_and(Log log) { list r = select holders(log, 3), (($1.n =~ /h/) && fail_on($1, "none")); log.add("size " + r.size()); remove r; log.add("after"); }
sub layout_map_select_and(Log log) { list r = map $1.n, holders(log, 3), (($1.n =~ /h/) && fail_on($1, "none")); log.add("size " + r.size()); remove r; log.add("after"); }

# operands that the AST interpreter evaluates at run time (new hash<K, V>(...), new list<T>(...)) in map, select and
# map-select over typed and untyped lists and iterators, in a function that has arguments of its own
sub new_inner(Log log, ...) {
    list<int> tl = (1, 2);
    list ul = (1, 2);
    log.add("map tl $1 " + fmt(map (new hash<string, int>({"a": $1})).a, tl));
    log.add("map tl $# " + fmt(map (new hash<string, int>({"a": $#})).a, tl));
    log.add("map tl $2 " + fmt(map (new list<auto>(($2,)))[0], tl));
    log.add("map tl list " + fmt(map (new list<int>($1))[0], tl));
    log.add("map ul $1 " + fmt(map (new hash<string, int>({"a": $1})).a, ul));
    log.add("map it $1 " + fmt(map (new hash<string, int>({"a": $1})).a, ul.iterator()));
    log.add("select tl $1 " + fmt(select tl, (new hash<string, int>({"a": $1})).a == 2));
    log.add("select tl $# " + fmt(select tl, (new hash<string, int>({"a": $#})).a == 1));
    log.add("select tl $2 " + fmt(select tl, !exists (new list<auto>(($2,)))[0]));
    log.add("select tl list " + fmt(select tl, (new list<int>($1))[0] == 1));
    log.add("select ul $1 " + fmt(select ul, (new hash<string, int>({"a": $1})).a == 2));
    log.add("select it $1 " + fmt(select ul.iterator(), (new hash<string, int>({"a": $1})).a == 2));
    log.add("map-select tl pred " + fmt(map $1, tl, (new hash<string, int>({"a": $1})).a == 2));
    log.add("map-select tl map " + fmt(map (new hash<string, int>({"a": $#})).a, tl, True));
    log.add("map-select tl $2 " + fmt(map $1, tl, !exists (new list<auto>(($2,)))[0]));
    log.add("map-select tl list " + fmt(map (new list<int>($1))[0] + $#, tl, $1 > 0));
    log.add("map-select ul " + fmt(map (new hash<string, int>({"a": $1})).a, ul, (new hash<string, int>({"a": $1})).a > 1));
    log.add("map-select it " + fmt(map (new hash<string, int>({"a": $1})).a, ul.iterator(), (new hash<string, int>({"a": $#})).a == 0));
    log.add("fn " + fmt(($1, $2)));
}
sub new_operands(Log log) { new_inner(log, "a", "b"); log.add("after"); }

# chain receivers: the temporary that a call gives the body for a method or member access is released when the next
# element's is created, in the statement's temp scope, as the AST interpreter releases it, also when the body needs the
# implicit argument list (qore#5680 review)
class Mk {
    public {
        Log log;
        string n;
    }

    constructor(Log l, string nn) {
        log = l;
        n = nn;
        log.add("new " + n);
        log.created{n} = (log.created{n} ?? 0) + 1;
    }

    destructor() {
        log.add("del " + n);
        log.released{n} = (log.released{n} ?? 0) + 1;
    }
}
Mk sub mk(Log log, auto v) { return new Mk(log, "m" + v); }
class O {
    public {
        string n;
    }

    constructor(string nn) {
        n = nn;
    }
}

sub chain_inner(Log log, ...) {
    list ul = ("a", "b", "c");
    list<string> tl = ("a", "b", "c");
    list e = (1, 2);
    list<int> il = (1, 2, 3);
    log.add("plain " + fmt(map mk(log, $1).n, ul));
    log.add("ctx ul " + fmt(map mk(log, $1).n + ($2 ?? ""), ul));
    log.add("ctx tl " + fmt(map mk(log, $1).n + ($2 ?? ""), tl));
    log.add("ctx it " + fmt(map mk(log, $1).n + ($2 ?? ""), new StrIt(ul)));
    log.add("ctx newhash " + fmt(map mk(log, $1).n + (new hash<string, int>({"k": $#})).k, tl));
    log.add("extract " + fmt(map mk(log, $1).n + (extract e, 0, 0).size(), ul));
    log.add("const newhash " + fmt(map $1 + (new hash<string, int>({"k": 1})).k, il));
    log.add("fused " + fmt((take 3, (map mk(log, $1).n + ($2 ?? ""), tl))));
    log.add("select " + fmt(select ul, mk(log, $1).n != ($2 ?? "x")));
    log.add("select tl " + fmt(select tl, mk(log, $1).n != ($2 ?? "x")));
    log.add("map-select " + fmt(map $1, ul, mk(log, $1).n != ($2 ?? "x")));
    log.add("map-select tl " + fmt(map mk(log, $1).n + ($2 ?? ""), tl, True));
    # the arguments of a declaration's constructor are evaluated with the map's implicit arguments
    log.add("decl " + fmt(map (O o(sprintf("%y", $2))).n, ul));
    # a map over a single value gives it the implicit element 0 (qore#5690)
    log.add("single $# " + fmt(map (map ($#, $2), "s"), il));
    # map-select where only one operand needs the implicit argument list
    log.add("map-select one pred " + fmt(map new list<auto>(($1,)), il, $1 % 3 == 0));
    log.add("map-select one map " + fmt(map $1, il, !exists (new list<auto>(($2,)))[0][0]));
}
sub chain(Log log) { chain_inner(log, "A", "B"); log.add("after"); }

# the implicit arguments in map bodies, in a function that has arguments of its own: $1 and $# are those of the map,
# $2 is NOTHING, and the function's arguments are those of the function again after the map
sub args_inner(Log log, ...) {
    list<string> l = ("x1", "x2");
    list al = ("x1", "x2");
    auto au = avalues("x1", "x2");
    # lists of lists of no declared element type, so that NOTHING can be an element of the results
    auto nl = avalues(avalues(1, 2), avalues(3, 4));
    auto ns = avalues(avalues("a1", "b"), avalues("c", "d1"));
    # no delegated expression in the body
    log.add("list $1 " + fmt(map $1, l));
    log.add("list $2 " + fmt(map $2, al));
    log.add("list $# " + fmt(map $#, l));
    log.add("auto $2 " + fmt(map $2, au));
    log.add("it $2 " + fmt(map $2, new StrIt(l)));
    log.add("it $# " + fmt(map $#, new StrIt(l)));
    log.add("list all " + fmt(map ($1, $2, $#), al));
    log.add("it all " + fmt(map ($1, $2, $#), new StrIt(l)));
    # the value of the map is not used
    map log.add("void " + fmt(($1, $2, $#))), al;
    map log.add("void d " + fmt((($1 =~ /2/) ? ($1, $2, $#) : $1))), new StrIt(l);
    # a delegated expression in the body
    log.add("d list $2 " + fmt(map (($1 =~ /1/) ? $2 : $#), al));
    log.add("d list all " + fmt(map (($1 =~ /2/) ? ($1, $2, $#) : $1), al));
    log.add("d auto all " + fmt(map (($1 =~ /2/) ? ($1, $2, $#) : $1), au));
    log.add("d it all " + fmt(map (($1 =~ /2/) ? ($1, $2, $#) : $1), new StrIt(l)));
    log.add("d call " + fmt(map fmt(($1, $2, $#)), al));
    # closures and call references have implicit arguments of their own; the arguments of their calls are those of
    # the map
    log.add("closure call " + fmt(map (auto sub (...) { return argv; })($1, $#), al));
    log.add("closure own " + fmt(map (auto sub (...) { return ($1, $2); })("own1", "own2"), al));
    list cl = map (auto sub (...) { return ($1, $2); }), al;
    log.add("closure later " + fmt(cl[0]("p", "q")) + " " + fmt(cl.size()));
    log.add("closure d " + fmt(map (auto sub (...) { return argv; })((($1 =~ /2/) ? $2 : $1), $#), al));
    log.add("call ref " + fmt(map (\fmt())(($1, $2, $#)), al));
    log.add("method ref " + fmt(map (\log.add())("mref " + fmt(($1, $2, $#))), al));
    # operands that the AST interpreter evaluates at run time
    log.add("new hash " + fmt(map new hash<string, auto>({"k": $1, "i": $#, "two": $2}), al));
    log.add("new list " + fmt(map new list<auto>(($1, $#, $2)), al));
    log.add("it new hash " + fmt(map new hash<string, auto>({"k": $1, "i": $#, "two": $2}), new StrIt(l)));
    # nested maps: the inner map binds its own, and its source is evaluated with the outer element
    log.add("nested " + fmt(map (map ($1, $2, $#), $1), nl));
    log.add("nested d " + fmt(map (map (($1 =~ /1/) ? ($1, $2, $#) : $1), $1), ns));
    log.add("nested outer " + fmt(map ((map $1, $1) + ($2, $#)), nl));
    log.add("nested outer d " + fmt(map ((map (($1 =~ /1/) ? $1 : $#), $1) + ($2, $#)), ns));
    # fused map stages
    log.add("fused take " + fmt((take 2, (map ($1, $2, $#), al))));
    log.add("fused take d " + fmt((take 2, (map (($1 =~ /2/) ? ($1, $2, $#) : $1), al))));
    log.add("fused first " + fmt((first exists $1, (map $2, al))));
    log.add("fused count " + fmt((count !exists $1, (map $2, new StrIt(l)))));
    list fl;
    foreach auto v in (map ($1, $2, $#), al) {
        fl += v;
    }
    log.add("fused foreach " + fmt(fl));
    # the function's own arguments
    log.add("fn " + fmt(($1, $2, $3)));
}
sub implicit_args(Log log) { args_inner(log, "a", "b"); log.add("after"); }

const Cases = (
    "it_map_raises", "list_map_raises", "auto_map_raises", "local_map_raises", "it_map_index_raises",
    "it_map_void_raises", "list_map_void_raises", "no_handler", "no_handler_list", "on_error_handler", "nested_raises", "fused_take_raises",
    "fused_it_count_raises", "fused_foreach_raises", "fused_ternary_raises", "it_map_normal", "list_map_normal",
    "layout_list_ternary", "layout_list_elvis", "layout_list_and", "layout_it_ternary", "layout_it_elvis",
    "layout_it_and", "layout_fused_ternary", "layout_fused_elvis", "layout_fused_and", "layout_ctx_list_ternary",
    "layout_ctx_it_and", "layout_ctx_fused_elvis", "select_ctx_raises", "map_select_ctx_raises",
    "map_select_ctx_map_raises", "layout_select_and", "layout_map_select_and", "new_operands", "chain",
    "implicit_args",
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
