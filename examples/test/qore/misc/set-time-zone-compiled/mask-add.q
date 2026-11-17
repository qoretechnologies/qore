# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: a module parse option mask that adds PO_NO_LOCALE_CONTROL to the Programs of
# modules, which then ignore the time zone that a module sets (qore#6270)

%modern
%no-child-restrictions

sub main() {
    set_module_parse_option_mask(new ParseOptions(), new ParseOptions(("no-locale-control",)));
    Program p(PO_NEW_STYLE);
    p.parse("%requires TzMod\nstring sub f() { return TzMod::tz_region(); }", "mask-add");
    printf("module with the mask adding no-locale-control: %s\n", p.callFunction("f"));
}

main();
