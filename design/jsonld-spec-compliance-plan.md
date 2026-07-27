> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# JSON-LD Spec Compliance Plan — Definitive Fixes

24 deviations found across 8 files. Each must be fixed to reach 100%.
Ordered by cascade impact — fixing earlier items enables later ones.

## Fix 1: Expansion Algorithm Step Ordering (E2 + E3 + E1 + E4)

**THE critical fix.** The expansion step ordering is wrong. The spec says:

```
Step 5:  Process element's @context
Step 8:  Apply property-scoped context (for objects only)
Step 10: Restore previousContext (if no @value key)
Step 13: Apply type-scoped contexts
Step 14: Process properties
```

Our code does:
```
expand():  Apply property-scoped context (ALL element types, line 76)
expand():  Remove previousContext unconditionally (line 84)
expandObject(): Apply type-scoped contexts (line 174)
expandObject(): Restore previousContext with extra non-spec conditions (line 215)
expandObject(): Process properties
```

### Fix 1a: Move property-scoped context to after object check
- File: `JsonLdExpansion.qc` lines 76-88
- Move the property-scoped context application from `expand()` to
  `expandObject()`, AFTER step 5 (@context processing) but BEFORE step 10
- Remove the unconditional `previousContext` deletion (lines 84-86)
- For scalars: the property-scoped context should still affect `expandValue()`
  via the active context that was modified BEFORE the recursive `expand()` call
  in `expandObject()` when processing the property value

### Fix 1b: Reorder steps in expandObject
- File: `JsonLdExpansion.qc` lines 174-252
- Current order: type-scoped (174-209), then previousContext (215-252)
- Correct order: previousContext (step 10) FIRST, then type-scoped (step 13)
- After previousContext restoration, the type-scoped context is applied ON TOP
  of the restored context

### Fix 1c: Remove extra has_type_scope check from previousContext restoration
- File: `JsonLdExpansion.qc` lines 226-249
- The spec says: restore previousContext if element has no key expanding to @value
- Our code adds an extra check for @type with scoped context — REMOVE IT
- The type-scoped context application at step 13 handles this correctly

### Fix 1d: Capture type_expansion_context at the right time
- File: `JsonLdExpansion.qc` line 172
- Save `type_expansion_context` AFTER previousContext restoration (step 10)
  but BEFORE type-scoped context application (step 13)

## Fix 2: IRI Expansion vocab fallback (I2)

- File: `JsonLdIri.qc` lines 119-128
- Remove the non-spec base IRI concatenation fallback
- The spec says step 7 returns `vocabularyMapping + value`, period
- This will break `@vocab: ""` tests — those need a SEPARATE fix in
  `expandObject` to allow relative properties when @vocab is set

## Fix 3: IRI Compaction relative IRI (I5)

- File: `JsonLdIri.qc` lines 425-438
- Implement RFC 3986 Section 5 relative reference computation (reverse of resolution)
- Compute common prefix between base path and IRI path
- Emit `../` segments as needed
- Handle fragment (`#`), query (`?`), and same-directory (`./`) forms

## Fix 4: Inverse Context cleanup (I6 + I7)

- File: `JsonLdIri.qc` lines 524-542 and 575-583
- Remove the non-spec @any population for typed terms (lines 537-542)
- Remove the @any fallback in selectTerm (lines 576-583)
- The @any dimension is only used when type_lang is explicitly set to "@any"
  by the caller (e.g., for empty lists)

## Fix 5: Flattening @id-only node filtering (F1 + F2)

- File: `JsonLdFlattening.qc` lines 70-71 and 79-80
- Remove BOTH filters — include ALL nodes from default and named graphs
- The spec says no filtering at the flatten step

## Fix 6: fromRdf @id-only node filtering (R7)

- File: `JsonLdRdf.qc` lines 140-154
- ADD filtering: skip nodes in the default graph that have ONLY @id and
  don't have a corresponding named graph entry
- This is the OPPOSITE of F1/F2 — fromRdf SHOULD filter while flatten should NOT

## Fix 7: List reconstruction fixes (R5 + R6)

- File: `JsonLdRdf.qc` lines 196-233
- Allow `@type: [rdf:List]` on list nodes (add @type to allowed keys,
  verify it only contains rdf:List)
- Use referenced_as_rest to identify list HEADS (nodes NOT referenced as
  rdf:rest from other nodes)
- Only start list extraction from heads, not from all list nodes

## Fix 8: IRI Resolution RFC 3986 compliance (U1 + U2 + U3 + U4)

- File: `JsonLdUtils.qc` lines 111-210
- Replace `parse_url()` with RFC 3986 Appendix B regex for ALL schemes:
  `^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?`
- Fix empty reference to strip fragment from base
- Remove "https" default — use the parsed scheme
- Properly handle query component preservation/replacement

## Fix 9: Context Processing fixes (C3 + C4 + C5 + C6 + C7)

- File: `JsonLdContext.qc`
- C3: Add blank node identifier check for prefix flag in simple term defs
- C4: Change `term.find(":") > 0` to `>= 0` for colon-at-position-0 terms
- C5: Follow spec for auto-prefix detection instead of heuristic
- C6 + C7: Store language tags with original case, apply case-insensitive
  comparison where the spec requires it

## Fix 10: Compaction Algorithm fixes (K1 + K3 + K4 + K5 + K6)

- File: `JsonLdCompaction.qc`
- K1: Remove the @id shortcut at lines 102-107 — all node objects go through
  compactNodeObject
- K3: Remove the single-representative-value approach — use per-value
  processing for all container checks
- K4: @included MUST always be an array
- K5: @graph values should always be arrays per spec
- K6: Remove the invented @included wrapping for @graph containers —
  just use arrays

## Fix 11: Nested property handling (E5)

- File: `JsonLdExpansion.qc` lines 452-498
- Process nested properties through the SAME property processing pipeline
  as regular properties, not a simplified handler
- Extract nested key-value pairs and feed them through the main key loop

## Fix 12: @graph container wrapping (E6)

- File: `JsonLdExpansion.qc` lines 410-422
- Check `isGraphObject(item)` before wrapping
- Don't double-wrap items that are already graph objects

## Fix 13: Remaining fixes

- R2: Remove defensive @type-as-list handling in valueToRdfTerm (add assertion)
- R4: Use more permissive xsd:double regex
- F4: Implement proper deep structural equality for deduplication
- V3: Ensure compactValue always compacts internal hash keys
- K2: Only look up type-scoped contexts by expanded IRI, not compacted

## Implementation Order

| # | Fix | Files | Tests | Effort | Dependency |
|---|-----|-------|-------|--------|-----------|
| 1 | Expansion step ordering | JsonLdExpansion.qc | ~25 | 3h | None |
| 8 | RFC 3986 IRI resolution | JsonLdUtils.qc | ~10 | 2h | None |
| 2 | IRI vocab fallback | JsonLdIri.qc + JsonLdExpansion.qc | ~5 | 1h | Fix 1 |
| 5+6 | Flatten/fromRdf filtering | JsonLdFlattening.qc + JsonLdRdf.qc | ~8 | 1h | None |
| 7 | List reconstruction | JsonLdRdf.qc | ~5 | 1h | None |
| 4 | Inverse context cleanup | JsonLdIri.qc | ~5 | 1h | None |
| 3 | Relative IRI compaction | JsonLdIri.qc | ~8 | 2h | Fix 8 |
| 9 | Context processing | JsonLdContext.qc | ~5 | 1h | None |
| 10 | Compaction algorithm | JsonLdCompaction.qc | ~10 | 2h | Fix 1, Fix 4 |
| 11+12 | Nested props + @graph | JsonLdExpansion.qc | ~5 | 1h | Fix 1 |
| 13 | Remaining | Various | ~5 | 1h | Various |
| | **Total** | | **~96** | **~16h** | |

## Key Principle

Every fix must follow the spec algorithm STEP BY STEP:
- Match the spec's step numbers in code comments
- Use the spec's exact conditions (no extra checks, no missing checks)
- Maintain the spec's exact ordering (steps must execute in spec order)
- No "optimizations" that change behavior (shortcuts that bypass steps)
- No "defensive" code that masks bugs in other algorithms
