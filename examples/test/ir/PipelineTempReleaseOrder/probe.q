#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of PipelineTempReleaseOrder.qtest: a call of an element of a pipeline (a select stage that map, fold and
# first stages follow) raises an exception in a try statement; the temporaries of the element are released before the
# catch block runs, as with the AST interpreter (qore#6072)

%modern

class P {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { printf("~%s\n", n); }
}

int sub thrower(int i) {
    if (i == 2) {
        throw "ERR";
    }
    return i;
}

auto x;

# at the top level, which jit compiles
try {
    x = (map $1 * 2, (select (1, 2, 3), (new P("ms" + $1), $1)[1] > 0 && thrower($1)));
    printf("top map-select: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top map-select: caught %s\n", ex.err);
}
try {
    x = (map $1 + 1, (map $1 * 2, (select (1, 2, 3), (new P("mm" + $1), $1)[1] > 0 && thrower($1))));
    printf("top map-select-map: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top map-select-map: caught %s\n", ex.err);
}
try {
    x = (foldl $1 + $2, (select (1, 2, 3), (new P("fs" + $1), $1)[1] > 0 && thrower($1)));
    printf("top foldl-select: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top foldl-select: caught %s\n", ex.err);
}
try {
    x = (map $1 * 2, (select (1, 2, 3), (new P("fi" + $1), $1)[1] > 0 && thrower($1))).first();
    printf("top first: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top first: caught %s\n", ex.err);
}
try {
    x = ((map $1, (select (1, 2, 3), (new P("co" + $1), $1)[1] > 0 && thrower($1))) ? 1 : 0);
    printf("top condition: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top condition: caught %s\n", ex.err);
}
try {
    x = (map ((new P("pm" + $1), $1)[1] + thrower($1)), (1, 2, 3));
    printf("top plain-map: none\n");
} catch (hash<ExceptionInfo> ex) {
    printf("top plain-map: caught %s\n", ex.err);
}

# in functions, which tiered and qcc compile, and in a loop
sub f_map_select() {
    try {
        auto v = (map $1 * 2, (select (1, 2, 3), (new P("ms" + $1), $1)[1] > 0 && thrower($1)));
        printf("function map-select: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function map-select: caught %s\n", ex.err);
    }
}
sub f_map_select_map() {
    try {
        auto v = (map $1 + 1, (map $1 * 2, (select (1, 2, 3), (new P("mm" + $1), $1)[1] > 0 && thrower($1))));
        printf("function map-select-map: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function map-select-map: caught %s\n", ex.err);
    }
}
sub f_foldl_select() {
    try {
        auto v = (foldl $1 + $2, (select (1, 2, 3), (new P("fs" + $1), $1)[1] > 0 && thrower($1)));
        printf("function foldl-select: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function foldl-select: caught %s\n", ex.err);
    }
}
sub f_first() {
    try {
        auto v = (map $1 * 2, (select (1, 2, 3), (new P("fi" + $1), $1)[1] > 0 && thrower($1))).first();
        printf("function first: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function first: caught %s\n", ex.err);
    }
}
sub f_condition() {
    try {
        auto v = ((map $1, (select (1, 2, 3), (new P("co" + $1), $1)[1] > 0 && thrower($1))) ? 1 : 0);
        printf("function condition: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function condition: caught %s\n", ex.err);
    }
}
sub f_plain_map() {
    try {
        auto v = (map ((new P("pm" + $1), $1)[1] + thrower($1)), (1, 2, 3));
        printf("function plain-map: none\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("function plain-map: caught %s\n", ex.err);
    }
}

sub in_loop() {
    foreach int n in (1, 2) {
        try {
            auto v = (map $1 * n, (select (1, 2, 3), (new P("lo" + n + $1), $1)[1] > 0 && thrower($1)));
            printf("loop %d: none\n", n);
        } catch (hash<ExceptionInfo> ex) {
            printf("loop %d: caught %s\n", n, ex.err);
        }
    }
}

for (int i = 0; i < 2; ++i) {
    f_map_select();
    f_map_select_map();
    f_foldl_select();
    f_first();
    f_condition();
    f_plain_map();
    in_loop();
}
