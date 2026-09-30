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

## Operand Temp Scope Rule

The value operand of an assignment to a container path is evaluated before the mutation, and its evaluation can create temps that refer to the very container being changed: in
`h.x.y = h.x.a + 1` the operand loads `h` and projects `h.x`.  A statement's temps are released by its
`DiscardTemps`, after the mutation, so these temps would make `h` and `h.x` shared when copy-on-write is
evaluated, and the mutation would copy them.  The AST releases them when the operand's evaluation returns.

`QoreIRLowering::lowerMutationOperand()` therefore lowers such an operand in a temp scope of its own and
closes it with `DiscardTempsKeep`, right before the mutation.  The same applies to every operand lowered
before a mutation: the value of an assignment, compound assignment, `push`, or `unshift`, and the dynamic
keys and indexes of the path (`lowerExpressionInTempScope()`).  A compound assignment, `push`, or
`unshift` to a variable changes the variable's value in place, so its operand is scoped too unless the
variable's type holds no reference; a plain assignment to a variable replaces the value and is not scoped:

```text
push.temp.mark
load.local @h -> %6
hash.key.access.hash.guarded .x -> %7 %6
hash.key.access .a -> %8 %7
add.any -> %10 %8, %9
discard.temps.keep -> %11 %10
lvalue.path.assign %11
```

`DiscardTempsKeep` takes a reference to its operand before it releases the scope's temps, since the
operand can be one of them or be borrowed from one, and registers that reference in the enclosing scope,
so the mutation can still take the value over.  The operand scope is used only when the operand creates
temps besides its value and the statement has a temp scope of its own; its mark has no source location, so
it is not a debugger step.  A destructor that raises while the operand's temps are released branches to the
exception target with the enclosing scope, which is then the innermost one: the mutation is not made, as in
the AST.

Statements whose expression is evaluated before their body runs hold the expression's temps while the
body changes containers, for the same reason: the list of a `foreach` statement and the value of a `switch`
statement are lowered with `lowerExpressionInTempScope()`, so that only the value iterated or switched on
is kept, and the initializer of a `for` statement, whose value is not used, gets a temp scope closed with
`DiscardTemps` before the loop.  Without this, a `foreach` over `index.map{app}.pairIterator()` that writes
to `index.top` copied `index.top` in its first iteration (ProviderIndexUtil::mergeIndexedActions()).

The interpreter also releases its own cached references to the root local (slot cache and earlier load
slots) before it navigates an assignment, compound assignment, `push`, or `unshift` path, not only
afterwards (see *Cache Invalidation Rule*).

Regression coverage: `examples/test/ir/lvalue-operand-temps/` checks container identity (with library
debugging) and values in every execution mode and in a compiled module.

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

## Removal Semantics: `remove` and `delete`

The AST engine implements both operators with `LValueRemoveHelper`, and that class defines the
behaviour every other engine has to reproduce.  Two parts of it are easy to lose in a reimplementation:

- **`delete` runs destructors; `remove` never does.**  `LValueRemoveHelper::deleteLValue()` calls
  `QoreObject::doDelete()` on the removed value when it is an object, and raises `SYSTEM-OBJECT-ERROR`
  for a system object.  Clearing the slot and letting the reference count collect the object later is
  not the same thing: `delete` is defined to destroy it immediately however many references remain.
- **The "direct list" form.**  A list index slice or range slice removes a *set* of elements, and the
  removed value is a synthetic list holding them rather than a value that was stored in the container.
  `delete` applies to each element of such a list.  A hash slice and an object member slice remove a
  *hash* instead, which `deleteLValue()` does not iterate, so those release their values without
  deleting them.

Destructors are Qore code, so they must run with the lvalue locks released - see the same rule in
`design/dgc.md`.  `~LValueHelper()` drops its locks before discarding its own temporaries, and
`LValueHelper::saveTemp()` defers a removed value's dereference to that point; a removal that has to
run `doDelete()` defers it until after the `LValueHelper` has been destroyed.

Regression coverage: `examples/test/qore/misc/lvalue-delete-semantics.qtest` pins every shape in
whatever engine it is run under, and `examples/test/qore/misc/lvalue-delete-cycle-scan.qtest` covers
the lock-release requirement.

## Review Checklist

When adding a new lvalue opcode or helper:

- The lowering path uses borrowed loads for mutation roots.
- A value operand that can read the container is lowered with `lowerMutationOperand()`.
- Paired local mutations clear only compiler-owned references before COW.
- Shared local roots use lock-held structured lvalue navigation.
- Interpreter cleanup never owns borrowed roots.
- Exception edges release lvalue locks before cleanup or catch transfer.
- Caches are invalidated before mutation.
- COW replacement writes back to the runtime local.
- JIT and AOT variants preserve the same writeback behavior.
- Removal opcodes match `LValueRemoveHelper` semantics, including which forms run
  destructors, and run them with the lvalue locks released.
- Tests cover unique and shared-container mutation.
