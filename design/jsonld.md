# JSON-LD in Qore

This records what is durable about the `JsonLd` module (`qlib/JsonLd/`) and not evident from the code:
the layering, the one %Qore-specific hazard that produced a whole family of bugs across it, and what
the W3C conformance suite does and does not cover.

Module documentation — API, options, usage — is on the `JsonLd.qm` main page.

## Layering

The module implements the JSON-LD 1.1 algorithms as separate classes, each in its own file, in
dependency order:

| File | Role |
|---|---|
| `JsonLdTypes.qc`, `JsonLdValue.qc`, `JsonLdUtils.qc` | type predicates and value helpers; everything depends on these |
| `JsonLdIri.qc` | IRI expansion and compaction; called by every algorithm |
| `JsonLdContext.qc`, `JsonLdContextCache.qc` | context processing and term definitions |
| `JsonLdExpansion.qc` | the Expansion algorithm |
| `JsonLdCompaction.qc` | the Compaction algorithm |
| `JsonLdFlattening.qc`, `JsonLdBlankNodeGenerator.qc` | the Flattening algorithm and its node map |
| `JsonLdFraming.qc` | the Framing algorithm |
| `JsonLdRdf.qc` | conversion to and from RDF |
| `JsonLdDocumentLoader.qc` | remote/file/caching document loaders |
| `JsonLdProcessor.qc` | the public entry points |
| `JsonLdError.qc` | the spec's error codes |

The dependency direction is strict downward through that table. A defect in the top three rows cascades
into every algorithm below, which is why they are worth fixing first and testing hardest.

## `hasKey()` versus `exists`: JSON null is present

This is the single hazard most likely to bite anyone working on the module.

JSON-LD distinguishes a key that is **absent** from a key whose value is **JSON null** — `{"@value":
null}` is a value object, and a context entry set to null *removes* a definition rather than leaving it
alone. In %Qore, `exists` is false for a null value, so `exists h."@value"` conflates the two cases.

**The rule:** use `hasKey()` to ask whether a key is present; use `exists` only to ask whether the value
is non-null.

Getting this wrong in a predicate as low-level as `isValueObject()` (`JsonLdUtils.qc`) produced wrong
results in expansion, compaction, flattening and RDF conversion simultaneously, from one line. Both
predicates now use `hasKey()` and carry a comment saying why. Any new keyword check anywhere in the
module should be written the same way — grep for `hasKey("@` to see the existing sites.

## The W3C conformance suite

`examples/test/qlib/JsonLd/JsonLdW3cConformance.qtest` runs the official W3C test manifests from
`examples/test/qlib/JsonLd/w3c-jsonld/` and asserts a **100% pass rate over the tests it runs** — not a
percentage target. A regression fails the build.

Four categories are skipped by the runner, deliberately and permanently:

- tests marked `specVersion: "json-ld-1.0"` — the module targets 1.1;
- `jld:HtmlTest` — extracting JSON-LD from HTML is out of scope;
- tests whose options require live HTTP behaviour (`httpLink`, `httpStatus`, `redirectTo`,
  `contentType`), which would make the suite network-dependent;
- manifest entries whose `@type` is none of the evaluation/syntax test types the runner understands.

The counts are printed per suite, so a skip category growing unexpectedly after a manifest update is
visible in the test output rather than silently reducing coverage.

The algorithm-specific unit tests (`JsonLdExpansion.qtest`, `JsonLdCompaction.qtest`, and so on) exist
alongside the conformance suite because a conformance failure names a W3C test ID, not a code path; the
unit tests are where a root cause gets pinned down and locked in.
