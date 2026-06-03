# Readonly `const` Binding Follow-Ups

Status: pending follow-up for generally implemented readonly const bindings

## Scope

Readonly `const` bindings are generally implemented. This document captures
pending clarifications, verification work, and edge-case hardening that should
not live in the stable design document until they are audited against the
implementation.

The implemented const-method design is documented in `design/const-methods.md`.
That design owns receiver constness, const method declarations, readonly
receiver call restrictions, and mutation checks rooted at `self`.

## Overlap With Const Methods

Dependency direction:

- `design/const-methods.md` depends on the implemented readonly-binding
  baseline to identify readonly local/closure object receivers.
- This follow-up document does not depend on const methods; it is binding-level
  cleanup and hardening for an already implemented feature.
- Const methods have now been implemented. Pull from this follow-up only where
  shared infrastructure still needs hardening, especially lvalue-root analysis,
  reference bypass handling, IR/AOT verification, and cross-mode tests.

Do not duplicate receiver-constness semantics here. Earlier readonly-binding
notes included an example where a readonly object binding could call a mutating
method because the binding itself was only read:

```qore
const MyClass obj = new MyClass();
obj.update();
```

That behavior is exactly the boundary changed by `design/const-methods.md`: readonly
object receivers become useful by allowing calls only to const methods when the
target is statically resolved. Keep this topic in the const-methods design.

This follow-up document is limited to binding-level readonly behavior:
declaration syntax, lvalue rejection, reference bypasses, metadata, AOT/IR
verification, and tests.

Shared infrastructure that both documents may touch:

- Parse-analysis helpers that determine whether an expression root is readonly.
- Lvalue validation for assignment, removal, mutation, and reference creation.
- IR verifier checks for stores and lvalue paths rooted at readonly values.
- AOT metadata and load-time verification for readonly-related flags.
- astparser, QoreCodeFormat, reflection, documentation, and execution-mode
  tests.

## Implementation Checklist

For remaining readonly-binding cleanup, verify the implementation in dependency
order:

1. Bison parser grammar and parse-time declaration metadata.
2. astparser grammar and CST/AST classification.
3. Runtime lvalue and removal guards for local and closure bindings.
4. Mutable-reference creation and reference-parameter rejection.
5. IR lowering and IR verifier invariants.
6. JIT/AOT execution paths using verified IR.
7. AOT signature, local-slot, and instruction metadata serialization.
8. Reflection, documentation, and formatting/tooling preservation.
9. Cross-mode tests covering AST execution, IR interpreter, JIT, and AOT.

## Unified Execution Plan

Execute this work together with the builtin const-method annotation audit using
the unified ordering in
[`const-followup-unified-plan.md`](const-followup-unified-plan.md). The
readonly-binding phases are the foundation and must complete before expanding
builtin const annotations beyond inventory/audit work.

## Context And Grammar Hardening

Readonly `foreach` and `catch` variable declarations are not part of the
implemented readonly-binding feature unless they have explicit parser,
verifier, and AOT rules that distinguish initialization/rebinding from ordinary
writes. The parser should reject `foreach const ...` and `catch (const ...)`
forms until that separate design exists.

The `const` split should remain structural:

- Namespace and class-scope `const` is parsed as a parse-time constant.
- Statement/lvalue-scope `const` is parsed as a local declaration and marks the
  resulting `VarRefDeclNode` readonly.
- astparser must mirror the same split.

Global variables declared with `our` are out of scope. `our const` must not
become a readonly-binding form unless separately designed. Keep readonly const
bindings distinct from the existing read-only imported global variable feature
on `GlobalVariableList::import` / `Program::importGlobalVariable`.

## Binding Semantics Clarifications

The governing rule should be stated explicitly in docs and diagnostics:

> Any operation that acquires the binding as an lvalue is rejected after
> declaration initialization, whether it rebinds the slot or mutates the
> contained value in place.

This covers direct assignment, compound assignment, increment/decrement,
`remove`, `delete`, index/member lvalue writes, mutable reference creation, and
in-place value-mutating operators such as `push`, `pop`, `shift`, `unshift`,
`splice`, `extract`, `chomp`, `trim`, shift-equals, and regex
substitution/transliteration.

The binding is readonly, not deeply immutable. Containers and objects reachable
through other mutable aliases keep their normal mutability. Positive tests
should document aliasing boundaries so the behavior does not drift.

Readonly applies within a single binding lifetime. Re-running a readonly local
initializer on each function call or loop-body entry is initialization, not a
write. The `initial_assignment` marker must remain the boundary between allowed
initialization and rejected later stores.

Closure capture must not bypass readonly. A captured readonly local should
remain readonly inside the closure body, and closure variable values need their
own readonly bit for runtime paths that operate directly on closure values.

## Comma Declarations

Comma-separated declarations should keep Qore's repeated-type declaration model:

```qore
const int a = 1, const int b = 2;  # both readonly
const int c = 3, int d = 4;        # only c readonly
```

The ambiguous form should be rejected rather than silently changing meaning:

```qore
const int a = 1, b = 2;  # reject
```

A second declarator must repeat the full declaration form.

## References

Mutable references are the main bypass risk. Recheck these cases:

- Creating a mutable parse reference to a readonly binding, such as `\x`, where
  `x` is readonly.
- Creating `reference<T>` or `*reference<T>` values rooted at a readonly binding.
- Passing a readonly binding where a `reference<T>` or `*reference<T>`
  parameter can write back to the argument.
- Using a readonly binding whose value type is itself `reference<T>` or
  `*reference<T>` as an update lvalue.

Static cases should be parse errors. Runtime-only escape paths should raise
`RUNTIME-READONLY-VIOLATION`.

## IR And AOT Verification

The IR verifier should reject reference-creation paths rooted at a readonly
local, including `CreateParseRef` and any reference-foreach setup that carries a
parse-reference expression.

AOT serialization must preserve readonly and `initial_assignment` bits when
`QORE_AOT_FEAT_READONLY_LOCALS` is present. Cross-version behavior should be
audited as follows:

- New artifact loaded by a new runtime: readonly state is restored and verified.
- Old artifact loaded by a new runtime: no readonly bits are read because the
  old compiler could not emit readonly const-local IR.
- New artifact loaded by an old runtime: unknown feature flags must cause clean
  rejection.

Artifacts produced by a readonly-aware compiler must set
`QORE_AOT_FEAT_READONLY_LOCALS`. A new-runtime loader cannot infer readonly
state from a new artifact that omits the flag. If version/header metadata makes
that detectable, reject the artifact as malformed; if the artifact is genuinely
old, accept it under the old-code rule.

## Parse Options

`%require-types` applies to readonly local declarations like ordinary locals.
A bare inferred `const name = expr` is rejected under `%require-types`; use
`const auto name = expr` when the initializer should determine the value type.

Readonly parameter markers are orthogonal to `%require-types`: the marker
constrains the callee body and does not relax or tighten the parameter type
requirement.

`%allow-bare-refs` / old-style `$` syntax should follow ordinary local and
parameter declaration rules. Under bare-reference mode, `const int name = expr`
is the local binding form and `$name` declarations are rejected. Without
bare-reference mode, `const int $name = expr` is the local binding form and
bare local declarations are rejected. The readonly marker should not change the
existing `$` requirement.

## Compatibility Checks

Before promoting any of these clarifications into the stable design, run a
corpus parse/format check over `qlib/`, `examples/test/`, and representative
modules. Confirm existing namespace/class-scope constants are still classified
as parse-time constants and no statement-scope syntax is reclassified
unexpectedly.

## Error Classes

`READONLY-VARIABLE-ASSIGNMENT-ERROR` is a parse-time error class for all
statically detectable writes rooted at a readonly binding. Despite the word
`ASSIGNMENT`, it also covers missing initializers, `delete`/`remove`, in-place
mutating operators, and mutable reference creation.

`RUNTIME-READONLY-VIOLATION` is for runtime escape paths such as closure-variable
lvalues, local/closure removal, and indirect reference/reflection-style writes.

Keep existing constant-related errors reserved for parse-time constants.

## Tests

The negative matrix should include one parse-time case per write form:
assignment, compound assignment, pre/post increment/decrement, `delete`,
`remove`, index write, member write, every in-place mutating operator, mutable
`reference` / `*reference` creation, and a readonly declaration with no
initializer.

Runtime escape-path tests should cover closure-variable lvalue writes, local and
closure removal, and indirect reference/reflection-style writes that bypass
parse-time proof.

IR verifier tests should cover `StoreLocal`, `StoreClosure`, fused local
updates, index stores, lvalue-path store/remove/delete records,
`CreateParseRef`, and reference-foreach setup rooted at a readonly local.

AOT tests should cover readonly local and parameter round-trip, injected-write
verifier rejection after reload, unknown feature flag rejection by old runtimes,
and detectable readonly-aware artifacts missing `QORE_AOT_FEAT_READONLY_LOCALS`.

Positive tests should cover re-initialization on each loop iteration and call,
closure capture preserving readonly, read/pass-by-value use, and mutable alias
behavior.

Every bypass-sensitive negative test should run in each applicable execution
mode: AST execution, IR interpreter, JIT, and AOT. If a construct cannot reach a
mode because it is rejected earlier, assert the earlier phase and document why
no later-mode execution is possible.
