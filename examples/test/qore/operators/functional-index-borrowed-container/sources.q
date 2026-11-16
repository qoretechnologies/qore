%modern
# the cases of functional-index-borrowed-container.qtest (qore#6204): [] as the source of a functional operator reads
# its container from a variable, and its index assigns the variable another value: the operator reads the container
# that the variable held when it was read, as the [] operator does

# Copyright (C) 2026 Qore Technologies, s.r.o.

sub main() {
    list<auto> l = map $1 + 100, (1, 2);
    printf("map index: %y\n", (map $1, l[(l = (3, 4)) ? 0 : 0]));
    l = map $1 + 100, (1, 2);
    printf("map list: %y\n", (map $1, l[((l = (3, 4)) ? 0 : 0, 1)]));
    l = map $1 + 100, (1, 2);
    printf("map range: %y\n", (map $1, l[(((l = (3, 4)) ? 0 : 0) .. 1, 0)]));
    # a string with a range: the operator gives the substrings before an iterator is made
    string str = sprintf("%s%s", "a", "b");
    printf("map string range: %y\n", (map $1, str[(((str = sprintf("x%s", "y")) ? 0 : 0) .. 1, 0)]));
    str = sprintf("%s%s", "a", "b");
    printf("plain string range: %y\n", str[(((str = sprintf("x%s", "y")) ? 0 : 0) .. 1, 0)]);
    l = map $1 + 100, (1, 2);
    printf("select: %y\n", (select l[((l = (3, 4)) ? 0 : 0, 1)], $1 > 101));
    l = map $1 + 100, (1, 2);
    list<auto> f = ();
    foreach auto v in (l[((l = (3, 4)) ? 0 : 0, 1)]) {
        f += v;
    }
    printf("foreach: %y\n", f);
    # the [] operator, which already held its own reference
    l = map $1 + 100, (1, 2);
    printf("plain: %y\n", l[(l = (3, 4)) ? 0 : 0]);
    printf("variable after: %y\n", l);
}

main();
