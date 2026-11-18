%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the parse-time types of the implicit arguments: $1 has the type of the element that an operator gives its code;
# $2 has the type of the next element in the code of foldl and foldr, and no type in the code of every other
# operator; the following arguments have no type; see implicit-argument-types.qtest

string sub fmt(auto v) { return sprintf("%y", v); }

sub maps(list<int> tl, list<string> sl) {
    printf("map ($2,): %s\n", fmt(map ($2,), tl));
    printf("map ($1, $2): %s\n", fmt(map ($1, $2), tl));
    printf("map ($3, $1): %s\n", fmt(map ($3, $1), tl));
    printf("map ($2,) string: %s\n", fmt(map ($2,), sl));
    printf("select ($2,): %s\n", fmt(select tl, ($2,)[0] === NOTHING));
    printf("map-select ($2, $1): %s\n", fmt(map ($2, $1), tl, ($2,)[0] === NOTHING));
    printf("new list $2: %s\n", fmt(map (new list<auto>(($2,)))[0], tl));
    printf("new list $1: %s\n", fmt(map (new list<auto>(($1,)))[0], tl));
    printf("new list $1 $2: %s\n", fmt(map (new list<auto>(($1, $2)))[0], tl));
    printf("new hash $2: %s\n", fmt(map new hash<string, auto>({"a": $2}), tl));
    list<list<int>> l = map ($1,), tl;
    printf("$1 element type: %s %s\n", fmt(l), (map ($1,), tl)[0].fullType());
}

sub folds(list<int> tl, list<float> fl, list<string> sl) {
    int r = foldl $1 + $2, tl;
    printf("foldl sum int: %d\n", r);
    printf("foldl diff int: %d\n", foldl $1 - $2, tl);
    printf("foldl prod int: %d\n", foldl $1 * $2, tl);
    float f = foldl $1 + $2, fl;
    printf("foldl sum float: %y\n", f);
    printf("foldr sum int: %d\n", foldr $1 + $2, tl);
    printf("foldr diff int: %d\n", foldr $1 - $2, tl);
    printf("foldl ($2,): %s\n", fmt(foldl $1 + ($2,), tl));
    printf("foldl string: %s\n", foldl $1 + "/" + $2, sl);
    printf("foldl max: %d\n", foldl max($1, $2), tl);
    # a map in the code of a fold gives its own $2, which has no value and no type
    printf("foldl nested map: %d\n", foldl $1 + $2 + (map ($2,), (5,)).size(), tl);
    # a fold in the code of a map
    printf("map nested foldl: %s\n", fmt(map (foldl $1 + $2, (1, $1)), tl));
}

maps((1, 2), ("a", "b"));
folds((1, 2, 3), (1.5, 2.5), ("a", "b", "c"));
