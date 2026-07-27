> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# Plan: Fix All 135 Remaining W3C JSON-LD Conformance Failures

Current: 1032/1167 (88.4%). Target: 1167/1167 (100%).
135 failures across ~95 unique test IDs. 42 tests permanently skipped.

## Group 1: Type-Scoped Context Deep Propagation (22 tests)

These all share the same root cause: the type-scoped context is reverted
too early for nested objects that should still use it.

**Root fix needed in `JsonLdExpansion.qc`:**
The previousContext restoration in expandObject checks if ANY key expands
to @value (already done). But it also needs to check if ANY key expands
to @type with a scoped context. More importantly, nested objects that
are property values of the typed node need the type-scoped context for
property expansion — the restoration should only happen for "deep" nesting,
not direct property values.

**Tests:** tc009, tc012(×3), tc013(×2), tc014, tc015(×3), tc016, tc017,
tc018(×3), tc019(×2), tc021, tc023, tc024(×3), tc037(×2), tc038(×2)

**Subtasks:**
- [ ] G1.1: In expandObject, don't revert previousContext when expanding
  properties — only revert when entering a NEW nested expandObject call
  (i.e., move restoration from expandObject into expand() but ONLY for
  recursion depth > 1)
- [ ] G1.2: Fix type map scoped context application (tc013)
- [ ] G1.3: Fix @type term sorting for scoped context application (tc018)
- [ ] G1.4: Fix property-scoped context on @nest alias (tc037)

## Group 2: Compact Term Selection (10 tests)

Wrong term selected during compaction due to inverse context limitations.

**Tests:** t0015, t0018, t0024, tla01 (list term ranking),
tdi03-tdi07 (direction-aware), t0106 (@type @set)

**Subtasks:**
- [ ] G2.1: Fix list term ranking for heterogeneous lists
  - When list items have mixed types/languages, select the term with NO
    type/language restriction
  - File: `JsonLdIri.qc` step 4.7 common_type/common_language computation
- [ ] G2.2: Fix @direction compound keys in buildInverseContext
  - Store direction-qualified keys (lang_dir) for terms with @container:@language
    + @direction
  - File: `JsonLdIri.qc` buildInverseContext + compactIri step 4.9.1
- [ ] G2.3: Fix @type:@set compaction for @type keyword
  - @type keyword with @container:@set should still compact to scalar
  - File: `JsonLdCompaction.qc` @type handler

## Group 3: Container Compaction (14 tests)

Container map compaction edge cases.

**Tests:** t0080, t0083, t0090, t0092, t0094, t0109, t0110 (@graph),
tpi01-tpi04 (property index), t0112-t0114 (property index compact IRI),
t0074 (list + @type:@id)

**Subtasks:**
- [ ] G3.1: Implement property-valued index compaction
  - Extract index values from the expanded property (td.indexProperty)
  - Build compacted index map using property values as keys
  - File: `JsonLdCompaction.qc`
- [ ] G3.2: Fix @graph container: don't use when node has @id (t0080, t0083)
- [ ] G3.3: Fix @graph container with multiple objects → @included (t0109, t0110)
- [ ] G3.4: Fix @graph+@set unwrapping (t0090, t0092, t0094)
- [ ] G3.5: Fix list + @type:@id term selection (t0074)

## Group 4: IRI Resolution (12 tests)

IRI resolution edge cases and relative IRI compaction.

**Tests:** t0062/te062 (fragment+query), t0122-t0125 (42-line tests),
t0045/t0066/t0095/t0111 (relative IRI compaction),
te111/te112 (relative @vocab), t0124/te124 (@vocab compact IRI)

**Subtasks:**
- [ ] G4.1: Fix fragment+query IRI resolution (t0062)
  - Strip fragment before appending query in resolveIri
- [ ] G4.2: Fix 42-line IRI resolution tests (t0122-t0125)
  - Specific dot-segment handling differences
- [ ] G4.3: Fix relative IRI compaction (t0045, t0066, t0095, t0111)
  - Produce fragment, query, and ./ relative forms
- [ ] G4.4: Fix relative @vocab IRI resolution (te111, te112)
- [ ] G4.5: Fix @vocab as compact IRI (t0124, te124)

## Group 5: RDF Conversion (18 tests)

RDF serialization and deserialization edge cases.

**Tests:** tdi11/tdi12 (compound-literal × toRdf+fromRdf = 4),
tm004(×2) (blank node @type), te018/te036 (boolean expansion),
te061 (native type coercion), te064 (reverse blank node),
te075 (blank node @vocab), te088 (native values not IRIs),
te122 (extra triple from @-prefixed IRI), tjs12/tjs13 (JSON precision),
trt01 (number representation), tli12/tli14 (list @base),
t0016-t0022+t0027 (fromRdf edges), ttn02 (@type:@none expansion)

**Subtasks:**
- [ ] G5.1: Implement compound-literal rdfDirection mode (tdi11, tdi12)
- [ ] G5.2: Fix blank node @type N-Quads serialization (tm004)
- [ ] G5.3: Fix boolean expansion with @language (te018, te036)
- [ ] G5.4: Fix native value coercion (te061, te088, ttn02)
- [ ] G5.5: Fix reverse property with blank node values (te064)
- [ ] G5.6: Fix blank node @vocab predicate (te075)
- [ ] G5.7: Fix @-prefixed IRI extra triple (te122)
- [ ] G5.8: Fix JSON literal precision (tjs12, tjs13)
- [ ] G5.9: Fix number representation (trt01)
- [ ] G5.10: Fix list with invalid/null @base (tli12, tli14)
- [ ] G5.11: Fix fromRdf edge cases (t0016-t0022, t0027)
- [ ] G5.12: Fix fromRdf nested list (tli01-tli03)

## Group 6: Protected Terms (8 tests)

Protected term handling edge cases.

**Tests:** tpr04 (legal override), tpr25(×2) (scoped context identical),
tpr26(×2) (scoped context different), tpr30 (keyword protection),
tpr37(×2) (keyword-like with @vocab), tpr42(×2) (flag retention)

**Subtasks:**
- [ ] G6.1: Fix tpr04 — property-scoped override of protected term
  - The inline @context should be able to override protected terms when
    inside a property whose scoped context cleared protection
- [ ] G6.2: Fix protected term comparison across scoped contexts
  - Include localContext in the comparison (tpr25, tpr26, tpr42)
- [ ] G6.3: Fix keyword protection (tpr30)
- [ ] G6.4: Fix keyword-like term with @vocab (tpr37)

## Group 7: Infrastructure + Negative Tests (6 tests)

**Tests:** tc031(×2) (document loader path), te002 (IRI confused with prefix),
ten01 (undefined nest term), ter48(×2) (relative IRI term)

**Subtasks:**
- [ ] G7.1: Fix tc031 document loader path resolution
- [ ] G7.2: Implement IRI confused with prefix detection (te002)
- [ ] G7.3: Implement undefined nest term detection (ten01)
- [ ] G7.4: Fix relative IRI term validation (ter48)

## Group 8: Flatten Edge Cases (5 tests)

**Tests:** t0021 (multi-graph), t0036 (boolean @index), t0039 (reverse maps),
t0042 (list dedup)

**Subtasks:**
- [ ] G8.1: Fix multi-graph flatten (t0021)
- [ ] G8.2: Fix boolean in indexed container (t0036)
- [ ] G8.3: Fix reverse property forward link in flatten (t0039)
- [ ] G8.4: Fix list object deduplication (t0042)

## Group 9: Expansion Edge Cases (8 tests)

**Tests:** t0035(×2) (language map merge order), t0122 (@id:null),
t0131/t0133 (reverse property index), tn004/tn005(×2) (@nest ordering),
tpi07/tpi09 (property index ordering), tpi11(×2) (property index on graph),
tin01 (@included), tin06(×3) (json.api), tjs07 (JSON array), tso06(×2) (@propagate+@import)

**Subtasks:**
- [ ] G9.1: Fix language map merge ordering (t0035)
- [ ] G9.2: Fix @id:null preservation (t0122)
- [ ] G9.3: Fix reverse term property-based index (t0131, t0133)
- [ ] G9.4: Fix @nest ordering edge case (tn004, tn005)
- [ ] G9.5: Fix property index value ordering (tpi07, tpi09)
- [ ] G9.6: Fix property index on graph objects (tpi11)
- [ ] G9.7: Fix @included compaction (tin01)
- [ ] G9.8: Fix json.api @included handling (tin06)
- [ ] G9.9: Fix JSON literal array compaction (tjs07)
- [ ] G9.10: Fix @propagate:false + @import (tso06)

## Execution Priority

| Group | Tests | Effort | Cascade |
|-------|-------|--------|---------|
| G1 Type-scoped | 22 | 4h | expand→compact→toRdf |
| G5 RDF | 18 | 3h | toRdf+fromRdf |
| G3 Containers | 14 | 2.5h | compact |
| G4 IRI | 12 | 2h | expand→toRdf |
| G2 Term select | 10 | 2h | compact |
| G6 Protected | 8 | 1.5h | expand→toRdf |
| G9 Expansion | 8 | 2h | expand→toRdf |
| G7 Infra/neg | 6 | 1h | mixed |
| G8 Flatten | 5 | 1h | flatten |
| **Total** | **~135** | **~19h** | |

After deducting cascade duplicates (~30 tests appear in multiple suites),
there are ~95 unique fixes needed.
