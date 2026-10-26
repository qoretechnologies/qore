#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for hashdecl-null-sink.qtest: creates hashes of hashdecls with initializers as the C++ API does without an
# ExceptionSink (dbg_new_hashdecl_hash_without_sink()) and prints "name=value" lines.  Run once per execution mode.

%modern
%require-types
%enable-all-warnings

hashdecl NsDefaults {
    int a = 1;
    string b = "x";
    list<int> l = (1, 2);
}

hashdecl NsChild inherits NsDefaults {
    int c = 3;
}

int sub ns_fail() {
    throw "NULL-SINK-TEST", "initializer";
}

hashdecl NsThrows {
    int a = 1;
    int b = ns_fail();
}

hash<auto> sub ns_new(auto h) {
    return call_function("dbg_new_hashdecl_hash_without_sink", h);
}

printf("defaults=%y\n", ns_new(<NsDefaults>{}));
printf("defaults-type=%s\n", ns_new(<NsDefaults>{}).fullType());
printf("child=%y\n", ns_new(<NsChild>{}));
# the exception of the initializer has no sink to go to: it is reported as unhandled, and the hash keeps the members
# initialized before it
printf("throws=%y\n", ns_new(<NsThrows>{"b": 2}));
printf("done=1\n");
