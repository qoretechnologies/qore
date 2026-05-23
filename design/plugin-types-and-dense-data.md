# Plugin Types and Dense Data

**Status:** Implemented on `feature/5164_jit`.

This document records the implemented plugin-type and dense-data support in
Qore. It replaces the exploratory pending design note and is the checklist to
use when adapting external binary modules.

## Goals

The implementation makes typed columnar data a first-class runtime shape:

- `buffer<T>` stores primitive arrays without per-element boxing.
- `Qore::SQL::ColumnarResult` carries ordered typed SQL/data blocks.
- C++ modules can register plugin types and plugin operations with runtime,
  IR, JIT, AOT, QORD, reflection, diagnostics, and optional LLVM hooks.
- The `dataframe` module registers DataFrame and RowMask plugin operations for
  native subscript, comparison, and mask-composition syntax.
- DataProvider pipelines negotiate `RowRecords`, `HashOfLists`,
  `BufferColumns`, and `DataFrameBlock` shapes and keep data columnar through
  compatible processors.

The design is deliberately conservative: unsupported operations fall back to
row or hash/list semantics with the same result shape when possible instead of
weakening type safety.

## Implemented Runtime Features

### `buffer<T>`

`buffer<T>` is a built-in complex type for dense primitive data. The supported
storage element names are:

- `int8`, `int16`, `int32`, `int64`
- `float32`, `float64`
- `bool`
- `string`
- `decimal128`

Narrow integer and floating storage types are not general scalar local-variable
types. Reading an element widens to Qore `int`, `float`, `bool`, or `string`.
`buffer<T>` intentionally remains a dense primitive-vector type. Nested arrays,
structs, maps, and dictionary-like columns are represented by recursive
`ColumnarResult` schema metadata and list/hash values, with Arrow/Parquet
round-trips preserving the logical schema.

Implemented APIs include list construction, `sized()` and `filled()`
factories, typed indexing and assignment, slicing, zero-copy views, iteration,
numeric reductions, QORD serialization, numeric/bool/string comparisons, and
numeric elementwise arithmetic operators in AST, IR, and JIT execution paths.

### Plugin-Type ABI

The public plugin-type ABI is defined by `include/qore/QorePluginType.h`.
Modules register descriptors during module initialization with
`qore_register_plugin_types_v1()`, using the runtime-owned
`QoreModuleInitContext::plugin_module_handle`.

Implemented infrastructure includes:

- descriptor validation with structured diagnostics and section references
- staged all-or-nothing registration with rollback on module-init failure
- process-global plugin registry and Qore-side `Qore::Reflection::PluginRegistry`
- runtime helper dispatch for value/list helper ABIs
- dense-buffer helper dispatch for raw numeric buffer helpers
- `PLUGIN_IMPORTS`, `PLUGIN_TYPE_REGISTRY`, and `PLUGIN_HELPER_REFS` QORD sections
- serialized plugin value instances through `VT_PLUGIN_INSTANCE`
- Program-local plugin fallback-site diagnostics
- `QORE_PLUGIN_REGISTER_TRACE`, `QORE_PLUGIN_DISPATCH_TRACE`,
  `QORE_PLUGIN_VERIFY`, `QORE_PLUGIN_VERIFY_TRACE`,
  `QORE_PLUGIN_CROSS_TYPE_TRACE`, and `QORE_PLUGIN_QORD_TRACE`
- `examples/plugins/sample-buffer/`
- `examples/plugins/qore-plugin-lint`

### IR, JIT, AOT, and LLVM Hooks

Plugin dispatch is represented by built-in plugin-dispatch IR opcodes carrying
registered operation descriptors, not by externally allocated opcode IDs. This
keeps the binary opcode range stable while allowing modules to participate in
typed lowering and execution.

Implemented execution-tier support includes:

- IR builder, printer, verifier, interpreter, and JIT plumbing
- canonical plugin operation signature hashes
- helper ABI verification and helper return-type verification
- `ReturnsLhs` / `ReturnsRhs` alias-contract verification
- plugin value type profiling and profiled plugin type guards
- optional `QorePluginLLVM.h` codegen extension callbacks
- plugin lowering callbacks invoked before built-in expression lowering
- conservative fast-math gating through both operation metadata and the
  Program-level `%fp-fast-math` parse option

## DataFrame and DataProvider Integration

The dataframe module keeps the public DataFrame class/private-data ownership
model and registers the class as a plugin type. The module registers plugin
operations for:

- `df["column"]`
- `df[row]`
- `df[start..stop]`
- `df[row_mask]`
- `df.column("name")` comparison predicates
- RowMask `&`, `|`, `^`, and `~`

DataProvider shape negotiation is implemented in `qlib/DataProvider`:

- `RowRecords`: one record hash at a time
- `HashOfLists`: bulk record blocks in existing hash-of-lists form
- `BufferColumns`: `Qore::SQL::ColumnarResult` or hash-of-buffers blocks
- `DataFrameBlock`: dataframe module `DataFrame` objects

Implemented shape-aware processors include filter, group-by, select-fields,
remove-fields, record-limit, set-fields, search-replace, running analytics,
correlation, cross-correlation, and DataProviderML processors.
Analytics and DataProviderML processors currently emit hash-of-lists event
blocks after evaluating shaped input.

`QoreFilterRecordsProcessor` compiles supported expressions directly to
DataFrame RowMask or dense `ColumnarResult` masks:

- scalar comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
- computed scalar comparisons over top-level fields, including arithmetic
  expressions such as `@amount * 1.2 >= 100`, `lwr(@status) == "active"`, and
  `coalesce(@override, @value) > 0`
- bare truthy field expressions such as `@flag`
- `between`
- `inRange` with inclusive lower and exclusive upper bounds
- `in`
- `not in`
- null checks
- case-sensitive string predicates: `contains`, `starts-with`, `ends-with`
- BufferColumns `like`, `=~`, and `!~` pattern predicates
- `&&`, `||`, and `!`

Unsupported expressions fall back to generic row evaluation and rebuild the
negotiated output shape where possible.

## DBI and Module Support Matrix

All drivers can use libqore's compatibility fallback for
`Datasource::selectColumnar()` and `SQLStatement::fetchColumnar()` when the
driver does not provide native methods. Native methods are still preferred:
they avoid per-row hash allocation and provide the fastest SQL-read path for
DataFrame/DataProvider workloads.

| Module | Native `selectColumnar()` | Native `fetchColumnar()` | Native dense buffers | Notes |
|--------|---------------------------|--------------------------|----------------------|-------|
| core DBI fallback | compatibility | compatibility | conversion only | Functional baseline for all drivers. |
| `module-pgsql` | yes | yes | yes for numeric/bool/string-compatible columns | Primary benchmarked native path. |
| `module-mysql` | yes | yes | partial, via typed column hash conversion | Uses driver result metadata and `ColumnarResult::fromColumnHash()`. |
| `module-oracle` | yes | yes | partial, via typed column hash conversion | Native DBI methods are registered. |
| `module-odbc` | yes | yes | partial, via typed column hash conversion | Native DBI methods are registered. |
| `module-sqlite3` | yes | yes | conversion only | Native DBI methods are registered; values are shaped through libqore columnar conversion. |
| `module-sybase` / freetds | yes | yes | conversion only | Native DBI methods are registered; values are shaped through libqore columnar conversion. |
| `module-jni` / JDBC | yes | yes | yes for JDBC fixed-width numeric, boolean, string, and decimal128 columns | Builds remain compatible with qore develop through CMake feature probes. |

When applying this work to external modules, check:

1. CMake feature detection for the columnar DBI API.
2. Driver capability flags, especially `DBI_CAP_HAS_COLUMNAR_SELECT`.
3. Native `Datasource::selectColumnar()` support.
4. Native `SQLStatement::fetchColumnar()` support.
5. Null handling and column metadata preservation.
6. Direct dense buffer construction for numeric, boolean, and string columns where the
   driver API exposes typed storage.
7. Release notes and module docs describing native vs fallback behavior.
8. Tests for empty results, null columns, partial fetches, and fallback
   behavior.

## Deferred Dense Types

The following are intentionally deferred API work, not partially implemented
features:

| Feature | Status | Rationale |
|---------|--------|-----------|
| `buffer<string>` / `buffer<*string>` | implemented | Uses offsets + UTF-8 byte storage in `QoreBufferNode` with optional validity bitmap. |
| `buffer<decimal128>` | implemented | Uses fixed-width signed decimal128 storage with precision/scale metadata and overflow checks. |
| nested buffers / array columns | implemented through recursive column schemas | Nested Arrow/Parquet columns keep recursive schema metadata and reuse immutable Arrow chunked arrays for round trips; direct `buffer<list<...>>` syntax is intentionally outside the primitive `buffer<T>` contract. |
| GPU/device buffers | implemented as provider-owned external storage | `QoreBufferNode` exposes provider-neutral device descriptors, explicit copy-to-host callbacks, Qore storage-inspection methods, and detach-on-write semantics. |
| native JDBC packed buffers | implemented for host buffers | module-jni constructs packed Qore buffers for fixed-width numeric, boolean, string, and decimal128 columns and uses Java-side primitive-array batch extraction when built with the new qore feature probes; unsupported shapes fall back to typed getter conversion. |

Short-term mappings remain:

- SQL string/text columns use `buffer<string>` or `buffer<*string>` when columnar metadata or inferred values identify
  a string column.
- SQL date/time values use Qore date lists unless a driver has a more specific
  dense mapping.
- SQL `NUMERIC` / `DECIMAL` values use `buffer<decimal128>` when precision and
  scale fit the fixed-width decimal representation; otherwise they use
  `list<number>` with decimal schema metadata.

## Performance and Optimization Status

The implemented hot paths are:

- typed DataFrame construction
- direct `ColumnarResult` to/from DataFrame conversion
- dense boolean mask filtering
- RowMask predicate composition
- horizontal concat
- DataProvider filter/group/projection/limit/set/search-replace shape
  preservation
- native PostgreSQL columnar SQL reads
- SqlUtil/BulkSqlUtil-backed DataFrame SQL writes

The benchmark suite lives under `bench/`. On the measured local setup with a
native PostgreSQL columnar driver, Qore was faster than pandas for the included
typed column construction, filtering, aggregation, join, value-counting,
missing-data cleanup, reshape, Arrow/Parquet roundtrip, SQL read/write,
DataProvider pipeline, and ML analytics cases.

Further optimization work should require both a correctness test and a
benchmark showing that the added specialization pays for itself. Good future
targets are:

- more DPQL expression forms lowered to dense masks when they can preserve
  generic expression semantics without hidden row materialization
- wider vectorized analytics outputs
- extending dense string predicate lowering to case-folded and locale-sensitive
  forms where benchmarks show a payoff and semantics can be documented clearly
- direct packed-column support in more DBI drivers
- more direct Arrow C Data consumers in binary modules that need libqore-level
  columnar interchange without linking against Arrow C++; module-grpc already
  uses this bridge when built against a qore runtime that exports it
- benchmark-driven native codegen callbacks for module-owned dense kernels

## Documentation and Verification Checklist

Before committing changes in this area:

1. Update `doxygen/lang/224_plugin_types.dox.tmpl` for language/API behavior.
2. Update `doxygen/lang/150_container_data_types.dox.tmpl` for
   `ColumnarResult` / dense container behavior.
3. Update `modules/dataframe/docs/mainpage.dox.tmpl` for DataFrame user API
   behavior.
4. Update `doxygen/lang/900_release_notes.dox.tmpl`.
5. Add or update tests in `modules/dataframe/test/dataframe.qtest`,
   `examples/test/qlib/DataProvider/`, or DBI module tests.
6. Run targeted docs builds with `docs-lang-final-fast` and affected module
   doc targets.
7. Run targeted qtests in AST/IR/JIT/tiered modes when parser, type, IR, or
   plugin dispatch behavior changes.
8. Run the dataframe benchmark suite when claiming performance improvements.

## Public Compatibility Rules

- Plugin descriptors are ABI contracts once external modules ship against them.
- Omitted semantic metadata means conservative behavior.
- Algebraic metadata such as purity, commutativity, associativity, alias
  contracts, and floating-point reassociation must not be used for rewrites
  unless the operation descriptor explicitly opts in.
- Plugin imports in QORD artifacts are hard requirements. A missing plugin
  import is not a soft optimization fallback.
- LLVM codegen extensions are opt-in and tied to the libqore LLVM major
  version. Runtime-helper fallback should remain available unless the module
  deliberately marks the LLVM extension as required.
