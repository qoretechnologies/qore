#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: Apache-2.0
"""Exercise the packaged public APIs without external servers or source paths."""

import numpy as np
import pyarrow as pa
import pyarrow.acero as acero
import pyarrow.compute as pc
import pyarrow.dataset as ds
import pyarrow.flight as flight
import pyarrow.ipc as ipc
import pyarrow.parquet as parquet
import pytest


def test_arrays_compute_and_ipc():
    values = pa.array([1, None, 3], type=pa.int64())
    assert pc.sum(values).as_py() == 4
    assert pc.add(values, 2).to_pylist() == [3, None, 5]
    assert pa.array(np.array([2, 4], dtype=np.int64())).to_pylist() == [2, 4]
    table = pa.table({"value": values, "label": ["one", "", "třetí"]})
    sink = pa.BufferOutputStream()
    with ipc.new_stream(sink, table.schema) as writer:
        writer.write_table(table)
    assert ipc.open_stream(sink.getvalue()).read_all().equals(table)
    with pytest.raises(pa.ArrowInvalid):
        pa.array(["invalid"], type=pa.int64())


def test_parquet_dataset_and_acero(tmp_path):
    table = pa.table({"value": [1, 2, 3], "label": ["a", "b", "c"]})
    path = tmp_path / "example.parquet"
    parquet.write_table(table, path)
    assert parquet.read_table(path).equals(table)
    filtered = ds.dataset(path, format="parquet").to_table(filter=ds.field("value") > 1)
    assert filtered.to_pydict() == {"value": [2, 3], "label": ["b", "c"]}
    plan = acero.Declaration("table_source", acero.TableSourceNodeOptions(table))
    assert plan.to_table().equals(table)


def test_flight_roundtrip():
    table = pa.table({"value": [1, None, 3], "label": ["first", "", "last"]})

    class Server(flight.FlightServerBase):
        def do_get(self, context, ticket):
            if ticket.ticket != b"example":
                raise KeyError("unknown ticket")
            return flight.RecordBatchStream(table)

    with Server(("127.0.0.1", 0)) as server:
        with flight.connect(("127.0.0.1", server.port)) as client:
            options = flight.FlightCallOptions(timeout=10)
            assert client.do_get(flight.Ticket(b"example"), options).read_all().equals(table)
            with pytest.raises(pa.ArrowKeyError):
                client.do_get(flight.Ticket(b"missing"), options).read_all()
