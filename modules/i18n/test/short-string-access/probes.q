# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The probes of short-string-access.qtest: each passes strings of 6 bytes or fewer, which a value can hold inline
# (with no QoreStringNode), to the i18n module in the values of an option hash, and prints what the module made of
# them.  A qcc-compiled executable holds the strings of a constant hash inline; in a debug build ss() makes them
# inline in every execution mode (dbg_make_short_string()).

%modern
%requires i18n

#! the strings of the probes: a qcc-compiled executable holds the strings of a constant hash inline
const Short = {
    "/x": "/x",
    "/y": "/y",
};

#! returns the string: inline in a debug build, and from the constant hash otherwise
string sub ss(string s) {
%ifdef QoreDebug
    return dbg_make_short_string(s);
%else
    return Short{s};
%endif
}

#! the probes in the order that they run
const Probes = (
    # a catalog search path option given as a string
    "search_path",
    "override_path",
);

#! runs a probe and returns its result
string sub run_probe(string name) {
    switch (name) {
        case "search_path":
            return I18n::get_catalog_search_path({"search_path": ss("/x")}).join(",");
        case "override_path":
            return I18n::get_catalog_search_path({"search_path": ss("/x"), "override_path": ss("/y")}).join(",");
    }
    throw "PROBE-ERROR", sprintf("unknown probe %y", name);
}

foreach string name in (ARGV ?: Probes) {
    try {
        printf("%s: %s\n", name, run_probe(name));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}
