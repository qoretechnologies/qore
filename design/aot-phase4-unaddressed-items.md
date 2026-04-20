# AOT Phase 4 — plan for unaddressed slice-10 limitations

**Scope.**  Six items documented as "deferred" or "not yet shipped" at
slice-10 landing, audited against current code (commit `e4f3325ff`
develop merge).  This doc collects problem statement + design sketch +
execution plan for each, organises them by ROI, and proposes a
rollout order.

**Current footprint without these.**  The AOT Phase 1–4 migration
sweep (qctl, qbugreport, qdsp, qwf, qsvc, qjob, qorus launcher) is
functional end-to-end; every binary parses + runs cleanly.  The
unaddressed items surface in narrower cases: one is a real functional
gap (item 1), three are workflow/perf ergonomics (items 2, 3, 5),
one is build-system topology (item 4), and one is an unmigrated
binary (item 6).

---

## Priority summary

| # | Item | Kind | Effort | User-visible impact | Status |
|---|---|---|---|---|---|
| 1 | Cross-`.qo` runtime-eval'd constants | Functional gap | ~1–2 days | **Done** — Phase A landed (commit `beaca911b`) | ✓ |
| 6 | qorus-core Phase 5 AOT migration | Feature | ~2–3 days | **Done** structurally — Phase B (Qorus `be3113fc3`); runtime smoke blocked on env (xml module) | ✓* |
| 5 | Public C ABI for parsing | API ergonomics | ~0.5 day | **Done** — Phase C/5 landed (commit `0cd54f908`) | ✓ |
| 2 | `qcc -o binary *.qo` auto-linker | Workflow | ~1 day | **Done** — Phase C/2 landed | ✓ |
| 3 | `%requires` link-time union | Perf nicety | ~1 day | Shaves module-load dup overhead | Pending |
| 4 | Parser-level forward decls | Build-topo relaxation | ~3–5 days | Parallel/order-agnostic .qo builds | Deferred |

---

## 1. Cross-`.qo` runtime-evaluated constants

**Status: implemented** — see commit(s) landing on feature/5164_jit
(format feature bit `QORE_AOT_FEAT_CONST_PENDING`, ConstantEntry
bit `aot_shell_pending`, deserializer wraps val in
`RuntimeConstantRefNode(aot_deferred=true)` when pending).
Regression coverage lives in `examples/aot/script_demo/` (main.q +
lib.qc — cross-file `Local::Magic` + `LibConsts::Answer`);
`examples/aot/qo_script_test.sh` asserts correctness.

### Problem

Slice 10c's compile-time preload (`qcc -c -L<dir> main.q`) extracts
every sibling `.qo`'s fragment blob and feeds it to the multi-
deserializer.  The deserializer creates **metadata shells** — namespaces,
class stubs, hashdecl stubs, enum stubs — so `main.q`'s parser can
resolve cross-file name refs via `parseFindClassIntern` and friends.

But for **constants with non-literal initialisers**, the slot shell
exists (name + declared type) while the VALUE is empty.  When `main.q`
references such a constant, the parser's constant-folding path reads
the (empty) value and folds the reference to `NOTHING`.  At runtime
the .qo's `__const_init::X` init-func runs and populates the value,
but by then the parser has already baked in `NOTHING` — the live value
never reaches main's code.

Reproducer (from the 10f session note):

```qore
# lib.qc
class Local {
    const Magic = sprintf("%d", 42).toInt();   # non-literal init
}

# main.q
any v = Local::Magic;                          # folds to NOTHING at compile
assertEq(42, v);                               # FAILS: got NOTHING
```

Today works around by shipping `lib.qc` as a `%requires`-able module
— module mode dlopens at parse time so init-funcs fire before
main.q parses.  That breaks the slice-10 "no module wrapper" promise
for any compilation unit containing runtime-eval'd constants.

### Design

Three paths proposed in the 10f session note — here's the evaluated
recommendation:

**Path (a) — dlopen-relocatable preload**: link each `.qo` → temp
`.so`, `dlopen` it during compile-time preload, run its init-funcs.
Gives a real populated ConstantEntry table.
Risk: compile-time loads native code; breaks sandboxing; non-trivial
toolchain glue (ld -shared with qlib symbols resolved).  **Reject**.

**Path (b) — IR interpreter at compile time**: run init-exprs through
the IR interpreter during preload.  Gives a populated ConstantEntry
without loading native.
Risk: init-exprs may call arbitrary Qore functions.  The IR interp
is complete enough to run most of these (it runs user code in JIT
mode daily), but compile-time execution has different expectations
(sandboxing, determinism).  **Defer** pending IR-interp readiness
audit — would also need sandbox model for compile-time code exec.

**Path (c) — parser emits runtime ref, not fold**: teach the parser
to detect the "deserialized shell, value pending" state of a
ConstantEntry and emit a `RuntimeConstantRefNode` instead of folding.
At actual runtime, the ref dereferences the (now-populated) value.
Slice 10f's `executeInitFunctions(end_batch)` ensures the value is
populated before any runtime access.
Risk: low — the `RuntimeConstantRefNode` infrastructure already
exists for module-crossing constant refs; the parser's fold path
needs one extra branch.  **This is the design we ship.**

### Execution plan

1. **Add a state bit to `ConstantEntry`**: `bool aot_shell_pending = false;`
   — set during shell deserialization, cleared when the init-func
   populates the value.
   - Parser-time: treat `aot_shell_pending == true` as "value not
     foldable yet"; emit `RuntimeConstantRefNode` with `aot_deferred=true`.
   - `executeInitFunctions` post-resolveAll: clear the bit on each
     ConstantEntry it populates.
2. **Grep-audit every constant-folding call site** for callers that
   short-circuit when `ConstantEntry` is non-null but value is
   NOTHING: they must re-route through the runtime-ref path.
   Candidates: `qore_ns_private::parseResolveBarewordIntern`,
   `QoreDotEvalOperatorNode`'s constant access, `RuntimeConstantRefNode`
   eval.
3. **Extend `examples/aot/script_demo`** with a cross-.qo
   runtime-eval'd constant: `lib.qc` exposes `Local::Magic`;
   `main.q` asserts `Local::Magic == 42`.  Harness
   `qo_script_test.sh` gets a corresponding assertion.
4. **Regression coverage**: add cases under
   `examples/test/qore/aot/cross-qo-constants.qtest` covering (a)
   literal const fold unchanged, (b) runtime-eval'd const via .qo
   yields correct value, (c) negative — parse error if runtime-eval'd
   const referenced where a parse-time constant is required
   (e.g., `%ifdef` or `case` label).
5. **Docs**: update `design/aot-phase4-slice10-script-context.md` to
   remove the "known limitation" paragraph + note the fix.

### Risk

Low.  The `RuntimeConstantRefNode` path is exercised every day by
module-mode constants.  The change is a narrow parser branch + a
state bit on ConstantEntry.  Back-compat: pre-feature-bit `.qo`s
never set `aot_shell_pending` (default false), so their constants
fold as before.

### Effort

~1–2 engineer-days.

---

## 2. `qcc -o binary *.qo` auto-linker

**Status: implemented** — `qcc -o <binary> [-e <fn>] *.qo` now
detects a pure `.qo` positional input set, emits a `<binary>.main.cpp`
glue (qore_init → create_program → begin_batch → per-.qo
`qore_<san>_<san>_script_register` → end_batch → run entry fn →
destroy), then invokes `$CXX` (fallback `g++`) to link against
`-lqore`.  Harness `examples/aot/qo_link_test.sh` proves end-to-end
(`.qc` + `.q` → two `.qo`'s → standalone binary without any
host-written C++).  Baselines green.

### Problem

Slice 10 ships per-file `.qo` compilation.  Turning a set of `.qo`s
into an executable today requires:

```bash
cat > main.cpp <<'EOF'
#include <qore/Qore.h>
#include <qore/QoreAOT.h>
extern "C" void qore_lib_lib_script_register(QoreProgram*);
extern "C" void qore_main_main_script_register(QoreProgram*);
int main(int argc, char** argv) {
    qore_init(nullptr, "UTF-8", true, 0);
    QoreProgram* p = qore_create_program(0);
    qore_lib_lib_script_register(p);
    qore_main_main_script_register(p);
    qore_run_callable(p, "compute", nullptr);
    qore_destroy_program(p);
    qore_cleanup();
}
EOF
g++ main.cpp lib.qo main.qo -lqore -o app
```

~30 lines of boilerplate per project, plus `-L`/`-l` flag discipline.

### Design

New `qcc -o <binary> *.qo <entry.q>` mode:

- Consumes the same `.qo` set that `g++` would link
- Identifies `<entry.q>` as the script source (for its
  `qore_<app>_<file>_script_register` entrypoint) + a callable name
  (`_toplevel` by default, or `-e <name>`)
- Synthesises the equivalent main.cpp in a temp dir
- Invokes `$CXX` (or `g++` fallback) with the appropriate `-lqore`
  and `.qo` list
- Cleans up the temp main.cpp after link (or keeps it with `-K`)

### Execution plan

1. New `qcc` arg parser case for `-o <binary>` with `.qo` inputs +
   an optional `.q` entry source.
2. Helper emits `<tmpdir>/__qcc_main_<pid>.cpp` referencing each
   `.qo`'s register symbol via the slice-10e naming convention.
3. Invokes compiler detection (`CXX` env → `g++` → error).
4. Link-line derivation: `-L$(qore-pkgconfig ...)` + `-lqore` +
   the `.qo` set + the generated main.cpp.
5. New toy harness under `examples/aot/qo_link_test.sh` — takes a
   directory of `.qc`/`.q` and runs the full pipeline:
   `qcc -c *.qc`, `qcc -c -L. main.q`, `qcc -o app *.qo main.q`,
   `./app`.
6. Docs: slice-10 design doc update.

### Risk

Low.  Purely additive — doesn't change existing codegen or runtime
paths.  Compiler detection is fragile on cross-platform; start with
`$CXX` + g++ fallback on Linux, defer Darwin/Windows to a follow-up.

### Effort

~1 engineer-day.

---

## 3. `%requires` link-time union

### Problem

Each `.qo` carries its own module manifest (the set of modules its
source `%requires`).  At runtime, each `qore_<app>_<file>_script_register`
call triggers its own set of `parseLoadModule` calls.  Module loading
is idempotent (second call is a hash-map hit), but there's still
overhead — serialization cost + metadata walk to detect the "already
loaded" state.

Example: 10 `.qo`s each `%requires RestClient` → 10 no-op loads at
register time.  Each is ~100 µs → 1 ms wasted on module-load
idempotency paths per binary startup.

### Design

Emit a per-binary manifest when the aggregator is generated:

1. `qorus_qore2qo_agg` (or a new `qcc -A <set>.qo -o <agg>.cpp`)
   reads each input `.qo`'s `%requires` list from its metadata
2. Unions the lists, dedupes
3. Emits a **single** `qore_<agg>_load_modules(QoreProgram*)` helper
   called **once** before the per-file register loop

The runtime register functions keep their own module-load calls as
a safety net (so individual `.qo`s remain standalone-registerable),
but the aggregator short-circuits when the program already has the
modules loaded.

### Execution plan

1. Reader helper: parse `.qo`'s metadata section for the `%requires`
   list (already serialized per slice 5).
2. Aggregator emits `qore_<agg>_load_modules` fn + calls it before
   the register fns in `init_<agg>_qo`.
3. Per-file `script_register` gets a fast-path: if the target
   program already has each required module, skip the load calls.
4. Harness: add a multi-.qo test with shared modules and measure
   register-phase timing with/without the union.

### Risk

Low.  Failure mode is redundant load attempts — harmless.

### Effort

~1 engineer-day.

---

## 4. Parser-level forward declarations (compile-time topo-order)

### Problem

`qcc -c -L<dir> main.q` requires every sibling `.qo` main.q
references to already exist in `<dir>`.  Build topology must be
topological: compile leaves first, then things that reference them.
Makes parallel per-file builds in `make -j` work only if the dep
graph is shallow + all siblings are up-to-date.

Note: **register-time** topo-order is already handled (slice 10g
batch-register covers class/type refs at runtime).  This item is
only about the compile-time constraint.

### Design

Allow `qcc -c` to emit a lightweight "declaration stub" `.qo`
alongside the full one, containing only the namespace/class/hashdecl/
enum shells but no function bodies or init-exprs.  Preload uses the
stubs; the full bodies link at final-binary time.

This mirrors C/C++ `.h` files + an include-only compilation mode.

### Execution plan

(Large; 3–5 days.  Outline only.)

1. Fragment blob emission: extract shell section from the full
   compiled blob into a separate `<file>.qo.decl` object.
2. Preload API extended: `openAndDeserializeShells(data, size)`
   accepts either full or decl-only blobs.
3. Build pipeline: `qcc -c` also generates `<file>.qo.decl` with
   minimal metadata (linkable shells only).
4. New `qcc` mode `--preload-decls-only=<dir>` so compile-time
   preload uses the decl-only set, letting sibling builds run in
   parallel without the full `.qo` being ready.

### Risk

Medium-high.  Touches the core blob format (decl-vs-full split);
increases the number of artefacts cmake manages; parser has to
tolerate partially-populated shells in new ways.

### Effort

~3–5 engineer-days.  **Recommend deferring** until build-parallelism
pain becomes a real bottleneck — current `make -j8` on DataProvider
(134 files, biggest case we've measured) is 93 s wall-clock, and
the dep graph is shallow enough that full topo-order works.

---

## 5. Public C ABI for parsing

**Status: implemented** — commit `0cd54f908`.
`include/qore/QoreAOT.h` now exposes `qore_parse_source_file`,
`qore_parse_source_string`, `qore_parse_commit`, `qore_last_error`
as thin public C wrappers.  `examples/aot/qoa_link_test.cpp` uses
them exclusively (no more C++ staging helper).  Baselines green
(AOTSmoke 93/93, JITSmoke 156/156, all slice harnesses).

### Problem

Slice 8 shipped `qore_create_program` / `qore_destroy_program` /
`qore_run_callable` as public `extern "C"` entry points.  Parsing is
still C++-only (`QoreProgram::parse`).  Host code that wants to parse
a Qore source file via the public C API has to wrap it in C++.

### Design

Add to `include/qore/QoreAOT.h`:

```c
//! Parse a Qore source file into the program.
//! @return 0 on success; non-zero on parse error (check qore_last_error).
int qore_parse_source_file(QoreProgram* pgm, const char* path, const char* label);

//! Parse a Qore source string.
int qore_parse_source_string(QoreProgram* pgm, const char* source, size_t len, const char* label);

//! Commit staged parse into the program.
//! @return 0 on success; non-zero on parse-commit error.
int qore_parse_commit(QoreProgram* pgm);

//! Retrieve the last parse/runtime error message (null if none).
const char* qore_last_error(QoreProgram* pgm);
```

All four are thin wrappers around existing `QoreProgram` methods +
private `ExceptionSink` capture.

### Execution plan

1. Add declarations to `include/qore/QoreAOT.h`.
2. Implement in `lib/QoreAOT.cpp` alongside the slice-8 wrappers.
3. Per-program thread-local error stash for `qore_last_error`.
4. Update `examples/aot/qoa_link_test.cpp` to use the new C API
   instead of the `extern "C"` staging helper (that's the
   "slice 9 flagged" spot referenced at slice-8 landing).
5. Docs: public header comments + design note.

### Risk

Low.  Purely additive.  The slice-8 review already flagged exactly
this need.

### Effort

~0.5 engineer-day.

---

## 6. qorus-core Phase 5 AOT migration

**Status: structurally implemented** — Qorus commit `be3113fc3`,
requires Qore `f777b22df` (PO_REQUIRE_PROTOTYPES in PO_MODERN).
Cmake configures clean; the QORUS_CORE_MAIN batch aggregates 13
source groups emitting ~575 register calls across 13 QO_AGG
targets; qorus_core_main.cpp rewritten to use
`qore_aot_script_begin_batch` / `end_batch` wrapping the
aggregator entry points.  QORUS_CORE_MODULES_X stays on Phase 0
(runtime .qm registration).  Runtime smoke (`qorus-core --version`)
is blocked on the current dev env missing the xml binary module
(WebContentUtil depends on it).  Install `qore-xml-module` to
unblock end-to-end validation.

### Problem

`qorus-core` is the last Phase 0 binary.  It runs the cluster's
master process — the HTTP/REST front-end + process manager.  Its
source groups are the largest in the Qorus tree:

- `QORUS_CORE_QORE_SRC_X.cpp` (main Qorus code)
- `QORUS_CORE_MASTER_QWF` / `QSVC` / `QJOB` / `SHARED`
- `QORUS_CORE_QWF` / `QSVC` / `QJOB`
- `COMMON_INTERFACE_CORE_*` / `COMMON_INTERFACE_QDSP`
- `QORUS_CORE_MODULES_X.cpp` (10+ QPP classes)
- `QORUS_DOCS_SOURCES_X` / `QORUS_DOCS_REST_SOURCES_X`

Totalling ~26 source groups.  Recipe is identical to
qwf/qsvc/qjob/qorus (just done in this session).

### Design

Mirror the Phase 4 qorus-launcher pattern:

- `qorus_qore2qo_batch(QORUS_CORE_ALL_QO QORUS_CORE "...")`
  covering every source group
- `qorus_qore2qo_agg` emitting per-group aggregators:
  `QORUS_CORE_QORE_QO_AGG`, `QORUS_CORE_MASTER_*_QO_AGG`, etc.
- `exec/qorus_core_main.cpp`: wrap init-func block in
  `qore_aot_script_begin_batch` / `end_batch`; replace individual
  `qorus_*(qpgm, &xsink)` calls with `init_<agg>_qo(qpgm)` calls
- `exec/qorus-core-compile-stubs.qc`: mirror
  `qorus-compile-stubs.qc` + any qorus-core-specific C++ QPP class
  stubs (`PerformanceCache`, `SegmentEventQueue`, etc.)

### Execution plan

1. Audit `qorus_core_main.cpp` for all `qorus_<file>(...)` init-func
   calls + required parseDefine/parse-option/load-module set.
2. Create `exec/qorus-core-compile-stubs.qc` (copy
   `qorus-compile-stubs.qc`, add the QPP class stubs qorus-core
   needs at parse time: `PerformanceCache`, `PerformanceCacheManager`,
   `TimedWorkflowCache`, `TimedSyncCache`, `OrderExpiryCache`,
   `SegmentEventQueue`, `AutoFastLock`, `FastCondition`,
   `ServiceManagerBase`, `AbstractQorusService`,
   `AbstractQorusCoreServiceBase`).
3. CMake: add a new `QORUS_CORE_MAIN` batch + aggregators before
   `add_executable(qorus-core)`.  Swap the Phase 0
   `$<TARGET_OBJECTS:...>` set + `${*_SRC_X}` cpp set for the
   aggregator vars + `${QORUS_CORE_MAIN_ALL_QO}`.
4. C++: wrap the init-func block in `qorus_core_main.cpp`, add
   `#include <qore/QoreAOT.h>`, replace individual calls with
   `init_*_QO_AGG_qo` calls.
5. Build + `qorus-core --version` (only safe single-invocation
   test — full start needs the cluster).
6. Scale-up debugging: expect 1–2 cross-session phase-sync bugs to
   surface (p4-session pattern), fixable against existing
   `QoreAOTBinaryMultiDeserializer` phase-split infrastructure.
7. `qorus-core-compile-stubs.qc` may need iterative extension as
   batch parse surfaces missing symbols.

### Risk

Medium.  Size of the source set (≈2× qorus launcher) means more
potential edge cases.  Mitigation: chunk-by-chunk migration —
port the `CORE_MASTER_*` groups first (already have QO_AGG targets
from the qwf/qsvc/qjob batches), then `CORE_*_QORE_SRC`, then
`QORUS_CORE_QORE_SRC` last.

### Effort

~2–3 engineer-days.  Roughly distributed:

- 0.5 day: stubs + initial cmake wiring
- 1 day: main.cpp rewrite + first build cycle
- 0.5 day: scale-up debug (expect surfaced phase-sync or module-load
  issues)
- 0.5 day: runtime verify (`qorus-core --version` clean,
  ideally a full cluster smoke if dev env available)
- 0.5 day: cleanup + docs

### Payoff

- Eliminates the last Phase 0 source-embed binary
- Removes ~30 `x_<file>.qX.cpp` generated files from the build
- Enables Qorus to drop the `qorus_qore2cpp` macro + associated
  `make-source.qr` / `gen_prototypes` tooling once the other
  Phase 0 consumers are gone (they're not — qwf/qsvc/qjob still
  have compile-time-shared `.qc` files that go through the same
  path — but qorus-core was the biggest producer)
- Symmetry with cluster-interface binaries: all 7 Qorus binaries
  on the same AOT recipe

---

## Rollout

Proposed sequence:

**Phase A — real functional fix (1–2 days)** — ✓ **done**
1. Item 1 (cross-`.qo` runtime-eval'd constants).  Landed in
   commit `beaca911b`.

**Phase B — big-ticket migration (2–3 days)** — ✓ **structurally done**
2. Item 6 (qorus-core Phase 5).  Landed in Qorus `be3113fc3` +
   Qore `f777b22df`.  Runtime smoke blocked on env (xml module).

**Phase C — API + workflow polish (1.5 days total)** — ✓ **done**
3. Item 5 (public C ABI for parsing).  Landed in commit `0cd54f908`.
4. Item 2 (`qcc -o binary` auto-linker).  Landed — harness at
   `examples/aot/qo_link_test.sh`.

**Phase D — optional perf nicety** — pending
5. Item 3 (`%requires` link-time union).  Land only if post-
   migration startup benchmarks show module-load idempotency as a
   measurable fraction.  **Benchmarking should precede
   implementation** — if the 5×-per-binary module-load overhead is
   sub-millisecond, skip the engineering.

**Deferred indefinitely**
6. Item 4 (parser-level forward decls).  Revisit only if build-
   parallelism bottlenecks appear on a module bigger than
   DataProvider's current 134-file set.

**Remaining Phase C-D effort: ~2 engineer-days** (Items 2 + 3).

Items are independent; can be executed in parallel or reordered.
