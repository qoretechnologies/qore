#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*  default-encoding-names.q Copyright 2026 Qore Technologies, s.r.o.
    MIT License

    The child script of dataframe-default-encoding-names.qtest, run with a default character encoding given by the
    test; it has only ASCII characters, so that it means the same in any default encoding, and prints each result as
    "<name>=<hex bytes>".

    usage: default-encoding-names.q write|read <dir>
*/

%modern
%requires dataframe

string mode = ARGV[0];
string dir = ARGV[1];
# "cafe" with an e acute, in ISO-8859-1
string iso_cafe = binary_to_string(<636166e9>, "ISO-8859-1");
# the string in the default encoding of the process
string cafe = convert_encoding(iso_cafe, get_default_encoding());

sub out(string name, data value) {
    printf("%s=%s\n", name, make_hex_string(binary(value)));
}

out("default", get_default_encoding());

if (mode == "write") {
    # a column name and a hash key (the name of a struct field) in the default encoding
    DataFrame::DataFrame df = DataFrame::DataFrame::fromColumnarResult(ColumnarResult::fromColumnHash({
        cafe: (1,),
        "h": ({cafe: 2},),
    }, {
        "h": {"schema": {"kind": "struct", "nullable": False, "children": (
            {"name": cafe, "kind": "int", "nullable": False},
        )}},
    }));
    df.writeParquet(dir + "/names.parquet");
    DataFrame::DataFrame read = DataFrame::DataFrame::readParquet(dir + "/names.parquet");
    out("write-column", read.columnNames()[0]);
    out("write-key", (keys read.getColumn("h")[0])[0]);

    # a CSV file in the default encoding with a null string given in UTF-8
    File f();
    f.open2(dir + "/null.csv", O_CREAT | O_WRONLY | O_TRUNC);
    f.write(binary("v\n") + binary(iso_cafe) + binary("\nx\n"));
    f.close();
    DataFrame::DataFrame csv = DataFrame::DataFrame::readCSV(dir + "/null.csv",
        <DataFrame::CsvOptions>{"null_string": convert_encoding(iso_cafe, "UTF-8")});
    out("csv-null", exists csv.getColumn("v")[0] ? "value" : "null");
} else {
    DataFrame::DataFrame read = DataFrame::DataFrame::readParquet(dir + "/names.parquet");
    out("read-column", read.columnNames()[0]);
    out("read-key", (keys read.getColumn("h")[0])[0]);
}
