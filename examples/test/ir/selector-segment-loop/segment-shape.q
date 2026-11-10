%modern
# the functions whose native code selector-segment-loop.qtest checks: a list of selectors over a list of int, float
# or bool values is iterated with the selector-segment loop (seg_*), any other list with the iterator (iter_*); stable
# selectors with more single indices than ranges are evaluated into a list first (list_*)

# Copyright (C) 2026 Qore Technologies, s.r.o.

list<int> li = (1, 2, 3, 4);
list<*int> lni = (1, NOTHING, 3);
list<float> lf = (1.5, 2.5, 3.5);
list<bool> lb = (True, False);
list<string> ls = ("a", "b", "c");
list<auto> la = (1, "b", 3.0);

int sub f(int i) {
    return i;
}

float sub ff(float v) {
    return v;
}

int sub seg_foreach_int() {
    int t = 0;
    foreach int x in (li[f(0), f(1)..f(3)]) {
        t += x;
    }
    return t;
}

list<int> sub seg_map_int() {
    return map $1 + f(1), li[f(0), f(1)..f(3)];
}

list<*int> sub seg_select_or_nothing() {
    return select lni[f(0), f(1)..f(2)], f(1);
}

float sub seg_foldl_float() {
    return foldl ff($1) + $2, lf[f(0), f(1)..f(2)];
}

list<bool> sub seg_map_bool() {
    return map !$1, lb[f(0), f(1)..f(1)];
}

int sub seg_foreach_stable() {
    int n = f(1);
    int t = 0;
    foreach int x in (li[0, n..3]) {
        t += x;
    }
    return t;
}

list<int> sub list_map_stable_indices() {
    int n = f(3);
    return map $1 * 2, li[0, 1, 2, n..3];
}

list<string> sub iter_map_string() {
    return map $1 + "x", ls[f(0), f(1)..f(2)];
}

int sub iter_foreach_auto() {
    int t = 0;
    foreach auto x in (la[f(0), f(1)..f(2)]) {
        t += exists x ? 1 : 0;
    }
    return t;
}

printf("%y\n", (seg_foreach_int(), seg_map_int(), seg_select_or_nothing(), seg_foldl_float(), seg_map_bool(),
    seg_foreach_stable(), list_map_stable_indices(), iter_map_string(), iter_foreach_auto()));
