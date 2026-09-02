# AOT Object Files and Module Artifacts

## Status

The `.qo` object-file pipeline is implemented in `qcc` and the AOT runtime.
This document records the current design contract rather than the historical
phase plan.

Relevant code:

- `qcc-main.cpp`
- `include/qore/intern/QoreAOT.h`
- `include/qore/intern/QoreAOTBinary.h`
- `lib/QoreAOT.cpp`
- `lib/QoreAOTRuntime.cpp`
- `lib/QoreAOTBinary.cpp`

## Artifact Types

| Artifact | Produced by | Purpose |
|---|---|---|
| Executable | `qcc -o app script.qr` or `qcc -o app *.qo` | Native launcher for a Qore script/app |
| `.qmod` | `qcc -m Module.qm` or `qcc -m qlib/Module` | Loadable binary user module |
| `.qo` | `qcc -c file.qr` / `file.qc` / `file.qm` | Relocatable object containing native code and AOT metadata |
| `.qoa` | `qcc -a --context=DIR *.qo` | Static archive exposing `qore_qoa_register_all()` |

`.qo` is both object code and declaration carrier. It contains compiled native
functions plus enough AOT metadata for later compile/link stages to preload
cross-file declarations.

## Object Registration

Script-context `.qo` files expose registration entries that a host or
qcc-generated glue object calls to add metadata and native function tables to a
`QoreProgram`.

Per-file module `.qo` files produced with `--context=DIR` are intermediate
fragment carriers. They do not emit the module self-registration ABI; `qcc -m
--from-objects` reads their fragment symbols and synthesizes a single `.qmod`
registration path.

Batch registration uses the multi-deserializer:

1. Open all metadata blobs and create namespace/class/hashdecl/typedef shells.
2. Resolve types and base classes across all sessions.
3. Register constants across all sessions.
4. Resolve own members, then inherited members.
5. Deserialize functions, methods, globals, static members, and init functions.
6. Commit class structures and run final fixups.

The cross-session barriers are required because one `.qo` can reference classes,
constants, or members declared in another `.qo`.

### Class-Static LValue Roots Resolve By Name, With A Load-Time Fallback

An AOT image cannot carry the parse-time `StaticClassVarRefNode` that a `ClassName::var` lvalue root normally
holds in `LVPathStep::ref_ptr`, so the step keeps only its symbolic `"ClassPath::varName"` name and
`LValueHelper::navigatePath()` resolves that name **in the program running the code** when the assignment
executes. That is deliberate: it keeps a module's writes aligned with `LoadStaticVar`-by-path reads after the
module's namespace is merged into an importing program, and it lets a standalone fragment reference a static
provided by a sibling `.qo` that is not registered yet when its context is built.

By-name resolution cannot see a class that is **private to its module**, because such a class never appears in
the namespace of the program that loaded the module. The static is therefore *also* resolved when the image is
loaded — in the program that owns the code (`local_owner_pgm`, else `pgm`) — and kept in
`LVPathStep::aot_static_var_info`, which `navigatePath()` uses only when the by-name lookup fails. Resolution at
load time is best-effort: a class that is not registered yet simply leaves the field null and the symbolic path
remains the only mechanism.

The three restore sites that must populate the field are `buildContextFromSlotMap()` and
`deserializeIRInstruction()` in `lib/QoreAOTRuntime.cpp` and the `LValuePath` case in
`lib/QoreAOTInstRegistry.cpp`. Only a *dynamic* lvalue path reaches this code — a constant hash key such as
`Holder::smap{"k"}` is lowered differently — so a regression test must use a variable key; see
`examples/test/ir/AOTModulePrivateClassStatic.qtest`.

### Restored Expression Trees Are Already Parse-Initialized

Expression trees restored from an AOT image (class member initializers, static
var and global initializers, default argument values) are rebuilt by the AOT
expression readers in their **post-parse-init** form: target classes and methods
are resolved, pseudo-method calls are tagged, and argument lists are already
resolved via `resolveParseArgs()`. Nothing may parse-initialize them again.

Two independent paths would otherwise do exactly that, and both are blocked by
marking the deserialized declaration as already parse-initialized:

- static class vars: `resolveStaticMembers()` calls `QoreVarInfo::parseInit()`
  before installing the serialized value, so the later `parseInit()` from
  `qore_class_private::copy()` during a class merge is a no-op
- instance class members: `resolveInstanceMembers()` calls
  `QoreMemberInfo::setParseInitDone()`. The trigger is any class subclassed
  either inside the same module or by the code loading it:
  `importInheritedMembers()` → `qore_class_private::initializeMembers()` →
  `BCNode::initializeMembers()` → `parseImportMembers()` calls
  `QoreMemberMap::parseInit()` on the **base** class to guarantee parent members
  are initialized before merging (issue #2657), which walks straight into the
  restored trees. Re-initializing a restored tree aborts on
  `!pseudo && !pseudoTypeInfo` in `MethodCallNode::setPseudo()` in debug builds
  and silently re-resolves call targets in release builds.

The asserts in `MethodCallNode::setPseudo()` and
`AbstractMethodCallNode::parseSetClassAndMethod()` are the only thing reporting
a violation of this invariant; they must be kept.

### Restored Expression Trees Resolve Symbols in a Late Phase

A restored tree can reference any symbol in the module, so it can only be
rebuilt once every symbol it might name is registered. Registration is
incremental — `deserializeFunctions()` adds each function to its namespace only
after reading that function's own variants, and the FUNCTIONS section is written
in `func_list` hash order — so "is the referenced symbol registered yet?" has no
stable answer mid-phase.

Trees are therefore captured as raw blobs and resolved in a dedicated late
phase rather than in place:

- BCA (base-class constructor argument) blobs → `resolveBCAExpressions()`
- general expression-tree param defaults (`VT_EXPR_NATIVE`) →
  `resolveNativeExprDefaults()`, via `PendingNativeExprDefault`; the signature
  slot holds a non-`NOTHING` placeholder in the interim so `hasDefaultArg()`
  stays true and overload resolution still treats the parameter as optional
- static-method param defaults → `PendingStaticMethodDefault`, resolved in
  `finalizePreIndex()`

Both `resolveBCAExpressions()` and `resolveNativeExprDefaults()` run in
`finalizePostIndex()`, after `commitDeserializedClasses()` and
`rebuildAOTRootIndexes()`, and before any module init function executes. Adding
a new deferred-tree kind means adding it to that phase, not resolving it earlier.

## Compile-Time Preload

`qcc -c -L<dir> file.qr` scans `.qo` files in preload directories and preloads
their declaration metadata before parsing `file.qr`. This gives Qore the C-style `.o + .h`
workflow without separate header files.

The preload step must not execute user code. It only creates declaration shells
needed for parse-time name and type resolution.

### Cross-File Body Contracts Are Body Dependencies

Batch script compiles and `-L` preload let a caller consume a callee's lowered
body contract from another object. The contract can provide a `_fast` entry (a
hidden, direct, unboxed-argument entry point), or an importable scalar, string,
collection, aggregate, or object-operation summary that replaces the call
entirely. This is not a declaration dependency, it is a dependency on the
callee's **lowered body**:

- the fast entry exists only while that body stays eligible for one, and
- the emitted call sequence bakes in the body's ABI and effect summary
  (parameter/return kinds, cache-invalidation and runtime-local effects,
  importable scalar/string/aggregate body summaries).

A body-only edit therefore changes or removes the fast entry while the callee's
declaration hash and standard entry symbol are untouched. Two rules keep that
sound in incremental builds:

- an object that consumes a cross-file body contract records the callee's
  source file as a build dependency, so the caller is recompiled whenever the
  callee's body changes. Without it the caller can keep an undefined reference
  to a removed `_fast` symbol or machine code folded from an obsolete summary;
- imported body-summary provenance is transitive. If `top.qc` folds a summary
  from `middle.qc`, and that summary incorporated `leaf.qc`, `top.qo` records
  both sources even when no native symbol from either callee survives;
- fast-entry metadata is never imported from a preloaded `.qo` whose recorded
  source hash no longer matches its on-disk source. A stale sibling describes a
  body that is about to be replaced, so importing from it would make the result
  depend on the order in which the build recompiles the two objects. Staleness
  must be *proven* — a missing source or a missing hash leaves the sibling
  usable.

Each symbol-index body-contract record carries its sorted, duplicate-free
`body_dependency_files` provenance. Ordinary cross-unit references stay
dependency-free: they resolve by name at load/register time, so a stale
dependent picks up the new provider automatically.

Starting with symbol-index version 37, each exportable lowered body contract
also has a deterministic `body_contract_hash`. An object that imports a fast
entry or body summary records the provider path, source file, and exact hash as
a `native_body` import. Both the explicit link planner and ordinary script
object linking reject an unresolved, ambiguous, or hash-mismatched body
contract. This preserves incremental build correctness even when a build tool
does not rebuild a caller after a provider body changes. Source-only edits that
leave the lowered contract unchanged retain the same hash and remain link
compatible.

## Module Aggregation

`qcc -m <module-dir>` is the preferred clean-build path for split-directory
modules. It parses the module once and emits one `.qmod`.

`qcc -m --from-objects --context=<dir> *.qo` remains the object aggregation
path for workflows that need per-file object granularity, but it is not the
default standard-library module build path.

## Metadata Compatibility

Every AOT metadata blob carries:

- QORD magic and format version.
- Qore producer version.
- Maximum IR opcode ID.
- AOT feature flags.
- Module dependencies and reexports.
- Program metadata such as execution class where applicable.
- Module path prepend/append lists.
- Optional build information for diagnostics.

Loaders must reject metadata requiring unsupported features or newer opcodes.

Native `.qmod` code has an additional ABI contract independent of the metadata
format and the C/C++ binary-module API. Generated module descriptors encode the
exact `QORE_AOT_MODULE_ABI_VERSION` in a tagged form of the existing module
API-minor argument; this preserves the descriptor helper's C ABI and makes an
older runtime reject a newer AOT module as an unsupported module API. A current
loader decodes and validates the AOT revision before loading dependencies or
running generated initialization code. A descriptor produced before this
contract reports version zero and is rejected with a rebuild diagnostic.
Increment the native ABI revision whenever generated code can no longer execute
safely against the previous runtime-helper or ownership contract. Metadata-only
backward readers remain governed by their format and feature versions.

## Symbol Index

Newly generated metadata may also carry an optional `SYMBOL_INDEX` section. The
section is versioned independently from the core QORD format and is advisory for
current runtimes: old readers can ignore it, and absence of the section remains
valid for older `.qo` files.

The first index version records:

- defined Qore symbols: namespaces, classes, hashdecls, enums and enum members,
  typedefs, constants, globals, functions, methods, constructors, static
  methods, static vars;
- known native symbols associated with compiled Qore bodies and init functions;
- compilation context key/value pairs such as producer Qore version, AOT binary
  version, feature flags, opcode limit, and source/module filters.

Imports are intentionally conservative in the first version. The index format
has import fields for provider/consumer source files, required hashes, and
dependency classes, but the writer omits unsafe guesses until parse/codegen
sites can report symbol identity precisely.

Hashes are split by use:

- `signature_hash` covers callable name, return type, parameter names/types,
  and varargs flags;
- `declaration_hash` covers the API surface relevant to recompilation, such as
  symbol kind, path, visibility, type surface, callable signature, and method
  modifiers;
- `value_hash` covers folded scalar/container constant and enum values, while
  runtime-initialized constants are marked as `pending` and complex runtime-backed
  values are marked as `not-foldable:<type>:<reason>`;
- implementation/native body changes are represented by the native symbol
  association and future linker metadata, not by changing API hashes.

Version 36 adds `body_dependency_files` to native body-contract records. The
field is consumed only when code generation actually imports that contract; it
is not a general declaration dependency.

Version 37 adds `body_contract_hash` and structured
`body_contract_dependencies`. A consumed contract is emitted as a required
`native_body` import with ABI kind `qore_body_contract_import_v1`; link planning
matches it against the exact hash on the provider's defined function or method
record. Dependencies remain transitive, so a summary folded through an
intermediate object still names and hashes its original provider.

Build tools should treat these hashes as a dependency-planning contract. Runtime
loading still depends on the normal AOT metadata sections, not on the symbol
index.

## PC→Location Section (`qore_aot_pcloc`)

To report real source locations in exceptions thrown from AOT-compiled native code,
qcc emits a per-function PC→location map into a dedicated **object section** rather
than into the QORD metadata blob. Unlike the metadata blob (which is consumed when a
`.qmod`/`.qo`/executable is loaded), this section must survive *arbitrary downstream
linking* — a `.qo` fragment gets `ld -r`-merged, aggregated into a `.qmod`, or linked
into a host executable — so the map has to ride through relinking with no cooperation
from the linker or the user.

- Both ELF and Mach-O linkers concatenate same-named input sections into the output
  artifact, so qcc adds the map as its own section to every object it emits and the
  data is preserved verbatim across relinks.
  - ELF: a non-alloc section named `qore_aot_pcloc` (added with GNU `objcopy`).
  - Mach-O: `__QORE,__pcloc` (segment/section pair; added with `llvm-objcopy`, since
    GNU objcopy corrupts Mach-O).
- The payload is a sequence of self-delimiting framed records
  (`[uint32 magic 'QPCM'][uint32 payload_len][payload]`); each object contributes one
  record, and the reader walks all concatenated records, accumulating per-function
  entries. The framed-record and per-function payload format is identical on ELF and
  Mach-O.
- The section is non-alloc / not mapped at runtime; it is read from the artifact file
  (via `dladdr`'s `dli_fname`) lazily at throw time, so it adds no steady-state cost.
  See [`aot-lazy-loc-innermost-frame.md`](aot-lazy-loc-innermost-frame.md) for how the
  map is consumed during exception construction.

## Inspection

`qcc --dump-info <path>` inspects executables, `.qmod`, `.qo`, `.qoa`, and
ordinary object files without executing embedded Qore code.

Optional flags:

- `--dump-symbols`: print an nm-like symbol table.
- `--dump-sections`: print object and AOT section tables.
- `--dump-index-json`: print build-consumable JSON for the AOT symbol index.
- `--write-index-json=<path>`: write the same JSON sidecar after a successful
  single-output build.

With `--dump-symbols`, AOT metadata dumps also include `SYMBOL_INDEX` defined,
imported, native, and context records when the section is present.

Build tools can materialize the JSON output next to an object as
`<object>.idx.json` by passing `--write-index-json=<object>.idx.json` in the
same qcc invocation that produces the object. The sidecar mirrors the binary
index with `defines`, `provides`, `requires`, `native`, `source_text`, and
`native_body_hash` fields so planners can distinguish source/API/value rebuilds
from relink-only native body changes without parsing the human dump format or
starting a second qcc process.

The dump path is for diagnostics only; runtimes ignore `BUILD_INFO` and do not
require `SYMBOL_INDEX`.

## Object Linking

`qcc --link-qo -o <aggregate.qo> --aggregate-symbol=<SYM> input1.qo ...`
links existing script-context `.qo` inputs into one aggregate register object
without reparsing the original source files.  The linker reads each input
object's `SYMBOL_INDEX`, validates static Qore imports that can be checked from
the available index records, extracts exactly one exported `*_script_register`
symbol from each input object, and emits `init_<SYM>_qo(QoreProgram*)`.

The generated aggregate object calls the input register symbols in command-line
order.  It is linked alongside the input `.qo` files; therefore missing native
register symbols still fail in the normal native linker.  This first
object-driven link mode preserves the per-file metadata registration semantics
while removing source-list assumptions from the aggregate registration step.

Register symbols extracted from input objects are normalized to their logical
LLVM IR spelling: object symbol tables that use a global-symbol prefix (Mach-O,
and 32-bit x86 COFF) decorate `qore_<sanmod>_<sanfile>_script_register` as
`_qore_<sanmod>_<sanfile>_script_register`, and that prefix is stripped before
the name is stored.  The aggregate is generated as LLVM IR, so the target
mangler re-applies the prefix when the object is emitted; storing the decorated
spelling would make the aggregate reference `__qore..._script_register` while
its inputs define `_qore..._script_register`, and the native link would fail
with undefined symbols.  Only the qcc-generated `qore..._script_register`
family is normalized.

Only *exported* (global, defined) symbols qualify as registration entry points.
The aggregate references them across a native link, so a local symbol-table
record can never satisfy the reference, and accepting one makes a valid input
look like it exports the registration function twice.  This matters on Mach-O:
the parallel-codegen path merges its codegen partitions with `ld -r`, and the
Darwin linker writes a STABS debug map into the merged symbol table
(`N_SO`/`N_OSO`/`N_BNSYM`/`N_FUN`/`N_ENSYM`).  The `N_FUN` record for the
registration function repeats the exported definition's name and address as a
non-external entry, so filtering on "defined" alone rejected split-codegen
objects with `input has multiple exported *_script_register symbols`.
Identical logical names are additionally collapsed, so an object format that
lists one exported definition more than once still contributes one name.

The command also writes `<aggregate.qo>.qolink.json` by default, or the path
given with `--qolink-map`.  The link map records input object hashes, register
symbols in their logical (undecorated) spelling, provided Qore symbols,
resolved/unresolved/ambiguous imports, hash mismatches, external dependencies,
module path lists, module commands, and native symbols expected by the final
link.

## Build-System Contract

When `QORE_BUILD_AOT_MODULES=ON`, CMake builds standard user modules as `.qmod`
artifacts and installs them beside their `.qm` sources. Runtime module lookup
prefers `.qmod` in the same location, so installed modules use compiled code by
default.

The `qcc-format` stamp controls mass qmod rebuilds. Only changes that can alter
qmod output or qcc compile semantics should update that stamp.

The stamp is a content digest -- path, size and SHA-256 per entry -- over a
curated list of libqore's AOT/IR codegen sources (`_qcc_format_sources` in
`CMakeLists.txt`). It is precise enough for in-tree iteration but cannot be
complete: AOT output is generated by libqore, whose codegen spans far more of
the library than that list names. A consumer building against an *installed*
qore therefore tracks the installed library itself in addition to the stamp,
so reinstalling a qore whose codegen changed outside the curated list rebuilds
`.qo` objects instead of leaving them silently stale. Stale objects do not fail
at the edit that caused them; they surface later as a link or
deferred-resolution error naming a file the developer never touched. A
reinstall is rare and a full `.qo` rebuild is cheap, so the conservative
dependency is the right trade against that diagnosis cost. In-tree builds keep
the curated list alone, preserving fast iteration.

`QORE_GET_QCC_DEPS()` decides both, and they are independent questions: which
file carries the fingerprint, and whether the library is tracked as well. It
answers the second from `QORE_IN_BUILD_TREE`, which `QoreConfig.cmake` exports.
Folding the two into one `if`/`elseif` chain is what once made the library
dependency unreachable -- `QoreConfig.cmake` always sets
`QORE_QCC_FORMAT_STAMP`, for a build-tree qore as much as an installed one, so
every out-of-tree consumer took the fingerprint branch and never reached the
library branch below it. Both dependencies reach qcc as `--manifest-input`
entries as well as CMake `DEPENDS`, because `--skip-if-manifest-current` decides
staleness from the manifest alone: a build input that only reaches the CMake
graph makes the recipe run and qcc skip inside it, which leaves the object
exactly as stale as tracking nothing.

Membership of the curated list is audited at configure time. Every
`lib/QoreAOT*.cpp`, `lib/QoreIR*.cpp`, `include/qore/intern/QoreAOT*.h` and
`include/qore/intern/QoreIR*.h` must appear either in `_qcc_format_sources` or
in `_qcc_format_excluded_sources`, and adding one fails the configure until
someone classifies it. The list drifts silently otherwise: a new codegen file
compiles, links and works, and only its absence from the fingerprint is wrong.
`QoreAOTExprNodeRegistry.cpp` went un-fingerprinted for three months that way,
and the order of its 256-entry kind table *is* the on-disk expression-tree
format. A file is excused only when it cannot change what qcc emits or how an
emitted object is read back; the IR interpreter qualifies only because the
compiler's outlining pass no longer calls into it.

Both halves of the binary format belong in the fingerprint, not just the
writer. The load-time check rejects a `.qo` whose version is *newer* than the
reader's and accepts an older one, so a reader change against an
un-invalidated cache is silent misinterpretation rather than a load error.

`qcc` owns the reusable build-sidecar contract for `.qo` and aggregate outputs.
Build systems should prefer qcc options over external scanners when the needed
information comes from parsed Qore metadata:

- `--depfile=<path>` emits Make/Ninja dependencies for single-source compile,
  `--script-aggregate`, and `--link-qo` single-output commands.
- `--batch-script-aggregate=<symbol>` with
  `--batch-script-aggregate-output=<path>` emits runtime aggregate metadata
  from the same committed program used for batch `.qo` compilation. Clean
  builds can therefore avoid reparsing the complete source set in a second qcc
  process; standalone `--script-aggregate` remains available for incremental
  aggregate rebuilds and callers without a shared batch parse.
- `--depfile-module-deps=source|artifact|none` selects how module-mode depfiles
  represent `%requires` dependencies. `source` is the deterministic default and
  expands a split module to its complete `.qm`/`.qc`/`.ql` source set, retaining
  the resolved artifact only when no source representation can be found;
  `artifact` preserves the resolved-file behavior; `none` leaves cross-module
  edges to the build system.
- `--write-index-json=<path>` emits the AOT symbol index JSON for the generated
  object or aggregate.
- `--compile-contract-stamp=<path>` emits a canonical, write-if-changed digest
  input containing the object's declaration, folded-value, and lowered-body
  contract hashes. Compile consumers should depend on this stamp instead of the
  complete object whenever they use the object only for preload metadata.
- `--depfile-compile-contract-stamps` rewrites source dependencies covered by
  exact imported symbol/body contracts to the corresponding provider
  `.qo.compile-contract.stamp`. A comment-only provider rebuild then leaves the
  caller current through both its explicit metadata edge and its
  compiler-generated depfile. Final links still use `.content.stamp`, because a
  provider implementation can change without changing any compile-time
  consumer contract.
- `--write-manifest=<path>` emits a deterministic manifest containing qcc
  options, selected environment, sidecar paths, output hash/size, and hashes of
  all known inputs.
- `--manifest-input=<path>` adds build-system-owned files, such as response
  files or ABI stamps, to the manifest.
- `--skip-if-manifest-current` exits successfully without rebuilding when the
  manifest content already matches current output/input state.

The manifest skip check intentionally runs before Qore initialization when the
output and sidecars are current. This makes no-op delta builds cheap and keeps
the correctness decision close to the compiler state that can affect generated
artifacts.
