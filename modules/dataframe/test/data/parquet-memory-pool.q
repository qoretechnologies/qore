#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# child script of parquet-memory-pool.qtest: writes, reads and round-trips data through Parquet and Arrow IPC with
# all the code paths that allocate from an Arrow memory pool (column writers with compression, dictionaries and
# statistics, the stored Arrow schema; page reads with decompression, projections and row groups; IPC readers) and
# prints the results

%new-style
%strict-args
%require-types
%enable-all-warnings

%requires dataframe

string path = ARGV[0] + "/pool.parquet";

list<hash<auto>> rows;
for (int i = 0; i < 200; ++i) {
    rows += {"id": i, "name": sprintf("name-%d", i % 7), "score": i * 1.5, "flag": (i % 3) == 0};
}
DataFrame::DataFrame df(rows);
df.writeParquet(path, <DataFrame::ParquetWriteOptions>{
    "compression": "snappy",
    "use_dictionary": True,
    "write_statistics": True,
    "write_page_index": True,
    "row_group_size": 50,
});

DataFrame::DataFrame whole = DataFrame::DataFrame::readParquet(path);
printf("whole: %d rows, score sum %n\n", whole.numRows(), foldl $1 + $2, whole.getColumn("score"));

DataFrame::DataFrame part = DataFrame::DataFrame::readParquet(path, <DataFrame::ParquetReadOptions>{
    "columns": ("name", "id"),
    "row_groups": (3, 1),
    "use_threads": False,
});
printf("part: %d rows, first id %d, columns %y\n", part.numRows(), part.getColumn("id")[0], part.columnNames());

# store_schema: the Arrow schema is written in the file's key-value metadata
string schema_path = ARGV[0] + "/pool-schema.parquet";
df.writeParquet(schema_path, <DataFrame::ParquetWriteOptions>{"store_schema": True, "compression": "zstd"});
DataFrame::DataFrame with_schema = DataFrame::DataFrame::readParquet(schema_path);
printf("schema: %d rows, dtypes %y\n", with_schema.numRows(), with_schema.dtypes());

DataFrame::DataFrame ipc = DataFrame::DataFrame::fromArrowIpc(whole.toArrowIpc());
printf("ipc: %d rows, last name %s\n", ipc.numRows(), ipc.getColumn("name")[ipc.numRows() - 1]);
