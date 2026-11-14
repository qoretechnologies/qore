# probes of closure-expr-defaults.qtest (qore#5967): the default values of a closure's parameters that are
# expressions (also ones that modify a variable: +=, ++, push, a hash member assignment), constants and calls give
# their value and have their effect in a qcc executable; probes() returns one line per pass, so that the code runs
# on every tier with tiered execution

%modern

our int n = 1;
our list<int> l = (1, 2);
our string s = "ab";
our hash<auto> h = {};

code cl_assign = auto sub (auto q = (n += 5)) { return (q, n); };
code cl_expr = auto sub (auto q = (n + 1)) { return q; };
code cl_push = auto sub (auto q = (push l, 3)) { return (q, l); };
code cl_const = auto sub (int q = 5) { return q; };
const C = 7;
code cl_constref = auto sub (int q = C) { return q; };
code cl_call = auto sub (auto q = s.size()) { return q; };
code cl_inc = auto sub (auto q = n++) { return (q, n); };
code cl_hash = auto sub (auto q = (h.k = 3)) { return (q, h); };

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        n = 1;
        l = (1, 2);
        s = "ab";
        h = {};
        rv += sprintf("%d: %y %y %y %y %y %y %y %y\n", i, cl_assign(), cl_expr(), cl_push(), cl_const(),
            cl_constref(), cl_call(), cl_inc(), cl_hash());
    }
    return rv;
}
