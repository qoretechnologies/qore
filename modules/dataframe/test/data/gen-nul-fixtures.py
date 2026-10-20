#!/usr/bin/env python3
# -*- mode: python; indent-tabs-mode: nil -*-
#
# gen-nul-fixtures.py Copyright 2026 Qore Technologies, s.r.o.
# MIT License
#
# Generates the Parquet and Arrow IPC files with NUL characters in column names, string values, struct field names
# and map keys that dataframe.qtest reads.  A DataFrame cannot write all of them itself: struct and map values are
# Qore hashes, whose keys cannot contain a NUL character.
#
# usage: python3 -I gen-nul-fixtures.py <output-dir>   (requires pyarrow)

import sys

import pyarrow as pa
import pyarrow.parquet as pq


def write(table, out, name, row_group_size=None):
    pq.write_table(table, out + "/" + name + ".parquet", row_group_size=row_group_size)
    with pa.OSFile(out + "/" + name + ".arrow", "wb") as sink:
        with pa.ipc.new_file(sink, table.schema) as writer:
            writer.write_table(table)


def main():
    out = sys.argv[1]
    # one row per row group, so that a filter can skip row groups by their statistics
    write(pa.table({
        "na\x00me": pa.array(["v\x00w", "x", "y"], type=pa.string()),
        "plain": pa.array(["a", "a\x00b", "b"], type=pa.string()),
    }), out, "nul_strings", row_group_size=1)
    write(pa.table({
        "st": pa.array([{"a\x00b": 1}, {"a\x00b": 2}], type=pa.struct([("a\x00b", pa.int64())])),
    }), out, "nul_struct")
    # only null values: the file can be read, and the field name is kept in the schema of the column
    write(pa.table({
        "st": pa.array([None, None], type=pa.struct([("a\x00b", pa.int64())])),
    }), out, "nul_struct_null")
    write(pa.table({
        "mp": pa.array([[("k\x00x", 1), ("k", 2)], [("k", 3)]], type=pa.map_(pa.string(), pa.int64())),
    }), out, "nul_map")


if __name__ == "__main__":
    main()
