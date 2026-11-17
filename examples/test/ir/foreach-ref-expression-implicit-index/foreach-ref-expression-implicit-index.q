%modern
%disable-warning unreferenced-variable
# $# in the reference expression of a foreach over a reference nested in a foreach is the outer loop's index; see
# foreach-ref-expression-implicit-index.qtest (qore#6424)
# Copyright (C) 2026 Qore Technologies, s.r.o.

list log;

# qore#6424: the reference expression is evaluated when the loop starts and when it writes the list back
sub ref_expression() {
    list mm = ((0, 0), (0, 0), (0, 0));
    foreach auto x in ((1, 2, 3)) {
        foreach auto y in (\mm[$#]) {
            y = x;
        }
    }
    log += "ref expression " + sprintf("%y", mm);

    hash h = {"0": (0,), "1": (0,)};
    foreach string k in (("a", "b")) {
        foreach auto y in (\h{string($#)}) {
            y = k;
        }
    }
    log += "ref expression hash " + sprintf("%y", h);

    list mm2 = ((0,), (0,));
    foreach auto x in ((1, 2)) {
        foreach auto y in (\mm2[$#]) {
            y = x;
            break;
        }
    }
    log += "ref expression break " + sprintf("%y", mm2);
}
ref_expression();

# a return from the inner loop writes the list back with the outer index
list mm3 = ((0,), (0,), (0,));
sub ref_expression_return() {
    foreach auto x in ((1, 2, 3)) {
        foreach auto y in (\mm3[$#]) {
            y = x;
            if (x == 2) {
                return;
            }
        }
    }
}
ref_expression_return();
log += "ref expression return " + sprintf("%y", mm3);

# an outer fused loop
list mm4 = ((0,), (0,));
foreach auto x in (map $1 * 10, (1, 2)) {
    foreach auto y in (\mm4[$#]) {
        y = x;
    }
}
log += "ref expression fused outer " + sprintf("%y", mm4);

foreach string line in (log) {
    printf("%s\n", line);
}
