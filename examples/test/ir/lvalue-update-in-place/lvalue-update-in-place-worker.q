#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; the
# checks are included in this program, since code in a module loaded from source does not take the execution mode.
# The checks run three times, so that a function the JIT compiles on its first call runs native on the later ones
# (and, with tiered execution, runs on the AST tier, then on the IR tier, then native), and every run must report the
# same results.  The top-level code below runs once, so it checks the tier that runs top-level code.

%modern

%include lvalue-update-in-place-checks.qi

hash<string, auto> first = LValueUpdateInPlace::get_results();
for (int i = 1; i < 3; ++i) {
    hash<string, auto> again = LValueUpdateInPlace::get_results();
    foreach string key in (keys first) {
        if (again{key} !== first{key}) {
            printf("%s=%y (run %d: %y)\n", key, first{key}, i + 1, again{key});
            exit(1);
        }
    }
}
foreach hash<auto> i in (first.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}

# the original repro in top-level code, whose locals are visible to the functions it calls
hash<string, hash<string, auto>> c = LValueUpdateInPlace::make_hash();
int before = LValueUpdateInPlace::copies();
for (int r = 0; r < LValueUpdateInPlace::Rounds; ++r) {
    foreach string id in (LValueUpdateInPlace::Keys) {
        if (!c{id}{"l"}) {
            c{id}{"l"} = (0, 0, 0, 0, 0);
        }
        ++c{id}{"l"}[0];
        if (!c{id}{"l"}) {
            break;
        }
        splice c{id}{"l"}, 4, 1;
    }
}
printf("top-level=%s:%s\n", LValueUpdateInPlace::copied(before), LValueUpdateInPlace::field(c, "l"));

# a container that another variable holds is still copied in top-level code
hash<string, hash<string, auto>> other = c;
before = LValueUpdateInPlace::copies();
if (c{"k0"}{"l"}) {
    ++c{"k0"}{"l"}[0];
}
printf("top-level-alias=%s:%y:%y\n", LValueUpdateInPlace::copied(before), c{"k0"}{"l"}, other{"k0"}{"l"});
