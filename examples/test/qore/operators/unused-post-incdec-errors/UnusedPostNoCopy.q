#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# counts the old values that post-increments and post-decrements copy; run by unused-post-incdec-errors.qtest

/** An unused post-increment or post-decrement of a number in an lvalue whose type can refuse the result (an untyped
    local, an untyped member, also named without "self." in a method) is lowered as
    the post operation, so that an error names the post operator, but its old value is not used, so it must not be
    copied (qore#6197).  Each check prints the number of old values copied by its operations
    (dbg_get_number_old_value_copy_count(), which only debug builds have; other builds print "skip"): 0 for the
    unused operations, and one per operation for the controls, whose old value is used.
*/

%modern

const N = 10;

class Holder {
    public {
        auto a = 1.5n;
    }

    #! changes the member named without "self."
    int bareMember() {
        int start = copies();
        for (int i = 0; i < N; ++i) {
            a++;
            a--;
        }
        return copies() - start;
    }

    #! changes the member named with "self."
    int selfMember() {
        int start = copies();
        for (int i = 0; i < N; ++i) {
            self.a++;
            self.a--;
        }
        return copies() - start;
    }
}

int sub copies() {
    return call_function("dbg_get_number_old_value_copy_count");
}

int sub local_unused() {
    auto a = 1.5n;
    int start = copies();
    for (int i = 0; i < N; ++i) {
        a++;
        a--;
    }
    return copies() - start;
}

int sub member_unused() {
    object o = new Holder();
    int start = copies();
    for (int i = 0; i < N; ++i) {
        o.a++;
        o.a--;
    }
    return copies() - start;
}

#! the control: the old value is used, so each operation copies it
int sub local_used() {
    auto a = 1.5n;
    auto old;
    int start = copies();
    for (int i = 0; i < N; ++i) {
        old = a++;
        old = a--;
    }
    return copies() - start;
}

if (!Option::HAVE_LIBRARY_DEBUGGING) {
    print("skip\n");
} else {
    Holder holder();
    printf("local-unused %d\n", local_unused());
    printf("member-unused %d\n", member_unused());
    printf("bare-self-member-unused %d\n", holder.bareMember());
    printf("self-member-unused %d\n", holder.selfMember());
    printf("local-used %d\n", local_used());
}
