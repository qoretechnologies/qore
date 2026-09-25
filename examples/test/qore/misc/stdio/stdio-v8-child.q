# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%requires v8

# Keep module initialization inside parsing of an actual file: with stdin closed,
# that file used to occupy fd 0 until parsing finished, hiding the problem from Node.
our list<JavaScriptProgram> progs = ();
for (int i = 0; i < 3; ++i) {
    JavaScriptProgram js("globalThis.x = 1;", "stdio-test.js");
    progs += js;
}
printf("started\n");
