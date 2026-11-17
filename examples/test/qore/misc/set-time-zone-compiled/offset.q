# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: the time zone that a program sets with %set-time-zone as a UTC offset (qore#6270)

%modern
%set-time-zone +05:30

sub main() {
    printf("%s\n", TimeZone::get().region());
    printf("%d\n", TimeZone::get().UTCOffset());
    printf("%n\n", date("2026-06-01 12:00:00"));
}

main();
