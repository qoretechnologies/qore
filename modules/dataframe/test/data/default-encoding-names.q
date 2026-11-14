#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*  default-encoding-names.q Copyright 2026 Qore Technologies, s.r.o.
    MIT License

    The child script of dataframe-default-encoding-names.qtest, run with a default character encoding given by the
    test; it has only ASCII characters, so that it means the same in any default encoding, and prints each result as
    "<name>=<hex bytes>".

    usage: default-encoding-names.q write|read|read-iso <dir>
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

# prints the error code of the call as a hex string, or "none"
sub err(string name, code c) {
    try {
        c();
        out(name, "none");
    } catch (hash<ExceptionInfo> ex) {
        out(name, ex.err);
    }
}

# a frame with a column name, a struct field name and a map key in the default encoding
DataFrame::DataFrame sub names_frame(string name) {
    return DataFrame::DataFrame::fromColumnarResult(ColumnarResult::fromColumnHash({
        name: (1,),
        "h": ({name: 2},),
        "m": ({name: 3},),
    }, {
        "h": {"schema": {"kind": "struct", "nullable": False, "children": (
            {"name": name, "kind": "int", "nullable": False},
        )}},
        "m": {"schema": {"kind": "map", "nullable": False, "children": (
            {"name": "key", "kind": "string", "nullable": False},
            {"name": "value", "kind": "int", "nullable": True},
        )}},
    }));
}

out("default", get_default_encoding());

if (mode == "write") {
    # a column name and hash keys (the name of a struct field, a map key) in the default encoding
    DataFrame::DataFrame df = names_frame(cafe);
    df.writeParquet(dir + "/names.parquet");
    DataFrame::DataFrame read = DataFrame::DataFrame::readParquet(dir + "/names.parquet");
    out("write-column", read.columnNames()[0]);
    out("write-key", (keys read.getColumn("h")[0])[0]);
    out("write-map-key", (keys read.getColumn("m")[0])[0]);
    # the columns and filters options name a column in the default encoding
    read = DataFrame::DataFrame::readParquet(dir + "/names.parquet",
        <DataFrame::ParquetReadOptions>{"columns": (cafe,)});
    out("write-columns-option", read.columnNames()[0]);
    read = DataFrame::DataFrame::readParquet(dir + "/names.parquet",
        <DataFrame::ParquetReadOptions>{"columns": ("h",), "filters": (
        {"column": cafe, "op": "==", "value": 1},
    )});
    out("write-filters-option", sprintf("%y:%d", read.columnNames(), read.numRows()));
    # Arrow IPC
    binary ipc = df.toArrowIpc();
    File ipc_file();
    ipc_file.open2(dir + "/names.arrow", O_CREAT | O_WRONLY | O_TRUNC);
    ipc_file.write(ipc);
    ipc_file.close();
    read = DataFrame::DataFrame::fromArrowIpc(ipc);
    out("write-ipc-column", read.columnNames()[0]);
    out("write-ipc-map-key", (keys read.getColumn("m")[0])[0]);

    # a CSV file in the default encoding with a null string given in UTF-8
    File f();
    f.open2(dir + "/null.csv", O_CREAT | O_WRONLY | O_TRUNC);
    f.write(binary("v\n") + binary(iso_cafe) + binary("\nx\n"));
    f.close();
    DataFrame::DataFrame csv = DataFrame::DataFrame::readCSV(dir + "/null.csv",
        <DataFrame::CsvOptions>{"null_string": convert_encoding(iso_cafe, "UTF-8")});
    out("csv-null", exists csv.getColumn("v")[0] ? "value" : "null");
    # a null string that the default encoding cannot represent: a euro sign
    err("csv-null-euro", sub () {
        DataFrame::DataFrame euro_csv = DataFrame::DataFrame::readCSV(dir + "/null.csv",
            <DataFrame::CsvOptions>{"null_string": binary_to_string(<e282ac>, "UTF-8")});
    });
    # a null string from a string cell, which is a short string with no string node
    DataFrame::DataFrame cells({"s": ("x", "yz")});
    DataFrame::DataFrame short_csv = DataFrame::DataFrame::readCSV(dir + "/null.csv",
        <DataFrame::CsvOptions>{"null_string": cells.toRecords()[0].s});
    out("csv-null-short", exists short_csv.getColumn("v")[1] ? "value" : "null");
    # an error message with a column name from the file, which is in UTF-8, in the default encoding
    try {
        DataFrame::DataFrame missing = DataFrame::DataFrame::readParquet(dir + "/names.parquet",
            <DataFrame::ParquetReadOptions>{"columns": ("nope",)});
        out("missing-column-desc", "none");
    } catch (hash<ExceptionInfo> ex) {
        out("missing-column-desc", convert_encoding(ex.desc, get_default_encoding()).find(cafe) >= 0
            ? "found" : "missing");
    }
} else if (mode == "read") {
    DataFrame::DataFrame read = DataFrame::DataFrame::readParquet(dir + "/names.parquet");
    out("read-column", read.columnNames()[0]);
    out("read-key", (keys read.getColumn("h")[0])[0]);
    out("read-map-key", (keys read.getColumn("m")[0])[0]);
    read = DataFrame::DataFrame::readParquet(dir + "/names.parquet",
        <DataFrame::ParquetReadOptions>{"columns": (cafe,)});
    out("read-columns-option", read.columnNames()[0]);
    read = DataFrame::DataFrame::fromArrowIpc(ReadOnlyFile::readBinaryFile(dir + "/names.arrow"));
    out("read-ipc-column", read.columnNames()[0]);
    out("read-ipc-map-key", (keys read.getColumn("m")[0])[0]);
    # names that ISO-8859-1 cannot represent, for read-iso
    string euro = binary_to_string(<e282ac>, "UTF-8");
    names_frame(euro).writeParquet(dir + "/euro-column.parquet");
    DataFrame::DataFrame euro_key = DataFrame::DataFrame::fromColumnarResult(ColumnarResult::fromColumnHash({
        "m": ({euro: 3},),
    }, {
        "m": {"schema": {"kind": "map", "nullable": False, "children": (
            {"name": "key", "kind": "string", "nullable": False},
            {"name": "value", "kind": "int", "nullable": True},
        )}},
    }));
    euro_key.writeParquet(dir + "/euro-key.parquet");
} else {
    # names in the file that the default encoding cannot represent
    err("iso-euro-column", sub () { DataFrame::DataFrame df = DataFrame::DataFrame::readParquet(dir
        + "/euro-column.parquet"); });
    err("iso-euro-map-key", sub () { DataFrame::DataFrame df = DataFrame::DataFrame::readParquet(dir
        + "/euro-key.parquet"); });
}
