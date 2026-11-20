#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# helper of TabularProfilerEncoding.qtest: run with a default encoding other than UTF-8 (--charset=ISO-8859-1) and
# with UTF-8; it profiles a sheet whose text has characters that ISO-8859-1 cannot represent ("—", "✓", CJK, Greek and
# Cyrillic text) in values, header labels, yes/no values, currencies, formulas and empty markers (issue #6666), and
# prints each profile as the hex digits of the UTF-8 bytes of a rendering of it, which the parent decodes

%modern

%requires DataProvider

#! the text of a literal of this file as UTF-8: the file is UTF-8, but a process tags a literal with its own encoding
string sub t(string literal) {
    return binary_to_string(binary(literal), "UTF-8");
}

#! renders a value as UTF-8 text: strings in double quotes, the keys of a hash in their order
string sub render(auto v) {
    string rv = convert_encoding("", "UTF-8");
    switch (v.typeCode()) {
        case NT_HASH: {
            rv += "{";
            foreach hash<auto> i in (v.pairIterator()) {
                if ($#) {
                    rv += ", ";
                }
                rv += render(i.key) + ": " + render(i.value);
            }
            rv += "}";
            break;
        }
        case NT_LIST: {
            rv += "[";
            foreach auto e in (v) {
                if ($#) {
                    rv += ", ";
                }
                rv += render(e);
            }
            rv += "]";
            break;
        }
        case NT_STRING:
            # each part is added to the UTF-8 string: "\"" + v would convert v to the encoding of the literal
            rv += "\"";
            rv += convert_encoding(v, "UTF-8");
            rv += "\"";
            break;
        default:
            rv += sprintf("%y", v);
    }
    return rv;
}

#! prints a labelled rendering as the hex digits of its UTF-8 bytes
sub show(string label, auto v) {
    printf("%s %s\n", label, binary(render(v)).toHex());
}

list<list<auto>> rows = (
    ("SKU", t("Note"), t("日本"), "Flag", t("Δelta"), "Price", "Mark", "Pair", t("δELTA"), "Accent"),
    ("A1", t("—"), t("日本語"), t("✓"), "a", t("€5.00"), t("✓"), t("日本"), "1", t("é")),
    ("A2", t("–"), t("中文"), t("✗"), "b", t("€7.50"), "", t("中国"), "2", t("É")),
    ("A3", t("…"), t("한국어 텍스트가 아주 길어서 잘립니다 정말로요"), t("✓"), "c", t("−€3.00"), t("✓"), t("日本"), "3",
        t("é")),
    ("A4", "n/a", t("—✓"), t("✗"), "d", t("€12.50"), "", t("中国"), t("—x"), t("é")),
    # a string in ISO-8859-1, as a reader may deliver it
    ("A5", t("—"), "x", t("✓"), "e", t("€1.00"), t("✓"), t("日本"), "5", convert_encoding(t("é"), "ISO-8859-1")),
    (t("Итого"), "", "", "", "", t("€22.00"), "", "", "", ""),
);
hash<TabularWorkbookGrid> grid({
    "format": "xlsx",
    "sheets": (<TabularSheetGrid>{
        "name": t("Лист"),
        "rows": rows,
        "formulas": {"C3": t("CONCAT(\"—\", \"日本\")")},
    },),
    "named_ranges": {"r": t("'日本'!$A$1:$B$2")},
});
show("redacted", TabularProfiler::profile(grid));
show("unredacted", TabularProfiler::profile(grid, <TabularProfileOptions>{"redact": False}));
# the same grid as a plain hash, as a Java reader returns it
hash<auto> plain = grid;
plain.sheets = map $1{"name", "rows", "formulas"}, grid.sheets;
show("from_hash", TabularProfiler::profile(TabularProfiler::gridFromHash(plain)));
# binary cells: bytes that are UTF-8, bytes that only ISO-8859-1 reads, and bytes that neither UTF-8 nor ISO-8859-1
# can read in a UTF-8 process
show("binary", TabularProfiler::profile(<TabularWorkbookGrid>{
    "format": "csv",
    "sheets": (<TabularSheetGrid>{
        "name": "b",
        "rows": ((<fffe>, "I"), (<fffe>, "a"), (binary(t("日本")), "b"), (<e9>, "c"), (binary("ab"), "d")),
    },),
}));
# a yes/no column of binary cells ("Sí" and "no"), which the values of a column are read from as text
show("binary_bool", TabularProfiler::profile(<TabularWorkbookGrid>{
    "format": "csv",
    "sheets": (<TabularSheetGrid>{
        "name": "y",
        "rows": (("Flag",), (binary(t("Sí")),), (binary("no"),), (binary(t("Sí")),), (binary("no"),)),
    },),
}));
# a currency in text in ISO-8859-1, as a reader may deliver it: the symbol is extracted by a regular expression
show("iso_currency", TabularProfiler::profile(<TabularWorkbookGrid>{
    "format": "csv",
    "sheets": (<TabularSheetGrid>{
        "name": "c",
        "rows": (("P",), (convert_encoding(t("£5"), "ISO-8859-1"),), (convert_encoding(t("£7.50"), "ISO-8859-1"),)),
    },),
}));
printf("default %s\n", get_default_encoding());
