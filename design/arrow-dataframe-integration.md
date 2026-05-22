# Arrow/DataFrame Integration

**Status:** implemented on `feature/5164_jit`; verification uses targeted
builds, qtests, docs-fast targets, Valgrind, and focused benchmarks.

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
| `QORE_HAVE_COLUMNAR_RESULT_V2` | Implemented: `QoreColumnarResult` exposes recursive schema descriptors and v2 schema APIs. |
| `QORE_HAVE_ARROW_C_DATA_INTEROP` | libqore can import/export `ColumnarResult` through the Arrow C Data Interface. |
| `QORE_HAVE_EXTERNAL_BUFFER_STORAGE` | Implemented: `QoreBufferNode` can wrap immutable external fixed-width storage with owner lifetime tracking and detach-on-write mutation. |
| `QORE_HAVE_DECIMAL128_BUFFER` | `buffer<decimal128>` or equivalent fixed-width decimal storage is available. |
| `QORE_HAVE_ARROW_DATAFRAME_INTEROP` | Implemented: DataFrame exposes in-memory Arrow IPC import/export APIs and ColumnarResult-compatible dense storage paths suitable for module-grpc integration. |

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

- Implemented: `QoreColumnarResult` schema v2 metadata and v2 column insertion
  APIs while preserving the flat compatibility schema.
- Implemented: external immutable fixed-width storage support in
  `QoreBufferNode`, with owner lifetime tracking and copy-on-write mutation.
- Implemented: decimal and nested schema metadata through recursive
  `ColumnarResult` descriptors; DataFrame and Arrow/Parquet interop preserve
  the logical schema through lossless `auto` storage when dense physical storage
  is not available.
- Implemented: `ColumnarResult` filter, slice, null-mask, and mask-composition
  behavior for dense fixed-width masks and compatible column blocks.
- Implemented: DataProvider bulk record iterators expose a shaped accessor so
  native columnar sources can pass `ColumnarResult` and other `BufferColumns`
  blocks through pipelines without forcing the compatibility `hash<list>` view.
- Not exported in the current feature contract: `QORE_HAVE_ARROW_C_DATA_INTEROP`
  and `QORE_HAVE_DECIMAL128_BUFFER`.  These macros remain undefined because
  stable libqore Arrow C Data ABI wrappers and dense `buffer<decimal128>` storage
  are not implemented.

### dataframe

- Route Parquet read/write through the shared Arrow/ColumnarResult bridge.
  Decimal and nested Arrow/Parquet columns now map through lossless DataFrame
  `auto` storage with recursive schema metadata instead of scalar string
  fallbacks.
- Add DataFrame Arrow IPC import/export APIs.  Implemented as
  `DataFrame::fromArrowIpc(binary)` and `DataFrame::toArrowIpc()`;
  fixed-width Arrow columns are imported through dense DataFrame buffers when
  possible.
- Preserve decimal and nested schema in `fromColumnarResult()` and
  `toColumnarResult()`.  Implemented with lossless DataFrame `auto` column
  storage for recursive `ColumnarResult` schemas, including decimal, binary,
  list, struct, map, and dictionary-compatible values.
- Keep row/hash/list materialization explicit.
- Document copy vs zero-copy behavior.

### module-grpc

- Keep existing Arrow schema, IPC, and RecordBatch APIs.
- Add `ArrowRecordBatch(ColumnarResult)` and
  `ArrowRecordBatch::toColumnarResult()`.  Implemented with conditional CMake
  probes so the module still builds against qore versions without the new
  feature contract.
- Make Arrow Flight readers yield `ColumnarResult` blocks by default.
  Implemented as explicit `readColumnarResult()` helpers to preserve source
  compatibility.
- Make Arrow Flight writers accept `ColumnarResult`, DataFrame, hash-of-lists,
  and row records.  Implemented through shared record-batch coercion helpers.
- Make ArrowFlightDataProvider advertise and preserve `BufferColumns`, with
  `DataFrameBlock` support when the dataframe module is available.
  Implemented for DoGet bulk reads through `DataProvider` shaped bulk blocks;
  request/event writers now accept `ColumnarResult`, DataFrame, hash-of-lists,
  row lists, and `ArrowRecordBatch` values.  The request API remains row-list
  compatible by default and accepts `output_shape: "columnar"` for explicit
  ColumnarResult batch responses.
- Compile the new path only when qore exposes the feature-detection contract.

### module-jni

- Add native JDBC `select_columnar` and statement `fetch_columnar` methods.
  Implemented in module-jni with conditional CMake probes so the module still
  builds against qore versions without `QORE_HAVE_COLUMNAR_RESULT_V2`.
- Map `ResultSetMetaData` to `ColumnarResult` schema v2.  Implemented for JDBC
  type names, precision, scale, and nullability.
- Fill typed buffers with JDBC typed getters and `wasNull()`.  Implemented for
  fixed-width integer, floating-point, boolean, and safe integer decimal
  columns using C++ external buffer owners.
- Map `BigDecimal` to decimal128 schema metadata when precision and scale fit.
  Dense decimal128 buffers remain unavailable until
  `QORE_HAVE_DECIMAL128_BUFFER` is implemented; decimal values that cannot be
  represented as fixed-width integers use list storage with recursive schema
  metadata.
- Java-side primitive-array/direct-buffer batch extraction is not part of the
  current compatibility contract.  The implemented JDBC path already preserves
  packed Qore buffers for supported fixed-width columns, but still calls JDBC
  typed getters per cell.

## Verification Checklist

- qore build and dataframe build.
- dataframe qtest, including Parquet, decimal, nested, and ColumnarResult
  round trips.
- module-grpc Arrow IPC and Arrow Flight qtests.
- module-jni JDBC tests with at least one PostgreSQL JDBC path and one simple
  embedded/local driver path when available.  PostgreSQL JDBC verification has
  been run for `selectColumnar()` and statement `fetchColumnar()`; embedded
  local-driver coverage remains dependent on an available driver fixture.
- docs fast targets for qore, dataframe, grpc, and jni.
- valgrind DataFrame/Parquet/Arrow tests with `ARROW_DEFAULT_MEMORY_POOL=system`
  when the packaged Arrow build uses mimalloc.
- Benchmarks cover DataFrame construction, DataFrame-to-ColumnarResult export,
  Parquet round trips, Arrow IPC round trips, Arrow Flight columnar reads,
  DataProvider columnar pipeline paths, SQL read/write, and optional JDBC
  `selectColumnar()` when module-jni and a JDBC connection string are available.

## Commit Checklist

Before every commit in this area:

1. Run the `audit-changes` skill against staged or uncommitted changes.
2. Check that feature macros do not over-promise unimplemented APIs.
3. Run `git diff --check`.
4. Run the narrowest build/test target that exercises changed behavior.
5. Update this design document if an implementation detail or deferred item
   changes.
