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
- Interpreter cleanup never owns borrowed roots.
- Caches are invalidated before mutation.
- COW replacement writes back to the runtime local.
- JIT and AOT variants preserve the same writeback behavior.
- Tests cover unique and shared-container mutation.
