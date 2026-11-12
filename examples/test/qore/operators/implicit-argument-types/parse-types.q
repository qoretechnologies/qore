%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the parse-time types of $1 and $2 in folds and maps: prints the error of parsing each case in a new %modern
# Program, or "ok"; see implicit-argument-types.qtest

const Cases = (
    # $2 of a fold has the element type: the result of adding two ints is not a string
    "list<int> tl = (1, 2); string s = foldl $1 + $2, tl;",
    "list<int> tl = (1, 2); string s = foldr $1 + $2, tl;",
    # $2 of a map has no type: a list built from it is not typed with the element type
    "list<int> tl = (1, 2); list<list<auto>> l = map ($2,), tl;",
    # $1 of a map has the element type
    "list<int> tl = (1, 2); list<string> l = map $1, tl;",
);

foreach string code in (Cases) {
    Program p(PO_NEW_STYLE | PO_STRICT_TYPES | PO_REQUIRE_TYPES);
    string rv;
    try {
        p.parse(code, "parse-types");
        rv = "ok";
    } catch (hash<ExceptionInfo> ex) {
        rv = ex.err;
    }
    printf("%s: %s\n", code, rv);
}
