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

The counter reports the vector of the innermost running IR frame, so each measured function reads it itself.  The
checks first read it through a helper function, which runs in its own IR frame: they measured the helper's vector,
which does not change with the loop, and could not fail.  The ir worker also asserts that a `switch` over a live
temporary adds entries to the vector that the function running it reads, so the growth checks measure the loop's
frame.

## The list of instantiated locals

A frame also records the locals it instantiates on the runtime local stack, in instantiation order
(`IRCallFrame::instantiated_locals_ordered`, filled by `ensureLocalInstantiated()`), so that frame exit cleans them up
in reverse.  Only locals that the IR interpreter does not keep in its own slots are recorded: objects (so that a
destructor runs when the block is left), lists and hashes assigned through the runtime stack (such as the target of
`placed += {...}`), and every local of a function that `computeIROnlyLocals()` cannot analyse completely; a method
call on `stderr` is one such construct, so `qore-data-provider-i18n`'s `rekeySourceTree()` has all of its locals
recorded.

`UninstantiateLocal` cleared a local's entry without removing it, and it scanned the list even for a local that was
not instantiated.  A `continue` uninstantiates every local of the loop body, including the locals the body declares
after the `continue`, which that iteration never instantiated; each such `continue` therefore scanned the whole
list, and the list grew by every local the body instantiated in every iteration.  A loop like

    foreach hash<auto> entry in (new_messages.pairIterator()) {
        if (*hash<string, int> c = counts{entry.key}) {
            string message = mostFrequent(c, first{entry.key});
            placed += {"id": entry.key, "pos": first{entry.key}{message}, ...};
            continue;
        }
        *hash<string, bool> same_text = by_text{text_key};
        if (same_text.size() == 1) {
            placed += {"id": entry.key, "pos": unplaced, ...};
            continue;
        }
        ...
    }

got slower with every iteration that the function had run before it, and so did every later pass of a loop around
it.  A single `continue` branch only grows the list; the scans come from the locals declared after it, which is why
the slowdown needed a second branch.  The JIT and tiered modes run a function in the IR interpreter until its native
form is ready, from its next call on, so a single long call such as `rekeySourceTree()` showed the same slowdown.

`UninstantiateLocal` now scans only for a local that is instantiated (the list has exactly one entry for each one),
and drops the cleared entries at the end of the list.  A block uninstantiates its locals in the reverse order of
their instantiation, so the list stays at the locals that are instantiated.  An entry can stay behind a local that
remains instantiated, but only once per local, not once per iteration.  Native code keeps no such list.

A local declared in the condition of an `if` statement (`if (*T v = ...)`) belongs to the statement: AST mode
releases it when the statement ends (`IfStatement::execImpl()`).  The IR did not release it at all, so it stayed
instantiated until the function returned, holding its value (an object was destroyed one loop iteration late, or at
function return), and the entries of a loop's first pass that preceded it stayed in the list.  The lowering of an
`if` statement now releases these locals at the end of the statement, through a block cleanup entry for a `break`,
`continue` or `return` out of a branch, and in an exception cleanup block on the way of an exception to an enclosing
target, in reverse order and after the branch's own locals; `examples/test/ir/if-condition-local/` compares the
order of the events and destructions of each of these exits with AST mode, in every execution mode.

`examples/test/ir/loop-local-uninstantiate/` asserts, via `dbg_get_ir_instantiated_local_entries()` (the size of the
innermost IR frame's list, 0 outside the IR interpreter), that the list does not grow across the iterations of loops
that hold an object, a list extended with `+=`, and the re-key's placement loop, nor across the passes of the
placement loop from the second pass on, in the ast, ir, jit and tiered modes; that the IR interpreter records the
object loop's local, so the growth checks are not vacuous; and that the loop's locals are released when the AST
interpreter releases them.  The counter is called directly in each measured function: a helper function would report
its own frame.
