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
  The file stays in the script directory between builds, and a standalone helper
  invocation can precede the next build's first freeze. Generated object rules
  depend on the coordinator target, including copies attached to independent
  object-stamp consumers. The snapshot
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
|rebuild closure ≥ the escalation floor (below)|whole-group parse
|stale set spans a multi-source component, or ≥ 2 components|one parse over the stale set
|otherwise|standalone compiles, one component at a time

The **closure** decides only the first question — compiling a component rewrites
its compile contract, so its consumers go stale as a result, and a closure that
large will be parsed either way. The **stale set** decides the second: two stale
components already pay for a shared parse, because N standalone compiles preload
the group N times while one parse preloads it once.

The escalation floor is `max(QORE_QCC_INCREMENTAL_GROUP_BATCH_MINIMUM,
QORE_QCC_INCREMENTAL_GROUP_BATCH_PERCENT% of the group's components)` — 8 and a
percentage that defaults to what serves the band below it (next paragraph);
`QORE_QCC_INCREMENTAL_BATCH_THRESHOLD` (2) is the second boundary.
`QORE_QCC_SUBSET_PARSE=0` declines partial parses, and a group configured by an
older `QoreMacros.cmake` has no `qcc-subset.sh`, in which case the scheduler
behaves exactly as it did before partial parses existed.

**The percentage defaults to what serves the band below it**, because that is
what the closure is being compared against. With a partial parse the band below
is one parse over the stale set, and the group's own parse is worth reaching for
only when the closure really is most of the group: 50%. Without one the band
below is a walk of standalone compiles, one component at a time, each preloading
the whole group — while the group's own parse compiles every member in one parse
and at the build tool's parallelism.

What decides that crossover is not one build but the next one. The two modes
publish different compile contracts for the same source, so the first incremental
build after a group parse walks the whole closure of whatever was edited whatever
the size of the edit — and **a build that escalates leaves the tree in the mode
that makes the next edit walk it again**. A walk pays that transition once and
every edit after it is one compile. Measured on Qorus, editing a source whose
closure is 13 components:

|!Build|!Walk|!Escalate to the group's parse
|first edit after a group parse|5m03, 23 `qcc` invocations|4m54, 876
|the edit after that|1m38, 1|4m54, 876

So the floor belongs well above the closures a developer edits through, and
should catch only the ones a walk can never amortise: at 408 components a walk is
an hour of serial compiles against a four-minute parse. The default of 5% puts
that boundary at 41 components on that tree. Left at 50% the floor was 410, and
thirteen widely required sources sat at closures of 408 and 409 — exactly the
ones the walk cannot amortise. Setting the variable pins either value.

**What a partial parse preloads is the transitive predecessors of what it
compiles, staged into a directory of its own — never the group's object
directory.** A `.qo` carries every declaration it was compiled against, including
declarations made in other sources, so preloading a *consumer* of a source the
parse compiles brings that source's own declarations back and the parse can no
longer make them:

```
PARSE-EXCEPTION: enum 'X' conflicts with existing namespace 'X' in namespace '::'
```

The per-file path has always staged its preloads this way (`--scc-preload` into a
`.qcc-preload.*` directory); a parse of several sources needs the union of their
predecessors, which is what `--scc-preload-set` reports. Both answer from one
closure (`componentPreloadOutputs()`), so what a source is preloaded with does not
depend on which path compiled it.

The partial parse is nevertheless **opt-in** (`QORE_QCC_SUBSET_PARSE=1`), for
its cost rather than its correctness (measured below). The coordinator falls back to the group's own
parse when a partial parse fails, so enabling it cannot break a build — but the
fallback costs the failed parse on top of the group's, so a partial parse that
fails routinely is worse than not trying.

## Parsing part of a group is not the same as parsing all of it

The scheduler's open problems come from one place: a parse that resolves the rest
of its group from preloaded `.qo` shells does not get what the group's own parse
would have given it, and the rules written for a parse of ONE source do not all
carry over to a parse of several.

Three defects sat between the partial parse and a Qorus build; all three are fixed.

**It makes the parse defer to sources it is compiling itself.** *(Fixed.)* The
source-symbol manifest names which source of the build group provides each
symbol, and a parse that resolves against `-L` shells *defers* every symbol the
manifest attributes to a source other than the consumer's own: the emitted object
records an import rather than binding a same-name declaration from a loaded
module or stub. That rule is right for a parse of **one** source, where "not the
consumer" and "not in this parse" are the same thing. A parse over part of a
group compiles several sources at once, and there they are not — a provider being
compiled in the same parse was deferred to a placeholder anyway, and a value
folded through a placeholder loses its declared type. On Qorus,
`Classes/ConstantMetadata.qc` builds a constant from
`QorusMapManager::CodeBaseMetadata`, a `hash<string, hash<MetaFieldInfo>>`; with
`QorusMapManager` deferred it folded to `hash<string, hash<auto>>`, while the
source that declares `MetaFieldInfo` — the same `QorusMapManager.qc`, which is
not "another source" to itself — got the real type for its own signature:

```
RUNTIME-OVERLOAD-ERROR: no variant matching
  'QorusMapManager::getUiCompatFields(hash<string, hash<auto>>)' can be found
```

The deferral test now asks whether the provider is any source in the current
parse, not only whether it is the consumer: the parse arms the set of sources it
is compiling alongside the manifest, exactly as it already arms the set it
preloaded. A parse of one source has a one-element set, so that path is
unchanged, and the whole-group parse never arms the manifest at all.

**A parse directive belonged to its batch rather than to its source.**
*(Fixed.)* A batch parses many sources into one program, and parse options were
program state for the rest of the batch. `%exec-class` sets
`PO_NO_TOP_LEVEL_STATEMENTS`, so the first script in a batch made every
declaration source parsed *after* it reject a top-level statement it accepts on
its own — a stray `;` after a hashdecl, in the case that surfaced it. The
whole-group parse escaped only by the order its context lists: with Qorus's one
`.qr` at position 874 of 875, nothing followed it. A parse of part of a group
orders by the plan instead. Parse options are now saved and restored around each
batch source; module loads, parse defines and module parse commands are batch
state by construction and are left alone.

**A subset must be convex, and the preload set must be complete.** *(Fixed.)* Two
separate requirements hide
here, and both come from the same fact: **a `.qo` carries the declarations it was compiled
against**.

*Completeness (fixed).* The preload set was the members reachable over **required edges**
only, and a required edge is recorded only when a compile imports a symbol from a provider
or bakes its link-time hash. A source that uses another's declaration purely as a **type**
records no such edge — only the provider's source-content digest — so those providers were
left out of the parse altogether. The type then resolved to a deferred placeholder, and an
initializer that has to *construct* it at parse commit got nothing:

```
RUNTIME-TYPE-ERROR: <return statement> expects type 'object<::OmqMap>', but got
  no value instead (while initializing constant 'TypeMap')
```

`Classes/GroupRuntimeContext.qc` declares `static OmqMap host_map();` and names
`Classes/OmqMap.qc` in its depfile *only* as `OmqMap_qc.sha256` — no compile contract — so
`OmqMap.qc` was neither compiled nor preloaded. The preload set now closes over
prerequisites **and** content dependencies together, iterating to a fixpoint: a shell added
for either reason brings its own unmet requirements, and preloading a class whose base is
absent fails the parse just as surely. These are not ordering edges, so the decomposition
is untouched — no components merge and nothing extra goes stale.

A **folded constant** is the second reference that records no required edge, and it fails
harder than a type does. The aliasing object serializes its constant as a reference naming
the *owner's* constant rather than as a copy, so a preload without the owner cannot
deserialize it at all:

```
error: sibling .qo cross-resolution failed: AOT cannot deserialize value for class
  constant 'QorusTypeInfoRestClass::QorusDataTypeInfo': cannot resolve const_ref
  'QorusDataTypeCatalogue::Info' in the current program
```

This is why the closure is `componentPreloadOutputs()`, one helper shared by every caller —
`--scc-preload` and `--scc-compile-plan` for the standalone path, `--scc-preload-set` for
the subset parse. Widening only the subset parse left the **default** path, one component
at a time, with the defect: on Qorus a documentation-only edit to
`Classes/QorusRestApiHandlerV9.qc` left one stale component whose preloaded
`Classes/QorusRestApiHandlerV8.qc` aliases a constant owned by
`Classes/QorusDataTypeCatalogue.qc`, and the rebuild stopped on the message above.

*Upgrading an existing object tree.* The attribution fix changes what a compile RECORDS,
not what it emits, so it is deliberately not part of the `qcc` format fingerprint -- the
files it touches are ordinary parser sources that change constantly, and fingerprinting them
would invalidate every AOT cache on every unrelated edit. The consequence is that installing
the fixed compiler does not repair a tree built by the old one: a provider that is not itself
edited never has its depfile rewritten, so the missing content dependency persists and the
`const_ref` failure recurs on the next incremental build. One whole-group parse fixes it,
because that is the only operation that rewrites every depfile in the group -- either
`QORE_QCC_INCREMENTAL_GROUP_BATCH_PERCENT=0` for one build, or `rm -rf <OUTPUT_DIR>`, which
is a complete invalidation and needs no `cmake` re-run. After that, ordinary incremental
builds are correct.

*Attribution (fixed).* Completing the closure is no use if the dependency it follows was
never recorded, and for a folded constant reached by a **scoped** reference it was not. A
folded value leaves no trace in the emitted object, so it is recorded when the reference is
*resolved*; a batch attributes that record to the consuming source, taken from the referring
expression's location. A batch resolves constant initializers at parse commit, after every
source in it has been parsed, so the source "currently being parsed" is gone by then and the
location is the only consumer available — and the scoped resolvers (`Owner::CONST`) did not
pass it down to the constant lookup. An unscoped `CONST` was unaffected, because its
resolver always has. The result was not a weaker edge but no edge at all: a whole-group
parse recorded nothing from an aliasing source to the source it aliased, so the fold was
invisible both to the preload closure and to currency. The lookups now carry the consumer
location, and a batch records what the standalone compile of the same source records.

*A preloaded object needs its modules, and the summary is all inheritance has (fixed —
[#5463](https://github.com/qoretechnologies/qore/issues/5463)).* Two more gaps made the
**default** path fail terminally on Qorus, with a message that named neither:

```
error: sibling .qo cross-resolution failed: cannot resolve base class
  '::OMQ::AbstractQorusTokenAdmissionStore' for class 'QorusTokenAdmissionStore'
qore-qo-incremental-plan: cannot build stale component scc-5872098917c07d7a
```

- **Module dependencies were loaded outside the parse lock, and their failures discarded.**
  A preloaded class can inherit a class a *module* declares, and when no source the compile
  parses `%requires` that module, only the preload loads it: each `.qo` records the modules its
  compile had loaded. But a `--stub` parse (which every Qorus compile has) had already left the
  program mid-parse, and the preload loaded the modules *before* taking the parse lock, so an AOT
  module's namespace init — which enters the importing program — was refused ("the Program
  accessed is currently undergoing parsing"). The refusal was dropped, the module stayed
  unmerged, and resolution failed later on a class nothing had changed. On the Qorus tree 14
  modules failed this way in one compile. The loads now run under the parse lock, exactly as a
  `%requires` in the parse does, and a module that still cannot be loaded is named in the error
  if resolution then fails. Which components went stale only decided whose preload closure
  reached such an object, which is why an unrelated edit that repartitioned the graph surfaced it.
- **A declaration of another kind hid a class from its subclass.** Inheritance records nothing
  in a depfile (it is resolved by name at load), so the source-symbol summary is the only thing
  that connects a subclass to its base — and it resolved `inherits Map` among *every* declaration
  named `Map`. A class constant `QorusDataTypeCatalogue::Map` made the name ambiguous, so
  `class OmqMap inherits Map` had no edge to `Classes/Map.qc`, and every preload closure that
  reached OmqMap left its base out ("cannot resolve base class '::OMQ::Map' for class 'OmqMap'").
  A mention that can only name one kind of declaration now resolves among the sources declaring
  that kind: `inherits`, `new` and static calls among classes, `hash<X>` among hashdecls,
  `enum<X>` among enums, call references among functions, a declared type among classes and
  hashdecls. Two sources declaring the same kind and name remain unresolved. On Qorus this adds
  exactly the one missing edge and leaves the decomposition unchanged (862 components).

A compile that fails loading or resolving its preloaded objects now exits with **78**, distinct
from a failure in the sources it compiles. The coordinator answers 78 on the default path the way
it answers a failed partial parse: with the group's own parse, which preloads nothing and so
cannot fail that way. A typo in a compiled source still fails fast, without paying for a group
parse first.

*A preloaded object is run, not only declared (fixed).* Parse commit runs initializers, and the
initializers of what a compile preloads are among them: on Qorus, `Classes/QorusMapManager.qc`
declares `static TestMetadata tests = new TestMetadata(...)`, whose constructor calls
`TestEngine::listStepKinds()`. Code reached that way in a preloaded object is the object's
source-stripped IR, and registering it needs every class and function it calls declared in the
Program -- a need the two dependency relations above cannot express, because a late-bound call
bakes nothing of its target and so is, correctly, no dependency at all. What made the gap
intermittent is that the two compile modes disagree about it: a whole-group parse resolves the
call against the declaration it has just parsed and records the callee as a content dependency,
while a standalone compile defers it through the source-symbol manifest and records it only as a
late-bound import in the object's symbol index. So the first incremental build after a group
parse worked, and once the caller had been compiled standalone every later preload of it lost the
callee:

```
AOT-SOURCE-IR-ERROR: could not materialize source-stripped function IR for 'listStepKinds': AOT
  slot map registration failed for 'TestEngine::_static_listStepKinds(*hash<auto>,*string)':
  unsupported AOT slot metadata: expr slot 48 (STATIC_METHOD_CALL): cannot resolve the call to
  'TestStepPhasePolicy::validatePhase()': class 'TestStepPhasePolicy' is not declared in this Program
error: parse commit failed: Classes/QonsoleDesignArtifactStore.qc
```

Reading the symbol indexes was not an option (1.2 GB of `.idx.json` on Qorus), so `qcc` writes the
providers beside the object in `<object>.load-requires`, one `require` row per build-group source
its code needs declared -- the provider a resolved call records, or, for a reference it deferred
(a class it constructs or names as a type records only its path), the source the group's
source-symbol manifest names. `componentPreloadOutputs()` closes over these as a third relation.
They are not ordering edges and not staleness inputs: nothing merges and nothing extra goes stale.

Three related defects were fixed with it:

- the same failure used to exit 1 and so failed the build; it is a property of the preload set, so
  a parse commit that fails with `AOT-SOURCE-IR-ERROR` while resolving against preloaded objects now
  exits 78 and the coordinator answers it with the group's parse; and
- a *construction* of the missing class failed silently: it raises `AOT-PENDING-CLASS`, which
  parse commit treats as "linked later" and defers, and a read of the variable then deferred it
  again and returned a value that was never assigned. A static variable initializer is now deferred
  only at parse commit; a read reports what is still pending, a failed initializer leaves the
  variable uninitialized, and each initializer is classified from its own exceptions.
- the declaration form of a construction, `X x(args)`, disagreed with the other two about a class
  the source-symbol manifest deferred: a scoped `new X()` and a static call report it as pending
  linking, while `X x(args)` raised a hard `CREATE-OBJECT-ERROR`. On Qorus that failed
  `Classes/TestEngine.qc`'s own standalone compile (the preloaded `QorusMapManager` initializer runs
  `listStepKinds()`, which declares `TestStepPhasePolicy policy(rv)`) whenever no earlier record
  named the class. During an AOT source parse it now raises `AOT-PENDING-CLASS` as well, and the
  initializer is deferred like any other that reaches a symbol linked later.

*Convexity (fixed — [#5458](https://github.com/qoretechnologies/qore/issues/5458)).*
Completing the preload set is necessary but not sufficient. The compiled set must also be
**convex**: no preloaded source may depend on a source the parse compiles. On Qorus,
`Classes/AbstractCompilableMetadata.qc` is preloaded while its own base
`Classes/AbstractMetadata.qc` is compiled — so the shell brings back the *previous*
`AbstractMetadata` and the parse's copy is shadowed:

```
INVALID-MEMBER: 'type' is not a registered member of class 'ReleaseScriptMetadata'
   AbstractMetadata::constructor() (Classes/AbstractMetadata.qc:106)
```

Enforcing convexity by *growing* the compiled set — promoting any such shell into it and
iterating — was measured on Qorus and rejected. Taking the provider relation as required
edges plus content dependencies (content is the right relation here, because a shell
carries the declarations it was compiled against), the convex closure is:

|!edited source|!must be compiled|!plus preloaded|!of 820
|`Classes/QorusRestApiHandler.qc`|386|290|82% involved
|`Classes/QorusMapManager.qc`|386|290|82%
|`lib/misc.ql`|386|290|82%
|`Classes/ServiceApi.qc`|386|290|82%
|`Classes/QorusRestClass.qc`|386|290|82%

The closure is **the same for every seed**: Qorus's combined provider graph is dense enough
that growing to convexity collapses to one fixed point regardless of what was edited.

**The fix goes the other way: it shrinks the set.** A stale component whose preload closure
leaves the parse and comes back — Y preloads X, and X was compiled against Z, which the
parse recompiles — is *left out* of this parse. It stays stale, and the next pass compiles
it, after X has been recompiled against the new Z if Z's declarations moved. That costs
nothing the build would not do anyway: the coordinator already walks the consumer closure
one pass at a time. `qore-qo-source-order --scc-convex-set` decides it
(`convexComponentSubset()`, over the relation `componentPreloadOutputs()` closes over), and
`qore-qo-batch-bootstrap` compiles only what it keeps, names what it left out, and tells the
coordinator so the escalation bound counts only what was compiled.

Two details decide whether the answer is ever empty, and it is not:

- Members are taken in dependency order, providers first, and each is kept only if it
  neither reaches nor is reached from an already-kept member through something preloaded.
  Content dependencies are not ordering edges, so the relation can have cycles between
  components — on Qorus, `AbstractMetadata` and the 35-member component holding
  `ReleaseScriptMetadata` each reach the other through preloaded objects. Leaving out
  every offending member at once left nothing to compile; the first member is always kept.
- A path from a member back to *itself* through what it preloads is not a reason to leave
  it out. That is a standalone compile of it, which is what the default path does.

A member whose providers reach one left out is left out as well, so nothing is compiled
against a stale member's previous object only to go stale again in the next pass.

`QORE_QCC_SUBSET_PARSE` remains opt-in. Convexity was the last defect keeping a partial
parse from compiling consistently; whether it should be the default is a performance
decision for the escalation floor below, and wants measuring on the trees it would serve.

**Measured on Qorus (2026-09-22), it does not pay for the edit it was meant for.** A comment
appended to three core sources (`QorusQonsoleCore.qc`, `QorusRestApiHandlerV9.qc`,
`QonsoleDesignArtifactStore.qc`) leaves three singleton components stale:

|!path|!passes|!sources per parse|!coordinator time
|standalone compiles (default)|1|1, three times|3m07
|`QORE_QCC_SUBSET_PARSE=1`|3|1 (convexity left 2, then 1, out)|3m47

`--scc-convex-set` kept only one of the three in each parse -- each of the others would have
preloaded an object compiled against a source the parse recompiled -- so the partial parse
became three passes of one source, each paying the parse and planning overhead the one
standalone pass pays once. On a group this dense the convexity rule turns most multi-source
stale sets into one source per pass, and a parse of one source is a standalone compile with
more overhead. Making it the default would need a stale set whose members do not reach each
other through preloads, which the edits measured here did not produce.

*Standalone compiles follow what they preload (fixed).* The default path has the same
hazard between passes' members that convexity handles inside one parse. A pass compiles its
stale components one at a time, and each compile preloads the `.qo` of what it resolves
against; a provider that is stale in the same pass is preloaded as its *previous* object
unless it was compiled first. The stale listing was compiled in topological order, which
follows **required** edges only, and a provider reached solely over a content dependency or a
load requirement is preloaded without being ordered. On Qorus, one edit changed
`QonsoleReferenceScope::runAuxiliary(string, code<auto()>)` to take a third argument and made
`Classes/QorusQonsoleCore.qc` pass one. The consumer reached the provider only over a
declaration contract (the group's own parse records no compile contract for it), and it came
first:

```
PARSE-TYPE-ERROR: no variant matching 'QonsoleReferenceScope::runAuxiliary(string, code<auto()>, string)'
  can be found; the following variants were tested:
   QonsoleReferenceScope::runAuxiliary(string purpose, code<auto()> task)
```

A cross-source static call is normally deferred to link time, which hides this; the call named
the class without its namespace (the manifest has `OMQ::QonsoleReferenceScope`), and that form
resolves against the preloaded class so the stale shell's variants decided it.

The coordinator now compiles a pass's rows in the order `qore-qo-source-order --scc-order-set`
gives (`preloadCompileOrder()`): a component is taken only once no other pending component is
reachable through what it preloads — the relation `componentPreloadOutputs()` closes over,
walked through current components in between. That relation includes every required
predecessor, so the result is still a topological order. Content dependencies can form cycles;
a cycle among pending components has no order that serves all of them and is broken at the
first pending component in topological order, whose required predecessors are all taken by
then. The escalation that follows a failed compile is unchanged.

## What actually causes the mode-transition cascade

The cascade — the first incremental build after any full build walking the whole closure of
whatever was edited — was attributed to the two modes emitting "different cross-object
fast-entry providers". Compiling one real source both ways and diffing the compile
contracts locates it exactly. `Classes/QorusRestApiHandlerV2.qc`, group parse against shell
parse: 436 differing lines, 208 of them `defined` entries. Taking one symbol present in
both:

```
GROUP:  sig=c2012fb3dfaef684  decl=985a5a1d427a63ca  value=(empty)  body=fnv1a64:4986fd2c00554eed
SHELL:  sig=c2012fb3dfaef684  decl=985a5a1d427a63ca  value=(empty)  body=(empty)
```

**Signature, declaration and value hashes are identical. Only the body-contract hash
differs — present in one mode, absent in the other.** (A handful of entries also differ in
type-name qualification, `hash<OMQ::SlaInfo>` against `hash<SlaInfo>`, and ten `native`
fast-entry symbols differ; those are the minority.)

`hasBodyContract()` is `approach_b_eligible || hasImportableBodySummary() ||
hasBodyEffectContract()` — all results of the interprocedural summary pass over *the batch
being lowered*. A whole-group parse lowers 875 sources and derives richer summaries than a
parse of one, so more functions qualify. The difference is therefore real, not cosmetic:
it is the optimisation opportunity the compiler could prove.

**The problem is where it is recorded, not that it differs.** A body contract is an
optimisation opportunity, but it sits in the compile contract — the thing that decides
whether consumers are stale. So a source that gains or loses one invalidates every consumer,
and switching a component between modes rebuilds its whole closure for a difference that
changes no declaration.

It cannot simply be dropped from the contract: the link step validates a consumer's recorded
import against the provider's body-contract hash (`qo-link hash mismatch`), so a consumer
that fast-called a provider must be rebuilt when that provider's contract changes. The
obvious answer is granularity — a consumer records the body contract only of providers it
actually fast-called, so invalidation could follow those recorded imports rather than the
provider's whole contract.

**That was specified, its precondition checked, and measured to be insufficient.**
Projecting one real contract pair to the mode-stable part (`defined` rows without the body
field, `native` rows dropped) still leaves 126 of 668 rows differing. Of the 101 symbols
that differ:

|!differing fields|!count|!nature
|`body_contract_hash` only|41|optimisation artifact — what granularity would fix
|`value_hash` only|39|real content difference
|`declaration_hash` only|12|real content difference
|`signature_hash` + `declaration_hash` (+ body)|9|real content difference

Contracts are compared whole, so fixing only the 41 changes nothing — the other 60 still
invalidate every consumer.

**The 60 are the useful result.** A body contract *should* differ between modes: it hashes
what the analysis proved, and a parse of 875 sources proves more than a parse of one. A
signature, declaration or folded constant value **should not** — the same source text ought
to yield the same declarations however much else was in the parse. Those 60 are shell-mode
type erasure surfacing in the contract, the same class the deferral fix above addressed only
for providers inside the parse.

**The order of work is therefore fixed by measurement**: make declarations mode-independent
first, because they are bugs; only then is body-contract granularity both sufficient and
worth its machinery. The reverse order buys nothing.

### What making declarations mode-independent turned out to need

One of the two causes was a rendering bug and is fixed: `aotDeferredTypePath()` rooted a
deferred class (`object<::X>`) where a resolved one is unrooted (`object<X>`), while
deferred hashdecls were already unrooted — so the asymmetry hit classes only. On
`QorusRestApiHandlerV2.qc` that took symbol paths present in only one mode from 6/5 to 2/2
and symbols differing in `signature_hash` from 9 to 1.

The other cause is not a rendering bug, and 52 of the original 60 remain. **They are all
constants**, and the mechanism is that a parse resolving against shells infers weaker types
for a constant's initializer expression. Reduced to two sources:

```
// provider.qc, preloaded
public class CProvider { public { const Base = {"a": <CField>{"name": "a"}}; } }
// consumer.qc, compiled
public class CUser {
    public {
        const Derived = CProvider::Base + {"b": <CField>{"name": "b"}};
        const Forced  = CUser::Derived.b.name;      // declaration_hash differs by mode
    }
}
```

Writing `cast<string>(CUser::Derived.b.name)` makes both modes produce the identical hash,
which isolates it to inference rather than to the value or the rendering. `CProvider::Base`
carries `value_hash = "pending"` in the *whole-group* parse too, so neither mode has folded
it at emit time: what differs is that the group parse can evaluate the provider's retained
initializer expression during the consumer's parse and a shell-based one cannot.

So the remaining half is a compiler capability — a preloaded shell has to supply constant
initializers the consuming parse can evaluate with full type fidelity — not a
normalisation.

### Declarations are now mode-independent

That capability, and three defects it uncovered, are in. On `QorusRestApiHandlerV2.qc`,
whole-group parse against a parse with the rest preloaded:

|!symbols differing in|!before|!after
|`value_hash`|39|0
|`declaration_hash`|12|0
|`signature_hash` + `declaration_hash`|9 -> 1|0
|symbol paths present in only one mode|6 / 5 -> 2 / 2|0 / 0
|`body_contract_hash`|41|12
|`native` rows|10|0

Every remaining difference is `body_contract_hash`, which is what the section above says a
body contract legitimately is. Four things had to change.

**A shell now carries the value a pending constant's initializer produced** (AOT binary
format v15). A `.qo` holds declarations, not executable bodies, so a parse resolving a
constant against a preloaded shell cannot run the provider's `__const_init` function; a
whole-group parse evaluates the provider's retained initializer during the consumer's parse
instead. Evaluating a constant narrows its declared type to its value's type
(`ConstantEntry::parseCommitRuntimeInit()`), so `Forced` above came out `*string` in one
mode and `string` in the other. The writer records the value the producing parse computed
beside the pending flag; the reader attaches it to the `ConstantEntry`
(`ConstantEntry::setAOTParseShellValue()`) and `RuntimeConstantRefNode` reads it. It is
attached **only** where `.qo` shells are preloaded for a compile
(`QoreAOTBinaryDeserializer::preload_parse_constant_values`), so a runtime module load never
sees one and the init function stays the only source of a constant's value there. A value
that cannot be serialized without loss — an object, a closure — writes a presence byte of 0
and the consuming parse defers exactly as before.

Cost, measured over the 875 Qorus objects: **−0.01% total object size** (147 grew, 73 shrank,
655 unchanged; the largest single growth is 0.9% on `QorusMapManager.qc`).

**The value hash was representation-dependent.** `aotAppendValueHashParts()` gave a short
(NaN-boxed) string its own case, so the same string content hashed differently depending on
whether the bytes were stored inline or on the heap — and a source parse builds heap strings
where AOT deserialization rebuilds the short ones inline. A constant such as
`("name", "id", "version")` therefore published a different `value_hash` depending on how
the parse reached it. Short strings are `NT_STRING` like any other and now go through the
same case.

**`callref` and `closure` do not read back as themselves.** The language deliberately treats
both as interchangeable with `code` and resolves either name to `codeTypeInfo`, so a
serialized `hash<string, callref>` came back as `hash<string, code>`. A constant initialized
to a call reference takes its declared type from its evaluated value, so the two modes
published different declarations for it. `getAOTSerializableTypePath()` now emits the name
that round-trips.

**A regex literal is not a comment.** `qore-qo-source-order` masks strings and comments
before counting braces to attribute each declaration to its namespace, and
`if (line =~ /^#/ || !line.size()) {` was masked from the `#` onwards — taking the block's
opening brace with it. Brace depth desynchronised for the rest of the file, and every
declaration after that point was recorded in the manifest without its namespace: 201 of the
210 symbols in Qorus's `lib/qorus.ql`, plus 28 across five other sources. A type deferred
against such an entry rendered as `hash<SlaInfo>` where the same type resolved live renders
as `hash<OMQ::SlaInfo>`. The mask pattern now matches a regex literal first, anchored on
`=~` / `!~` (which is what distinguishes the leading `/` from division).

**A `.qo` was admitting its metadata twice.** The blob is reachable both through its metadata
symbol and by scanning the file, and `add_aot_metadata_blob()` keys its duplicate check on
the byte count — while the symbol path passed the symbol's *size*, which is the storage the
emitter reserved, rounded up for alignment. Whenever that exceeded the length in the blob's
own header the same metadata was admitted twice and **every row of the object's compile
contract was emitted twice**, so an object's contract depended on the parity of its metadata
length rather than on its declarations. 697 of 875 Qorus contracts carried duplicated rows.
The symbol path now trims to the length the header records.

**The cascade of declarations is gone; what remained was body-contract granularity**
([#5459](https://github.com/qoretechnologies/qore/issues/5459)). With declarations
mode-independent, a comment-only edit to `Classes/QorusRestApiHandler.qc` from a group-parse
state still rebuilt 13 objects in 4m49, because contracts were compared whole and 12 symbols
still differed in `body_contract_hash`. The two channels below — each watching the
provider's *declaration* contract — took that to one compile; the body contracts that
`--link-qo` validates are watched separately, per row. See "Two channels decide staleness"
and "The bound this narrowing must not cross".

**It also loses bodies.** A whole-group parse lowers cross-member calls
it can see in its own parse, so an object it emits is not byte-identical to one
emitted against preloaded shells: it records more cross-object fast-entry
providers. Switching a component between the two modes therefore changes its
compile contract and rebuilds its consumers once — which is why the first
incremental build after a group parse walks the whole closure of whatever was
edited, however small the edit, and why the coordinator prefers to stay in one
mode for a whole build.

**Parse commit runs user code, including a preloaded object's**
([#5466](https://github.com/qoretechnologies/qore/issues/5466)). A preloaded `.qo` is not a
declaration shell: it carries its source-stripped IR, and a constant initializer in a source the
parse compiles calls into a preloaded provider exactly as it would into one the parse compiled.
What that needs is every class and function the provider's code reaches *declared* in the parse,
which is the preload closure's job, not a body the object lacks. The report that opened #5466,

```
RUNTIME-TYPE-ERROR: <return statement> expects type 'object<::OmqMap>', but got
  no value instead (while initializing constant 'TypeMap')
   GroupRuntimeContext::hostOmqMap() (Classes/MetadataActionContext.qc:379-615)
```

was the completeness defect above -- `OmqMap.qc` was neither compiled nor preloaded, so
`new OmqMap()` was deferred and its deferred value read as no value -- and not a missing body. On
the current Qorus tree a partial parse compiling `MetadataActionContext.qc` with
`QorusMapManager.qc`, `GroupRuntimeContext.qc` and `OmqMap.qc` all preloaded commits cleanly, and
`CMakeBuildHelpers.qtest` pins the same call chain, counting the preloaded body's runs.

What #5466 did find was a hole in what an object records about a **static class variable**. A
read the parse resolves is a `LoadStaticVar` that names the variable itself and takes no
expression slot, and every write is a `LValuePath` rooted at the variable, so only a read
*deferred* to link time reached the symbol index, and with it `<object>.load-requires`. A constant
initializer is where that decides the outcome. `TypeMap` folds `QorusMapManager::groups`, so:

- the whole-group parse resolves the read and records nothing about `QorusMapManager.qc`;
- the next partial parse therefore does not preload it, and defers the whole initializer to link
  time instead of running it -- recording the provider, because the read was deferred;
- the parse after that preloads it, runs the initializer, and records nothing again.

The object alternated between the two forms on every build. Slot extraction now also records the
static variables a function reaches by name (`AOTStaticVarRefId`, compile-time only), and the
symbol index imports each one another source declares as an optional `static_var` record without a
provider -- the same record whichever mode compiled the object, and the same one the deferred form
writes. `.load-requires` attributes it through the source-symbol manifest. It carries no provider
because an import's provider widens an existing dependency on that source to its compile contract,
which only the compile that resolved the variable could do.

`QORE_QCC_SUBSET_PARSE` stays opt-in for the reason measured above: on a group as dense as Qorus the
convexity rule reduces most multi-source stale sets to one source per pass.

## Two channels decide staleness, and both have to agree about granularity

Once declarations are mode-independent, the remaining question is what a dependency
*watches*. Two independent things decide whether a component is stale, they are computed from
different inputs, and narrowing one alone accomplishes nothing because the other still
invalidates the same set.

**The dependency sink records source files, and only two things put anything in it.**
Instrumenting the four call sites that reach `qore_aot_note_referenced_decl()` and running a
whole-group parse of an 875-source group:

|!feeder|!site|!edges recorded
|folded constant|`ConstantList.cpp`|2262
|folded constant|`ConstantEntry::get()`|1659
|body contract|`QoreIRToLLVM.cpp`|0
|fast entry|`QoreAOT.cpp`|0

371 distinct (consumer, provider) pairs, and nothing else contributes. So the dependency a
folded value leaves behind is the whole of it: the consumer folded a compile-time constant,
that leaves no trace in the emitted object, and nothing else would rebuild the consumer when
the value changes. The dependency is genuinely required — only its **granularity** is wrong.
Without narrowing it is the provider's source-content digest, so appending a comment to a
widely required source rebuilds every object that folded any constant from it.

**Channel 1 — the depfile edge, which is what the build tool reads.**
`--depfile-declaration-contract-stamps` rewrites those source dependencies to the provider's
`.qo.aggregate-contract.stamp`, after the pass that narrows *imported* providers to their
compile-contract stamp and before the one that would otherwise reduce them to source digests.
Two things are deliberately not narrowed: a source the object also defines into (its own bytes
are what must rebuild it), and a provider it imports with a recorded body-contract hash (the
link step validates that hash, and a body contract is not part of a declaration contract).

The stamp is written with `write_generated_file_if_changed()`, so an unchanged declaration
keeps its mtime and the edge does not fire. That is the whole mechanism: the file the build
tool stats moves only when a declaration moves.

(In a group with a coordinator the build tool no longer reads member depfiles at all -- see "The
build tool reads one depfile per group, not one per object" below. The scheduler still reads
them for the graph and the preload closure, and a single-source group still hands its one
recipe the depfile qcc writes, so the narrowing matters to both.)

Two invariants make this work, and both were violated by the obvious implementation:

- **The suffix must not be `.compile-contract.stamp`.** That is how `qore-qo-source-order`
  recognises a REQUIRED edge. Spelling a content dependency that way promotes every resolved
  declaration to an ordering constraint: on Qorus it collapsed 820 components into 435, one of
  them with 441 members, and the scheduler then has nothing small left to compile.
- **Every declaration contract must exist before any depfile is rewritten.** The batch emits
  its members in parallel and wrote declaration contracts per member, so a consumer narrowed or
  did not narrow according to whether its provider happened to be earlier in the batch — or, on
  a clean tree, according to nothing at all. The batch now writes them in the same pre-pass that
  already wrote the compile contracts, for the same reason.

**Channel 2 — the planner's generation token, which is what the coordinator reads.**
`memberContractDigest()` hashed each predecessor's `.compile-contract.stamp`. That is what
makes the *cascade*: a compile contract carries lowering artifacts as well as declarations, an
875-source parse derives richer interprocedural summaries than a 1-source parse, so recompiling
one source standalone against shells republishes a compile contract that differs with no
declaration having changed — and every consumer goes stale, and recompiling *those* moves
*their* contracts, one ring at a time. It now reads the declaration contract, and falls back to
the compile contract only where none has been published.

**Neither channel suffices alone, and that is not a coincidence.** Measured separately against
a baseline of 23 qcc invocations: channel 1 alone gives 22 — it converges and leaves the
component graph intact, but the token still invalidates the same closure. Channel 2 alone gives
879 and a plan that does not converge. They address different halves of the same decision and
have to be applied together.

**Applied together they are still not enough, and what remained was a third thing.** The pair
removes the cascade — the closure of a comment edit goes from 13 stale components to 0 — which
then exposes a defect the cascade had been hiding: compiling a component moves the dependency
graph its own record was published against, so the next pass finds it stale immediately after
building it. See "Compiling a component is not what makes it stale" below. With that fixed as
well, on Qorus (875 sources, 820 components, one comment appended to
`Classes/QorusRestApiHandler.qc`, whose closure is 13 components):

|!`make qorus-core`|!Before|!After
|first build after a whole-group parse|23 invocations, 5m05|**1 invocation, 1m46**
|the same edit built again|1 invocation, 1m37|**0 invocations, 11s**
|the whole-group parse itself|4m01–4m21|3m40

The first build after a full build now costs what the steady state costs, which is what the
whole line of work was for. The whole-group parse is not slower for the extra per-object work:
the declaration contract every member must publish before any depfile is rewritten is written
in the pre-pass that already wrote the compile contracts, and the narrowing itself reads the
symbol index the passes around it already read.

## Compiling a component is not what makes it stale

A generation token names a component's members **and its prerequisite components**, so it is
comparable only against a token computed from the same dependency graph. `componentSourceToken()`
exists for exactly this reason on the publication compare-and-swap. The currency check compared
full tokens and had no such guard.

A component's predecessors come from its own depfile, and a compile rewrites its own depfile.
So compiling a component is by itself enough to move the graph its record was published against
— and it routinely does: a whole-group parse resolves cross-member references it can see in its
own parse and records a compile-contract prerequisite for each, where the same source compiled
standalone against shells defers more of them and records fewer. On Qorus the two forms of one
member's depfile carry 45 and 28 contract edges. The first standalone compile after a group
parse therefore *drops* prerequisites, and the token it published under the pass's frozen graph
cannot match the token the next pass computes under the graph its own compile produced.

The coordinator then sees a pass leave stale exactly the set it compiled, correctly calls the
plan non-convergent by its own rule, and reparses the whole group: 879 invocations for a
one-line edit.

**This was invisible for as long as the cascade existed.** With the cascade, pass 2's stale set
was the twelve consumers rather than the component just compiled, so the signature differed, the
loop had different work to do, and it walked to convergence. Removing the cascade leaves the
component alone in the stale set, the signature repeats, and the guard fires. Two defects, and
fixing either one alone leaves the build no better off.

What is comparable across a graph transition is what does not come from the graph. The rule:

- the component's own members must match exactly — this is what still catches a source edit;
- every prerequisite the **current** graph names must appear in the record with the same
  contract digest;
- a prerequisite the record does not name at all is stale: the artifacts were compiled without
  knowing about it;
- a prerequisite the record names and the current graph no longer does is **not** a reason to
  rebuild — the component's own compile is what said it no longer depends on it.

The check only ever accepts records it previously rejected, so it invalidates nothing already
published: the settling build after applying it recompiled zero objects.

### A compile that gains a prerequisite publishes it

The mode switch moves edges in both directions. A standalone compile resolves against preloaded
objects and records a contract edge to each one it used, so it can also *add* an edge to a
current provider that the group's depfile never named. On Qorus, `QorusQonsoleCore.qc` compiled
standalone after a group parse gains `QonsoleCoreDesignSession.qc`, which was not rebuilt.

The rule above rightly rejects a prerequisite the record does not name. But the record was
computed from the pass's frozen graph, which predates the compile, so it could never name one
the compile discovered. The next freeze reads the edge from the component's own depfile, the
component is stale right after it was built, the pass leaves exactly what it compiled, and the
coordinator escalates. Every incremental Qorus build after a group parse ended that way: three
edited sources cost 950 objects and about 13 minutes.

So a publication under a frozen graph computes its record from that graph **with the
component's own depfile edges replaced by the live ones** (`publishedGraphView()`), and records
that graph's name as its `graph_generation`:

- only its own edges, because the edges into its members are the ones its compile wrote and
  the ones its prerequisites come from. Every other depfile is what the next freeze reads unless
  another compile rewrites it, and that compile's own publication accounts for it. Only a
  component's own compile writes its members' depfiles, under the component lock, so what
  publication reads is what the compile wrote. Reading only those depfiles also keeps a
  publication from paying for a whole-group scan;
- the planning check is unchanged: `--expect-graph` is still compared against the frozen graph,
  so a compile planned from another graph is still refused (76);
- when the live edges change the decomposition itself, because the compile closed a cycle
  through a consumer, the component the pass planned no longer exists. The record keeps the
  frozen graph's form, and the next pass finds the merged component unpublished and builds it
  with the parse a multi-member component needs.

Nothing is forgiven that was not true of the compile. The record names every prerequisite the
compile depended on, with the contract digests current when it was published. A provider
rebuilt after that still makes the consumer stale.

### The bound this narrowing must not cross, and the third contract that closes it

A consumer that baked a provider's body-contract hash must still be rebuilt when that hash
moves, or the aggregate link fails with `qo-link hash mismatch` naming an object the build had
no recorded reason to rebuild. A declaration contract does not carry body-contract hashes, by
design — that omission is exactly what makes it identical in both compile modes.

Channel 1 respects this directly: it excludes any provider the object imports with a
body-contract hash, and those keep the compile-contract edge the previous pass gave them.

Channel 2 cannot, because the token's prerequisites are the required-edge predecessors, which
is the whole imported-provider set. Measured on the Qorus group:

|!body contracts|!objects|!records
|objects that PUBLISH one|856 of 877|29576 provided symbols (plus 1273 native)
|objects that CONSUME one|280 of 877|1116 import records, naming 25 providers

The gap between 856 and 280 is why this cannot be fixed by putting the hashes back into the
declaration contract: that would make 856 of 877 objects mode-dependent again and restore the
cascade in full. It has to be watched per consumer.

So a provider publishes a **third** contract, `<object>.qo.body-contract.stamp`: the
body-contract hash of every symbol it publishes one for, and nothing else — not the whole row,
so a consumer that fast-called one function does not rebuild for an unrelated change to
another. A consumer records it as a dependency **only when it actually baked one**, and the
generation token carries those digests beside its prerequisites.

Three properties keep it cheap and correct:

- it is a **content** dependency, not an ordering one. Ordering already comes from the
  compile-contract edge recorded for the same provider, and spelling this one with that suffix
  would promote it to a required edge and collapse the decomposition.
- it is **covered but not a member artifact**. The generation token accounts for it by content,
  so an mtime comparison would make a consumer permanently stale behind a provider published
  later in the same flush; but a publication is not held to have produced one, so a build
  configured without `--depfile-declaration-contract-stamps`, and every generation published
  before this existed, is still complete. The manifest omits the list entirely when it is
  empty, so no token moves for a group that bakes nothing.
- a member that **stops** baking one is not stale for it. That is its own compile talking, the
  same asymmetry that applies to prerequisites across a graph transition — except that a
  body-contract edge is not a graph edge, so it has to be forgiven on an unchanged graph too.

**A consumer watches the rows it baked, not the provider's whole stamp.** The stamp covers every
symbol the provider publishes a body contract for — on Qorus a consumed provider publishes 154
rows on average and up to 1664 — while a consumer bakes a handful: 1222 import records across
292 consumers and 26 providers. Watching the whole stamp made a consumer stale whenever *any*
row moved, and rows move for exactly the reason body contracts differ between modes: a group
parse proves more about some bodies than a standalone one. So a provider recompiled in the other
mode rebuilt every consumer of its body contract, however little each took.

qcc now records the rows beside the object, in `<object>.body-contract-imports`: one `import`
row per (provider stamp, symbol), spelled with the stamp path the depfile uses and the symbol
path `--link-qo` matches (all 1222 records on Qorus match a provider row by exact path). The
generation token digests only the provider's current rows for those symbols (`rows:` digests
in the manifest's `body-contracts`); a symbol the provider no longer publishes counts as moved,
because the link can no longer match it. A member with no readable record — an object compiled
by an older qcc — keeps the whole-stamp digest, so upgrading does not rebuild every consumer of
a body contract at once.

The depfile edge still names the whole stamp. That is harmless: the stamp is covered by content,
never compared by mtime (invariant 15), so a recipe the build tool reaches for it only verifies
the component against the token.

## A pass walks one level of the closure, not the whole closure

A pass compiles the stale set it was planned from. Compiling a component rewrites
its compile contract, so the pass *leaves that component's consumers stale* — an
invalidation reaches the consumer closure one dependency level per pass, and the
plan is converged only when it has walked all of them. The coordinator therefore
loops until the group is current rather than for a fixed number of passes.

A fixed count was a cliff of its own. The pass that follows a **whole-group
parse** is the deep one: the group's parse and a standalone or subset compile
publish different compile contracts for the same source (it loses bodies, above),
so the first
incremental build after a full build crosses the whole closure of whatever was
edited, one level at a time, whatever the size of the edit. Capping the walk threw
a converging plan away and reparsed the group. Measured on Qorus — 875 sources,
820 components, one comment appended to `Classes/QorusRestApiHandler.qc`, whose
closure is 13 components:

|!Coordinator|!Passes|!`qcc` invocations|!Wall
|two-pass cap|2, then the whole-group parse|889|7m39
|walk to convergence|3|23|4m52
|walk to convergence, `QORE_QCC_SUBSET_PARSE=1`|3|15|2m24

The second incremental build over the same tree is one `qcc` invocation — 1m38
through `make`, of which 22 seconds is the compile: the closure is already in the
preload-based mode, so nothing cascades.

Two conditions end the walk early, and both say the same thing — the incremental
path is no longer the cheaper answer:

- a pass that leaves stale **exactly** the set it compiled has not moved, and no
  further pass will move it; and
- a walk whose compiles have added up to what the whole-group parse would have
  compiled anyway has spent the group parse's budget without its parallelism.

Both escalate to the group's own parse and say so. The second is also what bounds
the loop: every pass compiles at least one component, so the cumulative count
reaches the bound in a finite number of passes whatever the graph does.

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

Each object custom command also depends on the coordinator target. CMake copies
these commands into independent targets that consume object stamps directly, so
ordering only the exported group target is insufficient. The command dependency
makes every such consumer complete the coordinator before examining object
recipes. This prevents an incremental compile from changing the dependency graph
while the coordinator freezes or republishes it, including when a current tree
is adopted after its bootstrap stamp was removed. It adds no per-source targets.

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

## The build tool reads one depfile per group, not one per object

Every object recipe used to carry `DEPFILE <object>.qo.d`. In a group with a coordinator those
depfiles decided nothing: a recipe behind the coordinator asks the currency question with
`--source-deps-only`, so one that ran because a module or stub moved found its component current
and did nothing. What actually carried an external change was the bootstrap's own depfile --
written by the group parse -- whose recipe runs `--scc-stale` and hands the stale components to
the coordinator.

They were also expensive. The Makefile generators before CMake 4.0 consolidate a custom
command's depfile into the target's `compiler_depend.internal`/`compiler_depend.make` by
**appending** a rewritten depfile's paths to the entry already there, never replacing it
(`cmDependsCompiler::CheckDependencies()`; CMake 4.0 assigns instead). A member depfile is
rewritten on every compile, and CMake copies an object recipe into every target that consumes
its stamp, each with its own consolidated file. On Qorus (CMake 3.31.12):

|!measurement|!value
|`qore_qcc_QORUS_CORE_MAIN_generation` consolidated dependencies|840 MB + 923 MB, 14.7M lines
|lines for `QorusQonsoleCore.qc`'s stamp|29,496, for 326 distinct paths (each repeated up to 112 times)
|every target's `compiler_depend*` in the build tree|9.6 GB, 7 GB of it temporaries of interrupted rewrites
|CMake dependency scan after a three-object incremental compile|50 s

And they left a gap. An input a member started reading after the group's last shared parse --
a module it began to `%requires` -- is recorded only by that member's standalone compile, not by
the bootstrap's depfile, so an edit to it rebuilt nothing. On Qorus 40 modules and module
sources were in that state, among them `QorusTokenEntitlement.qmod`, which the same project
builds.

So a group with a coordinator now has one **external-input stamp**, `.qcc-external-inputs.stamp`
beside the generation records, whose custom command only touches it and whose depfile names
every build input from outside the group that any member's depfile records
(`qore-qo-source-order --scc-external-inputs`, the union of `externalDepfileInputs()`). The
bootstrap depends on the stamp, so a move in any of those inputs reaches the bootstrap's
currency check, which compares it against every member's stamp and hands exactly the members
that read it to the coordinator. The coordinator rewrites the depfile at the end of every
successful plan -- its compiles are what change the set -- and only when the set changed, so
CMake consolidates it rarely; on Qorus it names 176 paths. Object recipes carry no `DEPFILE`;
a single-source group, which has no coordinator, still gives its one recipe qcc's depfile.

Configuring also resets the consolidated dependencies of the group's own targets (source
content, source symbols, bootstrap, coordinator, objects, generation) and removes the
temporaries an interrupted rewrite left: CMake keeps a target's consolidated dependencies when
the depfiles that produced them go away, so a tree configured by an earlier `QoreMacros.cmake`
would otherwise keep reading gigabytes of dependencies that no longer exist. The next build
reads the remaining depfiles again, which takes a fraction of a second. Targets a project
defines itself that consume the object stamps keep what they had consolidated -- they stop
growing, but the file stays until it is deleted.

Two depfiles still accumulate under CMake before 4.0 and are left alone: the bootstrap's, which
the group parse rewrites (about 8 KB per parse), and the external-input stamp's, which changes
only when the set of external inputs does. Both are reset whenever the project is configured.

## A covered input has to be recognised however its path is spelled

The generation token accounts for every in-context source, digest sidecar and sibling artifact
by content, so `externalDepfileInputs()` removes those from the mtime comparison and leaves only
what has no content identity in the manifest — the toolchain, loaded modules, stubs, includes.

The covered set indexes each path three ways: as written, made absolute, and canonical. The
lookup tried only the first two, and `absoluteNormalized()` prepends the working directory and
nothing else — it does not collapse a `..`, and it does not resolve a symlink.

A group source that looks foreign does not merely lose an optimisation. It stops being accounted
for by content and is compared by mtime instead, against the rule that an input must be strictly
**older** than the stamp it feeds. A source written in the same filesystem tick as the artifacts
is then stale the instant it is published, and the component can never become current.

That is a cross-platform bug that only one platform shows. It surfaced the first time the ir
suite was run on Linux: on spinster the whole test fixture — context, source, depfile and success
stamp — landed on a single timestamp (`09:42:34.698857745`), where APFS had spread the same
writes far enough apart for the comparison to pass by luck. The lookup now also tries the
canonical form, memoised because a whole-group currency pass would otherwise resolve the same
1.9k distinct paths once per member.

qcc canonicalises everything it writes, so no real build reaches this today; a build tree under a
symlink is how one would. The test fixture reached it because it spelled a member source
`<qo dir>/../src/NAME.qc`, which qcc never emits — it now writes the canonical path, so the
fixture exercises the depfile the build actually writes.

## A lock the kernel owns, rather than one the build has to reclaim

Two things in a group build are serialised: the whole-group parse behind its bootstrap stamp,
and each component's compile behind its own lock. Both used to take the lock by creating a
hard link and, when that failed, sleeping a second and trying again.

The poll was the smaller half of the cost. A hard link outlives the process that made it, so a
waiter had to decide whether an existing lock was *abandoned*: read the owner pid out of a
record, ask whether that pid was alive, compare-and-remove if it was not, and recognise the
lock shapes older versions of the helpers had published. None of that could be made exclusive —
every waiter behind one abandoned lock reaches the same conclusion independently — so "two
builders removed the same lock" had to be defined as normal operation, which is what the
`remove_raced_path` section below was written for.

An advisory lock taken with `fcntl(2)` has none of that state. The kernel owns it and releases
it when the holder exits, however it exits: there is nothing to detect, nothing to parse,
nothing to reclaim, and a waiter is woken when the holder releases rather than when it next
looks. `flock(1)` would do the same on Linux, but macOS ships no such utility — which is what
the poll was working around — while `File::lockBlocking()` is available everywhere %Qore is.

A POSIX shell cannot hold an fcntl lock, so `qore-qo-lock LOCKFILE COMMAND...` holds it for as
long as `COMMAND` runs, and each helper runs its critical section by re-executing itself under
it. `qore-qo-batch-bootstrap` re-execs whole; `qore-qo-incremental` re-execs one locked attempt
and keeps its retry loop outside the lock, where it belongs — a 75 or a 76 is answered by
re-resolving the component's identity, which must happen on the next graph, not this one.

Two details are load-bearing:

- **The lock file is never removed.** An fcntl lock belongs to the open file description, not
  to the path, so a holder that unlinked it would let the next waiter create a fresh inode,
  take an uncontended lock on that, and run alongside it.
- **`QORE_QO_LOCK_WAITED` is observed, not timed.** The helper tries a non-blocking lock first
  and only then blocks, so it can tell the command whether it was contended. A follower that
  waited for the group lock re-checks what it came to do, because the holder it waited behind
  may have published exactly that; that check is what the old `lock_waited` flag drove, and it
  is preserved exactly.

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

Every such removal therefore goes through `remove_raced_path`, which succeeds
when the path is gone, whoever removed it, and still reports a removal that
failed for any other reason.

The lock paths that motivated it are gone with the reclaim itself — nothing unlinks a lock any
more. What is left is the ordering token, which a run that published nothing must put back, and
which two builders can still race to restore.

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
7. No convergence loops. An unpublishable outcome is rebuilt at most once, and
   repetition -- a pass that leaves stale exactly the set it compiled -- is a
   reported failure rather than a strategy. Walking a consumer closure one
   dependency level per pass is not repetition: each pass compiles a set the
   previous pass did not, and the walk is bounded by the work the group's own
   parse would have done.
8. Removing a lock succeeds when the lock is gone, whoever removed it.
9. A parse publishes what it compiled or nothing, and a run that publishes
   nothing leaves the tree exactly as it found it -- including the ordering
   token, whose advance would otherwise mark the whole group stale.
10. A graph transition under a partial parse is re-planned, not failed: a
    consumer is free to reach the object recipes without waiting for the
    coordinator, so a member can be compiled while the parse is running.
11. What a parse resolves live, and what a parse directive applies to, are
    properties of the parse's own target set. A symbol provided by a source in
    the same parse is resolved, not deferred; a directive applies to the source
    that wrote it, not to the sources parsed after it. Both rules read the same
    for a parse of one source, which is why both were written as if they were
    about the consumer and the batch.
12. A dependency watches the narrowest published artifact that still covers what the
    consumer took from the provider, and the two channels that decide staleness -- the
    depfile edge and the generation token -- watch the same one. A reference that consumed
    only declarations watches the declaration contract; one that baked a link-time body
    hash watches the compile contract. Narrowing one channel while the other still watches
    source bytes changes nothing, because either is enough to invalidate.
13. A generation token is only ever compared against a token computed from the same
    dependency graph. A component's predecessors come from its own depfile, so its own
    compile can move the graph its record was published against; across that transition
    only the graph-independent half of the token means anything, and a component is never
    stale because its own compile dropped a prerequisite or stopped baking a body contract.
14. A lock is held by a process, not published as a path. It is released by the kernel when
    its holder exits, so no builder inspects, reclaims or removes another's lock; the lock
    file itself is never unlinked, because the lock is a property of the open file
    description and not of the name.
15. What the token accounts for by content is never also compared by mtime, and a path is
    recognised as covered however it is spelled. The two rules are one rule: an in-group
    input that is compared by mtime must be strictly older than what it feeds, which a
    build that writes a source and its artifacts in the same filesystem tick can never
    satisfy.
16. A parse never preloads an object compiled against a source it compiles. A member whose
    preloads reach another member of the parse through something preloaded waits for the
    next pass; the set shrinks to satisfy this, it does not grow.
17. A source-summary mention resolves among the declarations of the kind it can name. A
    declaration of another kind with the same name does not make it ambiguous; two of the
    same kind do.
18. A preloaded object is loaded as a parse would load it -- its modules under the parse
    lock -- and a failure to load or resolve preloaded objects is reported as that (qcc
    exit 78), never as a failure of the sources compiled. The scheduler answers it with the
    group's own parse, which preloads nothing.
19. A consumer's dependency on a provider's body contract covers the rows it baked, and
    nothing else the provider publishes.
20. A publication under a frozen graph records the prerequisites its own compile wrote. The
    record is computed from the frozen graph with the component's own depfile edges as the
    compile left them, so a prerequisite the compile gained is in it, and the next freeze does
    not find the component stale for a dependency it was built with.
21. The build tool watches a group with a coordinator through one depfile: every input from
    outside the group that any member recorded, rewritten only when that set changes. No
    object recipe hands the build tool a depfile of its own.
22. A preload set is closed over what the code of each preloaded object needs declared when it
    runs at parse commit (`<object>.load-requires`), as well as over its dependencies. A
    late-bound call is never a dependency, whichever compile mode recorded the object.
23. What an object records about a symbol its code reaches does not depend on whether the
    compile had that symbol declared. A static class variable read or written by name is imported
    by path, without a provider, whether the parse resolved it or deferred it.

## Tests

- `examples/test/ir/AOTSccGeneration.qtest` — decomposition and generation identity,
  including that a generation follows a predecessor's declaration contract and not its
  compile contract, that a consumer watches only the body-contract rows it baked, that
  `--scc-convex-set` leaves out what a preloaded object stands between (and keeps the
  provider-first member of a cycle through preloads), and that a declaration of another kind
  does not hide a class from its subclass
- `examples/test/ir/AOTSccGraphTransition.qtest` — key vs index durability, graph
  generation naming, freeze semantics, the 75/76 split, that a declaration-contract
  dependency reaches the preload closure without becoming an ordering edge, and that a
  component whose own compile dropped a prerequisite is current rather than stale (with
  the three cases that must still be stale: a prerequisite the record never named, a
  surviving prerequisite whose contract moved, and an edited member source)
- `examples/test/ir/AOTIncrementalDeps.qtest` — what a compile records as a dependency,
  including that a folded constant narrows to the provider's declaration contract, that a
  comment-only provider edit leaves it byte-identical, and that a changed value does not;
  folded enum members in both compile modes; the body-contract rows a consumer records;
  that a preloaded object's module loads in a program a `--stub` parse left mid-parse; and
  that a preloaded object's code that cannot load at parse commit exits 78 while a failure
  in the compiled source's own initializer exits 1; and that a static class variable another
  source declares is imported the same way by a batch and a standalone compile, whether it is
  folded into a constant, read in a body or written
- `examples/test/ir/AOTSccIncrementalDriver.qtest` — the driver and coordinator
  against a compiler that rewrites another member's depfile mid-compile; also the
  coordinator's pass loop: a cascade walked to convergence, a pass that changes
  nothing escalating instead of lapping, a compile that gains an edge to a current
  provider converging in one pass (and one that closes a cycle keeping the frozen
  graph's record for the parse to replace), the external-input depfile the coordinator
  writes (one rule, escaped, rewritten only on change), the escalation floor following
  whether a partial parse is configured, a partial parse leaving a non-convex member for
  the next pass, a preload failure on the default path falling back to the group's
  parse, and a preload closing over load requirements transitively without them becoming
  ordering edges
- `examples/test/ir/AOTSymbolIndex.qtest` — the symbol index and the source-symbol
  manifest, including a subset parse resolving a provider it compiles itself
- `examples/test/ir/AOTQoLock.qtest` — the lock helper: status passthrough (including the
  75/76 the build acts on), the observed contention flag, that two holders do not overlap,
  that a SIGKILLed holder's lock is free with nothing to reclaim, that the lock file
  survives, and that arguments are not re-split by a shell
- `examples/test/ir/AOTDepfileAtomicWrite.qtest` — depfile publication atomicity
- `examples/test/ir/CMakeBuildHelpers.qtest` — the CMake surface end to end, including
  that no member depfile reaches a target consuming the object stamps, that a module a member
  started requiring after the group parse is watched and an edit to it rebuilds that member
  alone, and that configuring resets the consolidated dependencies an earlier configuration
  left, that code a preloaded sibling runs at parse commit finds what it calls -- a static
  method, a function and a construction -- after its caller was compiled standalone, and that a
  partial parse's constant initializers run a preloaded provider's body on every build, not every
  other one
- `examples/test/qore/misc/static-var-deferred-init.qtest` -- a static variable initializer is
  deferred only at parse commit: reads report what is still pending, a failure is reported by
  every read, and initializers are classified from their own exceptions
