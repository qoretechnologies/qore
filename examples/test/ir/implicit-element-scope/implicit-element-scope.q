%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the implicit index ($#) belongs to the operator or foreach statement whose per-element code encloses it: in the
# code that it calls, in a fold, and at the top level it is unset, and an operator over a single value gives it the
# index 0; see implicit-element-scope.qtest (qore#5690)

auto sub idx() { return $#; }
auto sub idx_single() { return map $#, "s"; }
auto sub idx_nested() { return map $#, (7, 8); }
auto sub idx_nested_foreach() { list r; foreach int i in ((7, 8)) { r += ($#, idx()); remove i; } return r; }

class C {
    auto m() { return $#; }
    static auto sm() { return $#; }
}

string sub fmt(auto v) { return sprintf("%y", v); }

sub cases(...) {
    list l = ("a", "b", "c");
    C c();
    code cl = auto sub () { return $#; };
    code cr = \idx();
    code mr = \c.m();
    list out;
    # called code
    out += "map fn " + fmt(map idx(), l);
    out += "map method " + fmt(map c.m(), l);
    out += "map static " + fmt(map C::sm(), l);
    out += "map closure " + fmt(map cl(), l);
    out += "map closure in body " + fmt(map (auto sub () { return $#; })(), l);
    out += "map call ref " + fmt(map cr(), l);
    out += "map method ref " + fmt(map mr(), l);
    out += "select fn " + fmt(select l, !exists idx());
    out += "map-select fn " + fmt(map idx(), l, True);
    out += "map-select pred fn " + fmt(map $1, l, !exists idx());
    out += "hash map fn " + fmt(map {$1: idx()}, l);
    out += "hash map-select fn " + fmt(map {$1: idx()}, l, !exists idx());
    out += "first fn " + fmt((first !exists idx(), l));
    out += "any fn " + fmt((any exists idx(), l));
    out += "all fn " + fmt((all !exists idx(), l));
    out += "count fn " + fmt((count !exists idx(), l));
    out += "takewhile fn " + fmt((takewhile !exists idx(), l));
    out += "takeuntil fn " + fmt((takeuntil exists idx(), l));
    out += "fused fn " + fmt((take 2, (map idx(), l)));
    list fe;
    foreach string s in (l) {
        fe += (s, $#, idx());
    }
    out += "foreach fn " + fmt(fe);
    # operators in the called code give their own elements their index
    out += "map single in fn " + fmt(map idx_single(), l);
    out += "map nested in fn " + fmt(map idx_nested(), l);
    out += "map foreach in fn " + fmt(map idx_nested_foreach(), l);
    # the caller's index is the same after the call
    out += "after call " + fmt(map (idx(), $#, idx_nested(), $#), l);
    # the operator's own index, also over a single value
    out += "own " + fmt(map $#, l);
    out += "own select " + fmt(select l, $# != 1);
    out += "own map-select " + fmt(map $#, l, $# != 1);
    out += "own hash map " + fmt(map {$1: $#}, l);
    out += "own count " + fmt((count $# > 0, l));
    out += "single map " + fmt(map $#, "s");
    out += "single select " + fmt(select "s", $# == 0);
    out += "single map-select " + fmt(map $#, "s", $# == 0);
    out += "single hash map " + fmt(map {$1: $#}, "s");
    out += "single in map " + fmt(map (map $#, "s"), l);
    # no operator sets it
    out += "fold " + fmt(foldl $1 + ($# ?? "-"), l);
    out += "foldr " + fmt(foldr $1 + ($# ?? "-"), l);
    out += "function " + fmt($#);
    out += "fn args " + fmt(argv);
    foreach string line in (out) {
        printf("%s\n", line);
    }
}

printf("top %y\n", $#);
cases("x", "y");
