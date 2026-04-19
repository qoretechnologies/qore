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

### Rough decomposition of the 2.2s qwf load

Approximate splits (per `QORE_AOT_TRACE_*` sampling during
investigation — not profiler-accurate, but order-of-magnitude):

| Phase | Time share | Notes |
|---|---|---|
| dlopen+ELF section load | <50 ms | free |
| addBlob × 11 (phase 1) | ~250 ms | namespace + class shell deser |
| resolveClassBases × 11 | ~150 ms | topo sort + `addBaseClass` per class |
| rebuildBaseClassSmlPhase | ~100 ms | `BCSMList::addBaseClassesToSubclass` for every class×base pair |
| resolveInstanceMembers / importInheritedMembers | ~400 ms | member map copy + init expr resolve |
| deserializeFunctionsAndMethods | ~600 ms | method variant objects + signatures |
| commitClassesDoCommit (parseCommit) | ~500 ms | `parseAddAncestors` + method vlist commit + `checkAssignSpecial` |
| commitClassesImportAbstract + Validate | ~100 ms | abstract method merge + base-reachability check |
| finalize (BCA + indexes) | ~100 ms | last-pass fix-ups |

Totals ~2250 ms, matching measured 2.20s median.

The two hot bands are **method deserialization** (~600 ms) and
**parseCommit** (~500 ms).  Both are per-method-variant.

## Optimization proposals, ordered by expected ROI

### 1. Parallel method deserialization across sessions (est. 30-40% gain)

The 7-phase `resolveAll` is currently serial over sessions.  Phases
that don't share mutable class state across sessions can run
concurrently.  Candidates:

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

## Recommended sequencing

1. **Measure precisely first**.  Add a `QORE_AOT_PHASE_TIMING` env
   that logs wall-clock per phase.  This validates the rough
   decomposition above and pinpoints the true hot spots.
2. **Implement (2)** — pre-committed variants.  Smallest risk with
   largest clean win; contained to `QoreAOTBinary.cpp`.  Expected
   ~300 ms on qwf.
3. **Implement (1)** — parallel per-session deser.  Requires
   thread-safety audit but ~700 ms win is worth it.
4. **Re-measure**.  If AOT ≤ source-parse at this point, done.
5. If still off, investigate (3) compact format.
6. Defer (4) lazy commit and (5) SML short-circuit unless
   profiling specifically calls for them.
7. Always keep (6) source-parse fallback available.

## Acceptance criteria

qwf / qsvc / qjob `--help` median startup <= current source-parse
baseline (1.80 / 1.86 / 1.35 s).  Stretch goal: <= 1.0s for all
three.

## Tracking

New task: "AOT load-time optimization".  Split into sub-tasks
matching proposals (1)-(5) once (1) profiling data is in.
