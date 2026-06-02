# Parser lvalue-type propagation for map + hash-literal narrowing

## Status: **Implemented** (Qore commit `72b91b78c`, Qorus `abe83195d`)

Workflow.qc casts removed.  AOTSmoke 93/93, JITSmoke 156/156, all 15
operator tests green, new `lvalue-type-hint.qtest` (6 cases) green.

qwf --help runs clean against current libqore without the `cast<>`
workaround.

## Motivation

`Workflow.qc:170-173` currently needs an explicit `cast<>` workaround:

```qore
*hash<string, softint> mh;  // inherited member
mh = cast<hash<string, softint>>(map {$1.name: $1.mapperid}, mappers.iterator());
```

Without the cast, `parseCommit` raises a PARSE-TYPE-ERROR even though the
runtime values always satisfy `hash<string, softint>`.  The same
workaround pattern appears in `ServiceTemplateManager.qc` and is a
recurring footgun every time a developer narrows a `map` result into a
typed-hash lvalue.

## Root cause

Found by inspection of parser internals:

1. **No downward type hint from lvalue to rvalue.**
   `QoreAssignmentOperatorNode::parseInitIntern`
   (`lib/QoreAssignmentOperatorNode.cpp:40-210`) extracts `ti` from the
   lvalue on line 54 but then calls `parse_init_value(right,
   parse_context)` with `parse_context.typeInfo = nullptr` on line 68 —
   the rvalue sub-expression has no idea what type it's being assigned
   into.  Type-check happens post-hoc via `parseAccepts()` on line 138,
   too late to influence inference.

2. **Map operator locks its return type to the body's inferred type.**
   `QoreHashMapOperatorNode::parseInitImpl`
   (`lib/QoreHashMapOperatorNode.cpp:75-108`) captures
   `parse_context.typeInfo` from the value expression on line 103 and
   calls `setReturnTypeInfo()`.  When the body is
   `{$1.name: $1.mapperid}` and `$1` is `auto` (AbstractIterator's
   element type), both key and value are `auto`, the hash literal
   resolves to `autoHashTypeInfo`, and the map returns plain
   `hashTypeInfo` (line 64).

3. **Hash literal also drops to auto when members are auto.**
   `QoreParseHashNode::parseInitImpl`
   (`lib/QoreParseHashNode.cpp:46-163`) line 127 forces
   `typeInfo = autoHashTypeInfo` when any value is auto.  There's no
   channel for upward context to tell it "the caller expects
   `softint` values".

4. **AbstractIterator is untyped.** `Pseudo_QC_Hash.qpp::iterator()`
   returns declared type `AbstractIterator` (the base), not a
   parameterised subclass.  `getValue()` returns bare `auto`, so
   `$1.field` is always `auto` downstream of any iterator call.

Fixing (4) is a language-level redesign (parameterised iterators) — out
of scope.  Fixing (1)–(3) is a contained parser change: make the
expected lvalue type visible to map + hash-literal parsing so it can
guide narrowing when the body would otherwise lock to auto.

## Design

Thread an **optional, non-binding** `expected_type_info` through
`QoreParseContext`.  Semantics:

- `nullptr` (default) → existing behaviour, no narrowing hint.
- non-null → "the caller would prefer values of this type; use it to
  disambiguate when inference would otherwise default to auto".
- Parse nodes that understand the hint apply it; others ignore it.
- Never *overrides* explicit types — if the body expression has a
  concrete non-auto type, that wins.
- Type-compatibility is re-checked at the assignment site as today, so
  the hint can only tighten inference, not sneak incompatible types
  through.

### Phase 1 — extend `QoreParseContext`

`include/qore/intern/QoreLibIntern.h`:

```cpp
struct QoreParseContext {
    // ... existing fields ...
    //! Optional hint: when the surrounding context (assignment lvalue,
    //! function parameter, return statement) has a concrete declared
    //! type and the rvalue's own inference would otherwise land on
    //! `auto`, sub-expressions that understand the hint narrow to match.
    //! NEVER mandatory — every consumer falls back to its own inference.
    const QoreTypeInfo* expected_type_info = nullptr;
};
```

### Phase 2 — wire through assignment

`lib/QoreAssignmentOperatorNode.cpp` around line 60 (between lvalue
init and rvalue init):

```cpp
const QoreTypeInfo* ti = ...;  // lvalue type (already computed)

QoreParseContext rctx(parse_context);
rctx.expected_type_info = ti;
parse_init_value(right, rctx);
```

Post-init path unchanged.  The existing `parseAccepts()` check on
line 138 stays as the authoritative compatibility gate.

Also wire in:
- Function call argument binding (`FunctionCallNode::parseArgs` — each
  argument gets `rctx.expected_type_info = param_type_info`)
- Return statement (`ReturnStatement::parseInit` — expected = declared
  return type)
- Foreach list/hash init
- Explicit variable declaration with initializer
  (`VarRefDeclNode` init path)

Keep the wiring uniform — one-line pattern at each site.

### Phase 3 — consume in map operator

`lib/QoreHashMapOperatorNode.cpp::parseInitImpl`:

When `parse_context.expected_type_info` is a `hash<K, V>`:

1. Before parsing the body value expression, set the body's
   `parse_context.expected_type_info = V`.
2. Before parsing the body key expression, set
   `parse_context.expected_type_info = K`.  (Less important — keys
   usually resolve via field access paths, but does no harm.)
3. After parsing, if `expTypeInfo2` is auto and V is concrete, adopt V
   as the value type when calling `setReturnTypeInfo`.

### Phase 4 — consume in hash literal

`lib/QoreParseHashNode.cpp::parseInitImpl`:

When `parse_context.expected_type_info` is `hash<K, V>` and the value
expressions resolve to auto, force `typeInfo = parse_context.expected_type_info`
instead of `autoHashTypeInfo` at line 127.  Runtime softening
(`softint`, `softstring`, etc.) is already handled by the value-type's
coercion on store.

### Phase 5 — map-operator runtime coercion

For the `hash<auto> → hash<string, softint>` case, the keys/values *at
runtime* are strings and int-like values that already satisfy the
softened target.  The existing `QoreHashNode` store path applies per-
value softening when the hash has a complex type.  So as long as the
parser now narrows the map's return type to `hash<string, softint>`,
the stored hash will build elements through the softening path — no
new runtime code needed.

A spot-check needed on line 57 of `QoreHashMapOperatorNode.cpp`:
`qore_get_complex_hash_type(expTypeInfo2)` uses ONLY value type.  For
`hash<K, V>`, the key-type narrowing pathway needs `qore_get_complex_hash_type`
to be extended to accept both, or we fall back to the value-only form
and live with `hash<auto, softint>` — still narrower than plain
`hash<auto>` and still assignment-compatible with `hash<string, softint>`
at the lvalue's parseAccepts() gate.

## Test plan

1. **New unit tests** in `examples/test/qore/typing/lvalue-hint.qtest`
   (new file):
   - `hash<string, softint> h = map {$1.name: $1.id}, list_of_hashes.iterator();`
     — must parse without cast.
   - `hash<string, softint> h = {"a": "5"};` — string-to-softint
     softening.
   - Negative: `hash<string, int> h = map {$1.name: "text"}, ...;`
     where body value is concretely `string` and target is strict
     `int` — must still error (hint doesn't override concrete
     incompatible types).
   - Nested: `list<hash<string, softint>> ll = map ...;` — outer
     list's value type propagates one level deep.
   - Hint fallback: map returns its own inferred concrete type when
     it has one; hint applies only when inference would otherwise
     default to auto.

2. **Regression baseline** on AOTSmoke (93) + JITSmoke (156).

3. **Workflow.qc cleanup**: remove the two `cast<>` calls at lines
   170 + 173, rebuild Qorus qwf .qo, verify `qwf --help` runs clean.

4. **ServiceTemplateManager.qc** same pattern cleanup if test passes.

5. **Wider sweep** after landing: `grep -rn 'cast<hash<' qorus/` to
   see other potential removals — not required for this change, but
   catalogue the scope of the payoff.

## Acceptance criteria

- Workflow.qc:170 and 173 compile cleanly without `cast<>`.
- ServiceTemplateManager.qc tactical workaround can be removed.
- All baselines green on the Qore repo.
- No regression in Qorus-side .qmod compile times (format doesn't
  change; only parser inference logic).

## Out of scope

- Parameterised iterators (`AbstractIterator<T>`) — the right long-term
  fix for this whole class of narrowing gaps, but touches thousands of
  callsites.  Document as follow-up in the Qore language backlog.
- Implicit narrowing `hash<auto> → hash<K, V>` at assignment (without
  map/hash-literal involvement) — a separate relaxation that would
  need its own design because it changes runtime type-check semantics
  at every assignment site, not just the parse-time inference path.

## Risk

- **Low, opt-in**.  Every consumer of `expected_type_info` checks for
  `nullptr` and falls back to existing behaviour.  Unhinted paths
  (every call site that doesn't set the hint) see zero change.
- The hint is a **preference, not a mandate** — `parseAccepts()` at
  the assignment site remains the authoritative compatibility gate,
  so the worst a buggy hint consumer can do is leave inference as it
  was today.

## Estimate

- 1 day: implementation + wiring at all 5 call sites
- 0.5 day: unit tests
- 0.5 day: Qorus-side cleanup + verification
- Total: ~2 engineer-days.
