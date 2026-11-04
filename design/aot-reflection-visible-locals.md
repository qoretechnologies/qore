# Reflection-Visible Locals in Compiled Debugger Programs

Status: implemented (2026-11-01).

## Problem

`get_local_vars()` and `set_local_var_value()` (domain `DEBUGGER`, so only in programs parsed with
`PO_ALLOW_DEBUGGER` / `%allow-debugger`) read and replace locals by walking the current thread's runtime local stack
(`ThreadLocalProgramData::lvstack`) and closure-variable stack, frame by frame (`thread_get_local_vars()`,
`thread_set_local_var_value()` in `lib/thread.cpp`).

The IR tiers keep a local that no AST code can reach in private IR storage (`QoreIRFunction::ir_only_locals`). A
private local never appears on the runtime local stack, so reflection cannot see or replace it.  The same holds for a
parameter that the body never reads, which a fast entry does not publish at all.

Before the 2026 stabilization, source IR lowering already made every local AST-visible in a debugger-enabled program
(`UserVariantBase::lowerIRFunction()`, `setCachedIR()`), but AOT lowering did not, so compiled debugger programs
returned `NOTHING` for private locals and ignored replacements.  The stabilization made AOT match the IR tier
(`tryLowerFunction()` in `lib/QoreAOT.cpp`): reflection is now correct, but every IR-only optimization is disabled in
every function of a compiled debugger program, contradicting the documented design that native code is kept for
programs that merely allow the debugger (`Function.cpp`, `UserVariantBase::evalIntern()`), and 20 regression tests
that assert native optimizations in debugger fixtures.

An attached debugger is not part of this problem: when a debugger attaches, frames switch to AST or to the separate AOT
debug IR (`materializeAOTDebugIR()`), in which every local is visible.

## Key observation

Both reflection builtins inspect frames of the **current thread** only.  A frame can therefore be inspected or changed
only while code running on the same thread, during the frame's activation, calls a reflection builtin.  That code is
either the function itself or something it causes to run: a callee, a callback, a destructor run when a value is
released, or any other user code.

A function whose activation provably cannot run a reflection builtin is **reflection-free**: no reflection can observe
its private locals, so keeping them private is exact, not an approximation.

## Proposal

Compute a conservative `may_reflect` effect per function in the existing interprocedural fixed point
(`qore_ir_compute_function_effect_summaries()`), and apply the debugger visibility rules only to functions that may
reflect.

### Local effect: an instruction may reflect if it

- calls a builtin function of the `DEBUGGER` domain (`get_local_vars()`, `set_local_var_value()`, and the others in
  that domain), or any builtin that can run user code or call by name (`call_function()`, `call_function_args()`,
  `call_builtin_function()`, methods taking `code` arguments, sorting and other callbacks);
- calls a user function, method or closure that is not resolved to an exact variant in the compilation group, or that
  passes reference arguments (the existing conservative case);
- calls through a call reference, `call_function()` by name, a dynamic method call, or a closure of unknown identity;
- can release a value that may hold an object whose class is not proven to have no user destructor in its hierarchy
  (a destructor runs user code, which may reflect on the releasing frame through `frame` 1);
- delegates to the AST interpreter (`has_opaque_ast_local_access`) or contains a statement the analysis does not know.

A resolved call to a function in the group propagates its callee's `may_reflect`; unresolved callees and external
modules are conservative.  The fixed point is the same one already used for `may_invalidate_external_caches`.

### Applying the result

- In a debugger-enabled program, AOT lowering keeps the current visibility rules (every local AST-visible, unused
  parameters published, `reflective_frame` set) only for functions with `may_reflect`; reflection-free functions keep
  exact IR-only classification and every optimization based on it, including private fast entries.
- Programs that do not allow the debugger are unchanged: reflection is impossible there.
- The IR and JIT tiers keep their current conservative rule (all locals visible in debugger programs), because they
  lower functions individually without the whole-group summary; they can adopt the summary later where a batch exists.

### Frames that may reflect need a real frame boundary

Reflection addresses frames by index (`get_local_vars(1)` is the caller).  Native JIT and AOT calls into a function
that calls a reflection builtin currently do not count as a frame for that indexing, so
`set_local_var_value(1, "value", ...)` in a callee fails with `UNKNOWN-VARIABLE` (IRFloatLocalAdd's `dynamicMutation()`;
pre-existing, the same on the frozen baseline in forced JIT and AOT).  A function whose local effect calls a
reflection builtin must therefore run with a runtime frame boundary: no private fast entry, no inline execution, and a
frame pushed by its native entry.

### Lowering-time decisions use the same rule

Two other debugger-only lowering rules exist only to keep reflection correct: re-activating ordinary block locals at
non-root block entry (block re-entry visibility) and lowering `push` to a local through the lvalue path (a reflected
plain-list replacement keeps its runtime type).  Both are made while one function is lowered, before group summaries
exist, and today apply to every function of a debugger-enabled program.  They should apply only to functions that may
reflect.  At lowering time the decision can use the intraprocedural part of the rule: a function containing no call,
no instruction that may run user code and no release of a value that may hold an object cannot reflect, whatever its
callees do.  Measured effect: AOTInPlaceIntListStore's `exactListStoreLoop()` (no calls, `list<int>` and `int` locals)
loses its fast entry only because of the `push` rule.

### Ordering in the AOT pipeline

The IR-only classification in `tryLowerFunction()` currently happens per function before the batch summaries exist.
The reflection decision has to move after the group's effect summaries are computed and before the fast-entry and
local-classification decisions that depend on it; functions found to reflect are then reclassified (all locals
visible) before code generation.  This ordering change is the main implementation risk and needs its own validation.

## Implementation notes

- `qore_ir_function_may_reflect_locally()` (`lib/QoreIRAnalysis.cpp`) is the local effect.  The value of a constructed
  list or hash is exempt from the object rule (it holds only its operands, which are checked themselves), and so is a
  load of `self` (the invocation holds a reference to it for its whole duration).
- An lvalue path operation (`LValuePath*`) on a member, a static, a global or a captured variable releases the value
  it replaces, and its result is often a scalar (`++member`), so neither the function-level local rule nor the result
  rule covers it: it may reflect unless the declared type of its root cannot hold an object
  (`qore_ir_lvalue_path_may_release_object()`; a member's type is not carried by the path, so a member update always
  may).  Without it, a method that only incremented an `auto` member holding an object was lowered reflection-free, and
  `set_local_var_value()` in the destructor failed with `UNKNOWN-VARIABLE` or its replacement was lost
  (AOTReflectionReach `memberRelease`).  The IR and JIT tiers apply the rule always; AOT, for a member update, only to
  a frame with body locals (`aotFunctionMayReflect()`), because its native method entries publish and reload the
  parameters around such an update, which keeps the effect-member entries of methods like `int next(int value) { ++calls; ... }` in debugger
  programs (AOTEffectMemberEntry checks their reentry with and without the effect entry), while the body locals of a
  reflection-free AOT frame stay private.
- A reference local or parameter counts as a local that can hold an object when the type that it refers to can
  (`qore_ir_lvalue_type_may_hold_object()`): a write through it (`r = 1`, `delete r`, `r -= "a"`) replaces the value
  of the variable referred to.  Releasing the reference itself releases no object, so the other uses of
  `qore_ir_type_may_hold_object()` (cleanup anchors, temporaries) are unchanged.  An update of a static, a global or a
  captured variable, a non-local element store and an lvalue expression instruction with a non-local base count in
  every AOT frame too: only the native method entries reload the parameters, and only around a member update
  (AOTReflectiveReleaseReload `staticParam`).
- Making the frame reflective is not enough: the tiers cache the values of the locals that are on the runtime stack
  (the IR interpreter's slot cache, the allocas of native code) and reloaded them only after calls.  A destructor run
  by an update that releases an object, or by the release of a temporary, replaced a local on the runtime stack while
  the frame kept reading its cached value (`int secret = 5; g = 1;` read 5 after the destructor of the object in `g`
  assigned 45, in the IR tier for an update of any kind, and in the JIT and AOT for a plain assignment of a global, an
  update of a local, a write through a reference, and a scope exit).  In a reflective frame, the locals are now
  reloaded after an instruction that can release an object when it changes an lvalue
  (`qore_ir_instruction_may_release_object()`: assignments, compound assignments, removals, element stores, scope
  exits, by the declared type of the variable or of the target of a reference), as after a call: the IR interpreter
  invalidates its caches before the next instruction, and native code bumps the reload epoch
  (`QoreIRToLLVM::lowerInstruction()`).  A release of temporaries does the same: in the interpreter when a value
  released is an object or a container, in native code when the scope has temporaries to release.  A frame that is
  not reflective, and a store whose old value cannot hold an object, are unchanged (AOTReflectiveReleaseReload's
  scalar stores keep their native entries).
- `QoreIRFunction::reflection_free` is set before lowering; `QoreIRLowering::localsMayBeReflected()` applies the
  block re-activation and `push` rules only without it, and on_exit/on_error handler bodies inherit it.
- The IR tiers (`UserVariantBase::lowerIRFunction()`) re-lower a function that neither reflects by itself nor calls a
  user function.  The reflection-free lowering does not activate the locals of nested blocks at their declaration;
  its locals still have the slots of every other lowering of the function, since all of them take the canonical
  numbering (`ir-local-slot-numbering.md`), which the debugger body, serialized IR and handler IR bind by.  AOT (`tryLowerFunction()`, `aotFunctionMayReflect()`) follows exact callees through their own
  conservatively lowered bodies, up to 64.
- In a reflective AOT frame the body locals move to the runtime stack, and so do the parameters that the function
  may access after an instruction that may reflect, before assigning them (`qore_ir_get_params_read_after_reflection()`).
  The native method, receiver and closure entries publish parameters for reflection, which makes reads by name
  correct, but a private parameter is read from a native alloca that is never reloaded, or from a load hoisted out
  of a loop, so a later access would miss a reflective write (fixed 2026-11-02).  A parameter read only before
  anything that may reflect stays private, with the native entries based on it; an unused parameter is never private
  in a reflective frame.
- The JIT creates the reload epoch of a reflective frame eagerly, as AOT does for every frame: a loop read lowered
  before the call that would create the epoch lazily otherwise gets no freshness check.
- Only the private direct entries of functions and static methods are refused to reflective frames
  (`isAOTDirectEntryFrameSafe()`): those calls push no runtime frame boundary.  Entries dispatched through the runtime
  call helpers (methods, closures, call references, self recursion) push it.

## Validation

- AOTLocalReflection and AOTScalarLocalStore: every reflected local, parameter, `argv` and `self` remains visible and
  replaceable in every tier and AOT variant.
- New cases that make each conservative rule necessary: reflection from a callee two levels down, through
  `call_function()` by name, through a callback passed to a builtin, through a destructor run by a release, through a
  closure, and from another compilation unit; each must see the caller's private locals.
- The 20 debugger-fixture tests pass with their existing structural assertions where the asserted function is
  reflection-free, and assert the conservative path where it is not.
- No change for programs without `%allow-debugger` (structural IR identical before and after).
- AOTReflectiveReleaseReload: a destructor run by each kind of update (assignment, `delete`, `remove`, compound
  assignment, splice, extract, pop, shift, hash and list element stores) of a global, a static, a local, a member and
  a variable referred to by a reference replaces a local or a parameter of the updating frame, and every tier and AOT
  variant reads the new value; stores of values that cannot hold an object keep their native entries.

Known, separate from the above: a destructor run while the lock of a global or static is held (`r -= "a"` with `r` a
reference to it) deadlocks in `get_local_vars()` on a frame whose reference refers to that variable, in every tier
including the AST interpreter; and the AST interpreter releases the value of a `?:` condition right away, where the
compiled tiers release it with the statement's temporaries.

## Limits

- Out of scope here: a function with two or more `code` (or other possibly-private object) locals keeps the
  stabilization's unwind cleanup anchor, whose exception edges defeat the finite call-reference analysis
  (AOTFiniteCallbackTargets `multipleGroups()`).  That needs the analyses to follow invoke normal edges; it is
  independent of reflection.

- The destructor rule makes any function that releases an object of an unproven class reflect; functions handling only
  scalars, typed containers of scalars and objects of destructor-free classes keep their optimizations.
- Reflection-free proofs do not cross into precompiled modules without their effect metadata; calls into them are
  conservative.
