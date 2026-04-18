# Phase 4 Slice 10 — Script-Context AOT

**Status:** Design. Depends on slices 4-8 (landed).
**Author:** AOT team, 2026-04-18.
**Sister doc:** `design/aot-phase4-qo-object-files.md` (modules).

## Motivation

Slices 4-8 added AOT support for Qore **modules** — source directories
containing a `<mod>.qm` with `%module { name: …; version: …; … }`
metadata, loadable at runtime via `%requires <mod>`. A C++ host can
consume them via `qcc -a` archives + `qore_qoa_register_all`.

**Qorus's qctl binary (and siblings like qbugreport)** does not match
that shape. `bin/qctl` is a 3,061-line `%exec-class QCtl` script, and
its 23 supporting `Classes/*.qc` + `lib/*.ql` files are plain Qore
sources — no `.qm`, no `%module` block, no `%requires qctl`. Today
Qorus's `qorus_qore2cpp` cmake macro embeds each source as zlib bytes
in a C++ TU; the `qctl_main.cpp` host decompresses + parses them at
every launch (~6 s wall-clock for `qctl --help`, 170 MB RSS peak).

Slice 10 fills the gap: pre-compile multi-file Qore **applications**
to `.qo` objects + an aggregator glue that a custom C++ host (like
`qctl_main.cpp`) calls once at startup, skipping the parse-and-codegen
phase entirely. Expected qctl startup post-integration: 200-500 ms
(10-30× faster; extrapolated from slice 6's AsyncSocketIo register-vs-
parse ratio).

## Goals

- `qcc -c` can compile ONE file of a multi-file Qore application, with
  the rest of the application providing parse context — **without
  requiring a `.qm` module header**.
- `qcc` can emit an aggregator `.o` carrying metadata + a single
  `qore_<app>_script_register(QoreProgram*)` entry point.
- A C++ host links the aggregator `.o` + per-file `.qo`s and calls
  the script-register fn once; every class/function becomes
  runtime-callable on the same `QoreProgram*`, including non-public
  items (unlike module-wrapped code where only public items propagate
  to `tpgm`).

## Non-goals

- **No auto-generated C++ main.** Hosts keep their custom `main()`
  (qctl_main.cpp has bespoke Qorus init: signal handling, module
  paths, DB option injection, `Qorus` namespace population). Slice 10
  supplies the glue only.
- **No changes to module semantics** from slices 4-8 (modules stay
  loadable via `qcc -m --from-objects` / `qcc -a` exactly as today).
- **No source-less aggregation** — the aggregator still re-parses the
  source set for metadata (same MVP tradeoff as slice 6).
- **No hybrid parse+AOT** path. Script mode is pure AOT: no
  `parsePending`, no embedded source, no fallback — if a function
  wasn't AOT-compiled, it's not callable.

## Design

Three pieces, all **additive** to slices 4-8. No existing behavior
changes.

### Piece 1 — script-context per-file compile

New qcc mode:

```bash
qcc -c --script-manifest <manifest.txt> -o <out>.qo <target>
```

- `<manifest.txt>` — plain-text, one file path per line, parse order
  preserved. Blank lines and `# comments` ignored. Paths are resolved
  relative to the manifest file's directory.
- `<target>` — one of the files listed in the manifest; the single
  file whose items will be lowered to native code and emitted in the
  output `.qo`.
- Mutually exclusive with `--context=DIR` (module mode).

**Semantics** (vs slice 4's `--context=DIR`):

| | Module mode (slice 4) | Script mode (slice 10) |
|-|-|-|
| Parse options | `PO_IN_MODULE \| PO_NO_TOP_LEVEL_STATEMENTS \| PO_REQUIRE_PROTOTYPES \| PO_REQUIRE_OUR` (+ defaults) | Plain defaults (caller's host sets PO via `parseSetParseOptions` before register) |
| Module context helper | `QoreUserModuleDefContextHelper` wraps parses | None — each file parses at top level |
| `%module { … }` block | Required in `<mod>.qm` | Forbidden in any source |
| `%exec-class X` | Illegal | Legal (single source may carry it, typically the "main") |
| Top-level code | Illegal | Legal (runs at `runClass`/`run` time after register) |
| Non-public items | Hidden from tpgm at register | Visible to tpgm at register (see Piece 3) |

**Compile-time filtering** reuses slice 4's `compile_file` parameter
on `compileNamespaceFunctions` — no new filtering infrastructure.

**Emitted `.qo` content** — same shape as slice 5 fragment `.qo`s:

- Compiled LLVM functions for the target file's items only
  (`ExternalLinkage`, AOT signature `i64 (ptr ctx, ptr xsink)`).
- Fragment symbols (slice 5): `qore_<sanapp>_<sanfile>_fragment_data`
  + `qore_<sanapp>_<sanfile>_fragment_order`. `<sanapp>` defaults to
  the manifest's basename (without extension) unless `--app-name=<n>`
  overrides.
- NO module-info globals, NO module descriptor, NO
  `qore_<mod>_register`, NO `qore_qoa_register_all`.

### Piece 2 — aggregator glue

New qcc mode:

```bash
qcc --app-glue --script-manifest <manifest.txt> [--app-name <name>] -o <out>.o
```

- Parses every manifest file into one `QoreProgram`.
- Runs `compileNamespaceFunctions(..., metadata_only=true)` — slice 6's
  codegen-skipping path — producing forward-decl LLVM Functions
  matching every per-file `.qo`'s external symbols.
- Serializes the aggregated namespace tree + slot maps + init-funcs
  via the existing `QoreAOTBinaryWriter` pipeline (same format as
  modules).
- Emits one exported function:

```c
extern "C" DLLEXPORT void qore_<sanapp>_script_register(QoreProgram* tpgm);
```

whose body calls the new runtime entry point (Piece 3) with the
embedded metadata blob + function table.

**No** module-info globals, module descriptor, or
`qore_qoa_register_all` are emitted. Glue is single-purpose: set up
one specific application in a caller-supplied `QoreProgram`.

Output: a single `<out>.o` relocatable object. Link together with
per-file `.qo`s and the host's C++ TUs to produce the final
executable:

```bash
g++ -o qctl qctl_main.cpp.o qctl_glue.o *.qo -lqore
```

### Piece 3 — runtime entry point `qore_aot_script_register`

New C ABI (added to public `include/qore/QoreAOT.h`, slice 8's
header):

```c
DLLEXPORT int qore_aot_script_register(
    QoreProgram* tpgm,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    const QoreAOTFunc* functions, int num_functions);
```

**Semantics** (vs slice 3's `qore_aot_register_into_program`):

- Does **not** create a shadow `QoreProgram`.
- Does **not** insert an entry into `aot_module_map[name]`.
- Deserializes metadata directly into `tpgm`'s root namespace tree
  — classes, functions, constants, globals, typedefs all land on
  `tpgm` regardless of public/non-public marking.
- Registers functions into `tpgm`'s function lookup (same
  mechanism modules use, but bound to `tpgm` not a shadow program).
- Runs init functions (namespace constants, class constants,
  static-var initializers, module-init closures) in serialized
  order against `tpgm`. Any exception → returned as status code
  (non-zero); exception details go to stderr via
  `ExceptionSink::handleExceptions`.

**Implementation**: factor the shared "deserialize blob into program"
body out of `qore_aot_module_init_v3` into a helper (e.g.
`deserializeAOTMetadataInto(pgm, blob, …)` in `QoreAOTRuntime.cpp`).
Module path calls it against a shadow pgm then hands off to
`ModuleManager`; script path calls it against `tpgm` and returns.
Estimated ~50 LOC of runtime code + a refactor of the existing
~500-line `module_init_v3` body to extract the helper.

### Why not just reuse the module-register path?

We considered synthesizing a fake module name (`"qctl"`) and letting
`qcc -a` + `qore_qoa_register_all` handle qctl as if it were a
module. Two blockers:

1. **Non-public items stay in the shadow pgm.** qctl's `Classes/`
   uses plain `class X { … }` (not `public class`). Module
   registration's `scanMergeCommittedNamespace` copies only
   user-public items into `tpgm` (see `session_2026_04_17_p67` memory
   entry). Making qctl work under module semantics would require
   adding `public` to every class — invasive, and changes the
   language-level contract.
2. **`aot_module_map["qctl"]` pollution.** A fake module entry would
   create a `%requires qctl` that pretends to work but has weird
   semantics. Script mode is the cleaner contract.

## qctl integration pattern (separate slice 11, after slice 10 lands)

1. **Generate manifest at build time.** Qorus's CMake has the ordered
   file list already in `QCTL_SOURCES + QORUS_SHARED_SRC + …`. A
   small cmake helper writes them one per line to
   `${CMAKE_BINARY_DIR}/qctl.manifest`.
2. **Per-file `.qo` via `add_custom_command`.** For each source, add
   a rule `qcc -c --script-manifest ${QCTL_MANIFEST} -o <src>.qo
   <src>`.
3. **Aggregator glue.** One more custom command:
   `qcc --app-glue --script-manifest ${QCTL_MANIFEST} --app-name qctl
   -o qctl_glue.o`.
4. **`add_executable(qctl qctl_main.cpp qctl_glue.o ${QCTL_QO_LIST})`**
   — drops `QCTL_QORE_SRC_X` + the 5 `*_LIB` object-bundle targets.
5. **`exec/qctl_main.cpp` changes** (line-by-line diff expected ~50
   deletions + 5 additions):
   - Delete 24 `extern void qorus_<name>(…);` declarations.
   - Delete 24 `qorus_<name>(qorus_dbg.internals, qpgm, &xsink);`
     call sites.
   - Add `extern "C" void qore_qctl_script_register(QoreProgram*);`.
   - Replace the 24-call block with one `qore_qctl_script_register(qpgm);`.
   - Remove the `int_modules[]`-loop (empty today; kept as dead code
     with a comment — safe to delete).
6. **No other Qorus source changes.** `parseSetParseOptions`,
   `setScriptPath`, Qorus namespace injection, `parseCommit`,
   `runClass("QCtl")` — all untouched.

Validation for slice 11:
- `qctl --help`: wall-clock before/after, target < 500 ms.
- `qctl --version`: unchanged (fast-path before register).
- `qctl ps` / `qctl status` / `qctl ping`: behavior matches
  installed `/home/david/src/Qorus/current/bin/qctl`.
- Qorus test suite (existing CI): green.
- RSS at startup: expect ≤ 100 MB (vs 170+ MB today).

## Open design questions

### Q1. Parse-options propagation

qctl_main.cpp calls `qpgm->parseSetParseOptions(QORUS_PARSE_OPTIONS)`
before parsing any source. Each source file-level directive
(`%new-style`, `%strict-args`, `%require-types`, `%enable-all-warnings`)
OR's into tpgm during `parsePending`.

For slice 10, the aggregator's metadata header records the PO that
was effective at compile time. Three options:

- **(a) Enforce superset**: `qore_aot_script_register` fails if
  `tpgm.PO` is not a superset of the metadata's PO. Clear contract;
  may trip up hosts whose PO is set mid-startup.
- **(b) OR-merge**: script-register adds metadata PO bits to tpgm's
  PO in-place. Matches today's behavior (file-level directives
  widen tpgm's PO). Risk: silent widening past what the host
  expects.
- **(c) Ignore metadata PO**: tpgm's PO stays authoritative;
  assume the host knows what it's doing. Simplest; places the burden
  on integrators.

**Recommendation**: **(b) OR-merge**, matching existing parsePending
semantics. Log a note if the merge changed any bits (debug-mode
only).

### Q2. Parse-commit timing

qctl_main.cpp runs one `parseCommit` after all 24 `parsePending`
calls to resolve forward refs and finalize the program. With AOT
script mode the "parse" is already committed into the metadata blob
at compile time — forward refs were resolved then.

But `parseCommit` also triggers runtime-side hooks (type finalization,
classlist lock-down, etc.) that the register path may or may not
replicate. **Action**: check `qore_aot_module_init_v3`'s internal
teardown to see if it effectively `parseCommit`s its shadow program.
Script mode should match that behavior.

Leave `qpgm->parseCommit(&xsink, …)` in `qctl_main.cpp` as a no-op
safety net; verify removal is safe in slice 11.

### Q3. `qore_aot_script_register` in public header?

Yes — add to `include/qore/QoreAOT.h` (slice 8). Users with
hand-crafted hosts can call it directly, skipping the
`qore_<app>_script_register` wrapper. The wrapper exists primarily
for **symbol namespacing** (a binary can link multiple apps; each
has its own named register fn) and for **opaque metadata
embedding** (host doesn't need to know the blob bytes).

### Q4. Multiple apps in one binary?

Legal: each `qcc --app-glue` run emits a differently-named
`qore_<app>_script_register`. A host links several and calls them
against the same or different `QoreProgram`s. Unlike `qcc -a`'s
`qore_qoa_register_all` (one per binary, slice 7 limitation),
script-mode has no such constraint.

### Q5. Per-file `.qo` reuse across apps?

If two apps share a manifest entry (e.g., `lib/qorus.ql` is in both
`qctl.manifest` and `qbugreport.manifest`), their `.qo`s have
distinct `qore_<app>_<file>_fragment_*` symbols. Sharing requires
the manifests to use the same `--app-name`, or pre-building
per-file `.qo`s once with a shared app name and only re-running the
aggregator per app. For MVP: **no sharing** — each app rebuilds
its own `.qo`s. (Qorus's build already does per-source rebuilds, so
this isn't a regression.)

### Q6. Init-closure handling

Some sources have `%module { init: sub () { … } }`-style init
closures (not applicable here — scripts don't have `%module`) or
top-level statements that act as initialization. For scripts,
top-level statements are legal and run at `tpgm->run()` /
`tpgm->runClass()` time, after register. AOT doesn't need special
handling — the top-level statement becomes a compiled function in
the script's metadata + runs as part of the normal program flow.

## Implementation scope

- **`lib/QoreAOT.cpp`**: ~300 LOC of additions.
  - `parseScriptManifest(path)` helper — reads manifest, returns
    `std::vector<std::string>` of absolute paths.
  - `QoreAOT::compileScriptFile(manifest_files, manifest_dir,
    target_file, output_path, …)` — mirrors slice 4's
    `compileSeparatedModuleFile` but without module context.
  - `QoreAOT::compileScriptGlue(manifest_files, manifest_dir,
    app_name, output_path, …)` — mirrors slice 6's
    `compileModuleFromObjects` but emits `qore_<app>_script_register`
    instead of `generateModuleABIV2(compile_only=false)`.
  - Helper `emitScriptRegisterFunction(ctx, module, sanitized_name,
    metadata_gv, funcs_gv)` — emits the exported wrapper.
- **`lib/QoreAOTRuntime.cpp`**: ~60 LOC.
  - Factor shared deserializer body out of `qore_aot_module_init_v3`
    into `deserializeAOTMetadataInto(pgm, …)`.
  - New `qore_aot_script_register(tpgm, metadata, …)` calls the
    helper against tpgm, skips `aot_module_map` insertion, runs
    init_funcs, returns int status.
- **`qcc-main.cpp`**: ~50 LOC.
  - New flags `--script-manifest=<path>`, `--app-glue`,
    `--app-name=<name>`.
  - Dispatch to `compileScriptFile` / `compileScriptGlue`.
  - Validation: `--script-manifest` is mutually exclusive with
    `--context`; `--app-glue` requires `--script-manifest`.
- **`include/qore/QoreAOT.h`**: ~10 LOC.
  - Public `qore_aot_script_register` declaration.
- **Tests**: ~150 LOC of new test files.
  - `examples/aot/qoa_script_test.cpp` — C++ host that
    `qore_aot_script_register`s a tiny 3-file app and calls a
    function.
  - `examples/aot/qoa_script_test.sh` — build script (per-file
    compile → glue → link → run).
  - Toy app under `examples/aot/script_demo/` — main.q with
    `%exec-class Demo`, lib.qc with a helper class, util.ql with a
    free function.

Total: ~500-600 LOC of additions, ~50 LOC of refactor. **~1 day** of
focused work. Zero changes to slice 4-8 externally visible behavior.

## Validation plan

1. Baselines green: JITSmoke 156/156, AOTSmoke 89/89, IRExecMode 4/4,
   LValuePathSmoke 13/13.
2. Slice 3, 5, 6, 7 harnesses unchanged.
3. New `qoa_script_test.sh` passes:
   - Toy app parses cleanly through `qore main.q` (reference).
   - Same app built through slice 10 pipeline → C++ host calls
     `qore_demo_script_register(pgm)` + `qpgm->runClass("Demo")` →
     observable output matches the reference run.
   - No unresolved symbols; 0 duplicate symbols in `ld -r` combined
     object.
4. Before-and-after timing on toy app: AOT register ≤ 10 ms; source
   parse baseline measured for comparison.
5. Standalone header selftest (slice 8): `<qore/QoreAOT.h>` still
   compiles standalone after adding the new declaration.

## Post-slice-10 sequence

Slice 10 merges. Then:

- **Slice 11**: qctl integration (the actual qorus-side trial —
  CMake changes + ~55-line diff to `qctl_main.cpp`).
- **Slice 12** (if slice 11 succeeds): extend to `qbugreport`
  (sister binary, same build shape, tiny incremental work).
- **Slice 13** (stretch): extend to the larger `qwf`/`qsvc`/`qjob`/
  `qdsp` binaries. Each has more bundled Qorus sources; same
  integration shape. Main worry is codegen time for a 100k-LOC-class
  app — may need slice 6b (fragment merging, source-less) to be
  practical.
