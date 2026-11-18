%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the implicit index ($#) belongs to the operator or foreach statement whose per-element code encloses it: an
# operator in called code gives its own elements their index, the caller's index is the same after the call, and an
# operator over a single value gives it the index 0; see implicit-element-scope.qtest (qore#5690); a $# outside that
# code is a parse error (qore#803, see ../implicit-index-outside)

auto sub idx_single() { return map $#, "s"; }
auto sub idx_nested() { return map $#, (7, 8); }
auto sub idx_nested_foreach() { list r; foreach int i in ((7, 8)) { r += $#; remove i; } return r; }

string sub fmt(auto v) { return sprintf("%y", v); }

sub cases(...) {
    list l = ("a", "b", "c");
    list out;
    # operators in the called code give their own elements their index
    out += "map single in fn " + fmt(map idx_single(), l);
    out += "map nested in fn " + fmt(map idx_nested(), l);
    out += "map foreach in fn " + fmt(map idx_nested_foreach(), l);
    out += "select nested in fn " + fmt(select l, idx_nested()[1] == 1);
    # the caller's index is the same after the call
    out += "after call " + fmt(map ($#, idx_nested(), $#, idx_nested_foreach(), $#), l);
    list fe;
    foreach string s in (l) {
        fe += (s, $#, idx_nested(), $#);
    }
    out += "foreach after call " + fmt(fe);
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
    # the source of a fold is evaluated with the enclosing index, and a map in its expression sets its own
    out += "fold source in map " + fmt(map (foldl $1 + $2, ($#, 10)), l);
    out += "map in fold " + fmt(foldl $1 + (map $#, (5, 6)).size(), (1, 2));
    out += "fn args " + fmt(argv);
    foreach string line in (out) {
        printf("%s\n", line);
    }
}

cases("x", "y");
