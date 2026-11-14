#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Functions without an exception handler whose statements in loops hold temporaries that can run a destructor when
# a call in them raises an exception: the lowering of each one walks the control flow graph of the function, before
# the function's error return block has a terminator, to find which temporaries a loop keeps at an exception check
# (qore#5884); the output shows the release order, which must be the AST interpreter's

%modern

class Tracked {
    public {
        string name;
    }

    constructor(string name) {
        self.name = name;
    }

    destructor() {
        printf("released %s; ", name);
    }

    string getName() {
        return name;
    }
}

string sub stage(int i, int stop) {
    if (i == stop) {
        throw "STAGE", sprintf("stop at %d", i);
    }
    return sprintf("v%d", i);
}

# a for loop: the temporary object of the statement and the call that raises
sub for_loop(int stop) {
    for (int i = 0; i < 4; ++i) {
        printf("%s %s; ", (new Tracked("f" + i)).getName(), stage(i, stop));
    }
}

# a while loop with a temporary that the statement stores and the next iteration reaches
sub while_loop(int stop) {
    int i = 0;
    while (i < 4) {
        string s = (new Tracked("w" + i)).getName() + "/" + stage(i, stop);
        printf("%s; ", s);
        ++i;
    }
}

# nested loops and a map whose code raises
sub nested_loops(int stop) {
    foreach int i in (0, 1) {
        foreach int j in (0, 1) {
            list<string> l = map (new Tracked(sprintf("n%d%d", i, $1))).getName() + stage(i * 2 + j, stop), (0, 1);
            printf("%y; ", l);
        }
    }
}

# a do-while loop that branches back to itself
sub do_loop(int stop) {
    int i = 0;
    do {
        printf("%s; ", (new Tracked("d" + i)).getName() + stage(i, stop));
    } while (++i < 4);
}

sub run(string label, code c) {
    printf("%s: ", label);
    try {
        c();
        printf("no exception");
    } catch (hash<ExceptionInfo> ex) {
        printf("catch %s: %s", ex.err, ex.desc);
    }
    printf("\n");
}

foreach int stop in (2, 9) {
    run("for " + stop, sub () { for_loop(stop); });
    run("while " + stop, sub () { while_loop(stop); });
    run("nested " + stop, sub () { nested_loops(stop); });
    run("do " + stop, sub () { do_loop(stop); });
}
