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

# each function is called several times, so that tiered execution compiles it before the calls checked
for (int i = 0; i < 3; ++i) {
    foreach string form in (("hash", "list", "hashdecl", "class")) {
        modify(form, False);
    }
    modify_in_try("hash");
    modify_in_try("list");
    conversions();
}
foreach string form in (("hash", "list", "hashdecl", "class")) {
    printf("%s\n", modify(form, False));
}
printf("%s\n", modify_in_try("hash"));
printf("%s\n", modify_in_try("list"));
printf("%s\n", conversions());
