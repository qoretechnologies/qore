#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Applies compound assignments to the members of hashes and the elements of lists whose value type was narrowed at
# runtime, and prints one line per operation giving its outcome and the container afterwards.  The operands are
# returned by functions declared to return auto so that no type is known at parse time and the checks are made at
# runtime in every execution mode.

%modern

hash<auto> sub mk_hash(hash<auto> h) {
    return h;
}

list<auto> sub mk_list(list<auto> l) {
    return l;
}

auto sub id(auto v) {
    return v;
}

#! true in the last run, which prints the outcomes
our bool print_results;

#! runs one operation and prints its outcome followed by the container it changed
sub check(string label, code op, code show) {
    string outcome;
    try {
        op();
        outcome = "ok";
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    if (print_results) {
        printf("%s: %s %y\n", label, outcome, show());
    }
}

#! applies compound assignments to a local that no closure captures
sub uncaptured_local_cases() {
    hash<auto> h = mk_hash({"a": 2});
    string outcome;
    try {
        h.n *= id(1.5);
        outcome = "ok";
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    try {
        h.a /= id(4.0);
        outcome += " ok";
    } catch (hash<ExceptionInfo> ex) {
        outcome += " " + ex.err;
    }
    list<auto> l = mk_list(("a",));
    try {
        l[2] -= id(1);
        outcome += " ok";
    } catch (hash<ExceptionInfo> ex) {
        outcome += " " + ex.err;
    }
    if (print_results) {
        printf("uncaptured local: %s %y %y\n", outcome, h, l);
    }
}

#! runs every case; the cases are run three times so that tiered execution runs them as native code in the last run
sub run_cases() {
    # a hash narrowed to hash<string, int>
    hash<auto> ih = mk_hash({"a": 2});
    code show_ih = auto sub () { return ih; };
    check("hash<string, int> type", sub () {}, auto sub () { return ih.fullType(); });
    # a float result must not be stored in an int value
    check("existing *= float", sub () { ih.a *= id(1.5); }, show_ih);
    check("existing /= float", sub () { ih.a /= id(4.0); }, show_ih);
    check("existing *= number", sub () { ih.a *= id(1.5n); }, show_ih);
    check("new *= float", sub () { ih.n1 *= id(1.5); }, show_ih);
    check("new /= float", sub () { ih.n2 /= id(1.5); }, show_ih);
    check("new /= 0", sub () { ih.n3 /= id(0); }, show_ih);
    check("new.x += 1", sub () { ih.n4.x += id(1); }, show_ih);
    check("new.x = 1", sub () { ih.n5.x = id(1); }, show_ih);
    # int operands converted to the int value type, as for an int variable
    check("existing += float", sub () { ih.a += id(1.5); }, show_ih);
    check("new += float", sub () { ih.b += id(1.5); }, show_ih);
    check("new -= number", sub () { ih.c -= id(1.5n); }, show_ih);
    check("new *= int", sub () { ih.d *= id(3); }, show_ih);

    # a hash narrowed to hash<string, string>
    hash<auto> sh = mk_hash({"a": "b"});
    code show_sh = auto sub () { return sh; };
    check("hash<string, string> type", sub () {}, auto sub () { return sh.fullType(); });
    check("new = int", sub () { sh.x = id(1); }, show_sh);
    check("new -= int", sub () { sh.n1 -= id(1); }, show_sh);
    check("new *= int", sub () { sh.n2 *= id(2); }, show_sh);
    check("new /= int", sub () { sh.n3 /= id(2); }, show_sh);
    check("new %= int", sub () { sh.n4 %= id(2); }, show_sh);
    check("new |= int", sub () { sh.n5 |= id(1); }, show_sh);
    check("new &= int", sub () { sh.n6 &= id(1); }, show_sh);
    check("new ^= int", sub () { sh.n7 ^= id(1); }, show_sh);
    check("new <<= int", sub () { sh.n8 <<= id(1); }, show_sh);
    check("new >>= int", sub () { sh.n9 >>= id(1); }, show_sh);
    check("new *= float", sub () { sh.n10 *= id(1.5); }, show_sh);
    check("new -= number", sub () { sh.n11 -= id(1.5n); }, show_sh);
    check("new ++", sub () { sh.n12++; }, show_sh);
    check("new --", sub () { sh.n13--; }, show_sh);
    check("new pre ++", sub () { ++sh.n14; }, show_sh);
    check("new pre --", sub () { --sh.n15; }, show_sh);
    check("existing -= int", sub () { sh.a -= id(1); }, show_sh);
    check("existing *= float", sub () { sh.a *= id(1.5); }, show_sh);
    # a string operand result: the int is converted to the string value type, as for a string variable
    check("new += int", sub () { sh.y += id(1); }, show_sh);
    check("existing += int", sub () { sh.a += id(1); }, show_sh);

    # a hash narrowed to hash<string, binary>
    hash<auto> bh = mk_hash({"a": <00>});
    code show_bh = auto sub () { return bh; };
    check("new -= int binary", sub () { bh.n1 -= id(1); }, show_bh);

    # a list narrowed to list<int>
    list<auto> il = mk_list((2, 3));
    code show_il = auto sub () { return il; };
    check("list<int> type", sub () {}, auto sub () { return il.fullType(); });
    check("existing elem *= float", sub () { il[0] *= id(1.5); }, show_il);
    check("existing elem /= float", sub () { il[1] /= id(2.0); }, show_il);
    check("new elem *= float", sub () { il[4] *= id(1.5); }, show_il);
    check("new elem /= 0", sub () { il[4] /= id(0); }, show_il);
    check("existing elem += float", sub () { il[0] += id(1.5); }, show_il);

    # a list narrowed to list<string>
    list<auto> sl = mk_list(("a", "b"));
    code show_sl = auto sub () { return sl; };
    check("list<string> type", sub () {}, auto sub () { return sl.fullType(); });
    check("new elem = int", sub () { sl[3] = id(1); }, show_sl);
    check("new elem -= int", sub () { sl[3] -= id(1); }, show_sl);
    check("new elem ++", sub () { sl[3]++; }, show_sl);
    check("existing elem *= float", sub () { sl[0] *= id(1.5); }, show_sl);
    check("new elem += int", sub () { sl[2] += id(1); }, show_sl);

    # a declared complex hash
    hash<string, int> dh = {"a": 2};
    code show_dh = auto sub () { return dh; };
    check("declared existing *= float", sub () { dh.a *= id(1.5); }, show_dh);
    check("declared new /= float", sub () { dh.n /= id(1.5); }, show_dh);

    # a hash<auto> lvalue re-seeded with += keeps accepting any value type
    hash<auto> seeded;
    seeded += mk_hash({"a": "b"});
    code show_seeded = auto sub () { return seeded; };
    check("seeded type", sub () {}, auto sub () { return seeded.fullType(); });
    check("seeded new = int", sub () { seeded.n = id(1); }, show_seeded);

    uncaptured_local_cases();
}

for (int i = 0; i < 3; ++i) {
    print_results = (i == 2);
    run_cases();
}
