#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# reads string parameters after a reference or a closure has removed their value, and prints one result per line

%modern

nothing sub clear_string(reference<string> value) {
    remove value;
}

#! index() of a parameter cleared through a reference
string sub ref_index(string value, string pattern) {
    clear_string(\value);
    try {
        return string(index(value, pattern));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! rindex() of a parameter cleared through a reference
string sub ref_rindex(string value, string pattern) {
    clear_string(\value);
    try {
        return string(rindex(value, pattern));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! <string>::find() of a parameter cleared through a reference
string sub ref_find(string value, string pattern) {
    clear_string(\value);
    try {
        return string(value.find(pattern));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! substr() of a parameter cleared through a reference
string sub ref_substr(string value) {
    clear_string(\value);
    try {
        return sprintf("%y", substr(value, 1));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! index() of a parameter cleared by a closure
string sub closure_index(string value, string pattern) {
    code c = sub () { remove value; };
    c();
    try {
        return string(index(value, pattern));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! index() of a parameter that is read before it is cleared through a reference in a loop
string sub loop_index(string value, string pattern) {
    string rv;
    for (int i = 0; i < 2; ++i) {
        try {
            rv += string(index(value, pattern)) + ";";
        } catch (hash<ExceptionInfo> ex) {
            rv += ex.err + ";";
        }
        clear_string(\value);
    }
    return rv;
}

#! index() of a parameter passed by reference that is not cleared
string sub ref_kept(string value, string pattern) {
    code observe = sub (reference<string> v) {};
    observe(\value);
    return string(index(value, pattern));
}

# the functions are called twice through call references, so that each call is a call of the function itself: with
# native compilation following the first call, the second call runs as native code with --exec-mode=jit and tiered
for (int i = 0; i < 2; ++i) {
    foreach hash<auto> c in ({
        "ref_index": \ref_index(),
        "ref_rindex": \ref_rindex(),
        "ref_find": \ref_find(),
        "closure_index": \closure_index(),
        "loop_index": \loop_index(),
        "ref_kept": \ref_kept(),
    }.pairIterator()) {
        code<string(string, string)> f = c.value;
        printf("%s=%s\n", c.key, f("a.b", "."));
    }
    printf("ref_substr=%s\n", call_function(\ref_substr(), "abc"));
}
