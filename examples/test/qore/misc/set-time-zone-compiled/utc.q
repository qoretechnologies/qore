# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: the time zone that a program sets with %set-time-zone as the UTC offset 0 (qore#6270)

%modern
%set-time-zone +00:00

sub main() {
    printf("%d\n", TimeZone::get().UTCOffset());
    printf("%n\n", date("2026-06-01 12:00:00"));
}

main();
