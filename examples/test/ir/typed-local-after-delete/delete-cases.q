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

#! takes a string, which NOTHING is not
string sub rs(string v) {
    return v;
}

#! deletes the string that a reference refers to
sub dels(reference<string> r) {
    delete r;
}

#! deletes the int that a reference refers to
sub deli(reference<int> r) {
    delete r;
}

#! an object whose destructor reads the global that holds it
class X {
    destructor() {
        printf("  dtor X: global exists %y\n", exists ::gx);
    }
}

our *X gx;
our int gi;

#! a typed local read in a loop before it is deleted, in a function that is not a closure
list<auto> sub plain_loop() {
    string s = "x";
    list<auto> rv = ();
    for (int i = 0; i < 3; ++i) {
        rv += (s == "", s + "!");
        delete s;
    }
    return rv;
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

    # a call with a parameter type that NOTHING is not, after a remove later in the loop
    t("typed param after remove", auto sub () {
        string s = "a";
        list<auto> rv = ();
        for (int j = 0; j < 2; ++j) {
            try {
                rv += rs(s);
            } catch (hash<ExceptionInfo> ex) {
                rv += "caught";
            }
            remove s;
        }
        return rv;
    });

    # a delete through a reference to the variable
    t("delete through reference", auto sub () {
        string s = "a";
        reference<string> r = \s;
        delete r;
        return (s == "", exists s, s + "!");
    });
    t("delete through reference call", auto sub () { string s = "a"; dels(\s); return (s == "", s + "!"); });
    t("switch after delete through reference", auto sub () {
        int i = 5;
        deli(\i);
        switch (i) {
            case 0: return "zero";
        }
        return "none";
    });

    # a global: the value of delete, a destructor that reads the global, and a system object
    t("delete global value", auto sub () { gi = 5; auto x = (delete gi, 1); return (x, gi); });
    t("delete global destructor", auto sub () { gx = new X(); delete gx; return exists gx; });
    t("delete system object", auto sub () { auto o = Qore::stdout; delete o; return "done"; });

    # a function that is not a closure
    t("plain function", \plain_loop());

    # the builtin functions and pseudo-methods with typed fast paths, read in a loop before a delete later in it
    t("loop join", auto sub () {
        list<string> l = ("a", "b");
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            try {
                rv += join(",", l);
            } catch (hash<ExceptionInfo> ex) {
                rv += ex.err;
            }
            delete l;
        }
        return rv;
    });
    t("loop join method", auto sub () {
        list<string> l = ("a", "b");
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            try {
                rv += l.join(",");
            } catch (hash<ExceptionInfo> ex) {
                rv += ex.err;
            }
            delete l;
        }
        return rv;
    });
    t("loop pseudo method", auto sub () {
        string s = "abc";
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            try {
                rv += s.substr(1);
            } catch (hash<ExceptionInfo> ex) {
                rv += ex.err;
            }
            delete s;
        }
        return rv;
    });
    t("loop length", auto sub () {
        string s = "abc";
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            rv += (length(s), strlen(s), substr(s, 1));
            delete s;
        }
        return rv;
    });
    t("loop switch transform", auto sub () {
        string s = "ABC";
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            switch (s.lwr()) {
                case "abc": rv += "abc"; break;
                default: rv += "other";
            }
            delete s;
        }
        return rv;
    });

    # += and -= on a local deleted later in the loop: the NOTHING path stores the type's default first
    t("loop plus equals string", auto sub () {
        string s = "a";
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            rv += (s += "x");
            delete s;
        }
        return rv;
    });
    t("loop plus equals list", auto sub () {
        list<int> l = (1,);
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            l += 2;
            rv += l.size();
            delete l;
        }
        return rv;
    });
    t("loop plus equals hash", auto sub () {
        hash<auto> h = {"a": 1};
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            h += {"b": 2};
            rv += h.size();
            delete h;
        }
        return rv;
    });
    t("loop minus equals int", auto sub () {
        int v = 10;
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            rv += (v -= 3);
            delete v;
        }
        return rv;
    });
    t("loop plus equals closure", auto sub () {
        string s = "a";
        code c = string sub () { return s; };
        list<auto> rv = ();
        for (int i = 0; i < 2; ++i) {
            s += "y";
            rv += c();
            delete s;
        }
        return rv;
    });

    # assigned again after the delete
    t("reassigned", auto sub () { string s = "ab"; delete s; s = "cd"; return (s == "cd", s + "e"); });
}

main();
