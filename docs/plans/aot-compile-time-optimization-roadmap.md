# AOT Compile-Time Optimization Roadmap

**Status:**
- Phase E (EH default-on) shipped at `8d392571f` (2026-04-18).
- Phase 1 (time-trace + debug-info flags) shipped at `c828cc1a8`.
- Phase 5a (OptimizeNone big-fn escape hatch) shipped at `f0f75a301`.
  **HS compile: 623s → 20.8s (30x)** with `--big-fn-threshold=200`.
- Phase 4 (`.qo`/`.qoa` object files + downstream linkage) **substantially
  shipped** on `feature/aot-build-link-stabilization`: `qcc -c`/`-o app *.qo`/
  `--from-objects`/`-a`, the relink-surviving `qore_aot_pcloc` section (ELF +
  Mach-O), per-file `.qo` indexes + `qore-qo-source-order` link ordering, and
  lazy exception source locations that survive into linked executables. See
  `design/aot-object-files-and-module-artifacts.md`,
  `design/aot-script-context.md`, `design/aot-lazy-loc-innermost-frame.md`.

**2026-06-26 re-measurement + evaluation (16-core x86_64, Release qcc):**
DataProvider.qm (137 files, 1907 variants, 13.8 MB qmod) is the critical-path
module at **~31 s** clean compile. Time-trace split: **OptModule ~16 s +
BackendCodegen ~16 s** (roughly equal halves), single `llvm::Module` → single
`emitObjectFile` → single-threaded. Opt-level is **codegen-bound, not opt-bound**:
`-O0` 21.6 s, `-O1` 28.7 s, `-O2` 32.3 s, `-O3` 30.7 s — so lowering the opt level
is NOT a build-time lever (Phase 7 note below). Cheap micro-opt landed: the
pc→loc section now reuses the trailer's already-parsed DWARF map instead of
re-parsing (`8c6f5e105`), removing a redundant per-object DWARF parse. The one
remaining large build-time lever is **Phase 3 (parallel codegen)** — see updated
Phase 3 for the measured ceiling and integration path.

Baseline for remaining work: HS.qm compile **20.8s** (opt-in flag),
**198/198 functions AOT-compiled**.

**Primary goal (met for HS):** Reduce qcc compile time from 623s → ~100s.
Phase 5a exceeded this by 5x for the pathological case. Next: make the
flag default-on and validate runtime cost is acceptable on representative
workloads.

**Secondary goal (open):** produce linkable `.qo` object files that can
be linked into large C++ binaries (qorus-core) from compiled Qore
sources, with debug-info control equivalent to CMake's `Release`,
`Debug`, and `RelWithDebInfo` modes.

## Why 25% wasn't enough — and what fixed it

Step 5 (EH migration) delivered a 25% compile-time improvement
(806s→623s) + full 198-function coverage. Profiling revealed 97% of the
remaining time was LLVM codegen (SelectionDAG + MachineInstr passes +
RegAlloc), not mid-end opt. HS profile (`aot-compile-time-profile-hs-p76.md`)
showed `handleRequest` alone = 87% of compile time (533s of 611s): a
single 905-basic-block function dominating SelectionDAG.

**Phase 5a cracked it.** Tagging functions ≥ N basic blocks with
`Attribute::OptimizeNone` + `NoInline` caused LLVM to skip SelectionDAG's
quadratic-on-huge-functions codegen. 623s → 20.8s (30x). Runtime cost
measured: ~1% on call-heavy HS workloads, ~7% on arithmetic-heavy
microbench. 4 functions crossed threshold=200 (`handleRequest` 905 BBs,
`sendReply` 254, `handleMultiplexedPersistentSync` 278,
`handlePersistentConnectionSync` 253).

This reorders the roadmap: Phases 2/3/5b are no longer urgent for HS.
Main open work:
1. Validate Phase 5a on more workloads; make default-on if costs hold.
2. Phase 4 (`.qo` object files) — independent track; unblocks qorus-core
   linkage. Now top priority.
3. Phase 2 (caching) — dev-cycle win, still wanted.

The clang playbook still informs the remaining phases:

1. **Measure first** (clang's `-ftime-trace`) — done (Phase 1).
2. **Parallelize** (clang's ThinLTO) — lower priority now.
3. **Cache** (clang's PCH / ccache) — Phase 2.
4. **Skip work** (clang's `-fdelayed-template-parsing`) — Phase 5a did
   this in the codegen pipeline.
5. **Per-build profile** (`-O2` vs `-O3`, DWARF vs no DWARF) — Phase 1 +
   Phase 7.

## Phased roadmap

Each phase can ship independently. Phases ordered by ROI × implementation
cost. Preceding phase informs priority of the next.

### Phase 1: Instrumentation (`--time-trace`, `--debug-info`)

**Goal:** expose backend-pass timing via Chrome trace + add debug-info
toggle.

**Size:** small. 1 session.

**Changes:**
- `qcc-main.cpp`: new flags `--time-trace[=PATH]`, `--debug-info={yes,no}`,
  `-g` shorthand.
- `lib/QoreAOT.cpp`: gate `llvm::timeTraceProfilerInitialize` and
  `timeTraceProfilerWrite` behind the flag.
- `lib/QoreIRToLLVM.cpp`: gate `DILocation`/`DISubprogram` emission behind
  a new `emit_debug_info` member (default true until Phase 2 settles).
- Document usage in qcc `--help`.

**Deliverables:**
- `qcc --time-trace=/tmp/hs.trace -O3 -m HttpServer.qm` emits a JSON file
  viewable at `chrome://tracing`.
- `qcc --debug-info=no -O3 ...` skips DWARF.
- Benchmark: measure HS compile time with/without debug info.

**Informs:** whether DWARF is a significant fraction of the 600s codegen.
If stripping saves >10%, it's worth making `RelWithDebInfo` a tier with
explicit opt-in.

### Phase 2: Per-function object-file cache

**Goal:** skip codegen for unchanged functions across iterative builds.

**Size:** medium. 1-2 sessions.

**Architecture:**
- New cache dir: `~/.cache/qcc/` (or `$QCC_CACHE`).
- Per-function key: SHA256 of
  `(qcc version + llvm version + opt-level + debug-info flag +
  target triple + function IR bytes)`.
- Value: pre-compiled `.o` blob for that function's LLVM module.
- On compile: for each function, hash key → lookup in cache. Hit: load
  `.o` bytes, skip codegen. Miss: full codegen, write result to cache.
- Integrate with existing `.qmod` format: cache is separate (indexed by
  IR hash), not a blob inside the `.qmod`.

**Consequences:**
- First build: same as current.
- Re-build after touching 1 function: ~5s (only that function +
  link/serialize).
- Cache poisoning prevented by including toolchain versions in key.
- `$QCC_NO_CACHE=1` escape hatch.

**Deliverables:**
- Cache hit rate + time savings on iterative HS compile.
- Verify cache invalidation works across qcc/llvm version bumps.

**Informs:** how much dev-loop time is saved. Sets expectation that
fresh/CI builds still need Phase 3.

**2026-06-26 note — relation to per-file `.qo` incremental builds.** A
related, coarser dev-loop win is per-file split-dir compilation (rebuild only
the changed `.qc`'s `.qo`, then relink) instead of the current whole-dir
`qcc -m`. The scaffolding exists (`QORE_QCC_COMPILE_OBJECTS`/`qore-qo-source-order`)
but is **disabled**: measurement showed per-file re-parses the full context N
times so *clean* builds are slower (DataProvider 93 s vs 69 s @ -j8), and the
per-file path hits a latent SIGSEGV in `extractAOTSlotIdentities`
(`lib/QoreAOT.cpp:15613`) on some modules (e.g. `FileLocationHandlerHttp.qc`)
that whole-dir avoids (see `cmake/QoreMacros.cmake:1454-1465`). Unblocking it =
fix the SIGSEGV + add an opt-in flag for dev loops; it does not help clean/CI
builds. Per-function content-addressed caching (this phase) is the more general
answer and subsumes most of the per-file benefit without the re-parse penalty.

### Phase 3: Parallel codegen (split-after-opt + make jobserver)

**Goal:** parallelize backend codegen across cores, on by default where safe,
delivered in-branch (`feature/aot-build-link-stabilization` is the feature
branch for AOT build+link — this is more of that feature work).

**Size:** large. 4-5 sessions.

**Measured ceiling (2026-06-26, 16-core):** DataProvider (worst case) =
OptModule ~16 s + BackendCodegen ~16 s of a ~31 s compile. We parallelize the
**codegen half only** (split-AFTER-opt): optimize the whole module once
(preserves cross-function inlining → **zero runtime-perf risk**, unchanged
optimization determinism), then partition + codegen on N threads → ~31 s →
~17 s ideal (**~45%**). Split-before-opt (parallelize both halves) is rejected:
it loses inlining and changes runtime perf.

**Strategy is NOT a wire-format change.** The final `.qmod` is produced by the
existing aggregate/`--from-objects` linker (N objects → one artifact, sections
concatenated, metadata/registration glue in one object). The loader and
downstream static-linkers cannot distinguish a parallel-built aggregate `.qmod`
from a single-object one — already true and exercised by the AOTSymbolIndex
`--from-objects`/`--link-qo` tests. So this is a build-pipeline feature whose
output is format-identical; only the exact bytes differ (handled by determinism
below). A required gate is a **load-equivalence test**: single-object vs
`--jobs N` `.qmod` must be functionally identical (loads, same symbols/metadata,
same runtime + backtraces).

**Concurrency model — jobserver-first, default-on-where-safe.** The
oversubscription problem (outer `make -jN` × inner codegen threads = N×N) is
solved the way GCC-LTO (`-flto=jobserver`) and Cargo↔rustc (codegen-units)
solve it: a **GNU-make jobserver client**.
- **Under `make` (jobserver present):** qcc parses `MAKEFLAGS`/`--jobserver-auth`
  and acquires one token per codegen thread, releasing on completion. Total live
  threads across all qcc invocations never exceed `-jN`; in the busy middle each
  qcc stays ~single-threaded, and the **tail-end big module gets all cores for
  free**. On by default — no flags.
- **Standalone `qcc -m X`:** default to `nproc` (no outer build to oversubscribe
  against); `--jobs N` overrides.
- **Ninja:** CMake `JOB_POOL` (or Ninja's newer jobserver) coordinates.
- **`--jobs 1`:** kept as the **deterministic single-threaded reference mode**
  (reproducible-byte builds, debugging) — a feature, not "the safe default".
- Caveat to handle: GNU make only exposes the jobserver to recipes it treats as
  recursive (the `+`-prefix convention); CMake custom commands do not inherit it
  automatically — must be wired (validated in M0).

**Architecture / the seam.** In `lib/QoreAOT.cpp::emitObjectFile`, optimization
ends at `MPM.run(module, MAM)` (~line 4604) and codegen is `addPassesToEmitFile`
(~line 4658). Cut there. When jobs>1, after opt:
1. `llvm::SplitModule(M, N, callback, …)` (present in LLVM 21;
   `llvm/Transforms/Utils/SplitModule.h`) → N deterministic partitions of
   function bodies. (`llvm/CodeGen/ParallelCG.h`/`splitCodeGen` was removed in
   modern LLVM, so we hand-roll the thread pool.)
2. Registration glue + metadata blob (`generateMainAndTableV2`) stay in one
   designated partition (codegen'd on the main thread), exactly like the
   `--from-objects` glue object; other partitions' functions are referenced as
   external symbols, resolved at link.
3. Each worker: bitcode round-trip its partition into a **fresh `LLVMContext`**
   (`BitcodeWriter`/`BitcodeReader`; cross-context `Value*` is UB), build its own
   `TargetMachine`, `addPassesToEmitFile` → a `.o`.
4. Feed the N `.o`s + glue to the existing aggregate linker
   (`linkSharedLibMulti`); it concatenates each part's `qore_aot_pcloc` section,
   so lazy locations keep working with no extra code.

**Hard problems (and handling):**
1. *Determinism* (content-digest stamps + reproducible builds): pin
   `SplitModule`'s hash-based assignment, sort partitions, fix link-input order.
   Gate: `--jobs N` twice ⇒ identical bytes.
2. *Cross-partition & internal symbols*: external AOT symbols resolve at link
   (proven by the aggregate path); the risk is `internal`/`static` helpers split
   from their sole caller. Use `SplitModule`'s locals handling (keep-with-refs or
   promote to hidden-external). Validate on a module with internal helpers.
3. *Metadata/registration partition*: one glue object holds the blob + registry;
   no new merge logic (mirrors `--from-objects`).
4. *Oversubscription*: the jobserver (above).

**Milestones (in-branch):**
- **M0 — de-risk spike** (½–1 session, throwaway/env-gated, not pushed):
  prove SplitModule → bitcode round-trip → per-thread codegen → aggregate-link
  produces a loadable, deterministic, test-passing `.qmod` for a real module;
  and prototype a jobserver client throttling under a real `make -jN`
  (including the CMake-custom-command caveat). Kill criterion: if determinism or
  internal-symbol resolution can't be made clean, stop and reconsider.
- **M1 — seam refactor** (1 session): split `emitObjectFile` into
  `optimizeModule()` + `codegenModuleToObject()`, `--jobs 1` path **byte-
  identical** to today. Land alone; full AOT suite green.
- **M2 — parallel path + jobserver client** (1-2 sessions): `--jobs N`/`$QCC_JOBS`
  + jobserver; SplitModule → thread pool → aggregate link. Gates: `--jobs 1`
  byte-identical; `--jobs N` load-equivalent + deterministic + valgrind clean;
  AOTSmoke/AOTSymbolIndex/exception-location green on ELF + macOS.
- **M3 — default-on + measure** (½ session): on by default under make jobserver /
  Ninja pool; benchmark DataProvider/SqlUtil/HttpServer at jobs 2/4/8/16; confirm
  ~45% + lazy locations survive (re-run single/multi/two-object backtrace checks).

**Deliverables:** DataProvider clean compile ~31 s → ~17-18 s at jobs≥8;
`--jobs 1` matches current bytes; load-equivalence + determinism tests added;
no runtime-perf regression (split-after-opt). Pairs with Phase 2 (caching) for
the unchanged-function dev-loop case.

### Phase 4: `.qo` object files + qorus-core linkage

**Goal:** make Qore-compiled code linkable into C++ binaries.

**Size:** large. 2-3 sessions.

**Status: SUBSTANTIALLY SHIPPED** on `feature/aot-build-link-stabilization`.
Delivered: `qcc -c` per-file `.qo`, `qcc -o app *.qo` executables-from-objects,
`qcc -m --from-objects` aggregate `.qmod`, `qcc -a` `.qoa` archives, the
cross-link symbol/index model (`qore-qo-source-order`), and lazy exception
source locations that survive arbitrary downstream relinking via the
`qore_aot_pcloc` section (ELF + Mach-O). Remaining/under validation: full
qorus-core static-link integration test, and the open design questions below
(symbol-namespace policy, static-init ordering) for embedding into large C++
binaries. The original list below is retained as the contract; most items are
now implemented — see `design/aot-object-files-and-module-artifacts.md` and
`design/aot-script-context.md` for the as-built design.

**Current state:**
- `.qmod` already contains compiled `.o` bytes + metadata blob. On
  module load, the `.o` is mmapped and code is executed via runtime
  relocation.
- Symbols are internally named (e.g., `Qore$Module$Func$variant`).
- Module relies on libqore.so runtime helpers resolved dynamically.

**What qorus-core linkage needs:**
1. **Stable, external symbol names** — linkable with `ld` or
   `lld`. E.g., `qore_HttpServer_HttpServer_constructor_v0`.
2. **External symbol resolution** — unresolved references to
   `qore_rt_*` resolved at link time against libqore.a (or .so at
   runtime).
3. **Runtime init hooks** — module init function exported as
   `qore_HttpServer_init()` (or similar), called by application init.
4. **Metadata accessibility** — class list, function signatures,
   module-info blob accessible at link/load time for introspection.
5. **Archive format** — `.qo.a` static lib / `.qo` single-function
   objects / `.qolib` shared lib.

**Proposed format:**
- `.qo`: single Qore compilation unit, emitted as ELF object file
  (Linux) / COFF (Windows) / Mach-O (Darwin). Format same as what
  the system `ar` / `ld` expect.
- `.qoa`: static archive (Unix `ar` format) containing multiple `.qo`
  files + metadata section.
- Linked into executable with `ld -lqore /path/to/myapp.qoa`.
- Requires `libqore.a` or `libqore.so` at link time.

**Open design questions:**
- Namespace pollution: all exported symbols need a naming convention
  that avoids clashes with C/C++ code in the binary.
- Initialization order: when a Qore module is statically linked,
  `qore_XXX_init()` must run before any code that references
  module classes/functions. Use `__attribute__((constructor))` or
  explicit init sequence.
- Class registration: currently done at runtime via `QoreClass::create`
  in module init. Compile-time registration harder; runtime init is
  probably still the way.
- Version compatibility: if Qore source changes between linker-input
  and runtime libqore, what happens? Mandate same version or gracefully
  fail.

**Deliverables:**
- `qcc -o myapp.qo myapp.qm` produces an ELF object.
- `ar rcs myapp.qoa myapp.qo` creates a static archive.
- Link example: compile a trivial C++ `main()` that calls into a
  Qore-compiled function via the exported symbol.
- Integration test: build qorus-core with some Qore sources pre-compiled
  and linked. Smoke-test that the resulting binary runs.

**Informs:** whether Qore can replace runtime parsing entirely for
production deployments.

### Phase 5a: OptimizeNone escape hatch for large functions — SHIPPED

**Commit:** `f0f75a301` (2026-04-18).

**Mechanism:** when a function's basic-block count ≥
`QORE_AOT_BIG_FN_THRESHOLD` (env) or `qcc --big-fn-threshold=N`, the LLVM
function gets `Attribute::OptimizeNone` + `NoInline`. LLVM then bypasses
SelectionDAG's expensive passes on that function while still producing
correct code.

**Results (HS.qm, threshold=200):**
- Compile: **623s → 20.8s** (30x).
- Output size: **18MB → 5.6MB** (3.2x).
- Runtime: ~1% regression call-heavy, ~7% arithmetic-heavy microbench.
- 4 functions tagged (handleRequest 905 BBs, sendReply 254,
  handleMultiplexedPersistentSync 278, handlePersistentConnectionSync 253).

**Status:** opt-in. Default threshold=0 (disabled). Flag promoted to
`qcc --big-fn-threshold=N`.

**Validation measurements (2026-04-18):**

| Module     | Funcs | Baseline | threshold=200 | Speedup | Output   |
|------------|-------|----------|---------------|---------|----------|
| HttpServer | 198   | 623 s    | 13.6 s        | **46x** | 18 → 5.7 MB |
| SqlUtil    | 747   | 293 s    | 172 s         | 1.70x   | 19.9 → 15.6 MB |
| OpenApi3   | 201   | 29.2 s   | 29.2 s        | 1.00x   | identical |

- HttpServer is the pathological case (one 905-BB function) — 46x win.
- SqlUtil has multiple medium-large functions but no single outlier —
  still a real 1.70x / 22% smaller win with no runtime regression.
- OpenApi3 has zero functions crossing threshold=200 — Phase 5a is a
  no-op; same SHA256. Confirms the flag doesn't touch small modules.

Runtime correctness: HS qtest 56/56 green with p5a qmod (p77); SqlUtil
has a **pre-existing** AOT runtime SEGV (separate issue, unchanged by
Phase 5a — identical behavior in base and p5a variants).

**Outstanding:**
- Decide default: threshold=200 vs remain opt-in. Recommendation: **flip
  to 200 by default**. The 46x win on pathological code with negligible
  cost on non-pathological modules is a clear net positive. Users who
  want the old behavior can set `--big-fn-threshold=0`.
- Consider tier: `-O3` → threshold 200; `-O2` → threshold 300; `-O0` →
  tag everything.

### Phase 5b: Outlining large functions (future, lower priority)

**Goal:** reduce per-function codegen cost by splitting monolithic
functions. Pursue only if Phase 5a's runtime cost proves unacceptable.

**Size:** medium. 1-2 sessions.

**Architecture:**
- At Qore IR generation time (in `QoreIRBuilder` or similar), detect
  functions with >N basic blocks or >M instructions.
- Split each try-catch body or each switch arm into its own helper
  function.
- Original function becomes a driver that calls helpers.
- LLVM SelectionDAG is near-linear per function (within normal sizes);
  by dropping handleRequest from ~500 BBs to 10 helpers of ~50 BBs
  each, per-function codegen time drops 2-10x — with runtime
  optimization preserved (unlike Phase 5a).

**Risks:**
- Runtime overhead: extra function calls. Mitigated by LLVM inliner
  possibly reconstructing the original at O3 (but that'd negate the
  compile-time benefit).
- Complexity: outlining needs to preserve Qore semantics (variable
  scoping, exception boundaries).

**Deliverables:**
- HS handleRequest compile time with outlining: should be a fraction of
  current.
- Runtime micro-bench: ensure no >3% runtime regression from extra
  calls.

**Informs:** only pursued if Phase 5a's 1-7% runtime cost is a blocker
for production workloads.

### Phase 6: Lazy codegen for cold functions

**Goal:** skip AOT for rarely-called functions; JIT them on demand.

**Size:** medium. 1-2 sessions.

**Architecture:**
- Cold-function detection: simple heuristic (function has no callers,
  or is only referenced in one place), plus annotation
  (`@cold` attribute if Qore adds one).
- `.qmod` contains only stubs for cold functions. Full IR retained
  (already-compiled qmod doesn't need it, but AOT-split modules would).
- Runtime: first call to stub triggers JIT compilation + replacement.

**Risks:**
- Runtime pauses at first invocation of each cold function. Measurable
  latency bump.
- JIT compilation of cold functions at runtime adds per-process work.

**Deliverables:**
- Flag `--lazy-cold[=N]` where N is BB threshold.
- HS compile time with lazy cold: additional savings on top of prior
  phases.

**Informs:** whether this tradeoff is worth it for the target audience.

### Phase 7: LLVM pipeline tuning

**Goal:** per-opt-level fine-tuning of the LLVM pass pipeline and
codegen options.

**Size:** small-medium. 1-2 sessions (data-driven).

**2026-06-26 note — the gross opt-level knob is a dead end.** Measured
DataProvider at each level: `-O0` 21.6 s, `-O1` 28.7 s, `-O2` 32.3 s, `-O3`
30.7 s. Because the build is codegen-bound (codegen ≈ optimization), even `-O0`
only saves ~30% (and de-optimizes runtime). `-O2`≈`-O3` for build time, so
there is no free build-time win from lowering the default opt level. Any Phase 7
value must come from *targeted* per-pass/codegen toggles (the items below) or
from `--big-fn-threshold` (Phase 5a), not from the opt-level dial.

**Likely wins (informed by Phase 1 trace):**
- Disable SLP vectorization if Qore code doesn't benefit.
- Reduce loop unrolling.
- Cap inlining depth.
- Skip specific backend passes (e.g., MachineLICM if it's slow for our
  patterns).

**Deliverables:**
- Per-pass timing before/after table.
- HS compile time improvement from each toggle.

**Informs:** nothing downstream; this is incremental.

## Phase dependencies

```
Phase 1 (time-trace, debug-info)                         — SHIPPED
    ↓
Phase 5a (OptimizeNone big-fn)                           — SHIPPED
    ↓
Phase 4 (.qo/.qoa object files + linkage)                — SUBSTANTIALLY SHIPPED
    ↓
    ├→ Phase 2 (caching)            — dev-cycle win; subsumes per-file incremental.
    ├→ Phase 3 (parallel codegen)   — measured ~45% on big modules (split-after-opt,
    │                                  safe); main win is critical-path tail + incremental.
    │                                  Reuses Phase 4 aggregate-link plumbing.
    ├→ Phase 5b (outlining)         — only if 5a runtime cost is a blocker.
    ├→ Phase 6 (lazy)               — optional polish.
    └→ Phase 7 (LLVM tuning)        — incremental; opt-level dial is a dead end (measured).
```

## Debug-info tier (CMake-style)

Introduce three levels, selectable via qcc flags:

| qcc invocation | Opt | Debug info | Analogy |
|---|---|---|---|
| `qcc -O0 -g`         | `-O0` | DWARF | `Debug` |
| `qcc -O3`            | `-O3` | none | `Release` |
| `qcc -O3 -g`         | `-O3` | DWARF | `RelWithDebInfo` |
| `qcc -O1 --strip-debug` | `-O1` | none | minimal |

`-g` flag toggles `emit_debug_info` in `QoreIRToLLVM`. Mirrors clang/gcc.

## Entry point for next session

Phases 1, 5a shipped; Phase 4 substantially shipped (this branch). The
build is well-parallelized across modules and incrementally scoped; the
gross opt-level dial is a measured dead end. Pick one of:

1. **Phase 2 (per-function caching)** — best dev-loop ROI; content-addressed
   `.o` cache keyed on IR+toolchain hash, skips codegen for unchanged
   functions. Subsumes most of the disabled per-file `.qo` incremental win
   without its re-parse penalty.
2. **Phase 3 (parallel codegen)** — the one remaining large clean-build lever
   (~45% on big modules via split-after-opt). Architectural change to the
   emit path; reuses the Phase 4 aggregate-link plumbing. Do as a dedicated,
   opt-in effort — not on a stabilization branch. Determinism + metadata
   partition are the risk, not the linking.
3. **Validate Phase 5a runtime cost** on a representative workload; decide
   whether to flip the `--big-fn-threshold` default to 200.
4. **Phase 4 finish** — qorus-core static-link integration test + the open
   symbol-namespace / static-init-ordering design questions.

## References

- `design/aot-eh-cleanup-dominance.md` — SSA-direct re-enable
  (LOW priority per codegen-dominated profile).
- `docs/plans/aot-phase2b-step5-exception-check-catalog.md` — Step 5
  catalog.
- Memory: `session_2026_04_18_p70_aot_step5_phase_a_iterator_pilot.md`.
