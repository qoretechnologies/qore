# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The locals of a block whose handlers raise an exception on its normal exit (#5855); see
# block-locals-before-catch.qtest
%modern
class Rec {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { printf("dtor %s\n", n); }
}
# v1: on_error + on_exit (runtime handlers), in a try
sub v1(int x) {
    try {
        while (x) {
            on_error { x = 3; }
            on_exit { throw "V1"; }
            Rec t("v1");
            x = 0;
        }
    } catch () {
        printf("caught v1\n");
    }
    printf("end v1\n");
}
# v2: on_exit only (inline handler), in a try
sub v2(int x) {
    try {
        while (x) {
            on_exit { throw "V2"; }
            Rec t("v2");
            x = 0;
        }
    } catch () {
        printf("caught v2\n");
    }
    printf("end v2\n");
}
# v3: on_success, in a try
sub v3(int x) {
    try {
        if (x) {
            on_success { throw "V3"; }
            Rec t("v3");
            x = 0;
        }
    } catch () {
        printf("caught v3\n");
    }
    printf("end v3\n");
}
# v4: no try: the exception leaves the function, the caller catches it
sub v4(int x) {
    if (x) {
        on_error { x = 3; }
        on_exit { throw "V4"; }
        Rec t("v4");
        x = 0;
    }
}
# v5: nested blocks with locals, the inner one's handler raises
sub v5(int x) {
    try {
        if (x) {
            Rec o("v5-outer");
            if (x) {
                on_error { x = 3; }
                on_exit { throw "V5"; }
                Rec t("v5-inner");
                x = 0;
            }
            printf("after inner v5\n");
        }
    } catch () {
        printf("caught v5\n");
    }
    printf("end v5\n");
}
# v6: a closure captures the local
sub v6(int x) {
    try {
        if (x) {
            on_error { x = 3; }
            on_exit { throw "V6"; }
            Rec t("v6");
            code c = Rec sub () { return t; };
            c();
            x = 0;
        }
    } catch () {
        printf("caught v6\n");
    }
    printf("end v6\n");
}
# v7: on_exit with only a local, no other statement between
sub v7(int x) {
    try {
        if (x) {
            on_exit { throw "V7"; }
            Rec t("v7");
        }
    } catch () {
        printf("caught v7\n");
    }
    printf("end v7\n");
}
v1(1); v2(1); v3(1);
try { v4(1); } catch () { printf("caught v4\n"); }
printf("end v4\n");
v5(1); v6(1); v7(1);
