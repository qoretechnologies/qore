# AOT Compile-Time Optimization Roadmap

**Status:**
- Phase E (EH default-on) shipped at `8d392571f` (2026-04-18).
- Phase 1 (time-trace + debug-info flags) shipped at `c828cc1a8`.
- Phase 5a (OptimizeNone big-fn escape hatch) shipped at `f0f75a301`.
  **HS compile: 623s → 20.8s (30x)** with `--big-fn-threshold=200`.

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

### Phase 3: Parallel per-function codegen

**Goal:** parallelize backend codegen across cores.

**Size:** large. 2-3 sessions.

**Architecture:**
- `generateModuleABIV2` currently builds one `llvm::Module` with all
  functions, runs `MPM.run(module, MAM)` serially, emits one `.o`.
- New flow:
  - Partition functions into N chunks (N = `std::thread::hardware_
    concurrency()` or `$QCC_JOBS`).
  - For each chunk, start a worker thread with its own fresh
    `llvm::LLVMContext` + `llvm::Module`.
  - Each worker clones the runtime helper declarations + its function
    slice into its own module, runs the pass pipeline, emits `.o` bytes
    to a buffer.
  - Main thread waits, concatenates the `.o` buffers into the final
    `.qmod`.
- LLVM constraint: **every thread needs its own `llvm::LLVMContext`**.
  Cross-context `llvm::Value*` is UB. Our current code shares one
  context; refactor required.

**Cross-function references:** currently functions call into each other
(e.g., handleRequest → helper method). In split modules:
- Callee declared in both modules; runtime linker resolves.
- Or: first emit all function declarations into a shared "header" IR
  that each worker imports.

**Deliverables:**
- HS compile time with `QCC_JOBS=8`: should drop from 623s toward
  ~80-150s depending on parallel efficiency.
- Verify all baselines + qmod tests green.
- Compare single-threaded fallback (`QCC_JOBS=1`) vs current code —
  should match.

**Informs:** whether 120s goal is met with just parallelism, or
additional phases needed.

### Phase 4: `.qo` object files + qorus-core linkage

**Goal:** make Qore-compiled code linkable into C++ binaries.

**Size:** large. 2-3 sessions.

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
    ├→ Phase 4 (.qo object files)   — TOP PRIORITY; enables qorus-core.
    ├→ Phase 2 (caching)            — dev-cycle win.
    ├→ Phase 3 (parallel)           — lower priority post-5a.
    ├→ Phase 5b (outlining)         — only if 5a runtime cost is a blocker.
    ├→ Phase 6 (lazy)               — optional polish.
    └→ Phase 7 (LLVM tuning)        — incremental.
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

Phases 1 and 5a shipped. Pick one of:

1. **Phase 4 (.qo object files)** — user-called-out goal, orthogonal
   to compile-time work, unblocks qorus-core linkage.
2. **Validate Phase 5a runtime cost** on a more representative workload
   than the synthetic microbenches; decide whether to flip the
   `--big-fn-threshold` default.
3. **Phase 2 (caching)** — dev-cycle ergonomics.

## References

- `design/aot-eh-cleanup-dominance.md` — SSA-direct re-enable
  (LOW priority per codegen-dominated profile).
- `docs/plans/aot-phase2b-step5-exception-check-catalog.md` — Step 5
  catalog.
- Memory: `session_2026_04_18_p70_aot_step5_phase_a_iterator_pilot.md`.
