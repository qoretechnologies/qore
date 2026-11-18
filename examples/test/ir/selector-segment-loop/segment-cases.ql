# -*- mode: qore; indent-tabs-mode: nil -*-
# the cases of selector-segment-loop.qtest: lists of selectors with ranges over lists of int, float and bool values
# that a foreach statement or a functional or streaming operator iterates lazily, which the IR interpreter, native
# code and AOT iterate with the selector-segment loop (qore#5711); included by segment-modes.q

# Copyright (C) 2026 Qore Technologies, s.r.o.

list<int> li = (10, 11, 12, 13, 14, 15);
# ints that the NaN-boxed representation cannot hold are nodes
list<int> lbig = (0x7fffffffffffffff, -0x7fffffffffffffff, 0x1000000000000, 3);
list<float> lf = (0.5, -1.5e300, 2.25, -0.0, 1e-300);
list<bool> lb = (True, False, True);
list<*int> ln = (1, NOTHING, 3);
*int k;

#! records that a selector was evaluated
int sub f(string tag, int rv) {
    print("  sel " + tag + "\n");
    return rv;
}

#! records a value that the body or an operator's expression uses
auto sub use(string tag, auto v) {
    printf("  %s %y\n", tag, v);
    return v;
}

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s: %s\n", ex.err, ex.desc);
    }
}

# --- foreach ---

auto sub fe_int() {
    foreach int x in (li[f("a", 0), f("b", 2)..f("c", 4), f("d", 1)]) {
        use("body " + $#, x);
    }
}

auto sub fe_int_descending() {
    foreach int x in (li[f("a", 4)..f("b", 1), f("c", 5)..f("d", 5)]) {
        use("body", x);
    }
}

auto sub fe_out_of_range() {
    foreach auto x in (li[f("a", 4)..f("b", 8), f("c", 9), f("d", -9)..f("e", -5)]) {
        use("body", x);
    }
}

auto sub fe_negative() {
    foreach auto x in (li[f("a", -1), f("b", -3)..f("c", -2), f("d", -2)..f("e", 1)]) {
        use("body", x);
    }
}

auto sub fe_empty_selectors() {
    foreach auto x in (li[f("a", 10)..f("b", 12), f("c", 2)]) {
        use("body", x);
    }
}

auto sub fe_big() {
    foreach int x in (lbig[f("a", 0)..f("b", 3), f("c", 1)]) {
        use("body", x);
    }
}

auto sub fe_float() {
    foreach float x in (lf[f("a", 1)..f("b", 4), f("c", 0)]) {
        use("body", x);
    }
}

auto sub fe_bool() {
    foreach bool x in (lb[f("a", 2)..f("b", 0), f("c", 1)]) {
        use("body", x);
    }
}

auto sub fe_or_nothing() {
    foreach *int x in (ln[f("a", 0)..f("b", 2), f("c", 7)]) {
        use("body", x);
    }
}

auto sub fe_typed_nothing() {
    foreach int x in (li[f("a", 4)..f("b", 6), f("c", 0)]) {
        use("body", x);
    }
}

auto sub fe_break() {
    foreach int x in (li[f("a", 0)..f("b", 3), f("c", 5)]) {
        use("body", x);
        if (x == 11) {
            break;
        }
    }
    return "after";
}

auto sub fe_continue() {
    foreach int x in (li[f("a", 0)..f("b", 2), f("c", 5)]) {
        if (x == 11) {
            continue;
        }
        use("body", x);
    }
}

auto sub fe_return() {
    foreach int x in (li[f("a", 0)..f("b", 3), f("c", 5)]) {
        use("body", x);
        if (x == 12) {
            return x;
        }
    }
    return -1;
}

auto sub fe_body_throws() {
    foreach int x in (li[f("a", 0)..f("b", 3), f("c", 5)]) {
        use("body", x);
        if (x == 11) {
            throw "BODY-ERROR", sprintf("at %d", x);
        }
    }
}

auto sub fe_start_nothing() {
    foreach int x in (li[f("a", 0), k..f("c", 3), f("d", 5)]) {
        use("body", x);
    }
}

auto sub fe_end_nothing() {
    foreach int x in (li[f("a", 0), f("b", 2)..k, f("d", 5)]) {
        use("body", x);
    }
}

auto sub fe_selector_throws_try() {
    try {
        foreach int x in (li[f("a", 0), f("b", 1)..k]) {
            use("body", x);
        }
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return "after";
}

auto sub fe_nested() {
    foreach int x in (li[f("a", 0)..f("b", 1)]) {
        foreach int y in (li[f("c", 4), f("d", 2)..f("e", 3)]) {
            use("body", x * 100 + y);
        }
    }
}

# the list that the body changes is not the one iterated (the iterated list is not shared with it)
auto sub fe_body_appends() {
    list<int> m = (1, 2, 3);
    foreach auto x in (m[f("a", 0), f("b", 1)..f("c", 4)]) {
        use("body", x);
        m += 10 + m.size();
    }
    return m;
}

auto sub fe_body_splices() {
    list<int> m = (1, 2, 3, 4);
    foreach auto x in (m[f("a", 0)..f("b", 3)]) {
        use("body", x);
        splice m, 0, 1;
    }
    return m;
}

auto sub fe_body_sets() {
    list<int> m = (1, 2, 3, 4);
    foreach auto x in (m[f("a", 3), f("b", 0)..f("c", 2)]) {
        use("body", x);
        m[2] = 99;
        m = ();
    }
    return m;
}

# --- stable selectors: constants and int locals, which native code also iterates with the loop ---

auto sub fe_inert() {
    foreach int x in (li[0, 2..4, -1, 9]) {
        use("body " + $#, x);
    }
}

auto sub fe_inert_locals() {
    int a = 1;
    int b = 3;
    foreach int x in (li[a..b, a]) {
        use("body", x);
    }
}

# the body changes a local that a selector reads: the selector is evaluated when it is reached
auto sub fe_inert_local_written() {
    int a = 1;
    foreach int x in (li[0, a..3]) {
        use("body", x);
        a = 4;
    }
    return a;
}

auto sub fe_inert_break() {
    foreach int x in (li[0..5]) {
        use("body", x);
        if (x == 12) {
            break;
        }
    }
    return "after";
}

auto sub fe_inert_body_throws() {
    foreach int x in (li[3, 0..2]) {
        use("body", x);
        if (x == 11) {
            throw "BODY-ERROR";
        }
    }
}

auto sub map_inert() {
    return map use("map", $1 + 1), li[0, 2..4, 7];
}

auto sub select_inert() {
    return select lf[4..0], use("select", $1) > 0;
}

auto sub foldl_inert() {
    return foldl use("fold", $1 + $2), li[5..0, 3];
}

# a pure expression keeps the eager path of a list of selectors
auto sub map_inert_pure() {
    return map $1 * 3, li[0, 2..4];
}

# --- functional and streaming operators ---

int sub g(int x) {
    return x + 1;
}

auto sub map_int() {
    return map use("map " + $#, $1 * 2), li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub map_typed_call() {
    return map g($1), li[f("a", 0), f("b", 5)..f("c", 2)];
}

auto sub map_typed_call_nothing() {
    return map g($1), li[f("a", 0), f("b", 5)..f("c", 7)];
}

auto sub map_float() {
    return map use("map", $1 * 2), lf[f("a", 4)..f("b", 0)];
}

auto sub map_bool() {
    return map use("map", !$1), lb[f("a", 0)..f("b", 2), f("c", 3)];
}

auto sub map_big() {
    return map use("map", $1), lbig[f("a", 3)..f("b", 0)];
}

auto sub map_or_nothing() {
    return map use("map", $1 ?? -1), ln[f("a", 2)..f("b", 0), f("c", 4)];
}

auto sub map_select() {
    return map use("map", $1), li[f("a", 0), f("b", 1)..f("c", 3)], use("select", $1) % 2;
}

auto sub map_body_appends() {
    list<int> m = (1, 2, 3);
    return map use("map", $1 + (m += 1).size()), m[f("a", 0)..f("b", 4)];
}

auto sub select_int() {
    return select li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)], use("select", $1) % 2;
}

auto sub select_float() {
    return select lf[f("a", 0)..f("b", 4)], use("select", $1) > 0;
}

auto sub select_or_nothing() {
    return select ln[f("a", 0), f("b", 1)..f("c", 2)], use("select", $1) != 3;
}

# a list of values of any type is iterated with the iterator, whose values can be NOTHING
auto sub select_auto() {
    list<auto> la = (1, "b", NOTHING, 4.5);
    return select la[f("a", 3), f("b", 0)..f("c", 2)], use("select", $1) != "b";
}

auto sub foldl_int() {
    return foldl use("fold", $1 + $2), li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub foldl_float() {
    return foldl use("fold", $1 + $2), lf[f("a", 0), f("b", 2)..f("c", 4)];
}

auto sub foldl_one() {
    return foldl use("fold", $1 + $2), li[f("a", 3)..f("b", 3)];
}

auto sub foldl_none() {
    return foldl use("fold", $1 + $2), li[f("a", 7)..f("b", 9), f("c", 8)];
}

#! uses a value; raises an exception for 12
auto sub use_raise(auto v) {
    if (v == 12) {
        throw "USE-ERROR", sprintf("at %y", v);
    }
    return use("use", v);
}

auto sub map_throws() {
    return map use_raise($1), li[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub foldl_throws() {
    return foldl $1 + use_raise($2), li[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub map_start_nothing() {
    return map use("map", $1), li[f("a", 0), k..f("c", 3)];
}

auto sub map_end_nothing() {
    return map use("map", $1), li[f("a", 0), f("b", 1)..k];
}

auto sub take_int() {
    return take 3, li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub drop_int() {
    return drop 2, li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub takewhile_int() {
    return takewhile use("while", $1) < 12, li[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub first_int() {
    return first use("first", $1) > 11, li[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub count_int() {
    return count li[f("a", 0), f("b", 1)..f("c", 3), f("d", 9)];
}

auto sub map_nested() {
    return map (map use("inner", $1 * 10), li[f("a", 0)..f("b", 1), f("e", 5)]), li[f("c", 2), f("d", 3)..f("f", 4)];
}

# the same case run many times: native code must not leak or reuse the cursor of an earlier iteration
auto sub repeated() {
    int t = 0;
    for (int i = 0; i < 200; ++i) {
        t += foldl $1 + $2, li[i % 6, (i % 3)..5];
    }
    return t;
}

sub main() {
    t("fe int", \fe_int());
    t("fe int descending", \fe_int_descending());
    t("fe out of range", \fe_out_of_range());
    t("fe negative", \fe_negative());
    t("fe empty selectors", \fe_empty_selectors());
    t("fe big", \fe_big());
    t("fe float", \fe_float());
    t("fe bool", \fe_bool());
    t("fe or nothing", \fe_or_nothing());
    t("fe typed nothing", \fe_typed_nothing());
    t("fe break", \fe_break());
    t("fe continue", \fe_continue());
    t("fe return", \fe_return());
    t("fe body throws", \fe_body_throws());
    t("fe start nothing", \fe_start_nothing());
    t("fe end nothing", \fe_end_nothing());
    t("fe selector throws try", \fe_selector_throws_try());
    t("fe nested", \fe_nested());
    t("fe body appends", \fe_body_appends());
    t("fe body splices", \fe_body_splices());
    t("fe body sets", \fe_body_sets());
    t("fe inert", \fe_inert());
    t("fe inert locals", \fe_inert_locals());
    t("fe inert local written", \fe_inert_local_written());
    t("fe inert break", \fe_inert_break());
    t("fe inert body throws", \fe_inert_body_throws());
    t("map inert", \map_inert());
    t("select inert", \select_inert());
    t("foldl inert", \foldl_inert());
    t("map inert pure", \map_inert_pure());
    t("map int", \map_int());
    t("map typed call", \map_typed_call());
    t("map typed call nothing", \map_typed_call_nothing());
    t("map float", \map_float());
    t("map bool", \map_bool());
    t("map big", \map_big());
    t("map or nothing", \map_or_nothing());
    t("map select", \map_select());
    t("map body appends", \map_body_appends());
    t("select int", \select_int());
    t("select float", \select_float());
    t("select or nothing", \select_or_nothing());
    t("select auto", \select_auto());
    t("foldl int", \foldl_int());
    t("foldl float", \foldl_float());
    t("foldl one", \foldl_one());
    t("foldl none", \foldl_none());
    t("map throws", \map_throws());
    t("foldl throws", \foldl_throws());
    t("map start nothing", \map_start_nothing());
    t("map end nothing", \map_end_nothing());
    t("take int", \take_int());
    t("drop int", \drop_int());
    t("takewhile int", \takewhile_int());
    t("first int", \first_int());
    t("count int", \count_int());
    t("map nested", \map_nested());
    t("repeated", \repeated());
}
