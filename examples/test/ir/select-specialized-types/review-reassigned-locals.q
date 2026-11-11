%modern
# cases from the review of qore#5654 (operand types of the specialized map opcodes, zero constants of select
# predicates, the exception edge of the map opcodes, gating); see select-specialized-types.qtest
string sub show(auto l) { return sprintf("%s %y", l.fullType(), l); }
sub tr(string n, code c) { try { printf("%s: %s\n", n, show(c())); } catch (hash<ExceptionInfo> ex) { printf("%s: EXC %s: %s\n", n, ex.err, ex.desc.substr(0, 60)); } }
sub setl(reference r) { r = ("x", 0.5, "-1"); }

auto sub loop_reassign() {
    auto l = (1, 2);
    list<auto> res = ();
    for (int i = 0; i < 2; ++i) {
        res += select l, $1 > 0;
        l = ("x", 0.5, "3");
    }
    return res;
}
auto sub branch_reassign(bool b) {
    auto l = (1, -2);
    if (b) {
        l = ("x", 0.5, "3");
    }
    return select l, $1 != 0;
}
auto sub ref_reassign() {
    auto l = (1, -2);
    setl(\l);
    return select l, $1 > 0;
}
auto sub closure_reassign() {
    auto l = (1, -2);
    code c = sub () { l = ("x", 0.5, "3"); };
    c();
    return select l, $1 > 0;
}
auto sub map_loop_reassign() {
    auto l = (1, 2);
    list<auto> res = ();
    for (int i = 0; i < 2; ++i) {
        res += map $1 + 1, l;
        l = ("x", 0.5);
    }
    return res;
}
auto sub map_closure_reassign() {
    auto l = (1, 2);
    code c = sub () { l = ("x", 0.5); };
    c();
    return map $1 * 2, l;
}
auto sub fused_loop_reassign() {
    auto l = (1, 2);
    list<auto> res = ();
    for (int i = 0; i < 2; ++i) {
        res += map $1 * 2, select l, $1 > 0;
        l = ("3", 0.5);
    }
    return res;
}

for (int i = 0; i < 3; ++i) {
    tr("loop_reassign", \loop_reassign());
    tr("branch_reassign", auto sub () { return branch_reassign(True); });
    tr("ref_reassign", \ref_reassign());
    tr("closure_reassign", \closure_reassign());
    tr("map_loop_reassign", \map_loop_reassign());
    tr("map_closure_reassign", \map_closure_reassign());
    tr("fused_loop_reassign", \fused_loop_reassign());
}
