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
    string tag() { return n; }
}

int sub thrower(int i) {
    if (i == 2) {
        throw "ERR";
    }
    return i;
}

# A constructor declaration gives its object to the local. Once the first operand has used that
# object, replacing the local can run its destructor before another side effect in the same statement.
sub constructor_condition() {
    int other = 0;
    reference<int> ref = \other;
    printf("constructor args %d %d\n", P p("condition-old") ? 1 : 0, (p = new P("condition-new")) ? 1 : 0);
}

int sub constructor_marker() {
    printf("constructor pipeline\n");
    return 0;
}

sub local_condition() {
    try {
        P p("local-old");
        printf("local args %d %d\n", p ? 1 : 0, (p = new P("local-new")) ? 1 : 0);
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
}

sub print_pair(P a, P b) {
    printf("held %s %s\n", a.tag(), b.tag());
}

sub constructor_argument() {
    # The truthy arm of ?: keeps the object for the call across evaluation of the second argument.
    print_pair((P p("held-old") ?: new P("fallback")), p = new P("held-new"));
}

sub constructor_pipeline() {
    int n = int(first (map 1, P p("pipeline-old"))) + ((p = new P("pipeline-new")) ? 1 : 0) + constructor_marker();
}

printf("top args %d %d\n", P top("top-old") ? 1 : 0, (top = new P("top-new")) ? 1 : 0);

sub captured_condition() {
    printf("captured args %d %d\n", P p("captured-old") ? 1 : 0, (p = new P("captured-new")) ? 1 : 0);
    code capture = sub () { printf("captured %s\n", p.tag()); };
    capture();
}

for (int i = 0; i < 2; ++i) {
    constructor_condition();
    constructor_pipeline();
    local_condition();
    constructor_argument();
    captured_condition();
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
