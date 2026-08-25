# The Incremental qcc Object-Group Scheduler

## Status

Implemented. This document records the contract the scheduler holds itself to,
because the failures it prevents are all failures of *identity* — two processes
disagreeing about what "this component", "this generation" or "this graph"
refers to — and those are not visible from any one file.

Relevant code:

- `tools/qore-qo-source-order` — the graph, the decomposition, and every identity
- `tools/qore-qo-incremental-plan` — the group coordinator
- `tools/qore-qo-incremental` — the builder for one component
- `tools/qore-qo-batch-bootstrap` — the whole-group shared parse
- `cmake/QoreMacros.cmake` — `QORE_QCC_COMPILE_OBJECTS`, which wires the above
- `qcc-main.cpp`, `lib/QoreAOT.cpp` — depfile emission

Related: [AOT Object Files and Module Artifacts](aot-object-files-and-module-artifacts.md).

## Why a component, not a file

A `.qc` source can require a declaration from another source that requires one
back. A required edge that closes a cycle cannot be expressed as a build edge,
so the members of a cycle have no valid relative order: they must be compiled
and published as one generation, under one lock.

The unit of work is therefore the strongly connected component of the
required-edge graph, not the file. `qore-qo-source-order --scc-*` computes that
decomposition; everything below is about naming its results well enough that two
processes can agree on them.

## The graph has two halves, and only one of them is stable

| Half | Derived from | Changes when |
|---|---|---|
| Summary edges | Static analysis of source content | A source is edited |
| Contract edges | The `.compile-contract.stamp` entries in each member's `.qo.d` | **The build itself compiles something** |

The second half is the whole problem. `qcc` records the compile contract of every
provider a source was actually compiled against, and the scheduler promotes those
depfile entries to *required* edges. A compile therefore rewrites part of the
graph that decides other components' membership, numbering, predecessors, preload
closures, lock paths and publication targets.

Read live, that graph is not the same from one helper invocation to the next.

## Three identities, deliberately kept apart

Conflating any two of these produces a diagnostic that blames the wrong agent,
which is worse than no diagnostic at all.

| Identity | Covers | Answers |
|---|---|---|
| Source token | The member sources' content digests | "Did something outside the build edit my sources while qcc was reading them?" |
| Generation token | Member sources **plus** the published compile contract of every direct predecessor member | "Is this component's published artifact set still valid?" |
| Graph generation | The depfile-derived contract edge set | "Is this still the dependency graph I planned against?" |

Consequences worth stating explicitly:

- The source token must **not** include the component key or membership. The key
  is a digest of the same member output paths the token already names, so it adds
  no identity — but it would make an internal membership change indistinguishable
  from an external source edit, which is the one distinction the token exists to
  draw.
- The generation token uses predecessors' *contract* digests, not their source
  digests. That is what makes a comment-only edit to a provider a no-op for every
  consumer, while a declaration change propagates.
- Currency deliberately does **not** compare the graph generation. A depfile edge
  that changed elsewhere in the group is not a reason to recompile this
  component. The graph generation is recorded in each generation record so a
  finished build can be checked for having published one coherent graph, and it
  is compared only at publication, against the graph the compile was planned
  against.

## A component index is not a name

The decomposition is numbered by lowest member node. One required edge appearing
or disappearing splits or merges a component and renumbers every component above
it. An index resolved in one process and used in the next can therefore name a
*different* component — which is not a hypothetical:

- passed to `--scc-compile-plan`, it hands a source another component's preload,
  and the compile fails with "reference to undefined class";
- passed to `--scc-publish`, it commits the compare-and-swap against the wrong
  generation record.

**A component key is the only durable name.** It is a digest of the member output
set, so it either names the same members or does not resolve at all. Every place
identity crosses a process boundary passes the key. Indexes remain valid inside
one process, and inside one frozen graph (see below).

A key that no longer resolves is a graph transition, not a bad argument, and is
reported as such.

## The graph is frozen for the duration of a build step

`qore-qo-source-order --scc-freeze-graph SNAPSHOT [--batch-stamp STAMP] CONTEXT`
records the current contract-edge set as one named generation. While
`QORE_QCC_GRAPH_SNAPSHOT` names a snapshot addressed to the current context,
every command applies its edges instead of scanning the live depfiles.

The rule for who freezes and who reads live is one sentence:

> The snapshot pins the graph for operations that touch **part** of a group.
> Whole-group operations under the group lock read the live graph, because
> nothing can be concurrently modifying it.

- The coordinator freezes at the top of each phase and again after the last one,
  so a pass is planned, executed and verified against one decomposition, and a
  newly discovered required edge is crossed at a defined point rather than
  mid-pass.
- The shared-parse batch runs with **no** snapshot: it holds the group lock,
  rewrites every depfile in the group, and must publish against the graph its own
  compiles produce. The coordinator re-freezes immediately afterwards.
- A snapshot that cannot be applied — missing, unreadable, addressed to a
  different output list, or naming a node outside the context — is discarded for
  a live scan. A partly-applicable snapshot would mis-attribute a dependency.
- **A snapshot does not outlive the whole-group publication it was frozen from.**
  The file stays in the script directory between builds, and a build tool is free
  to run an object recipe before the next build's first freeze: target-level
  ordering onto the coordinator reaches the group's order targets, but CMake
  duplicates each object rule into every independent target that depends on the
  object stamps, and such a target carries no planner barrier. The snapshot
  therefore dates the whole-group publication it was read from (`format` 3), and
  is discarded when that date has moved — a shared parse rewrites every depfile in
  the group, so the frozen edge set describes a graph it superseded. Without
  this, the recipe resolves its compile plan from the previous build's edge set,
  the coordinator's freeze makes the publication a graph transition (76), and the
  component is compiled a second time.
- **The publication is dated by `<batch stamp>.publication`, not by the batch
  stamp.** The stamp is also the build tool's ordering token, so runs of
  `qore-qo-batch-bootstrap` that publish nothing touch it: CMake's Makefile
  generator duplicates the bootstrap recipe into every independent target that
  depends on the stamp, and in a parallel build the copy that loses the race to
  the group lock finds the tree the winner published current and touches the stamp
  so no member stays older than its dependencies. That touch can land after the
  coordinator's last freeze. Dating the publication by the stamp therefore left
  the snapshot naming a publication the group no longer reported, and every later
  build discarded an edge set that was still exact — silently giving up the
  determinism the snapshot exists for. The sidecar is advanced only after
  `--scc-publish-all`, and adoption paths only create it when it is missing.

The snapshot is close to performance-neutral: it replaces reading and scanning
every member depfile (12.3 ms on a synthetic 600-source group) with one JSON read
(0.5 ms), but a helper invocation is dominated by process startup and the
source-order cache load, so end to end it is worth about 3%. Its justification is
determinism.

## Publication is a compare-and-swap with two distinguishable failures

```
qore-qo-source-order --scc-publish --expect SOURCE_TOKEN \
    --expect-graph GRAPH_GENERATION CONTEXT COMPONENT_KEY
```

| Exit | Meaning | Response |
|---|---|---|
| 0 | Published | — |
| 75 | A member source moved away from `--expect` | An agent outside the build edited it. Rebuild the component once; a second occurrence is reported as external interference |
| 76 | The graph moved away from `--expect-graph`, or the key no longer resolves | The build's own doing. Re-resolve identity from the current graph and rebuild once; never blame anything outside the build |
| 1 | Anything else, including an incomplete artifact set | Real failure |

Source movement is checked before the graph, so a build whose sources really were
edited is never told to look at its own scheduler instead.

Neither 75 nor 76 is retried more than once. A second occurrence means the input
is changing faster than the build can read it, and repeating the compile would
only make correctness depend on winning a race.

## One group, one scheduler

The coordinator (`qore-qo-incremental-plan`) is the group's scheduler; the
per-object recipes behind it are verifiers of the same predicate.

That only holds if they ask the same question. They previously did not: the
coordinator left build inputs from outside the group to the build tool while each
recipe weighed them by mtime, so the coordinator could find a group current while
every recipe behind it found its own component stale — and then compile
concurrently, which is the fan-out the coordinator exists to replace.

- The coordinator's plan target takes every build input an object recipe treats
  as one, so the build tool reaches the coordinator whenever any of them moves.
- A recipe the coordinator scheduled (`QORE_QCC_PREREQUISITES_ORDERED=1`) asks
  for currency with `--source-deps-only`, the coordinator's rule.
- A recipe never starts the shared parse. The batch takes the group lock while a
  recipe holds only a component lock, so a batch started from one object's recipe
  would republish every component's generation record underneath compiles it does
  not exclude. Reaching that point behind the coordinator means the plan was
  wrong, and is reported.

## How much of the group one pass parses

A shared parse used to mean the whole group, so the coordinator's only choices
were "compile stale components one at a time" and "reparse every source". With
`qcc -c --output-dir=DIR -L <object dir>` a parse can cover **part** of a group:
the sources on the command line are parsed together and every other member is
preloaded from its `.qo` as a declaration shell — the same mechanism a standalone
compile has always used, applied to a set instead of a single file. A partial
parse therefore publishes exactly what a standalone compile of the same source
publishes: the same object and the same compile contract.

The coordinator picks between three answers, in order of how much it parses:

|!Condition|!Answer
|rebuild closure ≥ `max(8, 50% of components)`|whole-group parse
|stale set spans a multi-source component, or ≥ 2 components|one parse over the stale set
|otherwise|standalone compiles, one component at a time

The **closure** decides only the first question — compiling a component rewrites
its compile contract, so its consumers go stale as a result, and a closure that
covers half the group will be parsed either way. The **stale set** decides the
second: two stale components already pay for a shared parse, because N standalone
compiles preload the group N times while one parse preloads it once. The
thresholds are `QORE_QCC_INCREMENTAL_GROUP_BATCH_PERCENT` (50),
`QORE_QCC_INCREMENTAL_GROUP_BATCH_MINIMUM` (8) and
`QORE_QCC_INCREMENTAL_BATCH_THRESHOLD` (2); `QORE_QCC_SUBSET_PARSE=0` declines
partial parses, and a group configured by an older `QoreMacros.cmake` has no
`qcc-subset.sh`, in which case the scheduler behaves exactly as it did before
partial parses existed.

A whole-group parse still lowers cross-member calls it can see in its own parse,
so an object it emits is not byte-identical to one emitted against preloaded
shells; switching a component between the two therefore changes its compile
contract and rebuilds its consumers once. That difference is a property of the
two compile modes, not of the partial parse — a standalone compile has always
published the preload-based contract — but it is why the coordinator prefers to
stay in one mode for a whole build rather than mixing them.

## The bootstrap recipe hands the stale set to the coordinator

`qore-qo-batch-bootstrap` runs before the coordinator and used to make its own
whole-group decision: any staleness at all selected the whole-group parse, so a
changed module timestamp or a handful of stale components reparsed every source
before the coordinator was consulted.

It now runs the group-wide currency pass — the one that compares build inputs
from OUTSIDE the group by mtime, which the coordinator's source-content scan
cannot see — and reports the result in the vocabulary the whole build already
speaks: it advances the ordering token, then touches the success stamp of every
member it found current. A member whose stamp predates the token is what currency
already means by "belongs to a previous bootstrap", so the coordinator, the object
recipes and the build tool all read the same stale set. Only when the tree has no
published generation at all — a first build, or a wiped object directory — does
this recipe still parse the group itself.

A parse that fails publishes nothing, so the ordering token it advanced is put
back on the way out. Leaving it advanced marked every member of the group as
belonging to a previous bootstrap, which turned one syntax error into a
whole-group rebuild on the next build.

## One order target per group, not one per source

The scheduler used to hand the build tool the condensation DAG: one custom target
per source, plus one target-level dependency per condensation edge, so object
recipes could be ordered against each other. That is one target and a handful of
edges per source — 868 targets and some 16,000 edges for one real group, 1,554
targets and 47,000 prerequisite lines for a project with seven of them. The cost
is paid on every build invocation, before any recipe runs: 12 seconds to decide
to build a single object, 46 seconds for a no-op build, and a 7.6 MB `Makefile2`
that every one of those recursive sub-makes re-reads.

The coordinator makes that ordering redundant. It plans the whole group once,
compiles every stale component in dependency order under the group lock, and only
then are the object recipes reached; each recipe is a verifier of the same
predicate rather than a builder that has to be sequenced. So the group publishes
**one** order target (`qore_qcc_<group>_objects`), which depends on every object
stamp and on the coordinator. `ORDER_TARGETS_VAR` still returns one entry per
source — the same target repeated — so a caller that indexes it by source is
unaffected.

On a 200-source group this takes the build tool from 206 targets to 7, a 600 KB
`Makefile2` to 21 KB, and a no-op build from 31 s to 0.7 s.

## Depfiles are shared inputs

A depfile is not private to the process that writes it: the scheduler reads every
member's depfile to recover the contract-edge half of the graph, while other
members of the same group are compiling. Writing one in place truncates it first,
and a reader inside that window sees an empty or partial dependency list — which
is indistinguishable from a source that genuinely lost a dependency, and so
silently splits or merges components underneath another builder.

Every depfile writer therefore assembles its content and publishes it with a
single `rename(2)`. The temporary is named after its target, so concurrent batch
threads cannot collide, and it does not end in `.d`, so a scan for depfiles cannot
pick one up mid-write.

## Reclaiming a lock asks for a state, not for an act

A component lock is a hard link whose owner record names a pid and a generation
token, and a waiter that finds it abandoned reclaims it: it re-reads the record,
confirms it still names the owner it inspected, and removes it. Nothing makes
that decision exclusive — every waiter behind the same abandoned lock reaches it
independently — so two of them removing the same path is normal operation, not an
error, and the second one has still got what it asked for.

`rm -f` does not say that. GNU coreutils absorbs an `unlink(2)` that returns
`ENOENT`, but busybox consults `-f` only when the preceding `lstat(2)` failed:
when the path was there a moment ago and is gone by the time it is unlinked, the
removal is reported and the command exits non-zero. Under `set -e` that ended the
build, and the failure surfaced as a component that was compiled but never
published, with nothing in the log but a `rm` diagnostic — only ever on musl
hosts, because the same code is silent under coreutils.

Every reclaim path therefore removes through `remove_raced_path`, which succeeds
when the path is gone, whoever removed it, and still reports a removal that
failed for any other reason.

## Invariants

1. Identity crossing a process boundary is a component **key**, never an index.
2. A build step schedules, compiles, publishes and verifies against **one** graph
   generation; the coordinator is the only thing that crosses to the next one.
3. Only whole-group operations holding the group lock read the live graph.
4. Source identity, generation identity and graph identity are computed from
   disjoint inputs and are never substituted for one another.
5. A depfile becomes visible atomically or not at all.
6. One group has one scheduler; everything behind it verifies with the
   scheduler's own rule.
7. No convergence loops. Every unpublishable outcome is rebuilt at most once,
   and repetition is a reported failure rather than a strategy.
8. Removing a lock succeeds when the lock is gone, whoever removed it.

## Tests

- `examples/test/ir/AOTSccGeneration.qtest` — decomposition and generation identity
- `examples/test/ir/AOTSccGraphTransition.qtest` — key vs index durability, graph
  generation naming, freeze semantics, and the 75/76 split
- `examples/test/ir/AOTSccIncrementalDriver.qtest` — the driver and coordinator
  against a compiler that rewrites another member's depfile mid-compile
- `examples/test/ir/AOTDepfileAtomicWrite.qtest` — depfile publication atomicity
- `examples/test/ir/CMakeBuildHelpers.qtest` — the CMake surface end to end
