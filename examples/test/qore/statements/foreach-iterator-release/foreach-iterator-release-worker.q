# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# leaves foreach loops over iterators early (by return and by break), runs context statements and foreach loops over
# references, and prints each case's result and how many heap integers it left behind (dbg_get_live_bigint_count()),
# which foreach-iterator-release.qtest checks: the IR interpreter keeps a loop's or a selector's iterator, an exception
# that it sets aside, and the state of a context statement or a reference foreach in a value slot, and on aarch64 Linux
# the slot held the object's address as a heap integer that was not released (issue #6345)
%modern
%disable-warning unreferenced-variable

auto sub xrange_return(auto x) {
    foreach int i in (xrange(3)) {
        auto v = x + 1;
        if (i == 1) {
            return v;
        }
    }
}

auto sub xrange_first_return(auto x) {
    foreach int i in (xrange(3)) {
        return x + 1;
    }
}

int sub xrange_break() {
    int n = 0;
    foreach int i in (xrange(10)) {
        if (i == 2) {
            break;
        }
        ++n;
    }
    return n;
}

auto sub range_return(auto x) {
    foreach int i in (range(1, 5)) {
        if (i == 3) {
            return x - 1;
        }
    }
}

auto sub pair_return(auto x) {
    hash<auto> h = {"a": 1, "b": 2, "c": 3};
    foreach hash<auto> p in (h.pairIterator()) {
        if (p.key == "b") {
            return x * 2;
        }
    }
}

auto sub keys_return(auto x) {
    hash<auto> h = {"a": 1, "b": 2, "c": 3};
    foreach string k in (keys h) {
        if (k == "b") {
            return x + 2;
        }
    }
}

# a map over an iterator left by a return from the loop over its result
auto sub map_return(auto x) {
    foreach int i in (map $1 * 2, xrange(5)) {
        if (i == 4) {
            return x + 5;
        }
    }
}

int sub two() {
    return 2;
}

# a selector iterator (a list of selectors with a range) left by a return
auto sub selector_return(auto x) {
    list<int> m = (10, 11, 12, 13);
    foreach int i in (m[0, 1..two()]) {
        if (i == 11) {
            return x + 6;
        }
    }
}

# a context statement, whose state the IR interpreter keeps in a value slot, left at its end and by a return
auto sub context_end(auto x) {
    hash<auto> q = {"a": (1, 2, 3)};
    int t = 0;
    context (q) {
        t += %a;
    }
    return x + t;
}

auto sub context_return(auto x) {
    hash<auto> q = {"a": (1, 2, 3)};
    context (q) {
        if (%a == 2) {
            return x + 7;
        }
    }
}

# a foreach over a reference, whose state the IR interpreter keeps in a value slot, left at its end and by a return
auto sub ref_end(auto x) {
    list<int> l = (1, 2, 3);
    foreach int i in (\l) {
        i *= 2;
    }
    return x + l[2];
}

auto sub ref_return(auto x) {
    list<int> l = (1, 2, 3);
    foreach int i in (\l) {
        if (i == 2) {
            return x + 8;
        }
    }
}

int sub raise_selector() {
    throw "SELECTOR-ERROR";
}

# a remove whose second selector raises: the IR interpreter sets the exception aside (ExceptionSuspend) while the
# first selector's element is removed, and keeps it in a value slot, in a loop over an iterator left by a return
auto sub suspend_return(auto x) {
    list<int> m = (10, 11, 12, 13);
    foreach int i in (xrange(3)) {
        try {
            remove m[0, raise_selector()];
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "SELECTOR-ERROR") {
                rethrow;
            }
        }
        if (i == 1) {
            return x + m.size();
        }
    }
}

# controls: a list literal and a while loop
auto sub list_return(auto x) {
    foreach int i in ((0, 1, 2)) {
        if (i == 1) {
            return x + 3;
        }
    }
}

auto sub while_return(auto x) {
    int i = 0;
    while (i < 3) {
        if (i == 1) {
            return x + 4;
        }
        ++i;
    }
}

# made before the counts are taken: the value of a constant expression is made at its first evaluation and kept
int big = 1 << 60;

hash<string, code> cases = {
    "xrange return": auto sub () { return xrange_return(big); },
    "xrange first return": auto sub () { return xrange_first_return(big); },
    "xrange break": auto sub () { return xrange_break(); },
    "range return": auto sub () { return range_return(big); },
    "pairIterator return": auto sub () { return pair_return(big); },
    "keys return": auto sub () { return keys_return(big); },
    "map return": auto sub () { return map_return(big); },
    "selector return": auto sub () { return selector_return(big); },
    "context end": auto sub () { return context_end(big); },
    "context return": auto sub () { return context_return(big); },
    "reference end": auto sub () { return ref_end(big); },
    "reference return": auto sub () { return ref_return(big); },
    "suspend return": auto sub () { return suspend_return(big); },
    "list return": auto sub () { return list_return(big); },
    "while return": auto sub () { return while_return(big); },
};

if (!Option::HAVE_LIBRARY_DEBUGGING) {
    printf("skip: dbg_get_live_bigint_count() requires library debugging\n");
    exit(0);
}
foreach hash<auto> c in (cases.pairIterator()) {
    int before = call_function("dbg_get_live_bigint_count");
    auto last;
    for (int i = 0; i < 5; ++i) {
        last = c.value();
    }
    # the last result can be a heap integer: it is released before the count
    string result = sprintf("%y", remove last);
    printf("%s: %s left: %d\n", c.key, result, call_function("dbg_get_live_bigint_count") - before);
}
