#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# helper of TabularProfilerEncoding.qtest: run with a default encoding other than UTF-8 (--charset=ISO-8859-1); it
# loads the DataProvider module from the module path (the AOT module of the build, when run_tests.sh puts it there)
# and profiles a sheet with empty markers; it prints where the module was loaded from and the markers of each column

%modern

%requires DataProvider

printf("module: %s\n", get_module_hash().DataProvider.filename =~ /\.qmod$/ ? "qmod" : "source");
# a Latin-1 letter, which the default encoding of the process represents
string latin = binary_to_string(<67726fc39f65>, "UTF-8");
# a yes/no column in Spanish ("sí" and "no"): the words are compared with the pairs of TabularProfiler::BoolPairs,
# which hold words that ISO-8859-1 cannot represent ("✓")
string si = binary_to_string(<73c3ad>, "UTF-8");
list<list<auto>> rows = (
    ("SKU", "Note", "Name", "Flag"),
    ("A", "n/a", latin, si),
    ("B", "-", "TBD", "no"),
    ("C", "N/A", latin, si),
);
hash<TabularWorkbookProfile> p = TabularProfiler::profile(<TabularWorkbookGrid>{
    "format": "csv",
    "sheets": (<TabularSheetGrid>{"name": "s", "rows": rows},),
});
foreach hash<TabularColumnProfile> c in (p.sheets[0].columns) {
    printf("markers %d: %y\n", $#, c.empty_markers);
}
foreach hash<TabularColumnProfile> c in (p.sheets[0].columns) {
    printf("type %d: %s\n", $#, c.type);
}
printf("default %s\n", get_default_encoding());
