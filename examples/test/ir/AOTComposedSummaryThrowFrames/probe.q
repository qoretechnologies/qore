#!/usr/bin/env qore
# Copyright (c) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
%modern

# the callees of the call chains below are inlined as summaries in AOT code: an exception raised in the innermost
# one has the frame of each function of the chain and the location of the operation (qore#7632)

int sub faultDivide(int value) {
    int numerator = 100;
    return numerator / value;
}
int sub faultOuter(int value) {
    return faultDivide(value);
}
int sub faultNested(int value) {
    return faultOuter(value);
}
int sub faultModulo(int value) {
    return 100 % value;
}
int sub faultModuloOuter(int value) {
    return faultModulo(value);
}
int sub faultModuloNested(int value) {
    return faultModuloOuter(value);
}
float sub faultFloat(float value) {
    return 100.0 / value;
}
float sub faultFloatOuter(float value) {
    return faultFloat(value);
}
float sub faultFloatNested(float value) {
    return faultFloatOuter(value);
}
int sub faultBeforeReturn(int value) {
    int result = 100 / value;
    return result + 1;
}
int sub faultBeforeReturnOuter(int value) {
    return faultBeforeReturn(value);
}
int sub faultBeforeReturnNested(int value) {
    return faultBeforeReturnOuter(value);
}
# a hash projection in the innermost callee, with a division by the projected value or by a parameter
int sub faultHashKey(hash<string, int> row) {
    return 100 / row.value;
}
int sub faultHashKeyOuter(hash<string, int> row) {
    return faultHashKey(row);
}
int sub faultHashKeyNested(hash<string, int> row) {
    return faultHashKeyOuter(row);
}
int sub faultHashDivide(hash<string, int> row, int value) {
    return row.value * 3 / value;
}
int sub faultHashDivideOuter(hash<string, int> row, int value) {
    return faultHashDivide(row, value);
}
int sub faultHashDivideNested(hash<string, int> row, int value) {
    return faultHashDivideOuter(row, value);
}
float sub faultHashFloat(hash<string, float> row) {
    return 100.0 / row.value;
}
float sub faultHashFloatOuter(hash<string, float> row) {
    return faultHashFloat(row);
}
float sub faultHashFloatNested(hash<string, float> row) {
    return faultHashFloatOuter(row);
}
float sub faultHashFloatDivide(hash<string, float> row, float value) {
    return row.value * 1.5 / value;
}
float sub faultHashFloatDivideOuter(hash<string, float> row, float value) {
    return faultHashFloatDivide(row, value);
}
float sub faultHashFloatDivideNested(hash<string, float> row, float value) {
    return faultHashFloatDivideOuter(row, value);
}

# composition through static method and method calls
final class FaultChain {
    static int divide(int value) {
        return 100 / value;
    }
    static int outer(int value) {
        return FaultChain::divide(value);
    }
    static float divideFloat(float value) {
        return 100.0 / value;
    }
    static float outerFloat(float value) {
        return FaultChain::divideFloat(value);
    }
    int modulo(int value) {
        return 100 % value;
    }
    int moduloOuter(int value) {
        return modulo(value);
    }
}
int sub faultStaticNested(int value) {
    return FaultChain::outer(value);
}
float sub faultStaticFloatNested(float value) {
    return FaultChain::outerFloat(value);
}
int sub faultMethodOuter(FaultChain receiver, int value) {
    return receiver.modulo(value);
}
int sub faultMethodNested(FaultChain receiver, int value) {
    return faultMethodOuter(receiver, value);
}
int sub faultSelfMethodNested(FaultChain receiver, int value) {
    return receiver.moduloOuter(value);
}

# a division by a nonzero constant cannot raise an exception: its summary is composed (qore#7632)
int sub quarter(int value) {
    return value / 4;
}
int sub quarterOuter(int value) {
    return quarter(value) * 2 + 1;
}
int sub remainder(int value) {
    return value % 7;
}
int sub remainderOuter(int value) {
    return remainder(value) * 2 + 1;
}
float sub quarterFloat(float value) {
    return value / 4.0;
}
float sub quarterFloatOuter(float value) {
    return quarterFloat(value) * 2.0 + 1.0;
}
# and the exception of a callee called after it has the callee's frame
int sub faultAfterQuarter(int value) {
    return quarterOuter(value) + faultDivide(value);
}
int sub faultAfterQuarterNested(int value) {
    return faultAfterQuarter(value);
}

sub show(hash<ExceptionInfo> ex) {
    printf("%s:%d:%s\n", ex.err, ex.line,
        (map sprintf("%s:%d", $1.function, $1.line), ex.callstack,
            ($1.function.startsWith("fault") || $1.function.startsWith("FaultMethods::")
                || $1.function.startsWith("FaultChain::"))).join(","));
}
int caught = 0;
foreach code<int(int)> fn in ((\faultDivide(), \faultOuter(), \faultNested(),
        \faultModulo(), \faultModuloOuter(), \faultModuloNested(),
        \faultBeforeReturn(), \faultBeforeReturnOuter(), \faultBeforeReturnNested())) {
    # Re-enter each call after the exception and check the ordinary result too.
    fn(2);
    try { fn(0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn(2);
}
foreach code<float(float)> fn in ((\faultFloat(), \faultFloatOuter(), \faultFloatNested())) {
    fn(2.0);
    try { fn(0.0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn(2.0);
}
foreach code<int(hash<string, int>)> fn in ((\faultHashKey(), \faultHashKeyOuter(), \faultHashKeyNested())) {
    fn({"value": 2});
    try { fn({"value": 0}); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn({"value": 2});
}
foreach code<int(hash<string, int>, int)> fn in ((\faultHashDivide(), \faultHashDivideOuter(),
        \faultHashDivideNested())) {
    fn({"value": 2}, 2);
    try { fn({"value": 2}, 0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn({"value": 2}, 2);
}
foreach code<float(hash<string, float>)> fn in ((\faultHashFloat(), \faultHashFloatOuter(),
        \faultHashFloatNested())) {
    fn({"value": 2.0});
    try { fn({"value": 0.0}); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn({"value": 2.0});
}
foreach code<float(hash<string, float>, float)> fn in ((\faultHashFloatDivide(), \faultHashFloatDivideOuter(),
        \faultHashFloatDivideNested())) {
    fn({"value": 2.0}, 2.0);
    try { fn({"value": 2.0}, 0.0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn({"value": 2.0}, 2.0);
}
foreach code<int(int)> fn in ((\FaultChain::divide(), \FaultChain::outer(), \faultStaticNested(),
        \faultAfterQuarter(), \faultAfterQuarterNested())) {
    fn(2);
    try { fn(0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn(2);
}
foreach code<float(float)> fn in ((\FaultChain::divideFloat(), \FaultChain::outerFloat(),
        \faultStaticFloatNested())) {
    fn(2.0);
    try { fn(0.0); }
    catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    fn(2.0);
}
{
    FaultChain chain();
    foreach code<int(FaultChain, int)> fn in ((\faultMethodOuter(), \faultMethodNested(),
            \faultSelfMethodNested())) {
        fn(chain, 2);
        try { fn(chain, 0); }
        catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
        fn(chain, 2);
    }
    foreach int value in (0, 2) {
        try { chain.moduloOuter(value); }
        catch (hash<ExceptionInfo> ex) { ++caught; show(ex); }
    }
}
if (caught != 36) {
    throw "THROW-LOCATION-TEST", sprintf("expected 36 exceptions, got %d", caught);
}
# the composed summaries with a division by a constant
printf("quarter:%d:%d:%d:%d:%d:%d:%y:%y\n", quarter(17), quarterOuter(17), quarterOuter(-17), remainder(17),
    remainderOuter(17), remainderOuter(-17), quarterFloat(17.0), quarterFloatOuter(17.0));

# An exact receiver has an unwind edge even when its method summary is imported.
final class FaultMethods {
    int divide(int value) { return 100 / value; }
    float divideFloat(float value) { return 100.0 / value; }
}
sub faultMethods() {
    FaultMethods receiver();
    try { receiver.divide(0); }
    catch (hash<ExceptionInfo> ex) { show(ex); }
    try { receiver.divideFloat(0.0); }
    catch (hash<ExceptionInfo> ex) { show(ex); }
    if (receiver.divide(2) != 50 || receiver.divideFloat(2.0) != 50.0) {
        throw "METHOD-REENTRY", "method call after an exception failed";
    }
}
faultMethods();
