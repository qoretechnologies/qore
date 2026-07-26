> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# JSON-LD 100% Conformance — Implementation Tasks

158 remaining failures. Each task below is a discrete, testable code change.
Tasks are ordered by dependency and cascade impact.

---

## P8: Infrastructure (do first — unblocks testing)

- [ ] **T8.1** Add `compactArrays` option to W3C test runner
  - File: `test/JsonLdW3cConformance.qtest` `buildOptions()`
  - Add: `if (exists o.compactArrays) { opts.compactArrays = o.compactArrays; }`
  - Fixes: t0070, t0091, t0093 (compact), t0044 (flatten)

- [ ] **T8.2** Download missing toRdf context files for e126/e127/e128
  - Files: `test/w3c-jsonld/toRdf/e126-context.jsonld` etc.
  - Already downloaded for expand dir; copy to toRdf or check test loader path
  - Fixes: te126, te127, te128 (toRdf)

- [ ] **T8.3** Fix double-slash in W3cTestDocumentLoader path for relative context URLs
  - File: `test/JsonLdW3cConformance.qtest` `W3cTestDocumentLoader.loadDocument()`
  - Handle relative context URLs (e.g., `c031/c031-context.jsonld`) that resolve
    against a different base than the test directory
  - Fixes: tc031 (expand+toRdf)

## P1: Type-Scoped Context Propagation (highest cascade impact)

- [ ] **T1.1** Refactor previousContext restoration to not revert for property values
  - File: `JsonLdExpansion.qc` `expand()` step 10
  - Current: reverts previousContext for ALL nested hashes except @value and @type-with-scope
  - Fix: pass a flag or use the `active_property` to determine if we're expanding
    a direct property value of the typed node. If `active_property` is set and is NOT
    `@graph`/`@reverse`/`@included`, keep the type-scoped context for this expansion level.
  - Implementation: instead of reverting in `expand()`, revert in `expandObject()` BEFORE
    processing properties — this way the type-scoped context is available for the
    immediate object's key expansion and value expansion.
  - Fixes: tc012, tc013, tc015, tc018, tc019, tc020, tc021, tc024, tc037, tc038
    (each × expand+compact+toRdf = ~30 tests)

- [ ] **T1.2** Fix @type value sorting before scoped context application
  - File: `JsonLdExpansion.qc` `expandObject()` step 13
  - The type values are sorted, but the expanded forms need to be sorted after
    IRI expansion, not before
  - Fixes: tc018 (multiple types resolved against previous context)

- [ ] **T1.3** Fix type-scoped context application in compactNodeObject
  - File: `JsonLdCompaction.qc` `compactNodeObject()`
  - Apply type-scoped contexts for compaction in the same manner as expansion:
    look up the expanded @type values, find terms with scoped contexts, apply
  - Fixes: tc009, tc012, tc014-tc018, tc021, tc023, tc024 (compact)

## P7: Flatten/Expansion Fixes (many quick wins)

- [ ] **T7.1** Preserve empty @set arrays in expansion
  - File: `JsonLdExpansion.qc`
  - When a property has `@container: @set` and the expanded value is an empty list,
    preserve it as `[]` instead of dropping it
  - Currently empty arrays result in no property being added; need to explicitly
    keep `[]` for @set containers
  - Fixes: t0004, t0015, t0016 (flatten)

- [ ] **T7.2** Fix @nest value ordering — append after base values
  - File: `JsonLdExpansion.qc` @nest keyword handler
  - Currently @nest properties are processed in the keyword handler which fires
    during key iteration. Values from @nest should be appended AFTER values from
    the base object for the same expanded property.
  - Implementation: process @nest content AFTER all regular properties, and use
    `mergeExpandedValue` to append (not prepend)
  - Fixes: tn003, tn004, tn005, tn006, tn007 (expand)

- [ ] **T7.3** Fix boolean expansion with default @language
  - File: `JsonLdExpansion.qc` `expandValue()`
  - When default @language is set, `true`/`false` should still expand to
    `{"@value": true}` / `{"@value": false}` without @language (booleans
    are not strings, @language only applies to strings)
  - Verify the current code handles this correctly; the issue may be in how
    the node map processes boolean values
  - Fixes: t0018 (flatten), te018 (toRdf), te088 (toRdf)

- [ ] **T7.4** Fix multi-graph flattening (named graph with blank node @id)
  - File: `JsonLdFlattening.qc` `flatten()`
  - When a node has `@graph` and a blank node `@id`, the graph contents should
    appear as a separate entry in the flattened output
  - Fixes: t0021 (flatten)

- [ ] **T7.5** Fix reverse property flattening
  - File: `JsonLdFlattening.qc` `generateNodeMap()` step 5.7
  - After the @reverse node map fix, verify that the forward property link
    (`foaf:knows`) is correctly added to the referenced nodes
  - Fixes: t0039 (flatten)

- [ ] **T7.6** Fix list deduplication in flattening
  - File: `JsonLdFlattening.qc` `addNodeProperty()`
  - List objects should NOT be deduplicated — identical `@list` values that
    appear multiple times should be kept
  - The `==` comparison for list objects is wrong; each @list is a distinct value
  - Fixes: t0042 (flatten)

- [ ] **T7.7** Fix @type:@none value expansion
  - File: `JsonLdExpansion.qc` `expandValue()`
  - When a term has `@type: @none`, native values (boolean, int, float) should
    be expanded as `{"@value": value}` WITHOUT any type inference
  - The @none type means "no type coercion" — the value object has no @type
  - Fixes: ttn02 (toRdf)

## P2: Compact Term Selection

- [ ] **T2.1** Fix heterogeneous list term ranking
  - File: `JsonLdIri.qc` `compactIri()` step 4.7
  - When list items have mixed types/languages, `common_type` = `@none` and
    `common_language` = `@none`. The term with NO type/language restriction
    should be selected, not a term with a specific restriction.
  - Verify the `selectTerm` function correctly uses `@none` to find the
    unrestricted term
  - Fixes: t0015, t0018, t0024, tla01 (compact)

- [ ] **T2.2** Fix @direction compound keys in inverse context
  - File: `JsonLdIri.qc` `buildInverseContext()`
  - Terms with `@container: @language` and `@direction` but no explicit
    `@language` should match values with ANY @language + that @direction.
  - Store terms under @language map with direction-qualified keys
  - When looking up values, check both exact and direction-qualified keys
  - Fixes: tdi03, tdi04, tdi05, tdi06, tdi07 (compact)

- [ ] **T2.3** Fix @type:@set for @type keyword compaction
  - File: `JsonLdCompaction.qc` `compactNodeObject()` @type handler
  - Test t0106 expects scalar compaction despite @set container on @type alias.
  - The @type keyword is special — `@container: @set` on @type means expansion
    always produces arrays, but compaction should still compact single values
    to scalar
  - Fixes: t0106 (compact)

- [ ] **T2.4** Fix term selection for @list + @type:@id
  - File: `JsonLdIri.qc`, `JsonLdCompaction.qc`
  - When a term has `@container: @list` and `@type: @id`, the inverse context
    should match empty lists and list values correctly
  - Fixes: t0074 (compact)

## P3: Container Compaction Edge Cases

- [ ] **T3.1** Implement property-valued index compaction
  - File: `JsonLdCompaction.qc` @index container handler
  - When the term has `indexProperty` set, extract the index value FROM the
    expanded property (not from @index) and use it as the map key
  - Remove the index property from the compacted value
  - Fixes: tpi01, tpi02, tpi03, tpi04, t0112, t0113, t0114 (compact)

- [ ] **T3.2** Fix @graph container selection (don't use when node has @id)
  - File: `JsonLdCompaction.qc`
  - When a graph node has `@id`, it should NOT match a term with plain `@graph`
    container — it should use `[@graph, @id]` container or no container
  - Fixes: t0080, t0083 (compact)

- [ ] **T3.3** Fix @graph container with multiple objects → @included
  - File: `JsonLdCompaction.qc`
  - When @graph container compacts multiple graph contents, the result should
    use `@included` wrapping, not a bare array
  - Fixes: t0109, t0110 (compact)

- [ ] **T3.4** Fix @graph + @set container compaction
  - File: `JsonLdCompaction.qc`
  - Various @graph+@set combinations need proper unwrapping/wrapping
  - Fixes: t0090, t0092, t0094 (compact)

- [ ] **T3.5** Fix t0073 — allow @type alias with @type: @id
  - File: `JsonLdContext.qc` createTermDefinition @type handler
  - The current validation rejects `@type` alias with `@type: @id`, but the
    spec allows it (it means @type values should be treated as IRIs)
  - Remove or relax the check at line 669-672
  - Fixes: t0073 (compact)

## P4: IRI Resolution

- [ ] **T4.1** Handle opaque (non-hierarchical) URI schemes
  - File: `JsonLdUtils.qc` `resolveIri()`
  - `tag:`, `urn:`, `ex:` etc. don't have `//authority` — resolving relative
    references against them should NOT add `//`
  - Detect non-hierarchical schemes (no `//` after `scheme:`) and handle
    resolution differently
  - Fixes: t0130, t0131, t0132, tli11 (toRdf)

- [ ] **T4.2** Fix fragment+query IRI resolution
  - File: `JsonLdUtils.qc` `resolveIri()`
  - When base has `#fragment` and relative has `?query`, the fragment should
    be stripped before appending the query
  - Fixes: t0062, te062 (expand+toRdf)

- [ ] **T4.3** Fix relative IRI compaction for fragments and queries
  - File: `JsonLdIri.qc` `compactIri()` step 10
  - Produce `#fragment`, `?query`, `./relative` forms
  - Handle keyword-like relative IRIs (prefix with `./` if they start with `@`)
  - Fixes: t0045, t0066, t0095, t0111 (compact)

- [ ] **T4.4** Fix dot-segment handling in IRI resolution tests
  - File: `JsonLdUtils.qc` `resolveIri()`
  - The 42-line IRI resolution tests (t0122-t0125) have specific expectations
    for dot-segment behavior that differ from our implementation
  - Fixes: t0122, t0123, t0124, t0125 (toRdf)

- [ ] **T4.5** Fix relative @vocab with fragment
  - File: `JsonLdIri.qc`
  - When @vocab is a relative IRI with `#`, property expansion should use
    the resolved vocab correctly
  - Fixes: te111, te112 (toRdf)

## P5: RDF Edge Cases

- [ ] **T5.1** Implement compound-literal rdfDirection mode
  - File: `JsonLdRdf.qc` `valueToRdfTerm()`, `rdfLiteralToValue()`
  - When `rdfDirection` is `"compound-literal"`, generate:
    ```
    _:cl rdf:value "text" .
    _:cl rdf:language "en" .
    _:cl rdf:direction "ltr" .
    ```
  - Also implement reverse detection in `fromRdf`
  - Fixes: tdi11, tdi12 (toRdf + fromRdf = 4)

- [ ] **T5.2** Fix blank node serialization in N-Quads @type values
  - File: `JsonLdRdf.qc` `processNodeToRdf()`
  - @type values that are blank node identifiers should NOT be angle-bracketed
    in N-Quads output: `_:b1` not `<_:b1>`
  - Fixes: tm003, tm004 (toRdf)

- [ ] **T5.3** Fix blank node @vocab predicate serialization
  - File: `JsonLdRdf.qc` `processNodeToRdf()`
  - When @vocab is a blank node and `produceGeneralizedRdf` is true,
    predicates should use raw blank node form
  - Fixes: te075 (toRdf)

- [ ] **T5.4** Fix _:suffix in N-Quads (not a compact IRI)
  - File: `JsonLdRdf.qc`
  - `_:suffix` as a property value should produce raw blank node,
    not angle-bracketed IRI
  - Fixes: te068 (toRdf)

- [ ] **T5.5** Fix fromRdf list detection edge cases
  - File: `JsonLdRdf.qc` `reconstructLists()`
  - t0016: nodes with rdf:List type should still be detected as lists
  - t0017: duplicate triples should be removed
  - t0020-t0022: lists shared across graphs should NOT be converted
  - Fixes: t0016, t0017, t0020, t0021, t0022 (fromRdf)

- [ ] **T5.6** Fix fromRdf nested list detection
  - File: `JsonLdRdf.qc` `reconstructLists()`
  - Inner lists (blank nodes that appear as rdf:first values and have
    their own rdf:first/rdf:rest) should be converted to nested @list
  - Fixes: tli01, tli02, tli03 (fromRdf)

- [ ] **T5.7** Fix fromRdf native type conversion edge cases
  - File: `JsonLdRdf.qc` `rdfLiteralToValue()`
  - `"1"^^xsd:boolean` should convert to `true` (not keep as string)
  - Overflow doubles should stay as typed literals
  - Fixes: t0027 (fromRdf)

- [ ] **T5.8** Fix JSON literal float precision
  - File: `JsonLdRdf.qc` `canonicalizeJson()` or `make_json()`
  - Float precision: `333333333.3333333134651184` should be `333333333.3333333`
  - May need custom JSON serialization for float values
  - Fixes: tjs12 (toRdf)

- [ ] **T5.9** Fix JSON literal Unicode serialization
  - File: `JsonLdRdf.qc`
  - Emoji characters should be serialized as direct UTF-8, not surrogate pairs
  - Fixes: tjs13 (toRdf)

- [ ] **T5.10** Fix number representation edge cases
  - File: `JsonLdRdf.qc` `valueToRdfTerm()`
  - `-0e0` should normalize to integer `0`, not double `-0.0E0`
  - Large numbers `>=1e21` need special formatting
  - Fixes: trt01 (toRdf)

- [ ] **T5.11** Fix native type coercion with @type:@id
  - File: `JsonLdExpansion.qc`
  - `true`/`false`/`0`/`1` with `@type: @id` should NOT be expanded to IRIs
  - Fixes: te088 (toRdf)

- [ ] **T5.12** Fix list with invalid/null @base
  - File: `JsonLdRdf.qc`
  - tli12: invalid IRI in list should be dropped
  - tli14: null @base should prevent relative IRI emission
  - Fixes: tli12, tli14 (toRdf)

- [ ] **T5.13** Fix native type coercion with custom datatype
  - File: `JsonLdRdf.qc`
  - te061: float values with custom @type should use the specified datatype
  - Fixes: te061 (toRdf)

## P6: Context Processing

- [ ] **T6.1** Allow recursive scoped context references
  - File: `JsonLdContext.qc` `processRemoteContext()`
  - A scoped context should be allowed to reference itself (it's only a cycle
    when actually PROCESSED, not when defined)
  - The cycle detection should track contexts being PROCESSED, not contexts
    that APPEAR in definitions
  - Fixes: t0126, t0127, t0128 (expand+toRdf = 6)

- [ ] **T6.2** Fix protected term comparison across scoped contexts
  - File: `JsonLdContext.qc` `checkProtectedRedefinition()`
  - Include `localContext` and `baseUrl` in the comparison for determining
    if two term definitions are "identical"
  - Fixes: tpr25 (expand+toRdf = 2)

- [ ] **T6.3** Fix protected flag propagation through scoped contexts
  - File: `JsonLdContext.qc` step 27
  - When a protected term is redefined identically, retain the protected flag
  - When checking, compare the FULL definition including scoped context
  - Fixes: tpr42 (expand+toRdf = 2)

- [ ] **T6.4** Fix keyword-like term with @vocab protected handling
  - File: `JsonLdContext.qc`
  - When a protected term maps to a keyword-like value with @vocab, the
    property should still be expanded using the IRI mapping
  - Fixes: tpr37 (expand+toRdf = 2)

- [ ] **T6.5** Fix @propagate:false with @import
  - File: `JsonLdContext.qc`
  - When a property-scoped context has `@propagate: false` and uses `@import`,
    the imported terms should be reverted for nested nodes
  - Fixes: tso06 (expand+toRdf = 2)

- [ ] **T6.6** Fix @vocab expansion for compact IRI values
  - File: `JsonLdContext.qc`
  - When @vocab is set to a compact IRI like `ex:ns/`, it should be expanded
    using the context and used as the vocabulary mapping
  - Fixes: t0124, te124 (expand+toRdf)

## P9: Negative Tests

- [ ] **T9.1** Add relative IRI term validation
  - File: `JsonLdContext.qc` `createTermDefinition()`
  - Terms like `./something` that contain `/` must expand to the same IRI
    as their @id mapping; if not, throw "invalid IRI mapping"
  - Fixes: ter48 (expand+toRdf = 2)

- [ ] **T9.2** Add IRI confused with prefix detection
  - File: `JsonLdIri.qc` `compactIri()` step 9
  - If a compact IRI's scheme matches a term with prefix flag and the IRI
    has no authority (`//`), throw "IRI confused with prefix"
  - Fixes: te002 (compact)

- [ ] **T9.3** Add @nest term validation in compaction
  - File: `JsonLdCompaction.qc`
  - When compacting, if a term's @nest value references an undefined term,
    throw "invalid @nest value"
  - Fixes: ten01 (compact)

- [ ] **T9.4** Fix protected term redefinition detection
  - File: `JsonLdContext.qc`
  - Ensure protected flag is compared correctly when scoped contexts are
    involved; detect redefinition when the scoped context differs
  - Fixes: tpr26, tpr42 (expand+toRdf = 4)

## Remaining Individual Fixes

- [ ] **T10.1** Fix @included compaction array wrapping
  - File: `JsonLdCompaction.qc` @included handler
  - Single @included should sometimes be in array depending on context
  - Fixes: tin01 (compact)

- [ ] **T10.2** Fix JSON literal array compaction
  - File: `JsonLdCompaction.qc`
  - JSON literal arrays double-wrapped: `[[{...}]]` instead of `[{...}]`
  - Fixes: tjs07 (compact)

- [ ] **T10.3** Fix reverse property expansion with blank node values
  - File: `JsonLdExpansion.qc` or `JsonLdFlattening.qc`
  - Reverse property with unlabeled blank node values should generate triples
  - Fixes: te064 (toRdf)

- [ ] **T10.4** Fix property-valued index on graph objects
  - File: `JsonLdExpansion.qc`
  - Property-valued index with @graph container: index property should be
    added to the graph object, not kept as @index
  - Fixes: tpi07, tpi09, tpi11 (expand+toRdf)

- [ ] **T10.5** Fix @id:null preservation through hash operations
  - File: `JsonLdExpansion.qc`, `JsonLdValue.qc`
  - `{"@id": null}` must survive through all hash copy/merge operations
  - Likely a Qore-specific issue with NOTHING in hash values
  - Fixes: t0122 (expand), te122 (toRdf)

- [ ] **T10.6** Fix t0035 language map merge ordering
  - File: `JsonLdExpansion.qc`
  - When multiple properties expand to the same IRI (e.g., language map
    and explicit property), the ordering of merged values matters
  - Fixes: t0035 (expand+flatten+toRdf = 3)

- [ ] **T10.7** Fix reverse term with property-based index expansion
  - File: `JsonLdExpansion.qc`
  - Reverse term + property index: expand the index property correctly
  - Fixes: t0131, t0133 (expand+toRdf)

---

## Execution Checklist

**Quick wins (< 30 min each, do first):**
- [ ] T8.1 (compactArrays option) — 4 tests
- [ ] T8.2 (download toRdf context files) — 3 tests
- [ ] T7.2 (@nest ordering) — 5 tests
- [ ] T3.5 (t0073 @type alias fix) — 1 test
- [ ] T5.2 (blank node N-Quads @type) — 2 tests

**Medium effort (30-60 min each):**
- [ ] T1.1 (type-scoped context propagation) — 30 tests (THE big one)
- [ ] T2.2 (@direction term selection) — 5 tests
- [ ] T3.1 (property-valued index compaction) — 7 tests
- [ ] T4.1 (opaque URI schemes) — 4 tests
- [ ] T5.1 (compound-literal rdfDirection) — 4 tests
- [ ] T6.1 (recursive scoped context) — 6 tests
- [ ] T7.1 (empty @set preservation) — 3 tests

**Harder (1-2 hours each):**
- [ ] T1.3 (type-scoped context in compaction) — depends on T1.1
- [ ] T2.1 (heterogeneous list ranking) — 4 tests
- [ ] T3.2-T3.4 (@graph container edges) — 8 tests
- [ ] T4.3-T4.4 (relative IRI compaction + dot segments) — 8 tests
- [ ] T5.5-T5.6 (fromRdf list edges) — 8 tests
- [ ] T6.2-T6.5 (protected term edges) — 8 tests
