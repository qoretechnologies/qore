# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# an old-style script calling the RUNTIME_NOOP variants of builtin functions, which return a constant value

my $f = abs();
my $r = round();
printf("%y %y\n", $f, $r);
