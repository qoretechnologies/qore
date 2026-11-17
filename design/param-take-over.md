# Parameters that take their arguments over

Copyright (C) 2026 Qore Technologies, s.r.o.

The AST interpreter moves a call's evaluated arguments into the callee's parameters (`UserVariantBase::setupCall()`):
a temporary argument (`f(new T())`, `f({"o": new T()})`) is then held by the parameter only, and a write to the
parameter (`a = NOTHING`, `remove a`, `h.o = x`) runs the destructor of the object at the write, before the callee
continues.  Every execution mode must do the same (issue #5958; handlers: #6004), and a call that gives the callee no
temporary must cost exactly what it costs on develop, in every mode (David, 2026-11-15).

`qore_ir_compute_param_take_over()` (`lib/QoreIRVerifier.cpp`, compiled in `lib/QoreIRTakeOver.cpp` outside the
single compilation unit) sets `QoreIRFunction::param_take_over` when a parameter that can hold an object is written:
a write that the IR records, one in a closure that captures it, one through a reference that the function creates and
one in an `on_exit`/`on_error`/`on_success` handler.  The function is walked once for all of its parameters.

## Whether a call gives a temporary: per instruction or per execution

Both, at two levels; the code is in `lib/QoreIRInterpreter.cpp`:

- `QoreIRInstruction::interpreter_gives_call_args` is fixed for the instruction when the caller's IR is prepared
  (the loop after `buildValueUseCounts()`): it is set if an argument is in `consumed_call_args`, the temporaries whose
  release at the call's return can be observed (call results, list and hash literals and other observable temporaries,
  reads of variables that the statement can write later).  An instruction without it never gives a temporary whose
  release can be observed.
- Which executor runs is decided for each execution.  `CallDirect` tests `has_last_use_args =
  hasLastUseCallArgSlots()` (use counts and `needsRef()`, not the flag) and runs `irCallDirectGiven()`, which gives
  every last-use slot (`giveLastUseCallArgSlots()`), including the frame's own reference of a read of an IR-only
  local: `fh(h, i)` with a variable `h` runs the code instantiated for `GIVEN`, although nothing it gives can be
  observed (the variable holds the value for the whole call; an IR-only local cannot be written through a reference or
  a closure).  The other calls test `interpreter_gives_call_args && hasConsumedCallArgs()`.
- The inline state of a call site (`inline_ir_state` and its `cached_*` payload) is per instruction, whichever
  executor computes it first (`ensureInterpreterInlineIRFunctionState()`,
  `ensureInterpreterResolvedInlineIRCallState()`); the inline cache of closures is a `thread_local` of each
  instantiation of `tryExecuteInterpreterInlineIRClosure<GIVEN>()`.

So a rule keyed on the per-instruction flag inside the computation of the per-instruction state is deterministic, and
one keyed on the `GIVEN` code is not: e317abfe88 rejected inlining for every caller of a function with
`param_take_over`; restricting that to the given unit's code was wrong because the given unit also computes the state
for executions that give nothing observable (`fh(h, i)`).

## The IR interpreter's inline calls

`interpreter_gives_call_args` is a byte: `QORE_IR_GIVES_CALL_ARGS` for a call that gives consumed arguments, with
`QORE_IR_GIVES_CALL_TEMPS` if one of them is not a read of a plain local (not through a reference, not closure-bound,
in a function that creates no reference) that nothing writes from the read to the call in its block (`f(o, o =
NOTHING)` writes it).  Such a local holds the value for the whole call, as no callee can write it, so a parameter
that takes the read over cannot release the value observably: the reads of top-level locals, which are not IR-only and
so are consumed (`read_needs_release_at_return()`), give the bit 0 only.  A call site is not
inlined if `callee_ir->param_take_over && (inst->interpreter_gives_call_args & QORE_IR_GIVES_CALL_TEMPS)` (closures,
whose inline cache has no instruction: `GIVEN && callee_ir->param_take_over`).  A site that gives no such temporary is
inlined exactly as on develop; only the decision of the state, made once per site, changes.  A giving site is made by
the call path, which must then bind the parameter so that it holds the only reference (below).

## The storage of the parameter

On develop, a written parameter that can hold an object is IR-only (an interpreter slot or a native alloca) and has a
fast entry and the batch direct ABI (`qore_ir_fast_entry_param_is_private()`).  e317abfe88 moved it to the runtime
stack and removed its fast entry for every caller; that is what cost the callers that give nothing (review of
e317abfe88: `fh(hash<auto>)` +33% per call in IR mode, `fa(auto)` +47%).  With the storage as on develop, a parameter
holds the only reference to a given temporary when its binding takes the caller's reference over:

- the direct parameters of `QoreIRInterpreter::execute()` with `IRDirectParams::arg_cleanups` (the consume path of
  `qore_rt_call_fast_t<true>()` in `lib/JITRuntime.cpp`): the slot takes a reference, then the caller's is released
  (`qore_rt_clear_arg_cleanups()`): the slot holds the only one;
- the binding from the runtime stack does not: `setupCall()` and `instantiateFastCallParams()` move the argument into
  the parameter's runtime variable, then the IR interpreter's first `LoadLocal` (the cache miss path) and the native
  normal entry (`emitLocalCacheLoad()` of a pre-instantiated parameter in `QoreIRToLLVM`) take a reference of their
  own, and the runtime variable keeps its reference until the call returns, so the write releases only the copy;
- the native fast entry takes a reference of its own at entry (`qore_rt_incref` in the prologue that binds
  `fast_entry_args`), and its caller keeps its own until the call returns.

The binding from the runtime stack is shared by every caller that reaches the callee through it: the AST interpreter
(the AST tier of tiered execution, statements delegated to the AST), the IR interpreter's call path
(`evalFunctionTmpArgs()`), and an IR caller of a native callee (`qore_rt_call_fast()`).  The callee cannot tell a
moved temporary from a copy of the caller's variable there, so the binding either moves the value out of the runtime
variable for every such caller (correct for both: the caller's variable, if any, still holds its own reference) or
for none.  Moving costs a call that gives nothing through that path a different binding (a move instead of a
reference: expected to be no more instructions, to be measured), never the inline, direct-parameter or fast-entry
calls.  A native caller that gives a temporary to a fast entry needs an entry that takes it over: the consume path
(binding from the runtime stack with the move) or a second fast entry whose prologue does not take a reference for
these parameters (a second compilation of the body).

The test `examples/test/ir/ParamReassignRelease.qtest` shows the gap with the storage as on develop and the per-site
inline rule: the object parameters (not IR-only) pass, the container parameters (`hash<auto>`, `list<auto>`, typed
hashes, the slices and extracts given to them) fail in the IR, JIT and tiered modes, through the binding from the
runtime stack.

## Explicit type arguments

A method call with explicit type arguments on a receiver whose class is known at run time only (`auto o = c;
o.n<int>(new D())`) is made by `dispatch_method_on_object()` with an argument list (the
`!explicit_type_param_instantiation` guards of `qore_rt_dot_eval_method_direct_impl()` and of the pseudo-method paths
in `lib/JITRuntime.cpp`), whose parameters take references of their own: it does not take its temporary arguments over
in the IR, JIT and tiered modes, on develop (82c07846f2) as on this branch; a call on a receiver of a known class
(`c.n<int>(new D())`) does.  Documented in #5958 as a gap.

## The binding from the runtime stack (option 1, David, 2026-11-15)

A parameter that takes its argument over is bound so that it holds the only reference to a temporary argument in
every binding, with no change to the inline, direct-parameter and fast-entry calls:

- the IR interpreter keeps such a parameter in IR storage, as on develop, in every frame; a frame whose parameters are
  bound on the runtime stack (no `IRDirectParams`) moves the value of each such parameter from its variable into its
  slot (`takeOverRuntimeStackParams()`, the list `QoreIRFunction::interpreter_take_over_param_moves` that
  `buildInterpreterAnalysis()` computes once): the slot then holds the variable's reference, which is the only one to
  a temporary that the caller gave (the AST interpreter, a giving call path), and a write releases it; a frame whose
  caller gave nothing holds a reference that the caller's variable also holds, so nothing observable changes, and
  its writes are those of an IR-only local, as on develop.  A variable that holds a reference, a weak reference, a
  static assignment or a value that is not a node keeps its value, which the first load reads as on develop.  The
  move is tested once per frame bound from the runtime stack (`!direct_params`, then the list's emptiness), the only
  change on the paths that give nothing.  The frame of a handler shares its parent's slots and moves nothing.  Until
  1d03004059 the parameter used its runtime variable as storage in every frame bound from the runtime stack
  (`interpreter_locals_ir_only` clear), which made each write of a caller that gives nothing slower (review of
  1d03004059: `fl(hv)` with 50 writes of `h.k`, +875 instructions per call in IR mode, +1078 in tiered);
- the normal entry of native code (`QoreIRToLLVM::preCreateLocalAllocas()`) loads such a parameter with
  `qore_rt_take_param()` (`_aot`), which releases the runtime variable's reference once the cache has its own,
  instead of `qore_rt_load_local()`: the entry's cleanup slot then holds the only reference, a write releases it, and
  the variable is empty when the call uninstantiates it.  The fast entry is unchanged.  Two cases cannot occur, and
  are asserted: a taken parameter with a reference flag (`qore_rt_load_local_raw()`: an untyped or `auto` parameter,
  which can be given a reference, is never IR-only when it is written, `computeIROnlyLocals()`), and a deoptimization
  of a function that took a parameter (the deoptimization runs the function again from the runtime variables, which
  the take emptied): a function with a guard that has a deopt target takes no parameter over
  (`qore_ir_function_may_deopt()`, checked only for a function with parameters that take their arguments over); a
  debug build asserts that no such function has one, so that the debug suites find a lowering that makes it, and a
  release build calls it without the take-over and reports nothing (`printd()` is compiled only in a debug build).
  The IR interpreter makes the same check for its frame's move (`buildInterpreterAnalysis()`: a release build then
  leaves the parameters on their variables until the frame loads them).  No lowering makes such a guard today: only
  `GuardNotNothing` is made, and it has a deopt target only in a try block (or the release block of a replaced
  return), where the values that it would check are made by invokes, which end their block, so
  `maybeInsertNotNothingGuard()` makes none for them (checked
  with `QORE_AOT_DUMP_IR` on `ParamReassignRelease.qr` and on probes of slices, ranges, member and key reads in try
  blocks).  The hand-back helpers of 1d03004059 (`qore_rt_return_param()`) and the `_raw` takes were removed;
- the take-over of a callee's AOT IR, which does not set `param_take_over`, is cached in
  `QoreIRFunction::take_over_params_state` for the fast-entry gates of its callers; every change of the write
  inventories that it is computed from (`computeIROnlyLocals()`, the scalar replacement of containers in
  `QoreIRAnalysis.cpp`) resets it, so a later gate computes it again from the inventories as they are;
- a native call that gives the callee a temporary argument (`QoreIRToLLVM::callGivesArgs()`, the test of
  `buildArgCleanupArray()`) does not call the fast entry of a callee with `param_take_over`
  (`qore_ir_fast_entry_refuses_given_args()` in the fast-entry gates): it is made by the consume path, which binds
  from the runtime stack.  No second fast entry is compiled.

The helpers are compiled in `lib/JITRuntimeTakeOver.cpp`, outside the single compilation unit.  A range slice and an
`extract` give a new sequence, which a call consumes as a call's result (`qore_ir_is_sequence_temp()`).

## Measurements (2026-11-15)

callgrind, cuttah Release, per-call slope (two sizes, `scripts/an.py`), `QORE_JIT_SYNC_COMPILE=1`, `qbuild --count`;
ast, ir, jit, tiered, tiered with thresholds 1 (tlow), qcc -O0 and -O3.

- Option 1 at 8d373dfedd against develop 82c07846f2 (results-n): the reviewer's rows (`fh(hash<auto>, int)`,
  `fl(list<auto>, int)`, `fa(auto, int)`, `fi(int, int)`, `C::m(hash<auto>, int)`), the variable-argument writing
  rows (`f_var_hwrite`, `f_var_lwrite`, `f_var_awrite`, `m_var_hwrite`), `f_tmp_read` and `m_tmp_read`: the code
  delta is 0 in every row; the only deltas are in libc alone (±7..21 per call, the placement class).  e317abfe88 had
  `fh` +4775 per call (+21.7%) and `fl` +4290 in IR mode.  `f_tmp_write` (a giving site) is -246 in IR mode, in libc.
- Against batch R (8a1ac4346b) after the merge, at 969e33ab5d (matrix q, run 0 of every case; `table-q-r0.txt` on
  cuttah): no non-giving row costs more code; the method calls that give nothing are 4 instructions per call cheaper
  in jit, tiered and tlow (`m_four`, `m_int`, `m_nine`, `m_var_hwrite`, `rv_cm`, ...) and `m_poly` 17.5..19.5 in ir,
  tiered and tlow: `qore_rt_dot_eval_method_direct_impl<false>()`, the instantiation that only the calls that give
  nothing call, is specialized for no given arguments (impl 91 -> 89, entry 9 -> 7 per call in `m_four` jit).  The
  calls that give a temporary to a callee whose parameter takes it over pay: `*_try_w`, `s_tmp_write`,
  `ref_tmp_write` +1067..1073 per call in ir and tiered (the call path instead of the inline call), +167..187 in
  jit, tlow and qcc, `f_try_w` qcc -O0/-O3 +4352/+4303 (+57%: the consume path instead of the fast entry).
  `m_poly` jit is -437 (the call of a method of another class that gives its arguments takes them over: fewer
  allocations), as against develop since e317abfe88.
- After the review of 1d03004059, against batch R on spinster (callgrind, Release, per-call slope of two sizes,
  `QORE_JIT_SYNC_COMPILE=1`, R 8a1ac4346b and the branch built on the same host; 17 cases including the reviewer's):
  the frames bound from the runtime stack that are given nothing write the parameter in IR storage again: `b_loop`
  (`fl(hv)`, a default argument, 50 writes of `h.k`) is -184 per call in IR mode (code -823, libc +639) and -19 in
  tiered and tlow, against +875 and +1078 at 1d03004059; `b_dflt`, `b_cref`, `b_meth` -19..-32 in ir, tiered and
  tlow; jit and qcc 0.  Every non-giving row (`rv_fh`, `rv_fl`, `rv_cm`, `f_var_hwrite`, `f_var_lwrite`, `f_int`,
  `f_tmp_read`, `m_tmp_read`) is 0 or cheaper in code in every mode; `rv_fa` ir/tiered/tlow +40..44 is libc only
  (+65, code -21..-25).  The move replaces the first load's reference of the IR frame (fewer instructions than the
  reference and its release).  `b_ast` (a giving site) is -1902 in IR mode and +1078 in jit, all of it in libc.
- After the second review (19ee4e85bc, ccb1e54b21; spinster, against R): `b_loop` writes `h.k` (an lvalue path), so
  its parameter is not IR-only and the frame's move does not run for it; the rows whose parameter is IR-only and
  reassigned, bound from the runtime stack (a default argument): `b_reloop` (read and reassigned in a loop, called
  with a variable) -1332 per call in IR mode, -346 in tiered and tlow, -327 in jit and qcc; `b_reone` (written once,
  never read) and `b_retop` (the same from top-level code) **+126 in IR mode** (the move's lookup of the runtime
  variable, which R never makes for a parameter that the frame does not read: `takeOverRuntimeStackParams()` +123),
  -346 in tiered and tlow, -327 in jit and qcc (one lookup instead of a load and a second lookup); `b_retmp` (the
  giving form) the same.  The other rows are unchanged from the previous measurement.  Whether the caller gave the
  frame a temporary is not known in the frame without that lookup.
- Decision (prof, 2026-11-16): the IR cost is accepted (option a): about +125 instructions per call in IR mode only,
  for a parameter that takes its argument over, is IR-only, is written before it is read (or never read) and is
  bound from the runtime stack (no direct parameters, e.g. a default argument); jit, qcc and tiered are cheaper than
  on R for the same rows (-327..-348).  The alternative, a bit with which a giving caller tells the frame that it
  gave a temporary (option c), cannot leave the path of a call that gives nothing exactly as on R: the AST
  interpreter's call (`UserVariantBase::eval()`, `setupCall()`, `evalTiered()`) gives its evaluated arguments to
  the parameters and is the same code as the IR interpreter's call that gives nothing
  (`qore_rt_call_function_direct()` → `evalFunctionTmpArgs()` → `UserFunctionVariant::evalFunction()` →
  `UserVariantBase::eval()` → `setupCall()` → `evalTiered()`), so the bit would be a store on that shared path
  (top-level code, which tiered execution runs on the AST tier, gives temporaries to IR callees:
  `ParamReassignRelease.qr`'s round loop); and the frame would need a test of the bit
  where it has none (`execute()` has no branch on the path of a frame bound from the runtime stack that the bit
  could share: a sentinel `IRDirectParams` costs the frames with direct parameters and no arguments a jump).
- Standard call and map rows against R (spinster, same build): `f_four`, `m_four`, `s_four`, `cl_int`, `map_cb` are
  0 or cheaper in every mode (ir -14..-105, tiered/tlow -9..-105, jit 0..-4, qcc 0; `cl_int` qcc -O0 +8.6 is libc
  only, +8.4).

## The values of functional operators and reads through references (issue #6262)

The AST interpreter gives a parameter these arguments with no other reference, so the parameter's write destroys them:

- the values of map, map with select, hash map and hash map with select, which the operator pushes into its result
  container: the IR lowering releases the value of the operator's expression once it is stored
  (`QoreIRLowering::releaseStoredOperatorValue()`, ReleaseOwnedTemp; nothing is emitted for a value of a type that
  holds no object, so the maps of scalars run as before); the IR interpreter's Phi of the operator takes its result
  container over (`CreateSizedList`, `MakeHash` and the list and hash literals in `buildValueUseCounts()`);
- over a single value, map, select and map with select give the value itself, which the compiled modes read from the
  result list: it gets a reference of its own and the list is released (`ownUnwrappedOperatorValue()`);
- a fold releases its source when it has run, also a source that is a conditional expression (`?:`, `??`, `?*`) of
  temporaries, or the list that the stages of a lazy pipeline gave it (`FunctionalSourceCapture::intermediate`), and
  its value, which `lowerWithSourceRelease()` gives a reference of its own (RefSelf), is a temporary that the IR
  interpreter's call gives (`def_by_call`), as native code gives its cleanup slot; the accumulator Phi that is its own
  incoming value on the loop's back edge does not keep the Phi after the loop from taking it over;
- the value of a fold or of such a conditional expression is released once a list or hash literal holds it, as a
  temporary is (`qore_ir_expr_gives_owned_value()`);
- a value read through a reference by a local with a reference flag (an untyped or `auto` parameter given `\v`) is
  held in the slot of its read site in native code: a call gives that slot when it holds the value
  (`QoreIRToLLVM::givenReferenceReadSlot()`, a select between the slot and a slot that holds NOTHING in
  `buildArgCleanupArray()`), as the IR interpreter gives the read's cleanup entry.  Such a call is made as a call
  that gives its arguments (the consume path), also when the local holds no reference, unless it calls a function
  or a static method whose IR shows that no parameter takes its argument over, or a builtin variant
  (`qore_ir_call_takes_no_arg_over()`): such a call releases the read when it returns, as before.  The AOT compiler
  does not cache the IR of a closure, so a call that gives a closure an argument does not call its native code
  directly (`qore_ir_native_closure_call_eligible()`).

The value of a cast is not one of them: the AST interpreter holds a temporary operand of a cast until the statement
ends (issue #5712), and so do the other modes.
