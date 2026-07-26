> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# JSON-LD Foundation Fixes Checklist

Ordered by dependency depth — lowest-level (most depended-on) first.
Each fix should be tested before moving to the next.

## Layer 0: Type System Primitives (everything depends on these)

These are Qore-specific bugs where JSON null vs absent is conflated.

- [x] **F0.1** `isValueObject()` uses `exists` instead of `hasKey("@value")`
  - File: `JsonLdUtils.qc:277`
  - Bug: `{"@value": null}` not detected as value object
  - Impact: Cascades through ALL algorithms — expansion, compaction, flattening, RDF
  - Fix: `return v.typeCode() == NT_HASH && cast<hash<auto>>(v).hasKey("@value");`

- [x] **F0.2** `isListObject()` uses `exists` instead of `hasKey("@list")`
  - File: `JsonLdUtils.qc:287`
  - Same pattern as F0.1

- [x] **F0.3** Audit every `exists obj."@keyword"` in all files
  - Systematic search for `exists.*"@` patterns that should be `hasKey`
  - Key locations: expandObject post-processing, compactNodeObject property
    checks, generateNodeMap, processNodeToRdf
  - Rule: use `hasKey()` when checking if a key is present (JSON null is present),
    use `exists` only when checking if the VALUE is non-null

## Layer 1: IRI Expansion (`expandIri`) — called by everything

- [~] **F1.1** (DEFERRED — causes regressions, needs deeper analysis) Remove non-spec base IRI resolution in step 7
  - File: `JsonLdIri.qc:114-120` (the `@vocab: ""` concatenation)
  - Bug: Spec says step 7 is just `vocabularyMapping + value`, period.
    The extra base IRI concatenation produces wrong IRIs.
  - Fix: Remove lines 114-120, return `expanded` directly.
    Tests that need `@vocab: ""` resolution will need separate handling
    in the caller (expandObject property filter).

- [x] **F1.2** Step 6.4: check `prefix` flag on term definition (already correct)
  - File: `JsonLdIri.qc:105`
  - Bug: Current code checks `exists prefix_td.iriMapping` but not
    `prefix_td.prefix`. Spec says step 6.4 requires the prefix flag to be true.
  - Fix: Add `&& prefix_td.prefix` to the condition

## Layer 2: Context Processing (`processContext` + `createTermDefinition`)

- [x] **F2.1** Fix step ordering: propagate check AFTER null check
  - File: `JsonLdContext.qc:108`
  - Bug: `previousContext` saved before context nullification
  - Fix: Move the `!propagate` check from top of `processContextItem` to after
    the null context handling block (after line 134)

- [ ] **F2.2** `@vocab` expansion should use IRI Expansion algorithm
  - File: `JsonLdContext.qc:187-206`
  - Bug: Uses custom `expandIriInContext()` heuristic instead of spec algorithm
  - Fix: Use `expandIri()` with vocab=true, document_relative=true

- [x] **F2.3** Handle `{"@type": null}` in context — should not throw
  - File: `JsonLdContext.qc:462`
  - Bug: Non-hash @type value throws error, but null should remove definition
  - Fix: Check for null before the hash check

- [~] **F2.4** (VERIFIED CORRECT per spec) `@id == term` case should still process @id (step 15 vs 16)
  - File: `JsonLdContext.qc:611`
  - Bug: When `val."@id" == term`, code falls through to step 16 (no @id)
    instead of processing @id normally
  - Fix: Change condition to `val.hasKey("@id")` and handle `@id == term`
    inside the block

- [x] **F2.5** Add `indexProperty` field to `JsonLdTermDefinition`
  - File: `JsonLdTypes.qc` + `JsonLdContext.qc:721`
  - Bug: Property-based indexing (`"@index": "schema:dateCreated"`) is silently
    ignored — no field to store it
  - Fix: Add `*string indexProperty` to hashdecl, store expanded @index value

- [x] **F2.6** Validate @container combinations
  - File: `JsonLdContext.qc:686-707`
  - Bug: Invalid combinations like `[@list, @index]` not rejected
  - Fix: Add validation per spec step 19.1:
    - @list cannot combine with @id, @index, @language, @type, @graph
    - @graph can combine with @id, @index, @set
    - @set can combine with any single other container
    - All others are invalid

- [x] **F2.7** @container @type requires @type mapping to be @id or @vocab
  - File: `JsonLdContext.qc` after container processing
  - Bug: No validation for spec step 19.4
  - Fix: If container includes @type, validate typeMapping is @id, @vocab, or unset
    (if unset, default to @id)

- [x] **F2.8** @language and @type are mutually exclusive in term definitions
  - File: `JsonLdContext.qc` — steps 22-23
  - Bug: Both can be set simultaneously without error
  - Fix: Steps 22 and 23 say "and does not contain the entry @type" —
    add this check

- [x] **F2.9** Keyword-like terms (starting with @) should set defined=true
  - File: `JsonLdContext.qc:469`
  - Bug: Returns without setting `defined{term} = True`
  - Fix: Set `defined{term} = True` before returning

- [x] **F2.10** Step 26: Validate no unknown keys in term definition
  - File: `JsonLdContext.qc` — before setting definition
  - Bug: Extra keys like `"foo": "bar"` in a term definition are silently
    accepted instead of throwing "invalid term definition"
  - Fix: Check that value only contains known keys

## Layer 3: Expansion Algorithm (`expand` + `expandObject`)

- [x] **F3.1** Previous context restoration: check @value, not @set
  - File: `JsonLdExpansion.qc:127-129`
  - Bug: Checks `!elem.hasKey("@set")` but spec says check `!elem.hasKey("@value")`
    and also check if @type values match type-scoped terms
  - Fix: Change to `!elem.hasKey("@value")` and for @type, check if any
    expanded type has a term with scoped context in the active context
    (if so, don't revert)

- [~] **F3.2** (DEFERRED — @nest IS a data keyword per spec) @nest in data should NOT be treated as keyword
  - File: `JsonLdExpansion.qc:610-617`
  - Bug: @nest appears in the keyword switch, validates value type.
    In data, @nest is only meaningful in term definitions. During expansion
    of a data object, if the expanded property is @nest, it should be
    ignored (already handled via nest processing elsewhere).
  - Fix: The @nest keyword case should not throw errors for data values.
    Currently it throws for non-object values, but @nest in data
    should just be silently dropped.

- [ ] **F3.3** Nest property processing should happen DURING key iteration
  - File: `JsonLdExpansion.qc:330-348`
  - Bug: Nested properties processed AFTER all regular properties
  - Fix: During key iteration, when a key has nestValue, immediately
    expand its nested content inline (before processing the next key)

- [~] **F3.4** (DEFERRED — coupled with F1.1) `@vocab: ""` property filter should allow relative IRIs
  - File: `JsonLdExpansion.qc:203-206`
  - Bug: After expandIri with `@vocab: ""`, properties that resolve to
    relative-looking absolute IRIs (like `http://base/../foo`) are
    correctly kept. But if F1.1 is fixed (removing base concatenation),
    `@vocab: ""` would produce relative results that get filtered out.
  - Fix: When @vocab is `""`, treat the expandIri result as valid even
    if it's not an absolute IRI (the property IS vocab-mapped).
    Alternative: keep the base concatenation in expandIri for this case.

- [ ] **F3.5** Empty result handling at end of expandObject
  - File: `JsonLdExpansion.qc:413`
  - Bug: `result.empty() ? NOTHING : result` unconditionally drops empty
    objects, but empty node objects should be returned when
    active_property is set (not null/@@graph)
  - Fix: Only return NOTHING for empty results when active_property is
    null or @graph (this is already guarded by step 19 above, so the
    empty check at line 413 should be safe to keep as-is, but verify)

## Layer 4: Compaction Algorithm

- [x] **F4.1** @type compaction should respect @set container
  - File: `JsonLdCompaction.qc:236`
  - Bug: Always compacts @type array to scalar when size==1
  - Fix: Check if the @type alias term has @set container; if so, keep array

- [~] **F4.2** (VERIFIED CORRECT per spec) @index compaction checks wrong property for container
  - File: `JsonLdCompaction.qc:282`
  - Bug: `hasContainerMapping(active_context, active_property, "@index")` checks
    the PARENT property, not the current node's property
  - Fix: This is actually correct per spec (the @index is suppressed when the
    parent property has @index container). Verify and leave as-is.

- [x] **F4.3** Add @graph container compaction
  - File: `JsonLdCompaction.qc` — add after @id container handler
  - Bug: Missing entirely
  - Fix: When term has @graph container (possibly + @index/@id),
    unwrap @graph arrays into map structure

- [x] **F4.4** Add @type container compaction
  - File: `JsonLdCompaction.qc` — add after @index container handler
  - Bug: Missing entirely
  - Fix: Group values by their @type into a type map

- [x] **F4.5** compactValueObject should strip @index when term has @index container
  - File: `JsonLdCompaction.qc` or `JsonLdValue.qc:131`
  - Bug: Value objects with @index are never compacted to plain values
  - Fix: If the term has @index container, remove @index before compaction check

## Layer 5: Node Map / Flattening

- [ ] **F5.1** Detect conflicting @index values
  - File: `JsonLdFlattening.qc:205`
  - Bug: Silently overwrites @index when it conflicts
  - Fix: If node already has @index and it differs, throw error

## Layer 6: RDF Conversion

- [x] **F6.1** Filter blank node predicates when !produceGeneralizedRdf
  - File: `JsonLdRdf.qc:415-416`
  - Bug: Blank node predicates always included
  - Fix: Skip blank node predicates when `!options.produceGeneralizedRdf`

- [ ] **F6.2** Validate well-formed IRIs in toRdf
  - File: `JsonLdRdf.qc:processNodeToRdf`
  - Bug: Malformed IRIs produce invalid N-Quads
  - Fix: Skip subjects/predicates/objects with malformed IRIs

- [x] **F6.3** fromRdf list reconstruction: tighten "pure list node" check
  - File: `JsonLdRdf.qc:167-176`
  - Bug: Allows @type on pure list nodes, may over-reconstruct
  - Fix: Pure list nodes should ONLY have @id, rdf:first, rdf:rest (no @type)

## Implementation Order

1. **F0.x** (30 min) — fix type primitives, immediate cascade everywhere
2. **F1.x** (30 min) — fix expandIri, cascades to expansion/compaction/RDF
3. **F2.1-F2.4** (1 hr) — fix critical context processing bugs
4. **F3.1-F3.2** (30 min) — fix expansion previousContext and @nest
5. **F2.5-F2.10** (1 hr) — remaining context validations
6. **F4.1-F4.5** (1.5 hr) — compaction container handlers
7. **F3.3-F3.5** (30 min) — remaining expansion edge cases
8. **F5.x + F6.x** (30 min) — flattening and RDF fixes
