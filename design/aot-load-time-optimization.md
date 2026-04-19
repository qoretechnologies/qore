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
2. **Next: profile inside `deserializeMethods`** — split
   `readAndSetupVariantSignature` vs variant allocation vs
   addUserMethod insertion.  Confirms which sub-tactic to try
   first.
3. **Low-risk quick wins**:
   - (1a) shared type resolver cache across sessions;
   - (1d) per-session arena allocator for method variants.
   Expected combined: 20-30% drop in `deserializeMethods` → ~200 ms.
4. **Medium effort, larger win**:
   - (1b) intern type paths in `.qo` metadata (format bump);
   - (3) parallelize `deserializeFunctionsAndMethods` across
     sessions after thread-safety audit.
5. **Re-measure** at each step.  Target: resolveAll total ≤ 500 ms
   on qwf (half of current 1073 ms).
6. If still off: (2) finalize index rebuild optimization.
7. Defer (4) lazy commit and (5) SML short-circuit unless
   profiling specifically calls for them.  The current measurements
   suggest they'd be <1% wins.
8. Always keep (6) source-parse fallback available.

## Acceptance criteria

qwf / qsvc / qjob `--help` median startup <= current source-parse
baseline (1.80 / 1.86 / 1.35 s).  Stretch goal: <= 1.0s for all
three.

## Tracking

New task: "AOT load-time optimization".  Split into sub-tasks
matching proposals (1)-(5) once (1) profiling data is in.
