# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: a program without a time zone directive that uses a module that sets its own
# (the module's code runs in its time zone, and the program in the process's; qore#6270)

%modern
%requires TzMod

sub main() {
    printf("program: %s %n\n", TimeZone::get().region(), date("2026-06-01 12:00:00"));
    printf("module: %s %s\n", tz_region(), tz_date());
    printf("module class: %s\n", (new TzC()).region());
}

main();
