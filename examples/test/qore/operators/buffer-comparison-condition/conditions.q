%modern
# the cases of buffer-comparison-condition.qtest (qore#6160): a comparison whose operands can be buffers gives the
# buffer of the elementwise comparison, which is true when it is not empty, as the condition of a statement

# Copyright (C) 2026 Qore Technologies, s.r.o.

#! runs each condition with a buffer and an int, which the parser types as auto
sub conditions(string label, auto b, auto x) {
    printf("%s:\n", label);
    int n = 0;
    if (b > x) {
        n = 1;
    }
    printf("  if: %d\n", n);
    n = 0;
    while (b > x) {
        if (++n == 2) {
            break;
        }
    }
    printf("  while: %d\n", n);
    n = 0;
    do {
        if (++n == 3) {
            break;
        }
    } while (b > x);
    printf("  do: %d\n", n);
    n = 0;
    for (int i = 0; b > x; ++i) {
        if (++n == 4) {
            break;
        }
    }
    printf("  for: %d\n", n);
    n = 0;
    if (b == x) {
        n = 1;
    }
    printf("  if eq: %d\n", n);
    n = 0;
    if (b != x) {
        n = 1;
    }
    printf("  if ne: %d\n", n);
    n = 0;
    if (b < x) {
        n = 1;
    }
    printf("  if lt: %d\n", n);
    n = 0;
    if (b <= x) {
        n = 1;
    }
    printf("  if le: %d\n", n);
    n = 0;
    if (b >= x) {
        n = 1;
    }
    printf("  if ge: %d\n", n);
    printf("  ternary: %s\n", (b > x) ? "t" : "f");
    # a comparison as a branch of ?:, which the parser types as bool
    n = 0;
    if (n == 0 ? b > x : False) {
        n = 1;
    }
    printf("  if ternary: %d\n", n);
    n = 0;
    while (n < 5 ? b > x : False) {
        ++n;
    }
    printf("  while ternary: %d\n", n);
    # a comparison with the value of such a comparison: buffer == True is elementwise as well
    n = 0;
    if ((b > x) == True) {
        n = 1;
    }
    printf("  if cmp eq: %d\n", n);
    n = 0;
    if ((b > x) != False) {
        n = 1;
    }
    printf("  if cmp ne: %d\n", n);
}

conditions("buffer", new buffer<int64>((1, 2, 3)), 1);
conditions("buffer of zeros", new buffer<int64>((0, 0, 0)), 1);
conditions("empty buffer", new buffer<int64>(()), 1);
conditions("int", 2, 1);
conditions("int false", 0, 1);
