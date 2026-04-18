# Phase 4 Slice 10 — Script-Context AOT (C/C++-like UX)

**Status:** Design, v2. Depends on slices 4-8 (landed).
**Author:** AOT team, 2026-04-18.
**Supersedes:** earlier "manifest-based" draft of this file.
**Sister doc:** `design/aot-phase4-qo-object-files.md` (modules).

## Motivation

Slices 4-8 added AOT support for Qore **modules** (directory + `.qm`
entry file). They don't cover multi-file Qore **applications** like
Qorus's `qctl` — 24 loosely-grouped `.q`/`.qc`/`.ql` files sharing a
single `QoreProgram` at runtime, driven by a custom C++ `main()`.
Today's `qctl --help` takes ~6 s at every launch because all 24
sources parse + JIT-codegen on each startup.

Slice 10 delivers AOT for such apps with a UX that mirrors the
universally-understood C/C++ build model:

```bash
qcc -c bin/qctl                               # → qctl.qo
qcc -c Classes/ClientProcessBase.qc           # → ClientProcessBase.qo
...
qcc -o qctl qctl_main.cpp *.qo -lqore         # link executable
```

Or in batch:

```bash
qcc -o qctl qctl_main.cpp bin/qctl Classes/*.qc lib/*.ql
```

No manifest files. No `--context=DIR`. No `--script-root`. Just
`cc -c` + `ld` semantics for Qore.

## Goals

- **`.qo` is a unified object+header** — carries compiled LLVM code
  AND a declaration table usable at compile-time by subsequent `qcc -c`
  invocations.
- **`qcc -c foo.qc -L<dir>`** parses `foo.qc` with sibling `.qo`s'
  declarations preloaded, so cross-file type references resolve.
- **`qcc -o binary *.qo [main.cpp] [-lqore]`** links per-file `.qo`s +
  optional C++ host + libqore into a native executable.
- **Custom C++ hosts welcome, auto-host also supported.** If
  `main.cpp` is supplied, qcc links against it. If not, qcc synthesizes
  a minimal `main()` that creates a `QoreProgram`, registers all
  linked `.qo`s, runs top-level / `%exec-class`, exits.
- **Multi-file deserializer** replaces the current single-blob
  `deserializeIntoProgram` — all existing callers migrate transparently
  via a 1-blob wrapper. New callers (slice 10's compile-time preload,
  slice 6's future source-less aggregator) use the multi-file API.

## Non-goals

- **No auto-generated C++ main for hosts that want customization**
  (qctl keeps its bespoke `qctl_main.cpp`). Auto-main is the
  convenience default, not a replacement.
- **No changes to slice 4-8 module semantics**. Modules keep their
  `.qm` entry, their `qcc -c --context=DIR`, their `qcc -a`/`-m
  --from-objects` aggregators. Script mode is an additive third
  path.
- **No source-level forward-declaration syntax.** Qore stays as-is;
  the build-time machinery plays the `.h`-file role via `.qo`
  decl-table preload.

## Architecture

### 1. `.qo` = object + header

Every slice-10 `.qo` carries:

| Section | Role | Already in slice 5? |
|---|---|---|
| Native LLVM code | This file's functions/methods — `.o` role | Yes |
| Declaration table (classes, typedefs, hashdecls, enums, constants, globals, functions — full signatures) | `.h` role for downstream `qcc -c` | Mostly — slice 5's fragment blob already serializes namespace trees; we widen it to include non-public items (deserializer already handles them) |
| Reference table (external symbols this file's parser consulted) | Link-time resolution + depfile emission | **New** — small addition to the binary format |
| `%requires` table (module deps) | Link-time union + runtime load | **New** — small addition |
| Fragment accessors (slice 5) | Discoverable via ELF symbols | Yes |

The reference table doesn't change compile-time parsing — it's a
record of what was resolved, for use at link time and by `-MMD`
depfile emission.

### 2. `qcc -c foo.qc -L<dir>` — per-file compile with decl preload

```
qcc -c [-L <dir>]* [-MMD] -o foo.qo foo.qc
```

Flow:

1. Create a fresh `QoreProgram` for the compile (parse options:
   `%modern` defaults).
2. For each `-L <dir>`, scan for `*.qo` files.
3. For each found `.qo`, read its decl-table blob via the fragment
   accessor and feed it to the **multi-file deserializer** (phase 1
   — create shells).
4. After all preloads: multi-file deserializer phase 2 — cross-file
   resolution of base classes, member types, etc.
5. Parse `foo.qc` into the program. Parser's `parseFindClassIntern` /
   `parseFindFunctionEntryIntern` / etc. find the preloaded decls in
   the namespace tree; references to sibling classes / functions /
   types resolve transparently.
6. Compile `foo.qc`'s contributions to LLVM (slice 4 per-file filter).
7. Emit `foo.qo` with:
   - Native code
   - Decl table for items defined in `foo.qc`
   - Reference table listing every sibling symbol the parser
     consulted (by fully-qualified name)
   - `%requires` table from file-level + inherited directives
   - Fragment accessors
8. If `-MMD`, emit `foo.d` listing every sibling `.qo` whose decls
   were actually consumed (a subset of `-L <dir>` inputs —
   `ld`-style).

**Parallelism**: `make -j` parallelizes per-file `qcc -c`. Each
invocation re-scans `-L <dir>`s (cheap — deserialize is fast, codegen
is the expensive phase). Depfile tracks real consumption → `make`
rebuilds only what's affected when sibling `.qo`s change.

### 3. Multi-file deserializer

New class in `lib/QoreAOTBinary.cpp`:

```cpp
class QoreAOTBinaryMultiDeserializer {
public:
    explicit QoreAOTBinaryMultiDeserializer(QoreProgram* pgm);

    //! Phase 1: open a blob and create shells (namespaces,
    //! class/hashdecl/enum/typedef names). Returns session index
    //! on success, -1 on error.
    int addBlob(const uint8_t* data, uint32_t size, std::string& err);

    //! Phase 2: run cross-blob resolution — base classes, member
    //! types, static members, constants, functions, methods. Must
    //! be called after all addBlob() calls for this batch.
    bool resolveAll(std::string& err);

    //! Convenience: single-blob shortcut equivalent to
    //! addBlob() + resolveAll().
    bool deserializeSingle(const uint8_t* data, uint32_t size,
        std::string& err);
};
```

Refactor:

- **Split** the existing `deserializeIntoProgram(pgm, data, size, err)`
  (QoreAOTBinary.cpp:4164) into phase-1 (shell creation) and phase-2
  (resolution) halves. Wrap both in `deserializeSingle`.
- The existing `deserializeIntoProgram` becomes a 2-line wrapper
  that constructs a temporary multi-deserializer. Zero change to
  the 5 current callsites in `lib/QoreAOTRuntime.cpp`.
- Phase 1 covers: `deserializeNamespaces`, `deserializeClasses`
  (shell only — name + path, defer bases), `deserializeHashDecls`
  (shell — name + namespace, defer members), `deserializeEnums`
  (shell), `deserializeTypedefs` (shell — name only, defer type
  resolution).
- Phase 2 covers: `resolveClassBases`, `resolveTypedefs`,
  `resolveEnumBaseTypes`, `resolveHashdeclMembers`,
  `resolveInstanceMembers`, `resolveStaticMembers`,
  `resolveClassConstants`, then constants / globals / functions /
  methods.
- Per-session state (reader, pending-class map, pending-base lists,
  etc.) is kept in a `DeserializeSession` struct owned by the
  multi-deserializer. Phase 2 iterates all sessions in insertion
  order, then does a final cross-session resolution pass.

Deserialize-dedup behavior (existing at QoreAOTBinary.cpp:4349, 4449)
already handles the case where shell-phase for session B finds a
class already created by session A. Becomes load-bearing under multi-
file mode.

### 4. `qcc -o binary *.qo [main.cpp] [-lqore]` — linker

```
qcc -o <binary> [main.cpp] *.qo [-lqore] [--entry=<fn>|--exec-class=<class>]
```

Behavior:

1. Classify each positional arg: `.qo` vs `.cpp` vs `.o` vs `.a`.
2. For each `.qo`, load its declaration + reference + `%requires`
   tables from the embedded metadata.
3. Cross-check references: every reference-table entry must resolve
   to a decl-table entry in SOME linked `.qo`, or to a symbol that'll
   come from a linked `%requires` module. Fail the link otherwise
   (analog of C's "undefined reference" link error).
4. Union the `%requires` tables — dedupe by module name.
5. Synthesize a glue `.o` that:
   - Embeds aggregated metadata (same `QoreAOTBinary` format slices
     4-8 already use).
   - Exports `qore_<binary-basename>_script_register(QoreProgram*)`.
   - If **no `main.cpp`** supplied, emits a default `main()`:
     ```cpp
     int main(int argc, char* argv[]) {
         qore_init(QL_GPL, "UTF-8", true);
         QoreProgram* pgm = qore_create_program(
             PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
         // load %requires modules (deduped)
         ExceptionSink xsink;
         for (const char* mod : all_requires) {
             MM.parseLoadModule(mod, pgm);
             /* error check */
         }
         // register all .qo's decls + function tables
         qore_<binary>_script_register(pgm);
         int rc = 0;
         if (<--exec-class is set>) {
             pgm->runClass("<exec-class-name>", &xsink);
             rc = xsink.isException() ? 1 : 0;
         } else if (<--entry is set>) {
             rc = qore_run_callable(pgm, "<entry-fn>", nullptr);
         } else {
             pgm->run(&xsink);
             rc = xsink.isException() ? 1 : 0;
         }
         qore_destroy_program(pgm);
         qore_cleanup();
         return rc;
     }
     ```
   - If `main.cpp` IS supplied, no default `main()` — the host's
     `main()` takes over. The host calls
     `qore_<binary>_script_register(pgm)` explicitly.
6. Invoke `g++` / `ld` on glue `.o` + input `.qo`s + optional
   `main.cpp`.o + `-lqore` + user `%requires`' needed shared libs.
7. Strip glue `.o` (intermediate).

Under the hood `qcc -o` is just: generate glue, delegate to `ld`.
Nothing mysterious.

### 5. `qcc -o binary src1 src2 …` — batch mode

Convenience wrapper. qcc:

1. For each source, call `qcc -c` to produce a `.qo` (cached in
   `$TMPDIR` or `./.qcc-cache/`).
2. Invoke the linker as above.

No parallelism inside one qcc invocation (user runs `make -j` for
that). Batch mode is the one-liner escape hatch for tiny apps; real
builds go through per-file `.qo` + link.

### 6. `qore_aot_script_register` runtime

New public C API (extends slice 8's `include/qore/QoreAOT.h`):

```c
DLLEXPORT int qore_aot_script_register(
    QoreProgram* tpgm,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    const QoreAOTFunc* functions, int num_functions);
```

Semantics (vs slice 3's module-centric `qore_aot_register_into_program`):

- Does NOT create a shadow module program.
- Does NOT insert into `aot_module_map[name]`.
- Calls `deserializeIntoProgram` directly on `tpgm`.
- Registers functions into `tpgm`'s function lookup.
- Runs init functions (constant / static-var inits) against `tpgm`
  in serialized order.
- Returns 0 success, non-zero error; exceptions to stderr.

Implementation ~50 LOC — most of it factored out of
`qore_aot_module_init_v3`'s existing body (there's already a
`deserialize + register + init` sequence in there; we extract the
middle and skip the shadow-pgm prelude).

## User-visible workflow summary

**Tiny app (one source, no deps between sources)**:

```bash
qcc -o hello hello.q                    # batch compile+link
./hello
```

**App with cross-file deps (qctl-shaped)**:

```bash
# Per-file compiles (parallel via make -j)
qcc -c -L. bin/qctl
qcc -c -L. Classes/ClientProcessBase.qc
qcc -c -L. Classes/AbstractLogger.qc
...

# Link
qcc -o qctl qctl_main.cpp *.qo -lqore
```

Or, since the user already has a CMakeLists.txt with source lists:

```cmake
# CMake has the source list in QCTL_ALL_QORE_SOURCES.
foreach(src IN LISTS QCTL_ALL_QORE_SOURCES)
    get_filename_component(stem "${src}" NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${stem}.qo
        COMMAND qcc -c -L${CMAKE_BINARY_DIR} -MMD
                    -o ${CMAKE_BINARY_DIR}/${stem}.qo
                    ${CMAKE_SOURCE_DIR}/${src}
        DEPENDS ${CMAKE_SOURCE_DIR}/${src}
    )
    list(APPEND QCTL_QO_FILES ${CMAKE_BINARY_DIR}/${stem}.qo)
endforeach()

add_executable(qctl exec/qctl_main.cpp ${QCTL_QO_FILES})
```

**Nothing about manifests, parse context, module shape, or Qore-
specific search paths in the CMake.** Just `qcc -c → .qo`, `qcc -o
binary .qo... main.cpp`. Matches every C/C++ project's CMake
structure.

## Implementation scope

| Piece | LOC | Notes |
|---|---|---|
| `.qo` metadata: references table + `%requires` table | ~120 | Adds two new sections to the `QoreAOTBinary` writer/reader. Non-breaking extension. |
| `.qo` metadata: ensure full decl table (public + non-public) | ~40 | Relax slice 5's implicit "per-file contributions" filter when emitting decl sections. Keep it for fragment slicing when caller explicitly wants that. |
| Multi-file deserializer refactor | ~250 | Split `deserializeIntoProgram` into phase-1 / phase-2; wrap existing API as 1-blob shim. |
| `qcc -c`: `-L` flag, sibling `.qo` auto-discovery + preload, `-MMD` depfile emission | ~200 | New driver logic. Calls the multi-deserializer. |
| `qcc -o binary`: classify args, synthesize glue, call linker | ~250 | New. Handles both `main.cpp`-supplied and auto-main modes. |
| `qcc -o binary src…` batch mode wrapper | ~100 | Optional. Thin layer over per-file + link. |
| `qore_aot_script_register` runtime + header entry | ~70 | Factored from existing `qore_aot_module_init_v3`. |
| Test harness (3-file toy app: main.q + lib.qc + util.ql) + shell script + C++ host | ~200 | Validates end-to-end. |
| **Total** | **~1230 LOC** | Plus ~50 LOC of refactor in QoreAOTRuntime.cpp (hoist deserialize helper). |

Risk: **low**. Every piece is additive. No parser changes, no module-
system changes, no new runtime path beyond the `script_register`
entry point. Multi-file deserializer refactor is the biggest chunk
and directly follows the existing single-blob pipeline structure.

## Validation

1. Baselines remain green: JITSmoke 156, AOTSmoke 89, IRExecMode 4,
   LValuePathSmoke 13.
2. Slice 3-8 harnesses unchanged: `qo_link_test`, `qo_fragment_test`,
   `qo_aggregator_test.sh`, `qoa_archive_test.sh`. These must pass
   BYTE-IDENTICAL outputs (or at least functionally equivalent .qo's
   — the metadata format extension is non-breaking).
3. New `qoa_script_test.sh`:
   - Tiny 3-file toy app: `main.q` (with `%exec-class Demo`),
     `lib.qc` (class `Helper`), `util.ql` (free fn `compute()`).
   - Compile each to `.qo`: `qcc -c -L<tmp> <src>`.
   - Link: `qcc -o demo *.qo` (no custom main — uses auto-generated
     `main()` driving `%exec-class Demo`).
   - Run `./demo` → output matches `qore main.q` reference.
4. `qoa_script_custom_host_test.sh`:
   - Same toy app but link with a custom `main.cpp` that calls
     `qore_aot_script_register` explicitly and invokes a function via
     `qore_run_callable`.
5. Before-and-after timing: target AOT `./demo` ≤ 50 ms vs source
   parse of the same code (informational, not a pass gate).

## Post-slice-10 sequence (qctl trial)

**Slice 11** — qctl integration at
`/home/david/src/Qorus/git/qorus`:

1. Add `%modern` to qctl's 24 source files. Fix any strong-encap
   violations (out-of-line class extensions, etc.) if the code has
   them. Most of the change is a mechanical directive-addition.
2. Rewrite qctl's cmake rules: drop `qorus_qore2cpp` macro usage,
   replace with `qcc -c -L<build-dir>` per-file rules.
3. Update `exec/qctl_main.cpp`: delete 24 `extern void
   qorus_<name>(…)` declarations + call sites; add single
   `extern "C" void qore_qctl_script_register(QoreProgram*);` +
   one call.
4. Measure `qctl --help` before/after. Target ≤ 500 ms (vs 6 s
   current).
5. Run Qorus test suite: green.

**Slice 12** — qbugreport (same shape, mostly a CMake copy-paste).

**Slice 13** — qorus-core / qwf / qsvc / qjob / qdsp. Larger
codebases; may need slice 6b (fragment-merging aggregator) to
stay build-time-practical.

## What's explicitly NOT in slice 10

- **Source-level forward declarations** — no new Qore syntax.
- **`.qoa`-style archive packaging** — slice 7 covers modules; for
  scripts, users ar-archive their `.qo`s themselves if they want a
  distributable intermediate. No new archive format.
- **IDE integration** (language-server, IntelliSense) — consumers of
  `.qo` decl tables might benefit, but that's a separate tool.
- **Cross-compilation** — slice 10 compiles for the native target only.
- **Incremental link** — `qcc -o binary` always relinks fully. For a
  large app, `make` avoids relinking when nothing changed via
  standard dep tracking.

## Open questions

### Q1. Handling of `%exec-class` across multiple files

Only one file in an app can carry `%exec-class X`. The linker must
detect conflicts (two `%exec-class`s → error). The auto-main path
dispatches to that class. For custom-main hosts, `%exec-class` is
informational — the host calls `runClass(X)` explicitly.

**Decision**: error at link if 2+ `%exec-class` found across inputs.
If 1 found: auto-main uses it, custom-main hosts can query via a
metadata accessor (optional, non-critical).

### Q2. Top-level code across multiple files

Each file can have top-level code. Today, running the program via
`pgm->run(xsink)` runs top-level in parse order. With per-file `.qo`s,
what's the run order?

**Decision**: top-level statements are legal only in the `%exec-class`
file OR in a single "main" file (detected via presence of top-level
non-declaration statements). Multiple files with top-level → link
error. This is a common C-style constraint (only one `main()` per
binary).

### Q3. Decl-table size overhead

Preserving full decl tables (public + non-public) in every `.qo`
grows the file slightly vs slice 5's "contributions only" format.
Estimated 10-30% size increase for typical files.

**Decision**: acceptable — fragment blobs are small (~1-10 KB) and
the overhead is paid once at compile, never at runtime.

### Q4. `-L` search path ordering

If two `.qo`s on the search path declare the same class (shouldn't
happen under strong-encap but possible with user error), which wins?

**Decision**: error out — "duplicate declaration of X in foo.qo and
bar.qo". Same as C's multiple-definition link error.

### Q5. Reference tables — what to record

Every external name the parser consulted during compile. Including
runtime helpers (`qore_rt_*`) — no, those are libqore-internal, not
user-visible. Including builtin classes (`string`, `int`, `Mutex`)
— no, those are in the Qore system namespace and always available.
Only: user-declared names that came from sibling `.qo`s or
`%requires` modules.

**Decision**: record (fully-qualified name, expected kind: class /
function / typedef / …, source `.qo` or `%requires` name). Excludes
system builtins.

### Q6. Per-file compile failure when siblings not yet built

First build of a project: `qcc -c foo.qc` runs before `bar.qc` has
been compiled → `foo.qc`'s reference to `Bar` fails to resolve.

**Decision** (under the multi-file deserializer): **not a failure**.
If `Bar` can't be found in `-L<dir>` AND can't be resolved via a
`%requires` module, qcc errors with a clear "unresolved reference to
class Bar (bar.qo not found in -L path)". Standard build-system
responsibility to ensure siblings are built before dependent files,
exactly like C with headers. Users fix it by letting `make -j`
resolve the DAG (after the first successful run the depfile exists
and subsequent builds auto-order).

Alternative considered (and rejected for MVP): speculative
forward-emit — qcc compiles `foo.qc` with symbolic `Bar`, fixed up
at link. Requires parser-level forward declarations; deferred.

## Why this architecture is the right endpoint

1. **Matches C/C++ muscle memory.** Build systems (Make, Ninja,
   CMake, Bazel, Meson) already know how to drive `cc -c` + `ld`
   workflows. Qore slots into that idiom. No Qore-specific CMake
   macros, no manifest files.

2. **The `.qo` is the unit of tracking.** One file on disk, one
   build-system artifact, one target for `make` rules. Dependencies
   are depfile-tracked — same as C.

3. **`-L` search path is universally understood.** Nothing to
   explain.

4. **Multi-file deserializer generalizes the runtime.** Today's
   single-blob `deserializeIntoProgram` was designed for module-at-
   a-time loading, but apps need multi-blob cross-resolution. The
   refactor benefits BOTH slice 10 (compile-time preload) and future
   slice 6b (source-less module aggregator — merging fragment blobs
   at link without re-parsing source).

5. **Existing slices aren't broken or competing.** Modules stay
   modules. Scripts stay scripts. `.qoa` archives stay as they are.
   Script mode is the third orthogonal path.

6. **Custom C++ hosts are first-class.** qorus-core's bespoke
   initialization (DB options, signal handling, Qorus namespace
   injection) just works — it gets one register call instead of
   24, nothing else changes.
