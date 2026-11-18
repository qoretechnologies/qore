#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

#! the values that Tracked destructors released, in order
our string released;

#! a value whose destructor records when it is released
class Tracked {
    public {
        string n;
    }

    constructor(string nn) {
        n = nn;
    }

    destructor() {
        released += n + " ";
    }
}

#! an iterator over a list of strings, which hash map and fold iterate with an iterator
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

#! raises for the value named \a name
Tracked sub fail_on(Tracked t, string name) {
    if (t.n == name) {
        throw "STAGE", t.n;
    }
    return t;
}

#! raises with the accumulator for the element "b"
string sub raise_at(auto element, auto accumulator) {
    if (element == "b") {
        throw "FOLD", accumulator;
    }
    return "";
}

#! raises an exception from code that runs with the implicit arguments set, with no handler in this function
string sub raise_in_fold(list<auto> l) {
    return foldl (new list<auto>($1, $2)).join("") + raise_at($2, $1), l;
}

#! the function's own arguments are $1 and $2 of the code outside of the operators
string sub t(...) {
    string out;
    list<string> tl = ("x1", "x2");
    list ul = ("x1", "x2");
    list<int> il = (1, 2, 3);

    # $2 in the code of hash map and hash map-select is unset, not the function's argument
    out += sprintf("hm typed $2 %y\n", map {$1: ($2 ?? "-")}, tl);
    out += sprintf("hm untyped $2 %y\n", map {$1: ($2 ?? "-")}, ul);
    out += sprintf("hm iterator $2 %y\n", map {$1: ($2 ?? "-")}, new StrIt(tl));
    # code that the AST interpreter evaluates reads the element and its index from the thread
    out += sprintf("hm value ctx %y\n", map {$1: (new hash<string, auto>({"v": $1, "i": $#})).v}, tl);
    out += sprintf("hm key ctx %y\n", map {(new list<auto>($1, $#)).join("-"): $#}, tl);
    out += sprintf("hm untyped ctx %y\n", map {$1: (new list<auto>($1, $#)).join("-")}, ul);
    out += sprintf("hm iterator ctx %y\n", map {$1: (new list<auto>($1, $#)).join("-")}, new StrIt(tl));
    out += sprintf("hm plain %y\n", map {$1: $#}, tl);
    out += sprintf("hms pred ctx %y\n", map {$1: $#}, tl, !exists $2);
    out += sprintf("hms insert ctx %y\n", map {$1: ($2 ?? "-")}, tl, $# == 1);
    out += sprintf("hms both %y\n", map {$1: ($2 ?? "-")}, tl, !exists $2);
    out += sprintf("hms untyped both %y\n", map {$1: ($2 ?? "-")}, ul, !exists $2);
    out += sprintf("hms iterator both %y\n", map {$1: ($2 ?? "-")}, new StrIt(tl), !exists $2);
    out += sprintf("hms value ctx %y\n", map {$1: (new list<auto>($1, $#)).join("-")}, tl, $# == 1);
    out += sprintf("hms plain %y\n", map {$1: $#}, tl, $# == 0);

    # foldl and foldr set (accumulator, element) as the implicit arguments of their expression
    out += sprintf("foldl ctx %y\n", foldl (new list<auto>($1, $2)).join(""), ("a", "b", "c"));
    out += sprintf("foldr ctx %y\n", foldr (new list<auto>($1, $2)).join(""), ("a", "b", "c"));
    out += sprintf("foldl typed ctx %y\n", foldl (new list<auto>($1, $2))[0] + $2, il);
    out += sprintf("foldr typed ctx %y\n", foldr (new list<auto>($1, $2))[0] + $2, il);
    out += sprintf("foldl iterator ctx %y\n", foldl (new list<auto>($1, $2)).join(""), new StrIt(tl));
    out += sprintf("foldl plain %y\n", foldl $1 + $2, il);
    out += sprintf("foldr plain %y\n", foldr $1 + $2, il);
    out += sprintf("foldl fused ctx %y\n", foldl (new list<auto>($1, $2)).join(""),
        (take 2, (map $1 + "!", ("a", "b", "c"))));
    out += sprintf("foldl fused plain %y\n", foldl $1 + $2, (take 2, (map $1 * 10, il)));

    # an exception that the code raises with the implicit arguments set restores them and releases the values
    try {
        hash<auto> h = map {$1.n: fail_on($1, $2 ? "x" : "h2")}, (new Tracked("h1"), new Tracked("h2"),
            new Tracked("h3"));
        out += sprintf("no exception %y\n", h);
    } catch (hash<ExceptionInfo> ex) {
        out += sprintf("hm catch %s released %s\n", ex.err, released);
        released = "";
    }
    try {
        hash<auto> h = map {$1.n: fail_on($1, $2 ? "x" : "h2")}, (new Tracked("h1"), new Tracked("h2"),
            new Tracked("h3")), !exists $2;
        out += sprintf("no exception %y\n", h);
    } catch (hash<ExceptionInfo> ex) {
        out += sprintf("hms catch %s released %s\n", ex.err, released);
        released = "";
    }
    try {
        string s = foldl (new list<auto>($1, $2)).join("") + raise_at($2, $1), ("a", "b", "c");
        out += sprintf("no exception %y\n", s);
    } catch (hash<ExceptionInfo> ex) {
        out += sprintf("foldl catch %s %y\n", ex.err, ex.desc);
    }
    try {
        raise_in_fold(("a", "b", "c"));
    } catch (hash<ExceptionInfo> ex) {
        out += sprintf("foldl leaves function %s %y\n", ex.err, ex.desc);
    }
    out += sprintf("function args %y\n", ($1, $2));
    return out;
}

print(t("a", "b"));
printf("top $1 %y\n", $1);
