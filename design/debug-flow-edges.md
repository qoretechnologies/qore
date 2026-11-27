# Debugger flow at a step event: modeled IR edges (issues #6201, #6200 break/continue)

Status: implemented on `fix/6201-ir-debug-break` (decision (A), extended by prof's decision (ii) to the return); the
sections below are the design handover, and "Implementation" at the end lists what differs from it.  Since decision
L1, only the debug IR of AOT code has the paths; code lowered from the parse tree, in the IR interpreter and in JIT
code, has none: see "Which code has paths" at the end, which takes precedence over the sections before it.

All line numbers refer to develop d79a3a93fc (the `fix/6201-ir-debug-break` worktree, `~/tmp/qdbg/6201`).
The return part of #6200 is done separately on `fix/6200-native-debug-flow` (head dc3975e3f4, worktree
`~/tmp/qdbg/6200`); see "Relation to the #6200 return fix" below. This note covers DebugFlowBreak and
DebugFlowContinue in the IR interpreter and in JIT code.

Constraints:
- AST is the reference.
- The hot path stays exactly +0 in every mode: the map, call, `+=` and loop callgrind rows.
- `QoreIRInterpreter::execute` stays within its stack budget: 4096 B on develop, 4320 B after batch R.
- One mechanism for the IR interpreter and the JIT, compatible with the density branch's folded steps.
- No stop-gap. Any IR or AOT format change is announced before it lands.

## 1. Reference behaviour (AST)

The AST behaviour comes from `StatementBlock::execIntern()` (lib/StatementBlock.cpp ~259-345):

```
if (tlpd->runtimeCheck()) stmt_rc = tlpd->dbgStep(this, nullptr, xsink);     // block event
for (auto i : statement_list) {
    if (tlpd->runtimeCheck()) { stmt_rc = tlpd->dbgStep(this, i, xsink); if (stmt_rc || *xsink) break; }
    stmt_rc = i->exec(rc, return_value, xsink);
    ...
}
// on_block_exit handlers run here, with error = xsink->isException()
```

`dbgStep()` (lib/QoreProgram.cpp:3752) returns the `flow` that `DebugProgram::onStep()` set. The values are
`DebugFlowReturn = RC_RETURN`, `DebugFlowBreak = RC_BREAK` and `DebugFlowContinue = RC_CONTINUE`
(lib/QC_DebugProgram.qpp:71-75). `dbgSyntheticBlockStep()` does the same for the block event.

The rc then propagates as if a statement had returned it:
- The block ends normally: the rest of its statements are skipped, its on_exit and on_success handlers run (no
  exception), and its locals are released (LVListInstantiator).
- `if` and `try` pass the rc up. TryStatement does not catch it.
- Loops handle it: While/DoWhile/For/ForEach treat RC_BREAK as a break and RC_CONTINUE as a continue
  (lib/WhileStatement.cpp:101-108, ForStatement.cpp:120-127, ForEachStatement.cpp:142-149 and 304-318).
- `switch` absorbs RC_BREAK (lib/SwitchStatement.cpp:209). It absorbs RC_CONTINUE too, but only under the
  brokenLoop() policy.
- With no enclosing loop or switch, the rc reaches the function body. The function then returns with its
  `return_value` untouched, i.e. no value, as for RC_RETURN. A non-optional return type then raises
  `RUNTIME-TYPE-ERROR ... block missing return statement` in the caller.
- A flow at a block event (statement == nullptr) skips the whole block.

In effect, a break or continue at a step event is identical to a `break;`/`continue;` statement inserted before
the statement. Outside any loop or switch, it is a `return;` (for continue: outside any loop).

Measured AST results are in section 9.

## 2. Root cause in the optimized tiers

### 2.1 IR interpreter (lib/QoreIRInterpreter.cpp)

- **DebugStatement case (~16260-16360).** On `dbg_rc == RC_BREAK` it sets `debug_break_target =
  current_block->enclosing_loop_exit`, or the legacy `debug_break_loop = true`, and then falls through to
  `++ip`. So the statement runs, and so does the rest of the iteration.
- **DebugBlock case (~16210-16245).** Same: it falls through to `++ip`.
- **Consumption.** Only `QoreIROpcode::BrIf` consumes the request (~12046-12060: `br->false_target ==
  debug_break_target`). The fused compare-and-branch opcodes never check it: `BranchIfLtLocalInt` (~13434) and
  the other fused int/float compare-and-branch opcodes. So `for (int i = 0; i < n; ++i)` and
  `while (i < n)` never leave.
- **RC_CONTINUE** is ignored entirely.
- **Lowering side.** `QoreIRLowering::createBlock()` stamps `QoreIRBasicBlock::enclosing_loop_exit` from
  `current_loop_exit` (lib/QoreIRLowering.cpp:~7290, set at 2082/2225/2363). Only while/for/do-while/foreach
  set it; `switch` does not. For foreach, the body and iterator blocks are stamped (2365-2366), but blocks
  created later in the body depend on `current_loop_exit` still being set.
- **RC_RETURN is already correct.** It uses the runtime unwind: `unwindForException(); cleanupLocalCaches();
  return true`.

### 2.2 JIT (lib/QoreIRToLLVM.cpp, lib/JITRuntime.cpp)

- `QoreIRToLLVM::emitDebugStepHook()` (~13021) emits a gated cold block. The gate is an atomic load of
  `source_dpgm_addr`, with branch weights 1:2000. The cold block calls `qore_rt_dbg_step` or
  `qore_rt_dbg_synthetic_block_step` (JITRuntime.cpp ~1422/1434), then `emitExceptionCheck()`, then `br cont`.
- Both runtime functions returned void, dropping the rc.
- On the 6200 branch the return is handled; break and continue are still dropped.

### 2.3 Why "just branch to the loop exit" is wrong: the optimizer cannot see the edge

The step event of statement S sits in the middle of an IR basic block. The IR has no non-exceptional
control-flow edge from a point inside a block:
- `QoreIRControlFlowGraph` (lib/QoreIRAnalysis.cpp:2381) builds edges from terminators only
  (`qore_ir_visit_successors`, 2292). Exception edges are added only with `include_exception_edges`.
- `qore_ir_prune_unreachable_blocks` (7007) is the only pass aware of `enclosing_loop_exit` (7038).

A jump from S to the loop exit X therefore enters X through an edge that no pass modeled. Code at and after X
uses SSA values and facts computed from the normal predecessors only, i.e. the header's false edge.

The statement position matters too: any jump that skips S (and the rest of the iteration) needs the state at S,
not at the end of the block.

## 3. Evidence: the reverted direct-jump patch

The temporary patch was applied on canoe only, in `~/tmp/qdbg/6201`, and is now reverted. In the DebugStatement
RC_BREAK branch it did:

```
if (current_block->enclosing_loop_exit && getenv("NAIVE_DBG_BREAK")) {
    prev_block = block; block = current_block->enclosing_loop_exit; ip = 0; break;
}
```

That is a direct jump to the loop exit, with no cleanups.

### 3.1 Stale SSA-promoted local

The target was `~/tmp/qdbg/probe/spin_target.q` (copied on canoe as `~/tmp/qdbg/flowtest/spin_target.q`):

```
string sub spin(int n) {
    float f = 0.0; int total = 0;
    for (int i = 0; i < n; ++i) { f = f + 1.5; total += 1; }
    return sprintf("f=%y total=%y", f, total);
}
```

The parent, `spin_parent.q`, runs `spin(2000000000)` in a background thread. It attaches from the main thread
after 300 ms and breaks at the 2nd step event of `total += 1;` (line 7).

- The IR reports `IR-OPT: spin: ... native-local-loads=2 native-local-stores=2`, so `f` is promoted to SSA.
- **AST:** `f=884767.5 total=589844`. f == 1.5*(total+1): the `+1.5` of the breaking iteration ran.
- **Naive IR jump:** `f=7040959.5 total=4693973`. f == 1.5*total, so f is stale. X reads the loop header's phi,
  which holds the value at the start of the iteration.
- **Develop IR** never leaves the loop (fused condition) and ran into the 120 s timeout.

### 3.2 Cleanups skipped

From the StepFlowTarget.q cases, IR with the naive jump against AST:

- `object_local` (break at `object_first`):
  - AST log: `destroy t0, t1, t2, t3, outer`.
  - Naive: `destroy t0, t1, t2, outer, t3`. The block local `t` of the loop body was not released at the break,
    only at the function's end.
- `handlers_in_loop` (break at `handlers_first`):
  - AST log: `success0, exit0, ..., success3, exit3`.
  - Naive: `..., success2, exit2, success, exit`. The handlers of the iteration's block did not run at the
    break; they ran later at the function's exit, with the block's local `i` gone (`"success" + i` gave
    `"success"`).
- `foreach_strings`: the naive patch did not fire at all (`current_block->enclosing_loop_exit` was null in the
  block of that statement), so the request was ignored ("s0,s1,s2,s3," vs AST "s0,s1,s2,").
- `for_loop` and `locals_changed` happened to match AST: no promoted local, no block cleanup.

## 4. Passes and code that an invisible mid-block edge breaks

Most of this list comes from a read-only audit. "Unsafe" means the pass gives wrong results. "Conservative"
means correct but slower: it either drops the optimization or sees the cold block as unreachable.

The new edge must be visible to every flow-sensitive pass, with the state **at the DebugStatement** (not at the
end of its block) as the input of the cold block.

All passes below are in lib/QoreIRAnalysis.cpp unless stated otherwise.

| Pass (line) | State propagated | Today with an invisible edge | Needed |
|---|---|---|---|
| `qore_ir_promote_native_local_loads` (9202) | SSA promotion of IR-only int/float/bool locals (liveness, phis, store elimination) | Bails on any `exception_target` (9219). An access in an unreachable block (9337) or a phi block with an unreachable predecessor (9463) invalidates promotion, which costs performance (f is no longer promoted). If the cold block were instead made reachable from the end of the block, values would be stale (the 3.1 failure). | Record `current` at the DebugStatement as the output of the trampoline block T_s. Count T_s as a successor of B for liveness. Add a phi incoming from the path block at X. |
| `qore_ir_values_proven_assigned_at` (10102; used by LICM 3031/7570, aggregate checks 10501, the interpreter and the LLVM lowering) | must-assigned locals | Unsafe: skips unreachable predecessors (~10316) | Input of T_s = state at s |
| `qore_ir_propagate_exact_boxed_local_facts` (15363, late JIT/AOT specialization) | exact boxed type per local | Unsafe: skips unreachable predecessors (15696). `qore_ir_specialize_proven_boxed_operations` (15816) and `qore_ir_specialize_proven_native_operations` (14847) consume its facts. | same |
| `qore_ir_mark_local_list_pushes` (8511) | fresh/unshared list bitset | Unsafe: skips unreachable predecessors (8818); the escape transfer runs on reachable blocks only (8854) | same |
| `qore_ir_mark_in_place_string_appends` (8864) | fresh string locals | Unsafe (9131, 9170) | same |
| `qore_ir_eliminate_common_scalar_expressions` (7193) | available expressions from a single predecessor or the idom | Unsafe: the idom ignores unreachable predecessors. Values defined after s would be treated as available on the break path. | Dominance must include T_s, and the input of T_s = state at s |
| `qore_ir_eliminate_redundant_phis` (6426) | single-valued phi replaced by a dominating definition | Unsafe: dominance (6481) | Dominance with T_s |
| `qore_ir_find_local_reads_overwritten_before_use` (929) | own successor graph: terminators plus `exception_target` (1062-1080) | Unsafe: a write in the cold path (e.g. UninstantiateLocal) between a read and a use is missed, so a read may be borrowed wrongly | Add the T_s edge, from the position of s |
| `qore_ir_find_open_implicit_contexts` (806) | open implicit contexts; models `exception_target` mid-block (867) | Low risk; the first-visit state of X wins | Add the edge |
| `qore_ir_refine_local_value_facts` (9833) | must "assigned" facts | Conservative (a block with no predecessors starts empty, 10005) | Input = state at s, to keep precision |
| `qore_ir_refine_dense_list_facts` (10567) | per-local dense facts | Bails on any `exception_target` (10581); otherwise conservative (10711) | Input = state at s |
| `qore_ir_scalar_replace_fixed_aggregates` (3345) | dominance and a store-dominated mutation region (4727-4736) | Conservative: the candidate is invalidated | Treat like promotion: field values at s must become the phi incomings at X |
| `qore_ir_prune_unreachable_blocks` (7007) | reachability | Would delete the cold blocks | Visit the new DebugStatement targets, as for `enclosing_loop_exit` (7038). Also rewrite or drop them on removal (7132). |
| `QoreIRVerifier::collectBranchTargets` (lib/QoreIRVerifier.cpp 119-213) | which blocks are checked | Blocks that are not targets are never verified (236-238). Phi incomings are not checked against the real predecessors (484-488). | Count the new targets, and verify phi incomings for T_s/X |

Not affected (value-based or block-local): `fold_pure_builtin_calls`, `fold_scalar_list_queries`,
`load_read_only_constant_containers`, `eliminate_common_pure_calls`, `elide_proven_return_checks`,
`build_exception_release_groups`, `fuse_call_target_checks`, `find_reused_call_args` / `QoreIRBlockReachability`
(the cold path is in no cycle), and `fold_constant_branches` (it removes phi incomings only for the predecessors
it removes).

LICM (11366-11531) and `qore_ir_find_natural_loops` (2855) are affected only through
`values_proven_assigned_at`. The cold path is outside the loop, and the preheader dominates it.

Also check the interpreter's and the JIT's own per-function preparation:
- QoreIRInterpreter.cpp ~3079-3376: `interpreter_cross_block_slots`, `return_protected_slots` and the release
  groups.
- QoreIRToLLVM.cpp ~14050-14317: `value_def_block`, `operand_remaining_uses`, `reused_call_args`.

Values used in T_s and its path, such as foreach `old_implicit_element` and the iterator values that a break
releases, are defined in a dominating block, so they are already cross-block. The new phis at X are new
cross-block uses and must be marked as such.

**Recommendation:** add the edges to `QoreIRControlFlowGraph` itself, as a third kind of edge next to terminator
and exception edges. Each mid-block edge records its source block and instruction index. Then give each
flow-sensitive pass one helper, "state at instruction k of block b", instead of hand-written special cases. The
promotion pass already walks blocks instruction by instruction, so recording `current` at the DebugStatement is
a small change there. The same is true for most of the others.

## 5. Proposed design (A)

### 5.1 IR shape

For every DebugStatement (statement event) and DebugBlock (block event) that a function with step hooks lowers:
- in JIT/IR mode, `source_dpgm_addr` is set (Function.cpp:7881, StatementBlock.cpp:~992);
- not in AOT debug IR (`emit_debug_statements`); see 5.7.

For a DebugStatement s in block B, the instruction gets an optional pointer to a flow-target record:

```
struct QoreIRDebugFlowTargets {           // owned by the QoreIRFunction (vector<unique_ptr<>>), shared by pointer
    QoreIRBasicBlock* break_target = nullptr;     // T_s(break): one block per DebugStatement
    QoreIRBasicBlock* continue_target = nullptr;  // T_s(continue)
};
// QoreIRDebugStatementInstruction gains:  QoreIRDebugFlowTargets* flow = nullptr;   (one pointer; see 5.7)
```

**Per-statement trampolines.** T_s holds exactly one instruction, a `Br` to the shared path block P of the
current cleanup segment. It is per DebugStatement because:
- block-keyed phis at P can then tell apart the incoming states of different statements of the same IR block;
- every pass then sees an edge whose input is exactly "the state at s".

T_s costs one IR instruction per DebugStatement in a breakable context. It must not count toward
`QoreIRFunction::getInstructionCount()` (the JIT budget, `QORE_JIT_MAX_IR_INSTRUCTIONS`, Function.cpp:8692,
QoreJIT.cpp:1439) and must not change the density test's exact counts. Mark the blocks
`QoreIRBasicBlock::debug_flow = true` and skip them in the count. They are never executed on the normal path, so
the interpreter's dispatch is unchanged.

**Per-segment shared paths.** P(break) and P(continue) are shared by all statements of one cleanup segment. A
segment is a run of statements of one StatementBlock between which `cleanup_stack`, the temp-scope depth,
`flow_stack.back()`, the handler count (`block_handlers.size()`, which changes at each on_exit/on_success/on_error
statement because inline-lowered handlers are position-dependent) and the catch depth are all unchanged.

Each path is lowered **exactly like the BreakStatement/ContinueStatement lowering at that position**
(lib/QoreIRLowering.cpp 3449-3540):
1. Find the target: walk `flow_stack` for `break_target` / `continue_target`, which gives the loop exit, the
   switch exit (`ft.break_target` at 2961 for switch) or the latch (continue).
2. `emitBlockCleanups(target_cleanup_depth, error, false, CF_CATCH_SCOPES, target_temp_depth, loc)` (4746). This
   emits, innermost first, ScopeExit or the inline-lowered handlers, UninstantiateLocal for block locals,
   UnwindTemps, CatchCleanup for the catch scopes left, and RefForeach record/finalize.
3. For break out of a foreach whose flow target has a saved element (`ft.old_implicit_element`: a value foreach
   that sets its element pops the element that it saved before the loop, a foreach over a reference the element of
   its iteration): `createPopImplicitElement(old_elem)`; a foreach over a lazy pipeline restores the element at its
   exit block, which the break branches to (qore#7379).
4. `createBranch(target)`, with `exception_target = target_exception_target` when the target has
   `loop_cancel` (3484-3488).

Factor that sequence into a helper, e.g. `lowerLoopJump(bool is_break, const QoreProgramLocation* loc)`, used
by the statements and by the debug paths, so they cannot diverge. When a handler lowered inline in the cleanups
terminates the block (`blockHasTerminator`, 3476/3519), the path ends there, as a statement's does.

**No breakable target.** With no loop or switch for break, or no loop for continue, the flow means "return no
value". It does not need an IR path: the step hook takes it as a return (5.3, 5.4). This matches AST (section 1).
switch absorbs the break (`is_switch` flow targets have a `break_target`). For continue inside a switch, keep
AST's rule: it passes through the switch to the loop, unless the brokenLoop() policy is set.

**On_exit/on_success/on_error, try and catch.**
- emitBlockCleanups() already handles handler scopes: ScopeExit with `is_error=false`, so on_exit and on_success
  run, plus the inline handler lowering, the HandlerBarrier protection, and CatchCleanup (`CF_CATCH_SCOPES`)
  for the catch blocks being left.
- A try statement is not a flow target, so a break inside a try leaves the try like a break statement does.
- A flow requested **inside a handler body** lowered inline: emitBlockCleanups' HandlerBarrier logic applies.
  For handler bodies compiled as separate IR functions (handler_func at 5165/5309), flow_stack is empty, so a
  break there is a return from the handler. AST instead propagates the handler block's rc (`stmt_rc = nrc`,
  StatementBlock.cpp ~344) to the enclosing block. This is an open question (section 10).

**Creation point.** Create P lazily, the first time a statement of a segment needs it, in
`QoreIRLowering::lowerStatementBlock()` (4237). That is where DebugStatement is created (4472-4479), before
`createPushTempMark`.

Cache it per StatementBlock lowering with a key of (cleanup_stack.size(), the identity of
cleanup_stack.back(), temp-scope depth, block_handlers.size(), catch_cleanup_depth, flow_stack.size()).
Invalidate the cache when the key changes.

Lower P by saving the builder's block, `func->createBlock("debug.flow.break.N")`, lowering, and restoring. Use
`moveBlockToEnd` like the `return.replaced.release` block (1773-1789), so that block order and the hot code
stay as they were. The paths are cold: put them at the end of the function.

**DebugBlock (block event).** Its paths are those of the enclosing block at the position of the nested block:
the block's ScopeEnter is already done. Check this in `lowerStatementBlock` (4419-4441): the DebugBlock is
created after the scope/lvars entries of the block are pushed, so the block's own cleanups are included.

### 5.2 Selecting the target at run time

The rc of `dbgStep()` / `dbgSyntheticBlockStep()` selects the target: RC_BREAK goes to
`flow->break_target`, RC_CONTINUE to `flow->continue_target`, and RC_RETURN is handled as on the 6200 branch.
A missing target means "return" (no loop or switch).

An exception raised by the debugger wins over any flow, as today.

### 5.3 IR interpreter dispatch

Put this in the DebugStatement case (~16295-16350) and the DebugBlock case (~16210), after the exception
handling:

```
if (dbg_rc == RC_BREAK || dbg_rc == RC_CONTINUE) {
    QoreIRBasicBlock* t = qore_ir_debug_flow_target(dbg_inst, dbg_rc);   // shared helper, see 5.5
    if (t) { prev_block = block; block = t; ip = 0; break; }
    // no loop/switch: as RC_RETURN (AST)
    unwindForException(); cleanupLocalCaches(); return true;
}
```

- Remove `debug_break_target`, `debug_break_loop` and the BrIf diversion (~12050-12060). Remove
  `enclosing_loop_exit` and its stamping in lowering, and in prune 7038/7111/7132 (QoreAOT.cpp:4670 copies it).
- prev_block = block is what makes the phi at P/X select the incoming of T_s, which is why T_s must be a block
  per statement.
- Stack budget: this adds no locals to `execute()`. It removes two (the break bool and the target pointer). Run
  `tools/check-stack-usage.py` on a Release build configured with `-DQORE_STACK_USAGE_REPORT=ON`. The budget
  holds when the new code is inside the existing case, or in a NOINLINE helper.

### 5.4 JIT emission

In `emitDebugStepHook()` (~13021), on the cold `dbg_fire` path only:
1. Make the hooks return the rc: `int64_t qore_rt_dbg_step(...)` (JITRuntime.cpp), instead of the 6200 branch's
   void hook that raises a return as thread_exit. Keep that thread_exit for RC_RETURN and for a break or continue
   with no target.
2. After `emitExceptionCheck()`, if `flow->break_target || flow->continue_target`, emit
   `switch rc: RC_BREAK -> block_map[T_s(break)], RC_CONTINUE -> block_map[T_s(continue)], default -> cont`.
   Otherwise emit `br cont` as today.
3. T_s and P are ordinary IR blocks, which the JIT lowers like any other block. LLVM sees real edges, so mem2reg
   and its SSA are right. The IR phis at P/X are lowered as usual, with incomings from T_s.
4. Keep the branch weights: the gate is unchanged, and the switch is inside `dbg_fire`.

Since the hot path only gains extra successors on a cold block, it should stay +0. Verify with the callgrind
rows (5.8).

Check the JIT's own block bookkeeping, which must handle blocks reached only through these edges:
- `block_map` creation for all IR blocks;
- `value_def_block`;
- pending SSA cleanups and `emitCondBrWithSsaPreamble`, because an edge out of a statement in the middle may need
  the same `pending_ssa_cleanup` preamble as an exception edge does (see `emitExceptionBranch` ~12730);
- `iterator_cleanup_allocas`, which a break out of a foreach releases;
- the `finalizeIteratorReleases` exit-edge counting (16270-16290).

AOT: `emitDebugStepHook` returns early in `aot_mode`. No AOT native change.

### 5.5 Density branch folded steps

On `fix/ir-int-plus-equals-density` (dugout `~/tmp/qore-ir-density/fix`, head e163f1c558),
`qore_ir_fold_debug_statements()` moves a DebugStatement into its fused host (IncrementLocalInt /
AddAssignLocalInt) as `folded_debug_step` (a unique_ptr). It sets `step->folded_host = host` and copies the
step's `exception_target` to the host. It runs on the final IR, after `qore_ir_optimize`, from
`UserVariantBase::lowerIRFunction()`.

- **Interpreter.** `QORE_IR_DISPATCH_FOLDED_DEBUG_STEP()` jumps to `dispatch_folded_step` with `inst =
  folded_step`, which runs the normal DebugStatement case. The 5.3 code therefore runs unchanged, as long as it
  comes before the case's final `if (folded_host) { ...; goto dispatch_folded_step_host; }` (a jump to T_s must
  skip the host, which is the skipped statement). Use one shared helper, e.g.
  `qore_ir_debug_flow_target(const QoreIRDebugStatementInstruction*, int rc)`.
- **JIT.** The folded path calls `emitDebugStepHook(module, llvm_func, folded_step, false, host)`. The switch of
  5.4 is in the hook, so it works the same way. The insertion point after the hook is `cont`, before the host's
  code, which is right.
- **Fold step.** It must keep `flow` on the moved instruction (it moves the whole object, so it does), and it
  must not change any CFG fact. Since it runs after the optimizer, the mid-block edge's source is the host's
  index. Any later pass that builds a CFG (the late JIT specializations, the interpreter preparation) must take
  the edge from folded steps too: `qore_ir_folded_debug_step(inst)`.
- **Fold condition.** `qore_ir_can_fold_debug_step()` needs no change.
- **Merge conflicts.** With the 6200 branch: three textual conflicts in `emitDebugStepHook` and its
  declaration, as reported. The design's change to the hook body will conflict the same way; keep both sides.
- **Density test.** After the merge, `FoldedDebugStepDensity.qtest` testDebuggerReturn and testDebuggerBreak
  should compare every mode with AST (42 / "none"), instead of asserting "same as the unfolded reference".

### 5.6 Relation to the #6200 return fix (dc3975e3f4)

- The native return already works.
- `qore_rt_dbg_flow` raises a thread_exit. The frame token is the address of `native_entry_slots`.
  `qore_rt_dbg_take_return` runs at error_return and exception_exit_tail.
- When the hooks are made to return the rc (5.4), keep that path for RC_RETURN and for a break or continue
  without a target. Only RC_BREAK/RC_CONTINUE with a target use the new switch.
- The IR interpreter's return stays the runtime unwind.

### 5.7 Serialization and formats

- **AOT debug IR** (`emit_debug_statements`, QoreAOT.cpp:2777/24228, QoreAOTBinary.cpp:12340) is serialized;
  for DebugStatement/DebugBlock see QoreAOTInstRegistry.cpp:3360-3420 (nest_depth, nest_flags, block_locals).
  Recommendation: **do not** create flow paths in AOT debug IR. Lowering then checks `source_dpgm_addr &&
  !emit_debug_statements`, as the density fold does. Then no instruction field is serialized and the AOT format
  is unchanged.
- **Consequence:** in AOT code with a debugger attached, break and continue keep the AST-incompatible legacy
  behaviour, which is today's behaviour. If you remove the legacy `debug_break_target`, they are ignored instead.
  Announce this as a known limitation, or plan a format change (adding the target block indices to the
  DebugStatement record bumps the AOT format, which must be announced first).
- **In-memory IR change.** The new pointer field on `QoreIRDebugStatementInstruction` (+8 bytes per
  instruction) and `QoreIRBasicBlock::debug_flow`. The IR printer (lib/QoreIRPrinter.cpp) should print the
  targets. The density branch already widened the instruction with `folded_host`. To keep the size, hold
  `{folded_host, flow}` in one side record, if prof cares.
- **QoreIRVerifier.** Accept T_s/P and verify them (section 4).

### 5.8 Validation plan

- **Tests:** debugger-step-flow.qtest (section 9) extended with the break and continue cases, all modes
  (ast/ir/jit/tiered plus `run_ir_tier`, except cases that call target code after the attach, which
  `run_ir_tier` rejects as AST-tier calls), with the AST results pinned. Add a spin case (3.1) proving that a
  promoted local is not stale. It needs a thread attach, so assert the invariant `f == 1.5*(total+1)`, not fixed
  numbers.
- **valgrind** (`qore -b`, `qbuild --count`) on break/continue in all modes, especially object_local,
  foreach_strings and handlers_in_loop.
- **callgrind:** `~/tmp/qdbg/bench/cg.sh <base tree> <fix tree>`. It computes the per-iteration Ir from N=2000
  and N=12000, for call/map/pluseq/loop, each with `_nd` (`%no-debugging`) and `_dbg` variants, in all 4 modes.
  - 6200 baseline noise is ±0.52 Ir/iter (seen in unchanged paths too).
  - Also run `QORE_IR_OPT_STATS=1` on `_dbg` loop functions: native-local-loads/stores must be unchanged, which
    proves promotion is still active.
- **IR counts:** the JIT budget's instruction count must not change; check with the density test's
  `QORE_JIT_MAX_IR_INSTRUCTIONS` probe.
- **Stack check, suites, merge:** `tools/check-stack-usage.py`; `-d ir -d qore/debug` (Linux Release, qbuild
  --timed); the Mac Debug targeted tests; merge-tree against batch R.

## 6. Interpreter execute() notes

- `debug_break_target` and `debug_break_loop` are locals of `execute()` (9917-9924), captured by nothing. They
  can be deleted.
- `current_block` (10661) is used only for `enclosing_loop_exit`. Check its other uses before removing it.

## 7. Other locations to touch or check

- `lib/QoreIRLowering.cpp`:
  - `createBlock()` (~7283, the enclosing_loop_exit stamping);
  - loops 2082-2093, 2225-2237, 2363-2406 (current_loop_exit);
  - switch 2961, foreach break_handler 2578/3357, 23841 (other flow targets);
  - `pushFlowTarget()` (4728);
  - the header: `FlowTarget` (QoreIRLowering.h 1042) and `current_loop_exit` (897).
- `include/qore/intern/QoreIR.h:4084`: `enclosing_loop_exit`.
- lib/QoreAOT.cpp:4670: copies enclosing_loop_exit when splitting blocks. A block split must also keep the
  DebugStatement flow pointer valid, since the targets are blocks and the split is safe, and must keep T_s a
  single-predecessor block.
- Every pass that splits or merges blocks (QoreAOT.cpp block splitting, the IR block merges, if any) must keep
  T_s, P and the flow pointers valid.

## 8. Not in scope / known gaps

- An exception raised by a handler during the cold path: through emitBlockCleanups this behaves like a break
  statement, which is AST-correct.
- The IR's existing RC_RETURN (runtime unwind) does not let a try statement in the frame catch an exception that
  a handler raises during the return. The native return (thread_exit) deletes it in a try. AST catches it. This is
  pre-existing and separate.

## 9. Test drafts and the develop results

The test drafts are in `~/tmp/qdbg/6201/examples/test/qore/debug/debugger-step-flow/`: uncommitted copies of the
6200 branch's files.

- `StepFlowTarget.q` is the target Program, without PO_ALLOW_DEBUGGER. Each function attaches the debugger
  while it runs (at i == 2 if it has a loop). The lines tagged `### <tag>` are the request points.
  - Functions: for_loop, while_loop, while_call_loop (a plain-BrIf condition), foreach_loop, nested_loops,
    try_in_loop, switch_in_loop, locals_changed, handlers_in_loop, no_loop, int_return, object_local (with the
    Tracker class), foreach_strings, catch_in_loop, callee/caller.
  - **Steps are absolute line numbers:** do not insert lines above existing functions. Append new functions at
    the end.
- `StepFlowDebugger.q` is the debugger in its own Program. Usage: `<target> <function> <return|break|continue>
  <line> <hit> [block]`.
  - It calls the function once (to compile it), then attaches through `ProgramControl::resolveProgramId()`.
  - It prints `RESULT:<value or err: desc> LOG:<list> STEPS:<list>`. Steps are a line, or "b<line>" for a block
    event.
- `debugger-step-flow.qtest` (from the 6200 branch) checks the return cases. To add break and continue, add
  entries to `Cases` and a `testBreak()` / `testContinue()`. The pinned AST values are below.
- Runner scripts: on canoe and spinster, `~/tmp/qdbg/runcases.sh <tree> <casefile> <modes...>` and
  `~/tmp/qdbg/vg.sh` (valgrind). The case files are `~/tmp/qdbg/return-cases.txt`.

### Develop results (Linux Release, d79a3a93fc)

AST is the reference; the "develop ir / jit / tiered" columns are the results on develop.

| Case (function flow tag hit) | AST | develop ir | develop jit / tiered |
|---|---|---|---|
| for_loop break for_first 2 | 33 | 55 | 55 |
| for_loop break for_second 2 | 43 | 55 | 55 |
| for_loop continue for_first 2 | 44 | 55 | 55 |
| for_loop break for_body 2 block | 44 | 55 | 55 |
| for_loop continue for_body 2 block | 44 | 55 | 55 |
| while_loop break while_first 2 | 33 | 55 | 55 |
| while_call_loop break while_call_first 2 | 33 (log cond0..cond3) | 44 (cond0..4) | 55 |
| foreach_loop break foreach_first 2 | 33 | 44 | 44 |
| foreach_strings break foreach_strings_first 2 | "s0,s1,s2," | "s0,s1,s2,s3," | same |
| nested_loops break nested_inner 2 | 5143 | 5154 | 5165 |
| nested_loops continue nested_inner 2 | 5154 | 5165 | 5165 |
| nested_loops break nested_outer 2 | 3132 | 5165 | 5165 |
| try_in_loop break try_first 2 | 33 | 55 | 55 |
| catch_in_loop break catch_first 2 | 33 | 55 | 55 |
| switch_in_loop break switch_case 1 | 54 (leaves the switch only) | 1154 | 1154 |
| locals_changed break locals_after 2 | 12.0 | 27.5 | 27.5 |
| locals_changed continue locals_after 2 | 21.5 | 27.5 | 27.5 |
| handlers_in_loop break handlers_first 2 | 33, log success0..exit3 | 55 | 55 |
| object_local break object_first 2 | 33, log destroy t0..t3, outer | 55 (t4 too) | 55 |
| no_loop break no_loop_stmt 1 | null, log success, exit | 3 | 3 |

The full AST step lists for these cases are in ~/tmp/qdbg/flow-dev-linux.txt on the Mac; regenerate them with
runcases.sh on dev. The return cases and their pinned steps are in the qtest.

## 10. Open questions

1. **A flow inside an on_exit/on_success/on_error handler compiled as a separate IR function.** AST propagates
   the handler block's rc to the enclosing block (StatementBlock.cpp ~344, `stmt_rc = nrc`), so a break in a
   handler of a loop body breaks the loop. The IR runs these handlers in their own frame. Options: treat it as a
   return from the handler (diverges from AST), or let the handler frame report the rc to the parent's
   ScopeExit, which then takes the parent's flow path (more work). Decide.
2. **Continue inside a switch.** AST under the brokenLoop() policy (SwitchStatement.cpp:210) absorbs continue in
   the switch. Mirror the policy: the lowering's flow_stack for switch has no continue_target, so this is the
   default; check how brokenLoop lowers a `continue` statement and use the same.
3. **AOT debug IR (5.7).** Accept "break/continue not taken in AOT code with a debugger", or plan the format
   change. If accepted, decide whether removing the legacy BrIf diversion is fine (it only helped plain BrIf
   conditions).
4. **IR size.** T_s adds one instruction per statement in a breakable context, plus the shared paths (inline
   handlers can be duplicated per segment). Exclude them from the JIT budget count and check that compile time
   is acceptable on large functions (HttpServer-size methods).
5. **Instruction size.** +8 bytes per DebugStatement. Acceptable, or merge it with the density branch's
   `folded_host`?
6. **Statement-level temp scopes.** The DebugStatement precedes `createPushTempMark`, so the statement's own temps
   do not exist yet. The enclosing statements' temp scopes (e.g. a foreach list temp) are released by the
   `target_temp_depth` unwinding in emitBlockCleanups, as for a break statement. Confirm with foreach_strings
   under valgrind.

## Implementation

One mechanism for return, break and continue; the #6200 token design (a thread_exit raised for a return and ended at
the frame's exits) is gone.

- **IR.** `QoreIRDebugStatementInstruction::flow` and `QoreIRScopeExitInstruction::flow` (`QoreIRDebugFlowTargets`:
  `break_target`, `continue_target`, `return_target`).  Every step event of code with step events gets all three: a
  break or a continue with no loop or switch to take it uses the return path.  Each request has an entry block of its
  own (one `Br` to its path); the paths are shared per segment (`QoreIRLowering::DebugFlowSegment`) and lowered with
  `lowerLoopJump()` / `lowerReturnWithoutValue()`, which the break, continue and return statements use too.  The blocks
  of the paths have `QoreIRBasicBlock::debug_flow`, are not counted by `getInstructionCount()`, and run handlers out of
  line (`CF_HANDLERS_OUT_OF_LINE`) unless a handler can return.  Deviation: the blocks stay in creation order, as the
  code of a break statement does (the LLVM lowering is block-order sensitive for temp scopes); they are not moved to the
  end.
- **CFG and passes.** `QoreIRControlFlowGraph` has the edges in `successors`/`predecessors` and
  `debug_flow_sources`; a pass that walks a block in order takes the state at the edge with `QoreIRDebugFlowCursor`
  (before a step event, after a ScopeExit).  Patched: SSA promotion (liveness and values), local value facts, dense list
  facts, `values_proven_assigned_at`, the JIT's `qore_ir_analyze_local_assignment` (which decides the native locals),
  exact boxed facts, fresh list pushes and string appends, scalar CSE (dominated blocks reached by a path get what is
  available at the first event), aggregate scalar replacement, natural loops (no loops from path back edges), LICM and
  bounded list reads (no path blocks), the open implicit contexts, the reads overwritten before use, the reused call
  arguments, block reachability, `statementWritesLocalAfter()`, the string transform fusion, prune and the verifier.
- **Handlers** (prof's decision 3).  A request in a handler ends the handler; the block takes the request of its last
  handler run (AST `StatementBlock::execIntern()`).  Inline handler: its events branch to the paths of its body's loops,
  or to the end of the handler (`InlineHandlerFlow`): for the last handler of the exit, the scope's paths
  (`getScopeFlowPaths()`); otherwise a join block after the handler, where the exit goes on (one extra `Br` in debug code
  with two or more inline handlers at an exit).  Out-of-line handler: the handler frame reports the request
  (`qore_ir_set_handler_flow()`), and the ScopeExit that ran it takes the scope's paths.
- **Runtime.** The IR interpreter branches to the path in the DebugStatement, DebugBlock and ScopeExit cases.  Native
  code has no paths (see "Which code has paths"): its step hooks (`qore_rt_dbg_step()`,
  `qore_rt_dbg_synthetic_block_step()`) report the events and honour an exception that the debugger raises, and
  ignore the requests.
- **AOT (format v46, `QORE_AOT_DEBUG_FLOW_VERSION`).** DebugStatement, DebugBlock and ScopeExit records end with a
  u8 mask and a u16 block index per path; block flag 0x10 is `debug_flow`.  AOT native code lowers no path blocks.

- **Analyses of the code** (prof's decision after the `-d ir` regressions).  The summary, pattern and projection
  analyses take the function through `QoreIRCodeBlocks` (`include/qore/intern/QoreIR.h`): the blocks without
  `debug_flow` (size, front, index, `getSingleBlock()`), the incomings of a Phi from them (`countIncomings()`,
  `getIncomings()`), and `QoreIRControlFlowGraph(func, exc, &code)`, the graph of the code only, without the path blocks
  and the flow edges.  So code with step events gets the summaries of the same code without them.  The return facts of
  the effect summaries are those of the code (see the next item).
- **Soundness of the return facts.**  The return facts of the effect summaries are those of the code.  Only the
  debug IR of AOT code has paths; it runs in the IR interpreter, which reads no return facts of a callee, and AOT native
  code has no paths, so an AOT batch keeps the facts of the code.  JIT code has no paths, so a request never makes a
  callee of a JIT batch return without a value.
- **Size of the paths.**  Every step event has paths, so their code must grow with the function, not with the
  product of its statements and anything else:
  - the return paths share the release of the locals of the blocks at the bottom of the cleanup stack (the Lvars and
    CatchVar entries, typically the function block) and the return (`QoreIRLowering::debug_flow_return_tails`, block
    flag `debug_flow_return_tail`, transient): each path branches to that block after the cleanups of its own blocks.
    Before, each path uninstantiated every local of the function, and each UninstantiateLocal of a local released
    all of its coercion cleanup allocas (DeferredLocalRelease), so `exception-edge-code-size` (a local assigned in
    every case of a switch) and `JITLargeFunctionBlocks` (many locals, many statements) grew quadratically;
  - the paths of the step events of a handler body lowered inline are shared per segment, as those of other code
    (`InlineHandlerFlow::segment`), and the handler bodies that the paths of a function lower inline have a budget of
    their own (`QORE_IR_PATH_INLINE_HANDLER_BUDGET`, past which they run out of line): each path lowers the handlers
    that it runs inline when they can return, whose step events have paths of their own, so nested handlers that
    return (`CatchExitHandlerOrder` b5) grew exponentially (2.8 million LLVM instructions for b5);
  - only the outermost path resets the budget of the handlers lowered inline (`DebugFlowPathLowering`);
  - the requests of an event whose path is the same (a break or a continue that no loop or switch statement takes is
    a return) share its entry block (`makeDebugFlowEntries()`, `QoreIRDebugFlowTargets::targets()` gives each block
    once), and in a handler lowered inline those that end at the same block share the path and the end;
  - a path leaves a scope none of whose handlers runs at its position (its on_exit statements come later in the
    block) as a return statement there does, without running handlers out of line.
- **AOT outlining.**  Outlined code is AOT native code only, which takes no requests, so the outliner sets the paths
  aside while it outlines a function and puts them back with `undo()` (design/aot-function-outlining.md, decision 11);
  the debug IR, and format v46, are unchanged.
- **IR interpreter handlers.**  A debugger attached in a call that then raises an exception gets the events of the
  handlers that the exception runs in the caller (pre-existing: `debug_active` was stale on the exception path).

## Which code has paths (decision L1)

Only the debug IR of AOT code has the paths of a debugger's requests (`QoreIRFunction::hasDebugFlowPaths()`, true
with `emit_debug_statements`).  Code lowered from the parse tree gets the step events (DebugStatement, DebugBlock and
the step hooks of native code) and breakpoints, but no paths: `QoreIRLowering` makes no `QoreIRDebugFlowTargets` for
its DebugStatement, DebugBlock and ScopeExit instructions, and pushes no `InlineHandlerFlow`.

Why:
- a call that starts while a debugger is attached runs on the AST interpreter (`UserVariantBase::evalTiered()` with
  `hasDebuggerAttached()`; the fast and inline call paths of the JIT runtime and of the IR interpreter refuse to run
  then), so the IR and the native code lowered from the parse tree only run under a debugger in frames that were
  already running when it attached, and in what those frames call directly (the callees of a JIT batch, and the
  callees that LLVM inlined into them);
- the paths of every step event cost about 15-19% of the JIT compile time of code with step events (all code that is
  not parsed with `%no-debugging`), and the continue edges into a loop changed the native code of the loop's hot path;
- new calls of AOT code under a debugger run its debug IR in the IR interpreter
  (`UserVariantBase::materializeAOTDebugIR()`), so that IR needs the paths, and keeps them (format v46).

A request at a step event of code without paths:
| Code | Return | Break or continue |
|---|---|---|
| native code (JIT, tiered) | ignored: the hook returns no request | ignored |
| IR interpreter | the frame returns without a value through the runtime unwind (`unwindForException()`), as on develop: the handlers of the blocks that it leaves run as for an exception that leaves the frame, so an exception that one raises leaves the function instead of going to a catch block of the frame; in a handler lowered inline, the function returns, also when a handler that runs after it would replace the request; in a handler that runs in a frame of its own, only the handler ends | ignored (`++ip`) |

In both, the step events and breakpoints are still reported.  The callees of a JIT batch that the native code of such
a frame calls directly run as native code too (they make no event for the callee's block on their next calls, #6289);
the other calls that it makes start with the debugger attached and run on the AST interpreter.  The inline call paths
of the IR interpreter refuse to run while a debugger is attached, as the fast paths of the JIT runtime do, for inline
functions, native leaves and closures.  Inline methods and static methods have no check on their path (a check in
`executeInterpreterInlineIRMethodTarget()` cost +58 Ir per IR method call, as it stopped the inlining of its helpers,
and +10 in its callers): instead, attaching a debugger suspends the eligible inline call states of the call sites
whose callees belong to the debugged Program (`qore_ir_suspend_inline_states()`, #6473), so that the calls of the IR
frame made after the attach are made by the call path and take the requests as the AST interpreter;
`debugger-step-flow.qtest` checks the break and continue in such a callee and its frame.  The suspension only affects
later calls: a call that was already running inline when the debugger attached, or the one call of a thread that
loaded the eligible state just before it was suspended (see the inline call-state publication note in QoreIR.h),
finishes in the caller's IR frame: it is listed on the call stack but has no frame of local variables of its own (the
debugger shows the caller's), a return request returns from the method, and a break or a continue is ignored.  In the IR interpreter, the DebugBlock and DebugStatement
cases take the return when the instruction has no `QoreIRDebugFlowTargets` (`qore_ir_debug_flow_targets()` is null);
an instruction with a record and no target (the body of a handler lowered inline in debug IR) still returns for every
request.  A handler without paths never reports a request (`qore_ir_set_handler_flow()`), so the ScopeExit case gets
none from it.

For a qcc function call made after attachment, the direct-call helper checks the debugger of the callee's Program.
A typed native entry checks the executing Program while obtaining its stack and location slots. When the Program has
a debugger, both paths enter through the ordinary function binder, which exposes the callee's argument locals and
reports its steps. The typed entry identifies the overload by its qualified name and standard native entry point.
`debugger-native-callee-steps.qtest` exercises step, next, and step-out through both call paths.

Native code has no path lowering: `QoreIRToLLVM::lowerFunction()` erases the path blocks (`debug_flow`) when it lowers
the blocks, with no PHIs predeclared for them and no PHI incomings from them, in AOT code and in the closures of AOT
code that the JIT compiles from their debug IR; the step hooks of native code ignore the requests, so no code branches
to a path block.  If the code of a JIT function did, its compilation fails (the function stays on its tier); AOT
lowering makes such a block unreachable.  The IR-level machinery (the CFG edges, `QoreIRDebugFlowCursor`, the patched passes, the
lowering of the paths and their serialization) stays for the debug IR of AOT code.

Tests: `debugger-step-flow.qtest` pins how a frame running at attach takes each request in each mode, checks that the
next call matches the AST interpreter in every mode, that the calls of each kind that such a frame makes after the
attach take the requests, and that the code runs without being compiled again after the debugger detached;
`DebugFlowPathCode.qtest` checks that code lowered from the parse tree has no path blocks; `AOTDebugIRFlow.qtest`
checks the paths of the debug IR of AOT code.  User documentation: `doxygen/lang/229_debugging.dox.tmpl`
(`debugger_running_code`).
