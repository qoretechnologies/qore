# IR Loop-Jump Temp Scope

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  A `break` or `continue` that jumps out of one or more open statement temp scopes releases those
scopes' temps on the jump's path, so the IR interpreter's runtime cleanup vector no longer grows every time the
jump runs, and a temporary held across the jump is destroyed when the jump leaves its scope rather than at frame
exit.

Relevant code:

- `lib/QoreIRLowering.cpp` — `statementMayCreateNodeTemp()` no longer brackets `break`/`continue`;
  `emitBlockCleanups()` emits `UnwindTemps` for each temp scope the jump leaves, interleaved with the block
  cleanups, innermost first; the `break`/`continue` cases pass the jump target's temp-scope depth.
- `lib/QoreIRBuilder.cpp` — `createUnwindTemps()` / `getTempScopeDepth()`.
- `lib/QoreIRInterpreter.cpp` — the `UnwindTemps` opcode drains the runtime cleanup vector back to and including
  the scope's mark; `q_get_ir_innermost_cleanup_entries()` exposes the vector size for the regression test.
- `lib/QoreIRToLLVM.cpp` — `UnwindTemps` is a no-op in native code (see below).
- `lib/QoreIRAnalysis.cpp` — `UnwindTemps` is treated as a temp-drain barrier alongside `DiscardTemps`.

## The bug

`lowerStatementBlock()` brackets every statement that can create a node temp with a `PushTempMark` /
`DiscardTemps` pair, so the statement's expression temps are released at its end (matching AST-mode
`ValueEvalRefHolder` timing).  `DiscardTemps` is emitted at the end of the statement, on its normal, fall-through
path.

A `break` or `continue` does not reach the end of its statement, or of the statements it is nested in up to the
loop: it branches to the loop's latch or exit block.  Every `PushTempMark` between the loop and the jump is
therefore left open on the jump's path — its `DiscardTemps` sits on a fall-through path the jump skips.

In the IR interpreter this is a runtime leak of cleanup-vector entries.  `PushTempMark` pushes a sentinel onto the
frame's `cleanup` vector; the matching `DiscardTemps` drains back to it.  When the jump skips the `DiscardTemps`,
the sentinel (and any temp registered after it) stays on the vector.  A loop that takes the jump on every
iteration pushes one or more sentinels per iteration that are never drained, so the vector grows without bound.
Operations that scan the vector — `removeAllCleanupEntries()`, exception unwinding — then get linearly slower, so
a loop of *n* iterations runs in *O(n²)* time.  This is what made

    for (int i = 0; i < 5000; ++i) {
        if (s.find("og") < 0) {
            continue;
        }
    }

take 5.6 ms for the first 5000 iterations and 232 ms after 150k continues, growing linearly with the number of
continues the program had executed.

Two distinct marks leak, exposed by two patterns:

- the jump statement's **own** mark.  `break` and `continue` create no node temps, but
  `statementMayCreateNodeTemp()` bracketed them through its default `return true`, so a bare `continue;` pushed a
  mark and branched away before its `DiscardTemps`.  This is the reported case (the method call in the condition
  is incidental — it is what makes the enclosing `if` close its own condition scope before the branch, leaving
  only the `continue`'s mark to leak).
- an **enclosing** statement's mark, such as a `switch` whose value is a live temporary: `continue` out of a case
  leaves the switch value's scope open, so its mark leaks and the switch value itself is held until frame exit.

## The fix

Two complementary changes:

1. `break` and `continue` are no longer bracketed (`statementMayCreateNodeTemp()` returns `false` for them).
   They create nothing to release, so the mark was pure overhead; removing it fixes the reported case in every
   tier at the source.

2. A jump releases the temp scopes it leaves.  `emitBlockCleanups()`, which already emits the block cleanups
   (`ScopeExit`, `UninstantiateLocal`, ref-foreach write-back) for a `break`/`continue`/`return`, now also emits
   an `UnwindTemps` for each temp scope open between the jump and its target, innermost first and interleaved with
   the block cleanups in the AST destruction order.  The jump target records the temp-scope depth where it was
   pushed (`FlowTarget::temp_scope_depth`), which bounds the unwind so the loop's own enclosing scopes — and a
   typed `foreach` iterator temp — are preserved.

`return` needs none of this: it leaves the frame, and frame exit drains the whole cleanup vector, so its open
scopes are released there.  `break`/`continue` stay in the frame, which is why they alone accumulated.

## Interpreter vs. native code

`UnwindTemps` behaves differently in the two execution models, by design:

- **IR interpreter**: it drains the runtime `cleanup` vector back to and including the scope's mark (like
  `DiscardTemps`, which it mirrors, but on the jump's path).  Only one of the jump path and the fall-through
  `DiscardTemps` path executes per iteration, so removing the sentinel here is correct and is what stops the
  vector from growing.

- **JIT / AOT native code**: it is a **no-op**.  Native code has no runtime cleanup vector; a scope temp lives in
  a reused stack slot released at the next overwrite or at frame exit, so the unbounded growth the interpreter
  suffered never occurred.  Emitting a decref on the jump path would have to pair with the same scope's
  `DiscardTemps` on the fall-through path, whose basic block the LLVM emitter may lay out before or after the jump
  block; the single-consumer mark bookkeeping the emitter uses cannot support that safely, so native code keeps
  its existing behavior.  The only observable difference from AST mode in native code is that a temp held across
  the jump is destroyed one iteration late (at the next overwrite of its slot, or at frame exit for the last),
  which is bounded and never accumulates.  Matching AST exactly in native code would require giving the LLVM temp
  marks per-instruction watermark snapshots; it is a possible follow-up, not a correctness or leak concern.

The analysis passes that reason about temp drains (`qore_ir_loop_temp_scopes_are_closed`, the pure-call CSE
barrier, the scoped-expression CSE, the aggregate-projection lifetime check) treat `UnwindTemps` conservatively,
like `DiscardTemps`.

## Coverage

`examples/test/ir/loop-jump-temp-scope/` runs in the test program and in a worker in the ast, ir, jit and tiered
execution modes.  It asserts, via `dbg_get_ir_cleanup_entries()` (the IR cleanup-vector size, 0 outside the IR
interpreter), that the vector does not grow across a loop whose body ends in a `continue` with a method call in
the condition (the reported case), a `continue` out of a `switch` over a live temporary, or a `break` out of an
inner loop nested in such a `switch`; and that no mode keeps an object alive past the frame that ran the loop.
With the fix reverted, the growth checks report one leaked entry per iteration for the `continue`'s own mark and
two for the `switch` case, in the ir and tiered modes.
