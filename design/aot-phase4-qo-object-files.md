# Phase 4: `.qo` Object Files for C++ Linkage

**Status:** Design. Scope: enable compiled Qore code to be linked into
a larger binary — either a C++ application like `qorus-core` (in
`~/src/qore/git/qorus`) or a split-module `.qmod` — with `.qo` as the
common intermediate granularity.

**Author:** AOT team, 2026-04-18. Supersedes the sketch in
`aot-compile-time-optimization-roadmap.md` §Phase 4.

## Goals

Two use cases share the same `.qo` intermediate format:

### Use case 1: C++ application with many Qore sources (qorus-core)

```bash
# Today: qorus-core parses many Qore sources at startup.
# Phase 4 target: pre-compile them, link into the binary.

qcc -c --context=src/ src/jobrunner.q       # → src/jobrunner.qo
qcc -c --context=src/ src/scheduler.q       # → src/scheduler.qo
qcc -c --context=src/ src/workflow.q        # → src/workflow.qo
...

g++ main.cpp src/*.qo -lqore -o qorus-core
./qorus-core   # Zero Qore parsing at startup; %requires-d modules
               # still load dynamically — that's OK, acts like dynamic
               # linking.
```

### Use case 2: Speed up large split-module builds

```bash
# Today: qcc -m qlib/BigMod/  is monolithic, slow, unparallelizable.
# Phase 4 target: compile each .qc to .qo, link into .qmod.

qcc -c --context=qlib/BigMod/ qlib/BigMod/BigMod.qm   # → BigMod.qo
qcc -c --context=qlib/BigMod/ qlib/BigMod/Foo.qc      # → Foo.qo
qcc -c --context=qlib/BigMod/ qlib/BigMod/Bar.qc      # → Bar.qo
...   # one .qo per .qc, independently buildable

qcc -m --from-objects qlib/BigMod/*.qo -o BigMod.qmod
```

Incremental rebuild: touch one `.qc` → rebuild one `.qo` → relink the
`.qmod`. Parallel build: `make -j` on the per-file `.qo` compiles.

### Shared mechanism

The same `.qo` format, the same register-function convention, the same
metadata aggregation rules serve both use cases. A `.qo` is a
relocatable ELF object emitted by LLVM, carrying:

- The compiled functions/methods defined in its source file.
- Metadata describing *only its own contributions* (not the full
  module/program).
- External references to everything else — runtime helpers
  (`qore_rt_*`), cross-file classes, cross-file globals.
- A per-file register function that registers *its* contributions when
  called.

Link-time aggregation (either into a `.qmod` or into a C++ executable)
stitches these per-file contributions into a coherent Qore runtime
state.

## What already exists

Current `qmod` emission (see `lib/QoreAOT.cpp::emitObjectFile`) already
produces an ELF relocatable via `llvm::CodeGenFileType::ObjectFile`.
The `.qmod` wrapping adds a small stub so libqore's module loader can
dlopen it; at the bitcode level, it *is* an ELF object.

`generateModuleABIV2` populates:

- **Exported symbols** (`ExternalLinkage` + DLL-export):
  - `qore_module_name`, `qore_module_version`, `qore_module_description`,
    `qore_module_author`, `qore_module_url`, `qore_module_license_str`,
    `qore_module_license`, `qore_module_api_major`, `qore_module_api_minor`.
- **Private globals:**
  - `qore_aot_mod_metadata` — compressed serialized class/function/etc table.
  - `qore_aot_mod_label`, `qore_aot_mod_name`, `qore_aot_mod_funcs`.
- **Init helpers (declarations, resolved against libqore at runtime):**
  - `qore_aot_module_init_v3(meta, len, label, po_lo, po_hi, name,
    funcs, n)` — registers classes/functions in the running program.
  - `qore_aot_module_ns_init(root_ns, qore_ns)` — namespace hookup.
  - `qore_aot_module_delete()` — module unload hook.
- **Per-function code** — private linkage `Qore$<Module>$<Class>$<Method>`.

Unresolved C references: `qore_rt_*` runtime helpers (resolved against
`libqore.so` / `libqore.a` at link time).

## Granularity: whole-module vs. per-file

qcc supports two `.qo` granularities:

| Mode           | Flag                  | Input              | Output          | Use case |
|----------------|----------------------|--------------------|-----------------|----------|
| Whole-module   | `qcc -c`             | single `.qm`       | one `.qo`       | single-file module linked into a C++ binary |
| Per-file       | `qcc -c --context=D` | `.qc` / `.qm` / `.q` with dir `D` as parse context | one `.qo` per file | split modules, multi-file applications (qorus-core) |

Per-file mode is a strict superset: a single-file module compiled with
`--context=.` yields the same `.qo` as whole-module mode would.

### Parse context — simplified by `%strong-encapsulation`

`PO_STRONG_ENCAPSULATION` (`%strong-encapsulation`, part of `%modern`,
required for compilation) **disallows out-of-line class and namespace
declarations** (see `lib/parser.ypp:1526-1527`). That means:

- Every class is defined in exactly one file, fully inline — no "open"
  classes extended across files.
- Every namespace declaration is self-contained in the file where it
  appears — no cross-file namespace merging beyond what each file
  explicitly declares.
- Cross-file type references resolve through ordinary name lookup
  against explicitly `%requires`-d modules, not through implicit
  merging.

For per-file `.qo` compilation this is a strong structural invariant:
**each file is a self-contained compilation unit.** qcc therefore:

1. **Loads the explicit dependencies** named in `%requires` directives
   in the source file (same as today's module load) — these give the
   parser the declarations it needs for type resolution.
2. **Parses and lowers just this one file** to LLVM IR. No
   whole-directory scan, no shared context beyond what `%requires`
   already declared.
3. **Emits metadata only for this file's contributions.** External
   classes/functions reached via `%requires` are referenced by name,
   resolved at register time.
4. **Emits a per-file register function** that registers only this
   file's contributions.

The `--context=dir/` flag still exists for split modules where several
files belong to the same module name and `%requires` wouldn't capture
their siblings, but the semantics are "also parse these sibling files
to populate the shared module-scope declarations" — not "do full
whole-program analysis." Within each file, `%strong-encapsulation`
keeps the boundaries clean.

**What this eliminates:**
- No risk of a `.qc` silently re-opening another file's class.
- No risk of file-ordering bugs within a single module — each file's
  contributions are fully described by its own text.
- No per-AST-node file-attribution pass needed — the existing parse
  scopes already give us the attribution for free under
  `%strong-encapsulation`.

**What remains:**
- `--context=dir/` mode must still agree on which files belong to the
  module, because the `%module{...}` block lives in one file while
  the rest contribute classes/functions. qcc reads the `.qm` for the
  module metadata, then compiles each `.qc` individually with that
  metadata as the declared context.

## Minimal design — "qmod minus the loader"

A `.qo` is the same ELF object that lives inside a `.qmod` today, with
four additions that make it directly linkable + initializable from C++:

### 1. Stable exported entry symbol

Every `.qo` exports a register function:

```c
// Whole-module .qo:
extern "C" void qore_<module>_register(QoreProgram* pgm);

// Per-file .qo (file basename sanitized: [A-Za-z0-9_] only):
extern "C" void qore_<module>_<file>_register(QoreProgram* pgm);
```

Each register call registers *only* that translation unit's
contributions — classes defined in this file, functions defined in
this file, constants/statics declared in this file. Cross-file
references are resolved at register time via libqore's namespace
lookup (the same path the runtime module loader uses today).

From C++:

```cpp
extern "C" void qore_HttpServer_register(QoreProgram*);
...
QoreProgram* pgm = qore_create_program(0);
qore_HttpServer_register(pgm);
pgm->run(argv);
```

For a multi-file Qore application (qorus-core):

```cpp
extern "C" void qore_qorus_jobrunner_register(QoreProgram*);
extern "C" void qore_qorus_scheduler_register(QoreProgram*);
extern "C" void qore_qorus_workflow_register(QoreProgram*);
...
QoreProgram* pgm = qore_create_program(0);
qore_qorus_jobrunner_register(pgm);      // order matches parse order
qore_qorus_scheduler_register(pgm);
qore_qorus_workflow_register(pgm);
```

**Registration order** for per-file `.qo`s follows parse order of the
original sources. Under `%strong-encapsulation` each file is
self-contained, but init expressions for constants / static variables
may still reference items in other files (via fully-qualified names
or `%requires`). Registering in parse order guarantees those names
resolve. qcc records the order in the per-module manifest and emits
a canonical aggregate glue object (see §4 "Archive format").

**One symbol per file**, not per function — function-level exports
remain private linkage in the `.qo`.

### 2. No global constructors

Do **not** use `__attribute__((constructor))`. Constructor ordering
across multiple translation units is fragile and interacts badly with
C++ static initializers in the host application. Require the
application to call each module's `register` function explicitly — same
model as C libraries that expose an `init()`.

`.qoa` archives can include a helper object that defines

```c
void qore_qoa_register_all(QoreProgram* pgm) {
    qore_HttpServer_register(pgm);
    qore_RestHandler_register(pgm);
    /* ... */
}
```

produced by qcc when building the archive. Application calls
`qore_qoa_register_all(pgm)` once.

### 3. Metadata accessor (debug/introspection)

```c
extern "C" const void* qore_<module>_get_metadata(size_t* out_len);
```

Returns a pointer to the `qore_aot_mod_metadata` blob so a host
application can introspect class/function lists without having to load
the module first. Useful for tooling; optional for runtime.

### 4. libqore helper API

Add to `include/qore/QoreAOT.h` (or similar public header):

```c
QoreProgram* qore_create_program(int64_t parse_options);
void         qore_destroy_program(QoreProgram*);
int          qore_run_callable(QoreProgram*, const char* fn_name,
                                const QoreListNode* args);
```

Thin wrappers around existing `QoreProgram::new` / `callFunction`. Host
C++ code uses these to drive a Qore program without touching the C++
`QoreProgram` class directly (keeps the ABI C-only → stable across
Qore minor versions).

## qcc flag: `-c` (compile, don't package)

```
# Whole-module (single .qm, no siblings):
qcc -c [--big-fn-threshold=N] [-g | --strip-debug-info] file.qm      # → file.qo

# Per-file within a split module or multi-file application:
qcc -c --context=dir/ dir/file.qc                                    # → dir/file.qo
qcc -c --context=src/ src/jobrunner.q                                # → src/jobrunner.qo
```

- `-c` emits a standalone `.qo` (ELF `.o`) instead of the `.qmod`
  wrapper. All other flags (`-O3`, `-g`, `--big-fn-threshold`, etc.)
  work identically.
- `--context=DIR` (per-file mode) tells qcc where to find the
  module's `.qm` (for metadata) and sibling `.qc` declarations needed
  to resolve cross-file names. In the absence of `--context`, qcc
  assumes the file is self-contained (e.g., a standalone `.q` script).
- Default output is `<basename>.qo` next to the input; `-o path`
  overrides.
- `--target=triple` remains supported for cross-compile.

### Per-file dependency tracking

For build-system integration (Make, Ninja, CMake), qcc emits a
Makefile-style depfile alongside the `.qo` when `-MMD` is passed —
mirroring gcc/clang. Each `.qo` depends on its source file plus any
`%requires`-d modules and (for per-file mode) the module's `.qm`
metadata file. A touched `.qc` invalidates only its own `.qo`.

## qcc flag: `-a` (archive / link-time aggregation)

Two archive targets share the same aggregator:

```
# Produce a C++-linkable static archive from per-file .qo's:
qcc -a -o mylib.qoa src/*.qo
    # → mylib.qoa (ar archive) + qore_qoa_register_all() glue object

# Produce a final .qmod from per-file .qo's (replaces monolithic qcc -m):
qcc -m --from-objects qlib/BigMod/*.qo -o BigMod.qmod
    # → BigMod.qmod with the per-file .qo's linked + module descriptor
    #   stitched by qcc
```

For `-a`: qcc invokes system `ar rcs` internally and generates a
`qore_qoa_register_all(pgm)` function that calls each `.qo`'s
per-file register in parse order. The order is recorded in a manifest
(`qcc.manifest` or embedded in a `.qo` produced by `qcc -c --manifest`)
that qcc consulted while compiling.

For `-m --from-objects`: qcc composes the `.qmod` by
- linking the listed `.qo`s into a single shared object,
- synthesizing the module-level register / namespace-init / delete
  entry points that call each per-file register in order, and
- emitting the module descriptor globals (`qore_module_name`, etc.)
  that libqore's loader looks up.

**MVP scope:** both `-a` and `-m --from-objects` are required for the
user's stated use cases (qorus-core linkage; speeding up large split
modules). Hand-rolled `ar` is a fallback for users who want it, not
the default.

## Symbol-naming convention

| Symbol kind                         | Scheme                                    |
|-------------------------------------|-------------------------------------------|
| Whole-module register entry         | `qore_<mod>_register`                     |
| Per-file register entry             | `qore_<mod>_<file>_register`              |
| Archive aggregate register          | `qore_qoa_register_all`                   |
| Module metadata accessor            | `qore_<mod>_get_metadata`                 |
| Module info globals                 | `qore_<mod>_module_name`, etc.            |
| AOT functions (private)             | `Qore$<Mod>$<Class>$<Method>` (unchanged) |
| Runtime helpers (external)          | `qore_rt_*` (unchanged)                   |

Names are sanitized: `A-Z a-z 0-9 _` allowed; all else → `_`. File
basename is taken without extension (e.g., `Foo.qc` → `Foo`).
Collision handling: if two files within a module sanitize to the same
basename, qcc errors at `-c` time. If two modules collide, the linker
catches it at `-a` / `-m --from-objects` time.

**Standalone scripts (`.q` for qorus-core sources):** treated like a
file in an anonymous module. Register symbol is `qore_<file>_register`
without a module prefix — the `--context=dir/` specifies the parse
directory but does not imply a module name unless a `.qm` is present.

Why prefix module globals with `qore_<mod>_` instead of the current
generic `qore_module_name`? Because multiple modules in the same binary
would collide on `qore_module_name` otherwise. The currently-exported
globals must be per-module-prefixed for static linkage. `.qmod` doesn't
hit this because only one `.qmod` is loaded at a time into any given
context; for `.qo` they coexist in the process image.

## Init ordering

The register function invokes init in this order:

1. `qore_aot_module_init_v3` — registers classes, builds function table.
2. `qore_aot_module_ns_init` — sets up namespace references (needs a
   root namespace from the pgm).
3. Constant/static-var init (class const, namespace const, module init)
   — today these run inside `executeInitFunctions`; must run here too.

**Dependency constraint:** if module B depends on module A, the host
application must call `qore_A_register(pgm)` before `qore_B_register(pgm)`.
In a `.qoa` built by qcc with dependency info, the generated
`register_all` reflects topological order.

**Failure mode:** if dependency order is wrong, `qore_B_register` raises
a `PARSE-EXCEPTION: cannot resolve class 'A::Foo'` — same error the
runtime module loader would raise, so no new failure surface.

## Integration with Phase 1 + Phase 5a

- `-g`, `--strip-debug-info` already propagate via env vars — work
  unchanged for `.qo`.
- `--big-fn-threshold=200` (now default) applies identically. .qo built
  from HS.qm benefits from the same 46x compile speedup.
- `--time-trace` supported identically.

## qorus-core integration (the headline use case)

Today qorus-core (at `~/src/qore/git/qorus`) uses `QoreProgram` from
C++ with Qore sources compiled at startup. Target:

1. Pre-compile qorus's bundled Qore sources to `.qo`.
2. Build `qorus.qoa` archive containing them.
3. Link qorus-core against `qorus.qoa -lqore`.
4. At startup, qorus-core calls `qore_qoa_register_all(pgm)` instead of
   parsing sources at runtime.

Benefits:
- **Startup latency:** no parsing / lowering / codegen at runtime.
- **Binary distribution:** one executable, no need to ship `.qm` files.
- **Debugging:** `.qo` can carry full DWARF if built with `-g`.
- **Security:** source not shipped to production nodes.

Out of scope for Phase 4 MVP: hot-reload of updated `.qo`s, runtime
`.qm` fallback if `.qo` missing, packaging via `qorus-core`'s build
system — those come later.

## Open questions

1. **Host-side API surface.** Should `qore_create_program` accept a
   full `hash<parse options>` or just a bitmask? Bitmask is simplest.

2. **`qore_rt_*` linkage.** Static-linking qorus-core against
   `libqore.a` (not `.so`) may be a later goal. Need to confirm
   `libqore.a` build path works — there's a `BUILD_STATIC_LIBQORE` flag
   in CMake but coverage is partial.

3. **DWARF for multi-`.qo` binaries.** Each `.qo` carries its own DWARF.
   When linked together, `ld` merges. Need to verify that debuggers
   (gdb, lldb) walk Qore symbols correctly in a merged binary.

4. **Version skew.** If `qorus.qoa` was built against Qore 2.3.0 but
   the runtime is 2.4.0, behavior? Emit a version check in the
   register function and abort/warn on mismatch. Probably runtime
   check + explicit opt-out flag.

5. **Static init of global vars.** Qore modules can have global
   mutables (`our int counter = 0;`). Those are set up inside
   `qore_aot_module_init_v3`. Re-running register on a fresh pgm
   should reset them — need to confirm no leak of state between
   QoreProgram instances.

6. **Per-file vs whole-module metadata.** Can `qore_aot_module_init_v3`
   be called repeatedly (once per `.qo` within the same module) to
   append that file's contributions, or does it assume one-shot
   per-module registration? If one-shot, qcc needs to synthesize an
   aggregated metadata blob at `-m --from-objects` time — which is
   straightforward but must be specified.

7. **Cross-file init dependencies.** A class constant in file B that
   references a constant in file A is legal under
   `%strong-encapsulation` (both are fully-qualified references). The
   init-expr runs at register time, so as long as A's register ran
   first, the lookup succeeds. qcc must encode parse order in the
   aggregate glue — confirm this matches current
   `executeInitFunctions` ordering.

## Deliverables

- `qcc -c` flag + `--context=DIR` support + `-MMD` depfile emission.
- Per-file register-function emitter (new helper in
  `generateModuleABIV2`, callable in whole-module or per-file mode).
- Per-module prefixing of info globals so multiple `.qo`s coexist.
- `qcc -a` archive builder + `qore_qoa_register_all` glue emitter.
- `qcc -m --from-objects` linker/aggregator for building a `.qmod`
  from per-file `.qo`s (replaces monolithic split-module compile).
- New public C API in `include/qore/QoreAOT.h`.
- Test cases:
  - minimal single-file `.qm` → `.qo` → linked into C++ `main`;
  - multi-file split module → per-file `.qo`s → combined `.qmod`
    (verify metadata matches monolithic build, runtime behavior
    identical);
  - multi-file application (mimics qorus-core) → per-file `.qo`s →
    linked C++ binary, calls Qore functions across files.
- Documentation + `qorus-core` integration example.

## Phase 4 vs. the rest

Orthogonal to Phases 2/3/5b/6/7. Can be developed in parallel. The
shipped work in Phases 1 + 5a benefits `.qo` for free.

Interaction with **Phase 2 (caching)**: per-file `.qo` compilation
effectively *is* a form of caching — if the user's build system (Make,
Ninja) tracks file-level dependencies, unchanged `.qc`s don't rebuild.
This may reduce the urgency of a qcc-native cache; the build system
handles it.

Interaction with **Phase 3 (parallel codegen)**: per-file `.qo`
compilation is embarrassingly parallel at the build-system level —
`make -j` achieves the Phase 3 goal without qcc needing thread-pool
infrastructure. Another reason Phase 3 urgency is reduced.

## Entry point for implementation

Recommended order:

1. **MVP: `qcc -c` for a self-contained single-file source.** Emits a
   `.qo` with one per-file register function. Integration test: tiny
   C++ `main()` links against it and calls a Qore function. This
   validates the ELF/link model.
2. **Per-module info-global prefixing.** Prerequisite for linking
   multiple `.qo`s into one binary.
3. **`--context=DIR` support** for per-file compilation within a
   module (`.qc` files).
4. **`qcc -m --from-objects`** — unblocks "speed up large split-module
   builds" use case. Combined with `make -j` this gives a huge dev-loop
   win for modules like HttpServer / SqlUtil.
5. **`qcc -a` + `qore_qoa_register_all` glue** — unblocks qorus-core
   linkage.
6. **`include/qore/QoreAOT.h` public API** — finalize the C ABI once
   concrete integration tests exist.
7. **qorus-core integration trial** — end-to-end validation with the
   real target binary.
