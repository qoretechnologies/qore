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

**Companion designs:**
[`generic-class-types.md`](generic-class-types.md) (parametric class types) is
related but no longer load-bearing for `buffer<T>` because this design makes
`buffer<T>` a special built-in complex type first. [`named-arguments.md`](named-arguments.md)
is helpful but not blocking.

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
types.

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
- Add a module-registration hook through the existing module ABI
  (`QoreModuleInfo` / `QoreModuleInitContext`) with deterministic ordering.
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

A C++ module wanting to register one or more plugin types supplies descriptors
through the existing module ABI. The concrete entry point can be either a new
field in `QoreModuleInfo` or a method on `QoreModuleInitContext`, but it must be
part of the normal module initialization path so load ordering, dependency
resolution, parse-time visibility, and error reporting are deterministic.

```cpp
// header: include/qore/QorePluginType.h (new)
//
// This header is the stable plugin-type ABI and must not include LLVM headers.
// Optional LLVM lowering lives in include/qore/QorePluginLLVM.h so modules that
// only use runtime-helper dispatch are not tied to libqore's LLVM version.

enum class QorePluginHelperAbi : uint8_t {
    UnaryValue       = 0,  //!< uint64_t(value, xsink)
    BinaryValue      = 1,  //!< uint64_t(lhs, rhs, xsink)
    CallValueList    = 2,  //!< uint64_t(self, args_list_bits, xsink)
    SubscriptValue   = 3,  //!< uint64_t(container, key_or_range, xsink)
    DenseBufferUnary = 4,  //!< dense-buffer ABI, see §3.5
    DenseBufferBinary = 5, //!< dense-buffer ABI, see §3.5
};

enum class QorePluginValueAccess : uint8_t {
    ReadOnly       = 0,
    MutatesLhs     = 1,
    MutatesRhs     = 2,
    MutatesBoth    = 3,
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

//! QORD wire-format codec for a plugin value. Called during AOT serialization
//! / deserialization of constants and slot-table entries.
typedef int  (*PluginSerializeCallback)(
    uint64_t value_bits, QoreAOTBinaryWriter* w, ExceptionSink* xsink);
typedef uint64_t (*PluginDeserializeCallback)(
    QoreAOTBinaryReader* r, ExceptionSink* xsink);

//! Opaque optional-extension payload. The stable v1 ABI defines extension ids
//! but not their contents. For example, the LLVM codegen extension id points to
//! a QorePluginLLVMExtension object declared in QorePluginLLVM.h.
struct QorePluginExtension {
    const char* extension_id;
    const void* extension_data;
};

struct QorePluginOperationSignature {
    uint8_t arity;                                  //!< 1, 2, or variadic-call
    const QoreTypeInfo* lhs_type;                   //!< nullptr for non-binary call forms
    const QoreTypeInfo* rhs_type;                   //!< nullptr for unary/call forms
    const QoreTypeInfo* return_type;
    bool lhs_nullable;
    bool rhs_nullable;
    bool return_nullable;
    QorePluginValueAccess access;
    QorePluginHelperAbi helper_abi;
};

struct QorePluginOperation {
    uint16_t local_id;                            //!< 0..N within this module
    const char* operation_name;                   //!< "add", "subscript", "sum", ...
    QorePluginOperationSignature signature;
    OpcodeInfoExtended info;                      //!< full metadata (see 3.4)
    //! Runtime helper pointer with the ABI declared by signature.helper_abi.
    //! If nullptr, runtime_helper_symbol must be resolvable from the module
    //! handle during registration. The registered table stores the pointer.
    void* runtime_helper;
    const char* runtime_helper_symbol;            //!< optional diagnostic / loader lookup name
    PluginLoweringCallback   lowering_pattern;    //!< optional AST-pattern matcher
    //! Bitmap of AST node kinds (NT_*) this matcher claims to cover. Empty if
    //! no pattern matcher is installed. Used to validate Lowered/NotApplicable
    //! returns: a NotApplicable on a node kind in this bitmap is a protocol
    //! violation in %modern (parse-time error).
    uint64_t lowering_claimed_node_kinds;

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
    //! QDOM domain bits this type's operations require; ORed into the
    //! sandbox-domain mask of every site that constructs or invokes this type.
    int64_t qdom_domains;
};

struct QorePluginDependency {
    const char* module_name;
    const char* min_plugin_abi_version;
    const char* min_operation_set_version;
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

//! Runtime-side registration hook, called from qore_module_init through
//! QoreModuleInitContext or an equivalent QoreModuleInfo callback. The _v1
//! suffix is mandatory: every breaking change to the registration ABI ships
//! as a new symbol (qore_register_plugin_types_v2, ...) and old binaries keep
//! resolving the symbol they linked against. See §8.1.
int qore_register_plugin_types_v1(
    const QorePluginTypeRegistration* reg,
    ExceptionSink* xsink);
```

Constraints:
- `local_id` must be contiguous starting from 0 and is meaningful only inside
  the registering module. The loader allocates a process-global runtime id at
  `qore_register_plugin_types_v1` time and records the local→global mapping in
  the per-module registration table; QORD `PLUGIN_IMPORTS` references are
  resolved through that table at load time.
- The first implementation should lower plugin operations to built-in
  descriptor-carrying opcodes such as `PluginUnary`, `PluginBinary`,
  `PluginCall`, and `PluginSubscript`. The descriptor contains
  `(module_id, operation_local_id, type_signature_id)` and is serialized in
  QORD through `PLUGIN_IMPORTS`.
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
- Optional extensions are ignored unless the runtime recognizes their
  `extension_id`. The first planned extension is `"qore.plugin.llvm.codegen"`
  from `QorePluginLLVM.h`.

Optional LLVM extension header:

```cpp
// header: include/qore/QorePluginLLVM.h (new)
//
// Including this header opts a module into libqore's exact LLVM ABI.
// Modules using it must be rebuilt when libqore's LLVM major version changes.

typedef llvm::Value* (*PluginLLVMCodegenCallback)(
    PluginLLVMCodegenContext* ctx,
    const QoreIRInstruction* inst,
    llvm::IRBuilder<>* builder);

struct QorePluginLLVMExtension {
    uint32_t libqore_llvm_major;
    PluginLLVMCodegenCallback codegen;
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
    // Algebraic
    bool is_commutative;           //!< op(a, b) == op(b, a)
    bool is_associative;           //!< op(op(a, b), c) == op(a, op(b, c))
    bool is_idempotent;            //!< op(op(a)) == op(a) for unary
    bool annihilator_zero;         //!< x * 0 = 0, x AND false = false, etc.

    //! Identity element for binary algebraic ops. has_identity == false ⇒
    //! the op has no identity (or none worth folding). Builtin scalar
    //! identities use identity_kind. Plugin-owned identities are produced by
    //! make_identity so refcounted/program-owned values are not stored inside
    //! constexpr-style metadata.
    bool has_identity;
    uint8_t identity_kind;            //!< 0=custom/none, 1=int0, 2=int1, 3=float0, ...
    uint64_t (*make_identity)(const QoreTypeInfo* result_type, ExceptionSink* xsink);

    // Effect
    bool is_pure_modulo_xsink;     //!< no side effects except via ExceptionSink
    bool may_alias_inputs;         //!< result may share storage with inputs
    bool can_vectorize;            //!< safe to apply elementwise across a buffer
    bool is_simd_friendly;         //!< runtime helper has SIMD codegen

    // Type system
    OpcodeTypePromotion type_promotion_kind;
    PluginTypePromotionCallback type_promotion_callback; //!< if kind == Custom

    // Cost hints
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
- `cost_class` informs scheduling and outlining decisions.

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
helper pointer; the protocol uses a single per-Program plugin-helper table to
serve all three:

1. **IR interpreter.** `qore_register_plugin_types_v1` populates a
   table indexed by global operation id. Each entry stores
   `(void* helper, QorePluginHelperAbi helper_abi, signature)`. The interpreter's
   `PluginUnary` / `PluginBinary` / `PluginCall` / `PluginSubscript` handlers
   read the descriptor's global id from the instruction's operand stream,
   verify the expected helper ABI in debug builds, cast to the declared
   trampoline type, and call it.
2. **JIT.** The ORC symbol resolver consults the same table on first
   reference: `qore_rt_<module>_<op>_<typesig>` becomes a generated lookup
   stub that loads from the table at the slot recorded in the IR
   instruction. Lazy compilation is therefore a no-op — the table is already
   populated by registration time.
3. **AOT.** `QoreAOTContext` gains a new slot-table array
   `plugin_helpers: void*[]` plus parallel helper-ABI/signature metadata
   populated alongside the existing `LocalVar*` / `Var*` / expression arrays.
   AOT-emitted code references plugin helpers by slot index
   (`ctx->plugin_helpers[N]`), not by symbol. The new
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
`"qore.plugin.llvm.codegen"` optional extension from `QorePluginLLVM.h`.
`PluginLLVMCodegenContext` exposes the host's value table, type table,
exception-sink pointer, current basic block, and helpers for emitting LLVM IR
in a way consistent with the rest of `QoreIRToLLVM.cpp`.

**LLVM ABI coupling — largest forever-promise in the protocol.** The
callback signature names `llvm::Value*` and `llvm::IRBuilder<>*` directly,
which means a plugin module that uses this hook is hard-bound to libqore's
exact LLVM version: an LLVM major-version bump in libqore breaks every
plugin that links against the codegen callback. This is the single biggest
ABI-stability commitment in the entire plugin protocol — bigger than the
QORD format and bigger than the runtime helper ABI, both of which are
plain-C and version-stable across compiler updates.

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

Plugin types participate in parse-time AST-pattern matching to pick their
typed operation descriptors when both operand types are known. A module
registers a pattern matcher via the `PluginLoweringCallback` typedef
(§3.3), which returns a `PluginLoweringResult` enum rather than a bare
`bool` so the no-silent-fallback invariant can be enforced mechanically.

The callback inspects the AST node, queries `parse_ctx` for operand types,
and returns one of:

- **`Lowered`** — emitted a built-in plugin-dispatch opcode with a
  registered operation descriptor.
- **`NotApplicable`** — the AST node is outside the matcher's claimed
  coverage (`lowering_claimed_node_kinds` bitmap). The lowering pass
  proceeds to the next matcher or, if none match, the `.any`-fallback path.
- **`Erroneous`** — the matcher detected a hard error (e.g., a type
  annotation it knows it cannot represent) and reported it through the
  context. Lowering aborts with the reported diagnostic.

`NotApplicable` returned for an AST node kind *inside* the matcher's
declared `lowering_claimed_node_kinds` bitmap is treated as a parse-time
error in `%modern`: a plugin matcher that claims to handle a node kind is
required to either lower it or report `Erroneous`. This closes the silent
miscompile gap that a bare `false` return would leave open.

### 3.8 Tag allocation in NaN-boxed `QoreValue`

Most plugin types use pointer representation managed by the registered native
data-type descriptor. This is the default and requires no tag allocation.

Plugin types representing immediate-sized values (e.g., a packed `bool`
bitmap byte, an interned short type identifier) may claim a reserved
immediate tag, but no concrete range is assigned by this design. The tempting
`0xFFE0..0xFFE7` range is currently unsafe because values below the integer tag
boundary can be classified as doubles by `QoreValue::isFloat()`. Before any
plugin immediate tags are published, `QoreValue` classification must reserve a
non-overlapping range and all fast predicates must be audited against it.

Tag allocation is centralized; modules do not pick their own tags. Most plugin
types will not need this — pointer-tagged values or native data-type nodes are
sufficient.

### 3.9 QORD format extensions

Three new section types:

```cpp
enum class QoreAOTSectionType : uint16_t {
    // ...existing 1-21...
    PLUGIN_TYPE_REGISTRY = 22,  //!< Per-module plugin-type metadata
    PLUGIN_IMPORTS       = 23,  //!< Required modules + type/op versions
    PLUGIN_HELPER_REFS   = 24,  //!< Slot-index → (import_idx, op_local_id)
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
slot table on `QoreAOTContext` (§3.5): each entry is
`(import_idx, op_local_id, signature_hash)` and the loader resolves it against
`PLUGIN_IMPORTS` + the registered runtime helper table to populate
`ctx->plugin_helpers[slot]` from `qore_aot_module_init_v4`. The signature hash
prevents accidentally binding an operation whose local id was reused with a
different ABI in an incompatible module build.

New value tag (or range):

```cpp
enum class QoreAOTValueTag : uint8_t {
    // ...existing 0-18...
    VT_PLUGIN_INSTANCE = 19,  //!< Module-defined value type
};
```

Encoded as `(import_idx, type_local_id, serialization_format_version,
payload_bytes)`; runtime dispatches to the registered deserializer after the
import is resolved. The payload is module-defined but must be length-prefixed
and rejected if the type's serializer version is unsupported.

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
   target Program, and has a compatible signature / serialization format
   version.
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
   set number that the other module declares an upper bound on.
3. **Commutative auto-symmetry.** If a registration for
   `(A, B, op)` declares `is_commutative = true`, the runtime synthesises
   the `(B, A, op)` site automatically — registering both halves
   explicitly is redundant.
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
  code.
- **Per-Program activation/import tables.** A Program only sees plugin types
  from modules it imported or from modules named by a QORD `PLUGIN_IMPORTS`
  section it is loading. This preserves optional-module semantics: loading
  `dataframe` in one Program does not silently expose DataFrame syntax or types
  in unrelated Programs.

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

---

## 4. Worked example: `buffer<T>`

`buffer<T>` is the canonical first plugin type. It validates the protocol
end-to-end before exposure to external modules.

**Type-system carrier: special built-in complex type.** `buffer<T>` ships as
a special built-in complex type (alongside `list<T>` / `hash<string,T>`)
with explicit `QoreTypeInfo` interning, equality, reflection, and QORD
serialization rules. The companion `generic-class-types.md` design is *not*
on the critical path for `buffer<T>` — committing to the special-built-in
representation removes the fork that would otherwise duplicate the
implementation surface and lets Phase 1 proceed independently. If parametric
class types ship later, `buffer<T>` becomes a candidate for migration as a
cleanup task; the user-visible syntax (`buffer<int64>`, `foreach int v in
(b)`) is the same in both representations.

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
float64         mean = a.mean();
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

---

## 7. Phased rollout plan

The proposal admits incremental delivery. Each phase ships independently
useful functionality without committing to later phases.

Phase sizes below are coarse t-shirt estimates relative to each other —
**S** ≈ targeted refactor / well-scoped subsystem, **M** ≈ multi-subsystem
work, **L** ≈ multi-subsystem with new wire-format and API surface. Real
calendar time depends on what other branch work is in flight.

### Phase 0 — implementation-gap cleanup (S)

- Centralize builtin opcode property consumers behind `OpcodeInfo` where
  practical, or document every remaining switch that intentionally stays local.
- Add conservative declarative-property fields to `OpcodeInfo` and populate
  them for the existing 364 opcodes.
- Add tests for metadata-driven verifier decisions before adding optimizer
  rewrites.
- Refactor or explicitly fence `QoreValue` tag classification so plugin
  immediate tags cannot collide with float/int/pointer/special predicates.
- Add extensible `TypeProfile` keys for non-builtin types: replace the
  current 6-enum `dominantType()` return with an interned-key encoding
  carrying `(kind, payload)` where `kind ∈ {builtin_int, builtin_float,
  builtin_string, builtin_bool, builtin_nothing, builtin_other,
  qore_class, plugin_type}` and `payload` is the interned `QoreTypeInfo*`
  for the user-class / plugin-type cases. Builtin keys keep the existing
  fast-path and atomic-counter layout; plugin-typed observations populate
  a per-guard `concurrent_hash_map<TypeKey, uint64_t>` allocated lazily on
  first non-builtin observation.

Ships independently useful cleanup even before plugin types exist. Optimizer
passes that rely on new algebraic metadata should wait until the metadata has
tests and conservative defaults.

### Phase 1 — `buffer<T>` in libqore (L)

- Add `buffer<T>` to libqore as a special built-in complex type with
  explicit `QoreTypeInfo` interning and QORD serialization (decision per
  §4 intro — does not depend on `generic-class-types`).
- Element types: `int8`, `int16`, `int32`, `int64`, `float32`, `float64`,
  `bool`.
- Arithmetic, reductions, slicing, iteration, conversion to/from `list<T>`.
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
- Registration through `QoreModuleInfo` / `QoreModuleInitContext`.
- Built-in plugin-dispatch opcodes carrying operation descriptors.
- IR-interpreter dispatch table for module-registered helpers.
- JIT helper symbol resolver for module-registered helpers.
- QORD `PLUGIN_IMPORTS` and `PLUGIN_TYPE_REGISTRY` sections.
- Documentation + sample module.

Opens the door externally. No new functionality on its own — Phase 1's
`buffer<T>` could optionally migrate to using the protocol internally, but
that's a refactor, not a feature.

### Phase 4 — optional LLVM codegen + lowering hooks (M)

- Optional `QorePluginLLVM.h` / `PluginLLVMCodegenCallback` extension ABI.
- `PluginLoweringCallback` ABI.
- Default fallback (no callback supplied → emit runtime-helper call).

Plugin types can opt into native SIMD codegen and parse-time pattern
matching.

### Phase 5 — make `dataframe` a native plugin data type (M)

- Because `dataframe` has not been released, change `DataFrame` from the
  current class/private-data representation to a native data type if that
  produces cleaner value, ownership, and operator semantics.
- Register `DataFrame` via the plugin protocol.
- Subscript: `df["col"]`, `df[1..10]`.
- Comparison and predicate operators returning bitmap masks.
- Lazy expression DSL through registered operators.

Validates the protocol on a more complex second consumer. Unlocks the
Polars / pandas-style ergonomics for DataFrame.

### Phase 6 — pipeline `DataFrame` propagation (M)

- `DataProviderShape` negotiation for row records, hash-of-lists,
  buffer-columns, and DataFrame blocks.
- `DataProviderPipeline` auto-routing with conversion costs.
- Sticky-typed-form bias.
- Stock processors updated where worthwhile. Start with
  `QoreFilterRecordsProcessor`; add vectorized analytics paths separately;
  treat `QoreGroupByProcessor` as a rewrite candidate because it is not bulk
  aware today.

End-to-end zero-conversion typed pipelines for analytics workloads.

### Phase 7 — declarative-metadata-driven optimizations (ongoing)

- Profile-guided specialization for plugin types.
- Lazy expression fusion (when at least two participating ops declare
  purity + commutativity / associativity).
- Cross-stage fusion in pipelines.

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
- [`generic-class-types.md`](generic-class-types.md) — parametric class
  types (related future cleanup path for generic built-ins and plugin types)
- [`named-arguments.md`](named-arguments.md) — call-site keyword
  arguments (helpful for wide DataFrame APIs)
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
