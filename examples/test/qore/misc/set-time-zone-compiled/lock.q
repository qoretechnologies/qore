# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: a program that sets its time zone and then disallows changes to it with
# %no-locale-control, which does not undo the directive before it (qore#6270)

%modern
%set-time-zone Asia/Tokyo
%no-locale-control

sub main() {
    printf("%s\n", TimeZone::get().region());
    printf("%n\n", date("2026-06-01 12:00:00"));
}

main();
