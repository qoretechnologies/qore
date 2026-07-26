> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# JSON-LD 100% Conformance Completion Plan

Current: 1009/1167 (86.5%). Skipped: 42. Effective target: 1009 → 1125.
158 remaining failures classified into 13 categories across 7 fix files.

## Category Summary

| Cat | Count | Description |
|-----|-------|-------------|
| A | 28 | Type-scoped context propagation — lost/wrong in nested objects |
| B | 5 | Type-scoped value expansion — context needed for value coercion |
| C | 11 | Property-scoped context chaining — recursive contexts, @propagate |
| D | 7 | Protected term handling |
| E | 14 | Compact term selection — direction, list specificity |
| F | 18 | Container compaction — @graph, @index, property-valued index |
| G | 17 | IRI resolution — relative, opaque schemes, fragment+query |
| H | 7 | Infrastructure — missing files, test runner option gaps |
| I | 7 | Negative test — expected error not thrown |
| J | 22 | RDF edge cases — blank nodes, lists, compound literals |
| K | 6 | Value preservation — null, empty set, ordering |
| L | 5 | @nest value ordering |
| M | 11 | Other — boolean expansion, reverse, list dedup |

## Fix Files (by impact)

| File | Failures | Key Changes |
|------|----------|-------------|
| `JsonLdCompaction.qc` | ~40 | Term selection (direction, list, type), container compaction, IRI |
| `JsonLdExpansion.qc` | ~35 | Type-scoped propagation, @nest ordering, @type:@none, booleans |
| `JsonLdRdf.qc` | ~22 | Blank node N-Quads, compound literal, native types, JSON precision |
| `JsonLdContext.qc` | ~18 | Recursive context, protected terms, @vocab, @propagate |
| `JsonLdIri.qc` | ~12 | Fragment+query, dot segments, opaque URI, relative IRI |
| `JsonLdFlattening.qc` | ~8 | Empty set, reverse properties, multi-graph, list equivalence |
| `JsonLdW3cConformance.qtest` | ~4 | compactArrays option propagation |
| Test data files | ~3 | Download missing e126/e127/e128 context files |

---

## Phase 1: Type-Scoped Context (A+B) — 33 tests

The single biggest cluster. The core problem: `previousContext` restoration
in `expand()` step 10 reverts ALL nested objects, but should only revert
when entering a truly new node scope, not for property values of the typed node.

### P1.1: Track context scope depth
- File: `JsonLdExpansion.qc`
- Add `typeScopeActive` flag or depth counter to track when we're inside
  a typed node's properties vs a new nested node
- When `previousContext` exists and element is a hash:
  - If element has @type with scoped context → keep context (already done)
  - If element is expanded as a value via @type:@id/@vocab → keep context
  - If element appears as a direct property value of the typed node → keep context
  - Only revert for genuinely nested node objects (objects with their own properties
    that aren't value objects)

### P1.2: Fix type-scoped @base and @vocab
- File: `JsonLdExpansion.qc`, `JsonLdContext.qc`
- Type-scoped `@base`/`@vocab` should persist for the typed node's property
  values but not leak to nested nodes

### P1.3: Fix type-scoped value coercion (B tests)
- File: `JsonLdExpansion.qc`
- When a type-scoped context defines a term as `@value` alias or sets
  `@type` on a property, the value expansion should use these definitions
  even for nested objects

**Tests**: tc012, tc013, tc015, tc018, tc019, tc020, tc021, tc024, tc031,
tc037, tc038 (each appears in expand + compact + toRdf = ~33 total)

## Phase 2: Compact Term Selection (E) — 14 tests

### P2.1: Heterogeneous list term ranking
- File: `JsonLdIri.qc` step 4.7, `JsonLdCompaction.qc`
- When a @list has mixed types/languages, select the term with NO type/language
  restriction, not the first matching term
- Tests: t0015, t0018, t0024, tla01

### P2.2: @direction compound keys in inverse context
- File: `JsonLdIri.qc` `buildInverseContext()`
- Store terms with `@direction` using compound `lang_dir` keys
- Match values with `@language` + `@direction` against these compound keys
- Tests: tdi03, tdi04, tdi05, tdi06, tdi07

### P2.3: @type:@set for @type keyword
- File: `JsonLdCompaction.qc`
- Test t0106: `@type` with `@container: @set` should still compact single
  values to scalar (special @type behavior)

### P2.4: Term selection with @type:@id on @list container
- File: `JsonLdCompaction.qc`
- Test t0074: empty list with @type:@id container not finding the right term

## Phase 3: Container Compaction (F) — 18 tests

### P3.1: Property-valued index compaction
- File: `JsonLdCompaction.qc`
- Use `td.indexProperty` to extract index values from property values
  and build the compacted index map
- Tests: tpi01-tpi04, t0112-t0114

### P3.2: @graph container edge cases
- File: `JsonLdCompaction.qc`
- t0080: don't use @graph term when node has @id
- t0083: don't use [@graph,@index] term when node has @id
- t0090-t0094: @graph with/without @set unwrapping
- t0109-t0110: @graph with multiple objects → @included

### P3.3: Reverse term with property index
- File: `JsonLdCompaction.qc`
- t0114: reverse term + property index compaction

## Phase 4: IRI Resolution (G) — 17 tests

### P4.1: Opaque URI scheme handling
- File: `JsonLdUtils.qc` `resolveIri()`
- `tag:`, `ex:` etc. are opaque (non-hierarchical) schemes — don't add `//`
- Tests: t0130, t0131, t0132, tli11

### P4.2: Fragment + query resolution
- File: `JsonLdUtils.qc` `resolveIri()`
- `file#fragment?query` → query replaces query, fragment stripped
- Tests: t0062, te062

### P4.3: Relative IRI compaction improvements
- File: `JsonLdIri.qc` `compactIri()`
- Produce `#fragment`, `?query`, `./relative` forms
- Tests: t0045, t0066, t0095, t0111

### P4.4: Dot-segment preservation in IRI resolution tests
- File: `JsonLdUtils.qc`
- t0122-t0125: 42-line IRI resolution tests with specific dot-segment behavior

## Phase 5: RDF Edge Cases (J) — 22 tests

### P5.1: Compound-literal rdfDirection mode
- File: `JsonLdRdf.qc`
- Generate blank nodes with rdf:value, rdf:language, rdf:direction predicates
- Tests: tdi11, tdi12 (toRdf + fromRdf = 4)

### P5.2: Blank node serialization in N-Quads
- File: `JsonLdRdf.qc`
- @type values that are blank nodes: `_:b1` not `<_:b1>`
- Tests: tm003, tm004, te068, te075

### P5.3: fromRdf improvements
- File: `JsonLdRdf.qc`
- t0016: rdf:List typed nodes
- t0017: duplicate triple removal
- t0020-t0022: cross-graph list detection
- t0027: native type edge cases
- tli01-tli03: nested list fromRdf

### P5.4: JSON literal precision
- File: `JsonLdRdf.qc`
- tjs12: float precision in JSON serialization
- tjs13: Unicode emoji serialization

### P5.5: Number representation
- File: `JsonLdRdf.qc`
- trt01: `-0e0` → integer 0, large exponent numbers

## Phase 6: Context Processing (C+D) — 18 tests

### P6.1: Recursive scoped context detection
- File: `JsonLdContext.qc`
- Allow scoped contexts to reference themselves (not a cycle)
- Tests: t0126, t0127, t0128

### P6.2: Protected term edge cases
- File: `JsonLdContext.qc`
- tpr25: identical redefinition across scopes
- tpr26, tpr42: protected flag propagation through scoped contexts
- tpr37: keyword-like value with @vocab
- tpr04: property-scoped override should be legal

### P6.3: @propagate with @import
- File: `JsonLdContext.qc`
- tso06: @propagate:false + @import combination

## Phase 7: Flatten/Expansion/Other (K+L+M) — 22 tests

### P7.1: Empty @set preservation
- File: `JsonLdExpansion.qc`, `JsonLdFlattening.qc`
- Properties with @container:@set and empty values should preserve `[]`
- Tests: t0004, t0015, t0016 (flatten)

### P7.2: @nest value ordering
- File: `JsonLdExpansion.qc`
- Nested values should be appended AFTER base values, not prepended
- Tests: tn003-tn007

### P7.3: Boolean/native value expansion
- File: `JsonLdExpansion.qc`
- `true`/`false` with @language context should still produce value objects
- Tests: t0018, te018, te088

### P7.4: Flatten edge cases
- File: `JsonLdFlattening.qc`
- t0021: multi-graph flatten
- t0039: reverse property flatten
- t0042: list deduplication

## Phase 8: Infrastructure (H) — 7 tests

### P8.1: compactArrays option in test runner
- File: `test/JsonLdW3cConformance.qtest`
- Propagate `compactArrays` option from manifest to test options
- Tests: t0070, t0091, t0093, t0044 (flatten)

### P8.2: Download remaining context files
- Download e126/e127/e128 context chain files for toRdf
- Tests: te126, te127, te128

### P8.3: Fix document loader double-slash
- Fix relative URL resolution in W3cTestDocumentLoader
- Test: tc031

## Phase 9: Negative Tests (I) — 7 tests

### P9.1: Remaining error validations
- ter48: relative IRI term validation
- tpr26, tpr42: protected redefinition detection
- te002: IRI confused with prefix
- ten01: undefined nest term
- Tests in both expand and toRdf suites

## Implementation Priority

| Priority | Phase | Tests | Effort | Cascade |
|----------|-------|-------|--------|---------|
| 1 | P1 Type-scoped context | 33 | 4h | expand→compact→toRdf |
| 2 | P7 Flatten/expansion fixes | 22 | 2h | flatten→toRdf |
| 3 | P2 Term selection | 14 | 2.5h | compact only |
| 4 | P3 Container compaction | 18 | 2.5h | compact only |
| 5 | P5 RDF edge cases | 22 | 3h | toRdf+fromRdf |
| 6 | P4 IRI resolution | 17 | 2h | expand→toRdf |
| 7 | P6 Context processing | 18 | 2h | expand→compact→toRdf |
| 8 | P8 Infrastructure | 7 | 0.5h | all suites |
| 9 | P9 Negative tests | 7 | 1h | expand+toRdf |
| | **Total** | **~158** | **~19.5h** | |

Note: ~40 of the 158 are cascade effects (same root cause appearing in
multiple suites). Fixing ~116 unique issues produces all 158 fixes.
