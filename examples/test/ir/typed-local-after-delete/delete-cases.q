%modern
# the cases of typed-local-after-delete.qtest: typed locals that delete or remove leave without a value, read in
# expressions, comparisons, list literals, calls and loops (qore#5710)

# Copyright (C) 2026 Qore Technologies, s.r.o.

#! takes any value
auto sub any(auto v) {
    return v;
}

#! takes an optional int
*int sub oint(*int v) {
    return v;
}

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s: %s\n", ex.err, ex.desc);
    }
}

sub main() {
    # the value of delete is NOTHING; the value of remove is the removed value
    t("delete value list", auto sub () { list<int> m = (0, 1, 2); auto x = (delete m, 2); return (x, m); });
    t("delete value int", auto sub () { int i = 5; auto x = (delete i, 1); return (x, i); });
    t("delete value string", auto sub () { string s = "ab"; auto x = (delete s); return (x, s); });
    t("remove value list", auto sub () { list<int> m = (0, 1, 2); auto x = (remove m, 2); return (x, m); });
    t("remove value hash", auto sub () { hash<auto> h = {"a": 1}; auto x = remove h; return (x, h); });
    t("delete value call", auto sub () { int i = 5; return any(delete i); });

    # comparisons of a deleted string
    t("string eq", auto sub () { string s = "ab"; delete s; return (s == "", s != "", s == "ab", "" == s); });
    t("string order", auto sub () { string s = "ab"; delete s; return (s < "a", s <= "", s > "", s >= "", s <=> "a"); });
    t("string remove eq", auto sub () { string s = "ab"; remove s; return (s == "", s != ""); });
    t("string concat", auto sub () { string s = "ab"; delete s; return s + "c"; });
    t("string exists", auto sub () { string s = "ab"; delete s; return (exists s, s ?? "none"); });

    # numbers
    t("int arith", auto sub () { int i = 5; delete i; return (i + 1, i * 2, i - 1, -i); });
    t("int compare", auto sub () { int i = 5; delete i; return (i == 0, i < 1, i > -1, i <=> 0); });
    t("int call", auto sub () { int i = 5; delete i; return oint(i); });
    t("float arith", auto sub () { float f = 1.5; delete f; return (f + 1.0, f == 0.0, f < 1.0); });
    t("bool logic", auto sub () { bool b = True; delete b; return (b == False, !b, b ? 1 : 2); });

    # containers
    t("list literal", auto sub () { list<int> m = (1, 2); delete m; return (m, 3); });
    t("list size", auto sub () { list<int> m = (1, 2); delete m; return (m.size(), m ?? ()); });
    t("list foreach", auto sub () {
        list<int> m = (1, 2);
        delete m;
        int n = 0;
        foreach int x in (m) {
            n += x;
        }
        return n;
    });
    t("hash eq", auto sub () { hash<auto> h = {"a": 1}; delete h; return (h == {}, h.size(), exists h); });
    t("binary eq", auto sub () { binary b = binary("ab"); delete b; return (b == binary(), b.size()); });

    # a delete later in a loop body leaves the variable without a value at a read earlier in the body
    t("loop read before delete", auto sub () {
        string s = "x";
        list<auto> rv = ();
        for (int i = 0; i < 3; ++i) {
            rv += (s == "", s + "!");
            delete s;
        }
        return rv;
    });
    t("loop int", auto sub () {
        int v = 1;
        list<auto> rv = ();
        for (int i = 0; i < 3; ++i) {
            rv += v + 10;
            remove v;
        }
        return rv;
    });

    # assigned again after the delete
    t("reassigned", auto sub () { string s = "ab"; delete s; s = "cd"; return (s == "cd", s + "e"); });
}

main();
