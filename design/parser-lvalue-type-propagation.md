# Parser lvalue type propagation

## Status: Implemented

Landed 2026-04-20 across `72b91b78c` (the hint channel), `8070e5a64` (AST/IR
hash-literal alignment), and `282da15f1` + `3b70e959d` (iterator element type
reaching `$1`, and the fold-pattern guard that change exposed).

Tests: `examples/test/qore/operators/lvalue-type-hint.qtest`.

## The problem

Narrowing a `map` result into a typed-hash lvalue used to require an explicit
`cast<>`:

```qore
hash<string, softint> h = cast<hash<string, softint>>(map {$1.name: $1.id}, src.iterator());
```

Without the cast, `parseCommit` raised a `PARSE-TYPE-ERROR` even though the
runtime values always satisfied the target type. Four independent causes:

1. **No downward type hint from lvalue to rvalue.**
   `QoreAssignmentOperatorNode::parseInitIntern` extracted the lvalue type but
   then parse-initialized the rvalue with no expected type at all. The
   compatibility check happened afterwards via `parseAccepts()` — too late to
   influence inference.
2. **The map operator locked its return type to the body's inferred type.**
   With `$1` at `auto`, both key and value expressions were `auto`, so the hash
   literal resolved to `autoHashTypeInfo` and the map returned plain
   `hashTypeInfo`.
3. **A hash literal dropped to auto whenever any member was auto**, with no
   channel for surrounding context to say "the caller expects `softint` values".
4. **`AbstractIterator` is untyped.** `iterator()` is declared to return the base
   class and `getValue()` returns bare `auto`, so `$1.field` was `auto`
   downstream of any iterator call.

Cause 4 is a language-level redesign — parameterized iterators — and remains out
of scope. Causes 1-3 were fixed by the two channels below.

## Channel 1: `expected_type_info` — a declared target reaching an rvalue

`QoreParseContext::expected_type_info` (`include/qore/intern/QoreLibIntern.h`)
carries an **optional, non-binding** hint downward into sub-expression parsing.

The contract, which every producer and consumer must preserve:

- `nullptr` (the default) means existing behaviour — no narrowing.
- A non-null hint means *"the surrounding context would prefer this type; use it
  to disambiguate when inference would otherwise land on `auto`"*.
- A node that does not understand the hint ignores it.
- The hint **never overrides a concrete type**. If the sub-expression infers a
  concrete non-auto type, that wins.
- `QoreTypeInfo::parseAccepts()` at the assignment site remains the
  authoritative compatibility gate. The hint can only tighten inference, never
  admit an incompatible type.

That last point is what makes the whole mechanism low-risk: the worst a buggy
consumer can do is leave inference exactly as it was.

**Producers** — sites that set a hint:

| Site | Hint |
|---|---|
| `QoreAssignmentOperatorNode.cpp` | the lvalue's declared type |
| `TypedHashDecl.cpp` | the hashdecl member's declared type |
| `ConstantList.cpp` | the constant's declared type |
| `FunctionCallNode.cpp` → `Function.cpp` | expected receiver type, for static factory / generic class type-argument inference |

**Consumers** — sites that narrow on it:

| Site | Behaviour |
|---|---|
| `QoreParseHashNode.cpp` | when the hint is `hash<K, V>` and member values would resolve to auto, adopt the hint instead of `autoHashTypeInfo`; the value type is pushed into each member expression and cleared for keys |
| `QoreHashMapOperatorNode.cpp` | pushes the hint's value type into the body value expression, and adopts it for `setReturnTypeInfo()` when the body lands on auto |
| `QoreHashMapSelectOperatorNode.cpp` | the same, for the select variant |

**Every producer saves and restores the previous value** around the nested parse
rather than assigning and leaving it. `QoreParseContext` is reused across sibling
sub-expressions, so a leaked hint would narrow an unrelated expression. Likewise
consumers explicitly set the field to `nullptr` before parsing sub-expressions
the hint must not reach — key expressions, iterator sources — instead of relying
on those nodes to ignore it.

No runtime work was needed for the softening case: once the parser narrows the
map's return type to `hash<string, softint>`, the existing `QoreHashNode` store
path applies per-value softening because the hash carries a complex type.

### What was proposed but did not ship

The original design also proposed wiring the hint into function-call argument
binding (per-parameter types), `ReturnStatement`, foreach initialization, and
declaration-with-initializer. Those are **not** wired. `FunctionCallNode` passes
the hint through for *receiver* inference only. Anyone extending the channel
should treat those as open sites, not as existing behaviour.

## Channel 2: iterator element type reaching `$1`

Narrowing the map's *return* type does not help when the problem is inside the
body — `$1.field` where the field access itself must resolve, a nested map, or
foreach destructuring. That is a separate channel:
`QoreTypeInfo::getImplicitArgTypeForIterator()` computes the element type of the
iterator source, and `ParseImplicitArgTypeHelper` installs it as the implicit-arg
type for `$1` during body parsing.

Unlike channel 1, this one is wired into every iterating operator:
`QoreMapOperatorNode`, `QoreHashMapOperatorNode`, `QoreMapSelectOperatorNode`,
`QoreSelectOperatorNode` and `QoreFoldlOperatorNode`.

The two channels are independent and compose. The list-producing `map` uses only
channel 2; the hash-producing `map` uses both.

### The fold-pattern hazard

Narrowing `$1` had a non-obvious consequence in IR lowering, and it is the reason
`analyzeFoldPattern()` in `lib/QoreIRLowering.cpp` takes a `list_type` argument.

The specialized fold opcodes (`FoldlSumInt`, `FoldlDiffInt`, `FoldlMinInt`, …)
iterate a **list** in one shot from a runtime helper. `analyzeFoldPattern()`
matched them from the *body expression shape* alone. Once `$1` narrowed to `int`
for a `list.iterator()` source, a body of `$1 - $2` started matching
`FoldlDiffInt` — but the source object was an `AbstractIterator`, not a list, so
the specialized opcode misread its input and produced garbage:
`foldr $1 - $2, (2,3,4).iterator()` returned `0` instead of `-1`.

The guard only allows specialization when `list_type` actually parse-returns
`NT_LIST`; anything else falls through to native lowering, which handles
iterators with a proper loop.

**The general lesson:** a pattern match on expression shape must also verify the
*source* shape it implicitly assumes. Improving type inference can newly satisfy
a pattern that was previously unreachable, so a specialization guarded only by
body shape is a latent bug waiting for the inference to get better.

## Alignment across execution modes

This is a parse-time change: it sets `typeInfo` and implicit-arg types on AST
nodes, and AST, IR and AOT all read them through the same
`ParseImplicitArgTypeHelper` machinery, so no mode-specific divergence is
introduced. `8070e5a64` aligned the AST hash-literal `evalImpl` with the IR-side
narrowing so that a narrower `$1` surfacing through a hash literal inside a body
is honoured identically under AST evaluation.

## Out of scope

- **Parameterized iterators** (`AbstractIterator<T>`) — the right long-term fix
  for this whole class of narrowing gaps, but it touches thousands of call sites.
- **Implicit `hash<auto>` → `hash<K, V>` narrowing at assignment**, without a map
  or hash literal involved. That changes runtime type-check semantics at every
  assignment site rather than only parse-time inference, and needs its own
  design.
