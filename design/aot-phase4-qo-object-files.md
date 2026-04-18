# Phase 4: `.qo` Object Files for C++ Linkage

**Status:** Design. Scope: enable compiled Qore code to be linked into
a C++ binary (primary target: `qorus-core` in `~/src/qore/git/qorus`)
without round-tripping through the runtime module loader.

**Author:** AOT team, 2026-04-18. Supersedes the sketch in
`aot-compile-time-optimization-roadmap.md` §Phase 4.

## Goal

```bash
# Today (works):
qcc -m HttpServer.qm       # → HttpServer.qmod (runtime-loaded by libqore)

# Phase 4 (target):
qcc -c HttpServer.qm       # → HttpServer.qo     (ELF relocatable object)
ar rcs mylib.qoa *.qo      # → mylib.qoa         (standard Unix ar archive)
g++ main.cpp mylib.qoa -lqore -o myapp
./myapp                    # Statically linked, no module loader needed
```

The user-visible win: ship a single binary that contains pre-compiled
Qore code plus C++ code, with no requirement to carry `.qmod` files or
go through the filesystem module-load path.

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

## Minimal design — "qmod minus the loader"

A `.qo` is the same ELF object that lives inside a `.qmod` today, with
four additions that make it directly linkable + initializable from C++:

### 1. Stable exported entry symbol

Add a single exported C function per module:

```c
extern "C" void qore_<module_name>_register(QoreProgram* pgm);
```

This is a thin wrapper that calls `qore_aot_module_init_v3` + the
namespace init using the already-private globals. From C++:

```cpp
extern "C" void qore_HttpServer_register(QoreProgram*);
...
QoreProgram* pgm = qore_create_program();
qore_HttpServer_register(pgm);
pgm->run(argv);
```

One symbol per module, named deterministically from the module name
(with `-` → `_`). No per-function exports needed; those remain private
and are reached via the function table the register call hands to
libqore.

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
qcc -c [--big-fn-threshold=N] [-g | --strip-debug-info] file.qm   # → file.qo
qcc -c [...] dir/                                                  # split-module .qo
```

- `-c` emits a standalone `.qo` (ELF `.o`) instead of the `.qmod`
  wrapper. All other flags (`-O3`, `-g`, `--big-fn-threshold`, etc.)
  work identically.
- Default output is `<basename>.qo`; `-o path` overrides.
- `--target=triple` remains supported for cross-compile.

## qcc flag: `-a` (archive)

```
qcc -a -o mylib.qoa *.qo
```

Wraps system `ar rcs` + generates the glue `qore_qoa_register_all`
object. Emitted as a static archive that any `g++` link command accepts.

Or skip this and let the user invoke `ar` directly — qcc only emits the
glue object via `qcc -c --qoa-index` or similar.

**Open:** do we need `qcc -a` at all, or is `ar rcs` + a hand-written
`register_all.c` sufficient for the MVP? Leaning toward "no qcc -a for
MVP; document the ar invocation."

## Symbol-naming convention

| Symbol kind                  | Scheme                                    |
|------------------------------|-------------------------------------------|
| Module register entry        | `qore_<mod>_register`                     |
| Module metadata accessor     | `qore_<mod>_get_metadata`                 |
| Module info globals          | `qore_<mod>_module_name`, etc.            |
| AOT functions (private)      | `Qore$<Mod>$<Class>$<Method>` (unchanged) |
| Runtime helpers (external)   | `qore_rt_*` (unchanged)                   |

Module name is sanitized: `A-Z a-z 0-9 _` allowed; all else → `_`.
Collision handling: if two modules sanitize to the same name, qcc errors
at link time (or the .qoa builder catches it).

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

## Deliverables

- `qcc -c` flag + test case (compile a minimal `.qm`, verify the
  produced ELF has the expected exported symbols).
- Register-function emitter in `generateModuleABIV2` (new helper,
  called when `-c` is set).
- Per-module prefixing of info globals.
- New public C API in `include/qore/QoreAOT.h`.
- Documentation + `qorus-core` integration example.
- Integration test: tiny C++ `main()` that loads a `.qo` and calls
  a Qore function.

## Phase 4 vs. the rest

Orthogonal to Phases 2/3/5b/6/7. Can be developed in parallel. The
shipped work in Phases 1 + 5a benefits `.qo` for free.

## Entry point for implementation

1. Land `qcc -c` (emits `.qo`; no register function yet) — validates
   that the existing ELF is linker-compatible.
2. Add register function + per-module symbol prefixing.
3. Land `include/qore/QoreAOT.h` + tests.
4. `qcc -a` / `.qoa` glue (MVP: manual `ar` + hand-written
   `register_all.c`).
5. qorus-core integration trial.
