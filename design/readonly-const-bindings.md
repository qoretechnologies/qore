# Readonly `const` Bindings

This document records the stable architecture for Qore's context-sensitive
`const` syntax. It is a development reference for parser, runtime, IR, AOT, and
tooling work; user-facing syntax and examples live in `doxygen/lang/`.

## Context Semantics

`const` is resolved by grammar context:

| Context | Meaning |
| --- | --- |
| Namespace top level, including nested namespaces | parse-time constant |
| Class declaration scope | parse-time class constant |
| Statement/local declaration scope | runtime read-only local binding |
| Parameter lists | runtime read-only parameter binding |

The two meanings are intentionally separate. A namespace/class constant is a
parse-time constant expression and may have an explicit declared type:

```qore
const int Limit = 100;
public const string Name = "orders";
```

A statement-context declaration is a runtime local variable whose binding is
initialized once and then protected from writes:

```qore
const int count = values.size();
const auto config = getConfig();
```

Top-level `const` outside an explicit statement block remains a parse-time
constant declaration. Runtime read-only semantics at top level require an
explicit statement block.

## Readonly Binding Model

Readonly is a property of a local binding, not transitive immutability of the
value. The protected binding cannot be reassigned, removed, incremented,
compound-assigned, used as the base of an index/member lvalue write, or exposed
as a mutable reference. In-place mutating operators such as `push`, `pop`,
`shift`, `unshift`, `splice`, `extract`, `trim`, `chomp`, regex substitution,
and transliteration are rejected for the same reason: they acquire the binding
as a writable lvalue. Objects and containers reachable through other mutable
aliases keep their normal mutability.

Readonly locals require an initializer. Qore does not implement
definite-assignment-once analysis for declarations such as:

```qore
const int x;
x = 1;
```

Under `%require-types`, including `%modern`, bare inferred declarations are
rejected:

```qore
const value = expr;       # rejected under require-types
const auto value = expr;  # explicit typed syntax
```

Comma-separated declarations follow Qore's repeated-type declaration model. The
`const` marker belongs to the binding declaration that carries it:

```qore
const int a = 1, const int b = 2;
const int c = 3, int d = 4;
```

`const int a = 1, b = 2` is not introduced by this feature.

## Parameters

Readonly parameter markers are callee-body constraints:

```qore
sub f(const int id, const *string name = NOTHING) {
}
```

They do not change the call ABI, overload identity, default argument
evaluation, type filtering, or abstract override compatibility. The following
declarations have the same overload signature:

```qore
sub f(int x) {}
sub f(const int x) {}  # duplicate signature
```

Reflection, documentation, AST output, and serialized metadata preserve the
marker even though overload resolution ignores it.

## References

Mutable references are the primary bypass risk. The architecture rejects:

- Creating `reference<T>` or `*reference<T>` to a readonly binding.
- Passing a readonly binding where a mutable reference parameter is required.
- Using a readonly binding whose value is a reference as an update lvalue.

Static cases are parse errors. Runtime-only escape paths raise
`RUNTIME-READONLY-VIOLATION`.

## Enforcement Invariant

The primary invariant is:

> After declaration initialization, a readonly local must never be the write
> target of any AST, IR, JIT, AOT, lvalue helper, closure, or reference path.

Parse-time lvalue validation rejects syntactic writes rooted at readonly
locals, including assignment, compound assignment, increment/decrement,
`remove`, `delete`, indexed writes, member writes, and mutable reference
creation.

Runtime guards remain necessary for paths that can bypass parse-time proof,
including closure variable lvalues, local/closure removal, and indirect
reference/reflection-style writes.

Compiled fast paths do not need a per-store readonly branch when they are fed
verified IR. Instead:

- IR lowering must not emit ordinary store/update instructions for readonly
  locals after declaration initialization.
- The IR verifier rejects readonly locals used as write targets by `StoreLocal`,
  `StoreClosure`, fused local update instructions, list/hash index stores rooted
  at that local, and lvalue-path store/remove/delete records rooted at that
  local.
- AOT loading applies the same invariant after local metadata is reconstructed,
  so stale or hand-built artifacts cannot bypass parser checks.

## Metadata and Serialization

Readonly state is part of local and parameter metadata:

- `LocalVar` carries the read-only bit for local and parameter bindings.
- Closure variable values carry their own read-only bit because some runtime
  paths operate directly on closure values rather than through the owning
  `LocalVar`.
- Function signatures preserve per-parameter readonly markers for reflection,
  documentation, AST output, and AOT.
- AOT local slot/signature serialization is feature-gated by
  `QORE_AOT_FEAT_READONLY_LOCALS`.

Any new local-introduction or code-generation path must preserve this metadata
and must either emit an initialization-only operation or reject later writes.

Compatibility behavior follows the AOT feature flag:

- New artifacts loaded by a readonly-aware runtime restore and verify readonly
  state.
- Old artifacts loaded by a readonly-aware runtime have no readonly local bits
  because an old compiler could not emit this metadata.
- New artifacts loaded by an old runtime must be rejected through unknown AOT
  feature flags.
- A readonly-aware artifact must set `QORE_AOT_FEAT_READONLY_LOCALS`; the loader
  cannot infer missing readonly metadata from instruction bodies alone.

## Parse Options

`%no-constant-defs` applies only to parse-time constant definitions. Runtime
readonly locals and readonly parameters remain valid because they are local
bindings, not constants.

`%require-types` applies to readonly local declarations like ordinary locals.
Use `const auto name = expr` when the initializer should determine the value
type.

## Error Classes

- `READONLY-VARIABLE-ASSIGNMENT-ERROR`: parse-time rejection for statically
  detectable writes or mutable reference creation rooted at a readonly binding.
- `RUNTIME-READONLY-VIOLATION`: runtime rejection for paths that escape
  parse-time analysis.

Existing constant-related errors remain reserved for parse-time constants.

## Tooling

Parser and formatting tools must preserve:

- Explicit types on parse-time constants.
- `const` markers on local declarations.
- `const` markers on parameter declarations.

The Bison parser and the tree-sitter based astparser grammar must stay aligned.
QoreCodeFormat must round-trip all accepted syntax idempotently.

## Out of Scope

Readonly local bindings are not receiver constness. `const` methods, const
method overload resolution, const-aware object/reference types, and deep
container/object immutability require separate designs.

Readonly `foreach` and `catch` variable declarations are also separate
extensions unless explicitly enabled in the parser and backed by verifier/AOT
rules that distinguish initialization/rebinding from ordinary writes.
