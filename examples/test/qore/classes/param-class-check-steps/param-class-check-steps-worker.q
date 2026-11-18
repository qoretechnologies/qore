#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# the worker of param-class-check-steps.qtest (qore#6449): run-time checks of objects of the top class of lattices of
# classes, where each class of a level inherits both classes of the level below (plain classes, generic classes, and
# generic classes whose edges wrap the type argument in list<> or softlist<>, so that a class is reached with many
# different type arguments), against parameterized class types;
# prints the steps of the lookups of each check, counted by a debug hook of the library ("-" without it), and the
# results of the checks

%modern

class G<T> {
}

class Unrel<T> {
}

#! returns the steps of the lookups of parameterized base types so far, or -1 without the debug hook
int sub steps() {
    # the debug hook exists in debug builds of the library only
    try {
        return call_function("dbg_param_class_check_steps");
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "NO-FUNCTION" || ex.err == "UNDEFINED-FUNCTION-CALL") {
            return -1;
        }
        rethrow;
    }
}

bool sub take_g(G<int> g) {
    return True;
}

string sub take_unrel(auto u) {
    try {
        Unrel<int> x = u;
        return "accepted";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! a check against Unrel<int> only, for a lattice whose top class does not inherit G<int>
string sub check_neg(string label, object o) {
    int s0 = steps();
    bool neg = o instanceof Unrel<int>;
    int s1 = steps();
    return sprintf("%s neg-steps=%s neg=%y", label, s0 < 0 ? "-" : (s1 - s0).toString(), neg);
}

string sub check(string label, object o) {
    int s0 = steps();
    bool neg = o instanceof Unrel<int>;
    int s1 = steps();
    bool pos = o instanceof G<int>;
    bool param = take_g(o);
    string unrel = take_unrel(o);
    return sprintf("%s neg-steps=%s neg=%y pos=%y param=%y unrel=%s", label, s0 < 0 ? "-" : (s1 - s0).toString(), neg,
        pos, param, unrel);
}

class P4_0a inherits G<int> {
}

class P4_0b inherits G<int> {
}

class P4_1a inherits P4_0a, P4_0b {
}

class P4_1b inherits P4_0a, P4_0b {
}

class P4_2a inherits P4_1a, P4_1b {
}

class P4_2b inherits P4_1a, P4_1b {
}

class P4_3a inherits P4_2a, P4_2b {
}

class P4_3b inherits P4_2a, P4_2b {
}

class P4_4a inherits P4_3a, P4_3b {
}

class P4_4b inherits P4_3a, P4_3b {
}

class Q4_0a<T> inherits G<T> {
}

class Q4_0b<T> inherits G<T> {
}

class Q4_1a<T> inherits Q4_0a<T>, Q4_0b<T> {
}

class Q4_1b<T> inherits Q4_0a<T>, Q4_0b<T> {
}

class Q4_2a<T> inherits Q4_1a<T>, Q4_1b<T> {
}

class Q4_2b<T> inherits Q4_1a<T>, Q4_1b<T> {
}

class Q4_3a<T> inherits Q4_2a<T>, Q4_2b<T> {
}

class Q4_3b<T> inherits Q4_2a<T>, Q4_2b<T> {
}

class Q4_4a<T> inherits Q4_3a<T>, Q4_3b<T> {
}

class Q4_4b<T> inherits Q4_3a<T>, Q4_3b<T> {
}

class P6_0a inherits G<int> {
}

class P6_0b inherits G<int> {
}

class P6_1a inherits P6_0a, P6_0b {
}

class P6_1b inherits P6_0a, P6_0b {
}

class P6_2a inherits P6_1a, P6_1b {
}

class P6_2b inherits P6_1a, P6_1b {
}

class P6_3a inherits P6_2a, P6_2b {
}

class P6_3b inherits P6_2a, P6_2b {
}

class P6_4a inherits P6_3a, P6_3b {
}

class P6_4b inherits P6_3a, P6_3b {
}

class P6_5a inherits P6_4a, P6_4b {
}

class P6_5b inherits P6_4a, P6_4b {
}

class P6_6a inherits P6_5a, P6_5b {
}

class P6_6b inherits P6_5a, P6_5b {
}

class Q6_0a<T> inherits G<T> {
}

class Q6_0b<T> inherits G<T> {
}

class Q6_1a<T> inherits Q6_0a<T>, Q6_0b<T> {
}

class Q6_1b<T> inherits Q6_0a<T>, Q6_0b<T> {
}

class Q6_2a<T> inherits Q6_1a<T>, Q6_1b<T> {
}

class Q6_2b<T> inherits Q6_1a<T>, Q6_1b<T> {
}

class Q6_3a<T> inherits Q6_2a<T>, Q6_2b<T> {
}

class Q6_3b<T> inherits Q6_2a<T>, Q6_2b<T> {
}

class Q6_4a<T> inherits Q6_3a<T>, Q6_3b<T> {
}

class Q6_4b<T> inherits Q6_3a<T>, Q6_3b<T> {
}

class Q6_5a<T> inherits Q6_4a<T>, Q6_4b<T> {
}

class Q6_5b<T> inherits Q6_4a<T>, Q6_4b<T> {
}

class Q6_6a<T> inherits Q6_5a<T>, Q6_5b<T> {
}

class Q6_6b<T> inherits Q6_5a<T>, Q6_5b<T> {
}

class P8_0a inherits G<int> {
}

class P8_0b inherits G<int> {
}

class P8_1a inherits P8_0a, P8_0b {
}

class P8_1b inherits P8_0a, P8_0b {
}

class P8_2a inherits P8_1a, P8_1b {
}

class P8_2b inherits P8_1a, P8_1b {
}

class P8_3a inherits P8_2a, P8_2b {
}

class P8_3b inherits P8_2a, P8_2b {
}

class P8_4a inherits P8_3a, P8_3b {
}

class P8_4b inherits P8_3a, P8_3b {
}

class P8_5a inherits P8_4a, P8_4b {
}

class P8_5b inherits P8_4a, P8_4b {
}

class P8_6a inherits P8_5a, P8_5b {
}

class P8_6b inherits P8_5a, P8_5b {
}

class P8_7a inherits P8_6a, P8_6b {
}

class P8_7b inherits P8_6a, P8_6b {
}

class P8_8a inherits P8_7a, P8_7b {
}

class P8_8b inherits P8_7a, P8_7b {
}

class Q8_0a<T> inherits G<T> {
}

class Q8_0b<T> inherits G<T> {
}

class Q8_1a<T> inherits Q8_0a<T>, Q8_0b<T> {
}

class Q8_1b<T> inherits Q8_0a<T>, Q8_0b<T> {
}

class Q8_2a<T> inherits Q8_1a<T>, Q8_1b<T> {
}

class Q8_2b<T> inherits Q8_1a<T>, Q8_1b<T> {
}

class Q8_3a<T> inherits Q8_2a<T>, Q8_2b<T> {
}

class Q8_3b<T> inherits Q8_2a<T>, Q8_2b<T> {
}

class Q8_4a<T> inherits Q8_3a<T>, Q8_3b<T> {
}

class Q8_4b<T> inherits Q8_3a<T>, Q8_3b<T> {
}

class Q8_5a<T> inherits Q8_4a<T>, Q8_4b<T> {
}

class Q8_5b<T> inherits Q8_4a<T>, Q8_4b<T> {
}

class Q8_6a<T> inherits Q8_5a<T>, Q8_5b<T> {
}

class Q8_6b<T> inherits Q8_5a<T>, Q8_5b<T> {
}

class Q8_7a<T> inherits Q8_6a<T>, Q8_6b<T> {
}

class Q8_7b<T> inherits Q8_6a<T>, Q8_6b<T> {
}

class Q8_8a<T> inherits Q8_7a<T>, Q8_7b<T> {
}

class Q8_8b<T> inherits Q8_7a<T>, Q8_7b<T> {
}

class P10_0a inherits G<int> {
}

class P10_0b inherits G<int> {
}

class P10_1a inherits P10_0a, P10_0b {
}

class P10_1b inherits P10_0a, P10_0b {
}

class P10_2a inherits P10_1a, P10_1b {
}

class P10_2b inherits P10_1a, P10_1b {
}

class P10_3a inherits P10_2a, P10_2b {
}

class P10_3b inherits P10_2a, P10_2b {
}

class P10_4a inherits P10_3a, P10_3b {
}

class P10_4b inherits P10_3a, P10_3b {
}

class P10_5a inherits P10_4a, P10_4b {
}

class P10_5b inherits P10_4a, P10_4b {
}

class P10_6a inherits P10_5a, P10_5b {
}

class P10_6b inherits P10_5a, P10_5b {
}

class P10_7a inherits P10_6a, P10_6b {
}

class P10_7b inherits P10_6a, P10_6b {
}

class P10_8a inherits P10_7a, P10_7b {
}

class P10_8b inherits P10_7a, P10_7b {
}

class P10_9a inherits P10_8a, P10_8b {
}

class P10_9b inherits P10_8a, P10_8b {
}

class P10_10a inherits P10_9a, P10_9b {
}

class P10_10b inherits P10_9a, P10_9b {
}

class Q10_0a<T> inherits G<T> {
}

class Q10_0b<T> inherits G<T> {
}

class Q10_1a<T> inherits Q10_0a<T>, Q10_0b<T> {
}

class Q10_1b<T> inherits Q10_0a<T>, Q10_0b<T> {
}

class Q10_2a<T> inherits Q10_1a<T>, Q10_1b<T> {
}

class Q10_2b<T> inherits Q10_1a<T>, Q10_1b<T> {
}

class Q10_3a<T> inherits Q10_2a<T>, Q10_2b<T> {
}

class Q10_3b<T> inherits Q10_2a<T>, Q10_2b<T> {
}

class Q10_4a<T> inherits Q10_3a<T>, Q10_3b<T> {
}

class Q10_4b<T> inherits Q10_3a<T>, Q10_3b<T> {
}

class Q10_5a<T> inherits Q10_4a<T>, Q10_4b<T> {
}

class Q10_5b<T> inherits Q10_4a<T>, Q10_4b<T> {
}

class Q10_6a<T> inherits Q10_5a<T>, Q10_5b<T> {
}

class Q10_6b<T> inherits Q10_5a<T>, Q10_5b<T> {
}

class Q10_7a<T> inherits Q10_6a<T>, Q10_6b<T> {
}

class Q10_7b<T> inherits Q10_6a<T>, Q10_6b<T> {
}

class Q10_8a<T> inherits Q10_7a<T>, Q10_7b<T> {
}

class Q10_8b<T> inherits Q10_7a<T>, Q10_7b<T> {
}

class Q10_9a<T> inherits Q10_8a<T>, Q10_8b<T> {
}

class Q10_9b<T> inherits Q10_8a<T>, Q10_8b<T> {
}

class Q10_10a<T> inherits Q10_9a<T>, Q10_9b<T> {
}

class Q10_10b<T> inherits Q10_9a<T>, Q10_9b<T> {
}

class P12_0a inherits G<int> {
}

class P12_0b inherits G<int> {
}

class P12_1a inherits P12_0a, P12_0b {
}

class P12_1b inherits P12_0a, P12_0b {
}

class P12_2a inherits P12_1a, P12_1b {
}

class P12_2b inherits P12_1a, P12_1b {
}

class P12_3a inherits P12_2a, P12_2b {
}

class P12_3b inherits P12_2a, P12_2b {
}

class P12_4a inherits P12_3a, P12_3b {
}

class P12_4b inherits P12_3a, P12_3b {
}

class P12_5a inherits P12_4a, P12_4b {
}

class P12_5b inherits P12_4a, P12_4b {
}

class P12_6a inherits P12_5a, P12_5b {
}

class P12_6b inherits P12_5a, P12_5b {
}

class P12_7a inherits P12_6a, P12_6b {
}

class P12_7b inherits P12_6a, P12_6b {
}

class P12_8a inherits P12_7a, P12_7b {
}

class P12_8b inherits P12_7a, P12_7b {
}

class P12_9a inherits P12_8a, P12_8b {
}

class P12_9b inherits P12_8a, P12_8b {
}

class P12_10a inherits P12_9a, P12_9b {
}

class P12_10b inherits P12_9a, P12_9b {
}

class P12_11a inherits P12_10a, P12_10b {
}

class P12_11b inherits P12_10a, P12_10b {
}

class P12_12a inherits P12_11a, P12_11b {
}

class P12_12b inherits P12_11a, P12_11b {
}

class Q12_0a<T> inherits G<T> {
}

class Q12_0b<T> inherits G<T> {
}

class Q12_1a<T> inherits Q12_0a<T>, Q12_0b<T> {
}

class Q12_1b<T> inherits Q12_0a<T>, Q12_0b<T> {
}

class Q12_2a<T> inherits Q12_1a<T>, Q12_1b<T> {
}

class Q12_2b<T> inherits Q12_1a<T>, Q12_1b<T> {
}

class Q12_3a<T> inherits Q12_2a<T>, Q12_2b<T> {
}

class Q12_3b<T> inherits Q12_2a<T>, Q12_2b<T> {
}

class Q12_4a<T> inherits Q12_3a<T>, Q12_3b<T> {
}

class Q12_4b<T> inherits Q12_3a<T>, Q12_3b<T> {
}

class Q12_5a<T> inherits Q12_4a<T>, Q12_4b<T> {
}

class Q12_5b<T> inherits Q12_4a<T>, Q12_4b<T> {
}

class Q12_6a<T> inherits Q12_5a<T>, Q12_5b<T> {
}

class Q12_6b<T> inherits Q12_5a<T>, Q12_5b<T> {
}

class Q12_7a<T> inherits Q12_6a<T>, Q12_6b<T> {
}

class Q12_7b<T> inherits Q12_6a<T>, Q12_6b<T> {
}

class Q12_8a<T> inherits Q12_7a<T>, Q12_7b<T> {
}

class Q12_8b<T> inherits Q12_7a<T>, Q12_7b<T> {
}

class Q12_9a<T> inherits Q12_8a<T>, Q12_8b<T> {
}

class Q12_9b<T> inherits Q12_8a<T>, Q12_8b<T> {
}

class Q12_10a<T> inherits Q12_9a<T>, Q12_9b<T> {
}

class Q12_10b<T> inherits Q12_9a<T>, Q12_9b<T> {
}

class Q12_11a<T> inherits Q12_10a<T>, Q12_10b<T> {
}

class Q12_11b<T> inherits Q12_10a<T>, Q12_10b<T> {
}

class Q12_12a<T> inherits Q12_11a<T>, Q12_11b<T> {
}

class Q12_12b<T> inherits Q12_11a<T>, Q12_11b<T> {
}

class W4_0a<T> inherits G<T> {
}

class W4_0b<T> inherits G<T> {
}

class W4_1a<T> inherits W4_0a<list<T>>, W4_0b<list<T>> {
}

class W4_1b<T> inherits W4_0a<softlist<T>>, W4_0b<softlist<T>> {
}

class W4_2a<T> inherits W4_1a<list<T>>, W4_1b<list<T>> {
}

class W4_2b<T> inherits W4_1a<softlist<T>>, W4_1b<softlist<T>> {
}

class W4_3a<T> inherits W4_2a<list<T>>, W4_2b<list<T>> {
}

class W4_3b<T> inherits W4_2a<softlist<T>>, W4_2b<softlist<T>> {
}

class W4_4a<T> inherits W4_3a<list<T>>, W4_3b<list<T>> {
}

class W4_4b<T> inherits W4_3a<softlist<T>>, W4_3b<softlist<T>> {
}

class W6_0a<T> inherits G<T> {
}

class W6_0b<T> inherits G<T> {
}

class W6_1a<T> inherits W6_0a<list<T>>, W6_0b<list<T>> {
}

class W6_1b<T> inherits W6_0a<softlist<T>>, W6_0b<softlist<T>> {
}

class W6_2a<T> inherits W6_1a<list<T>>, W6_1b<list<T>> {
}

class W6_2b<T> inherits W6_1a<softlist<T>>, W6_1b<softlist<T>> {
}

class W6_3a<T> inherits W6_2a<list<T>>, W6_2b<list<T>> {
}

class W6_3b<T> inherits W6_2a<softlist<T>>, W6_2b<softlist<T>> {
}

class W6_4a<T> inherits W6_3a<list<T>>, W6_3b<list<T>> {
}

class W6_4b<T> inherits W6_3a<softlist<T>>, W6_3b<softlist<T>> {
}

class W6_5a<T> inherits W6_4a<list<T>>, W6_4b<list<T>> {
}

class W6_5b<T> inherits W6_4a<softlist<T>>, W6_4b<softlist<T>> {
}

class W6_6a<T> inherits W6_5a<list<T>>, W6_5b<list<T>> {
}

class W6_6b<T> inherits W6_5a<softlist<T>>, W6_5b<softlist<T>> {
}

class W8_0a<T> inherits G<T> {
}

class W8_0b<T> inherits G<T> {
}

class W8_1a<T> inherits W8_0a<list<T>>, W8_0b<list<T>> {
}

class W8_1b<T> inherits W8_0a<softlist<T>>, W8_0b<softlist<T>> {
}

class W8_2a<T> inherits W8_1a<list<T>>, W8_1b<list<T>> {
}

class W8_2b<T> inherits W8_1a<softlist<T>>, W8_1b<softlist<T>> {
}

class W8_3a<T> inherits W8_2a<list<T>>, W8_2b<list<T>> {
}

class W8_3b<T> inherits W8_2a<softlist<T>>, W8_2b<softlist<T>> {
}

class W8_4a<T> inherits W8_3a<list<T>>, W8_3b<list<T>> {
}

class W8_4b<T> inherits W8_3a<softlist<T>>, W8_3b<softlist<T>> {
}

class W8_5a<T> inherits W8_4a<list<T>>, W8_4b<list<T>> {
}

class W8_5b<T> inherits W8_4a<softlist<T>>, W8_4b<softlist<T>> {
}

class W8_6a<T> inherits W8_5a<list<T>>, W8_5b<list<T>> {
}

class W8_6b<T> inherits W8_5a<softlist<T>>, W8_5b<softlist<T>> {
}

class W8_7a<T> inherits W8_6a<list<T>>, W8_6b<list<T>> {
}

class W8_7b<T> inherits W8_6a<softlist<T>>, W8_6b<softlist<T>> {
}

class W8_8a<T> inherits W8_7a<list<T>>, W8_7b<list<T>> {
}

class W8_8b<T> inherits W8_7a<softlist<T>>, W8_7b<softlist<T>> {
}

class W10_0a<T> inherits G<T> {
}

class W10_0b<T> inherits G<T> {
}

class W10_1a<T> inherits W10_0a<list<T>>, W10_0b<list<T>> {
}

class W10_1b<T> inherits W10_0a<softlist<T>>, W10_0b<softlist<T>> {
}

class W10_2a<T> inherits W10_1a<list<T>>, W10_1b<list<T>> {
}

class W10_2b<T> inherits W10_1a<softlist<T>>, W10_1b<softlist<T>> {
}

class W10_3a<T> inherits W10_2a<list<T>>, W10_2b<list<T>> {
}

class W10_3b<T> inherits W10_2a<softlist<T>>, W10_2b<softlist<T>> {
}

class W10_4a<T> inherits W10_3a<list<T>>, W10_3b<list<T>> {
}

class W10_4b<T> inherits W10_3a<softlist<T>>, W10_3b<softlist<T>> {
}

class W10_5a<T> inherits W10_4a<list<T>>, W10_4b<list<T>> {
}

class W10_5b<T> inherits W10_4a<softlist<T>>, W10_4b<softlist<T>> {
}

class W10_6a<T> inherits W10_5a<list<T>>, W10_5b<list<T>> {
}

class W10_6b<T> inherits W10_5a<softlist<T>>, W10_5b<softlist<T>> {
}

class W10_7a<T> inherits W10_6a<list<T>>, W10_6b<list<T>> {
}

class W10_7b<T> inherits W10_6a<softlist<T>>, W10_6b<softlist<T>> {
}

class W10_8a<T> inherits W10_7a<list<T>>, W10_7b<list<T>> {
}

class W10_8b<T> inherits W10_7a<softlist<T>>, W10_7b<softlist<T>> {
}

class W10_9a<T> inherits W10_8a<list<T>>, W10_8b<list<T>> {
}

class W10_9b<T> inherits W10_8a<softlist<T>>, W10_8b<softlist<T>> {
}

class W10_10a<T> inherits W10_9a<list<T>>, W10_9b<list<T>> {
}

class W10_10b<T> inherits W10_9a<softlist<T>>, W10_9b<softlist<T>> {
}

class W12_0a<T> inherits G<T> {
}

class W12_0b<T> inherits G<T> {
}

class W12_1a<T> inherits W12_0a<list<T>>, W12_0b<list<T>> {
}

class W12_1b<T> inherits W12_0a<softlist<T>>, W12_0b<softlist<T>> {
}

class W12_2a<T> inherits W12_1a<list<T>>, W12_1b<list<T>> {
}

class W12_2b<T> inherits W12_1a<softlist<T>>, W12_1b<softlist<T>> {
}

class W12_3a<T> inherits W12_2a<list<T>>, W12_2b<list<T>> {
}

class W12_3b<T> inherits W12_2a<softlist<T>>, W12_2b<softlist<T>> {
}

class W12_4a<T> inherits W12_3a<list<T>>, W12_3b<list<T>> {
}

class W12_4b<T> inherits W12_3a<softlist<T>>, W12_3b<softlist<T>> {
}

class W12_5a<T> inherits W12_4a<list<T>>, W12_4b<list<T>> {
}

class W12_5b<T> inherits W12_4a<softlist<T>>, W12_4b<softlist<T>> {
}

class W12_6a<T> inherits W12_5a<list<T>>, W12_5b<list<T>> {
}

class W12_6b<T> inherits W12_5a<softlist<T>>, W12_5b<softlist<T>> {
}

class W12_7a<T> inherits W12_6a<list<T>>, W12_6b<list<T>> {
}

class W12_7b<T> inherits W12_6a<softlist<T>>, W12_6b<softlist<T>> {
}

class W12_8a<T> inherits W12_7a<list<T>>, W12_7b<list<T>> {
}

class W12_8b<T> inherits W12_7a<softlist<T>>, W12_7b<softlist<T>> {
}

class W12_9a<T> inherits W12_8a<list<T>>, W12_8b<list<T>> {
}

class W12_9b<T> inherits W12_8a<softlist<T>>, W12_8b<softlist<T>> {
}

class W12_10a<T> inherits W12_9a<list<T>>, W12_9b<list<T>> {
}

class W12_10b<T> inherits W12_9a<softlist<T>>, W12_9b<softlist<T>> {
}

class W12_11a<T> inherits W12_10a<list<T>>, W12_10b<list<T>> {
}

class W12_11b<T> inherits W12_10a<softlist<T>>, W12_10b<softlist<T>> {
}

class W12_12a<T> inherits W12_11a<list<T>>, W12_11b<list<T>> {
}

class W12_12b<T> inherits W12_11a<softlist<T>>, W12_11b<softlist<T>> {
}

list<string> sub probes() {
    list<string> l = ();
    l += check("plain depth 4", new P4_4a());
    l += check("generic depth 4", new Q4_4a<int>());
    l += check_neg("wrap depth 4", new W4_4a<int>());
    l += check("plain depth 6", new P6_6a());
    l += check("generic depth 6", new Q6_6a<int>());
    l += check_neg("wrap depth 6", new W6_6a<int>());
    l += check("plain depth 8", new P8_8a());
    l += check("generic depth 8", new Q8_8a<int>());
    l += check_neg("wrap depth 8", new W8_8a<int>());
    l += check("plain depth 10", new P10_10a());
    l += check("generic depth 10", new Q10_10a<int>());
    l += check_neg("wrap depth 10", new W10_10a<int>());
    l += check("plain depth 12", new P12_12a());
    l += check("generic depth 12", new Q12_12a<int>());
    l += check_neg("wrap depth 12", new W12_12a<int>());
    return l;
}

# twice, so that tiered execution compiles the probes
for (int i = 0; i < 2; ++i) {
    printf("%d:\n%s\n", i, probes().join("\n"));
}
