#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a local of a block that runs again (a loop body) is a local of the frame in every run of the block

%modern
%allow-debugger

# the value of a local of the calling frame as get_local_vars() gives it, or "missing"
string sub reflected(hash<auto> locals, string name) {
    return exists locals{name} ? sprintf("%y", locals{name}.value) : "missing";
}

string sub for_loop() {
    list<string> out = ();
    for (int i = 0; i < 3; ++i) {
        int value = i * 10;
        push out, reflected(get_local_vars(0), "value");
    }
    return out.join(",");
}

string sub float_reassigned() {
    list<string> out = ();
    for (int i = 0; i < 3; ++i) {
        auto value = 1.25;
        value = float(i);
        push out, reflected(get_local_vars(0), "value");
    }
    return out.join(",");
}

string sub while_loop() {
    list<string> out = ();
    int i = 0;
    while (i < 3) {
        string value = sprintf("s%d", i);
        push out, reflected(get_local_vars(0), "value");
        ++i;
    }
    return out.join(",");
}

string sub foreach_loop() {
    list<string> out = ();
    foreach int e in ((1, 2, 3)) {
        hash<auto> value = {"e": e};
        push out, reflected(get_local_vars(0), "value");
    }
    return out.join(",");
}

string sub nested_block() {
    list<string> out = ();
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            int inner = i * 2 + j;
            push out, reflected(get_local_vars(0), "inner");
        }
    }
    return out.join(",");
}

# after the loop, its local is no longer a local of the frame
string sub after_loop() {
    for (int i = 0; i < 2; ++i) {
        int value = i;
    }
    return reflected(get_local_vars(0), "value");
}

const Expected = {
    "for_loop": "0,10,20",
    "float_reassigned": "0.0,1.0,2.0",
    "while_loop": "\"s0\",\"s1\",\"s2\"",
    "foreach_loop": "{e: 1},{e: 2},{e: 3}",
    "nested_block": "0,1,2,3",
    "after_loop": "missing",
};

foreach hash<auto> i in (Expected.pairIterator()) {
    # each function runs several times, so that the tiers that compile a function after calls compile it
    string result;
    for (int n = 0; n < 4; ++n) {
        string r = call_function(i.key);
        if (!n) {
            result = r;
        } else if (r != result) {
            result += " / " + r;
        }
    }
    printf("%s %s: %s\n", result == i.value ? "PASS" : "FAIL", i.key, result);
}
