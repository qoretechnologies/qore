#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls every reader of the AOTConstantInvoke fixture often enough for the JIT to compile it, then prints each
# distinct result per reader, so a reader whose compiled code read a constant differently shows two results

%modern
%requires AOTConstantInvoke

const Iterations = 40;

hash<string, code> readers = {
    "native-try": \AOTConstantInvoke::Cls::readNativeTry(),
    "native-plain": \AOTConstantInvoke::Cls::readNativePlain(),
    "closure-try": AOTConstantInvoke::Cls::closureTry(),
    "closure-plain": AOTConstantInvoke::Cls::closurePlain(),
    "const-try": AOTConstantInvoke::Cls::Readers."const-try",
    "const-plain": AOTConstantInvoke::Cls::Readers."const-plain",
};
code sub registered(string name) {
    return string sub () { return AOTConstantInvoke::Registry::call(name); };
}
map readers{$1} = registered($1), ("init-try", "init-foreach", "init-catch", "init-plain");

hash<string, hash<string, int>> results;
for (int i = 0; i < Iterations; ++i) {
    foreach hash<auto> r in (readers.pairIterator()) {
        ++results{r.key}{r.value()};
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s: %s\n", i.key, (map sprintf("%s x%d", $1.key, $1.value), i.value.pairIterator()).join(" / "));
}
