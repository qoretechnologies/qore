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

## Opcodes that follow the rule

Three shapes exist among the opcodes that can raise:

- **Raise and branch.** `Invoke`, `InvokeMethodDirect`, `InvokeDotEvalMethodDirect`,
  `InvokeHashKeyAccess`, `Throw`, `Rethrow`, `Context`, `ContextRef`, `ContextRow`,
  `Backquote`, `Find`, `RefForeachInit` and `RefForeachGetEntry` branch to
  `exception_target` themselves.  These are the ones that must drain by scope.
- **Raise and fall through.** `StoreLocal`, `StoreGlobal`, `StoreThreadLocal`,
  `StoreClosure`, `HashKeyStore`, `ListIndexStore`, `Sprintf` and the `LValuePath*`
  family leave the exception on the sink and let a later `CheckException` do the
  branch.  Their `exception_target` is null in practice.
- **Branch only.** `CheckException` performs the branch with no drain at all; the
  enclosing statement's `DiscardTemps` releases what is left.

An opcode in the first group is only reachable as such when the lowering actually gives
it `exception_target`.  `ContextRef` and `ContextRow` were created without one, so an
unknown context key (`%nosuch`) took the frame-exit path instead: the interpreter
returned from the frame and the exception escaped an enclosing `try` in the same
function, which AST mode caught.  `lowerExpression()` now sets their target from
`getCurrentExceptionTarget()`, as `Backquote`, `Find` and the `RefForeach*` opcodes
already did.

Most runtime errors reach a handler through one of the last two shapes, which is why
the defect was only ever visible for a literal `throw` / `rethrow`, a `context`
initializer, a context row reference or a `find` written directly in a loop body.

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

`QoreIRToLLVM` reads `temp_scope_id` only for `PushTempMark` and `DiscardTemps`;
its `Throw` and `Rethrow` lowering simply branches to the landing pad and never
had the full-drain behaviour.  Setting `temp_scope_id` on those instructions is
inert for the JIT and AOT tiers, exactly as it already was for `Invoke`.

## Coverage

`examples/test/ir/IRTypedForeachExceptionState.qtest` compares full program output
across the AST, IR, tiered, JIT and AOT tiers for caught throws, single and double
rethrows, nested typed loops, `continue` / `break` / `return` out of a handler,
exceptions that leave the loop, `on_error` handlers, throws nested several statement
scopes below the `try`, and failing `context` initializers, context row references
and `find` expressions inside a typed foreach.
