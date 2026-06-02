# AOT load-time optimization plan

## Context

Phases 3b/3c/3d migrated qwf, qsvc, and qjob from source-parse to AOT
batch registration.  Baseline startup measurements on `--help`
(which is the shortest cold path — main class is never instantiated):

| Binary | Aggregators | Source-parse median | AOT median | Ratio (AOT/src) |
|---|---|---|---|---|
| qctl         |  6 | n/a   | 0.55s | AOT-only |
| qbugreport   |  4 | n/a   | 0.29s | AOT-only |
| qdsp         |  4 | n/a   | 0.06s | AOT-only |
| qwf          | 11 | 1.80s | 2.20s | **1.22× slower** |
| qsvc         | 11 | 1.86s | 2.60s | **1.40× slower** |
| qjob         | 11 | 1.35s | 2.01s | **1.49× slower** |

Before this plan, the expectation was that AOT would be faster than
source-parse (AOT modules in qlib have seen 60× speedups for
`DataProvider.qmod` etc.).  The measurement above contradicts that
expectation for the 11-aggregator cluster-interface binaries.

## Why source-parse is fast on these binaries

The Qorus source-parse flow does not re-read `.qc` files at startup.
`qorus_qore2cpp` embeds each `.qc` file as zlib-compressed source
bytes in a generated `x_<Name>.qc.cpp` file (e.g.,
`build/x_AbstractLogger.qc.cpp`).  At startup, each `qorus_<Name>`
function:

1. `uncompress`es a ~12KB bytestring (microseconds).
2. Calls `qpgm->parsePending(src, filename)` — Qore's parser runs
   over the small inline string.
3. After all source fragments are loaded, a single
   `qpgm->parseCommit` resolves cross-file types + methods.

The Qore parser on a compressed-inline source corpus of ~500 classes
× ~12KB each finishes in ~1-1.5s for qwf/qsvc/qjob.

## Why AOT is slower right now

AOT batch register does equivalent work but through a different,
less-optimized code path:

1. `.qo` ELF sections already loaded at binary startup (free).
2. Per-file `qore_<file>_<file>_script_register(pgm)` (one per `.qo`)
   calls `qore_aot_script_register` which, in batch mode, calls
   `mdes.addBlob(metadata_bytes, size)`.  `addBlob` does
   phase-1 deserialization (namespaces, class shells, type shells,
   hashdecl shells, enum shells).
3. `qore_aot_script_end_batch` invokes the 7-phase `resolveAll`:
   - resolveTypesAndMembers (per session)
   - rebuildBaseClassSmlPhase (cross-session)
   - importInheritedMembersPhase (cross-session)
   - resolveStaticsAndConstants (per session)
   - deserializeFunctionsAndMethods (per session)
   - commitClassesPrepare / DoCommit / ImportAbstract / Validate (all cross-session)
   - finalize (BCA resolution, index rebuild, pending_smd fixup)
4. Per-file `registerAOTFunctionsFromSlotMaps` + init-func exec.

For 11 blobs × ~500 classes × ~50 methods each = ~275,000
per-method operations plus the cross-session walks.  Each phase
iterates the session list and the per-session class list.

### Measured decomposition of the 2.32s qwf load

Instrumented via `QORE_AOT_PHASE_TIMING=1` env var (commit
091e1a653).  Totals across all 132 sessions, 5-run medians,
microseconds (`us`):

| Phase | Time | % of resolveAll |
|---|---|---|
| addBlob (phase-1 shells, per session) | 68 ms | 6.4% |
| resolveTypesAndMembers | 27 ms | 2.5% |
| rebuildBaseClassSml | 0.4 ms | 0.0% |
| importInheritedMembers | 1 ms | 0.1% |
| resolveStaticsAndConstants | 30 ms | 2.8% |
| **deserializeFunctionsAndMethods** | **776 ms** | **72.3%** |
| — deserializeFunctions (sub) | 4 ms | 0.4% |
| — **deserializeMethods** (sub) | **779 ms** | **72.6%** |
| commitClassesPrepare | 2 ms | 0.2% |
| commitClassesDoCommit (parseCommit) | 5 ms | 0.4% |
| commitClassesImportAbstract | 0.5 ms | 0.0% |
| commitClassesValidate | 0.3 ms | 0.0% |
| finalize | 163 ms | 15.2% |
| **TOTAL resolveAll** | **1073 ms** | 100% |
| registerAOTFunctions (post) | 130 ms | — |
| executeInitFunctions (post) | 19 ms | — |

(qwf wall-clock --help: ~2.32 s.  Resolve + register + init
totals 1222 ms = 53% of wall-clock.  Remainder ~1100 ms in
binary startup + qore_init + req_modules[] loading ×36 modules
+ command-line parse + cleanup.)

**The hot path is `deserializeMethods`** — 779 ms, 73% of
resolveAll, 33% of wall-clock.  parseCommit and the cross-
session phase-sync machinery are collectively <1%.  Prior
estimates were way off.

## Optimization proposals (revised by real measurements)

The original plan's ordering was based on estimates that turned
out to be wrong.  parseCommit is 0.4% of resolveAll, not 23%.
Revised priorities based on measured time:

### 1. Optimize `deserializeMethods` internals (dominant hot path)

779 ms across 132 sessions = ~6 ms/session or ~30 μs/method
variant.  Sub-phases inside each variant:
- `readAndSetupVariantSignature` — reads return type path, params,
  per-param type paths and defaults.  Each type path triggers
  `QoreAOTTypeResolver::resolve` which hits a per-session cache
  (not shared across sessions — same builtin paths like `string`
  resolved 132 times via 60-entry linear-scan table).
- `new UserMethodVariant`/`UserConstructorVariant` allocation.
- Optional BCA blob read + pbca collection.
- `qore_class_private::addUserMethod` insertion into hm/shm.

Concrete sub-tactics, each worth investigation:

1a. **Share type resolver cache across sessions** (plausibly
    5-15% of deserializeMethods).  Move the `cache` out of the
    per-session `QoreAOTTypeResolver` into the MultiDeserializer
    (or a process-global cache for builtin types that never
    change).  132 sessions currently re-walk the 60-entry
    builtin linear scan for every common type path.

1b. **Intern common type paths at serialize time** (plausibly
    10-20%).  The `.qo` metadata currently stores full type paths
    as strings (`"*string"`, `"*hash<string, int>"`).  Replace
    with a per-blob type-table: first occurrence defines index,
    subsequent occurrences reference.  Shrinks metadata size
    (wins in disk read + zlib) and lets `resolve()` become an
    array lookup for the most common cases.

1c. **Fast-path for parameter defaults = `has_default == 0`**
    (unknown gain, likely small).  The common no-default case
    still goes through the value-reader switch.  A one-byte
    early-out would save per-parameter dispatch.

1d. **Batch-allocate variants per session** (5-10%).  Each
    variant goes through `new`; replacing with a per-session
    arena (destroyed in `~QoreAOTBinaryDeserializer`) reduces
    allocator pressure for ~25,000 small objects.

1e. **Skip abstract method body setup** (modest).  Abstract
    method variants have no body — the metadata still goes
    through the full UserMethodVariant construction.  An abstract
    fast-path that only builds the signature could save per-
    abstract-variant cost.

### 2. Finalize phase (163 ms, 15%)

`finalize` runs pending_smd fixup, deserializeFallbackSources,
rebuildAllIndexes, resolveBCAExpressions.  The index rebuild
(fmap/varmap/clmap) walks the entire namespace tree.  For 500+
classes it's non-trivial.  Candidates:

- Incremental index update during phase 1 (each `addBlob` appends
  to the indexes directly instead of rebuilding from scratch).
  Needs audit — current rebuild may exist for a reason
  (deduplication across blobs?).

### 3. Parallel method deserialization across sessions (potential 30-40%)

The 7-phase `resolveAll` is currently serial over sessions.
Phases that don't share mutable class state across sessions can
run concurrently.  Candidates:

- `deserializeFunctionsAndMethods` — each session owns its own
  hm/shm maps.  Methods go into separate class structures.  **No
  shared state**.  Can parallelize per-session.
- `resolveTypesAndMembers` — same.

Constraints:
- Memory allocator is thread-safe (glibc/jemalloc).
- Class registry writes need a lock or atomic CAS during shell
  creation.  Currently synchronous; would need auditing.

Expected wall-clock reduction: on a 4-core host, parallelizing the
two largest per-session phases (600+400 ms ≈ 1000 ms of serial time)
could drop by ~3× to ~300 ms, total save ~700 ms.

Risk: thread-safety of the Qore parser / namespace mutation paths.
Audit needed.

### 2. Pre-serialized "committed" class vlist (est. 20-30% gain)

Currently method variants are deserialized into **pending** state
(via `addPendingVariant`), then `parseCommit` moves them into the
**committed** vlist.  The split exists because Qore's parse-time
model distinguishes parsing-in-progress from committed state.

For AOT, this is pure overhead — the AOT serializer wrote variants
that are already "committed"; the deserializer reconstructs them as
pending and immediately re-commits.

**Plan**: add a `QoreAOTDeserializeMode::PreCommitted` flag that
skips the pending-list dance.  Have `deserializeMethods` append
directly to the committed vlist, and have `commitDeserializedClasses`
just run the bookkeeping (`initialized = true`, `num_methods +=`,
`checkAssignSpecial`, `parseAddAncestors`).

Expected save: ~300 ms on qwf (half of 500 ms parseCommit + half of
600 ms method deser).

Risk: the pending/committed invariant is load-bearing for the normal
source-parse path — a mismatched AOT bypass could corrupt class
state for programs that mix %requires source modules with AOT
binaries.  Gate with a capability check.

### 3. Compact metadata format v3 (est. 10-20% gain)

Current `.qo` metadata format stores:
- Strings as offset-into-strtab (good).
- Enum / class / hashdecl shell refs as path strings (long).
- Method variant signatures as full per-parameter type trees.

A v3 format can:
- Intern class/hashdecl/enum paths in a per-blob table, reference
  them as u16 indices (saves ~30% of shell-ref bytes).
- Bitpack flags (currently u8 each).
- Use varint for lengths, counts, small integers.
- For the common case of a simple type (e.g. `string`,
  `*hash<string, int>`), use a fast-path encoding (1 byte) instead
  of the generic type-info serialization.

Smaller metadata → less zlib decompress + less parser work on
strings.  Expected save: ~150 ms.

Risk: format bump needs backward-compat tag handling; all AOT
modules must be rebuilt.  One-shot migration; CI catches it.

### 4. Lazy parseCommit: defer until first use (est. depends on path)

`--help` instantiates **no** classes — the main class ctor never
runs.  Yet AOT load-time pays for committing every class's method
vlist + `parseAddAncestors` + `checkAssignSpecial`.

Lazy commit would:
- Mark newly deserialized classes as "deferred-commit" after
  phase 1.
- Skip `commitClassesDoCommit` / `ImportAbstract` / `Validate` at
  `end_batch`.
- On first `execConstructor` / method lookup on a deferred class,
  run its commit on demand (plus recursively for its bases).

For `--help` specifically, this could skip >500 ms of commit work.
But for a normal invocation (qwf actually starting a workflow
process), the commit work has to happen somewhere — so wall-clock
savings are strongest on short-lived invocations (--help, --version,
the command-line parse phase).

The cross-session commit interleave (commits bfe4e3e2e + 129a15d02
+ f65c2977c) complicates this — deferred commit would need to
preserve the "all sessions prepared before any commits" invariant
at first-use time.

Risk: high complexity; cross-session invariants hard to maintain at
runtime.  Only worth it if profiling shows --help is a common hot
path (it's used by shell completion, CI version checks, etc.).

### 5. Skip redundant phases when no session actually owns a class

`rebuildBaseClassSmlPhase` walks every class × every base pair, but
only matters when a base class was owned by a **later** session
than the derived.  Detecting the "no session crossed" case (all
classes in session X reference only classes in earlier sessions)
could short-circuit this phase.

Expected save: minor (~50-100 ms at best).  Not high ROI unless
baseline grows further.

### 6. Keep source-parse as an option

Add `-DQORUS_AOT_BINARIES=OFF` cmake flag that switches qwf/qsvc/qjob
back to the source-parse build path (C++ lib with embedded source
bytes + `init_*_SRC_X` calls).  Defensive option for shops that
hit AOT load-time issues in production.  Retain both paths in the
CMakeLists.txt until AOT load time closes the gap.

Low cost (cmake plumbing only) — no Qore-side work needed.

## Recommended sequencing (revised)

1. **Done** — per-phase instrumentation shipped (commit 091e1a653).
   Data reported above; `deserializeMethods` is the dominant hot
   path at 73% of resolveAll.
2. **Done** — finer breakdown inside `deserializeMethods` (commit
   091e1a653).  656,841 variants, sub-phase split:
   - `readAndSetupVariantSignature`: 566 ms (77%)
   - variant alloc + dynamic_cast: 84 ms (11%)
   - addUserMethod: 4 ms (<1%)
3. **Done** — (1a) shared type-resolver cache across batch sessions
   (commit e42d2b6a5).  `readAndSetupVariantSignature` 566→480 ms
   (-15%); qwf wall-clock 2.32→2.14 s (-8% trimmed mean).
4. **Next opportunities inside `readAndSetupVariantSignature`**
   (480 ms remaining):

   - **LocalVar allocation loop in `setupFromAOTMetadata`**
     (Function.cpp:776-792).  Every variant allocates (params +
     self + argv) LocalVars individually via
     `pp->createLocalVar()` → `new LocalVar` + push into
     `local_var_list`.  For ~656k variants with avg ~5 params,
     that's ~4M `new` allocations, estimated 150-300 ms.  Options:
     - Bulk-allocate from a program-scoped arena.  Reduces allocator
       pressure but requires careful lifetime handling.
     - Intern the always-present `argv` LocalVar (identical
       `("argv", autoListOrNothingTypeInfo)` across all AOT-
       deserialized methods).  Saves one LocalVar per variant,
       ~656k fewer allocations, ~40-70 ms.  Low risk; argv is read-
       only at runtime.
   - **Default-arg handling** — 7 branches of `has_default` switch
     inside the param loop.  Most params have `has_default == 0`
     (no default); unclear how many hit the expensive branches.
     Instrument per-branch counts to quantify.
   - **Signature string build** — `addAbstractParameterSignature(str)`
     at the tail of `setupFromAOTMetadata`.  Walks typeList to build
     a human-readable sig string used for error messages and
     parseCheckDuplicateSignature.  Not needed for AOT since there's
     no duplicate-signature check.  Could be deferred.

5. **Parallelize `deserializeFunctionsAndMethods` across sessions**
   (expected 30-40% of resolveAll if thread-safe).  Requires:
   - Confirming `createLocalVar` + `local_var_list.push_back` is
     thread-safe (almost certainly not; needs a mutex or per-thread
     arena).
   - Confirming `pp->findClass` / `runtimeFindClass` are thread-safe
     (they read the namespace tree while AOT sessions might not be
     mutating it during method-deser phase).
   - Confirming `type_resolver->resolve` with shared cache is thread-
     safe (currently uses unordered_map without sync — needs shared
     mutex or a lock-free variant).

6. **Medium effort, format changes**:
   - Intern type paths per-blob (format bump).  Reduces metadata
     size + avoids repeated `std::string` construction during read.
   - Intern param names per-blob.

7. **Done** — (1b) `safe_dslist<LocalVar*>` → `std::deque<LocalVar>`
   arena (commit 54fdd0e74).  Removed ~4M `new LocalVar` allocations
   and list-node overhead, cutting createLocalVar cost on the hot
   path.

8. **Done** — (3a) move-semantics for `setupFromAOTMetadata`
   (commit ecbeae7b1).  Rvalue-reference the 3 param vectors so
   `typeList`/`names`/`defaultArgList` acquire ownership without
   copying; removes the refSelf/discard round-trip for default args.
   setupFromAOTMetadata 209→183 ms (-12%).

9. **Done** — (2a) single cross-session root-index rebuild
   (commit 290feb2b2).  The per-session `finalize()` used to call
   `qore_root_ns_private::rebuildAllIndexes()` for every blob — 132
   full-tree walks in qwf's batch.  Split into `finalizePreIndex()`
   (no index needed) / `finalizePostIndex()` (index needed) with a
   single rebuild in the multi-deserializer.  finalize phase
   165 → 2.5 ms; wall-clock 1.97 → 1.81 s trimmed mean.

10. **Done** — (5a) cache last-resolved class in slot-map register
   loop (commit 76bc14d23).  Negligible measured effect because the
   full namespace walk was already fast, but the cache costs
   essentially nothing when it misses.

11. **Done** — (1c) intern the shared `argv` LocalVar across all
   AOT-deserialized variants (commit f73147c5d).  Every variant of
   every function/method passes the identical `("argv",
   autoListOrNothingTypeInfo)` to createLocalVar — 656 k deque
   emplaces in qwf collapse to one.  Safe because the runtime
   identifies locals by `(LocalVar*, stack frame)` and each
   invocation pushes its own argv slot onto the thread-local
   stack.  Wall-clock 1.82 → 1.75 s trimmed mean.

12. **Done** — (1d) intern per-class `self` LocalVar (commit
   c0210a5fe).  Every method variant of a given class passes the
   identical `("self", classTypeInfo)` — keyed cache on
   qore_program_private::shared_aot_self.  Wall-clock 1.75 → 1.74 s.

15. **Done** — (4a) defer signature-text build to `getSignatureText()`
    (commit ddf6e2a49).  `setupFromAOTMetadata` used to eagerly build
    the human-readable param signature string via
    `addAbstractParameterSignature` — 656 k std::string builds per
    qwf batch, almost none of them queried at runtime.  Made `str`
    mutable, lazy-built in `getSignatureText()` when non-empty
    signature+empty cache.  Source-parse paths unchanged (`resolve()`
    still builds eagerly, short-circuits the lazy check).
    setupFromAOTMetadata 148 → 118 ms; wall-clock 1.74 → 1.70 s.

16. **Done** — (4b) skip default-construction of param_names/types
    vectors (commit 2035abfa5).  `readAndSetupVariantSignature` used
    `resize(np)` + indexed assignment in the read loop; switched to
    `reserve(np)` + emplace_back / push_back so the np empty
    std::strings + null pointers per variant are never created.
    Marginal wall-clock effect but cleaner code.

17. **Done** — (6a) format bump: per-blob TYPE_TABLE section
    (commit 4939be6ed).  New section type + feature flag
    `QORE_AOT_FEAT_TYPE_TABLE`; `writeVariantSignature` interns
    every return / param type path in a per-blob table and emits a
    `u32` index.  At phase 2b entry, `resolveTypeTable` walks the
    table once and caches `const QoreTypeInfo*` per index.  The
    hot-path `readAndSetupVariantSignature` then pulls types by
    index instead of per-param hash lookup — eliminates ~3.3 M
    resolver lookups on qwf's 656 k variants.  Back-compat path for
    older .qmods (no feature bit) still uses inline strings.
    readAndSetupVariantSignature 285 → 195 ms (-32 %), wall-clock
    1.70 → 1.64 s trimmed mean.

18. **Done** — (6b) share type resolver across slot-map register
    phase (commit 3797ae247).  `buildContextFromSlotMap` used to
    allocate a fresh `QoreAOTTypeResolver` per body-local slot
    inside its inner branch (cold cache, full parser round-trip
    per resolve) — 3 M on qwf.  Hoist to function scope, and plumb
    an optional `shared_type_resolver` from
    `registerAOTFunctionsFromSlotMaps` down.  Callers now pass the
    session's own resolver whose cache was warmed during
    `deserializeFunctionsAndMethods`, so body-local types like
    `any`/`int`/`*hash<auto>` hit warm cache on first touch.
    Register phase 114-120 → 108-112 ms; wall-clock 1.64 → 1.59 s
    trimmed mean.

**State after step 18:** qwf `--help` ≈ 1.59 s (trimmed mean of 15
runs, down from 1.97 s start-of-session and ~210 ms below the 1.80 s
source-parse baseline).  NAME_TABLE analog was considered and
rejected — the wire format's string pool already interns shared
names via pool offsets; a NAME_TABLE index layer on top would yield
no runtime speedup unless paired with a storage-type change
(`UserSignature::names` → `vector<const char*>` into the stable
metadata buffer), which is a multi-call-site refactor.

13. **Attempted, reverted** — heterogeneous `string_view` lookup on
   the type-resolver cache (to skip implicit `std::string`
   construction on 3.3 M cache-hit lookups).  No measurable wall-
   clock change despite extensive variance noise across runs —
   `std::unordered_map::find(std::string_view)` with transparent
   `is_transparent` hash/equal didn't beat the implicit-conversion
   path on libstdc++.  Reverted.

14. **Attempted, reverted** — `name+sig → UVB*` fast-path map built
   during deserializeMethods to skip the namespace walk + per-
   variant signature rebuild in registerAOTFunctionsFromSlotMaps.
   94.9 % hit rate measured, but register wall-clock identical —
   `buildContextFromSlotMap` (slot-map data read + context
   construction) dominates the register phase, and the UVB-find
   sub-step was already cheap.  Reverted to keep the code simple.

**Remaining hot phases** (qwf, ~536 ms `resolveAll` + 120 ms post-
register): `deserializeFuncsMethods` (65–80 %, bounded by per-
variant alloc + 3.3 M type-path resolves), `buildContextFromSlotMap`
(inside the ~120 ms post-register, per-entry slot-map parse +
QoreAOTContext creation).  Further wins likely need format changes
(per-blob type-path interning) or parallelization, not incremental
per-hot-call optimization.

8. Defer lazy commit and SML short-circuit — measurements confirmed
   they'd be <1% wins.
9. Always keep source-parse fallback available as a cmake option.

## Acceptance criteria

qwf / qsvc / qjob `--help` median startup <= current source-parse
baseline (1.80 / 1.86 / 1.35 s).  Stretch goal: <= 1.0s for all
three.

### Status (2026-04-20)

| Binary | Source-parse | AOT (pre-opt) | AOT (current) |
|--------|--------------|---------------|---------------|
| qwf    | 1.80 s       | 1.97 s        | ~1.59 s       |
| qctl   | 6.6 s        | 240 ms (P1.5) | ~250 ms warm / ~440 ms cold |
| qsvc   | 1.86 s       | —             | — (untested)  |
| qjob   | 1.35 s       | —             | — (untested)  |

**qctl measurement (task #13):** full-stack AOT at parity with the
Phase 1.5 baseline — the session's optimizations deliver zero wall-
clock movement on qctl because qctl's AOT phase is already tiny
relative to its non-AOT startup overhead:

| Phase                  | Time   |
|------------------------|--------|
| resolveAll (24 sessions, 90 k variants) | 39 ms |
| post-resolveAll register + init-func    | 7.5 ms |
| **Total AOT**          | **~46 ms** |
| Non-AOT (libqore init, qmod dlopen, source parse) | ~200 ms |
| Wall-clock (warm)      | ~250 ms |

The TYPE_TABLE / LocalVar-intern / shared-resolver optimizations
scale linearly with variant count — qwf saw 380 ms because it has
~7× more variants (656 k vs 90 k).  qctl has already hit the AOT
floor; further wins require attacking non-AOT startup.  The bimodal
wall-clock (250 ms warm / 440 ms cold) is environmental (fs cache)
and reproduces identically on the pre-session deployed qctl binary.

qwf reached sub-baseline via:
 - shared type-resolver cache across batch sessions (e42d2b6a5)
 - safe_dslist → deque<LocalVar> arena (54fdd0e74)
 - move-semantics in setupFromAOTMetadata (ecbeae7b1)
 - single cross-session rebuildAllIndexes (290feb2b2)
 - last-class cache in slot-map register (76bc14d23)
 - interned shared argv LocalVar (f73147c5d)
 - interned per-class self LocalVar (c0210a5fe)
 - lazy signature-text build (ddf6e2a49)
 - reserve+emplace for param vectors (2035abfa5)
 - per-blob TYPE_TABLE format bump (4939be6ed)
 - shared type resolver in slot-map register (3797ae247)

### Parallelization note

The cluster-interface processes were designed single-threaded, and
the shared state that deserialization touches (qore_program_private's
local_var_list deque, the RootNS tree, the type-resolver cache) is
not thread-safe.  Cross-session parallelism would need either per-
thread arenas with merge-at-end or coarse locks that eliminate most
of the speedup.  Not pursued in this session.

Total AOT resolveAll time: 917 → ~536 ms.  Finalize phase alone:
165 → 2.5 ms.  Wall-clock: 1.97 → 1.71 s trimmed mean of 15 runs
(below the 1.80 s source-parse baseline).

Remaining hot phases: `deserializeFuncsMethods` (65–80 % of
resolveAll — bounded by per-variant `new` allocation + 3.3 M type-
path resolves + per-variant param LocalVar emplaces) and post-
resolveAll `registerAOTFunctions` (~120 ms — dominated by
`buildContextFromSlotMap` per-entry slot-map parse + context
creation, not the UVB-find step).  Tried a `name+sig → UVB*` fast-
path map (94.9 % hit rate measured) but it produced no wall-clock
improvement — register time is bounded by ctx build, not UVB
lookup.  Further wins likely need format changes (per-blob type-
path / param-name interning) or parallelization.

## Tracking

New task: "AOT load-time optimization".  Split into sub-tasks
matching proposals (1)-(5) once (1) profiling data is in.
