%modern
# calls whose argument has no value at run time although its type does not include it (a member that is not assigned); see
# compiled-overload-errors.qtest
class O { public { hash<string, bool> consumes; list<string> names; string s; *int n; } }
string sub take(string x) { return "take(string)"; }
string sub ov(string x) { return "ov(string)"; }
string sub ov(*int x) { return sprintf("ov(*int) x=%y", x); }
string sub kv(string x) { return "kv(string)"; }
string sub kv(int x) { return "kv(int)"; }
string sub nm(string x, int y = 1) { return "nm(string, int)"; }
string sub nm(*int x, int y = 1) { return sprintf("nm(*int, int) x=%y", x); }
# the fast call paths bind the argument without a value to the committed variant: a method, a static method and a
# method called on an object of a derived class
class M {
    string meth(string x) { return "M::meth(string)"; }
    static string smeth(string x) { return "M::smeth(string)"; }
    final string fmeth(string x) { return "M::fmeth(string)"; }
    # a parameter written in the body: the arguments are bound before the call, not in the callee
    final string wmeth(string x) { x += "!"; return "M::wmeth(string)"; }
    string selfCall(O o) { return fmeth(o.s); }
    string selfCallWritten(O o) { return wmeth(o.s); }
}
class N inherits M {
}
sub run(string label, code<string()> f) {
    try { printf("%s: %s\n", label, f()); } catch (hash<ExceptionInfo> ex) { printf("%s: %s: %s\n", label, ex.err, ex.desc); }
}
sub main() {
    O o();
    # more than once, so that the compiled tiers run the calls too
    for (int i = 0; i < 3; ++i) {
        mainCalls(o);
    }
}
sub mainCalls(O o) {
    # sort() of no value has regular variants, which return what its noop variants did; inlist() of no list matches
    # its softlist variant
    run("sort keys", string sub () { return sprintf("%y", sort(keys o.consumes)); });
    run("sort_descending keys", string sub () { return sprintf("%y", sort_descending(keys o.consumes)); });
    run("sort_stable keys", string sub () { return sprintf("%y", sort_stable(keys o.consumes)); });
    run("sort_descending_stable keys", string sub () { return sprintf("%y", sort_descending_stable(keys o.consumes)); });
    run("inlist", string sub () { return sprintf("%y", inlist("x", o.names)); });
    run("inlist_hard", string sub () { return sprintf("%y", inlist_hard("x", o.names)); });
    # another noop variant (chr(auto)) is not a candidate in %modern code: the call raises the error
    run("chr", string sub () { return chr(o.n); });
    # a single possible variant is committed; the argument without a value raises the error
    run("take", string sub () { return take(o.s); });
    # another variant accepts no value: the call is resolved at run time
    run("multi-candidate defers", string sub () { return ov(o.s); });
    run("multi-candidate typed value", string sub () { return ov("x"); });
    # no other variant accepts no value: the variant stays committed
    run("multi-candidate keeps commit", string sub () { return kv(o.s); });
    run("named", string sub () { return nm(x: o.s); });
    M m();
    N n();
    run("method", string sub () { return m.meth(o.s); });
    run("static method", string sub () { return M::smeth(o.s); });
    run("inherited method", string sub () { return n.meth(o.s); });
    run("method called on self", string sub () { return m.selfCall(o); });
    run("method called on self, written parameter", string sub () { return m.selfCallWritten(o); });
}
main();
