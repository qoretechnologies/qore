# AOT Phase 2B Step 3 — DominatorTree for SSA-direct cleanup

**Status:** Design. Not implemented. Step 5 (2026-04-18, p70-p72) exhausted
what it could contribute; HS compile is 626s (target ≤120s). Further
progress requires this step.

## Problem

`HttpServer::handleRequest` compiled at `-O3` with `QORE_AOT_EH=1` takes
~626s (10m26s) as of slice-10 (commit `68b0b1006`). The IR dump shows:

- 6736 allocas total
- **4630 `%cleanup` allocas** (goal: ~0)
- 2319 invokes (EH path is wired for the call sites that can be)

The cleanup-alloca forest is what LLVM SelectionDAG goes quadratic over.
With 1000+ cleanup allocas in a single function, compile time blows up.

## Why the allocas persist despite Step 5's invoke migration

Every call result goes through `trackResultForCleanup(result, id, func)`.
The SSA-direct path is gated by `canUseSsaCleanup`:

```cpp
// lib/QoreIRToLLVM.cpp:1298
bool QoreIRToLLVM::canUseSsaCleanup(llvm::BasicBlock* current_bb) {
    if (!aot_eh_enabled) return false;
    if (!last_call_was_invoke_eh) return false;
    if (!isOnStraightLineChain(current_bb)) return false;   // ← the blocker
    last_call_was_invoke_eh = false;
    return true;
}
```

`isOnStraightLineChain(bb)` returns `true` only if every step of the idom
chain from `bb` up to function entry has exactly ONE predecessor. Any BB
downstream of a merge point (post-try, post-CondBr convergence, catch
entry, etc.) returns `false`.

`HttpServer::handleRequest` is full of try/catch blocks and conditional
branches. Most of its call sites are NOT on a straight-line chain from
function entry. So `canUseSsaCleanup` returns `false` for them, and
`trackResultForCleanup` falls through to the alloca-creation path.

## Why the p69 attempt failed

Memory entry `session_2026_04_18_p69_aot_ssa_direct_cleanup_infra.md`
footgun (ii):

> per-site SSA-decref preamble BB is NOT SAFE with conservative single-pred
> idom — catch-block pushes fail dominance at post-try merge BBs — LLVM
> verifier rejects; AOTSmoke::testClosureExceptionSafety caught this;
> reverted to promote-at-escape.

The p69 attempt relaxed the push gating and replaced the promote-at-escape
with per-site SSA-decref preamble BBs. The failure mode:

1. A call inside a catch block pushes an entry with `def_bb = catch_bb`.
2. After the try/catch finishes, the next `emitCondBrWithSsaPreamble`
   (triggered by a subsequent `emitExceptionCheck`) tried to emit a
   preamble BB that decref'd the pending entries.
3. The preamble BB is dominated by the current block, which is
   post-try-merge, and thus NOT dominated by `catch_bb`.
4. Verifier rejects: using an SSA reg from `catch_bb` in a block not
   dominated by `catch_bb`.

The conservative single-pred idom couldn't distinguish "catch_bb dominates
post-try-merge" (it doesn't) from "catch_bb dominates the preamble BB"
(it doesn't). So the SSA register was referenced from a non-dominated
location, and the verifier rejected.

## Root cause of the p69 failure: wrong dominance model

The conservative single-pred idom is UNSAFE when combined with relaxed
push gating because:

- With single-pred-only idom, any block with ≥2 preds has `idom=nullptr`,
  making `dominates()` return `false` for any query through such blocks.
- That false-negative is tolerable with strict push gating (values are
  only pushed from "safe" single-pred chains).
- With relaxed push gating (values pushed from any block), the
  false-negative stops matching reality: a value IS defined upstream and
  IS available for decref, but `dominates()` says "no" — so the LP/cleanup
  correctly skips it. But the preamble BB CODE still references the SSA
  value, causing the verifier error.

The fix: use a CORRECT dominance relation (`llvm::DominatorTree`), so
`dominates()` returns TRUE when the SSA value is actually live, and FALSE
only when it isn't.

## Proposed approach

### Option A: LLVM DominatorTree with lazy rebuild

```cpp
// QoreIRToLLVM.h — new members
llvm::DominatorTree dt_cache;
llvm::Function* dt_cache_func = nullptr;
bool dt_dirty = true;

// Force rebuild on every CFG change (BB creation, terminator insertion).
// Simplest: rebuild at start of every dominates() query if dirty or
// function changed.
void rebuildDominatorTreeIfNeeded(llvm::Function* llvm_func) {
    if (dt_cache_func != llvm_func || dt_dirty) {
        dt_cache.recalculate(*llvm_func);
        dt_cache_func = llvm_func;
        dt_dirty = false;
    }
}

bool QoreIRToLLVM::dominates(llvm::BasicBlock* a, llvm::BasicBlock* b) {
    if (!a || !b) return false;
    if (a == b) return true;
    rebuildDominatorTreeIfNeeded(a->getParent());
    return dt_cache.dominates(a, b);
}
```

**The `dt_dirty` flag problem:** mid-lowering the CFG is incomplete.
Blocks are created and terminated as lowering proceeds. Setting
`dt_dirty=true` on every `BasicBlock::Create` + every terminator emit
would require wrapping N LLVM APIs, fragile.

**Simpler:** always rebuild on query. Cost: O(V+E) per rebuild, called
~O(invokes) times per function. For `handleRequest` with ~500 BBs + 1000
invokes, that's 1M total ops = ~10ms. Acceptable.

### Option B: Defer all dominance-dependent work to end of function

1. During lowering: **ALWAYS push to pending_ssa_cleanup** (remove
   `isOnStraightLineChain` check).
2. LP population in `createPerInvokeCleanupLP`: record `{lp_bb, invoke_bb,
   pending_snapshot}` in a deferred list. Create LP with just the
   landingpad + resume placeholder.
3. `emitCondBrWithSsaPreamble`: **remove `promotePendingSsaToAllocas`
   call**. Just do `CreateCondBr`. Pending entries stay alive.
4. At end of function (new `finalizePendingSsaCleanup()`):
   - `llvm::DominatorTree dt(*llvm_func);`
   - For each deferred LP entry, populate the LP's decref sequence using
     DT dominance against the snapshot.
   - For each `Return`'s pre-emitted `emitPendingSsaCleanup` call — walk
     back and inject decref calls there.
   - For each SSA entry that IS NOT DOMINATED by any cleanup site it
     should have reached (leaked value): promote to alloca in entry
     block, insert store at def site, rewrite cleanup sites.

Pros: conceptually clean. Cons: big refactor touching many code paths.

### Option C: Scope-based pending_ssa_cleanup

1. Introduce `scope_markers` — a stack of `size_t` recording
   `pending_ssa_cleanup.size()` at scope entry.
2. Push scope marker on:
   - Entry to a try block.
   - Entry to a catch block.
   - Entry to a branch arm (at `CreateCondBr` when emitted in IR).
3. On scope normal-exit (e.g., try-end, catch-end, branch arm end):
   - Promote entries pushed since the scope marker to allocas (so they're
     visible past the merge).
   - Pop the scope marker.
4. On scope exception-exit: LP handles the entries pushed in scope;
   promote nothing.
5. Keep `canUseSsaCleanup`'s straight-line check? Or relax? The scope
   marker makes either workable; scope-based promotion is the key.

Pros: no DT needed; conceptually matches the "stack-scoped" mental model.
Cons: needs careful mapping from Qore IR block structure to scopes.
Qore IR doesn't have explicit try/catch BBs — it uses `inst->exception_
target` on instructions. Would need to detect scope entry/exit from block
topology.

### Recommendation

**Start with Option A** (LLVM DominatorTree lazy rebuild). Smallest code
change, smallest risk, known primitive. Expected effects:

1. `canUseSsaCleanup`: relax `isOnStraightLineChain` to
   `dt.dominates(entry, bb) && dt.dominates(bb, somewhere_that_decrefs_it)`
   — OR just drop the check entirely. Let DT filtering at LP/cleanup
   sites catch divergence issues correctly.
2. `emitCondBrWithSsaPreamble`: remove the `promotePendingSsaToAllocas`
   call. Just do `CreateCondBr`. Verify verifier is happy.
3. `createPerInvokeCleanupLP` + `emitPendingSsaCleanup`: already filter
   via `dominates()`; no change needed, but they'll now use the correct
   DT-based dominance.

If the verifier rejects something, fall back to Option C's scope-based
promotion as the fix. DT still useful for CORRECTNESS checking.

## Implementation plan (Option A)

### Stage 1: Replace `dominates()` with DT-backed version

- Add `llvm::DominatorTree dt_cache;`, `llvm::Function* dt_cache_func;`,
  `bool dt_dirty = true;` to `QoreIRToLLVM`.
- Reset all three in `lowerFunction` (near the existing `entry_block_for_
  idom = nullptr` reset).
- New helper `rebuildDominatorTreeIfNeeded(llvm::Function*)` — always
  rebuild on call (or caller can set `dt_dirty=false` after rebuild to
  skip until next mutation). Simplest: always rebuild.
- Change `dominates()` body to call `rebuildDominatorTreeIfNeeded(a->
  getParent())` then `return dt_cache.dominates(a, b)`.
- Remove `isOnStraightLineChain`, `getOrComputeImmediateDominator`,
  `updateImmediateDominator` and the `immediate_dominator` map (now
  dead code).
- BUT: `canUseSsaCleanup` calls `isOnStraightLineChain`. Need to decide:
  drop the check entirely? Or replace with a DT-based equivalent?

  Drop entirely for now; test + measure.

### Stage 2: Remove `promotePendingSsaToAllocas` from `emitCondBrWithSsaPreamble`

- In `emitCondBrWithSsaPreamble`: delete the `promotePendingSsaToAllocas`
  call. Just emit `CreateCondBr(cond, exception_target, normal_target)`.
- Build + baselines. If verifier rejects, investigate with
  `QORE_DUMP_IR_ON_VERIFY_FAIL=1`.

### Stage 3: Measure HS compile

```bash
time QORE_AOT_EH=1 QORE_AOT_EH_MAX_CALLS=0 LD_LIBRARY_PATH=build \
    ./build/qcc -O3 -m -o /tmp/hs.qmod qlib/HttpServer.qm

QORE_AOT_EH=1 QORE_AOT_EH_MAX_CALLS=0 QORE_DUMP_IR_BEFORE_OPT=1 \
    LD_LIBRARY_PATH=build ./build/qcc -O3 -m \
    -o /tmp/hs.qmod qlib/HttpServer.qm 2>/tmp/hs.ll
grep -c '= alloca' /tmp/hs.ll
grep '= alloca' /tmp/hs.ll | grep -oE '%cleanup' | wc -l
```

Expected (if successful): `%cleanup` allocas drop from 4630 toward ~0,
HS compile time drops from 626s toward 120s.

If instead verifier fails, look at the stack trace / error message to
identify the verifier failure mode. Likely one of:
- SSA value from branch arm referenced at merge (fix: Option C scope).
- PHI node with incorrect predecessor (fix: verify preamble BB's pred
  matches).

### Stage 4: Full test sweep (Phase D)

- 4 baselines plain + EH.
- p64-p67 regressions.
- Representative qmod qtests (Logger, HttpServer, OpenApi3, Swagger,
  RestClient, RestClientIo, HttpClientIo, DataProvider, Mapper,
  CsvUtil, Util, RestHandler).
- Valgrind AOTSmoke.

### Stage 5: Phase E (flip default on)

Once Phase D green + compile target met:
- `aot_eh_enabled = !getenv("QORE_AOT_NO_EH") && aot_mode;`
- Remove `QORE_AOT_EH_MAX_CALLS` threshold.
- Optional: replace `QoreJITException` → `AbstractException` at
  `include/qore/intern/Function.h:782`.

## Open questions for next session

1. **Will `llvm::DominatorTree` tolerate un-terminated blocks mid-lowering?**
   Expected: yes (unterminated = leaf in DT), but verify with a small test.

2. **Can we drop `canUseSsaCleanup`'s straight-line check entirely, or do
   we need a lighter DT-based equivalent?**
   Start with drop; add back if divergence issues surface.

3. **`AOTSmoke::testClosureExceptionSafety`** is the test that caught the
   p69 issue. It's the canary. Run it FIRST after each stage to catch
   verifier failures early.

4. **Compile-time cost of always-rebuild DT.** For small functions it's
   trivial; for `handleRequest` it may be noticeable. Consider:
   - Only rebuild on demand (at start of `createPerInvokeCleanupLP` and
     `emitPendingSsaCleanup`).
   - Between rebuilds, reuse the cached tree.
   - Invalidate explicitly on scope-crossing operations.

## References

- Memory: `session_2026_04_18_p69_aot_ssa_direct_cleanup_infra.md` (Steps
  1-2 infrastructure + p69 footgun that motivated this step).
- Memory: `session_2026_04_18_p70_aot_step5_phase_a_iterator_pilot.md`
  (Step 5 progression + slice-10 post-mortem).
- Source: `lib/QoreIRToLLVM.cpp:1271` (`isOnStraightLineChain`), `:1298`
  (`canUseSsaCleanup`), `:1317` (`createPerInvokeCleanupLP`), `:1446`
  (`emitCondBrWithSsaPreamble`), `:1540` (`dominates`), `:1569`
  (`trackResultForCleanup`).
- Header: `include/qore/intern/QoreIRToLLVM.h` — search `pending_ssa_
  cleanup`, `immediate_dominator`, `entry_block_for_idom`.

## Starting commit

`68b0b1006` — Step 5 fast-path call-site migration complete. All Step 5
slices landed.
