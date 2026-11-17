# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: the time zone that a program sets with %set-time-zone as a region (qore#6270)

%modern
%set-time-zone Asia/Tokyo

sub main() {
    printf("%s\n", TimeZone::get().region());
    printf("%n\n", 2026-01-01T00:00:00);
    printf("%n\n", date("2026-06-01 12:00:00"));
    printf("%s\n", (2026-01-01T00:00:00 + 1D).format("YYYY-MM-DD HH:mm:SS Z"));
    printf("%d\n", TimeZone::get().UTCOffset());
}

main();
