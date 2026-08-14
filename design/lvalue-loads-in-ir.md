# Lvalue Loads in Qore IR

## Status

This document defines the current IR/JIT/AOT lvalue invariant. It is referenced
from the interpreter and lowering code because violations produce lost
mutations, leaks, or incorrect copy-on-write behavior.

## Core Invariant

Lvalue operations must see the variable's natural refcount at the point where
copy-on-write is evaluated.

Therefore, any IR operation that reads a container for in-place mutation must
avoid creating an extra owned reference before the mutation helper checks
`is_unique()`.

## Borrowed Load Rule

Container mutation lowering must use borrowed local loads:

```text
LoadLocal(container, auto_ref=false)
```

This applies to direct mutation opcodes and to root values for lvalue paths.
The borrowed value must not be placed in owned cleanup lists.

## Paired Local Mutation Rule

An ordinary local can remain runtime-backed because another statement in the
same function uses indexed or structured lvalue access. A native
`LoadLocal` → mutation → `StoreLocal` sequence must not become quadratic merely
because that classification prevents the stronger fresh-local optimization.

For an uninterrupted paired local mutation, analysis marks the load, mutation,
and store as one guarded local-COW operation. Immediately before the runtime
uniqueness check, the interpreter and LLVM lowering remove only compiler-owned
slot-cache and reload references. The runtime local and any semantic aliases
remain owners, so the helper mutates a truly unique value in place or creates a
replacement when a real alias exists. A replacement is always stored back to a
runtime-backed local.

The guarded marker remains valid across AOT function outlining because helper
cache references are cleared before the check. Stronger in-place and redundant
store markers are removed when outlining changes local ownership.

List `push` also preserves the AST's exception-visible auto-vivification order:
when a typed local is `NOTHING`, lowering stores the correctly typed empty list
before validating the pushed element. A caught element-type error therefore
leaves an empty list in the local, and any tentative helper-owned COW result is
released on failure.

## Shared-Local Mutation Rule

Captured, closure-bound, and thread-safe locals must use the structured lvalue
path for container mutations. The path acquires the variable lock before it
evaluates copy-on-write, so compiler bookkeeping references are removed while
the mutation is serialized. Direct hash-store and list-push fast paths are only
valid for ordinary, non-reference locals.

For a structured mutation that throws, the interpreter must release the
`LValueHelper` (and therefore its lock) before cleaning up values or transferring
control to the instruction's exception target.

## Cache Invalidation Rule

Before any lvalue mutation that can write through `LValueHelper`, the
interpreter must invalidate cached local values for the affected variable:

- `locals` map cache.
- `locals_slot_cache`.
- value slots associated with prior loads of the same local.
- closure-cache entries when the lvalue root is a closure variable.

Invalidating after mutation is too late: the cache may already have inflated
the refcount and forced COW to mutate a temporary copy.

## Current Mutation Families

The rule applies to:

- Hash and list direct stores.
- Compound assignment lvalue opcodes.
- Pre/post increment and decrement.
- `push`, `pop`, `shift`, `unshift`, `splice`, `trim`, `chomp`, `remove`,
  `delete`, regex substitution, transliteration, and extract mutation paths.
- Lvalue path assignment, compound assignment, unary mutation, slice mutation,
  and pattern-based mutation.
- Invoke instructions whose embedded operation is an lvalue mutation.

## Interpreter Responsibilities

When handling a borrowed lvalue load:

- Do not add the loaded value slot to owned cleanup.
- Do not add the loaded value slot to `local_load_slots`.
- Invalidate caches before entering `LValueHelper` or AST-delegated lvalue
  evaluation.
- If COW creates a replacement container, write it back to the runtime local and
  update subsequent IR-visible values to the replacement.

## JIT and AOT Responsibilities

Native mutation helpers must:

- Receive enough local-slot identity to write COW replacements back to the
  runtime local.
- Return or publish the updated container when subsequent IR instructions can
  observe it.
- Use JIT and AOT variants where process-local pointers must be replaced by
  `QoreAOTContext` slot indices.

After a helper that can replace the root container, LLVM lowering must reload or
refresh the cached root value before later uses.

## Parent Handler Interaction

Deferred handler IR can mutate parent locals. Native/JIT/AOT parents install an
exact parent slot cache before handler execution, track dirty slots, and publish
dirty values back to runtime locals afterward. This prevents name-based lookup
collisions and keeps native alloca caches coherent with handler writes.

## Review Checklist

When adding a new lvalue opcode or helper:

- The lowering path uses borrowed loads for mutation roots.
- Paired local mutations clear only compiler-owned references before COW.
- Shared local roots use lock-held structured lvalue navigation.
- Interpreter cleanup never owns borrowed roots.
- Exception edges release lvalue locks before cleanup or catch transfer.
- Caches are invalidated before mutation.
- COW replacement writes back to the runtime local.
- JIT and AOT variants preserve the same writeback behavior.
- Tests cover unique and shared-container mutation.
