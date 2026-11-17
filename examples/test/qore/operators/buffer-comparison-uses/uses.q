# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# every use of a comparison of untyped operands (qore#6356): the parser types it bool, but with a buffer operand it
# gives the elementwise buffer<bool>; each use prints its result, or the code of its exception, which must be the
# same in every execution mode (buffer-comparison-uses.qtest)
%modern
# the unary minus of a comparison typed bool is one of the uses
%disable-warning invalid-operation

#! the uses of "a > b"
hash<string, code> sub gt_uses() {
    return {
        "value": auto sub (auto a, auto b) { return a > b; },
        "boolean": auto sub (auto a, auto b) { return boolean(a > b); },
        "string": auto sub (auto a, auto b) { return string(a > b); },
        "int": auto sub (auto a, auto b) { return int(a > b); },
        "float": auto sub (auto a, auto b) { return float(a > b); },
        "not": auto sub (auto a, auto b) { return !(a > b); },
        "and": auto sub (auto a, auto b) { return (a > b) && True; },
        "or": auto sub (auto a, auto b) { return (a > b) || False; },
        "ternary": auto sub (auto a, auto b) { return (a > b) ? "t" : "f"; },
        "bool variable": auto sub (auto a, auto b) { bool v = (a > b); return v; },
        "bool return": bool sub (auto a, auto b) { return a > b; },
        "plus": auto sub (auto a, auto b) { return (a > b) + 1; },
        "times": auto sub (auto a, auto b) { return (a > b) * 2; },
        "minus": auto sub (auto a, auto b) { return (a > b) - 1; },
        "negate": auto sub (auto a, auto b) { return -(a > b); },
        "bitand": auto sub (auto a, auto b) { return (a > b) & 1; },
        "eq": auto sub (auto a, auto b) { return (a > b) == True; },
        "sprintf d": auto sub (auto a, auto b) { return sprintf("%d", a > b); },
        "sprintf s": auto sub (auto a, auto b) { return sprintf("%s", a > b); },
        "sprintf y": auto sub (auto a, auto b) { return sprintf("%y", a > b); },
        "list element": auto sub (auto a, auto b) { return (a > b,); },
        "hash value": auto sub (auto a, auto b) { return {"k": a > b}; },
        "if": auto sub (auto a, auto b) { if (a > b) { return 1; } return 0; },
        "while": auto sub (auto a, auto b) {
            int n = 0;
            while (a > b) {
                if (++n == 2) {
                    break;
                }
            }
            return n;
        },
        "for": auto sub (auto a, auto b) {
            int n = 0;
            for (; a > b; ++n) {
                if (n == 3) {
                    break;
                }
            }
            return n;
        },
        "exists": auto sub (auto a, auto b) { return exists (a > b); },
        "size": auto sub (auto a, auto b) { auto v = (a > b); return v.size(); },
        # the method of the parse type (bool)
        "size direct": auto sub (auto a, auto b) { return (a > b).size(); },
        "type": auto sub (auto a, auto b) { return type(a > b); },
        "cast": auto sub (auto a, auto b) { return cast<bool>(a > b); },
        "coalesce": auto sub (auto a, auto b) { return (a > b) ?? "x"; },
        "inlist": auto sub (auto a, auto b) { return inlist(a > b, (True,)); },
        "plus assign": auto sub (auto a, auto b) { auto x = 1; x += (a > b); return x; },
        "switch value": auto sub (auto a, auto b) {
            switch (a > b) {
                case True: return "true";
                case False: return "false";
            }
            return "default";
        },
        # a case with an operator (qore#6252): matches as the operator does in the condition of an if statement
        "switch case operator": auto sub (auto a, auto b) {
            switch (a) {
                case > 2: return "gt 2";
                case == 0: return "eq 0";
            }
            return "default";
        },
    };
}

#! the uses of "a == b", which give the result through another operator
hash<string, code> sub eq_uses() {
    return {
        "value": auto sub (auto a, auto b) { return a == b; },
        "boolean": auto sub (auto a, auto b) { return boolean(a == b); },
        "int": auto sub (auto a, auto b) { return int(a == b); },
        "not": auto sub (auto a, auto b) { return !(a == b); },
        "if": auto sub (auto a, auto b) { if (a == b) { return 1; } return 0; },
        "ternary": auto sub (auto a, auto b) { return (a == b) ? "t" : "f"; },
    };
}

#! formats a result: a buffer with its elements (its %y form shows its address), other values as %y
string sub show(auto v) {
    if (type(v) == "buffer") {
        list<auto> l = ();
        for (int i = 0; i < v.size(); ++i) {
            push l, v[i];
        }
        # the type of the buffer, as %y gives it (ex: "buffer<bool>"), and its elements
        return sprintf("%s %y", (sprintf("%y", v) =~ x/^(buffer(<[^>]*>)?)/)[0], l);
    }
    if (type(v) == "string") {
        # a string made by sprintf("%y") of a buffer shows the address as well
        return sprintf("%y", regex_subst(v, "object 0x[0-9a-f]+ ", ""));
    }
    return sprintf("%y", v);
}

#! prints the result of each use, or the code of its exception
sub run(string label, hash<string, code> uses, auto a, auto b) {
    printf("%s:\n", label);
    foreach hash<auto> i in (uses.pairIterator()) {
        string r;
        try {
            r = show(i.value(a, b));
        } catch (hash<ExceptionInfo> ex) {
            r = ex.err;
        }
        printf("  %s: %s\n", i.key, r);
    }
}

# a buffer operand, a buffer of zeros, an empty buffer and an int operand
run("buffer gt", gt_uses(), new buffer<int64>((1, 2, 3)), 2);
run("zeros gt", gt_uses(), new buffer<int64>((0, 0, 0)), 2);
run("empty gt", gt_uses(), new buffer<int64>(), 2);
run("int gt", gt_uses(), 3, 2);
run("int not gt", gt_uses(), 1, 2);
run("buffer eq", eq_uses(), new buffer<int64>((1, 2, 3)), 2);
run("int eq", eq_uses(), 2, 2);
