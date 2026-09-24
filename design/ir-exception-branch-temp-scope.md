# IR Exception-Branch Temp Scope

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  Every IR opcode that can raise an exception and branch to a landing
pad in the same function drains only the raising statement's temp scope.

Relevant code:

- `lib/QoreIRBuilder.cpp` — the `create*()` functions record `exception_temp_scope_id`
  in `QoreIRInstruction::temp_scope_id`.
- `lib/QoreIRInterpreter.cpp` — `cleanupToTempScope()`, and the opcode handlers listed
  under *Opcodes that follow the rule*.
- `lib/QoreIRLowering.cpp` — `lowerStatementBlock()` brackets each statement with
  `PushTempMark` / `DiscardTemps`; `lowerExpression()` assigns `exception_target`.

## The two cleanup primitives

The IR interpreter owns node references through a `cleanup` stack of value-slot
ids.  Two primitives release them, and they are not interchangeable:

- `cleanupValues()` drains the **whole** stack and clears every owned slot in the
  frame.  It is correct only when control is leaving the frame for good.
- `cleanupToTempScope(scope_id)` drains back to the `PushTempMark` sentinel with
  that scope id.  Slots registered before that mark — the enclosing statement's
  and enclosing loop's temps — keep their references.

`lowerStatementBlock()` brackets every statement that can create a node temp with
a `PushTempMark` / `DiscardTemps` pair, so at any point during lowering
`QoreIRBuilder::exception_temp_scope_id` names the innermost open mark.

## The rule

An instruction that branches to an in-frame `exception_target` must use the
scoped drain, with the `temp_scope_id` it recorded at build time:

    if (!inst->exception_target) {
        return returnAfterUnhandledException();   // leaves the frame: full drain
    }
    cleanupToTempScope(inst->temp_scope_id, true);

A landing pad is *inside* the frame.  Everything the handler may read, and
everything the code after the handler may read, is still live there; only the
raising statement's own temps are dead.

Nothing leaks.  What the scoped drain leaves behind — intermediate marks and
temps between the raising statement and the `try` — is released by the
`DiscardTemps` of the enclosing statement that the handler falls through to
(`try.merge`), or, if the exception leaves the frame after all, by the full drain
in `returnAfterUnhandledException()`.

## Every in-frame branch follows the rule

`QoreIRBuilder::append()` records the builder's `exception_temp_scope_id` - the innermost open mark, or 0 for a
statement that creates no node temps and has no mark - on every instruction it creates, and the lowering creates
its own instructions (hash key and list index stores, lvalue paths) through it too. `PushTempMark` and
`DiscardTemps` then set their own scope. Every place where the interpreter branches to an in-frame
`exception_target` drains that scope first, whatever the opcode:

- opcodes that raise and branch themselves (`Invoke*`, `Throw`, `Rethrow`, `Context*`, `Backquote`, `Find`,
  `RefForeach*`, `Summarize`, `TypedForeachNext*`, `ScopeExit`, `Decref`);
- the stores that raise and branch (`StoreLocal`, `StoreClosure`, `StoreGlobal`, `StoreThreadLocal`,
  `HashKeyStore*`, `ListIndexStore`, `ListSetValue`, the `Map*` / `Select*` hash operations, `CallClosureDirect`);
- `CheckException`, which a block with local variables emits at its end and which is the in-frame branch for
  anything raised while those variables are released;
- the `LValuePath*` family and `Sprintf`, which drained the whole frame (`cleanupValues()`) before an in-frame
  branch, and the cancellation check at loop headers, which did the same;
- scope 0 drains nothing: the statement has no temps, and the innermost mark belongs to an enclosing statement
  whose temps may still be needed.

The drain keeps the mark itself (`cleanupToTempScope(..., keep_mark)`): it releases the temps registered after the
mark's sentinel and removes only the marks above it. An instruction emitted at block level rather than inside a
statement - the check at the end of a block with local variables, the one in its exception cleanup block,
`ScopeExit`, `Decref` - records the scope of the statement that encloses the block, such as the `try` whose body it
is, and that statement runs on after its handler has: its `DiscardTemps` at `try.merge` must still find its mark.
When the mark had been removed, that `DiscardTemps` fell back to the nearest mark - the enclosing typed `foreach` -
and released the list the loop re-reads on every iteration, which crashed. A mark kept for a statement that the
exception did end is removed, with its sentinel, by the `DiscardTemps` of the statement enclosing it.

Before, only the first group drained by scope. A statement that failed in a store or at a block-end check kept its
temps until the enclosing statement ended - after its handler had run, where the AST interpreter had already
released them - and the `LValuePath*` family released the temps of the enclosing statements as well.
`examples/test/ir/exception-temp-scope/exception-temp-scope.qtest` counts the objects destroyed when each handler
runs for every kind of failing statement, including the negative case of a handler inside a loop over a temporary
list, in each execution mode and from a compiled module.

`ContextRef` and `ContextRow` were created without an `exception_target`, so an unknown context key (`%nosuch`)
took the frame-exit path: the interpreter returned from the frame and the exception escaped an enclosing `try` in
the same function, which AST mode caught. `lowerExpression()` sets their target from `getCurrentExceptionTarget()`.

## Why it matters: typed foreach

A typed foreach (`foreach int`/`float`/`bool`/`string`) does not allocate an
iterator object.  The preheader takes an owned reference to the list into a value
slot, and `TypedForeachNext*` re-reads that slot, plus the index and limit slots,
on every back edge:

    entry:
      push.temp.mark
      make.list -> %3 ...
      ref.self  -> %4 %3            # owned: live for the whole loop
    foreach.typed.header.1:
      phi -> %7 ...
      foreach.next.string -> %8 %4, %7, %5

`%4` sits in the `cleanup` stack below the loop body's mark.  A full drain at a
`throw` inside the body therefore released the list and set the slot to NOTHING;
the next `TypedForeachNext*` called `size()` on a null `QoreListNode` and the
process crashed after the first iteration.  A loop over `auto` is unaffected:
its `IteratorNext` holds a `FunctionalOperatorInterface*` as a plain integer,
which the cleanup stack does not own.

This is why an exception raised by a *call* inside a typed foreach always worked
— the `Invoke*` opcodes already drained by scope — while a `throw` or `rethrow`
written directly in the loop body did not.

## LLVM / AOT

`QoreIRToLLVM` reads `temp_scope_id` only for `PushTempMark` and `DiscardTemps`, so recording it on every
instruction is inert for the JIT and AOT tiers. Compiled code already released a failing statement's temps before
its handler ran; the compiled-module run of `exception-temp-scope.qtest` checks it.

## Coverage

`examples/test/ir/IRTypedForeachExceptionState.qtest` compares full program output
across the AST, IR, tiered, JIT and AOT tiers for caught throws, single and double
rethrows, nested typed loops, `continue` / `break` / `return` out of a handler,
exceptions that leave the loop, `on_error` handlers, throws nested several statement
scopes below the `try`, and failing `context` initializers, context row references
and `find` expressions inside a typed foreach.
