# Arrow/DataFrame Integration

**Status:** implementation in progress on `feature/5164_jit`.

This document is the implementation checklist for complete Apache Arrow,
Parquet, DataFrame, DataProvider, Arrow Flight, and JDBC dense-column
integration.  The goal is one columnar data model across Qore: use
`Qore::SQL::ColumnarResult` as the shared runtime interchange type, preserve
dense `buffer<T>` values when possible, and only materialize rows when an API or
processor explicitly needs row objects.

## Goals

- DataFrame, Parquet, Arrow IPC, Arrow Flight, DBI, JDBC, and DataProvider
  pipelines exchange data through `ColumnarResult`.
- Arrow/Parquet conversions preserve schema metadata, nullability, timestamp
  units/time zones, decimals, strings, binary values, and nested column shapes.
- Supported primitive columns can move between Qore and Arrow without copying
  when ownership, alignment, and mutability rules allow it.
- Unsupported or lossy conversions are explicit in API options and diagnostics;
  they must not silently weaken type safety.
- External modules can detect support at CMake/preprocessor time and keep
  compatibility with plain `develop` builds.

## Feature Detection Contract

Define these only when the matching APIs and tests are implemented:

| Macro / CMake variable | Meaning |
|------------------------|---------|
| `QORE_HAVE_COLUMNAR_RESULT_V2` | `QoreColumnarResult` exposes recursive schema descriptors and v2 add/import APIs. |
| `QORE_HAVE_ARROW_C_DATA_INTEROP` | libqore can import/export `ColumnarResult` through the Arrow C Data Interface. |
| `QORE_HAVE_EXTERNAL_BUFFER_STORAGE` | `QoreBufferNode` can safely wrap external immutable buffer storage with owner lifetime tracking. |
| `QORE_HAVE_DECIMAL128_BUFFER` | `buffer<decimal128>` or equivalent fixed-width decimal storage is available. |
| `QORE_HAVE_ARROW_DATAFRAME_INTEROP` | DataFrame exposes Arrow/ColumnarResult APIs suitable for module-grpc integration. |

The installed `QoreConfig.cmake` must export equivalent CMake variables so
modules can conditionally compile:

```cmake
if(QORE_HAVE_ARROW_DATAFRAME_INTEROP)
    target_compile_definitions(my_module PRIVATE QORE_HAVE_ARROW_DATAFRAME_INTEROP=1)
endif()
```

## Architecture

### Shared Type

`QoreColumnarResult` is the canonical interchange type.  DataFrame owns its
analytics API and may keep its internal column layout, but all external
interchange APIs should use `ColumnarResult` first.  Arrow wrappers in
module-grpc should add `ColumnarResult` constructors and `toColumnarResult()`
instead of adding a third public column format.

### Schema V2

`ColumnarResult` needs recursive schema descriptors for:

- primitive scalars: bool, signed integers, floating types, string, binary,
  date, timestamp, and duration;
- decimal values: `decimal128(precision, scale)` first;
- nested values: list, large-list, fixed-size-list, struct, and map;
- optional dictionary metadata;
- nullability at every schema node;
- source/native metadata for DBI, Arrow, Parquet, and JDBC.

The current flat schema remains a compatibility view.

### Zero-Copy Rules

Zero-copy import/export is allowed only when all of the following are true:

- the source memory outlives the Qore value through an owned holder;
- the physical layout is byte-compatible or explicitly bit-compatible;
- mutable Qore operations trigger copy-on-write before modifying external
  storage;
- slices preserve owner references and offsets;
- cancellation checks exist in any conversion loop that can process more than
  100 rows or elements.

If any rule fails, conversion must copy and report the copy mode in diagnostics
where an API exposes conversion details.

## Module Work

### qore / libqore

- Extend `QoreColumnarResult` schema metadata.
- Add v2 column insertion APIs while preserving existing APIs.
- Add external immutable storage support to `QoreBufferNode`.
- Add Arrow C Data import/export wrappers behind stable libqore APIs.
- Add decimal128 dense storage or an equivalent fixed-width decimal column type.
- Add nested column containers and row-materialization helpers.
- Update `ColumnarResult` filter/slice/null-mask behavior for new column
  shapes where meaningful.

### dataframe

- Route Parquet read/write through the shared Arrow/ColumnarResult bridge.
- Add DataFrame Arrow IPC import/export APIs.
- Preserve decimal and nested schema in `fromColumnarResult()` and
  `toColumnarResult()`.
- Keep row/hash/list materialization explicit.
- Document copy vs zero-copy behavior.

### module-grpc

- Keep existing Arrow schema, IPC, and RecordBatch APIs.
- Add `ArrowRecordBatch(ColumnarResult)` and
  `ArrowRecordBatch::toColumnarResult()`.
- Make Arrow Flight readers yield `ColumnarResult` blocks by default.
- Make Arrow Flight writers accept `ColumnarResult`, DataFrame, hash-of-lists,
  and row records.
- Make ArrowFlightDataProvider advertise and preserve `BufferColumns`, with
  `DataFrameBlock` support when the dataframe module is available.
- Compile the new path only when qore exposes the feature-detection contract.

### module-jni

- Add native JDBC `select_columnar` and statement `fetch_columnar` methods.
- Map `ResultSetMetaData` to `ColumnarResult` schema v2.
- Fill typed buffers with JDBC typed getters and `wasNull()`.
- Add a Java helper path using primitive arrays or direct buffers to reduce
  per-cell JNI overhead.
- Wrap direct buffers zero-copy when supported, otherwise copy into Qore
  buffers.
- Map `BigDecimal` to decimal128 when precision and scale fit.

## Verification Checklist

- qore build and dataframe build.
- dataframe qtest, including Parquet, decimal, nested, and ColumnarResult
  round trips.
- module-grpc Arrow IPC and Arrow Flight qtests.
- module-jni JDBC tests with at least one PostgreSQL JDBC path and one simple
  embedded/local driver path when available.
- docs fast targets for qore, dataframe, grpc, and jni.
- valgrind DataFrame/Parquet/Arrow tests with `ARROW_DEFAULT_MEMORY_POOL=system`
  when the packaged Arrow build uses mimalloc.
- Benchmarks covering Parquet, Arrow IPC, Arrow Flight, DataFrame construction,
  DataFrame export, and JDBC `selectColumnar()`.

## Commit Checklist

Before every commit in this area:

1. Run the `audit-changes` skill against staged or uncommitted changes.
2. Check that feature macros do not over-promise unimplemented APIs.
3. Run `git diff --check`.
4. Run the narrowest build/test target that exercises changed behavior.
5. Update this design document if an implementation detail or deferred item
   changes.
