%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# every construct that sets the implicit index ($#) for the code that it runs for each element; see
# implicit-index-outside.qtest (qore#803)

class C {
    list m(list l) {
        list r;
        foreach auto v in (l) {
            r += ($#, v);
        }
        return r;
    }

    static list sm(list l) {
        return map $#, l;
    }
}

list sub fn(list l) {
    return map $# * 10, l;
}

string sub fmt(auto v) { return sprintf("%y", v); }

list sub cases() {
    list l = ("a", "b", "c");
    list out;

    list fe;
    foreach string s in (l) {
        fe += (s, $#);
    }
    out += "foreach " + fmt(fe);

    list nf;
    foreach string s in (l) {
        foreach int i in ((7, 8)) {
            nf += (s, $#, i);
        }
        # the outer index again after the inner loop
        nf += $#;
    }
    out += "foreach nested " + fmt(nf);

    list wf;
    foreach string s in (l) {
        int i = 0;
        while (i < 2) {
            wf += $#;
            ++i;
        }
        for (int j = 0; j < 1; ++j) {
            wf += $# * 10;
        }
        do {
            wf += $# * 100;
        } while (False);
        switch (s) {
            case "b": wf += ("sw", $#); break;
        }
        try {
            if (s == "c") {
                throw "X";
            }
        } catch (hash<ExceptionInfo> ex) {
            wf += ("catch", $#);
        }
    }
    out += "loops in foreach " + fmt(wf);

    list ob;
    foreach string s in (l) {
        on_exit {
            ob += ("exit", $#);
        }
        ob += s;
    }
    out += "on_exit in foreach " + fmt(ob);

    out += "map " + fmt(map $#, l);
    out += "map-select " + fmt(map $# * 2, l, $# != 1);
    out += "select " + fmt(select l, $# != 1);
    out += "hash map " + fmt(map {$1: $#}, l);
    out += "hash map key " + fmt(map {$1 + $#: $1}, l);
    out += "hash map-select " + fmt(map {$1: $#}, l, $# > 0);
    out += "first " + fmt((first ($# == 1), l));
    out += "any " + fmt((any ($# == 2), l));
    out += "all " + fmt((all ($# < 3), l));
    out += "count " + fmt((count ($# > 0), l));
    out += "takewhile " + fmt((takewhile ($# < 2), l));
    out += "takeuntil " + fmt((takeuntil ($# == 2), l));
    out += "single " + fmt(map $#, "s");

    out += "map nested " + fmt(map (map $#, (1, 2)), l);
    out += "map nested outer source " + fmt(map (map $1, ($#, $#)), l);
    out += "closure own foreach in map " + fmt(map (list sub () {
        list r;
        foreach int i in ((5, 6)) {
            r += $# + i;
        }
        return r;
    })(), l);
    out += "closure arg in map " + fmt(map (auto sub (int i) { return i; })($#), l);
    out += "closure own map in map " + fmt(map (auto sub () { return map $#, (1, 2); })(), l);
    out += "fold source in map " + fmt(map (foldl $1 + $2, ($#, 10)), l);
    out += "map in fold " + fmt(foldl $1 + (map $#, (5, 6)).size(), (1, 2));
    out += "function " + fmt(fn(l));
    C c();
    out += "method " + fmt(c.m(l));
    out += "static method " + fmt(C::sm(l));
    code cl = list sub (list x) { return map $#, x; };
    out += "closure " + fmt(cl(l));
    return out;
}

foreach string line in (cases()) {
    printf("%s\n", line);
}
printf("top %y\n", map $#, (1, 2));
