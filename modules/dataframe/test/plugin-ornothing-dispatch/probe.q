# Copyright 2026 Qore Technologies, s.r.o.

# the probe of dataframe-plugin-ornothing-dispatch.qtest (qore#5911): comparisons of operands of an or-nothing or a
# union type with a class that has a plugin operation; its output as a qcc executable must be that of the AST
# interpreter
%modern
%requires dataframe
sub show(string l, auto r) { printf("%s: %s\n", l, r.typeCode() == NT_OBJECT ? r.className() : sprintf("%y", r)); }
auto sub opn(*DataFrame::ColumnRef c, float x, string op) {
    switch (op) {
        case "==": return c == x; case "!=": return c != x; case "<": return c < x;
        case "<=": return c <= x; case ">": return c > x; case ">=": return c >= x;
    }
}
auto sub opu(union<DataFrame::ColumnRef, int> c, float x, string op) {
    switch (op) {
        case "==": return c == x; case "!=": return c != x; case "<": return c < x;
        case "<=": return c <= x; case ">": return c > x; case ">=": return c >= x;
    }
}
auto sub opa(auto c, auto x, string op) {
    switch (op) {
        case "==": return c == x; case "!=": return c != x; case "<": return c < x;
        case "<=": return c <= x; case ">": return c > x; case ">=": return c >= x;
    }
}
auto sub opt(*DataFrame::ColumnRef c, float x, string op) {
    try {
        switch (op) {
            case "==": return c == x; case "!=": return c != x; case "<": return c < x;
            case "<=": return c <= x; case ">": return c > x; case ">=": return c >= x;
        }
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
auto sub ifauto(auto c, auto x) { if (c > x) { return "true"; } return "false"; }
auto sub ifctx(union<DataFrame::ColumnRef, int> c, float x) { if (c > x) { return "true"; } return "false"; }
auto sub notctx(*DataFrame::ColumnRef c, float x) { return !(c < x); }
DataFrame::DataFrame df(({"a": 1}, {"a": 2}));
foreach string op in (("==", "!=", "<", "<=", ">", ">=")) {
    show("*ColumnRef " + op, opn(df.column("a"), 1.5, op));
    show("*ColumnRef NOTHING " + op, opn(NOTHING, 1.5, op));
    show("union ColumnRef " + op, opu(df.column("a"), 1.5, op));
    show("union int " + op, opu(2, 1.5, op));
    show("auto ColumnRef " + op, opa(df.column("a"), 1.5, op));
    show("auto int " + op, opa(2, 1.5, op));
    show("try *ColumnRef " + op, opt(df.column("a"), 1.5, op));
    show("try NOTHING " + op, opt(NOTHING, 1.5, op));
}
# closures: run as their IR read from the executable with QORE_DISABLE_AOT_NATIVE_CLOSURES set
code cn = auto sub (*DataFrame::ColumnRef c) { return c > 1.5; };
code ca = auto sub (auto c) { if (c > 1.5) { return "true"; } return "false"; };
show("closure *ColumnRef", cn(df.column("a")));
show("closure NOTHING", cn(NOTHING));
show("closure if auto ColumnRef", ca(df.column("a")));
show("closure if auto int", ca(1));
# a comparison with an exception handler is an invoke in the IR
code ct = auto sub (*DataFrame::ColumnRef c) { try { return c > 1.5; } catch (hash<ExceptionInfo> ex) { return ex.err; } };
show("closure try *ColumnRef", ct(df.column("a")));
show("closure try NOTHING", ct(NOTHING));
# a case with an operator compares without plugins, as the AST's CaseNodeWithOperator does
code cs = auto sub (auto c) { switch (c) { case > 5: return "gt"; case == 2: return "eq"; } return "no"; };
show("closure case ColumnRef", cs(df.column("a")));
show("closure case int", cs(7));
show("if auto ColumnRef", ifauto(df.column("a"), 1.5));
show("if auto int", ifauto(1, 1.5));
show("if union ColumnRef", ifctx(df.column("a"), 1.5));
show("if union int", ifctx(1, 1.5));
show("not *ColumnRef", notctx(df.column("a"), 1.5));
show("not NOTHING", notctx(NOTHING, 1.5));
