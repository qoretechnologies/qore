> **Note**: This is a historical planning document from the JSON-LD 1.1 development process. All issues described here have been resolved. The module now passes 1167/1167 W3C conformance tests.

# JSON-LD Algorithmic Fixes Plan

Current: 774/1167 (66.3%). Target: 90%+ (~1050).
393 remaining failures classified by root cause.

## Priority 1: @nest expansion (~24 tests across expand+compact+toRdf)

**Status**: Not implemented. @nest term definitions are recognized but nested
properties are not extracted during expansion.

- [x] **A1.1** Implement @nest property extraction in `expandObject`
  - In `expandObject`, during key iteration, when a key's term definition has
    `nestValue`, the key maps to a "nest container". The VALUE of that key is
    a JSON object whose properties should be extracted and processed AS IF
    they were properties of the parent object.
  - Current code at lines 330-348 processes nesting AFTER all keys, but
    doesn't actually extract nested properties into the parent result.
  - Fix: For each key with nestValue, iterate the nested object's keys and
    expand them as if they were sibling properties of the parent.
  - Files: `JsonLdExpansion.qc`
  - Tests: tn001-tn008 (expand), tn001-tn011 (compact), tn001-tn008 (toRdf)

## Priority 2: Type-scoped context propagation (~55 tests)

**Status**: Partially implemented. The previousContext restoration is working
but the type-scoped context application has edge cases.

- [ ] **A2.1** Fix type-scoped context to use the TERM's expanded IRI for lookup
  - When applying type-scoped contexts in expandObject, the code looks up
    `active_context.termDefinitions{type_val}` using the UNEXPANDED type string.
    But the @type value might need expansion first (e.g., "Foo" → "http://example/Foo")
    before checking for a scoped context.
  - File: `JsonLdExpansion.qc:152-175`

- [ ] **A2.2** Type-scoped context should apply to the CURRENT node's properties
  - The type-scoped context applies to properties of the node that has the @type,
    but currently it leaks to nested nodes (partially fixed with previousContext).
    Need to verify the @type check in previousContext restoration works for
    all combinations of type-scoped + property-scoped contexts.
  - File: `JsonLdExpansion.qc:125-147`

- [ ] **A2.3** @propagate: true on type-scoped context should disable reversion
  - When a type-scoped context has `@propagate: true`, the context should
    propagate to nested nodes (don't set previousContext).
  - File: `JsonLdContext.qc` processContextItem

## Priority 3: @reverse property handling (~17 tests)

**Status**: Partially implemented. Reverse properties with containers work
but RDF serialization and compaction have issues.

- [x] **A3.1** Fix @reverse RDF triple generation (subject/object swap)
  - When a node has `@reverse: { "http://example/prop": [{"@id": "other"}] }`,
    the triple should be `<other> <prop> <this_node>`, NOT `<this_node> <prop> <other>`.
  - File: `JsonLdRdf.qc:processNodeToRdf` — the @reverse handling at line 410
    skips @reverse, but should process it with swapped subject/object.
  - Tests: te037, te039, te042, t0031, t0119, etc.

- [x] **A3.2** (partial — node map @reverse swap) Fix @reverse flattening to create separate nodes
  - When flattening, reverse properties should create entries in the node map
    with the subject and object swapped.
  - File: `JsonLdFlattening.qc:generateNodeMap` — step 5.7

## Priority 4: Negative test validations (~57 tests remaining)

- [x] **A4.1** Add processing mode conflict detection
  - When processingMode is "json-ld-1.0" and context has @version: 1.1,
    throw "processing mode conflict".
  - File: `JsonLdContext.qc` processContextItem @version check
  - Tests: tep02 (expand+compact+toRdf = 3 tests)

- [x] **A4.2** Add @type: @none validation in 1.0 mode
  - @type: @none is not valid in JSON-LD 1.0
  - File: `JsonLdContext.qc` createTermDefinition @type processing
  - Tests: ttn01 (expand+compact+toRdf = 3 tests)

- [ ] **A4.3** Add colliding keywords detection
  - When two terms map to the same keyword (e.g., "id": "@id" and "identifier": "@id"),
    throw "colliding keywords" if both are used in the same object.
  - File: `JsonLdExpansion.qc` expandKeywordProperty
  - Tests: ter26 (expand+toRdf = 2 tests)

- [x] **A4.4** (partial) Add invalid reverse property map/value validation
  - When @reverse object contains a value object or list object, throw error.
    When @reverse value is not a map, throw error.
  - File: `JsonLdExpansion.qc` expandKeywordProperty @reverse case
  - Tests: ter25, ter36 (expand+toRdf = 4 tests)

- [x] **A4.5** Add invalid typed value validation
  - When @value has @type and the value is not a scalar/null, throw error.
    Also when @type is @json in 1.0 mode.
  - File: `JsonLdExpansion.qc` normalizeValueObject
  - Tests: ter29, ter40, ter51 (expand+toRdf = 8 tests)

- [ ] **A4.6** Add invalid context entry validation
  - Context entries that are not strings, objects, or null should throw.
  - File: `JsonLdContext.qc` createTermDefinition
  - Tests: ter43, ter44 (expand+toRdf = 4 tests)

- [ ] **A4.7** Add keyword redefinition validation
  - Terms that map to keywords via @id should check for duplicate keyword
    aliases. Also @type redefinition in 1.0 mode.
  - File: `JsonLdContext.qc` createTermDefinition
  - Tests: ter42, ter48 (expand+toRdf = 4 tests)

- [ ] **A4.8** Protected term redefinition detection
  - Property-scoped contexts should NOT be able to redefine protected terms
    UNLESS override_protected is true. Some tests expect the error to be thrown.
  - File: `JsonLdContext.qc` checkProtectedRedefinition
  - Tests: tpr26, tpr28, tpr42 (expand+toRdf = 6 tests)

- [x] **A4.9** Add property-valued index validation
  - When @index is a keyword in a term definition, throw "invalid term definition"
  - File: `JsonLdContext.qc` @index processing
  - Tests: tpi01, tpi03, tpi05 (expand+compact+toRdf = 6 tests)

- [x] **A4.10** Add @included value validation (remaining)
  - @included values that expand to value objects should throw.
  - File: `JsonLdExpansion.qc` @included handler
  - Tests: tin08, tin09 (expand+toRdf = 4 tests)

## Priority 5: [@graph, @index/id] container handling (~30 tests)

**Status**: Partially implemented in expansion. Compaction has basic support
but doesn't handle all combinations.

- [ ] **A5.1** Fix [@graph, @index] expansion for multiple objects per index
  - When index map has arrays as values, each array item should be a separate
    @graph entry, not merged.
  - File: `JsonLdExpansion.qc` expandIndexMap with wrap_graph
  - Tests: t0096, t0097, t0107 (expand)

- [ ] **A5.2** Fix [@graph, @index/id] compaction for complete map structure
  - The @graph container compaction needs to handle @set combinations and
    nested graph objects properly.
  - File: `JsonLdCompaction.qc` @graph container handler
  - Tests: t0080-t0103 (compact)

## Priority 6: Property-valued index (~16 tests)

**Status**: Not implemented. The indexProperty field was added to the term
definition but expansion/compaction don't use it.

- [ ] **A6.1** Use indexProperty in expansion
  - When a term has @index container with indexProperty set, use the property
    value as the index key instead of @index.
  - File: `JsonLdExpansion.qc` expandIndexMap
  - Tests: tpi06-tpi11 (expand+toRdf)

- [ ] **A6.2** Use indexProperty in compaction
  - Extract the index value from the property instead of @index.
  - File: `JsonLdCompaction.qc` @index container handler
  - Tests: tpi01-tpi05 (compact)

## Priority 7: @type in value objects — flatten (~7 tests)

- [x] **A7.1** Don't wrap @type in array for value objects in flatten output
  - The flatten algorithm's normalizeValueObject wraps @type in an array,
    but value objects should have @type as a string, not a list.
  - File: `JsonLdExpansion.qc` normalizeValueObject or `JsonLdFlattening.qc`
  - Tests: t0002, t0007, t0013, t0023, t0028, t0031, t0033 (flatten)

## Priority 8: IRI resolution edge cases (~23 tests)

- [ ] **A8.1** Fix fragment handling in IRI resolution
  - `file#fragment?query=works` should resolve query by stripping fragment first
  - File: `JsonLdUtils.qc` resolveIri
  - Tests: t0062 (expand+toRdf)

- [ ] **A8.2** Fix relative IRI compaction
  - Compaction should produce relative IRIs when possible (e.g., `../parent`)
  - File: `JsonLdIri.qc` compactIri step 10
  - Tests: t0066, t0045, t0076 (compact)

- [ ] **A8.3** Fix @base reset through null context chains
  - When @base is set, then context is nullified, then @base is set again,
    the IRI resolution should use the new @base correctly.
  - File: `JsonLdContext.qc` processContextItem null handler
  - Tests: t0060, t0062 (expand+toRdf)

## Priority 9: fromRdf improvements (~19 tests)

- [x] **A9.1** fromRdf named graph support
  - Named graph quads should create @graph entries in the default graph nodes.
  - File: `JsonLdRdf.qc` fromRdf
  - Tests: t0006, t0007, t0020, t0021, t0022

- [x] **A9.2** fromRdf list detection validation
  - Lists that don't end in rdf:nil should NOT be detected as lists.
  - Lists with cycles should NOT be detected.
  - Lists with multiple rdf:first or rdf:rest should NOT be detected.
  - File: `JsonLdRdf.qc` reconstructLists
  - Tests: t0010, t0011, t0012, t0013, t0014, t0015

- [ ] **A9.3** Duplicate triple removal
  - File: `JsonLdRdf.qc` processQuadsToJsonLd
  - Tests: t0017

## Priority 10: JSON literal canonicalization (~7 tests)

- [x] **A10.1** Sort keys in JSON literal serialization
  - When @type: @json, the JSON value must be serialized with sorted keys
    for canonical form.
  - File: `JsonLdRdf.qc` valueToRdfTerm @json case
  - Tests: tjs08, tjs10, tjs12, tjs13

- [x] **A10.2** Handle JSON null literal
  - @type: @json with null value should produce a valid RDF literal.
  - File: `JsonLdRdf.qc` valueToRdfTerm
  - Tests: tjs17, tjs18, tjs22

## Priority 11: @none alias handling (~10 tests)

- [ ] **A11.1** Resolve @none to absent-key semantics in container maps
  - In language/id/type maps, @none should match values WITHOUT the
    corresponding property.
  - File: `JsonLdExpansion.qc` expandLanguageMap, expandIdMap, expandTypeMap
  - File: `JsonLdCompaction.qc` container map handlers
  - Tests: tm010-tm019

## Priority 12: Nested @list in toRdf (~15 tests)

- [x] **A12.1** Handle @list within @list (nested lists)
  - A list containing a list should produce rdf:first pointing to a list head.
  - File: `JsonLdRdf.qc` listToRdf
  - Tests: tli01-tli14

## Priority 13: Infrastructure — missing test files (~21 tests)

- [ ] **A13.1** Download missing W3C context files
  - Files needed: 0126-context.jsonld, 0127-context-1.jsonld,
    0128-context-1.jsonld, c031/c031-context.jsonld, c034-context.jsonld,
    so05-context.jsonld, so06-context.jsonld, so08-context.jsonld,
    so09-context.jsonld
  - Location: test/w3c-jsonld/expand/ and test/w3c-jsonld/toRdf/

## Impact Estimates

| Priority | Category | Est. Tests Fixed | Effort |
|----------|----------|-----------------|--------|
| 1 | @nest expansion | 24 | 2h |
| 2 | Type-scoped context | 35 | 3h |
| 3 | @reverse RDF/flatten | 17 | 1h |
| 4 | Negative validations | 57 | 2h |
| 5 | @graph containers | 30 | 2h |
| 6 | Property-valued index | 16 | 1.5h |
| 7 | @type in value objects | 7 | 0.5h |
| 8 | IRI resolution | 15 | 1.5h |
| 9 | fromRdf improvements | 19 | 1.5h |
| 10 | JSON literals | 7 | 0.5h |
| 11 | @none handling | 10 | 1h |
| 12 | Nested @list | 15 | 1h |
| 13 | Infrastructure | 21 | 0.5h |
| | **Total** | **~273** | **~18h** |

Fixing all items would bring conformance from 774 to ~1047 (89.7%).
