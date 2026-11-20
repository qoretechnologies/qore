# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%no-child-restrictions
%allow-weak-references

# Only the child Program requests access to frame variables. Its callback
# enters this frame through a call reference.
sub relay(code inspect) {
    inspect();
}

sub callInspector(code inspect, int seed, bool nested) {
    auto alias = {"a": seed};
    auto owner = alias;
    auto observer := owner;
    if (nested) {
        call_function("relay", inspect);
    } else {
        inspect();
    }
    printf("after inspection %d %d\n", owner.a, observer.a);
}

Program child(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
child.parse("code sub inspector(int depth) {
    return sub () {
        hash frame = get_local_vars(depth);
        printf(\"keys %s\\n\", (keys frame).join(\",\"));
        if (frame.hasKey(\"owner\")) {
            printf(\"owner %s\\n\", frame.owner.unavailable ? \"unavailable\" : string(frame.owner.value.a));
            try {
                set_local_var_value(depth, \"owner\", {\"a\": 11});
            } catch (hash<ExceptionInfo> ex) {
                printf(\"write %s\\n\", ex.err);
            }
        }
    };
}", "foreign-inspector");
call_function("callInspector", child.callFunction("inspector", 1), 7, False);
call_function("callInspector", child.callFunction("inspector", 2), 8, True);
