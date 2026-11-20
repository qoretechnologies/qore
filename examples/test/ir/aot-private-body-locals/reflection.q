# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-debugger
%allow-weak-references

sub inspectParent() {
    hash<auto> frame = get_local_vars(1);
    printf("parent %s captured %d\n", frame.owner.unavailable ? "unavailable" : string(frame.owner.value.a),
        frame.captured.value);
    try {
        set_local_var_value(1, "owner", {"a": 21});
        printf("parent after write %d\n", get_local_vars(1).owner.value.a);
    } catch (hash<ExceptionInfo> ex) {
        printf("write %s\n", ex.err);
    }
}

sub inspectOwners(int value) {
    auto alias = {"a": value};
    auto owner = alias;
    auto observer := owner;
    int captured = value;
    code readCaptured = int sub () { return captured; };
    hash<auto> frame = get_local_vars(0);
    printf("private %s captured %d\n", frame.owner.unavailable ? "unavailable" : string(frame.owner.value.a),
        frame.captured.value);
    set_local_var_value(0, "captured", value + 2);
    printf("after set %d %d\n", observer.a, readCaptured());
    call_function("inspectParent");
}

call_function("inspectOwners", 7);
