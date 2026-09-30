# Data Provider Discovery and Publication Qualification

## Purpose

Provider registration is intentionally resilient: one optional integration may fail while healthy providers remain
usable in the live process. An authoritative provider index and a release artifact have a stronger contract. They
must never silently certify a partial catalog. This design separates those two meanings and keeps qualification out
of action-execution hot paths. A provider index records the failures of individual apps and actions instead of
failing (see [provider-index-failures.md](provider-index-failures.md)): it is published, but its report is never
`complete`, and the failures are in the index for consumers to report.

This design coordinates [issue #5448](https://github.com/qoretechnologies/qore/issues/5448) with the independent
presentation-completeness work in [issue #5439](https://github.com/qoretechnologies/qore/issues/5439). Registration
completeness and presentation completeness describe the same canonical metadata graph, but neither implies the
other; release qualification requires both.

## Discovery state machine

`DataProviderActionCatalog` owns a monotonically increasing catalog revision and generation-scoped discovery state:

1. `beginDiscovery()` records the current revision and optional exact app/action inventory. Producers can register a
   typed, lightweight inventory callback with `registerDiscoveryInventoryProvider()`; ProviderIndex collects those
   callbacks after loading its requested discovery sources and completing deferred app/action, output-type, and
   scheme initialization, and scopes their output for filtered builds. Those initialization paths can introduce
   another inventory callback or add identities to an existing producer; collecting before that warm-up makes a
   rebuild invalidate its own snapshot. Producers still declare lightweight identity inventories before their own
   expensive schema materialization. Collection runs once per build, before the authoritative snapshot checkpoint;
   it must not retry or recollect over a failed or concurrently changed snapshot. Callback code
   runs without the catalog lock, and a callback exception becomes a structured `source`/`inventory` failure instead
   of aborting the remaining sources. Callback registration and producer identity changes advance a separate
   inventory revision; sealing rejects a snapshot whose revision changed during or after collection.
   Inventory-provider registration participates in the normal implicit module transaction, so a module that fails
   later in initialization restores the previous callback instead of leaving a stale discovery source behind.
2. Registration proceeds normally. Deferred initializer errors remain available through the compatibility failure
   API and are also merged into the active generation at seal time. Pending app/action declarations registered while
   a session is open automatically become scoped expected identities, so an initializer that returns without either
   registering or throwing cannot disappear silently. Source, factory, scheme, and publication errors are reported
   explicitly with `reportDiscoveryFailure()`.
3. After all intentional loading and deferred initialization, `checkpointDiscoverySnapshot()` records the exact
   revision immediately before the authoritative consumer copies registry state. Factory registration participates
   in the same catalog mutation critical section, so factory contents and the revision have one linearization point.
   The connection-scheme registry maintains its own revision and holds its registration lock around the nested
   catalog-qualified publication. Every pending or registered state transition advances that revision, including
   consuming a pending initializer that registers no replacement. New pending initializers invalidate the fully-loaded
   cache state; full-cache loading drains callbacks until a locked check sees none pending, and public cache snapshots
   take the same lock. A mutation in either registry while the candidate is copied rejects the attempt; neither
   registry can change during the atomic write.
4. `sealDiscovery()` snapshots registered technical identities and rejects the generation if an applicable retained
   failure, explicit discovery failure, missing expected identity, or stale checked-point snapshot remains. With
   `record_app_failures`, the failures of individual apps and actions (`isAppDiscoveryFailure()`) are moved to the
   report's `unavailable` list instead, and only the others reject the generation; the report is then not
   `complete`.
5. A successful seal returns an authenticated, single-use `DataProviderQualifiedDiscovery` token tied to the exact
   catalog revision.
6. `publishQualifiedDiscovery()` validates the token and revision and invokes publication while holding the catalog
   registration lock. This closes the check/write race; ordinary action execution neither takes this path nor updates
   the revision.

Session and token constructors are public because Qore has class-private rather than module-private constructor
access. They do not confer authority: the catalog records the exact object it issued and rejects externally
constructed objects even if their visible generation or revision values match.

## Failure semantics

Every failure retains stable machine-readable fields: generation, kind, phase, technical identity, app/action when
applicable, original error and argument, and owner module context. Human-readable output is diagnostic only and must
never determine success.

Retained initializer failures survive repeated discovery. They are cleared only when the same technical identity is
registered successfully or is explicitly deregistered. Pending declarations follow the same lifecycle, so a silent
no-op initializer remains a missing expected identity in later generations until that identity succeeds or is
deregistered. An explicit application scope excludes unrelated app failures from a selected build; an expected-
inventory fragment alone never narrows a full build. A catalog or qualification-state mutation after sealing
invalidates the token.

`ProviderIndex` treats requested source-load failures and unavailable factories as structured qualification
failures that block publication. Missing module paths, unknown schemes, missing selected identities, and app or
action initialization failures are app failures: `ProviderIndex` seals with `record_app_failures` and records them
in the index. It assembles the candidate in memory and enters the existing atomic writer only through
`publishQualifiedDiscovery()`, leaving the prior index untouched on all pre-publication failures.

Environment-driven discovery uses `loadProvidersFromEnvironmentWithReport()` rather than interpreting standard
error output. Registration-module, registration-map, and provider-module exceptions retain their exact module and
exception data. A module enters the loaded set only after the applicable operation succeeds; therefore the same
failed source remains visible in every qualification generation and can also recover when its dependency is fixed.

## Producer and presentation boundaries

Producer adapters normalize external schemas once at a versioned boundary. They preserve the distinction between an
absent value, an empty structure, and null; reject unsupported versions; and emit lightweight exact app/action
inventory before expensive action materialization. A raw user hash that happens to contain a `value` key is not an
`AllowedValueInfo`; conversion occurs only in a structurally declared allowed-value position.

Presentation extraction recursively covers structured option fields, list elements, union alternatives, and nested
allowed values. Existing flat IDs remain unchanged. The first nested field is action/option scoped; every union
branch adds its technical type identity. Type identities combine class/name with a SHA-256 digest of the stable
producer schema path, when one is available, and immediate technical field/type shape. Labels are excluded, so
anonymous types are deterministic, equal-shaped objects in different schema locations remain distinct, and label
edits preserve IDs.
Deeper reusable fields use app/action/option/occurrence/type/field IDs and each type is visited
once per concrete object in each catalog domain, action, and option. Technical field and union paths prevent
equal-shaped anonymous types at unrelated occurrences from silently sharing translations: stable producer paths
disambiguate them when available, and distinct objects claiming the same complete identity must agree on source
text. Repeated paths to one object are bounded, while a path-local object-identity set stops real cycles. This bounds
extraction by distinct type metadata rather than the number of paths
through a shared schema DAG. Extraction reads `getPresentationInfo()` and `getDeclaredType()` only; it never executes
dynamic type, default-value, or example callbacks and therefore remains deterministic and connection-free.
Repository-wide root regeneration loads every owner once and validates the complete owner/domain map before changing
the tree. Each validated catalog is then written to a same-directory temporary file and atomically renamed, so an
interrupted write cannot replace valid JSON with a truncated catalog. Exact root/locale parity remains the final
release gate.

## Qualification matrix

Local and release checks cover:

- eager, deferred, nested-load, repeated, scoped, successful-retry, forged-handle, and stale-token registration;
- normal, AST, and AOT parsing/execution of external-boundary fixtures and serialization round trips;
- exact root/locale ID and source parity, including nested fields and allowed values;
- build-and-install into an empty prefix, followed by checks with source module paths excluded;
- full provider-index generation with an expected inventory, zero structured failures, and a `complete` report
  (no unavailable apps or actions);
- inventory callbacks added or changed by lazy initialization, including late missing identities and callback
  failures; mutation during collection still rejects repeated attempts without replacing the previous index;
- fresh-process loading for binary provider modules and their declared runtime dependencies.

The release report records immutable source revisions and dependency image digests. A dependent module is qualified
only against the exact promoted Qore test-base digest, not a mutable tag.

## Change checklist

Any change to provider registration, schema adapters, presentation extraction, catalog generation, index publication,
or provider packaging must answer all of the following:

- Does every skipped or caught failure reach structured qualification state?
- Are failed discovery sources excluded from success caches so that both retries and repeated failures remain visible?
- Is success tied to exact technical inventory rather than output or translated prose?
- Does every producer publish its identity inventory before schema materialization, through a callback that avoids
  expensive schema construction and reentrant catalog locking?
- Can a retry clear only its own failure, and can a concurrent later mutation invalidate publication?
- Does every registry represented in the artifact mutate under the catalog revision boundary, and is a snapshot
  checkpoint taken only after intentional loading but before candidate serialization? If a registry must keep an
  independent lock, is its revision checked and its lock held around the nested qualified publication?
- Is producer inventory revisioned, filtered to the requested app scope, and proven unchanged from collection through
  publication?
- Are external shapes normalized at one versioned boundary without ambiguous casts?
- Are all reachable user-visible fields and finite choices extracted and projected with stable IDs?
- Do clean-prefix normal/AST/AOT/fresh-process tests exercise only installed artifacts?
- Did the full `audit-changes` checklist pass for the final commit range?
