#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# qore#6252: a switch case with == under strict boolean semantics matches as the operator does in the condition of an
# if statement (AST only: strict boolean semantics cannot be combined with %modern, which compiled code needs)

%new-style
%strict-args
%require-types
%strict-bool-eval

int mismatches = 0;

string sub sw_t(auto v) { switch (v) { case == True: return "1"; } return "0"; }
string sub if_t(auto v) { if (v == True) { return "1"; } return "0"; }
string sub sw_f(auto v) { switch (v) { case == False: return "1"; } return "0"; }
string sub if_f(auto v) { if (v == False) { return "1"; } return "0"; }
string sub sw_1(auto v) { switch (v) { case == 1: return "1"; } return "0"; }
string sub if_1(auto v) { if (v == 1) { return "1"; } return "0"; }
string sub sw_e(auto v) { switch (v) { case == "": return "1"; } return "0"; }
string sub if_e(auto v) { if (v == "") { return "1"; } return "0"; }

# returns the result of a call, or the error of its exception
string sub res(code c, auto v) {
    try {
        return c(v);
    } catch (hash<ExceptionInfo> ex) {
        return "ex:" + ex.err;
    }
}

list<auto> vals = (True, False, 1, 0, "", "x", NOTHING, NULL, (), {}, new buffer<int64>((1, 2)),
    new buffer<int64>());
foreach auto v in (vals) {
    list<string> r = (res(\sw_t(), v), res(\if_t(), v), res(\sw_f(), v), res(\if_f(), v), res(\sw_1(), v),
        res(\if_1(), v), res(\sw_e(), v), res(\if_e(), v));
    printf("strict %d: %s\n", $#, r.join(" "));
    for (int i = 0; i < r.size(); i += 2) {
        if (r[i] != r[i + 1]) {
            ++mismatches;
        }
    }
}
printf("mismatches: %d\n", mismatches);
