%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.
# folds over typed lists whose code adds, subtracts or multiplies $1 and $2: the IR lowering gives them the typed
# fold operations only when $2 has the element type at parse time; see implicit-argument-types.qtest
list<int> tl = (1, 2, 3);
list<float> fl = (1.5, 2.5);
int sum = foldl $1 + $2, tl;
int diff = foldl $1 - $2, tl;
int prod = foldl $1 * $2, tl;
float fsum = foldl $1 + $2, fl;
float fdiff = foldl $1 - $2, fl;
float fprod = foldl $1 * $2, fl;
printf("%d %d %d %y %y %y\n", sum, diff, prod, fsum, fdiff, fprod);
