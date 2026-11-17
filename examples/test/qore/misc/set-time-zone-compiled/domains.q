# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: modules that declare the locale control domain in their headers and set
# their time zones after (TzDomAfter) and before (TzDomBefore) the header, loaded by Programs whose restrictions
# disallow locale control: the directive after the header applies, as the declaration has removed the restriction
# there, the one before it does not (qore#6270); a module is loaded once in a process, so each is loaded once

%modern
%no-child-restrictions

#! loads the module in a new Program and prints the time zone of its code
sub check(string label, string mod, int po) {
    Program p(PO_NEW_STYLE | po);
    try {
        p.parse(sprintf("%%requires %s\nstring sub f() { return %s::tz_region(); }", mod, mod), label);
        printf("%s: %s\n", label, p.callFunction("f"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub main() {
    # a Program with PO_NO_LOCALE_CONTROL
    check("after the header, PO_NO_LOCALE_CONTROL", "TzDomAfter", PO_NO_LOCALE_CONTROL);
    # the module parse option mask adds PO_NO_LOCALE_CONTROL to the Programs of modules
    set_module_parse_option_mask(new ParseOptions(), new ParseOptions(("no-locale-control",)));
    check("before the header, mask", "TzDomBefore", 0);
}

main();
