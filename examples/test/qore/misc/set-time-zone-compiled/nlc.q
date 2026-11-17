# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: the time zone directive in Programs with PO_NO_LOCALE_CONTROL, which ignore
# it, and in a Program without (qore#6270)

%modern
%no-child-restrictions

sub main() {
    # code with the directive parsed in a Program with PO_NO_LOCALE_CONTROL
    Program p(PO_NO_LOCALE_CONTROL | PO_NEW_STYLE);
    try {
        p.parse("%set-time-zone Asia/Tokyo\nstring sub f() { return TimeZone::get().region(); }", "x");
        printf("parse with PO_NO_LOCALE_CONTROL: %s\n", p.callFunction("f"));
    } catch (hash<ExceptionInfo> ex) {
        printf("parse with PO_NO_LOCALE_CONTROL: %s: %s\n", ex.err, ex.desc);
    }
    # a Program with PO_NO_LOCALE_CONTROL loading a module that sets its time zone
    Program p2(PO_NO_LOCALE_CONTROL | PO_NEW_STYLE);
    try {
        p2.parse("%requires TzMod\nstring sub f() { return TzMod::tz_region(); }", "y");
        printf("module in a Program with PO_NO_LOCALE_CONTROL: %s\n", p2.callFunction("f"));
    } catch (hash<ExceptionInfo> ex) {
        printf("module in a Program with PO_NO_LOCALE_CONTROL: %s: %s\n", ex.err, ex.desc);
    }
    # code with the directive parsed in a Program without it
    Program p3(PO_NEW_STYLE);
    p3.parse("%set-time-zone +05:30\nint sub f() { return TimeZone::get().UTCOffset(); }", "z");
    printf("parse: %d\n", p3.callFunction("f"));
}

main();
