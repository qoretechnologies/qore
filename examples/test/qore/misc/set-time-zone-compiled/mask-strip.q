# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: a module parse option mask that removes PO_NO_LOCALE_CONTROL of the loading
# Program from the Programs of modules, which then keep the time zone that a module sets (qore#6270)

%modern
%no-child-restrictions

sub main() {
    set_module_parse_option_mask(new ParseOptions(("no-locale-control",)));
    Program p(PO_NEW_STYLE | PO_NO_LOCALE_CONTROL);
    p.parse("%requires TzMod\nstring sub f() { return TzMod::tz_region(); }", "mask-strip");
    printf("module with the mask removing no-locale-control: %s\n", p.callFunction("f"));
}

main();
