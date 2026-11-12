# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Hashdecl and complex hash values in the expression slots of AOT code (#5895); see aot-hashdecl-hash-value.qtest

%modern
# the error variable of a %try-module of a module that does not exist is a hash<ExceptionInfo> value
%try-module (err) ./not-present-5895.qm
%endtry
# a hashdecl of the name of a hashdecl of the module below, which is another type
hashdecl ThrowingInitHash5895 {
    string z;
}

# a module whose init raises an exception: the error hash has a call stack and hashdecl values in its argument
%try-module (init_err) ./ThrowingInit5895.qm
%endtry
const C1 = cast<hash<string, int>>({"a": 1});
# a hashdecl constant without a member that has a default value
const C4 = <CallStackInfo>{"function": "f", "line": 1, "endline": 1, "file": "x", "source": "x", "offset": 0,
    "typecode": 0, "type": "user", "lang": "Qore"} - "start_column";
const C2 = <ExceptionInfo>{"err": "X", "desc": "d"};
const C3 = cast<list<hash<string, int>>>((cast<hash<string, int>>({"b": 2}),));
sub show(string label, auto v) {
    printf("%s: %s\n", label, v.fullType());
}
show("err", err);
show("err.callstack", err.callstack);
printf("%s %s %d %d %s\n", err.err, err.type, err.line, err.offset, err.lang);
printf("desc ok: %y\n", err.desc =~ /not-present-5895\.qm/);
printf("keys: %y\n", keys err);
show("C1", C1);
show("C2", C2);
show("C3", C3);
show("C3[0]", C3[0]);
printf("%y %y %y\n", C1, C2, C3);
show("C4", C4);
printf("%y\n", C4);
printf("%s %s\n", init_err.err, init_err.desc);
show("init_err.callstack", init_err.callstack);
printf("callstack: %y\n", (map $1 - "programid", init_err.callstack));
show("removed", init_err.arg.removed);
printf("%y\n", init_err.arg.removed);
show("reordered", init_err.arg.reordered);
printf("%y\n", init_err.arg.reordered);
# a hash of a hashdecl of the module, and a list of a complex type
show("own", init_err.arg.own);
printf("%y\n", init_err.arg.own);
show("typed_list", init_err.arg.typed_list);
printf("%y\n", init_err.arg.typed_list);
show("objects", init_err.arg.objects);
printf("%y\n", init_err.arg.objects);
printf("%y\n", <ThrowingInitHash5895>{"z": "p"});
