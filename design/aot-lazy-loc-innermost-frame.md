# AOT lazy exception locations — innermost-frame mechanism (Step 6 enabler)

Status: **DONE** (2026-06-25). Lazy is now the default and the eager per-line AOT
updater is removed (Step 6). Commits: mechanism `e6ff2d78d`+`0c220a9ed`, lazy-default
`4ccda3d10` (Phase A), eager removal `e6f42799e` (Phase B). Opt out with
`QORE_AOT_LOC_NO_LAZY`; `--strip-debug-info` retains the eager updater (no DWARF).
Full core suite 789/789 in default mode AND with the eager updater removed; callstack
tests intact; AOT codegen emits 0 per-line location calls (was 3).

## Problem

The lazy path resolves an exception's source location from the innermost **AOT**
frame found by unwinding at throw. That is correct only when the innermost
**user-code** frame is actually AOT. In mixed callstacks where AOT code calls
AST-interpreted / IR-interpreted / JIT-compiled user code (closures, callbacks,
method dispatch — often through *unexported* libqore bridge functions) that then
throws, the true location is in the inner non-AOT frame, which the unwind skips,
so lazy mis-attributes to the outer AOT caller.

Measured: ~0.3% of AOT exceptions (296 / ~100k in the full-suite gate). The eager
per-line updater tracks these correctly (AST/IR/JIT all keep `runtime_loc` current
per statement/line), so today we keep eager and ship lazy opt-in. **Step 6** (delete
the eager AOT updater for the perf win) requires deciding, at throw, **"is the
innermost user frame AOT?"** robustly.

### Why the obvious approaches fail

- **Barrier by symbol** (defer if a known interp/dispatch frame is inner to the AOT
  frame): the AOT→non-AOT bridge is frequently an *unexported* libqore static
  function (no dynamic symbol to name/range). Verified via stack dumps
  (e.g. DataProvider closure case: bridge frame has `sym=?`). Not viable.
- **JIT segment-snapshot** (defer if an inner frame is in no loaded ELF segment =
  anonymous JIT memory): correct but only covers *true JIT*; the bulk of the tail is
  AST/IR-interpreted user code running inside libqore frames (in segments). Shipped
  (`219010537`) as it is safe + correct for JIT, but it barely moves the tail.
- **dladdr / dl_iterate on the unwind path**: deadlocks against a concurrent
  `dlopen` (both take `dl_load_lock`). Confirmed hang. Forbidden on the throw path.
- **SP/CFA frame-address comparison without lifecycle** (store
  `__builtin_frame_address` when the eager updater runs; compare at throw): a stale
  marker from a *returned* frame reuses the same stack address as a live inner frame,
  so a range check cannot tell them apart. **Staleness is the core issue.**

## Design: `runtime_loc_sp` — a stack-position shadow of `runtime_loc`

Maintain, per thread, the C++ stack-frame address of the **innermost live frame that
is executing the Qore statement `runtime_loc` points at**, with the *exact same
lifecycle* as `runtime_loc`. Because `runtime_loc` is already correctly save/restored
at call boundaries, shadowing it eliminates the staleness that killed the naive
SP/CFA attempt.

```
ThreadData {
    const QoreProgramLocation* runtime_loc;   // existing
    uintptr_t                  runtime_loc_sp; // NEW: frame addr of the frame owning runtime_loc (0 = none)
}
```

### Maintenance (set + save/restore)

`runtime_loc_sp` is written **only** by the non-AOT eager updaters and is
save/restored wherever `runtime_loc` is:

1. **IR interpreter** — `QoreIRInterpreter::execute` per-instruction loop already does
   `*rl_cache.loc_ptr = inst->loc`. Add `*rl_cache.sp_ptr = __builtin_frame_address(0)`
   (extend `RuntimeLocationCache` with `uintptr_t* sp_ptr`).
2. **JIT** — codegen already inline-stores the loc pointer per line
   (`QoreIRToLLVM` JIT branch). Add a store of `llvm.frameaddress(0)` to a cached
   `&td->runtime_loc_sp` (the reverted `qore_rt_get_loc_frame_ptr` plumbing).
3. **AST interpreter** — `CodeEvaluationHelper` / `update_runtime_statement_location`
   (thread.cpp + Function.cpp) set `runtime_loc` per statement. Set
   `runtime_loc_sp = __builtin_frame_address(0)` at the same point (centralized).
4. **AOT** — does **not** set it (after Step 6 it has no per-line updater). While an
   AOT frame executes, `runtime_loc_sp` retains the value of its nearest non-AOT
   ancestor — which is *outer* to the AOT frame. That is exactly what makes "AOT
   innermost" detectable (see decision rule).
5. **Save/restore** — the ~8 `runtime_loc` writers in thread.cpp (`swap_runtime_location`,
   `swap_runtime_statement_location`, `update_runtime_statement_location`, …) save and
   restore `runtime_loc`; extend each to carry `runtime_loc_sp` alongside (add an
   `old_sp` out-param to the swap variants; the restore counterparts already exist as
   paired calls). This guarantees that on return from a callee, `runtime_loc_sp` is
   restored to a **live ancestor's** frame — never a dead/reused address.

The save/restore is what defeats staleness: a returned frame's `runtime_loc_sp` is
overwritten by the restore at the call boundary, so the value is always the SP of a
frame currently on the stack.

### Decision rule (at throw)

In `qore_aot_resolve_throw_location` (called from `get_runtime_location_safe`, the sole
exception-location capture point):

```
unwind → innermost AOT frame's CFA  = aot_cfa   (0 if none found)
sp = td->runtime_loc_sp

if aot_cfa == 0:            return null    // no AOT frame → eager (non-AOT exception)
if sp == 0:                return lazy     // no non-AOT frame ever ran → AOT innermost
if aot_cfa <  sp - MARGIN:  return lazy     // AOT frame deeper than the live non-AOT frame → AOT innermost
else:                       return null     // a non-AOT frame is at/deeper → eager
```

Stacks grow down, so *inner = smaller address*. `aot_cfa < sp` means the AOT frame is
inner to the live non-AOT frame → AOT is the innermost user frame.

`MARGIN` absorbs the small constant offset between `_Unwind_GetCFA` (CFA) and
`__builtin_frame_address` (frame pointer) and the rare adjacent-frame case; tuned
empirically (a few frame-words). Refinement if MARGIN proves fragile: during the
unwind, also capture the CFA of the frame **immediately inner** to the AOT frame and
require `sp` to lie outside the AOT frame's own `[aot_cfa, inner_cfa)` extent.

### Why this is correct in the canonical cases

- **AOT leaf throws** (builtin called from AOT raises): `sp` = an outer non-AOT
  ancestor (or 0); `aot_cfa` inner → lazy. ✓ (fixes today's IMPROVE + OK cases)
- **AOT → interp/JIT callee throws**: the callee's eager updater set `sp` = callee
  frame (inner); `aot_cfa` = outer AOT caller → eager. ✓ (fixes today's REGRESS tail)
- **interp → AOT leaf throws**: `sp` = the interp caller (outer); `aot_cfa` inner →
  lazy. ✓
- **Nested AOT→interp→AOT**: each boundary save/restores `sp`; the innermost frame's
  mode wins by the address comparison. ✓

## Validation (the gate IS sufficient — earlier worry was wrong)

The earlier concern was that with the eager updater still active, false-defers are
masked. But we validate the **decision**, not the final output, against the existing
eager-vs-lazy divergence:

Run the full-suite **gate** (eager active) with the mechanism wired to log its choice:
- Cases that are **REGRESS?** today (eager≠lazy, both real, lazy=AOT-caller) MUST flip
  to **DEFER** (mechanism chose eager). Any remaining REGRESS? = a mechanism gap.
- Cases that are **IMPROVE** today (eager degenerate, lazy=real) MUST stay **lazy**
  (mechanism chose lazy). A drop in IMPROVE = the mechanism is over-deferring (false
  "non-AOT innermost"), i.e. losing real fixes.
- **OK** cases (eager==lazy): either choice is fine.

**Target: REGRESS? → 0 and IMPROVE preserved (~31k).** Then, separately, run the suite
with lazy *active* (`QORE_AOT_LOC_LAZY=1`) and confirm 789/789 (no assertion breakage).
Only after both, do Step 6 (delete `emitRuntimeLocationUpdate`'s AOT branch) and a
hot-path benchmark.

## Risks / open items

- **Fast-call paths bypass `CodeEvaluationHelper`** (`qore_rt_call_fast` et al.,
  noted in JITRuntime.cpp:6667). Confirm they don't run interp/JIT user code without
  the `runtime_loc`/`runtime_loc_sp` save/restore. For AOT→AOT fast calls the marker
  correctly retains the outer ancestor (no action needed); the worry is any fast path
  that enters interp/JIT — audit + cover.
- **MARGIN robustness** — empirically tune; fall back to the inner-CFA-extent refinement
  if needed.
- **`--strip-debug-info`** (`QORE_AOT_NO_DEBUG_INFO`) emits no DWARF → empty pc_loc_map
  → lazy no-ops. Step 6 must keep the eager AOT updater when debug info is stripped
  (gate the removal on debug-info-present), else stripped builds lose locations.
- **Exception safety** — `runtime_loc_sp` restore must run on the throwing path; since
  it is bundled into the existing `runtime_loc` restore (already exception-safe via the
  paired swap calls / `CodeEvaluationHelper` dtor), this is satisfied by construction.

## Implementation status (2026-06-25, committed e6ff2d78d + 0c220a9ed)

Implemented and validated. Full core suite 789/789 under the gate; the gate logs the
mechanism's decision (LAZY/EAGER) alongside the eager-vs-lazy tag. Decision tally over
~96k AOT-frame exceptions:

| tag       | decision=LAZY | decision=EAGER | meaning |
|-----------|---------------|----------------|---------|
| OK        | 61045 (all)   | 0              | identical either way ✓ |
| IMPROVE   | 32073 (kept)  | 4256           | EAGER = over-deferred, *lost* improvement (safe, not a regression) |
| REGRESS?  | ~110          | ~1040 (fixed)  | LAZY = residual mixed-stack bug |

vs the prior barrier/segment approach (296 REGRESS): all OK now resolve LAZY, ~91% of
REGRESS? correctly defer, ~88% of IMPROVE preserved. `exception-location.qtest`
(207 assertions) passes with `QORE_AOT_LOC_LAZY=1`. valgrind clean.

Set once-per-call (not per line): IR at execute() entry (FrameGuard save/restores);
JIT in the prologue via llvm.frameaddress (evalTiered + fast-call SpGuard save/restore);
AST per-statement via RuntimeConfigLocationHelper.

**Residual (~110 REGRESS?->LAZY + ~4256 IMPROVE->EAGER):** both are staleness — a
non-AOT callee set runtime_loc_sp inner and an execution path returned to AOT without
the save/restore guard (REGRESS?->LAZY = sp not set inner when it should be; IMPROVE->
EAGER = stale inner sp left after a non-AOT callee returned). Remaining uncovered paths
are other fast-call/closure-dispatch entries (qore_rt_call_method_fast,
qore_rt_call_closure_fast, batch-inlined JIT calls) that run non-AOT code without an
SpGuard. Closing them to 0 (the Step 6 gate) means auditing those dispatchers and adding
the same save/restore guard. The JIT fast-call guard (qore_rt_call_fast exec_fn) was
added; the others remain.

## Implementation order

1. `RuntimeLocationCache` += `sp_ptr`; `ThreadData::runtime_loc_sp`; thread.cpp
   getters; JIT `qore_rt_get_loc_frame_ptr` (restore the reverted plumbing).
2. Set `runtime_loc_sp` in the IR-interp loop, AST `update_runtime_statement_location`,
   and JIT codegen.
3. Save/restore `runtime_loc_sp` in the thread.cpp swap/update functions.
4. Decision rule in `qore_aot_resolve_throw_location` + gate instrumentation that logs
   the chosen branch.
5. Validate (gate: REGRESS?→0, IMPROVE preserved; lazy-active 789/789).
6. Step 6: remove the AOT eager updater (gated on debug-info-present); benchmark.
