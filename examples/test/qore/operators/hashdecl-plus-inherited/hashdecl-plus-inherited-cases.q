#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of hashdecl-plus-inherited.qtest (qore#6023): prints one "name=result" line per case

%modern

hashdecl D {
    int x = 1;
    string s = "a";
}

hashdecl D2 inherits D {
    int y = 2;
}

hashdecl D3 inherits D2 {
    int z = 3;
}

# a sibling of D2
hashdecl S inherits D {
    int w = 4;
}

# unrelated to D, with a member of the same name
hashdecl E {
    int x = 1;
}

hashdecl P<T> {
    T value;
}

hashdecl C<T> inherits P<T> {
    string name = "n";
}

# folded at parse time
const FoldChildParent = <D2>{"y": 5} + <D>{"s": "c"};
const FoldParentChild = <D>{"s": "c"} + <D2>{"y": 5};
const FoldSiblings = <D2>{} + <S>{};

# returns the value as a hash<D>, so the parse type of an expression with it is hash<D>
hash<D> sub hd(hash<D> h) {
    return h;
}

hash<D> sub sum(hash<D> l, hash<D> r) {
    return l + r;
}

string sub desc(auto v) {
    return sprintf("%s %y", v.fullType(), v);
}

sub check(string name, code c) {
    string result;
    try {
        result = c();
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("%s=%s\n", name, result);
}

sub plus_cases() {
    check("child-plus-parent", string sub () { return desc(hd(<D2>{"y": 5}) + hd(<D>{"s": "r"})); });
    check("parent-plus-child", string sub () { return desc(hd(<D>{"s": "l"}) + hd(<D2>{"s": "r"})); });
    check("same", string sub () { return desc(hd(<D>{"x": 7}) + hd(<D>{"s": "r"})); });
    check("same-child", string sub () { return desc(hd(<D2>{"x": 7}) + hd(<D2>{"s": "r"})); });
    check("grandchild-plus-parent", string sub () { return desc(hd(<D3>{}) + hd(<D>{"s": "r"})); });
    check("parent-plus-grandchild", string sub () { return desc(hd(<D>{}) + hd(<D3>{"s": "r"})); });
    check("grandchild-plus-child", string sub () { return desc(hd(<D3>{}) + hd(<D2>{"y": 9})); });
    check("siblings", string sub () { return desc(hd(<D2>{}) + hd(<S>{})); });
    check("unrelated", string sub () { return desc(<D>{} + <E>{"x": 2}); });
    check("plain-right", string sub () { return desc(hd(<D2>{}) + {"x": 3}); });
    check("plain-left", string sub () { return desc({"x": 3} + hd(<D2>{})); });
    check("auto-right", string sub () {
        hash<auto> h = {"x": 3};
        return desc(hd(<D>{}) + h);
    });
    check("auto-left", string sub () {
        hash<auto> h = {"x": 3};
        return desc(h + hd(<D>{}));
    });
    check("generic-child-plus-parent", string sub () {
        hash<C<int>> c(("value": 1));
        hash<P<int>> p(("value": 2));
        return desc(c + p);
    });
    check("generic-parent-plus-child", string sub () {
        hash<P<int>> p(("value": 1));
        hash<C<int>> c(("value": 2));
        return desc(p + c);
    });
    check("generic-other-argument", string sub () {
        hash<P<int>> p(("value": 1));
        hash<C<string>> c(("value": "v"));
        return desc(p + c);
    });
}

sub typed_cases() {
    # the case of the issue
    check("map", string sub () {
        list<hash<D>> l = map $1 + <D>{"s": "m"}, (hd(<D2>{}), hd(<D>{}));
        return (map $1.fullType(), l).join(",");
    });
    check("typed-local", string sub () {
        hash<D> h = hd(<D>{}) + hd(<D2>{});
        return desc(h);
    });
    check("return-child-parent", string sub () { return desc(sum(<D2>{}, <D>{"s": "r"})); });
    check("return-parent-child", string sub () { return desc(sum(<D>{}, <D2>{"s": "r"})); });
    check("return-siblings", string sub () { return desc(sum(<D2>{}, <S>{})); });
    check("fold-child-parent", string sub () { return desc(FoldChildParent); });
    check("fold-parent-child", string sub () { return desc(FoldParentChild); });
    check("fold-siblings", string sub () { return desc(FoldSiblings); });
}

sub unchanged_cases() {
    check("plus-equals-parent", string sub () {
        hash<D> h = hd(<D2>{});
        h += <D>{"s": "r"};
        return desc(h);
    });
    check("plus-equals-child", string sub () {
        hash<D> h = <D>{};
        h += hd(<D2>{});
        return desc(h);
    });
    check("plus-equals-undeclared", string sub () {
        hash<D> h = <D>{};
        h += {"zz": 1};
        return desc(h);
    });
    check("plus-equals-unrelated", string sub () {
        hash<D> h = <D>{};
        h += <E>{"x": 2};
        return desc(h);
    });
    check("minus-key", string sub () { return desc(hd(<D2>{}) - "s"); });
    check("minus-list", string sub () { return desc(hd(<D2>{}) - ("s", "y")); });
}

plus_cases();
typed_cases();
unchanged_cases();
