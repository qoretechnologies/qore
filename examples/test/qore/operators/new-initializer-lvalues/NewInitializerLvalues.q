#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# modifications of local variables in the initializer or arguments of each form of new, which every execution mode must
# perform as the AST interpreter does; prints the values built and the locals afterwards

%modern

hashdecl NewInitHd {
    auto a;
}

class NewInitC {
    public {
        auto v;
    }

    constructor(auto v) {
        self.v = v;
    }
}

#! the modifications made in each form, applied to the locals of one call
string sub modify(string form, bool in_try) {
    int c = 1;
    int d = 2;
    int x = 1;
    int y = 1;
    list<auto> l = (1, 2, 3);
    list<auto> l2 = (1, 2, 3);
    list<auto> l3 = (1, 2, 3);
    string s = "abc";
    auto built;
    switch (form) {
        case "hash": {
            built = (
                new hash<string, auto>({"a": ((c, d) = (7, 8))}),
                new hash<string, auto>({"a": (x += 5)}),
                new hash<string, auto>({"a": y++}),
                new hash<string, auto>({"a": extract l, 0, 1}),
                new hash<string, auto>({"a": (push l2, 4)}),
                new hash<string, auto>({"a": (s =~ s/a/z/)}),
                new hash<string, auto>({"a": splice l3, 0, 1}),
            );
            break;
        }
        case "list": {
            built = (
                new list<auto>(((c, d) = (7, 8))),
                new list<auto>(((x += 5), y++)),
                new list<auto>((extract l, 0, 1)),
                new list<auto>(((push l2, 4), (s =~ s/a/z/))),
                new list<auto>((splice l3, 0, 1)),
            );
            break;
        }
        case "hashdecl": {
            built = (
                new hash<NewInitHd>({"a": ((c, d) = (7, 8))}),
                new hash<NewInitHd>({"a": (x += 5)}),
                new hash<NewInitHd>({"a": y++}),
                new hash<NewInitHd>({"a": extract l, 0, 1}),
                new hash<NewInitHd>({"a": (push l2, 4)}),
                new hash<NewInitHd>({"a": (s =~ s/a/z/)}),
                new hash<NewInitHd>({"a": splice l3, 0, 1}),
            );
            break;
        }
        case "class": {
            built = map $1.v, (
                new NewInitC(((c, d) = (7, 8))),
                new NewInitC((x += 5)),
                new NewInitC(y++),
                new NewInitC((extract l, 0, 1)),
                new NewInitC((push l2, 4)),
                new NewInitC((s =~ s/a/z/)),
                new NewInitC((splice l3, 0, 1)),
            );
            break;
        }
    }
    return sprintf("%s%s: c=%d d=%d x=%d y=%d l=%y l2=%y l3=%y s=%y built=%y", form, in_try ? " (in try)" : "", c,
        d, x, y, l, l2, l3, s, built);
}

#! the same in a try block, where the construction can raise and is lowered as an invoke
string sub modify_in_try(string form) {
    try {
        return modify_try_body(form);
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s (in try): %s: %s", form, ex.err, ex.desc);
    }
}

string sub modify_try_body(string form) {
    int x = 1;
    list<auto> l = (1, 2, 3);
    string s = "abc";
    auto built;
    try {
        switch (form) {
            case "hash": built = new hash<string, auto>({"a": (x += 5), "b": (extract l, 0, 1), "c": (s =~ s/a/z/)});
                break;
            case "list": built = new list<auto>(((x += 5), (extract l, 0, 1), (s =~ s/a/z/)));
                break;
        }
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s (in try): %s", form, ex.err);
    }
    return sprintf("%s (in try): x=%d l=%y s=%y built=%y", form, x, l, s, built);
}

#! returns its argument as a value of no declared type
auto sub non_hash(auto v) {
    return v;
}

#! the initializer's value is checked and converted as before
string sub conversions() {
    int n = 0;
    string rv;
    try {
        hash<string, int> h = new hash<string, int>(non_hash(++n));
        rv = sprintf("no error: %y", h);
    } catch (hash<ExceptionInfo> ex) {
        rv = sprintf("%s: %s", ex.err, ex.desc);
    }
    hash<string, softint> sh = new hash<string, softint>({"a": "23", "b": (++n, "4")[1]});
    return sprintf("non-hash initializer: %s; n=%d; softint hash: %y", rv, n, sh);
}

#! an initializer that is the only use of a value read without a reference of its own (a local, a list element, a hash
#! member, an object member) is copied, not taken over: the source keeps its value
string sub borrowed_initializers() {
    hash<auto> src = {"a": 1, "b": 2};
    list<auto> srcl = (1, 2);
    list<auto> nested = ({"a": 3}, (4, 5));
    hash<auto> outer = {"h": {"a": 6}, "l": (7, 8)};
    NewInitC o({"a": 9});
    hash<string, int> h1 = new hash<string, int>(src);
    list<auto> l1 = new list<auto>(srcl);
    hash<string, int> h2 = new hash<string, int>(nested[0]);
    list<auto> l2 = new list<auto>(nested[1]);
    hash<string, int> h3 = new hash<string, int>(outer.h);
    list<auto> l3 = new list<auto>(outer.l);
    hash<string, int> h4 = new hash<string, int>(o.v);
    h1.c = 3;
    push l1[0], 3;
    h2.b = 4;
    push l2[0], 6;
    h3.b = 7;
    push l3[0], 9;
    h4.b = 10;
    # the value of a modification of a local is the local's own value
    hash<auto> msrc = {"a": 1};
    list<auto> mlist = (1,);
    hash<string, int> h5 = new hash<string, int>(msrc += {"b": 2});
    list<auto> l5 = new list<auto>((push mlist, 2));
    h5.c = 3;
    push l5[0], 3;
    # the values of a list iterated
    list<hash<auto>> each = ({"a": 1}, {"a": 2});
    list<hash<string, int>> hl = ();
    foreach hash<auto> e in (each) {
        hash<string, int> he = new hash<string, int>(e);
        he.b = 0;
        push hl, he;
    }
    return sprintf("built: %y %y %y %y %y %y %y %y %y %y; sources: %y %y %y %y %y %y %y %y", h1, l1, h2, l2, h3, l3,
        h4, h5, l5, hl, src, srcl, nested, outer, o.v, msrc, mlist, each);
}

# each function is called several times, so that tiered execution compiles it before the calls checked
for (int i = 0; i < 3; ++i) {
    foreach string form in (("hash", "list", "hashdecl", "class")) {
        modify(form, False);
    }
    modify_in_try("hash");
    modify_in_try("list");
    conversions();
    borrowed_initializers();
}
foreach string form in (("hash", "list", "hashdecl", "class")) {
    printf("%s\n", modify(form, False));
}
printf("%s\n", modify_in_try("hash"));
printf("%s\n", modify_in_try("list"));
printf("%s\n", conversions());
printf("%s\n", borrowed_initializers());
