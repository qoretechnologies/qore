# Plugin Types and Dense Data — Design

**Status:** Design.

**Target:** Pending — strategic direction proposal. This document captures the
language and runtime shape for a plugin-type extensibility protocol in Qore,
motivated by the analysis of the existing `dataframe` / `DataProvider` /
`DPQL` / `BulkSqlUtil` ecosystem and the gap between that ecosystem and a
credible in-process enterprise data/analytics platform.

**Branch context:** Targets Qore on top of the IR/JIT/AOT pipeline. See
[`design/qore-jit-aot-current-state.md`](../design/qore-jit-aot-current-state.md)
and [`design/qore-ir-spec.md`](../design/qore-ir-spec.md) for the substrate this
proposal builds on.

**Implemented prerequisites and companion designs:**
[`design/generic-class-types.md`](../design/generic-class-types.md)
(parametric class types, generic hashdecls, method-level generics, wildcards,
and generic API metadata) is implemented on this branch and is an available
substrate for reflection, metadata, and future plugin-facing APIs. It is still
not load-bearing for `buffer<T>` itself because this design intentionally makes
`buffer<T>` a special built-in complex type first. [`design/named-arguments.md`](../design/named-arguments.md)
is implemented and useful for wide DataFrame / plugin registration APIs, but
is not blocking.

**Implementation status on this branch (2026-05-20):**

- Phase 1's built-in `buffer<T>` substrate is implemented in libqore,
  including dense construction from lists, typed indexing/assignment,
  `buffer<T>::sized()` / `buffer<T>::filled()` factories, slicing, zero-copy
  views, iteration, reductions, and elementwise arithmetic/comparison operators
  in AST, IR, and JIT execution paths.
- Phase 2's typed DBI result API is implemented in libqore and the PostgreSQL,
  MySQL, ODBC, and Oracle drivers have branch-local support for the typed
  select methods.
- Phase 3's registration foundation is implemented: `QorePluginType.h`, the
  module-init `plugin_module_handle`, descriptor validation and all-or-nothing
  staging/commit/rollback, `QORE_PLUGIN_REGISTER_TRACE`, process-global
  registry introspection helpers, a C++ smoke target, and the initial
  `Qore::Reflection::PluginRegistry` Qore-side surface.
- Phase 3's first dispatch slice is implemented: built-in plugin-dispatch IR
  opcodes, process-global operation IDs, `QORE_PLUGIN_DISPATCH_TRACE`,
  interpreter/JIT runtime-helper dispatch for value/list helper ABIs,
  non-persistent AOT/debug-IR serialization of module-local operation
  references, canonical operation signature hashes, opcode metadata, IR
  builder/printer/verifier plumbing, and smoke coverage for process operation
  lookup plus runtime binary dispatch. QORD now emits and validates
  `PLUGIN_IMPORTS`, `PLUGIN_TYPE_REGISTRY`, and `PLUGIN_HELPER_REFS` sections
  for plugin-dispatch IR references. `PluginRegistry::resolveOperation()` now
  resolves committed binary operation descriptors from Qore code and is
  traceable with `QORE_PLUGIN_CROSS_TYPE_TRACE`. Runtime verifier hooks now cover helper
  ABI resolution, helper result type checks, and `ReturnsLhs` / `ReturnsRhs`
  alias-contract checks, enabled in debug builds and in release builds with
  `QORE_PLUGIN_VERIFY`; passing and failing verifier checks are traceable with
  `QORE_PLUGIN_VERIFY_TRACE`. `QORE_PLUGIN_QORD_TRACE` logs writer and loader
  decisions for plugin import, registry, and helper-ref sections. Dense-buffer
  dispatch opcodes are implemented through Qore `buffer<T>` value wrappers that
  validate non-nullable byte-addressable numeric storage, element-storage
  compatibility, and output/input non-aliasing before calling raw
  `DenseBufferUnary` / `DenseBufferBinary` helpers. Plugin value nodes,
  `VT_PLUGIN_INSTANCE` QORD read/write support, and `Serializable`
  round-tripping are implemented with the registered plugin serializer
  callbacks.
  Program-local activation filtering, operation resolution,
  fallback-site recording/query/clear diagnostics,
  `examples/plugins/sample-buffer/`, and `examples/plugins/qore-plugin-lint`
  are implemented.
  User-facing language/reference documentation and release notes are present.
- Phase 4's LLVM codegen extension slice is implemented: modules can opt into
  `QorePluginLLVM.h`, valid callbacks are used by `QoreIRToLLVM` before the
  runtime-helper fallback, optional LLVM-major mismatches are ignored, and
  required mismatches fail validation.
- Phase 4's lowering callback slice is implemented: `QoreIRLowering` snapshots
  active plugin lowering callbacks, invokes them before built-in expression
  handlers, returns callback-provided IR results, and reports claimed-node
  `NotApplicable` results as `PLUGIN-LOWERING-CLAIM-VIOLATED`.
- Phase 5's DataFrame operator slice is implemented as a class-backed plugin
  type: the existing `DataFrame` object/private-data API is preserved while
  the module registers plugin operations for column subscripts, row subscripts,
  inclusive row-range slices, row-mask subscripts, and `ColumnRef` comparison
  predicates; core `[]` and comparison parsing/runtime dispatch can route
  matching plugin operations in AST, IR, and JIT paths.

---

## Summary

The Qore IR/JIT/AOT pipeline already has the architectural foundation required
to support module-defined "plugin" types as first-class citizens of the runtime,
optimizer, JIT, and AOT toolchain — typed opcodes, opcode metadata, a NaN-boxed
`QoreValue` representation shared across all execution tiers, type guards, a
uniform C ABI for runtime helpers, a versioned QORD binary format, and a
`QoreAOTContext` indirection mechanism that solves runtime-resolved pointer
baking generically. These mechanisms are currently closed around the built-in
type and opcode set; this proposal defines the work required to open them
safely.

This design proposes **exposing those mechanisms as a stable external protocol
so that C++ modules can register new dense, typed, operator-bearing data types
that the parser, IR optimizer, JIT, and AOT compiler treat as natively as
`int`, `list<T>`, or `hash<auto>`** — closing the dense-buffer / numeric-interop
gap, enabling credible in-process analytics at scale, and providing the
foundation for cleanly registering future tensor / decimal128 / GPU / Arrow /
ML types without further core changes.

The first concrete consumer is `buffer<T>` (dense typed primitive arrays), which
unlocks meaningful end-to-end optimization for the existing `dataframe` /
`DataProvider` / SQL stack: ecosystem-typical speedups on hot numeric loops
(NumPy-vs-Python-list scale: usually one to two orders of magnitude, workload
dependent), substantial memory reduction on large homogeneous columns (a
`buffer<int8>` is 8× smaller than the equivalent `list<int>` of boxed
`QoreValue` cells, before counting allocator overhead), schema fidelity through
the SQL ↔ Qore boundary, and zero-copy interop with Eigen / Arrow / ONNX
Runtime / native BLAS. The second consumer is `DataFrame` itself, which
registers as a plugin type to gain native `df["col"]`, `df[1..10]`,
`df["col"] > 5` syntax and zero-conversion participation in
`DataProviderPipeline`. Concrete numbers must come from benchmarks against the
delivered implementation, not from this document.

The substrate is operational, but the current implementation still has several
hard-coded assumptions that must be addressed explicitly: fixed opcode ids,
fixed opcode metadata arrays, builtin-only profiling buckets, static JIT helper
registration, QORD compatibility rules, and NaN-box tag classification. This
document treats those as first-class design constraints rather than incidental
implementation details.

---

## 1. Motivation

### 1.1 The existing data ecosystem

Qore already has a thoughtful, layered data infrastructure:

- **`hash<string, list<auto>>` ("hash of lists")** is the canonical bulk shape
  used by `BulkSqlUtil`, `Mapper.mapBulk()`, `DataProvider.searchRecordsBulk()`,
  and `DataProviderPipeline`. Same conceptual shape as a columnar dataframe —
  each key maps to a column array.
- **`AbstractDataProcessor::processRecordsBulk(hash<auto>)` +
  `supportsBulkApi()`** already provides column-chunk streaming through
  pipelines, with auto-routing between bulk and row-at-a-time stages. This is a
  chunk API, not yet a dense-column/vector API: some processors are bulk-aware
  (`QoreFilterRecordsProcessor`, parts of the analytics stack), while others
  such as `QoreGroupByProcessor` currently require row-at-a-time processing.
- **DPQL** pushes filter/projection down to the source where possible
  (translated to native SQL `WHERE`, etc.), with client-side fallback via
  `DataProvider::parseDpqlExpression()`.
- **`modules/dataframe/`** provides an unreleased typed columnar DataFrame
  module backed by Eigen3 for `float64`, with SQL-like ops (`select`, `filter`,
  `sortBy`, `groupBy`, `join`, `pivot`, `melt`, window functions: `lag`,
  `lead`, `cumSum`, `rollingMean`, `rowNumber`), CSV / Parquet / DBI I/O, and
  ML interop (`toMatrix`, `toVector`, `fromMatrix`). Because the module has not
  shipped, its public representation can still change from a Qore class/private
  data object to a new native data type if that makes the plugin-type design
  cleaner.

These pieces compose well at the *protocol* level — bulk records flow through
pipelines as hash-of-lists with full type metadata available from the
DataProvider record info, DPQL lets you push filters to the source, and the
DataFrame module is the materialised columnar store on the consumer side.

### 1.2 The gaps

The composition breaks down at the *value* level. Several distinct gaps share a
single root cause:

1. **Boxing tax.** `list<int>` is a list of tagged `QoreValue` cells, not a
   `int64_t[]`. The DataFrame module's Eigen3-backed `float64` columns must
   unbox into a boxed list to enter a pipeline, then rebox into typed columns
   downstream. Hot numeric loops pay an order-of-magnitude or more overhead to
   indirection and lack of SIMD opportunity (the NumPy-vs-Python-list reference
   point — actual ratios depend on operation and width, and real numbers must
   come from benchmarks, not from this document).
2. **Width / precision collapse.** Qore's `int` is always 64-bit; `float` is
   always 64-bit; `number` is arbitrary precision. SQL types `SMALLINT`,
   `INTEGER`, `BIGINT` all map to `int`; `REAL` and `DOUBLE PRECISION` both map
   to `float`. Round-trips to Parquet / Arrow / Kafka / protobuf silently widen
   or lose precision metadata.
3. **No user-class subscript or operator overloading.** Built-in types have
   `[1..10]`, `{"a","c"}`, `s[0..4]`, but a user-defined `DataFrame` class
   cannot participate in this syntax — `df[1..10]`, `df["col"]`,
   `df["col"] > 5` are inexpressible without escapes.
4. **No declarative metadata for user-defined operations.** The IR's
   `OPCODE_REGISTRY` already records `may_have_side_effects`,
   `may_throw_exception`, etc. for built-in opcodes; the verifier uses parts of
   this table, while other opcode properties are still implemented in separate
   switch logic. There is no path for module-defined operations to participate.

These are not four problems. They are one problem — **libqore has a closed set
of "basic" types and no protocol for modules to add more** — viewed from four
angles. A plugin-type protocol solves all four; individually patching each is a
sequence of one-shot fixes that doesn't compose.

### 1.3 Strategic question

Python's success in numerics / data / ML is largely the C-API extension type
protocol. NumPy, pandas, Polars, PyTorch, JAX, xarray are all `PyTypeObject`-
backed extension types; the language coordinates the orchestration. Without
that protocol, Python would not be the dominant data/ML language; with it, new
ecosystems plug in without language changes.

The strategic question for Qore is whether to take an analogous direction:
expose the IR/JIT/AOT machinery as a public extension surface, so that
`buffer<T>`, `DataFrame`, future `tensor<T,N>`, future `decimal128`, future GPU
types, future Arrow bridges all register against a stable protocol instead of
requiring libqore changes per type.

The constraint that makes this realistic for Qore (in a way it is for Julia and
modern Python, but isn't for traditional scripting languages) is that the
IR/JIT/AOT pipeline already exists and already specializes by parse-time type
analysis. Plugin-type dispatch costs are amortized by JIT codegen the same way
built-in dispatch costs are. The architecture has done the hard part already.

---

## 2. Architecture context

### 2.1 What the IR provides

The current implementation in `lib/QoreIRLowering.cpp`, `lib/QoreIRInterpreter.cpp`,
`lib/QoreIRToLLVM.cpp`, `lib/QoreIRVerifier.cpp` exposes the following pieces
that are directly relevant:

**Numbered, binary-stable opcode set.** `enum class QoreIROpcode : uint16_t`
contains 364 opcodes (IDs 0–363), with explicit binary-compatibility discipline
in the header: never reuse, never reorder, never insert; removed opcodes leave
gaps. The implementation currently hard-caps this range with
`QORE_IR_MAX_OPCODE` and static assertions in multiple execution tiers. QORD
loading also rejects binaries whose `max_opcode_id` exceeds the built-in cap.
This means plugin operations should initially be represented by a small number
of built-in "plugin invoke" opcodes carrying operation descriptors, not by
arbitrary absolute opcode ids, unless the whole IR dispatch surface is first
made dynamic.

**Specialization by parse-time typing.** Most arithmetic / comparison / fold /
map / select / range / cast operations come in typed and `.any` flavours
(`AddInt`/`AddFloat`/`AddString`/`AddAny`, `EqInt`/`EqFloat`/`EqString`/`EqAny`,
`MapInt`/`MapFloat`/`MapAny`, etc.). The lowering pass consults
`QoreParseContext` analysis (`isLocalDefinitelyAssigned`, `guaranteedType`,
`expressionAnalysisType`, `expressionCanThrow`) to choose between typed and
`.any` opcodes. Typed opcodes monomorphize at parse time; `.any` opcodes
runtime-dispatch via uniform C ABI helpers.

**Deopt-safe type guards.** `GuardNotNothing`, `GuardInt`, `GuardFloat`,
`GuardType` are first-class IR ops. A failed guard traps to the active unwind
target; in the JIT path it lowers to LLVM `deopt`, falling back to the IR
interpreter with a state map (locals, live SSA temps, cleanup list). This is
the safety net for type-shape specialization. It is not, by itself, a safety
net for semantic claims such as "pure" or "commutative": an optimizer that
reorders or removes work based on incorrect metadata can silently change
program results unless the claim is independently verified or kept
conservative.

**Profile-guided specialization.** `TypeProfile` tracks per-guard observed
runtime types with atomic counters; `dominantType(threshold=0.95)` enables JIT
to adaptively promote polymorphic `.any` paths to monomorphic typed opcodes
based on runtime frequency. The current profile buckets are builtin-only
(`int`, `float`, `string`, `bool`, `nothing`, `other`), so plugin-type
specialization needs an extensible profile key, such as an interned
`QoreTypeInfo*` or stable plugin type id, before it can be used for external
types. The implemented profile key keeps the legacy builtin fast path and
adds explicit `plugin_type` keys carrying `(module_name, local_type_id,
type_info)` so plugin values whose public `QoreTypeInfo*` is intentionally
generic, such as `autoTypeInfo`, can still be profiled precisely.

**Opcode metadata registry.** `include/qore/intern/QoreOpcodeRegistry.h`
defines `struct OpcodeInfo` with fields including `name`, `description`,
`expected_operands`, `can_return_nothing`, `never_returns_nothing`,
`is_terminator`, `produces_result`, `may_have_side_effects`,
`may_throw_exception`, `is_unary_invoke`, `is_binary_invoke`,
`skip_aot_expr_slot`. The constexpr `OPCODE_REGISTRY[364]` is complete for the
built-in opcodes and enforced by `static_assert`, but it is not yet the only
source of opcode semantics; lowering, printing, interpretation, LLVM lowering,
and some optimizer logic still carry independent switch statements. The plugin
protocol should first centralize the builtin metadata consumers, then add an
extension path.

**Uniform runtime-helper C ABI.** `lib/JITRuntime.cpp` ships 454 helpers in the
shape `extern "C" DLLEXPORT uint64_t qore_rt_<op>_<type>(uint64_t lhs,
uint64_t rhs, ExceptionSink* xsink)` (NaN-boxed operands and return,
`ExceptionSink*` for errors, `QORE_RT_CHECK_THROW(xsink)` macro converts to C++
exception for invoke/landingpad). `qore_fast_*` is the fast-path /
specialization variant. The current JIT registers these helpers explicitly in
the ORC symbol table; there is no generic plugin `dlsym` resolver yet. Plugin
helpers need a registered function table and JIT symbol resolver hook.

**Massive fusion precedent.** The fold/map/select families have ~50 fused
variants (`FoldlSumInt`, `MapScaleFloat`, `MapHashKeyOffsetInt`,
`FusedMapSelectScalePositiveInt`, `FusedMapFoldlSumScaleFloat`, etc.). The
lowering pass already does aggressive AST-pattern recognition to emit
specialized opcodes — exactly the architecture you want for `buffer<T>` and
lazy expression DSLs.

**Single value representation across all tiers.** The IR uses the same 64-bit
NaN-boxed `QoreValue` representation as the runtime: 16-bit tag in high bits,
48-bit payload. Tags include `TAG_INT48`, `TAG_POINTER`, `TAG_SPECIAL`,
`TAG_SHORTSTR_BASE`. Doubles are encoded inline. There may be room for new
immediate-typed plugin values, but the exact tag range must be allocated only
after auditing `QoreValue` classification; tags below the current integer
boundary can be misclassified as floats. Most plugin types should use pointer
representation and identify through their type descriptor.

### 2.2 What the AOT pipeline provides

`include/qore/intern/QoreAOT.h` and `include/qore/intern/QoreAOTBinary.h`
define a complete, versioned AOT toolchain:

**Versioned C ABI for module entry points.** `qore_aot_run` / `_v2` / `_v3`,
`qore_aot_module_init` / `_v2` / `_v3`, `qore_aot_module_ns_init`,
`qore_aot_module_delete`, `qore_aot_register_into_program`,
`qore_aot_fill_module_desc`, `qore_aot_raise_init_error`. The "v1, v2, v3 as
separate symbols, old binaries keep working" pattern is established — plugin-
type registration rides this exact discipline.

**Generic runtime-pointer indirection.** `QoreAOTContext` carries slot-indexed
arrays of `LocalVar*`, `Var*`, expression `QoreValue` bits, `AbstractStatement*`,
`CaseNodeRegex*`, `QoreIRLValuePathInstruction*`. Build-time AOT generates
index-based code; runtime re-lowers IR in deterministic order to populate the
fresh pointers. Plugin-type runtime-resolved references plug in here.

**Pre-compiled function descriptor.** `struct QoreAOTFunc { const char* name;
AotFunctionPtr fn_ptr; int num_locals, num_globals, num_exprs, num_stmts,
num_regex_cases; };` — uniform unit AOT artifacts ship. Plugin-type bodies and
helpers ship in the same shape.

**QORD binary format with full forward-compatibility design.**
`QoreAOTBinaryHeader` (60 bytes) carries `magic`, `version`, `flags`,
`parse_options_lo/hi`, `section_count`, `label_offset/length`,
**`max_opcode_id`**, Qore version, `compression`, `source_hash` (xxHash64), and
**`feature_flags`** (64-bit bitmap). 18 feature flags currently active; runtime
mask `QORE_AOT_SUPPORTED_FEATURES` gates compatibility. Some AOT paths warn and
fall back to JIT for unsupported feature bits, while unsupported opcode ids are
hard failures. Plugin types need their own hard-required import/version records
so a missing plugin cannot be mistaken for a soft optimization feature.

**Extensible section-typed payload.** 21 section types
(`QoreAOTSectionType` 1–21) including `STRINGS`, `NAMESPACES`, `CLASSES`,
`HASHDECLS`, `ENUMS`, `TYPEDEFS`, `CONSTANTS`, `GLOBALS`, `FUNCTIONS`,
`METHODS`, `SLOT_MAPS`, `FUNC_SOURCES`, `TOPLEVEL`, `DEPENDENCIES`,
`REEXPORT_MODULES`, `PROGRAM_METADATA`, `INIT_FUNCS`, `TYPE_TABLE`,
`MODULE_PATH_PREPEND/APPEND`, `BUILD_INFO`. Adding a `PLUGIN_TYPE_REGISTRY` and
`PLUGIN_IMPORTS` section is mechanical at the enum level, but the loader must
distinguish hard-required plugin imports from soft feature fallback.

**Extensible value-tag set.** `QoreAOTValueTag` 0–18, including
`VT_OPAQUE_DEFAULT`, `VT_NEW_OBJECT`, `VT_ENUM`, `VT_NEW_COMPLEX_DEFAULT`,
`VT_EXPR_TREE`. Plugin-type instances slot in here.

### 2.3 What this means

The plugin-type protocol has a strong substrate, but the current
implementation is not yet a public extension surface. The first design task is
to make the existing built-in machinery explicitly extensible:

- Centralize builtin opcode properties behind the registry before relying on
  registry-driven optimizer decisions.
- Use built-in plugin-dispatch opcodes carrying operation descriptors for the
  first version, or deliberately refactor every fixed opcode table and switch
  before allowing externally allocated opcode ids.
- Add extensible type-profile keys before promising profile-guided plugin
  specialization.
- Add named QORD plugin imports with module name, ABI version, type ids,
  and operation ids; every plugin import is a hard requirement (soft
  fallback is reserved for core QORD format-feature bits, not plugins).
  Do not spend one global feature bit per plugin.
- Add an explicit module-registration hook populated from
  `QoreModuleInitContext`, with deterministic ordering, a runtime-owned plugin
  module handle, and a public registration context that stays out of the C++
  module-loader ABI.
- Add a JIT helper symbol resolver and interpreter helper table for registered
  plugin operations.
- Audit `QoreValue` tag classification before assigning any plugin immediate
  tags.

This is still incremental work on top of operational infrastructure, but it is
not just exposing an already-general mechanism. Several fixed builtin
assumptions must be lifted or intentionally bypassed.

---

## 3. Proposed design

### 3.1 Goals

1. **Allow C++ modules to register new dense, typed, operator-bearing data
   types** that participate in the parser, IR optimizer, IR interpreter, JIT,
   and AOT compiler on equal terms with built-in types.
2. **Preserve the existing typed-opcode-vs-`.any` dispatch architecture.** No
   new dispatch model; plugin types ship typed operation descriptors for
   monomorphic call sites, lowered through built-in plugin-dispatch opcodes, and
   rely on `.any` runtime fallback for ambiguous ones.
3. **Honour the "no silent semantic fallbacks in `%modern`" invariant.**
   Plugin-type lowering is explicit; ambiguous cases either lower to a named
   `.any` fallback or fail at parse time.
4. **Use QORD plugin imports as the ABI-versioning mechanism.** Cross-version
   incompatibility is detected at load time with module-name, type-name, and
   operation-version diagnostics. Feature flags remain reserved for core QORD
   format capabilities.
5. **Keep `dataframe` an optional module.** The plugin-type protocol must work
   without `dataframe` loaded; if absent, code that doesn't use plugin types is
   unaffected.
6. **Make first-class participation in the IR optimizer cheap for plugin
   authors.** Declarative metadata (purity, commutativity, identity element,
   type-promotion lattice) is a small struct extension, not a new framework.
7. **Treat semantic metadata as conservative until verified.** Type guards and
   deopt protect type-shape specialization; they do not automatically prove
   algebraic claims such as purity, commutativity, or associativity.

### 3.2 Non-goals

- **Not a rewrite of how user-defined `class` works.** Existing Qore classes
  continue with single-dispatch method calls and their current C++ Priv
  pattern. The plugin-type protocol is for *new dense data types*, not for
  retrofitting every class as a plugin type.
- **Not a Python C-API clone.** The shape is informed by `PyTypeObject` but
  built around Qore's existing typed-opcode + NaN-boxing + IR/JIT/AOT
  architecture. Slots are coarser-grained and fewer.
- **Not multiple dispatch as a language-level feature.** The IR's typed-vs-
  `.any` mechanism delivers most of multiple dispatch's benefits via parse-time
  monomorphization. User-facing methods stay single-dispatch. Operators
  effectively become multimethods through opcode registration, but the
  programmer model remains Qore's existing class-based one.
- **Not an attempt to make non-`%modern` code participate.** Plugin types are
  a `%modern`-only feature, consistent with the IR's coverage scope.
- **Not a runtime-dynamic type registration story.** Plugin types are
  registered at module load time, before any code that uses them parses.
  Dynamic registration after parse-time is out of scope.

### 3.3 Plugin-type registration ABI

A module wanting to register one or more plugin types supplies descriptors
during the existing module-init path. Phase 3 extends `QoreModuleInitContext`
with an opaque, runtime-owned `plugin_module_handle`; module-init code copies
the relevant fields into an exact-size `QorePluginRegistrationContextV1` and
passes that C ABI context to registration. This keeps plugin registration part
of normal module initialization so load ordering, dependency resolution,
parse-time visibility, helper-symbol lookup, and error reporting are
deterministic, while the public plugin protocol does not expose
`QoreModuleInitContext` directly.

The `plugin_module_handle` is valid only for the calling thread's current
`qore_module_init` invocation. Modules MUST NOT cache it, compare it, serialize
it, or pass a handle issued for another module. Registration and validation
MUST reject a non-null handle that does not match the live current-thread TLS
handle with `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` and subreason
`module_handle_stale`. Detecting other stale-handle bugs — for example a cached
handle whose private representation is accidentally still recognizable — is
best-effort using private representation details such as generation cookies.
Tests may assert the mandatory TLS-mismatch rejection, but tests asserting
"any cached stale handle is rejected" are non-portable and should not be
written.

The corresponding `ModuleManager.h` ABI addition is intentionally narrow:

```cpp
struct QorePluginModuleHandle;  // opaque, runtime-owned

struct QoreModuleInitContext {
    //! Absolute filesystem path of the loaded .qmod (or .qm source) — the
    //! same value the existing module loader passes to other module-init
    //! paths. Predates the plugin protocol and is unchanged.
    std::string path;

    //! Runtime-owned opaque handle, valid only during this thread's
    //! current qore_module_init invocation. Copied into
    //! QorePluginRegistrationContextV1 / QorePluginValidationContext to
    //! authorize helper-symbol lookup against the module's own binary.
    //! Added in Phase 3.
    const QorePluginModuleHandle* plugin_module_handle;
};
```

**C++ module ABI vs. plugin C ABI boundary.** `ModuleManager.h` remains the
existing C++ module-loader ABI: `QoreModuleInitContext` contains
`std::string path` and is coupled to libqore's compiled-in C++ standard library.
`QorePluginType.h` does not include or expose that type. The stable plugin
protocol receives only a C-compatible registration context, fixed-width fields,
plain structs, opaque handles, and function pointers. A module can still use the
existing C++ `qore_module_init` callback as a thin shim that fills
`QorePluginRegistrationContextV1` from `QoreModuleInitContext`; v1 does not
define a separate pure-C module-init ABI.

```cpp
// header: include/qore/QorePluginType.h (new)
//
// This header is the stable plugin-type ABI and must not include LLVM headers.
// Optional LLVM lowering lives in include/qore/QorePluginLLVM.h so modules that
// only use runtime-helper dispatch are not tied to libqore's LLVM version.

struct QorePluginModuleHandle;   // opaque, runtime-owned

enum class QorePluginHelperAbi : uint8_t {
    UnaryValue        = 0,  //!< uint64_t(value, xsink); lowers to PluginUnary
    BinaryValue       = 1,  //!< uint64_t(lhs, rhs, xsink); lowers to PluginBinary
    CallValueList     = 2,  //!< uint64_t(self, args_list_bits, xsink); lowers to PluginCall
    SubscriptValue    = 3,  //!< uint64_t(container, key_or_range, xsink); lowers to PluginSubscript
    Construct         = 4,  //!< uint64_t(args_list_bits, xsink); lowers to PluginConstruct,
                            //!< used for type::sized / type::filled / static factories
    DenseBufferUnary  = 5,  //!< dense-buffer ABI (§3.5); lowers to PluginDenseBufferUnary
    DenseBufferBinary = 6,  //!< dense-buffer ABI (§3.5); lowers to PluginDenseBufferBinary
};

enum class QorePluginValueAccess : uint8_t {
    ReadOnly       = 0,
    MutatesLhs     = 1,
    MutatesRhs     = 2,
    MutatesBoth    = 3,
};

//! How the operation's return value relates to operand storage.
//! Unknown is the conservative default — the optimizer treats it identically
//! to MayAliasInputs for alias analysis. ReturnsLhs and ReturnsRhs are
//! contracts: the runtime helper MUST return the named operand's NaN-boxed
//! bits exactly. Debug builds verify this on every call; misdeclaration is
//! undefined behaviour in release builds and may cause silent miscompile via
//! CSE / store-elimination based on the (false) identity claim.
enum class QorePluginResultAlias : uint8_t {
    Unknown            = 0,
    MayAliasInputs     = 1,
    FreshNoAliasInputs = 2,
    ReturnsLhs         = 3,
    ReturnsRhs         = 4,
};

//! Per-instance lifecycle operations. Required for every registered plugin
//! type. NaN-boxed bits in / NaN-boxed bits out; ExceptionSink* receives any
//! error. Helpers must be exception-safe — partial state on xsink is the
//! caller's responsibility per QORE_RT_CHECK_THROW discipline.
struct QorePluginValueOps {
    //! Increment refcount on a live plugin value. Must not throw. Cheap.
    void (*incref)(uint64_t value_bits) noexcept;

    //! Decrement refcount; free storage at zero. Must not throw under any
    //! condition; a destructor that can fail is a protocol violation.
    void (*decref)(uint64_t value_bits) noexcept;

    //! Deep copy. Returns a new owned value (caller now owns one ref).
    //! May allocate; on failure, raises through xsink and returns 0.
    uint64_t (*clone)(uint64_t value_bits, ExceptionSink* xsink);

    //! Structural equality. May throw via xsink for ill-typed comparison.
    bool (*equal)(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);

    //! Stable hash (for use in hash<Plugin, T> keys; must agree with equal).
    int64_t (*hash)(uint64_t value_bits, ExceptionSink* xsink);

    //! Cleanup helper invoked from QoreAOTContext / IR-interpreter cleanup
    //! lists at deopt or unwind boundaries. Receives the value as stored in
    //! the cleanup slot (the same NaN-boxed representation).
    void (*cleanup_slot)(uint64_t value_bits) noexcept;
};

//! Result of a parse-time pattern-match attempt. See §3.7.
enum class PluginLoweringResult : uint8_t {
    Lowered      = 0,  //!< matcher emitted a typed plugin-dispatch opcode
    NotApplicable = 1, //!< AST shape is outside this matcher's claimed coverage
    Erroneous    = 2,  //!< matcher detected a hard error and reported via ctx
};

//! Optional parse-time AST-pattern matcher. See §3.7.
typedef PluginLoweringResult (*PluginLoweringCallback)(
    QoreIRLoweringContext* ctx,
    const AbstractQoreNode* ast_node,
    const QoreParseContext* parse_ctx,
    QoreIRBuilder* builder);

//! Type-promotion callback when type_promotion_kind == Custom. See §3.4.
typedef const QoreTypeInfo* (*PluginTypePromotionCallback)(
    const QoreTypeInfo* lhs,
    const QoreTypeInfo* rhs);

//! Stable byte sink/source for plugin QORD payloads. These intentionally avoid
//! exposing QoreAOTBinaryWriter / QoreAOTBinaryReader in the public plugin ABI.
//! Return 0 on success, -1 on failure with diagnostic in xsink — same
//! convention as qore_register_plugin_types_v1. The read callback fails any
//! attempt to read past the deserializer's payload boundary (see consumption
//! discipline below).
typedef int (*PluginByteWriteCallback)(
    const void* data,
    uint32_t len,
    void* user_data,
    ExceptionSink* xsink);
typedef int (*PluginByteReadCallback)(
    void* data,
    uint32_t len,
    void* user_data,
    ExceptionSink* xsink);

//! QORD wire-format codec for a plugin value. Called during AOT serialization
//! / deserialization of constants and slot-table entries. The serializer writes
//! only the module-defined payload; the QORD layer writes the common
//! VT_PLUGIN_INSTANCE header from §3.9.
//!
//! Consumption discipline (deserializer): the deserializer MUST consume
//! exactly payload_len bytes from the read callback before returning success.
//! The read callback rejects reads that would cross the boundary (returns -1
//! with a diagnostic). Consuming fewer than payload_len bytes is a load-time
//! error — the QORD layer raises a deserialization failure and the artifact
//! is rejected. This keeps the per-payload framing self-describing without
//! requiring the deserializer to track its own offset.
typedef int  (*PluginSerializeCallback)(
    uint64_t value_bits,
    PluginByteWriteCallback write,
    void* write_user_data,
    ExceptionSink* xsink);
typedef uint64_t (*PluginDeserializeCallback)(
    PluginByteReadCallback read,
    uint32_t payload_len,
    void* read_user_data,
    ExceptionSink* xsink);

//! Opaque optional-extension payload. The stable v1 ABI defines extension ids
//! but not their contents. For example, the LLVM codegen extension id points to
//! a QorePluginLLVMExtension object declared in QorePluginLLVM.h.
//! extension_id is a non-empty dot-separated ASCII identifier. Each label
//! matches [A-Za-z0-9_]+; ':' and whitespace are invalid, and canonical ids
//! should use lowercase labels. The same grammar is used when extension ids
//! appear in diagnostic subreasons.
struct QorePluginExtension {
    const char* extension_id;
    const void* extension_data;
    bool required;
};

struct QorePluginOperationSignature {
    //! Argument arity. 0 = nullary constructor (Construct ABI with empty args_list);
    //! 1 = unary (uses primary_type); 2 = binary (uses primary_type + secondary_type);
    //! 0xFF = variadic call (CallValueList; primary_type is the receiver type).
    uint8_t arity;

    //! Primary operand type. For unary/binary ops this is the LHS type; for
    //! CallValueList it is the receiver "self" type; for Construct it is the
    //! constructed type. nullptr is invalid for any defined arity.
    const QoreTypeInfo* primary_type;

    //! Secondary operand type. Meaningful only when arity == 2; nullptr otherwise.
    const QoreTypeInfo* secondary_type;

    const QoreTypeInfo* return_type;
    bool primary_nullable;
    bool secondary_nullable;
    bool return_nullable;

    //! Aliasing/effect contract used by the optimizer. ReadOnly enables
    //! reordering reads of the operands across the call site; the Mutates*
    //! variants disable that reordering for the named operand. See §3.4
    //! "Optimizer passes use these fields" for the full list of consumers.
    QorePluginValueAccess access;
    QorePluginResultAlias result_alias;
    QorePluginHelperAbi helper_abi;
};

struct QorePluginOperation {
    uint16_t local_id;                            //!< 0..N within this module
    //! Operation name. Drawn from the protocol's canonical operation-name table
    //! (see "Operation-name table" below) when one of those names is used; that
    //! entry validates only category and signature shape. Algebraic/effect
    //! permissions still come from OpcodeInfoExtended on this signature. Names
    //! outside the table are permitted and treated as opaque module-local
    //! identifiers — registration does not validate signature against an opaque
    //! name. Built-in names always take precedence over module-local
    //! interpretation.
    const char* operation_name;
    QorePluginOperationSignature signature;
    OpcodeInfoExtended info;                      //!< full metadata (see 3.4)
    //! Runtime helper pointer. Cast at the call site to the trampoline type
    //! implied by signature.helper_abi. void(*)() is used (rather than void*)
    //! because the C standard does not guarantee object-pointer / function-
    //! pointer interconvertibility, even though POSIX permits it.
    //! If null, runtime_helper_symbol must be resolvable via dlsym on the
    //! registering module's own handle at registration time; the resolved
    //! pointer is stored in the helper table. JIT/AOT/interpreter execution
    //! never performs late dlsym from hot paths.
    //! When both runtime_helper and runtime_helper_symbol are non-null, the
    //! pointer is authoritative; the symbol becomes a diagnostic label only
    //! and the runtime does not verify they resolve to the same address.
    void (*runtime_helper)();
    const char* runtime_helper_symbol;            //!< optional diagnostic / loader lookup name
    PluginLoweringCallback   lowering_pattern;    //!< optional AST-pattern matcher
    //! Bitmap of AST node kinds (NT_*) this matcher claims to cover. Empty if
    //! no pattern matcher is installed. Used to validate Lowered/NotApplicable
    //! returns: a NotApplicable on a node kind in this bitmap is a protocol
    //! violation in %modern (parse-time error).
    uint64_t lowering_claimed_node_kinds;

    //! QDOM domain bits required by this operation; ORed into the sandbox-domain
    //! mask of every call site using this operation. Keep operation domains
    //! narrow: in-memory DataFrame filtering should not inherit FILESYSTEM just
    //! because the same type also has CSV helpers.
    int64_t qdom_domains;

    const QorePluginExtension* extensions;
    int num_extensions;
};

struct QorePluginTypeDescriptor {
    uint16_t local_type_id;                       //!< 0..N within this module
    const char* type_name;                        //!< "buffer<int64>", "DataFrame", etc.
    const QoreTypeInfo* type_info;                //!< Qore type-system entry
    QorePluginValueOps value_ops;                 //!< lifecycle, required
    PluginSerializeCallback   serialize;          //!< QORD writer hook
    PluginDeserializeCallback deserialize;        //!< QORD reader hook
    //! Serializer wire-format version, written into VT_PLUGIN_INSTANCE
    //! payloads (§3.9). Bumped when the type's binary encoding changes;
    //! readers reject payloads whose recorded version is unsupported by
    //! the live deserializer. Default 1 for new types.
    uint16_t serializer_format_version;
    //! Optional baseline QDOM domain bits required by all operations on this
    //! type. Most types should leave this as QDOM_DEFAULT and declare domains
    //! per operation instead; do not use a baseline for optional I/O helpers.
    int64_t baseline_qdom_domains;
};

struct QorePluginDependency {
    const char* module_name;
    const char* min_plugin_abi_version;
    const char* min_operation_set_version;
};

struct QorePluginRegistrationContextV1 {
    //! Size of this struct as known to the caller. MUST be set to
    //! sizeof(QorePluginRegistrationContextV1). V1 is a fixed-size contract:
    //! smaller values are rejected as too small and larger values are rejected
    //! as a version mismatch. Incompatible growth uses
    //! QorePluginRegistrationContextV2 plus qore_register_plugin_types_v2,
    //! not trailing-field append on this struct.
    uint32_t struct_size;

    //! Registration-context flags. v1 defines no nonzero flag bits; any
    //! bit set in flags is unknown and rejected with subreason
    //! reserved_field_nonzero. Future registration flags are added only in a
    //! new QorePluginRegistrationContextV<N> consumed by the matching
    //! qore_register_plugin_types_v<N> symbol. v1 readers reject any nonzero
    //! value so a caller accidentally using newer flags against v1 fails
    //! loudly.
    uint32_t flags;

    //! Borrowed absolute module path for diagnostics and trace output. MUST be
    //! non-null and non-empty. Pointer must remain valid until
    //! qore_register_plugin_types_v1 returns; libqore reads it during the
    //! call, copies internally if it needs to retain it for diagnostics or
    //! process-global descriptor metadata, and does not reference the
    //! caller's buffer after return.
    const char* module_path;

    //! Runtime-owned opaque handle copied from
    //! QoreModuleInitContext::plugin_module_handle during the current
    //! qore_module_init invocation. Required for helper-symbol lookup.
    //! The runtime maintains thread-local state recording the live
    //! plugin_module_handle for the current qore_module_init call;
    //! qore_register_plugin_types_v1 requires a non-null module_handle and
    //! compares it against that TLS value, rejecting mismatches with subreason
    //! module_handle_stale. qore_validate_plugin_types_v1 performs the same
    //! comparison only when its optional validation module_handle is non-null;
    //! nullptr remains the dry-run "defer helper-symbol checks" mode. Detection
    //! beyond the current TLS equality check is best-effort per §3.3
    //! stale-handle rules.
    const QorePluginModuleHandle* module_handle;
};

struct QorePluginTypeRegistration {
    const char* module_name;            //!< matches the .qmod feature name
    const char* plugin_abi_version;     //!< protocol ABI version
    const char* operation_set_version;  //!< module-local semver string

    const QorePluginTypeDescriptor* types;
    int num_types;

    const QorePluginOperation* operations;
    int num_operations;

    const QorePluginDependency* dependencies;
    int num_dependencies;
};

//! Runtime-side registration hook, called from qore_module_init with a
//! QorePluginRegistrationContextV1 populated from the current
//! QoreModuleInitContext. The _v1 suffix is mandatory: every
//! breaking change to the registration ABI ships as a new symbol
//! (qore_register_plugin_types_v2, ...) and old binaries keep resolving the
//! symbol they linked against. See §8.1.
//!
//! Returns 0 on success, -1 on failure with diagnostic in xsink. Registration
//! staging is all-or-nothing: if any type, operation, dependency, or extension
//! fails validation, no part of `reg` is staged or published and the function
//! returns -1.
//! Callers must propagate the failure rather than continuing module init. A
//! null context, wrong-size context (subreason registration_context_too_small
//! for struct_size < sizeof(V1), registration_context_size_mismatch for
//! struct_size > sizeof(V1)), nonzero flags field, missing module_path,
//! context not owned by the current module-init call, or context without a live
//! module_handle is rejected before descriptor validation. Registration does
//! not accept an explicit Program parameter and does not activate the module in
//! a Program. It stages process-global descriptor/helper metadata in the current
//! module-init transaction; final publication happens only after
//! qore_module_init succeeds. Per-Program activation is performed by module
//! load/add-to-Program and QORD import processing.
//!
//! `ctx` and `reg` are consumed during the call only; libqore does not retain
//! either top-level pointer past return. The caller's descriptor arrays may be
//! released or reused after qore_register_plugin_types_v1 returns, because
//! libqore copies the descriptor metadata it owns (names, counts, dependency
//! records, extension records) before returning. Function pointers and
//! QoreTypeInfo pointers are stored as stable external identities and must stay
//! valid for the lifetime of the registering module. Extension payload lifetime
//! is defined by the extension ABI: v1 generic registration never deep-copies
//! arbitrary extension_data bytes. An extension that needs retained owned state
//! must define copy/free callbacks in its extension-specific ABI; otherwise its
//! extension_data pointer must remain valid for the module lifetime.
int qore_register_plugin_types_v1(
    const QorePluginRegistrationContextV1* ctx,
    const QorePluginTypeRegistration* reg,
    ExceptionSink* xsink);
```

Typical C++ module-init code, assuming the init callback parameter is named
`module_ctx`, creates the registration context on the stack with designated
initializers and passes it immediately. Designated initializers keep the call
site readable, make field names visible, and surface field-name typos at
compile time (a stray `.module_handle = some_path_string` fails the type
check rather than miscompiling silently). The released V1 field order is
ABI and must not change.

```cpp
// qore_module_init has the current ModuleManager.h signature:
// void qore_module_init(QoreModuleInitContext& module_ctx, ExceptionSink& xsink).
QorePluginRegistrationContextV1 plugin_ctx{
    .struct_size   = sizeof(plugin_ctx),
    .flags         = 0,
    .module_path   = module_ctx.path.c_str(),
    .module_handle = module_ctx.plugin_module_handle,
};
if (qore_register_plugin_types_v1(&plugin_ctx, &plugin_registration, &xsink)) {
    // xsink carries the diagnostic; module init must stop immediately.
    return;
}
```

Registration-context validation is ordered before descriptor validation:
`ctx == nullptr` yields subreason `null_registration_context`;
`ctx->struct_size < sizeof(QorePluginRegistrationContextV1)` yields
`registration_context_too_small`; a `struct_size` larger than the v1 sizeof
yields `registration_context_size_mismatch`; nonzero `flags` yields
`reserved_field_nonzero`; null or empty `module_path` yields
`module_path_missing`; null `module_handle` yields `module_handle_missing`;
stale or cross-module handles yield `module_handle_stale` when detected by the
current-module TLS comparison.

Registration is serialized by the plugin registry, not by the module loader.
The current module loader may run `qore_module_init` while its global module
mutex is released, so `qore_register_plugin_types_v1` MUST be thread-safe for
concurrent module-init threads. It takes the registry write lock before checking
committed descriptor conflicts and installing a pending registration record for
the current module-init transaction. Pending records are not visible to hot-path
dispatch, reflection, QORD import resolution, or per-Program activation.
Plugins must not spawn background threads that re-enter registration: those
threads do not own the current module-init TLS handle and fail with
`module_handle_stale`.

Hot-path dispatch (`PluginUnary` / `PluginBinary` / `PluginCall` /
`PluginSubscript` / etc.) reads the descriptor table lock-free, relying on
the §3.10 immutability guarantee that committed descriptors never change.
The registry write lock is held only while staging pending registration records
and publishing successful module-init transactions; after all modules have
loaded it is uncontested, and steady-state plugin-opcode execution incurs no
locking overhead from the registry.

If `qore_register_plugin_types_v1` succeeds but the surrounding
`qore_module_init` then fails (raises an error through `xsink` or returns
abnormally), libqore discards the pending registration as part of the same
module-load failure path. No descriptor from that failed attempt was ever
published, no hot-path reader could have observed it, and provisional ids from
the pending transaction may be reused. If `qore_module_init` succeeds, libqore
publishes the pending registration under the registry write lock, converts
provisional local-to-global mappings into committed ids, builds a new immutable
committed snapshot, release-stores the committed snapshot pointer/generation,
and wakes any concurrent registrants waiting on the module's outcome.
Descriptors from earlier, already-finished module loads are never affected by a
later module's init failure.

The wakeup primitive is a per-pending-transaction condition variable paired
with the registry write lock; waiters block on the condvar until the pending
transaction publishes or discards. There is no polling and no protocol timeout.
Deadlock detection below guarantees only that registration-internal waits cannot
form cycles; it does not bound a module-init thread that blocks indefinitely in
unrelated init work.

Publish is atomic with respect to all concurrent registrants and all hot-path
readers. There is no observer-visible state where a module's descriptors are
partially published: the registry write lock covers construction of the next
immutable descriptor/helper snapshot and the release-store that makes it the
committed snapshot. Hot-path readers acquire-load the snapshot pointer and read
only immutable committed tables; they never read the mutable pending table and
never depend on the registry lock. List-building reflection APIs additionally
read and re-check the generation counter to confirm a consistent
multi-descriptor view, as detailed in the next paragraph.

The generation counter accompanies the pointer so list-returning reflection
APIs that snapshot the registry state for consistent multi-descriptor
queries can detect a superseded snapshot mid-iteration. Hot-path dispatch
and single-descriptor lookups (`PluginRegistry::resolveOperation`,
cross-type lookup) read the pointer alone and ignore the generation; only
list-returning reflection APIs (`getTypes`, `getOperations`,
`getProcessModules`, `getActiveModules`) compare
generations across reads to confirm a stable snapshot for the duration of
a list build. The pair is conceptual; the implementation is free to pack
them into a single 128-bit DWCAS, into two separate atomics with a
re-check pattern, or into a hazard-pointer scheme — the design contract
is the release/acquire ordering and the generation invariant, not the
storage layout.

A module-init transaction supports at most one successful
`qore_register_plugin_types_v1` call. A second call after a successful first
within the same transaction fails with
`PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` and subreason
`registration_already_pending`. Plugins that need conditional descriptors
must compose a single descriptor table that covers all conditions and call
registration once.

**Deadlock detection.** The registry maintains a wait-for graph among
module-init transactions. When a registrant attempts to wait on a pending
transaction whose owning thread is itself (transitively) waiting on the
registrant's own transaction, the cycle is detected: the detecting registrant
fails immediately with `PLUGIN-REGISTRATION-WAIT-CYCLE` (subreason
`wait_cycle`) instead of waiting. The structured diagnostic payload names the
other transaction that completes the cycle. This converts a registry wait cycle
into a named, recoverable diagnostic. Plugin authors hitting this code in
production almost always indicate two modules with mutually dependent init paths
that would race regardless of plugin registration; the wait-cycle diagnostic
surfaces the load-order bug at the registration boundary.

**Two evolution disciplines, intentionally.** The plugin protocol uses two
distinct forward-compatibility patterns:

- `QorePluginRegistrationContextV1` carries `V1` in the type name and
  evolves by type rename (`V2`, `V3`, ...) paired with a new entry-point
  symbol (`qore_register_plugin_types_v2`). This is appropriate because
  the registration context is small, mandatory, and already part of the
  symbol-versioned `_v1` family on the entry point. Its `struct_size` field is
  a sanity check for the exact v1 layout, not a trailing-field growth mechanism.
- `QorePluginValidationContext` does *not* carry a version in the type
  name and evolves by `struct_size` size-prefix (§3.12). This is
  appropriate because the validation context is size-bounded, optional
  (callers can pass `nullptr`), and used outside the symbol-versioned
  registration entry point.

Future protocol surfaces should pick one of these two patterns and stay
consistent within the chosen pattern. Mixing — e.g., treating a `_V<N>`
struct's exact-size sanity check as permission for trailing-field growth —
defeats both schemes.

Constraints:
- `local_id` must be contiguous starting from 0 and is meaningful only inside
  the registering module. `qore_register_plugin_types_v1` assigns provisional
  process-global runtime ids in the pending module-init transaction; when module
  init succeeds, the registry publishes the local→global mapping as committed
  metadata. QORD `PLUGIN_IMPORTS` references are resolved only through the
  committed table at load time. If a QORD load races a module's pending
  `qore_module_init` and the QORD's `PLUGIN_IMPORTS` reference the
  still-pending module, QORD load fails with `QORD-PLUGIN-IMPORT-MISSING` and
  subreason `module_pending`, with the pending module in `related_module_name`;
  if the import names a module that is not loaded at all, the same error code
  is raised with subreason `module_not_loaded`. Both subreasons are positive
  signals: callers distinguish a retryable pending import from a genuinely
  missing module by matching on the subreason rather than its absence. The
  QORD loader does not auto-load missing modules and does not distinguish
  between never-loaded, failed-to-load, and rolled-back-after-init-failure
  cases — all three surface as `module_not_loaded`. The caller is responsible
  for triggering module load through the existing module-loader path before
  retrying the QORD. The QORD loader also does not wait on pending
  module-init transactions because QORD load and module-init can themselves
  form wait cycles; callers retry the QORD load after the module's init
  completes.
- The first implementation lowers plugin operations to built-in
  descriptor-carrying opcodes, one per `QorePluginHelperAbi`:
  `PluginUnary`, `PluginBinary`, `PluginCall`, `PluginSubscript`,
  `PluginConstruct`, `PluginDenseBufferUnary`, `PluginDenseBufferBinary`.
  Each opcode reads the descriptor from its operand stream and dispatches
  to the resolved helper pointer through the helper table (§3.5). The
  descriptor contains `(module_id, operation_local_id, type_signature_id)`
  and is serialized in QORD through `PLUGIN_IMPORTS`.
- A future implementation may allow true dynamic opcode ids, but only after
  replacing the fixed `QORE_IR_MAX_OPCODE` assumptions in the enum, registry,
  verifier, printer, interpreter, LLVM lowering, AOT writer, and AOT loader.
- Plugin dependencies are named imports, not global feature-flag bits. QORD
  records the provider module name, plugin ABI version, operation-set version,
  type ids, and operation ids.
- `value_ops` is mandatory. Registration with any null lifecycle pointer is
  rejected at `qore_register_plugin_types_v1` time. `incref`, `decref`, and
  `cleanup_slot` are `noexcept` by contract — the runtime relies on this for
  exception-safe deopt and unwind paths and treats a thrown exception from any
  of them as undefined behaviour.
- Every operation must declare a full signature. Registration rejects an
  operation whose arity, operand types, nullability, mutability, or helper ABI
  conflicts with the operation name or with the helper pointer installed for
  the operation. This signature is the canonical key for cross-type dispatch
  and the canonical type contract used by verifier/debug checks.
- `runtime_helper` is preferred. If it is null, the runtime resolves
  `runtime_helper_symbol` from the binary module handle at registration time
  and stores the resolved pointer in the helper table. JIT/AOT/interpreter
  execution never performs late `dlsym` lookup from hot paths.
- Extensions are ignored unless the runtime recognizes their `extension_id`.
  Registration rejects a null `extension_id`, an empty id, empty labels,
  whitespace, non-ASCII bytes, or `:` because extension ids also appear in the
  colon-separated diagnostic namespace (§3.12).
  If `required == false`, a recognized but invalid extension is dropped with a
  warning and the rest of registration continues. If `required == true`, the
  same validation failure rejects the whole registration. The first planned
  extension is `"qore.plugin.llvm.codegen"` from `QorePluginLLVM.h`:
  `libqore_llvm_major != QORE_LLVM_MAJOR` drops an optional codegen extension
  but rejects a required one. The diagnostic names both versions and the
  offending module.
- `operation_set_version` and `min_operation_set_version` use semver-major-
  compatible comparison: a dependency is satisfied iff the provided major
  equals the requested major and the (minor, patch) tuple is ≥ the
  requested one. `plugin_abi_version` is compared with strict equality —
  ABI versions never gain backward compatibility, by construction.
- The effective sandbox-domain mask at a call site is the bitwise OR of
  the operation's `qdom_domains` and every participating type's
  `baseline_qdom_domains`: unary receiver/primary type, binary primary and
  secondary types, and the constructed type at construction sites. Baseline
  domains are only for inherent type capabilities that every operation needs;
  optional I/O stays on the specific operation. For example, in-memory
  `DataFrame` filtering should normally keep
  `baseline_qdom_domains == QDOM_DEFAULT`, while CSV readers declare
  `QDOM_FILESYSTEM` and typed DBI ingestion declares `QDOM_DATABASE` on those
  operations.

**Operation-name table.** When `operation_name` is one of the names below,
the runtime recognizes the operation's category and expected signature shape.
The table does **not** grant unsafe algebraic rewrite permissions by itself.
Unsafe properties such as associativity, idempotence, identity folding, and
floating-point reassociation must be declared per signature in
`OpcodeInfoExtended`. Names outside the table are opaque.

| Name | Arity | Category / conservative default |
|---|---|---|
| `add`, `mul`, `bit_or`, `bit_and`, `bit_xor` | 2 | arithmetic/bitwise; no associativity by name |
| `sub`, `div`, `mod` | 2 | non-commutative, non-associative |
| `eq`, `ne` | 2 | comparison; commutativity must be declared per signature |
| `lt`, `le`, `gt`, `ge` | 2 | ordered comparison; non-commutative |
| `neg`, `bit_not`, `not` | 1 | unary; involution must be declared per signature |
| `subscript`, `slice` | 2 | dual-use: read-like when `signature.access == ReadOnly` (CSE candidate when `is_pure_modulo_xsink` is also declared); assignment-like when `signature.access == MutatesLhs` (no CSE; treated as a write to the receiver). The optimizer's classification follows `signature.access`, not the name. |
| `size`, `count` | 1 | dimension query; registration requires `signature.access == ReadOnly`, but purity must be declared per signature because lazy / external-backed lengths may load, cache, or throw |
| `any`, `all` | 1 | predicate reduction; purity must be declared per signature |
| `sum`, `min`, `max` | 1 | reduction; associativity/reassociation must be declared per signature |
| `mean` | 1 | reduction; FP-non-associative even when `sum` is — see fast-math note below |
| `clone` | 1 | allocation-producing copy; declare `FreshNoAliasInputs`, **do not declare `is_pure_modulo_xsink`** — CSE would coalesce two `clone(x)` calls to one, but distinct return identities are observable |

Floating-point `add`, `mul`, `sum`, `mean`, and any operation whose result
depends on evaluation order may only opt into reassociation/vector-reduction
rewrites when an explicit fast-math-style flag is present. Plain
`is_associative = true` is not sufficient for IEEE-sensitive floating-point
storage. The fast-math flag is itself a deliverable — see Phase 7 and Open
Question 10 — but the v1 operation-name table is forward-compatible: when
the flag ships, FP ops in this table will gain the additional rewrite
opt-in without changing the table itself.

Optional LLVM extension header:

```cpp
// header: include/qore/QorePluginLLVM.h (new)
//
// Including this header opts a module into libqore's exact LLVM ABI.
// Modules using it must be rebuilt when libqore's LLVM major version changes.

typedef llvm::Value* (*PluginLLVMCodegenCallback)(
    QorePluginLLVMCodegenContext* ctx);

struct QorePluginLLVMExtension {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t llvm_major_version;
    QorePluginLLVMCodegenCallback codegen;
};
```

### 3.4 Extended OpcodeInfo: declarative metadata

`struct OpcodeInfoExtended` is `OpcodeInfo` plus the following fields. All
fields default to the conservative ("worst case") value, so omitting a field
is always safe:

```cpp
enum class OpcodeTypePromotion : uint8_t {
    Exact         = 0,  //!< both operands must already be the typed shape
    WideningLattice = 1, //!< (int8,int16)→int16, (float32,int64)→float64, ...
    IdentityLHS   = 2,  //!< result type follows lhs unchanged
    Custom        = 3,  //!< invoke type_promotion_callback
};

struct OpcodeInfoExtended : public OpcodeInfo {
    // Algebraic — contractual: optimizer reorders/removes work based on these.
    bool is_commutative;           //!< op(a, b) == op(b, a)
    bool is_associative;           //!< op(op(a, b), c) == op(a, op(b, c))
    bool is_idempotent;            //!< op(op(a)) == op(a) for unary
    bool annihilator_zero;         //!< x * 0 = 0, x AND false = false, etc.

    //! Identity element for binary algebraic ops. has_identity == false ⇒
    //! the op has no identity (or none worth folding). Builtin scalar
    //! identities use identity_kind. Plugin-owned identities are produced by
    //! make_identity so refcounted/program-owned values are not stored inside
    //! constexpr-style metadata.
    //!
    //! Caching contract: the runtime calls make_identity at most once per
    //! (operation, result_type) pair. The returned NaN-boxed bits and one
    //! owning reference are cached for the lifetime of the metadata table
    //! (process lifetime in practice). The cached value is shared, immutable,
    //! and refcounted — plugin authors must return a value safe to share via
    //! refcount across all use sites. Callers do NOT decref the returned
    //! identity; the metadata table owns it.
    bool has_identity;
    uint8_t identity_kind;            //!< 0=custom/none, 1=int0, 2=int1, 3=float0, ...
    uint64_t (*make_identity)(const QoreTypeInfo* result_type, ExceptionSink* xsink);

    // Effect — contractual.
    bool is_pure_modulo_xsink;     //!< no side effects except via ExceptionSink
    bool can_vectorize;            //!< safe to apply elementwise across a buffer

    // Type system — contractual.
    OpcodeTypePromotion type_promotion_kind;
    PluginTypePromotionCallback type_promotion_callback; //!< if kind == Custom

    // Hints — advisory only. Affect cost modelling and codegen heuristics
    // but never correctness; an optimizer that ignores these must still
    // produce equivalent program results.
    bool is_simd_friendly;         //!< runtime helper has SIMD codegen
    uint8_t cost_class;            //!< 0=O(1), 1=O(log n), 2=O(n), 3=O(n²)+
};
```

Optimizer passes use these fields the same way they currently use
`may_have_side_effects` and `may_throw_exception`:

- `is_pure_modulo_xsink` enables CSE, loop-invariant code motion, dead-code
  elimination.
- `is_commutative` + `is_associative` enable reordering for SIMD-friendly
  reductions.
- `has_identity` + `identity_kind` / `make_identity` enable neutral-element folding.
- `is_idempotent` enables redundant-op elimination.
- `type_promotion_kind` enables compile-time width inference instead of
  runtime dispatch.
- `signature.access` (§3.3) feeds alias analysis: `ReadOnly` permits
  reordering reads of the operands across the call site; `MutatesLhs`,
  `MutatesRhs`, `MutatesBoth` disable that reordering for the named
  operand and force the optimizer to treat the call as a write to that
  operand's storage for purposes of memory-dependency tracking.
- `signature.result_alias` tells the optimizer whether the return value may
  alias operand storage. `FreshNoAliasInputs` is required for copy-like
  allocation operations such as `clone`; those operations are not pure just
  because allocation is their only side effect.
- `is_simd_friendly` and `cost_class` are advisory inputs to scheduling,
  outlining, and codegen heuristics; they do not affect correctness.

**Verification.** Misdeclared metadata is a soundness hazard. The protocol's
position is that *the plugin module's own test suite is the source of truth*
— the runtime cannot in general synthesise representative values for an
opaque plugin type, and a half-baked property tester would give false
confidence rather than catch real bugs. Mitigations are therefore stratified:

- **Required, runtime-side, cheap.** Every registered operation declares its
  input/output type signature. Debug builds assert that runtime helpers
  produce values whose tag matches the declared output type; mismatch is a
  hard assertion. This catches the most common class of errors (returning
  `NOTHING` from an op declared non-nothing, returning the wrong width).
- **Optional, plugin-side, thorough.** Plugin authors may register a test-data
  generator through a future `"qore.plugin.testdata"` extension to opt into
  runtime property checks. The base v1 ABI does not define that generator type.
  When such an extension is present, debug builds can run a bounded number of
  `op(a, b) == op(b, a)` / `op(x, identity) == x` / `op(op(a)) == op(a)`
  checks per process at first use of each operation. When absent, those checks
  are skipped — the optimizer still trusts the metadata, and the plugin's CI
  suite is responsible for catching errors.
- **Conservative defaults.** Omitted fields default to the safe value (not
  commutative, not associative, not pure, no identity). Release builds run
  no property checks regardless of generator presence.
- **Deopt does not catch bad algebra.** Type guards and deopt protect
  type-shape specialization. They cannot detect that the optimizer reordered
  or removed work based on a false `is_commutative` / `is_pure_modulo_xsink`
  claim. A plugin that lies about its algebra silently miscompiles, and the
  protocol's guarantee is no stronger than the plugin's test suite.

### 3.5 Runtime helper protocol

Plugin-type runtime helpers follow the existing `qore_rt_*` value-helper shape
where possible:

```c
extern "C" DLLEXPORT uint64_t qore_rt_<module>_<op>_<typesig>(
    uint64_t lhs_bits,
    uint64_t rhs_bits,
    ExceptionSink* xsink);
```

NaN-boxed `uint64_t` operands and return; `ExceptionSink*` for errors;
`QORE_RT_CHECK_THROW(xsink)` translates to JIT C++ exception. Symbols are
externally visible (`DLLEXPORT`) when exported by the module, but the runtime
does not currently perform generic `dlsym` lookup for JIT helpers.

**Resolution mechanism.** Three execution tiers must reach the registered
helper pointer; the protocol uses a process-global immutable helper table plus
per-Program activation/import state:

1. **IR interpreter.** `qore_register_plugin_types_v1` populates a
   process-global immutable helper table indexed by global operation id. Each
   entry stores `(void* helper, QorePluginHelperAbi helper_abi, signature)`.
   Each Program carries an activation/import bitset that says which global ids
   it may reference. The interpreter's
   `PluginUnary` / `PluginBinary` / `PluginCall` / `PluginSubscript` handlers
   read the descriptor's global id from the instruction's operand stream,
   verify the Program activation bit and expected helper ABI in debug builds,
   cast to the declared trampoline type, and call it.
2. **JIT.** The ORC symbol resolver consults the same table on first
   reference: `qore_rt_<module>_<op>_<typesig>` becomes a generated lookup
   stub that loads from the table at the slot recorded in the IR
   instruction. Lazy compilation is therefore a no-op — the table is already
   populated by registration time.
3. **AOT.** `QoreAOTContext` gains a new slot-table array
   `plugin_helpers: void*[]` plus parallel helper-ABI/signature metadata
   populated alongside the existing `LocalVar*` / `Var*` / expression arrays.
   AOT-emitted code references plugin helpers by slot index
   (`ctx->plugin_helpers[N]`), not by symbol. QORD serializes the per-function
   mapping from local slot index to global operation id in `PLUGIN_HELPER_REFS`
   (§3.9), so an artifact never assumes that two Programs have identical local
   helper-table layouts. The new
   `qore_aot_module_init_v4` entry point fills `plugin_helpers[]` after the
   `PLUGIN_IMPORTS` section has been resolved against loaded modules — the
   discipline matches `qore_aot_module_init_v3`'s expression-slot
   population. v3-and-earlier AOT artifacts that do not reference plugin
   types continue to load through the existing init entry points unchanged.

This is the same indirection model `QoreAOTContext` already uses for
runtime-resolved pointers; plugin helpers are simply one more slot table.

For helpers that operate on dense buffers (the `buffer<T>` case), an
auxiliary signature shape carries a stride and length:

```c
extern "C" DLLEXPORT uint64_t qore_rt_<module>_buffer_<op>_<elemtype>(
    void* result_buffer_data,    //!< pre-allocated by caller
    int64_t result_size,
    const void* lhs_data,
    int64_t lhs_size,
    int64_t lhs_stride,
    const void* rhs_data,
    int64_t rhs_size,
    int64_t rhs_stride,
    ExceptionSink* xsink);
```

Element-type-specific symbols (`_int64`, `_float64`, `_int32`, etc.) allow
SIMD specialization without runtime dispatch.

### 3.6 LLVM codegen hooks

Modules optionally provide LLVM IR generators per registered operation. Without
this, the IR-to-LLVM lowering emits a runtime-helper call by default —
adequate for most plugin types.

Modules that want native SIMD / inline codegen register the
`"qore.plugin.llvm.codegen"` operation extension from `QorePluginLLVM.h`.
`QorePluginLLVMCodegenContext` exposes the registered operation signature,
boxed QoreValue operands, exception-sink pointer, active LLVM context,
builder, module, and function in a way consistent with the rest of
`QoreIRToLLVM.cpp`. The callback returns an `i64` `QoreValue` bit pattern.

**LLVM ABI coupling — largest forever-promise in the protocol.** The
callback signature names `llvm::Value*` and `llvm::IRBuilder<>*` directly,
which means a plugin module that uses this hook is hard-bound to libqore's
exact LLVM version: an LLVM major-version bump in libqore breaks every
plugin that links against the codegen callback. This is the single biggest
ABI-stability commitment in the entire plugin protocol — bigger than the
QORD format and bigger than the runtime helper ABI, both of which are
plain-C and version-stable across compiler updates.

**Runtime compatibility check.** `qore_register_plugin_types_v1` MUST validate
the `"qore.plugin.llvm.codegen"` extension's struct size, extension ABI version,
LLVM major version, and callback pointer. If the extension is optional
(`required = false`) and incompatible with the libqore build, registration
ignores the callback and the operation loads in runtime-helper mode without
inline codegen. If the extension is required (`required = true`), the same
mismatch rejects registration with a diagnostic naming the offending field and
module. The version fields are therefore load-bearing, not decoration.

The recommended discipline is therefore:

- The runtime-helper path (§3.5) is the supported, stable extension surface
  for all plugin types. Most types should stop here.
- The LLVM codegen hook is offered as an opt-in performance escape hatch for
  the small number of types that need inline SIMD beyond what the runtime
  helper achieves. Modules using it accept that an LLVM bump in libqore is a
  rebuild event for them.
- A future protocol version may replace the raw `llvm::*` types with a thin
  function-pointer-returning shim API that hides the LLVM version, but the
  v1 protocol intentionally does not promise this — premature abstraction
  here would constrain the codegen quality the hook exists to enable.

### 3.7 Lowering pattern hooks

Plugin types participate in IR lowering-time AST-pattern matching to pick their
typed operation descriptors when both operand types are known. A module
registers a pattern matcher via the `PluginLoweringCallback` typedef
(§3.3), which returns a `PluginLoweringResult` enum rather than a bare
`bool` so the no-silent-fallback invariant can be enforced mechanically.
`QoreIRLowering` copies matching callbacks out of the registry before invoking
them, so callbacks never run under the registry mutex.

The callback inspects the AST node, queries `parse_ctx` for operand types,
and returns one of:

- **`Lowered`** — emitted IR and made the result available through
  `QoreIRLoweringContext::setResult()` or, for simple cases, by emitting a
  result-producing instruction as the final instruction in the active block.
- **`NotApplicable`** — the AST node is outside the matcher's claimed
  coverage (`lowering_claimed_node_kinds` bitmap). The lowering pass
  proceeds to the next matcher or, if none match, the `.any`-fallback path.
- **`Erroneous`** — the matcher detected a hard error (e.g., a type
  annotation it knows it cannot represent) and reported it through
  `QoreIRLoweringContext::setError()`. Lowering aborts with the reported
  diagnostic.

`NotApplicable` returned for an AST node kind *inside* the matcher's
declared `lowering_claimed_node_kinds` bitmap is treated as a parse-time
error in `%modern`: a plugin matcher that claims to handle a node kind is
required to either lower it or report `Erroneous`. The diagnostic is
`PLUGIN-LOWERING-CLAIM-VIOLATED` (see §3.12 error-code table). This
closes the silent miscompile gap that a bare `false` return would leave
open.

### 3.8 Tag allocation in NaN-boxed `QoreValue`

All plugin types in the v1 protocol use pointer representation managed by
the registered native data-type descriptor. **No plugin immediate tags are
allocated in v1.** This avoids the single largest soundness hazard the v1
protocol could carry: the tempting `0xFFE0..0xFFE7` range is currently
unsafe because values below the integer tag boundary can be classified as
doubles by `QoreValue::isFloat()`, and a plugin claiming an immediate tag
without a complete `QoreValue`-classification audit would silently
miscompile every fast predicate that touches its values.

A future protocol version may add an immediate-tag mechanism after the
Phase-0 audit identifies a safe reserved range. Until then: pointer-tagged
values and native data-type nodes are the only supported representations,
and modules do not pick their own tags.

### 3.9 QORD format extensions

Three new section types:

```cpp
enum class QoreAOTSectionType : uint16_t {
    // ...existing 1-22...
    PLUGIN_TYPE_REGISTRY = 23,  //!< Per-module plugin-type metadata
    PLUGIN_IMPORTS       = 24,  //!< Required modules + type/op versions
    PLUGIN_HELPER_REFS   = 25,  //!< Slot-index → operation import reference
};
```

`PLUGIN_TYPE_REGISTRY` records symbolic metadata exported by each referenced
module: stable type-local ids, display names, type names, operation-local ids,
operation names, signatures, and serialization format versions. It does **not**
store function pointers, constructors, callbacks, or other process-local
addresses. At load time, the resolved module registration table binds these
symbolic records to live type descriptors, serializer/deserializer callbacks,
and helper pointers. `PLUGIN_IMPORTS` records the hard requirements of the QORD
artifact (every reference is a hard requirement; there is no "soft plugin
import" — soft fallback is reserved for core QORD format-feature bits, which
never reference plugins):

```text
(module_name, plugin_abi_version, operation_set_version,
 required_type_ids[], required_operation_ids[])
```

The section records names and versions, not globally assigned feature bits.
`feature_flags` remains a core-format capability bitmap.

`PLUGIN_HELPER_REFS` is the wire-format counterpart of the `plugin_helpers[]`
slot table on `QoreAOTContext` (§3.5). Wire format (per entry, little-endian,
no padding):

```text
uint16  slot_idx       // index into ctx->plugin_helpers[]
uint16  import_idx     // index into PLUGIN_IMPORTS
uint16  op_local_id    // matches QorePluginOperation.local_id in that import
uint8   canonical_signature_version
uint8   reserved       // must be 0
uint64  signature_hash // xxHash64 of canonical signature form (see below)
```

Total entry size 16 bytes. Loader behaviour: resolve `(import_idx,
op_local_id)` against the live `PLUGIN_IMPORTS` table to a process-global
operation id, verify `reserved == 0`, verify that
`canonical_signature_version` is recognized, verify the live registration's
signature hash for that canonical version matches `signature_hash`, and write
the resolved helper pointer into `ctx->plugin_helpers[slot_idx]`. The signature
hash prevents accidentally binding an operation whose local id was reused with
a different ABI in an incompatible module build; mismatch rejects the artifact
with a diagnostic naming both the recorded and live signature hashes.

`signature_hash` is a canonical wire hash, not a hash of the in-memory
`QorePluginOperationSignature` bytes. The 64 bits in the wire format are
xxHash64 with seed 0 over the canonical-form bytes in the order below — same
hash primitive and seed as `QoreAOTBinaryHeader::source_hash` so QORD readers
and writers reuse a single implementation. All integers are little-endian and
all enums are encoded as their assigned `uint8_t` ABI values:

```text
uint8   canonical_signature_version = 1
uint8   arity
uint8   helper_abi
uint8   access
uint8   result_alias
uint8   primary_nullable            // 0 or 1
uint8   secondary_nullable          // 0 or 1
uint8   return_nullable             // 0 or 1
type_ref primary_type
type_ref secondary_type             // kind Null when arity != 2
type_ref return_type
```

`type_ref` is itself canonical:

```text
uint8   kind
payload
```

`kind == 0` means null and has no payload. `kind == 1` means a Qore
non-plugin type and the payload is the canonical QORD type path as a
`uint32 byte_count` plus UTF-8 bytes, using the same spelling returned by
`QoreTypeInfo::getPath()` / `qore_type_get_path()` and consumed by
`QoreAOTTypeResolver`. This path form is the canonical spelling for signature
hashing; display names such as `Qore::Reflection::Type::getName()` are not
used because class and hashdecl simple names can collide across namespaces.
`kind == 2` means a plugin type and the payload is canonical `module_name`
(per the rules below) plus `uint16 type_local_id`. Other `kind` values are
reserved and invalid in canonical signature version 1.

**Canonical signature version policy.** `canonical_signature_version` bumps
on **any** byte-layout change to the canonical form, including field
appends. Trailing-field appends are explicitly not version-stable: the
writer always emits the wire-field version that matches its layout and uses the
same byte as the first byte of the hash preimage.

The version byte is stored in the `PLUGIN_HELPER_REFS` entry header and is also
the first byte of the canonical-form hash preimage. The writer uses the same
value for both roles; the preimage copy is not serialized independently. This
lets loaders fast-reject unsupported versions in O(1) before paying for
canonical-form serialisation and hashing of the live signature. A loader that
sees an unrecognised `canonical_signature_version` in a `PLUGIN_HELPER_REFS`
entry rejects that entry with `QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED`
(subreason `unsupported_canonical_version`, see §3.12 error-code table) before
comparing hashes. For a supported header version, the loader recomputes the
canonical hash using that header byte as the preimage version; any writer bug
that hashed a different version byte is reported as
`QORD-PLUGIN-SIGNATURE-HASH-MISMATCH`. This keeps version skew loud without
inventing a second serialized version field.

Pointer-valued fields such as `QoreTypeInfo*` are therefore normalized to
stable identities before hashing. The hash excludes process-local addresses,
helper pointers, extension pointers, and padding so the same artifact can load
in processes with different address layouts.

`module_name` is canonicalised as: UTF-8 byte sequence with no Unicode
normalization and no case folding, length-prefixed by `uint32 byte_count`
in little-endian for the canonical hash, byte-exact comparison. QORD may store
the same bytes through the normal `STRINGS` pool, but hashing never depends on
string-pool offsets or host `char*` addresses. Implementations MUST NOT trim
whitespace, fold case, or NFC-normalise the name before hashing or comparing.

`qore_aot_module_init_v4` performs this resolution after `PLUGIN_IMPORTS`
has been resolved against loaded modules. The wire format intentionally
records the unresolved `(import_idx, op_local_id)` pair rather than a
resolved global id so artifacts remain portable across processes whose
plugin descriptors are loaded in different orders.

New value tag (or range):

```cpp
enum class QoreAOTValueTag : uint8_t {
    // ...existing 0-19...
    VT_PLUGIN_INSTANCE = 20,  //!< Module-defined value type
};
```

Concrete wire encoding (little-endian, no padding):

```text
uint16  import_idx                 // index into PLUGIN_IMPORTS
uint16  type_local_id              // matches QorePluginTypeDescriptor.local_type_id
uint16  serializer_format_version  // matches the live descriptor's value
uint16  reserved                   // must be 0; future flags
uint32  payload_length              // bytes that follow
uint8   payload[payload_length]     // module-defined
```

Total fixed-header size is 12 bytes; payload is module-defined and opaque
to the loader. The reader rejects the entry if `import_idx` is out of
range, if `type_local_id` is not registered in the resolved module, if
`serializer_format_version` is not supported by the live deserializer, or
if `reserved != 0`. Runtime then calls the registered deserializer with
the raw payload bytes and length.

Header field `max_opcode_id` continues to record only built-in opcode ids for
the first plugin protocol version. Plugin operations are represented by
builtin plugin-dispatch opcodes plus serialized operation descriptors. If a
future protocol introduces true dynamic opcode ids, `max_opcode_id` must either
become plugin-aware after import resolution or be replaced by a richer opcode
table compatibility check.

Loader behaviour:

1. Read `PLUGIN_IMPORTS`; resolve each referenced module against loaded
   modules.
2. Verify each loaded module provides a compatible plugin ABI and
   operation-set version.
3. Verify every required type id and operation id is present, active in the
   target Program, has a supported canonical signature version, and has a
   compatible canonical signature hash / serialization format version.
4. Verify `header.feature_flags & ~runtime_supported == 0` for core QORD
   format features. (Plugin imports are not encoded in `feature_flags` —
   they have their own section. Unsupported core features may still fall
   back to JIT per existing rules; missing or version-mismatched plugin
   imports always reject the artifact.)
5. On mismatch, reject with a diagnostic listing the missing or inactive
   module, version, type, operation, signature, or serializer version.

### 3.10 Cross-module type interaction

When module A defines `Tensor` and module B defines `Buffer<float64>`, and a
user writes `tensor + buffer`, who registers `add.tensor_buffer`?

**Resolution rules.** Cross-type operations follow a fixed precedence:

1. **Explicit registration wins.** If any loaded module has registered an
   operation for the exact `(LHS_type_id, RHS_type_id, op)` triple, that
   registration is used.
2. **Conflicting explicit registrations are a load-time error.** Two
   registrations for the same triple cause `qore_register_plugin_types_v1`
   to fail with a diagnostic naming both modules. There is no "last wins";
   silent overwrite is a footgun. A module that needs to override another
   module's cross-type op must do so explicitly with a versioned operation-
   set number that the other module declares an upper bound on. Under
   concurrent module-init, the registry write lock serializes conflict checks
   against committed descriptors and pending registration records. A registrant
   that collides with another module's pending triple records the dependency,
   releases the registry lock, waits for that module's init transaction to
   finish, then reacquires the lock and retries the conflict check: if the other
   module committed, this registration fails with a diagnostic naming both
   modules; if the other module failed and its pending record was discarded,
   this registration may proceed. The diagnostic content is independent of
   timing — both module names appear regardless — so test harnesses do not
   depend on serialization order.

   A registrant that conflicts with multiple pending registrations waits on
   them in registration-order — meaning the order in which the conflicting
   pending transactions acquired the registry write lock, tracked internally
   by the registry's pending-transaction list. The ordering is part of the
   runtime contract; plugin authors don't introspect it directly but rely
   on its determinism through the wait-cycle and conflict diagnostics. Each
   wakeup re-checks the full set of conflicts before either succeeding,
   failing with a committed-conflict diagnostic, or waiting on the next
   pending registrant. There is no bound on how many sequential waits a
   registrant may incur; waits that would create a registration-internal
   wait-for cycle fail immediately with `PLUGIN-REGISTRATION-WAIT-CYCLE`
   per §3.3 rather than blocking. The wait sequence is deterministic for
   any given lock-acquisition order — once the registry has observed a
   fixed sequence of write-lock acquisitions among concurrent module-inits,
   the resulting wait queue is a pure function of that observed order.
   The acquisition order itself remains subject to thread scheduling and
   is not reproducible across runs.
3. **Commutative auto-symmetry.** If a registration for
   `(A, B, op)` declares `is_commutative = true`, the runtime synthesises
   the `(B, A, op)` site automatically — registering both halves
   explicitly is redundant. Auto-synthesised entries participate in
   conflict detection per rule 2: a subsequent explicit registration of
   `(B, A, op)` from any module fails with a diagnostic naming both the
   originating commutative registration and the conflicting explicit one.
4. **Default lattice.** If no explicit registration exists and one operand's
   type declares `type_promotion_kind == WideningLattice`, the lowering
   pass attempts to widen the narrower side to the wider side and dispatch
   the same-type op. This covers `(int, buffer<int64>)` → widen scalar to
   buffer-broadcast and similar common cases without hand-written
   cross-type ops.
5. **`.any` fallback.** If none of the above apply, lowering emits the
   `.any` opcode and runtime dispatches via type-tag matching on both
   operands. Correctness is preserved (no silent miscompile); the cost is
   slower dispatch.

Registration has two levels:

- **Process-global immutable descriptors.** Once a module binary is loaded, its
  plugin descriptors are interned process-wide so type ids, helper pointers,
  and operation metadata have stable addresses and can be shared by JIT/AOT
  code. Descriptors live for the process lifetime: module unload does not
  drop them and `dlclose` is not called on a module whose descriptors are
  reachable from any QORD-baked reference. This guarantees that QORD imports
  and AOT slot-table entries remain valid across Program lifecycles, which
  is necessary because a JIT-compiled function or an AOT artifact may bind
  a helper pointer or type id at one Program's parse time and execute it in
  another Program's context.
- **Per-Program activation/import tables.** A Program only sees plugin types
  from modules it imported or from modules named by a QORD `PLUGIN_IMPORTS`
  section it is loading. This preserves optional-module semantics: loading
  `dataframe` in one Program does not silently expose DataFrame syntax or types
  in unrelated Programs.

**Activation vs. registration.** Activation is separate from registration. `qore_register_plugin_types_v1` runs
when the binary module's `qore_module_init` runs and installs process-global
descriptor/helper entries. If that already-loaded module is later added to a
different `QoreProgram`, module load/add-to-Program logic marks the existing
global plugin ids active in that Program; it does not rerun registration or
allocate new global ids. QORD `PLUGIN_IMPORTS` processing performs the same
Program-local activation for imported plugin modules before resolving baked
helper refs.

Cross-type registrations are visible only when all participating modules are
active in the Program being parsed or loaded. Conflicting process-global
registrations are still rejected at registration time.

QORD load-time check: a binary that references the cross-type triple
`(A, B, op)` fails to load unless both modules are active in the target Program
and either an explicit registration for that triple is in place, or both
modules' declared operation-set versions admit the lattice fallback the binary
recorded at build time.

### 3.11 Operator dispatch model

The IR's typed-opcode-vs-`.any` mechanism *is* the dispatch architecture.
Plugin-type operator dispatch falls out without a separate decision:

- **Parse-time monomorphization** when both operand types are statically
  known → plugin-dispatch opcode plus typed operation descriptor emitted
  directly. Effectively pre-resolved multimethod resolution baked at parse time.
- **`.any` runtime helper** when types are ambiguous → runtime dispatch on
  type tags via the registered helper. The helper internally can implement
  multimethod lookup or single-dispatch with fallback — module's choice.
- **Profile-guided specialization** adaptively promotes hot `.any` call
  sites to guarded monomorphic plugin-dispatch descriptors via guards + deopt.

The user-facing model can be presented as Julia-style multimethod
registration because the typed-opcode-per-pair pattern naturally supports
it. There is no need for a separate language-level multiple-dispatch
feature; the architecture already does it.

### 3.12 Validation and developer experience

Plugin authors writing their first module against this protocol need
debugging support that goes beyond "registration returned -1, see
xsink." The bone-level rejection rules in §3.3 / §3.9 / §3.10 are
necessary but not sufficient. This subsection specifies the developer-
experience surface, all of which is part of the v1 ABI ship in Phase 3.

**Structured error codes.** Every rejection raises a Qore exception with
one of the following error codes (matching existing `AOT-PENDING-CONSTANT`
/ `RUNTIME-OVERLOAD`-style convention). Tooling and CI test harnesses
match against the code, not the message. Codes are stable families; the
structured violation payload carries the precise `field_name` and `subreason`
for field-level checks. Diagnostics that compare two runtime entities carry
`related_module_name`, `related_type_name`, and `related_operation_name` fields
when applicable instead of encoding those names into `subreason`. Each
diagnostic message MUST include: offending module name, offending type/operation
name when known, the field that violated the rule for field-level checks,
expected vs. actual values when a comparison exists, and the section number of
this design that defines the rule. When the structured payload populates any
`related_*` field, the diagnostic message MUST include that name rather than a
paraphrase such as "the previously-registered module". The message is the
human-readable surface and the structured fields are the test-harness surface,
but both must carry the same critical comparison data.

**Diagnostic name escaping.** Names rendered into messages are emitted
byte-for-byte except for the following byte-set, which is rendered as
`\xHH` (two uppercase hex digits per byte):

- `0x00`–`0x1F` (C0 controls, including `TAB`, `LF`, `CR`)
- `0x7F` (DEL)
- `0x80`–`0x9F` (C1 controls, when the byte is the first byte of a valid
  UTF-8 sequence whose code point falls in this range)
- `0x22` (`"`), `0x27` (`'`), `0x5C` (`\`)
- any byte that does not start a valid UTF-8 sequence

All other bytes appear verbatim. This byte-set is normative: implementations
MUST escape exactly this set. Adding bytes to the escape set is a future
revision to this design; subtracting bytes never happens.

**UTF-8 scanner discipline.** The scanner is byte-oriented. At each
position, if the next byte starts a complete and valid UTF-8 sequence
(1, 2, 3, or 4 bytes per RFC 3629), the entire code point is consumed and
emitted either verbatim or escaped according to the byte-set rule above
(the escape decision uses the first byte of the sequence). If the next
byte does not start a valid sequence — including stray continuation
bytes, truncated sequences, overlong encodings, and surrogate-range code
points — that single byte is emitted as `\xHH` and the scanner advances
by one byte. Bytes after an invalid byte are re-evaluated from scratch.

**Bijection.** The escaping is bijective: a parser scanning the rendered
output recovers the exact structured-field bytes. The bijection holds
because `\` is unconditionally in the escape set, so a literal
backslash in the input never collides with the rendered escape sequence.
Worked example: input bytes `5C 78 34 44` (the four literal characters
`\x4D`) are rendered as `\x5Cx4D` — the `\` is escaped to `\x5C`, then
`x4D` appears verbatim. A bijective parser scanning `\x5Cx4D` consumes
`\x5C` as one escape (decoding to `0x5C`), then reads `x`, `4`, `D` as
three verbatim bytes (`0x78`, `0x34`, `0x44`). The original input bytes
are recovered exactly.

**Length.** Messages have no protocol-imposed length cap; the
verbatim-plus-escape encoding can inflate output up to 4× input bytes for
fully invalid input. Module/type/operation names are short enough in
practice that this is not a concern. Implementations rendering messages
to fixed-width displays may truncate, but truncated output breaks the
bijective recovery guarantee — tools that need exact byte recovery
should read the structured `related_*` fields rather than parsing
truncated messages.

| Error code | Raised by | Section |
|---|---|---|
| `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` | registration / validation (bad counts, null registration/init context, null names/type info, non-contiguous ids, invalid reserved values, stale module handles) | §3.3 |
| `PLUGIN-REGISTRATION-NULL-LIFECYCLE` | registration / validation | §3.3 |
| `PLUGIN-REGISTRATION-NULL-CODEC` | registration / validation (missing serializer/deserializer for a QORD-visible type) | §3.3 / §3.9 |
| `PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID` | registration / validation | §3.3 |
| `PLUGIN-REGISTRATION-SIGNATURE-CONFLICT` | registration / validation | §3.3 |
| `PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING` | registration / contextual validation | §3.3 / §3.5 |
| `PLUGIN-REGISTRATION-OPERATION-SET-VERSION-INCOMPATIBLE` | registration / contextual validation | §3.3 |
| `PLUGIN-EXTENSION-ABI-MISMATCH` | registration / validation (LLVM extension version mismatch) | §3.3 / §3.6 |
| `PLUGIN-EXTENSION-UNRECOGNIZED-REQUIRED` | registration / validation | §3.3 |
| `PLUGIN-EXTENSION-VALIDATION-FAILED` | registration / validation (recognized extension whose payload fails its own validation, non-version causes; subreason carries per-extension specifics) | §3.3 |
| `PLUGIN-CROSS-TYPE-CONFLICT` | registration / contextual validation (cross-module); subreason `pending_conflict` when the conflict is with a still-pending registration seen during dry-run validation, with the pending module in `related_module_name` | §3.10 / §3.12 |
| `PLUGIN-REGISTRATION-WAIT-CYCLE` | registration wait-for-graph cycle detection; subreason `wait_cycle`, with the other transaction in `related_module_name` | §3.3 |
| `PLUGIN-LOWERING-CLAIM-VIOLATED` | parse-time lowering | §3.7 |
| `PLUGIN-HELPER-RESULT-TYPE-MISMATCH` | runtime verifier | §3.4 |
| `PLUGIN-HELPER-ALIAS-CONTRACT-VIOLATED` | runtime verifier | §3.3 (`ReturnsLhs`/`ReturnsRhs`) |
| `PLUGIN-HELPER-ABI-MISMATCH` | runtime verifier and runtime dispatch; subreason `helper_abi_mismatch` when the opcode/helper ABI or helper call frame shape is wrong, `operation_not_registered` when the referenced operation ID is absent, and `module_not_loaded` when a process-local operation ID resolves to a module that is no longer loaded | §3.5 |
| `QORD-PLUGIN-IMPORT-MISSING` | QORD loader; subreason `module_not_loaded` when no module by that name is loaded in the process, when its load already failed, or when its module-init transaction rolled back; `module_pending` when the named import matches a still-pending module-init transaction. Both cases are positive subreasons so harnesses match on the subreason rather than its absence | §3.9 |
| `QORD-PLUGIN-HELPER-REF-INVALID` | QORD loader (`slot_idx`, `import_idx`, or `op_local_id` invalid) | §3.9 |
| `QORD-PLUGIN-SIGNATURE-HASH-MISMATCH` | QORD loader | §3.9 |
| `QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED` | QORD loader (unrecognised `canonical_signature_version` byte; subreason `unsupported_canonical_version`) | §3.9 |
| `QORD-PLUGIN-SERIALIZER-VERSION-UNSUPPORTED` | QORD loader | §3.9 |
| `QORD-PLUGIN-RESERVED-NONZERO` | QORD loader | §3.9 |
| `QORD-PLUGIN-PAYLOAD-LENGTH-MISMATCH` | QORD loader (deserializer consumption out of bounds; subreason is `over_read` for callback boundary failure or `under_consumed` for fewer-than-payload-len bytes consumed) | §3.3 / §3.9 |
| `PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE` | `Qore::Reflection::PluginRegistry` (Program-bound method called on the process-global view); subreason `program_not_available` | §3.12 |
| `PLUGIN-REGISTRY-MODULE-NOT-LOADED` | `Qore::Reflection::PluginRegistry` `getTypes` / `getOperations` / operation-id lookup called with a module name that is not loaded in the process; subreason `module_not_loaded` | §3.12 |
| `PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED` | operation-id lookup called with a module-local operation id that is not registered in the loaded module; subreason `operation_not_registered` | §3.12 |

**Dry-run validation entry point.** Authors testing a descriptor before
wiring it into module init use:

```cpp
struct QorePluginModuleHandle;  // opaque, runtime-owned

enum QorePluginValidationContextFlags : uint32_t {
    QORE_PLUGIN_VALIDATE_NO_FLAGS = 0,
    QORE_PLUGIN_VALIDATE_RESERVED_MASK = 0xFFFFFFFFu,
};

struct QorePluginValidationContext {
    //! Size of this struct as known to the caller. MUST be set to
    //! sizeof(QorePluginValidationContext) by callers compiled against a
    //! given header revision. The runtime reads only up to min(struct_size,
    //! its-own-sizeof) bytes; missing trailing fields take their default
    //! values. Adding a field bumps the canonical sizeof; never removes,
    //! never reorders. This avoids needing _v2 / _v3 of the validator
    //! entry point as the context grows.
    uint32_t struct_size;

    //! Validation flags. v1 defines no nonzero flags; any unknown nonzero
    //! bit is rejected as PLUGIN-REGISTRATION-INVALID-DESCRIPTOR with
    //! subreason "unknown_validation_context_flag". Future versions may
    //! define optional flags, but required semantics still need a new
    //! validator symbol.
    uint32_t flags;

    //! Optional runtime-owned module handle used to validate
    //! runtime_helper_symbol lookup against the registering binary. The only
    //! valid non-null value is the opaque handle supplied by
    //! QoreModuleInitContext::plugin_module_handle, and copied into
    //! QorePluginRegistrationContextV1::module_handle, for the module
    //! currently being initialized.
    //! The handle is valid ONLY during the calling thread's qore_module_init
    //! invocation that received it; using a cached handle outside that scope
    //! is undefined behaviour, and the runtime is permitted (but not
    //! required) to detect and reject reuse. Validation results are not
    //! cacheable across init invocations: a successful validation does not
    //! guarantee the same descriptor will register cleanly in a later
    //! qore_module_init pass for a different module.
    //! Unit tests and tools outside module init pass nullptr; symbol resolution
    //! is then reported as a deferred check in collect-all mode and is
    //! performed by registration.
    const QorePluginModuleHandle* module_handle;

    //! Optional Program used to validate per-Program activation/import rules.
    //! nullptr means process-global descriptor validation only.
    const QoreProgram* program;
};

//! Validate registration without committing. Returns 0 if reg would
//! register cleanly in the supplied context, -1 with diagnostics. Does
//! not register anything, does not allocate global ids, does not invoke
//! runtime helpers, and is safe to call from a unit test or a
//! sample-module's startup self-check. collect_all controls whether the
//! validator walks all descriptor-local rules or stops at the first
//! violation.
int qore_validate_plugin_types_v1(
    const QorePluginTypeRegistration* reg,
    const QorePluginValidationContext* ctx,
    bool collect_all,            //!< true: report all violations; false: fail-fast
    ExceptionSink* xsink);
```

The `module_handle` field of `QorePluginValidationContext` accepts the same
opaque handle that `QorePluginRegistrationContextV1::module_handle` uses,
ultimately copied from `QoreModuleInitContext::plugin_module_handle` during the
calling thread's `qore_module_init` invocation. The validator and registration
share a single handle type: a plugin author writing a self-test inside
`qore_module_init` reads the handle once from the init context and passes it to
either entry point through the relevant C ABI context.

Validator behaviour by `module_handle` value:

- **Non-null and matches the live TLS handle for this thread's
  `qore_module_init`** — full contextual validation runs, including
  `runtime_helper_symbol` lookup against the module's binary.
- **Non-null but does NOT match the live TLS handle (or no
  `qore_module_init` is in progress on this thread)** — the validator
  returns `-1` with `module_handle_stale`. The validator compares pointer
  values only and never dereferences the supplied handle, so a wild pointer
  in `ctx->module_handle` is rejected without UB. Test harnesses may assert
  this mandatory TLS-mismatch case. They should not assert that an arbitrary
  cached handle from an earlier init invocation is always detected after the
  runtime has left module init; that broader stale detection remains
  best-effort per §3.3.
- **`nullptr`** — descriptor-only validation; helper-symbol lookup is
  reported as a deferred check in collect-all mode and is not performed.
  This is the dry-run mode for unit tests and tools that run outside any
  `qore_module_init` invocation.

A non-null `ctx` with `struct_size < offsetof(QorePluginValidationContext,
module_handle)` is rejected as `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` with
subreason `"validation_context_too_small"` because the runtime cannot read the
v1 flag word safely. Missing trailing fields at larger but older `struct_size`
values use their documented default values.

Future validation flags are assigned by clearing bits from
`QORE_PLUGIN_VALIDATE_RESERVED_MASK` in later headers. v1 defines no nonzero
flags, so any bit set in `ctx->flags` is unknown and rejected with subreason
`"unknown_validation_context_flag"`. Candidate future behaviours such as
"treat deferred contextual checks as failures" are described in prose until
they become real ABI flags.

The mask is informational only — callers do not mask against it before
passing flags. The runtime is the sole arbiter of which bits are accepted,
and rejects unknown bits unilaterally regardless of what the calling
header's mask declares. The mask documents which bits the published v1
header treats as unassigned, useful for downstream tooling that wants to
display "all flag bits unassigned in this libqore version" without
hardcoding the value.

When `collect_all` is true, the validator walks the entire descriptor
and reports every violation it finds as a single chained exception
whose `arg` is a `list<hash<PluginValidationViolation>>` listing each
issue with `(error_code, module_name, related_module_name, type_name,
related_type_name, operation_name, related_operation_name, field_name, expected,
actual, section_ref, subreason)`. The related-name fields are `NOTHING` unless a
diagnostic compares the descriptor to another module/type/operation. This is the
mode plugin author CI scripts and IDE integrations should use; production module
load uses the fail-fast equivalent (`collect_all == false`) which returns the
first violation only.

**Reserved subreasons.** `subreason` is a stable identifier, not a free-form
message. Core libqore-defined subreasons are snake_case ASCII with no version
suffix and never embed runtime data; comparison entities (other module/type/
operation names) live in `related_*` structured fields, not in the subreason
string. Extension validation subreasons use the colon-separated
`extension:<id>:<reason>` namespace because they are owned by the extension
ABI rather than libqore, and the extension is responsible for keeping its own
`<reason>` portion stable. The dual pattern is intentional: test harnesses
match core subreasons by exact equality and extension subreasons by parsing
the colon-separated form. Colons separate the namespace fields rather than
dots so the format remains parseable when extension ids themselves contain
dots but never colons.

The core-subreason naming convention is:

- `null_<field>` covers only the null-pointer case for that field; an
  empty-but-non-null string is a separate failure if the field would
  accept "exists but invalid" semantics.
- `<field>_missing` is intentionally broader, covering both null and
  empty string values, or a required non-string field that is absent as a
  single subreason. Use it when finer distinctions add no diagnostic value
  for the field. Currently `module_path_missing` and `module_handle_missing`
  follow this form.

The v1 protocol defines these values; future revisions append without
renaming or removing:

| Subreason | Used by |
|---|---|
| `unknown_validation_context_flag` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `validation_context_too_small` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_registration_context` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `registration_context_too_small` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `registration_context_size_mismatch` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `module_path_missing` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` (covers both null pointer and empty string; intentionally broader than the `null_<field>` family so a single subreason matches both invalid forms of `module_path`) |
| `negative_count` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `count_out_of_range` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `non_contiguous_local_id` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `duplicate_local_id` | `PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID` |
| `invalid_count` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_array` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_registration_descriptor` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` (`reg == nullptr`; distinct from `null_registration_context` which is `ctx == nullptr`) |
| `null_module_name` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_plugin_abi_version` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `unsupported_plugin_abi_version` | `PLUGIN-REGISTRATION-OPERATION-SET-VERSION-INCOMPATIBLE` |
| `null_operation_set_version` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_type_name` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_operation_name` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_type_info` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `null_lifecycle_callback` | `PLUGIN-REGISTRATION-NULL-LIFECYCLE` |
| `null_codec` | `PLUGIN-REGISTRATION-NULL-CODEC` |
| `invalid_signature_shape` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `unknown_helper_abi` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `unknown_value_access` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `unknown_result_alias` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `unknown_type_promotion_kind` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `invalid_extension_id` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `extension_unrecognized_required` | `PLUGIN-EXTENSION-UNRECOGNIZED-REQUIRED` |
| `invalid_dependency` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `duplicate_signature` | `PLUGIN-REGISTRATION-SIGNATURE-CONFLICT` |
| `reserved_field_nonzero` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR`, `QORD-PLUGIN-RESERVED-NONZERO` |
| `over_read` | `QORD-PLUGIN-PAYLOAD-LENGTH-MISMATCH` |
| `under_consumed` | `QORD-PLUGIN-PAYLOAD-LENGTH-MISMATCH` |
| `unsupported_canonical_version` | `QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED` |
| `signature_hash_mismatch` | `QORD-PLUGIN-SIGNATURE-HASH-MISMATCH` |
| `helper_symbol_not_found` | `PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING` during registration/contextual validation or runtime dispatch if a committed operation has no resolved helper pointer |
| `helper_abi_mismatch` | `PLUGIN-HELPER-ABI-MISMATCH` when an IR opcode/runtime helper is paired with a registered operation using a different helper ABI or when a runtime helper receives an invalid call-frame shape |
| `result_type_mismatch` | `PLUGIN-HELPER-RESULT-TYPE-MISMATCH` when a runtime helper returns a value that does not match the operation's declared `return_type` |
| `alias_contract_violation` | `PLUGIN-HELPER-ALIAS-CONTRACT-VIOLATED` when a runtime helper declared as `ReturnsLhs` or `ReturnsRhs` returns different value bits |
| `operation_not_registered` | `PLUGIN-HELPER-ABI-MISMATCH`, `PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED` when a process-local or module-local operation reference does not resolve to a committed operation |
| `program_not_available` | `PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE` |
| `module_handle_missing` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` (null module handle pointer; uses the `<field>_missing` convention from the preamble) |
| `module_handle_stale` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` |
| `registration_already_pending` | `PLUGIN-REGISTRATION-INVALID-DESCRIPTOR` (second `qore_register_plugin_types_v1` call within one module-init transaction) |
| `wait_cycle` | `PLUGIN-REGISTRATION-WAIT-CYCLE`; `related_module_name` is the byte-exact module name of the other transaction completing the wait-for cycle |
| `pending_conflict` | `PLUGIN-CROSS-TYPE-CONFLICT` from `qore_validate_plugin_types_v1` when a conflict is detected against a still-pending registration; `related_module_name` is the byte-exact name of the pending module |
| `duplicate_committed_module` | `PLUGIN-CROSS-TYPE-CONFLICT` when a module-init transaction tries to commit a plugin registration whose module name is already committed |
| `module_pending` | `QORD-PLUGIN-IMPORT-MISSING` when `PLUGIN_IMPORTS` references a still-pending module-init transaction; `related_module_name` is the byte-exact name of the pending module |
| `module_not_loaded` | `QORD-PLUGIN-IMPORT-MISSING`, `PLUGIN-HELPER-ABI-MISMATCH`, `PLUGIN-REGISTRY-MODULE-NOT-LOADED` when a plugin import, runtime dispatch, or registry lookup references a module name that is not loaded in the process and is not pending, or whose previous load failed or rolled back; `related_module_name` is the byte-exact name of the missing module when available |
| `extension:<extension_id>:<reason>` | `PLUGIN-EXTENSION-VALIDATION-FAILED`. Format uses colon separators (not dots) because canonical extension ids contain dots — `qore.plugin.llvm.codegen` → `extension:qore.plugin.llvm.codegen:unsupported_target`. Parsers split on the first and last `:`; `extension_id` is byte-exact equal to the registered `QorePluginExtension::extension_id` and therefore cannot itself contain `:`; `reason` is ASCII snake_case owned by the extension ABI. The literal `extension` prefix is reserved and never used as a core subreason. |

Implementations MUST NOT emit subreason strings outside this table for
the corresponding error codes, except for the explicitly patterned extension
namespace above. New core rejection causes either reuse an existing subreason
(when semantically equivalent) or are added to the table in a follow-up to this
document; the discipline mirrors IETF "assigned numbers" registries.

Validation is split into **descriptor-local** and **contextual** checks.
Descriptor-local checks (ids, null pointers, signatures, extension payload
shape, codec presence, declared QDOM domains) run without `ctx`. Contextual
checks need live runtime state: resolving `runtime_helper_symbol` from a module
handle, validating Program activation/import visibility, and checking
cross-module conflicts against already registered modules. A non-null
`module_handle` is trusted only if it is a `QorePluginModuleHandle` issued by
the runtime for the current `QoreModuleInitContext::plugin_module_handle` and
passed through `QorePluginRegistrationContextV1` or
`QorePluginValidationContext`; arbitrary pointers are invalid and fail
contextual validation. A dry-run with
`ctx == nullptr` cannot promise those contextual checks will pass; collect-all
mode reports them as deferred checks instead of pretending the descriptor is
fully registrable.

Cross-module-conflict contextual validation considers both committed
registrations and still-pending ones held by other module-init transactions;
the validator reads them under the registry read lock. The validator does not
wait on pending transactions because dry-run validation can be invoked from
any thread, including ones that would form wait cycles. When a conflict is
detected against a pending registration, the validator reports
`PLUGIN-CROSS-TYPE-CONFLICT` with subreason
`pending_conflict` and the pending module in `related_module_name`; against a
committed registration the subreason is `duplicate_committed_module` and
`related_module_name` names the committed module when the reporting path can
carry structured related names. Plugin authors thus see during the dry run
whether a conflicting pending registration may still resolve in their favour
(the other module's init might fail) or has already committed and locked them
out.

`qore_register_plugin_types_v1` itself runs in fail-fast mode and never
collects — registration is a single module-init transaction, not a diagnostic
walk. It validates the supplied `QorePluginRegistrationContextV1` against the
runtime's current module-init state, resolves helper symbols against the module
handle, waits/retries around conflicting pending registrations as described in
§3.10, and stages a pending registration record. Global ids become committed
only when the surrounding module init succeeds. It does not perform Program
activation; Program-local visibility is updated later when the module is added
to a Program or when QORD `PLUGIN_IMPORTS` activates the module for the loading
Program.

**Diagnostic env-var family.** Matches the existing `QORE_AOT_*_TRACE`
discipline. Trace variables only log; they never enable additional checks or
alter program semantics. None are gated by build mode, because authors debug
against the same binary their users run.

| Env var | Effect |
|---|---|
| `QORE_PLUGIN_REGISTER_TRACE` | Logs every type/operation/extension as it is accepted or rejected during `qore_register_plugin_types_v1`, with the field-by-field validation outcome. |
| `QORE_PLUGIN_DISPATCH_TRACE` | Logs every plugin-opcode dispatch (`PluginUnary` / `PluginBinary` / `PluginCall` / etc.) including operand types, resolved global operation id, helper-ABI, and resolved helper pointer. |
| `QORE_PLUGIN_VERIFY` | Enables runtime verifier checks that are normally debug-build-only in a release build; intended for production reproductions and plugin-author CI. |
| `QORE_PLUGIN_VERIFY_TRACE` | Logs every runtime verifier check that is enabled by a debug build or `QORE_PLUGIN_VERIFY`, including the ones that pass. |
| `QORE_PLUGIN_CROSS_TYPE_TRACE` | Logs cross-module operator resolution decisions per §3.10 (which precedence rule fired, what was synthesised, what was rejected). |
| `QORE_PLUGIN_QORD_TRACE` | Logs QORD `PLUGIN_IMPORTS` / `PLUGIN_HELPER_REFS` resolution: per-import resolution outcome, canonical-signature-version check, signature-hash compare, slot-table population. |
| `QORE_PLUGIN_FALLBACK_BUFFER` | Capacity of each Program's rolling plugin-fallback-site buffer. Unset = 1024, `0` disables recording (in which case `getRecentFallbackSites()` returns an empty list and `clearFallbackSites()` is a no-op), invalid values fall back to 1024 with a register-trace warning, and values above 65536 are capped. The 65536 cap is chosen so even verbose entries (source-location string + reason + operand type names ≈ 500 bytes/entry) keep the fallback list under ~32 MiB per Program. |

**Runtime verifier inventory.** A single, normative list of checks that debug
builds always perform and release builds perform only when
`QORE_PLUGIN_VERIFY=1`:

- Every plugin-helper call site asserts the returned NaN-boxed value's
  tag matches the declared `signature.return_type` (§3.4). Mismatch
  raises `PLUGIN-HELPER-RESULT-TYPE-MISMATCH`.
- Every plugin-helper call site asserts `signature.helper_abi` matches
  the trampoline used to invoke the helper (§3.5). Mismatch raises
  `PLUGIN-HELPER-ABI-MISMATCH`.
- Every operation declaring `result_alias == ReturnsLhs` or
  `ReturnsRhs` is checked: the returned bits MUST equal the named
  operand's bits exactly. Mismatch raises
  `PLUGIN-HELPER-ALIAS-CONTRACT-VIOLATED` (§3.3).
- `value_ops.incref` / `decref` / `cleanup_slot` are called through
  `noexcept` function-pointer types. Registration validates that the pointers
  are non-null, but the runtime cannot recover if C++ code throws through one:
  normal C++ `noexcept` rules apply (`std::terminate`), and the protocol treats
  this as a plugin bug rather than a recoverable Qore exception. Debug builds
  and `QORE_PLUGIN_VERIFY=1` runs set a thread-local "current plugin lifecycle
  call" record before invoking each hook; libqore's terminate handler uses that
  record to log the offending plugin module, type, hook name, and operation
  context when termination is caused by an exception escaping a `noexcept`
  hook. This is diagnostic context, not recovery. Release builds without
  `QORE_PLUGIN_VERIFY` may omit the thread-local record and inherit raw C++
  `noexcept` semantics with no Qore-side log.
- Every `make_identity` callback is invoked at most once per
  `(operation, result_type)` pair; debug builds assert the cache
  invariant on every fold attempt (§3.4).

`QORE_PLUGIN_VERIFY_TRACE` only logs verifier decisions. It does not turn a
release build verifier on; use `QORE_PLUGIN_VERIFY=1` for that.

**Reflection from Qore code.** Phase 3 ships `Qore::Reflection::PluginRegistry`
(accessible from any Program that has `reflection` loaded). The API exposes both
the process-global immutable registry and a Program-bound view, because §3.10
keeps plugin activation per Program:

```qore
namespace Qore::Reflection;

class PluginRegistry {
    # Program-bound view for the current Program.
    static PluginRegistry get();

    # Program-bound view for an explicit Program.
    static PluginRegistry get(Qore::Program pgm);

    # Process-global descriptor registry; methods that depend on Program
    # activation or parse diagnostics raise PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE
    # on this view (see error-code table).
    static PluginRegistry getProcessRegistry();

    # Process-global: every module whose binary is loaded and whose
    # registration has run — regardless of which Programs activate it.
    list<string> getProcessModules();

    # Program-bound: every module activated in this view's Program.
    # Always a subset of getProcessModules(). Raises
    # PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE on the process-global view.
    list<string> getActiveModules();

    # Both views: raise PLUGIN-REGISTRY-MODULE-NOT-LOADED when module_name
    # is not loaded in the process. Process-global view: descriptors for
    # the named loaded module regardless of activation. Program-bound view:
    # descriptors for the named loaded module only if active in this
    # Program; loaded-but-inactive modules return an empty list (distinct
    # from the missing-module case so a typo'd name fails loudly while
    # an inactive-but-loaded name reports the unambiguous empty result).
    list<hash<PluginTypeInfo>> getTypes(string module_name);
    list<hash<PluginOperationInfo>> getOperations(string module_name);

    # Answers "why didn't my expression lower to my fast path?". Returns
    # the operation that would be selected for (lhs, rhs, op) at this
    # call site, or NOTHING if it would fall back to .any. Program-bound;
    # raises on the process-global view.
    *hash<PluginOperationInfo> resolveOperation(
        Type lhs, Type rhs, string operation_name);

    # Plugin-type call sites that fell back to .any in this Program. The
    # buffer is rolling, sized by QORE_PLUGIN_FALLBACK_BUFFER (default
    # 1024); oldest entries are dropped on overflow. Each entry records
    # source location + reason. Program-bound; raises on process-global.
    list<hash<PluginFallbackSite>> getRecentFallbackSites();

    # Reset the rolling fallback buffer for this Program. Useful for
    # bisecting which file/parse step introduced a regression. Program-
    # bound; raises on process-global.
    nothing clearFallbackSites();
}
```

`PluginRegistry` is the protocol's answer to the "type registered, but
where is it?" debugging question. The process-global view (`getProcessRegistry`)
sees every loaded plugin module's *descriptors*; `getTypes()` and
`getOperations()` on that view return all descriptors for the named loaded
module regardless of Program activation. The Program-bound view (`get`) sees
the *activations* visible to a specific Program plus parse-time diagnostics;
`getTypes()` and `getOperations()` on that view filter to active modules and
return an empty list for a loaded-but-inactive module. Both views raise
`PLUGIN-REGISTRY-MODULE-NOT-LOADED` when a module name is not loaded in the
process at all, so a typo or stale module reference is a loud failure
distinct from the deliberate "loaded but not active here" empty result.
Calling a Program-bound method on a process-global view raises
`PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE` with a diagnostic naming the method
and pointing the caller at `get()` / `get(pgm)`. The registry is not a
substitute for the env-var trace family — the registry is a snapshot of
the steady-state, the traces are a record of decisions. Both are necessary.

**Reference module + lint.** Phase 3 ships:

- `examples/plugins/sample-buffer/` — a minimal but complete
  registered plugin type with all five lifecycle ops, a binary `add`
  operation, a dense-buffer `dense_add_i64` operation, a serializer, and a
  lowering hook. Plugin authors copy this and edit. The optional LLVM
  extension remains reserved until the plugin LLVM ABI header exists.
- `examples/plugins/qore-plugin-lint` — a script (Qore source) that
  loads a built `.qmod`, reads committed descriptors through
  `PluginRegistry`, then reports violations and warnings about descriptor
  quality (e.g., missing runtime-helper symbols, non-contiguous local ids, or
  dense-buffer operations that are not marked vectorizable). The lint is
  advisory; it never blocks a build by default but is the recommended
  pre-commit check for plugin modules. The C dry-run validator remains the
  correct API for module-local self-tests that can see the descriptor table
  before module init commits it.

**Error-message quality contract.** Every diagnostic raised by registration,
validation, QORD load, parse-time lowering, the runtime verifier, or
`PluginRegistry` MUST include, at minimum:

1. The error code from the table above.
2. The offending module name when one is available, or the reflection method /
   runtime context when no module is involved.
3. The offending type and/or operation name (when the violation is
   localised to one), or the source location / API method for parse-time and
   reflection diagnostics.
4. The descriptor field, wire-format field, API method, or verifier check that
   violated the rule.
5. Expected value vs. actual value when the failure is a comparison.
6. The structured `subreason` when the error code covers multiple field-level
   causes.
7. A reference to the section of this document defining the rule
   (e.g., "see plugin-types-and-dense-data.md §3.3 ‘Operation-name
   table'").

Diagnostics that omit any of these MUST be treated as a libqore bug and
filed accordingly. The intent is that a plugin author hitting any
rejection during development can fix it without reading the libqore
source.

---

## 4. Worked example: `buffer<T>`

`buffer<T>` is the canonical first plugin type. It validates the protocol
end-to-end before exposure to external modules.

**Type-system carrier: special built-in complex type.** `buffer<T>` ships as
a special built-in complex type (alongside `list<T>` / `hash<string, T>`)
with explicit `QoreTypeInfo` interning, equality, reflection, and QORD
serialization rules. Generic class support is already implemented, but
`buffer<T>` deliberately does not depend on representing dense primitive
storage as a parameterized class: the special-built-in representation avoids
boxing, keeps storage element widths explicit, and lets Phase 1 validate the
low-level type/IR/AOT path directly. Future plugin-facing APIs can still reuse
the implemented generic metadata and reflection substrate where useful; the
user-visible syntax (`buffer<int64>`, `foreach int v in (b)`) remains the same.

### 4.1 Element type set

Day 1: `int8`, `int16`, `int32`, `int64`, `float32`, `float64`, `bool`
(packed bitmap).

Deferred: `date` (int64 epoch microseconds), `string` (Arrow-style packed
bytes + offsets, immutable-by-element), `decimal` (variable-width — fits
poorly into a dense buffer; revisit).

**Narrow and width-specific names are storage-only**, not scalar types. A
`buffer<int8>` exists; `int8 x = 5` does not. `float32` / `float64` likewise
name storage layouts, while scalar code uses Qore `float`. Reading `b[i]` from
a `buffer<int8>` widens to `int` (64-bit) on the way out; reading from
`buffer<float32>` widens to `float`. Writing `b[i] = v` range-checks or
precision-converts on assignment. This avoids C-style integer-promotion rules
in scalar code while preserving width fidelity in dense storage. NumPy follows
this exact split.

### 4.2 Syntax

```qore
# Declaration + construction. Named constructors avoid the
# buffer<int64>(5) ambiguity (zero-filled length-5 vs. one-element value-5);
# the only positional overload accepted is the from-list one, since list
# vs. integer is statically distinguishable.
buffer<int64>   ids    = buffer<int64>::sized(1000);                # zero-init, length 1000
buffer<int64>   tens   = buffer<int64>::filled(1000, 10);           # value 10, length 1000
buffer<float64> p      = buffer<float64>((1.5, 2.5, 3.5));          # from list
buffer<*float64> q     = buffer<*float64>((1.5, NOTHING, 3.5));     # per-element nullable

# Subscript: unboxed storage read/write; narrow/wide storage integers widen to Qore int
int v = ids[42];
ids[42] = 100;

# Length, slicing — match list semantics (copy by default)
int n = ids.size();
buffer<int64> sub = ids[10..19];
buffer<int64> view = ids.view(10, 10);   # start, count; zero-copy view

# Typed iteration — loop variable is the scalar Qore type for T
int total = 0;
foreach int v in (ids) {
    total += v;
}

# Conversions
list<int> l = ids.toList();                        # explicit boxing
buffer<int64> b2 = buffer<int64>(l);               # explicit unboxing (from-list overload)

# Arithmetic (elementwise, SIMD-backed via Eigen or hand-written)
buffer<float64> c    = a + b;
buffer<float64> d    = a * 2.0;                    # broadcast scalar
float           mean = a.mean();
buffer<bool>    mask = a > 0.0;                    # predicate → bitmap

# Reductions
float s = a.sum();
float m = a.min();
float x = a.max();
int   c = a.count();   # excludes nulls if buffer<*T>
```

### 4.3 Storage model and null handling

`buffer<T>` is non-nullable at the element level — every position has a
valid `T` value. `buffer<*T>` is per-element nullable, carrying a validity
bitmap alongside the data buffer. Reading `b[i]` on a nullable buffer
returns `*T` (`NOTHING` for null). Non-nullable buffers skip the bitmap
entirely — no memory cost for the common case.

**Validity bitmap layout — Arrow-compatible.** When present, the bitmap
follows Apache Arrow's specification verbatim: 1 bit per element, **LSB
ordered within each byte** (bit `i` of byte `i/8` represents element `i`),
**1 = valid / 0 = null**, padded to a multiple of 8 bytes (64 bits) with
trailing bits indeterminate beyond the logical length, **64-byte aligned
allocation**. A cached null-count is stored alongside the bitmap header,
populated lazily; readers must treat `null_count == -1` as "not yet
computed". This layout is chosen so a `buffer<*T>` can be handed to Arrow
C Data Interface consumers (and accepted from Arrow producers) without
copying or remapping bits.

`buffer<bool>` stores values as a packed bitmap. `buffer<*bool>` stores two
bitmaps: one bitmap for values and one Arrow-compatible validity bitmap. A
tri-state single-bitmap encoding is deliberately avoided because it would not
round-trip through Arrow's value-buffer + validity-buffer model without
translation.

`*buffer<T>` (note position) is the whole buffer being optional — the
buffer itself is `NOTHING` or a buffer. Distinguishes from `buffer<*T>`
(buffer of optional elements).

Reference semantics: buffers are reference-counted; not copied on
assignment. `clone()` for explicit deep copy. Mutating a shared buffer uses
copy-on-write unless the value is an explicit view; mutating a view mutates the
referenced storage. Buffer storage is immutable across threads unless the caller
has exclusive ownership or uses a synchronization primitive supplied by the
owning module.

### 4.4 Slice and view semantics

`b[a..c]` returns a copy by default and uses the same inclusive range semantics
as existing list slices. `b.view(start, count)` returns a zero-copy view sharing
storage; the view holds a reference to the source buffer to keep storage alive.
Mutating a view mutates the source (analogous to NumPy slicing). A view with
negative `count`, an out-of-range start, or `start + count` beyond the source
size raises a range exception.

### 4.5 Operators and reductions

Operators (`+`, `-`, `*`, `/`, comparisons) are registered via the
plugin operation-descriptor protocol with full declarative metadata
(`is_commutative` for `+`/`*`, `is_associative` for both, identity
elements, type-promotion lattice). The IR optimizer can therefore reorder,
fuse, hoist, and vectorize.

Reductions (`sum`, `mean`, `min`, `max`, `count`, `any`, `all`) are method
calls that lower to dedicated reduction opcodes — analogous to the existing
`FoldlSumInt` / `FoldlMaxFloat` family.

### 4.6 Interop with hashdecls and `DataFrame`

A typed hashdecl can carry buffer-typed members. Because `buffer<*string>`
depends on Arrow-style packed-bytes-plus-offsets storage that Phase 1
defers (see §5.1 and Open Question 3), Phase 1 examples use `list<*string>`
for the string-bearing column; once the string buffer lands, callers can
substitute `buffer<*string>` without touching consumer code that uses the
hashdecl by name:

```qore
hashdecl UsersResult {
    buffer<int64>    id;
    list<*string>    name;          # buffer<*string> when string buffers ship
    list<date>       created_at;    # buffer<date> when date buffers ship
}
```

This becomes the natural carrier for typed query results from DBI drivers
(see §5). The unreleased `dataframe` module should be changed to expose
DataFrame as a native data type if that produces cleaner value semantics than
the current class/private-data representation. In either representation,
`DataFrame::getColumn(name)` returns `buffer<T>` directly — no boxing.
`DataFrame::toMatrix()` returns `list<buffer<float64>>`. The ML modules accept
`buffer<float64>` and receive a `const double*` + size for zero-copy hand-off to
Eigen / Arrow / ONNX Runtime.

### 4.7 Pipeline propagation

`AbstractDataProcessor` gains shape negotiation above the current bulk boolean:

```qore
enum DataProviderShape {
    RowRecords,
    HashOfLists,
    BufferColumns,
    DataFrameBlock,
}

class AbstractDataProcessor {
    private list<DataProviderShape> supportedInputShapesImpl();
    private list<DataProviderShape> supportedOutputShapesImpl();
    *auto processRecordsShape(auto records, DataProviderShape shape);
}
```

For `BufferColumns`, `hash<auto>` carries `buffer<T>` values. For
`DataFrameBlock`, the value is a DataFrame native type. `DataProviderPipeline`
auto-routes by choosing the highest common shape between adjacent stages, with
a conversion cost model for row records, hash-of-lists, buffer columns, and
DataFrame blocks. Sticky-typed-form bias keeps data in the densest compatible
shape across chains. Conversion at boundaries follows the rules from §3.

---

## 5. Worked example: typed driver hashdecls

Closing the SQL → Qore type fidelity gap requires DBI drivers to lift
schema metadata into the runtime value's *type*, not just into a side
channel.

### 5.1 SQL → Qore type mapping

| SQL type | Qore type |
|---|---|
| `SMALLINT` | `buffer<int16>` |
| `INTEGER` | `buffer<int32>` |
| `BIGINT` | `buffer<int64>` |
| `REAL` | `buffer<float32>` |
| `DOUBLE PRECISION` | `buffer<float64>` |
| `NUMERIC(p, s)` | `list<number>` short term; `buffer<decimal128>` recommended target — see Open Question 4 |
| `BOOLEAN` | `buffer<bool>` |
| `VARCHAR`, `TEXT` | `list<*string>` (Arrow-style buffer deferred) |
| `DATE`, `TIMESTAMP` | `list<date>` short term; `buffer<date>` after date-buffer support ships |
| `BYTEA` / `BLOB` | `list<binary>` |
| `JSON` / `JSONB` | `list<auto>` |
| `array<T>` | `list<list<T>>` (nested buffers deferred) |
| `geometry` | `list<auto>` (driver-specific decoder) |

Nullable columns use `buffer<*T>` (per-element validity bitmap) where
defined; non-nullable columns use `buffer<T>`.

### 5.2 Driver-side lifting

Each DBI driver gains an option (per-connection or per-query) to return
typed hashdecls. Default behaviour is unchanged — `hash<auto>` /
hash-of-lists return values for backward compatibility. Opt-in via:

```qore
hash<auto> result = ds.selectTyped("SELECT id, name, created_at FROM users");
```

The driver:
1. Reads SQL column metadata (already available from prepare/describe).
2. Mints (or looks up in cache) a hashdecl whose member types are the
   buffer types per the mapping table. The cache key includes driver name,
   datasource identity where needed, normalized SQL result metadata, nullability,
   column order, and the plugin ABI version.
3. Allocates buffers sized to the result row count.
4. Fills buffers by direct typed copy from native storage — no per-value
   boxing.
5. Returns `hash<MintedHashdecl>` with full type identity.

For statically declared schemas, user code may still assign to an explicit
hashdecl such as `hash<UsersResult>` when the minted result shape is compatible.
For ad hoc SQL, the minted hashdecl follows this model:

- **Anonymous, never namespace-visible.** Minted hashdecls are not
  registered in any program namespace; they cannot be referenced by name in
  parser-visible source. User code interacts with them only via `hash<auto>`
  binding plus reflective member access (`result.id`, `result{"id"}`).
- **Cache key.** Per-Datasource cache, keyed by the tuple
  `(driver_name, normalized_result_signature, plugin_abi_version)` where
  `normalized_result_signature` is the canonical (column-name, qore-type,
  nullability, ordinal) sequence. Identical SQL against the same Datasource
  returns the same cached hashdecl pointer; structurally identical results
  across different Datasources of the same driver share a hashdecl.
- **Lifetime.** Hashdecl entries live until the owning Datasource is
  destroyed; on Datasource close the cache drops its references and any
  hashdecl whose only remaining holders are in-flight result objects is
  freed when those objects go away. There is no global TTL.
- **Reflection name.** A deterministic, generated synthetic name of the
  form `__minted_<driver>_<8-hex-hash-of-signature>` — sufficient for
  reflective debugging and error messages, intentionally not parseable as
  user-visible source.
- **Collision handling.** Structural identity by canonical form; two
  result sets with the same normalized signature share the same hashdecl
  pointer regardless of the SQL that produced them. No collision is
  possible by construction.
- **AOT serialization.** Minted hashdecls are an inherently *runtime*
  concept tied to a live Datasource. They are **not** AOT-serialized:
  AOT-compiled code that calls `selectTyped` receives a freshly-minted
  (or cache-hit) hashdecl at runtime, and the runtime hashdecl pointer
  flows through the existing `QoreAOTContext` slot-table mechanism. A QORD
  artifact that statically references a minted hashdecl name fails to
  load — the synthetic name is intentionally not portable across runs.

### 5.3 Consumer-side benefits

- `DataFrame(typed_hash)` is a cheap, lossless one-liner; no type
  inference, no schema-sniffing.
- With an explicit source-visible hashdecl such as `hash<UsersResult>`,
  `result.id` returns `buffer<int64>` with full QLS / static-type-checking
  support.
- With ad hoc `hash<auto> result = ds.selectTyped(...)`, callers still get
  runtime typed buffers and reflective field metadata, but QLS/static checking
  cannot infer `result.id` without a declared or generated schema visible to
  the parser.
- Round-trips to Parquet / Arrow / Kafka preserve column widths and
  precision exactly.
- Pipelines that propagate typed hashdecls through bulk stages keep type
  metadata end-to-end.

---

## 6. Pipeline `DataFrame` propagation

A pipeline can preserve full typed-column representation across stages
when both stages support it, falling back to hash-of-lists at boundaries
where the next stage doesn't.

### 6.1 Capability advertise + auto-convert

Extends the existing bulk-API pattern into explicit shape negotiation:

```qore
class AbstractDataProcessor {
    private list<DataProviderShape> supportedInputShapesImpl();
    private list<DataProviderShape> supportedOutputShapesImpl();
    *auto processRecordsShape(auto records, DataProviderShape shape);
}
```

`DataProviderPipeline::submit` auto-routes:
- If the source emits DataFrame and the next stage supports DataFrameBlock →
  propagate as DataFrame, no conversion.
- If the source emits DataFrame but the next stage only supports HashOfLists →
  convert via `df.toColumnHash()` at the boundary.
- If the source emits HashOfLists and the next stage supports DataFrameBlock
  → optionally lift via `DataFrame(record_hash)` or a native constructor
  (sticky-typed-form bias).
- If the source emits BufferColumns and the next stage supports DataFrameBlock
  → construct a zero-copy DataFrame view where ownership and mutability rules
  permit it.

### 6.2 Boundary semantics

Conversion `DataFrame → hash<auto>`:
- Lossless in shape (each typed column → list).
- Lossy in storage representation (typed columns → boxed lists), but the
  type metadata travels with the typed hashdecl variant when applicable.

Conversion `hash<auto> → DataFrame`:
- Requires either typed-hashdecl input (lossless) or schema sniff
  (lossy on width).
- Schema-sniff path emits a parse warning when a wider DataFrame stage is
  in the chain — encourages migration to typed-hashdecl sources.

### 6.3 Sticky-typed-form bias

Once data enters DataFrame form, the pipeline tries to keep it there as
long as *any* downstream stage supports DataFrame. Avoids
DataFrame → bulk → DataFrame thrash, which would lose all the conversion
savings.

Current stock processors must be classified honestly before conversion rules
are added. `QoreFilterRecordsProcessor` is a good early candidate for dense
predicate support. `AbstractAnalyticsProcessor` is bulk-aware today but still
converts through row records internally, so it needs a separate vectorized path.
`QoreGroupByProcessor` currently does not support the bulk API and should not
be listed as a dense-pipeline participant until it is rewritten for grouped
column blocks.

Implementation status:
- `DataProviderShape` is implemented in `DataProvider` with `RowRecords`,
  `HashOfLists`, reserved `BufferColumns`, and `DataFrameBlock`.
- `DataProviderPipeline` detects shaped values, preserves row-record
  `submitImpl()` output boundaries, keeps DataFrame blocks when a processor
  supports them, and converts DataFrame blocks to hash-of-lists or row records
  at unsupported boundaries.
- `QoreFilterRecordsProcessor` supports `DataFrameBlock` directly for simple
  comparison predicates and `&&` conjunctions. Unsupported expressions fall
  back to row evaluation and rebuild a DataFrame.
- `DataProviderDataFrame` provides DataFrame-backed bulk reads by slicing
  DataFrame blocks and returning hash-of-lists blocks.
- `BufferColumns` currently uses the hash-of-lists carrier as an API placeholder
  until dense buffer columns are implemented in Phase 7.

---

## 7. Phased rollout plan

The proposal admits incremental delivery. Each phase ships independently
useful functionality without committing to later phases.

Phase sizes below are coarse t-shirt estimates relative to each other —
**S** ≈ targeted refactor / well-scoped subsystem, **M** ≈ multi-subsystem
work, **L** ≈ multi-subsystem with new wire-format and API surface. Real
calendar time depends on what other branch work is in flight.

### Phase 0 — implementation-gap cleanup (S)

- [x] Centralize builtin opcode property consumers behind `OpcodeInfo` where
  practical, or document every remaining switch that intentionally stays local.
- [x] Add conservative declarative-property fields to `OpcodeInfo` and populate
  them for the existing builtin opcodes.
- [x] Add tests for metadata-driven verifier decisions before adding optimizer
  rewrites.
- [x] Refactor or explicitly fence `QoreValue` tag classification so plugin
  immediate tags cannot collide with float/int/pointer/special predicates.
- [x] Add extensible `TypeProfile` keys for non-builtin types alongside the
  legacy builtin-only `dominantType()` result, carrying `(kind, payload)`
  where `kind ∈ {builtin_int, builtin_float,
  builtin_string, builtin_bool, builtin_nothing, builtin_other,
  qore_class, plugin_type}` and `payload` is the interned `QoreTypeInfo*`
  for user-class cases and the stable `(module_name, local_type_id,
  type_info)` tuple for plugin-type cases. Builtin keys keep the existing
  fast-path and atomic-counter layout; plugin-typed observations populate
  a per-guard concurrent hash map (using libqore's existing
  reader/writer-locked hash pattern, not a new TBB-style primitive)
  allocated lazily on first non-builtin observation.
- [x] Before Phase 1 starts, explicitly confirm that `int8`, `int16`, `int32`,
  `int64`, `float32`, and `float64` are storage element type names for
  `buffer<T>`, not general scalar local-variable types. Reading a narrow
  integer element widens to Qore `int`; reading a narrow floating element
  widens to Qore `float`.

Ships independently useful cleanup even before plugin types exist. Optimizer
passes that rely on new algebraic metadata should wait until the metadata has
tests and conservative defaults.

### Phase 1 — `buffer<T>` in libqore (L)

- Add `buffer<T>` to libqore as a special built-in complex type with
  explicit `QoreTypeInfo` interning and QORD serialization (decision per
  §4 intro — does not depend on `generic-class-types`).
- Element types: `int8`, `int16`, `int32`, `int64`, `float32`, `float64`,
  `bool`.
- Elementwise arithmetic/comparison operators, reductions, static factories,
  slicing, zero-copy views, iteration, and conversion to/from `list<T>`.
- Ensure `buffer<T>` is reachable through whichever name-based type lookup
  the `reflection` binary module exposes for non-class types, and through
  the `*Type` family in `Qore::Reflection`. Do not route native non-class
  types through `Class::forName`. Phase 1 also adds a Qore-side reflection
  accessor for the canonical type path (working name
  `Qore::Reflection::Type::getPathName()`; implementation maps to
  `qore_type_get_path()` / `QoreTypeInfo::getPath()`). This mirrors the
  existing `Class::getPathName()` — same semantics for class types
  (returns `Foo::MyClass`), extends naturally to non-class types
  (`buffer<int64>`, `hash<string, int>`, etc.), and is the canonical
  identity used by QORD and plugin signature hashing. `Type::getName()`
  remains the display/simple name and is never used for QORD identity or
  plugin signature hashing. The exact reflection-API name can change at
  implementation time, but the requirement is that plugin types not be
  invisible to Qore-side metaprogramming and QLS, and that Qore code can see
  the same canonical type identity used by AOT.
- ABI not yet exposed externally — plugin-type registration still internal.

Validates the protocol shape end-to-end through IR interpreter, JIT, AOT.
Closes the dense-buffer gap. Unblocks DataFrame zero-copy improvements
even before the plugin protocol opens externally.

### Phase 2 — typed hashdecls from drivers (M)

- Driver-side lifting helper in libqore.
- `Datasource::selectTyped` / `selectRowsTyped` opt-in API.
- Postgres, MySQL, ODBC, Oracle drivers updated to support the option.

Closes the SQL → Qore type fidelity gap. Independent of plugin-type
exposure — reuses Phase 1's `buffer<T>`.

### Phase 3 — plugin-type registration C ABI (L)

- Public `QorePluginTypeRegistration` ABI.
- Registration through explicit `qore_register_plugin_types_v1(ctx, reg,
  xsink)` calls from `qore_module_init`, where `ctx` is a
  `QorePluginRegistrationContextV1` populated from the runtime-owned
  `QoreModuleInitContext::plugin_module_handle`.
- Built-in plugin-dispatch opcodes carrying operation descriptors.
- IR-interpreter dispatch table for module-registered helpers.
- JIT helper symbol resolver for module-registered helpers.
- Dense-buffer runtime helper trampolines for `DenseBufferUnary` and
  `DenseBufferBinary`, plus dedicated dense-buffer IR opcodes that evaluate
  Qore `buffer<T>` operands and safely derive raw data/size/stride frames.
- QORD `PLUGIN_IMPORTS`, `PLUGIN_TYPE_REGISTRY`, and `PLUGIN_HELPER_REFS`
  sections for plugin-dispatch IR references, plus `VT_PLUGIN_INSTANCE`
  reader/writer support for serialized plugin value-instance payloads.
- **Validation and developer experience (§3.12)**, all on the public ABI
  ship boundary:
  - `QoreModuleInitContext` extended with the runtime-owned opaque
    `plugin_module_handle`, `QorePluginRegistrationContextV1` carrying that
    handle across the public plugin ABI, and `qore_register_plugin_types_v1`
    rejecting null, wrong-size, stale, or cross-module registration contexts
    before descriptor validation.
  - `qore_validate_plugin_types_v1` dry-run validation entry point with
    `QorePluginValidationContext` (size-prefixed for forward-compatible
    field growth, fixed-width flags, and a runtime-owned opaque module
    handle), fail-fast mode, and collect-all-errors mode.
  - The structured error-code set listed in §3.12 — every rejection rule
    in §3.3 / §3.7 / §3.9 / §3.10 / §3.12 wired to its assigned code,
    including `PLUGIN-EXTENSION-VALIDATION-FAILED`,
    `PLUGIN-REGISTRY-PROGRAM-NOT-AVAILABLE`,
    `PLUGIN-REGISTRY-MODULE-NOT-LOADED`, and
    `QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED`. Core subreason strings are
    drawn from the §3.12 reserved-subreasons table only; extension validators
    may use only the documented namespaced extension pattern. Emitting an
    unlisted core subreason for a listed error code is a libqore bug.
  - The `QORE_PLUGIN_*_TRACE` env-var family (`REGISTER`, `DISPATCH`,
    `VERIFY_TRACE`, `CROSS_TYPE`, `QORD`) plus `QORE_PLUGIN_VERIFY`
    for release-build runtime verifier opt-in and
    `QORE_PLUGIN_FALLBACK_BUFFER` for Program-local fallback diagnostics,
    matching the existing `QORE_AOT_*_TRACE` logging discipline without
    making trace flags change semantics.
  - Runtime verifier checks per the §3.12 inventory (helper return-tag,
    helper-ABI, alias contract, `noexcept` lifecycle discipline with the
    debug/`QORE_PLUGIN_VERIFY` terminate-context record, `make_identity`
    cache).
  - `Qore::Reflection::PluginRegistry` for Qore-side introspection of
    registered modules / types / operations and "why didn't this
    lower?" diagnostics, with split process-global / Program-bound views
    and `clearFallbackSites` for bisecting.
  - Error-message quality contract: every diagnostic includes module/context +
    type/op/source/method + field/check + expected/actual + subreason +
    section reference where applicable.
- [x] `examples/plugins/sample-buffer/` reference module + `qore-plugin-lint`
  advisory linter.
- Documentation.

Opens the door externally. No new functionality on its own — Phase 1's
`buffer<T>` could optionally migrate to using the protocol internally, but
that's a refactor, not a feature. The dev-experience surface ships
together with the ABI because plugin authors hit registration failures
on day one and a -1 with a free-form xsink string is not a usable
diagnostic for an external module author.

### Phase 4 — optional LLVM codegen + lowering hooks (M)

- [x] Optional `QorePluginLLVM.h` / `PluginLLVMCodegenCallback` extension ABI
  and required-extension LLVM-major rejection (§3.6).
- [x] `PluginLoweringCallback` wiring in the lowering pass (the typedef itself
  was published in Phase 3's stable `QorePluginType.h`).
- [x] Default fallback (no callback supplied → emit runtime-helper call).

Plugin types can opt into native SIMD codegen and parse-time pattern
matching.

### Phase 5 — make `dataframe` a native plugin data type (M)

- [x] Because `dataframe` has not been released, change `DataFrame` from the
  current class/private-data representation to a native data type if that
  produces cleaner value, ownership, and operator semantics. The implemented
  decision is to keep the class/private-data representation as the public
  ownership model and register that class type as the plugin type; replacing
  the public object with `NT_PLUGIN_VALUE` would lose the existing method
  surface without improving ownership.
- [x] Register `DataFrame` via the plugin protocol.
- [x] Subscript: `df["col"]`, `df[1..10]`.
- [x] Comparison and predicate operators returning bitmap masks.
- [x] Lazy expression DSL through registered operators. Implemented as explicit
  `df.column("name")` `ColumnRef` values; comparisons with scalar values return
  `RowMask` objects accepted by `df[mask]` and `df.filter(mask)`. Direct
  `df["name"]` remains list-returning column access to preserve simple
  inspection semantics.
- [x] Surface `DataFrame` through the `reflection` module (per the Phase 1
  precedent for `buffer<T>`) so QLS, doxygen, and Qore-side
  metaprogramming see the new type through `PluginRegistry` metadata.

Validates the protocol on a more complex second consumer. Unlocks the
Polars / pandas-style ergonomics for DataFrame.

### Phase 6 — pipeline `DataFrame` propagation (M)

- [x] `DataProviderShape` negotiation for row records, hash-of-lists,
  buffer-columns, and DataFrame blocks.
- [x] `DataProviderPipeline` auto-routing with conversion costs.
- [x] Sticky-typed-form bias for current-stage compatibility.
- [x] Stock processors updated where worthwhile. Started with
  `QoreFilterRecordsProcessor`; vectorized analytics paths remain separate;
  `QoreGroupByProcessor` remains a rewrite candidate because it is not bulk
  aware today.

End-to-end zero-conversion typed pipelines for analytics workloads.

### Phase 7 — declarative-metadata-driven optimizations (ongoing)

- [x] Profile-guided specialization for plugin types. Guard profiles now
  retain stable `(module_name, local_type_id, type_info)` keys for
  `NT_PLUGIN_VALUE` observations and `GuardType` LLVM lowering consumes a
  plugin-dominant profile by emitting `qore_rt_guard_plugin_type_profiled()`.
  The profiled helper exact-matches the hot plugin value descriptor on the
  fast path and falls back to the normal `runtimeAcceptsValue()` semantics
  for all non-matching or non-plugin values, so broad guards such as `auto`
  and ordinary deopt behavior remain type-safe.
- [x] Row-mask predicate composition as the first lazy-expression fusion
  surface. Descriptor-registered `bit_and`, `bit_or`, `bit_xor`, and
  `bit_not` operations are now routed through Qore's `&`, `|`, `^`, and `~`
  operators when the active module supplies matching plugin operations. The
  dataframe module registers these for `RowMask`, with pure/commutative/
  associative metadata on the binary mask combinators, and exposes named
  `intersect()`, `unionWith()`, `symmetricDifference()`, and `invert()` aliases
  for readability. This lets callers compose predicates without materializing
  rows and apply one `df[mask]` / `df.filter(mask)` operation.
- Cross-stage fusion in pipelines.
- [x] **Fast-math flag for floating-point plugin operations.** Adds a
  per-operation declarative bit (working name `fp_reassociation_allowed`)
  and a corresponding Program-level parse directive (`%fp-fast-math`) that
  together enable IEEE-unsafe rewrites — vector
  reduction over `sum`/`mean`, contraction across `add`/`mul`, FMA
  formation, etc. — for ops that opt in. Plain `is_associative = true` on
  an FP op is insufficient by design (per §3.3 operation-name table) so
  this flag is the load-bearing path for vectorising FP reductions.
  Implemented as a conservative-default descriptor field plus extended
  `QoreParseOptions::FP_FAST_MATH`; reflection exposes the metadata so
  tools can verify both sides of the gate. Plugin LLVM codegen callbacks now
  receive both the operation metadata and the already-gated
  `fp_reassociation_enabled` decision in `QorePluginLLVMCodegenContext`, so
  module-owned native rewrites have the same conservative default as core
  optimizer passes. Core optimizer passes still have to call the gate before
  introducing any concrete reassociation rewrite.

Long-tail performance work, ongoing rather than discrete phase.

---

## 8. Risks and tradeoffs

### 8.1 ABI stability

Plugin-type protocol exposes operation descriptor ids, helper-function ABI,
codegen-callback shapes, value-tag allocation, and QORD section formats.
Once external modules ship against the protocol, any of these become
forever-promises.

The single largest commitment is the **LLVM codegen callback (§3.6)**: its
signature names `llvm::Value*` and `llvm::IRBuilder<>*` directly, hard-binding
plugins that opt in to libqore's exact LLVM major version. Every other
plugin-protocol surface is plain-C and version-stable across LLVM bumps; the
LLVM codegen hook is not, by design, because hiding the LLVM types behind a
shim would constrain the codegen quality the hook exists to enable.

**Mitigation:** Use the existing `qore_aot_*_v3`-style pattern.
`qore_register_plugin_types_v1` is versioned from day one; each breaking
change to the registration ABI ships as a new symbol and old binaries keep
working. Declare in writing which slots are stable contract vs.
internal-only. Start small; you can always add slots, but you can never
remove them. Treat the LLVM codegen callback as an explicit opt-in escape
hatch — document it as "rebuild on LLVM bump" rather than as a stable
contract.

**Registration-context evolution.** When the registration context grows
incompatibly, ABI evolution adds a new struct `QorePluginRegistrationContextV2`
(or higher) and a new symbol `qore_register_plugin_types_v2`. Old binaries
continue to resolve `qore_register_plugin_types_v1` against the original
fixed-size `QorePluginRegistrationContextV1` struct. The v1 entry point
requires `ctx->struct_size == sizeof(QorePluginRegistrationContextV1)`; larger
sizes are rejected instead of being treated as compatible trailing-field
growth. A module that wants to support
both vintages dispatches via `dlsym` for the highest version it knows,
falling back to the v1 path. The `_V<N>` suffix in the struct name
type-checks the version compatibility at compile time — passing a v2
context to a v1 entry point or vice versa is a hard error, not a runtime
mystery. See §3.3 for the registration-context vs. validation-context
evolution-discipline split.

**`required = true` extensions sharpen the failure mode.** A module may
declare its LLVM codegen extension `required = true` (§3.3) to refuse
loading against a libqore whose LLVM major doesn't match. This is
appropriate for SIMD-only modules where the runtime-helper fallback is
unacceptable; the module then participates fully in libqore's LLVM
upgrade discipline (rebuild required) instead of degrading silently.
Ordinary plugin types should leave `required = false` so an LLVM bump
loses inline codegen but keeps the module functional. Choosing
`required = true` is a deliberate constraint on deployment, not a
default.

### 8.2 Single-vs-multiple dispatch decision

Resolved by §3.11: the IR's typed-vs-`.any` mechanism delivers the benefit
of multiple dispatch for operators (parse-time monomorphization per
operand-type pair) while keeping methods single-dispatch. No language-level
"multimethod" feature; user model stays simple.

### 8.3 Verification of declared metadata

A plugin module that incorrectly declares `is_commutative = true` or
`is_pure_modulo_xsink = true` can cause silent miscompile under JIT
optimization. Deopt catches failed type guards and shape assumptions; it does
not automatically detect a bad algebraic promise after work has been reordered
or removed.

**Mitigation:**
- Debug-build property tests run on registered operations.
- Conservative defaults (omitted field = worst case).
- Disable algebraic rewrites for external plugin operations until the module
  opts into verified metadata.
- Long-term: a verifier mode that runs declared-property tests in CI for
  every plugin-type module before publishing.

### 8.4 Tooling coverage

QLS code completion, hover info, doxygen generation, AST tools, sandboxing
all need plugin-type awareness. Without this, plugin types are second-
class in the IDE / docs experience.

**Mitigation:** plugin-type registration ships metadata (display names,
doc strings, completion hints) that tooling consumes via reflection. One
extension point for all tools, not per-tool.

### 8.5 Sandboxing

New types loaded by C++ modules: the QDOM domain-tag system needs to extend to
operations on those types so that, e.g., DataFrame CSV I/O remains governed by
the filesystem domain and typed DBI ingestion remains governed by the database
domain.

**Mitigation:** plugin-type operations declare the QDOM domains they touch
in their metadata; the sandboxing layer enforces these the same way it
does for built-in operations.

### 8.6 Cross-module coordination

Two modules both wanting the same global id, immediate value tag, or operation
descriptor namespace would collide.

**Mitigation:** avoid global feature bits and absolute opcode ranges for the
first protocol version. Use named QORD imports for modules and local ids for
types/operations. Keep a small central registry only for scarce global
resources such as immediate `QoreValue` tags.

### 8.7 Plugin type interaction with `%modern`-only IR

Plugin types are usable only in `%modern` code — consistent with the IR's
overall coverage. Non-modern code calling into a plugin-type-aware module
gets the AST-interpreter fallback, which dispatches via `.any` runtime
helpers. Functionally correct but slow.

**Mitigation:** documentation. The performance benefit is the headline,
and the headline assumes `%modern`. Code that doesn't need the
performance can stay non-modern.

### 8.8 Scope discipline

The proposal could grow without bound — every "what about X" feature
request becomes a candidate for plugin-type-ization. This is the "Python
C-API surface accumulation" failure mode.

**Mitigation:** keep the slot set small and orthogonal. Add slots only
when a real consumer demonstrates need. Resist adding slots speculatively.
Treat the protocol like an IETF assigned-numbers registry, not like a
feature wishlist.

---

## 9. Open questions

1. **Cross-module plugin-type interactions: explicit registration vs.
   default lattice.** Recommendation in §3.10 is "default lattice for
   common cases, explicit registration for the rest" — but the lattice
   needs a precise specification, and the precedence between explicit
   registrations and lattice rules needs nailing down.
2. **Mutation through views vs. copy semantics.** §4.4 recommends
   copy-by-default with explicit `.view()`, but ML / numerics workloads
   often want views as default. Worth surveying actual usage before
   freezing.
3. **String storage in `buffer<*string>`.** Arrow-style packed bytes +
   offsets is the right answer for performance and ecosystem interop, but
   it forces immutability-by-element. Some Qore users will expect
   mutability. Either accept the constraint or defer string buffers.
4. **Decimal storage.** `buffer<number>` doesn't compose with the dense
   storage model because `number` is variable-width. Recommended near-term
   direction is **`buffer<decimal128>`** as a fixed-width 16-byte IEEE
   754-2008 decimal element (matches Arrow `Decimal128`, Postgres NUMERIC
   when precision ≤ 38, Parquet decimal logical type), with a chunked-arena
   `buffer<number>` deferred or skipped. The `list<number>` mapping in §5.1
   is interim — consumers should treat the SQL ↔ Qore decimal column as a
   future `buffer<decimal128>` and avoid building APIs that depend on
   `list<number>` to minimise migration cost when the dense form lands.
5. **GPU types in the protocol.** Future `buffer<T>` on GPU would want a
   distinct sub-type indicating device location and possibly a different
   element-type set. Worth confirming the protocol shape is forward-
   compatible without committing day-1.
6. **DPQL ↔ plugin-type interaction.** Should DPQL learn to compile
   filter expressions against `buffer<T>` directly (vectorized predicate)
   instead of round-tripping through hash-of-lists? Probably yes, but
   that's its own design.
7. **Multimethod-style operator registration vs. per-pair operation descriptors
   for N-way operations.** Today the protocol is binary-op-per-pair. Ternary
   ops (e.g., `where(cond, a, b)`) need their own scheme. Likely separate
   per-arity slots.
8. **Per-element-type typed locals (e.g., `int32 x`).** Recommendation
   in §4.1 is storage-only — no narrow scalar types. Worth revisiting
   only if a strong ergonomic case emerges from real `buffer<T>` usage.
9. **Exact native representation for DataFrame.** Because `dataframe` is
   unreleased, the preferred direction is to change it from class/private data
   to a native plugin data type if that simplifies operators and zero-copy
   columns. The exact ownership model, private-data compatibility layer, and
   migration path for existing in-tree tests still need design.
10. **FP fast-math flag for plugin operations.** Resolved for v1 as a
    two-part opt-in: a per-operation `fp_reassociation_allowed` descriptor
    field and a Program-level `%fp-fast-math` / `QoreParseOptions::FP_FAST_MATH`
    option. Both default off, and both must be true before libqore may
    introduce IEEE-unsafe FP reassociation. The v1 flag is intentionally a
    single broad permission bit; finer gcc/LLVM-style subflags can be added
    in a later ABI revision if real modules need them.

---

## 10. References

**Existing Qore architecture:**
- [`design/qore-jit-aot-current-state.md`](../design/qore-jit-aot-current-state.md)
- [`design/qore-ir-spec.md`](../design/qore-ir-spec.md)
- `include/qore/intern/QoreIR.h` — opcode enum, instruction model
- `include/qore/intern/QoreOpcodeRegistry.h` — builtin opcode metadata registry
- `include/qore/intern/QoreAOT.h` — AOT module ABI, `QoreAOTContext`, `QoreAOTFunc`
- `include/qore/intern/QoreAOTBinary.h` — QORD format and core feature flags
- `lib/JITRuntime.cpp` — runtime helper C ABI patterns
- `lib/QoreIRLowering.cpp` — AST → IR lowering with parse-context analysis
- `lib/QoreIRToLLVM.cpp` — IR → LLVM IR codegen

**Existing data infrastructure:**
- `qlib/BulkSqlUtil/` — bulk DML via hash-of-lists
- `qlib/DataProvider/AbstractDataProvider.qc` — `searchRecordsBulk`,
  bulk record interface
- `qlib/DataProvider/DataProviderPipeline.qc` — pipeline execution and
  capability routing
- `qlib/Mapper.qm` — `mapBulk` hash-of-lists transform
- [`design/dpql-syntax.md`](../design/dpql-syntax.md),
  [`design/dpql-integration.md`](../design/dpql-integration.md) — DPQL
- `modules/dataframe/` — unreleased current DataFrame module
  (Eigen3-backed `float64`)

**Companion designs:**
- [`design/generic-class-types.md`](../design/generic-class-types.md) —
  implemented parametric class types, generic hashdecls, wildcards, and
  generic API metadata used by adjacent reflection/tooling surfaces
- [`design/named-arguments.md`](../design/named-arguments.md) — implemented
  call-site keyword arguments (helpful for wide DataFrame APIs)
- [`streaming-operators.md`](streaming-operators.md) — `first`/`any`/`all`
  / take / drop / count operators (composes with buffer reductions)

**External references:**
- Python C-API extension types (`PyTypeObject`, buffer protocol) —
  ecosystem precedent for the proposed shape
- Apache Arrow columnar format — packed-string and validity-bitmap
  conventions
- NumPy dtype system — storage-only narrow integer types
- Julia multiple dispatch — type-pair-keyed method registration
- Rust trait system — declarative property contracts
