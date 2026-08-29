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
predecessors, which is what `--scc-preload-set` reports.

The partial parse is nevertheless **opt-in** (`QORE_QCC_SUBSET_PARSE=1`); what
the default waits on is below. The coordinator falls back to the group's own
parse when a partial parse fails, so enabling it cannot break a build — but the
fallback costs the failed parse on top of the group's, so a partial parse that
fails routinely is worse than not trying.

## Parsing part of a group is not the same as parsing all of it

The scheduler's open problems come from one place: a parse that resolves the rest
of its group from preloaded `.qo` shells does not get what the group's own parse
would have given it, and the rules written for a parse of ONE source do not all
carry over to a parse of several.

Three defects sat between the partial parse and a Qorus build; two are fixed and
the third is the one the default still waits on.

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

**A subset must be convex, and the preload set must be complete.** *(Partly fixed;
the convexity half is what the default still waits on.)* Two separate requirements hide
here, and both come from the same fact: **a `.qo` carries the declarations it was compiled
against**.

*Completeness (fixed).* The preload set was the members reachable over **required edges**
only, and a required edge is recorded only when a compile folds a provider's value or
bakes its link-time hash. A source that uses another's declaration purely as a **type**
records no such edge — only the provider's source-content digest — so those providers were
left out of the parse altogether. The type then resolved to a deferred placeholder, and an
initializer that has to *construct* it at parse commit got nothing:

```
RUNTIME-TYPE-ERROR: <return statement> expects type 'object<::OmqMap>', but got
  no value instead (while initializing constant 'TypeMap')
```

`Classes/GroupRuntimeContext.qc` declares `static OmqMap host_map();` and names
`Classes/OmqMap.qc` in its depfile *only* as `OmqMap_qc.sha256` — no compile contract — so
`OmqMap.qc` was neither compiled nor preloaded. `--scc-preload-set` now closes over
prerequisites **and** content dependencies together, iterating to a fixpoint: a shell added
for either reason brings its own unmet requirements, and preloading a class whose base is
absent fails the parse just as surely. These are not ordering edges, so the decomposition
is untouched — no components merge and nothing extra goes stale.

*Convexity (open).* Completing the preload set is necessary but not sufficient. The
compiled set must also be **convex**: no preloaded source may depend on a source the parse
compiles. On Qorus, `Classes/AbstractCompilableMetadata.qc` is preloaded while its own base
`Classes/AbstractMetadata.qc` is compiled — so the shell brings back the *previous*
`AbstractMetadata` and the parse's copy is shadowed:

```
INVALID-MEMBER: 'type' is not a registered member of class 'ReleaseScriptMetadata'
   AbstractMetadata::constructor() (Classes/AbstractMetadata.qc:106)
```

Enforcing convexity means promoting any such shell into the compiled set and iterating.
**That closure was measured on Qorus, and it decides the question.** Taking the provider
relation as required edges plus content dependencies — content is the right relation here,
because a shell carries the declarations it was compiled against — the convex closure is:

|!edited source|!must be compiled|!plus preloaded|!of 820
|`Classes/QorusRestApiHandler.qc`|386|290|82% involved
|`Classes/QorusMapManager.qc`|386|290|82%
|`lib/misc.ql`|386|290|82%
|`Classes/ServiceApi.qc`|386|290|82%
|`Classes/QorusRestClass.qc`|386|290|82%

The closure is **the same for every seed**: Qorus's combined provider graph is dense enough
that convexity collapses to one fixed point regardless of what was edited. A partial parse
would compile 386 components and load 290 shells where the group's own parse compiles all
820 in about four minutes with full build-tool parallelism — and it would still pay the
mode-transition cascade. (The figure is an upper bound: it assumes a shell carries
declarations for everything its compile recorded as a content dependency.)

**So the partial parse should not be finished for this codebase.** It can only pay where a
group's dependency graph is sparse enough for convex subsets to stay small; Qorus's is not.
`QORE_QCC_SUBSET_PARSE` stays off, and the effort belongs in the two levers that do not
depend on partitioning a parse: parallelising the standalone walk, and removing the
mode-transition cascade by making the two compile modes publish the same contract.

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

**The cascade is not yet gone.** A comment-only edit to `Classes/QorusRestApiHandler.qc` from
a group-parse state still rebuilds 13 objects in 4m49, because contracts are compared whole
and 12 symbols still differ in `body_contract_hash`. But the ordering above is now
discharged: body-contract granularity is what remains, and it is now both sufficient and
worth its machinery.

**It also loses bodies.** A whole-group parse lowers cross-member calls
it can see in its own parse, so an object it emits is not byte-identical to one
emitted against preloaded shells: it records more cross-object fast-entry
providers. Switching a component between the two modes therefore changes its
compile contract and rebuilds its consumers once — which is why the first
incremental build after a group parse walks the whole closure of whatever was
edited, however small the edit, and why the coordinator prefers to stay in one
mode for a whole build.

The sharper form of the same fact is that **parse commit runs user code**. A
constant initializer that calls across the subset boundary needs the provider's
body, and a shell does not carry one. With the two defects above fixed, a
419-source subset of Qorus reaches exactly that:

```
RUNTIME-TYPE-ERROR: <return statement> expects type 'object<::OmqMap>', but got
  no value instead (while initializing constant 'TypeMap')
   GroupRuntimeContext::hostOmqMap() (Classes/MetadataActionContext.qc:379-615)
```

`hostOmqMap()`'s body is in `QorusMapManager.qc`, which that subset preloads
rather than compiles. No preload-set or ordering rule fixes this: either the
shells carry enough to execute the initializer, or a subset must be widened to
include every source whose body a member's constant initialization reaches. Until
one of those exists, `QORE_QCC_SUBSET_PARSE` stays off by default.

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

## Tests

- `examples/test/ir/AOTSccGeneration.qtest` — decomposition and generation identity,
  including that a generation follows a predecessor's declaration contract and not its
  compile contract
- `examples/test/ir/AOTSccGraphTransition.qtest` — key vs index durability, graph
  generation naming, freeze semantics, the 75/76 split, that a declaration-contract
  dependency reaches the preload closure without becoming an ordering edge, and that a
  component whose own compile dropped a prerequisite is current rather than stale (with
  the three cases that must still be stale: a prerequisite the record never named, a
  surviving prerequisite whose contract moved, and an edited member source)
- `examples/test/ir/AOTIncrementalDeps.qtest` — what a compile records as a dependency,
  including that a folded constant narrows to the provider's declaration contract, that a
  comment-only provider edit leaves it byte-identical, and that a changed value does not
- `examples/test/ir/AOTSccIncrementalDriver.qtest` — the driver and coordinator
  against a compiler that rewrites another member's depfile mid-compile; also the
  coordinator's pass loop: a cascade walked to convergence, a pass that changes
  nothing escalating instead of lapping, and the escalation floor following
  whether a partial parse is configured
- `examples/test/ir/AOTSymbolIndex.qtest` — the symbol index and the source-symbol
  manifest, including a subset parse resolving a provider it compiles itself
- `examples/test/ir/AOTQoLock.qtest` — the lock helper: status passthrough (including the
  75/76 the build acts on), the observed contention flag, that two holders do not overlap,
  that a SIGKILLed holder's lock is free with nothing to reclaim, that the lock file
  survives, and that arguments are not re-split by a shell
- `examples/test/ir/AOTDepfileAtomicWrite.qtest` — depfile publication atomicity
- `examples/test/ir/CMakeBuildHelpers.qtest` — the CMake surface end to end
