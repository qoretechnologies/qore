#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of MapStageTempDtorException.qtest: a destructor of a temporary of a map, select or fold expression raises
# an exception; the expression is in a try statement, at the top level (which jit compiles) and in a function, and the
# exception leaves a function; each is caught, with the exceptions of the destructors of the other elements chained
# (qore#6072)

%modern

class E {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { throw "DTOR-" + n; }
}

E sub mk(string n) { return new E(n); }
list sub mkl() { return (1, 2); }
class P3 { private { string n; } constructor(string n) { self.n = n; } }
int y = 5;

#! returns the errors of an exception and of the exceptions chained to it, failing if one is chained to itself
string sub errs(hash<ExceptionInfo> ex) {
    list<string> l = ();
    *hash<ExceptionInfo> e = ex;
    int n = 0;
    while (e) {
        l += e.err;
        if (++n > 20) {
            return "LOOP";
        }
        e = e.next;
    }
    return l.join(",");
}

auto x;

# a temporary whose destructor raises nothing is released before the try statement's catch block runs when a call of
# the same element raises an exception, also in a select stage that a map stage follows
class P {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { printf("~%s\n", n); }
}
int sub thrower(int i) { if (i == 2) { throw "ERR"; } return i; }
try {
    x = (map $1 * 2, (select (1, 2, 3), (new P("p" + $1), $1)[1] > 0 && thrower($1)));
    printf("order map-select: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("order map-select: caught %s\n", ex.err);
}
try {
    x = (map ((new P("q" + $1), $1)[1] + thrower($1)), (1, 2, 3));
    printf("order map: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("order map: caught %s\n", ex.err);
}
try { x = (map (new E("l" + $1), $1)[1], (1, 2)); printf("top list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top list: %s\n", errs(ex)); }
try { x = (map {"e": new E("h" + $1), "v": $1}.v, (1, 2)); printf("top hash: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top hash: %s\n", errs(ex)); }
try { x = (map {"e" + $1: new E("k" + $1), "v": $1}.v, (1, 2)); printf("top hash-keys: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top hash-keys: %s\n", errs(ex)); }
try { x = (map (mk("c" + $1), $1)[1], (1, 2)); printf("top call: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top call: %s\n", errs(ex)); }
try { x = (select (1, 2), (new E("s" + $1), $1)[1] > 0); printf("top select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top select: %s\n", errs(ex)); }
try { x = (map $1 * 2, (select (1, 2, 3), (new E("ms" + $1), $1)[1] > 0)); printf("top map-select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top map-select: %s\n", errs(ex)); }
try { x = (foldl $1 + (new E("f" + $2), $2)[1], (1, 2, 3)); printf("top foldl: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top foldl: %s\n", errs(ex)); }
try { x = (foldr $1 + (new E("r" + $2), $2)[1], (1, 2, 3)); printf("top foldr: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top foldr: %s\n", errs(ex)); }
try { x = (map (map (new E("n" + $1), $1)[1], (1, 2)), (1, 2)); printf("top nested: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top nested: %s\n", errs(ex)); }
try { x = (map ($1 == 1 ? (new E("a"), $1)[1] : $1), (1, 2, 3)); printf("top first: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top first: %s\n", errs(ex)); }
try { x = (map ($1 == 2 ? (new E("m"), $1)[1] : $1), (1, 2, 3)); printf("top middle: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top middle: %s\n", errs(ex)); }
try { x = (map ($1 == 3 ? (new E("z"), $1)[1] : $1), (1, 2, 3)); printf("top last: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top last: %s\n", errs(ex)); }
try { x = (map (new E("o" + $1), $1)[1], (1,)); printf("top single: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top single: %s\n", errs(ex)); }
try { x = (map (new E("t" + $1), $1)[1], (1, 2, 3)); printf("top three: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top three: %s\n", errs(ex)); }

try { x = (map (new E("al" + $1), 1).size() + (y, 3).size(), (1, 2)); printf("top adjacent-list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top adjacent-list: %s\n", errs(ex)); }
try { x = (map (new E("ah" + $1), 1).size() + {"a": y}.size(), (1, 2)); printf("top adjacent-hash: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top adjacent-hash: %s\n", errs(ex)); }
try { x = (map (new E("ac" + $1), 1).size() + mkl().size(), (1, 2)); printf("top adjacent-call: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top adjacent-call: %s\n", errs(ex)); }
try { x = (map (new E("a3" + $1), 1).size() + (y, 3).size() + (y, 4).size(), (1, 2)); printf("top adjacent-three: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top adjacent-three: %s\n", errs(ex)); }
try { x = (foldl $1 + (new E("fa" + $2), 1).size() + (y, 2).size(), (1, 2, 3)); printf("top fold-adjacent: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top fold-adjacent: %s\n", errs(ex)); }
try { x = (map (new E("mo" + $1), 1).size(), (select (1, 2, 3), $1 < 3)); printf("top map-over-select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top map-over-select: %s\n", errs(ex)); }
try { x = (map {"e": new E("h3" + $1), "p": new P3("p" + $1), "v": $1}.v, (1, 2, 3)); printf("top hash-3-keys: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top hash-3-keys: %s\n", errs(ex)); }

auto sub f_list() { try { return (map (new E("l" + $1), $1)[1], (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_list() { return (map (new E("l" + $1), $1)[1], (1, 2)); }
auto sub f_hash() { try { return (map {"e": new E("h" + $1), "v": $1}.v, (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_hash() { return (map {"e": new E("h" + $1), "v": $1}.v, (1, 2)); }
auto sub f_hash_keys() { try { return (map {"e" + $1: new E("k" + $1), "v": $1}.v, (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_hash_keys() { return (map {"e" + $1: new E("k" + $1), "v": $1}.v, (1, 2)); }
auto sub f_call() { try { return (map (mk("c" + $1), $1)[1], (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_call() { return (map (mk("c" + $1), $1)[1], (1, 2)); }
auto sub f_select() { try { return (select (1, 2), (new E("s" + $1), $1)[1] > 0); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_select() { return (select (1, 2), (new E("s" + $1), $1)[1] > 0); }
auto sub f_map_select() { try { return (map $1 * 2, (select (1, 2, 3), (new E("ms" + $1), $1)[1] > 0)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_map_select() { return (map $1 * 2, (select (1, 2, 3), (new E("ms" + $1), $1)[1] > 0)); }
auto sub f_foldl() { try { return (foldl $1 + (new E("f" + $2), $2)[1], (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_foldl() { return (foldl $1 + (new E("f" + $2), $2)[1], (1, 2, 3)); }
auto sub f_foldr() { try { return (foldr $1 + (new E("r" + $2), $2)[1], (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_foldr() { return (foldr $1 + (new E("r" + $2), $2)[1], (1, 2, 3)); }
auto sub f_nested() { try { return (map (map (new E("n" + $1), $1)[1], (1, 2)), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_nested() { return (map (map (new E("n" + $1), $1)[1], (1, 2)), (1, 2)); }
auto sub f_first() { try { return (map ($1 == 1 ? (new E("a"), $1)[1] : $1), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_first() { return (map ($1 == 1 ? (new E("a"), $1)[1] : $1), (1, 2, 3)); }
auto sub f_middle() { try { return (map ($1 == 2 ? (new E("m"), $1)[1] : $1), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_middle() { return (map ($1 == 2 ? (new E("m"), $1)[1] : $1), (1, 2, 3)); }
auto sub f_last() { try { return (map ($1 == 3 ? (new E("z"), $1)[1] : $1), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_last() { return (map ($1 == 3 ? (new E("z"), $1)[1] : $1), (1, 2, 3)); }
auto sub f_single() { try { return (map (new E("o" + $1), $1)[1], (1,)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_single() { return (map (new E("o" + $1), $1)[1], (1,)); }
auto sub f_three() { try { return (map (new E("t" + $1), $1)[1], (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_three() { return (map (new E("t" + $1), $1)[1], (1, 2, 3)); }

auto sub f_adjacent_list() { try { return (map (new E("al" + $1), 1).size() + (y, 3).size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_adjacent_list() { return (map (new E("al" + $1), 1).size() + (y, 3).size(), (1, 2)); }
auto sub f_adjacent_hash() { try { return (map (new E("ah" + $1), 1).size() + {"a": y}.size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_adjacent_hash() { return (map (new E("ah" + $1), 1).size() + {"a": y}.size(), (1, 2)); }
auto sub f_adjacent_call() { try { return (map (new E("ac" + $1), 1).size() + mkl().size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_adjacent_call() { return (map (new E("ac" + $1), 1).size() + mkl().size(), (1, 2)); }
auto sub f_adjacent_three() { try { return (map (new E("a3" + $1), 1).size() + (y, 3).size() + (y, 4).size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_adjacent_three() { return (map (new E("a3" + $1), 1).size() + (y, 3).size() + (y, 4).size(), (1, 2)); }
auto sub f_fold_adjacent() { try { return (foldl $1 + (new E("fa" + $2), 1).size() + (y, 2).size(), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_fold_adjacent() { return (foldl $1 + (new E("fa" + $2), 1).size() + (y, 2).size(), (1, 2, 3)); }
auto sub f_map_over_select() { try { return (map (new E("mo" + $1), 1).size(), (select (1, 2, 3), $1 < 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_map_over_select() { return (map (new E("mo" + $1), 1).size(), (select (1, 2, 3), $1 < 3)); }
auto sub f_hash_3_keys() { try { return (map {"e": new E("h3" + $1), "p": new P3("p" + $1), "v": $1}.v, (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_hash_3_keys() { return (map {"e": new E("h3" + $1), "p": new P3("p" + $1), "v": $1}.v, (1, 2, 3)); }

# the value of a Phi that holds a container of the inner expression, which no later instruction of the body checks;
# the result of a call; a fold's accumulator, which holds an object
try { x = (map (map new E("ns" + $1), (1, 2)).size() + (y, 1).size(), (1, 2)); printf("top nested-size: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top nested-size: %s\n", errs(ex)); }
try { x = (map (new list<auto>((new E("cl" + $1), 1))).size() + (y, 1).size(), (1, 2)); printf("top complex-list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top complex-list: %s\n", errs(ex)); }
try { x = (foldl $1 + (new list<auto>((new E("fc" + $2), 1))).size(), (1, 2, 3)); printf("top fold-complex: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top fold-complex: %s\n", errs(ex)); }
try { x = (foldl (new E("ac" + $2), $1)[1], (1, 2, 3)); printf("top fold-object: none\n"); } catch (hash<ExceptionInfo> ex) { printf("top fold-object: %s\n", errs(ex)); }
auto sub f_nested_size() { try { return (map (map new E("ns" + $1), (1, 2)).size() + (y, 1).size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_nested_size() { return (map (map new E("ns" + $1), (1, 2)).size() + (y, 1).size(), (1, 2)); }
auto sub f_complex_list() { try { return (map (new list<auto>((new E("cl" + $1), 1))).size() + (y, 1).size(), (1, 2)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_complex_list() { return (map (new list<auto>((new E("cl" + $1), 1))).size() + (y, 1).size(), (1, 2)); }
auto sub f_fold_complex() { try { return (foldl $1 + (new list<auto>((new E("fc" + $2), 1))).size(), (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_fold_complex() { return (foldl $1 + (new list<auto>((new E("fc" + $2), 1))).size(), (1, 2, 3)); }
auto sub f_fold_object() { try { return (foldl (new E("ac" + $2), $1)[1], (1, 2, 3)); } catch (hash<ExceptionInfo> ex) { return errs(ex); } }
auto sub p_f_fold_object() { return (foldl (new E("ac" + $2), $1)[1], (1, 2, 3)); }

for (int i = 0; i < 2; ++i) {
    printf("function list: %y\n", f_list());
    try { p_f_list(); printf("leaves list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves list: %s\n", errs(ex)); }
    printf("function hash: %y\n", f_hash());
    try { p_f_hash(); printf("leaves hash: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves hash: %s\n", errs(ex)); }
    printf("function hash-keys: %y\n", f_hash_keys());
    try { p_f_hash_keys(); printf("leaves hash-keys: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves hash-keys: %s\n", errs(ex)); }
    printf("function call: %y\n", f_call());
    try { p_f_call(); printf("leaves call: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves call: %s\n", errs(ex)); }
    printf("function select: %y\n", f_select());
    try { p_f_select(); printf("leaves select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves select: %s\n", errs(ex)); }
    printf("function map-select: %y\n", f_map_select());
    try { p_f_map_select(); printf("leaves map-select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves map-select: %s\n", errs(ex)); }
    printf("function foldl: %y\n", f_foldl());
    try { p_f_foldl(); printf("leaves foldl: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves foldl: %s\n", errs(ex)); }
    printf("function foldr: %y\n", f_foldr());
    try { p_f_foldr(); printf("leaves foldr: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves foldr: %s\n", errs(ex)); }
    printf("function nested: %y\n", f_nested());
    try { p_f_nested(); printf("leaves nested: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves nested: %s\n", errs(ex)); }
    printf("function first: %y\n", f_first());
    try { p_f_first(); printf("leaves first: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves first: %s\n", errs(ex)); }
    printf("function middle: %y\n", f_middle());
    try { p_f_middle(); printf("leaves middle: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves middle: %s\n", errs(ex)); }
    printf("function last: %y\n", f_last());
    try { p_f_last(); printf("leaves last: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves last: %s\n", errs(ex)); }
    printf("function single: %y\n", f_single());
    try { p_f_single(); printf("leaves single: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves single: %s\n", errs(ex)); }
    printf("function three: %y\n", f_three());
    try { p_f_three(); printf("leaves three: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves three: %s\n", errs(ex)); }
    printf("function adjacent-list: %y\n", f_adjacent_list());
    try { p_f_adjacent_list(); printf("leaves adjacent-list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves adjacent-list: %s\n", errs(ex)); }
    printf("function adjacent-hash: %y\n", f_adjacent_hash());
    try { p_f_adjacent_hash(); printf("leaves adjacent-hash: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves adjacent-hash: %s\n", errs(ex)); }
    printf("function adjacent-call: %y\n", f_adjacent_call());
    try { p_f_adjacent_call(); printf("leaves adjacent-call: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves adjacent-call: %s\n", errs(ex)); }
    printf("function adjacent-three: %y\n", f_adjacent_three());
    try { p_f_adjacent_three(); printf("leaves adjacent-three: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves adjacent-three: %s\n", errs(ex)); }
    printf("function fold-adjacent: %y\n", f_fold_adjacent());
    try { p_f_fold_adjacent(); printf("leaves fold-adjacent: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves fold-adjacent: %s\n", errs(ex)); }
    printf("function map-over-select: %y\n", f_map_over_select());
    try { p_f_map_over_select(); printf("leaves map-over-select: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves map-over-select: %s\n", errs(ex)); }
    printf("function hash-3-keys: %y\n", f_hash_3_keys());
    try { p_f_hash_3_keys(); printf("leaves hash-3-keys: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves hash-3-keys: %s\n", errs(ex)); }
    printf("function nested-size: %y\n", f_nested_size());
    try { p_f_nested_size(); printf("leaves nested-size: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves nested-size: %s\n", errs(ex)); }
    printf("function complex-list: %y\n", f_complex_list());
    try { p_f_complex_list(); printf("leaves complex-list: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves complex-list: %s\n", errs(ex)); }
    printf("function fold-complex: %y\n", f_fold_complex());
    try { p_f_fold_complex(); printf("leaves fold-complex: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves fold-complex: %s\n", errs(ex)); }
    printf("function fold-object: %y\n", f_fold_object());
    try { p_f_fold_object(); printf("leaves fold-object: none\n"); } catch (hash<ExceptionInfo> ex) { printf("leaves fold-object: %s\n", errs(ex)); }
}
