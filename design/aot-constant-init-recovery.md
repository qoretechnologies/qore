# AOT Constant Initialization Recovery

## Status

Implemented. This document records why an AOT constant may be readable before
its value exists, and the rule that makes that state recoverable instead of
permanent.

Relevant code:

- `lib/QoreAOTRuntime.cpp` — `executeInitFunctions()` (the load-time fix-point
  loop), `aotRegisterPendingConstantInit()`, `qore_aot_run_pending_constant_init()`,
  `AotScriptPendingConstantState`
- `include/qore/intern/ConstantList.h` — `ConstantEntry::aot_pending_init`,
  `RuntimeConstantRefNode` read paths
- `examples/test/ir/AOTPendingConstantRecovery.qtest` — the tests for a module load
- `examples/test/ir/AOTScriptPendingConstantRecovery.qtest` — the tests for a
  script load (a compiled executable)
- `examples/test/ir/AOTConstantValuePublication.qtest` — an import copying a
  constant while its value is stored (see "Publishing a stored value")
- `include/qore/intern/ConstantList.h` / `lib/ConstantList.cpp` —
  `ConstantEntry::setRuntimeValue()`, `materializeRuntimeRefs()`,
  `runtimeValueLock()`, `retireValue()`, the copy constructor

## The state that exists

A constant whose value cannot be materialized when the module is serialized —
an object, a value computed from other constants, anything a `__const_init`
function has to produce — is deserialized as a *shell*: `aot_shell_pending` is
set and `val` is a self-referential `RuntimeConstantRefNode`. Module load then
runs the generated initializers.

Load-time initialization is ordered by serialization order, which is
declaration order, which is not dependency order. The loop therefore retries:
an initializer that raises `AOT-PENDING-CONSTANT` because a dependency has not
run yet is retried in a later round, and the loop stops when a round makes no
progress.

Two things can still leave a shell unpopulated after that loop, and neither is
repaired by any later load:

| How | What is left |
|---|---|
| No round could run the initializer | the module's own entry is a shell |
| A Program's entry was created from the module's entry before that entry held a value | that Program's entry is a shell while the module's entry is fine |

The second one is the reason a constant can be broken in one Program and
correct in another, and why the failure looks intermittent and
service-specific rather than like a module that does not work.

## The rule

**A shell must be recoverable from its first read.** The read path may not
assume load-time initialization succeeded, because it sometimes did not.

Every constant an AOT load initializes therefore carries a recovery record
(`ConstantEntry::aot_pending_init`), and the record is copied with the entry,
so each Program's copy can recover on its own. The first read of a shell:

1. adopts the value from the module's own entry if that entry has one — the
   common case, and it runs no code; otherwise
2. runs the initializer, if the record kept one, under the module's Program
   context. A dependency read from inside that initializer is itself a read of
   a shell, so it recovers one level deeper; recursion is serialized by one
   recursive lock and a per-record `running` flag turns a genuine cycle into a
   reported error rather than a stack overflow.

Only an initializer that did not run at load is kept executable. Retaining the
compiled context of every constant of every loaded module would cost real
memory for nothing, since a constant that initialized normally can only ever
need path 1.

## Publishing a stored value

An initializer's result is stored in two steps, and other threads can see the
entry during both.

1. `ConstantEntry::setRuntimeValue()` stores the result and the flags that
   describe it (`saved_val_set`, `aot_shell_pending`, `init`).
2. `ConstantEntry::materializeRuntimeRefs()` replaces the stored value with one
   whose serialized constant references are resolved, once every initializer
   of the round has stored its value (a value may refer to a constant a later
   initializer produces).

The entries are not private to the loading thread. A class method of a module
runs in the module's Program, so a `load_module()` call there stores the loaded
module's constants in the module's Program, and every import of that module
copies the module Program's entries (`ConstantList::mergeUserPublic()`). The
namespace locks do not cover the store: the merge lock
(`RuntimeNamespaceMergeLocker`) is released before the deferred initialization
runs, because that initialization waits for threads that read the same
namespace.

A copy made during a store could take the value from after it and the flags
from before it — `saved_val` set with `saved_val_set` clear. The copy never
released that value: a leak in a release build, and a failed destructor
assertion in a debug build, when the importing Program was destroyed.

The rules:

- **One lock orders a value with its copies.** A store, the replacement at the
  end of a materialization, and the copy constructor's snapshot of the value
  and its flags hold the entry's value lock (`ConstantEntry::runtimeValueLock()`,
  one of a fixed set of locks shared by address). It is a leaf lock: nothing
  else is acquired under it, and no Qore code runs under it. Resolution itself
  runs with no lock held, because it evaluates other constants and can run
  their initializers; its result is only installed if the stored value is still
  the one that was resolved.
- **A value other threads can see is retired, not freed.** Readers that follow
  a constant reference to its stored value (`RuntimeConstantRefNode`,
  `resolveRtConstRef()`, `getValue()`, `ConstantList::getInfo()`) take no lock
  and no reference of their own before reading it, so a value replaced by a
  store or a materialization is kept until the entry is deleted
  (`ConstantEntry::retireValue()`). That happens at most twice per entry.
- **The flags are published after the value.** A store writes the value, then
  a release fence, then the flags; a reader tests `saved_val_set` and passes an
  acquire fence (`ConstantEntry::acquireRuntimeValue()`) before it reads
  `saved_val`.

A read made between the two steps is correct without them: a stored container
that still holds reference nodes is not a committed value, so evaluating it
evaluates the nodes. Materialization only replaces it with an equal value that
no longer needs that evaluation.

## A script load is a load

The rule holds for every AOT load, not only a module's. A compiled executable
loads its whole program as one script batch, so an ordering the fix-point
cannot satisfy there is not a broken module but a broken program: before script
loads registered records, such a constant raised `AOT-PENDING-CONSTANT` on
every read for the life of the process, and the only cure was a rebuild that
happened to order the initializers differently. That is the same defect this
document describes, with no recovery available — and it is worse to diagnose,
because a rebuild appears to fix it.

What genuinely differs is Program lifetime, and it constrains what a script
record may hold rather than whether it exists:

- A module Program lives for the process, so a module record names it, and
  path 1 can look up the module's own entry through it.
- A script Program need not: `qore_aot_script_register()` is a host API that
  may be given a Program the host later destroys, while a record lives for the
  process (it is reachable from every copy of the entry, and nothing can say
  when the last copy is gone). **A script record therefore names no Program at
  all.** The first read already runs in the Program that owns the entry, which
  is the only Program its initializer may touch, so it needs none; path 1 has
  no module entry to adopt from and is simply skipped.
- A retained execution context is bound to the Program whose objects and locals
  its slots name. A script load hands its contexts to that Program
  (`AotScriptPendingConstantState`, attached as Program external data), so
  Program teardown neutralizes the records it created. A copy of a
  still-pending entry that outlived its Program then reports the constant as
  unpopulated instead of running against freed state.

  A module load does the same for every pass that is not populating the shared
  shadow. Only the shadow-populating pass owns its locals in the module Program;
  every later per-Program pass owns them in the importing Program and loses them
  at its teardown, which is the same constraint a script load is under. See
  `design/aot-module-init-program-lifetime.md`.

Program external data whose state belongs to one Program alone returns
`nullptr` from `copy()` rather than a copy — a child Program runs its own AOT
load — so `qore_program_private_base::copyExternalData()` must skip a null
result rather than store it.

## Why the error message matters

Before recovery existed, the only thing a reader could be told was that the
initializer had not populated the value. That sentence is true of every
instance of this defect and identifies none of them: the actual cause is the
exception the initializer raised at load, which the loop had already
discarded — a module whose constants were unusable reported an
`AOT-PENDING-CONSTANT` per read, forever, with the real error nowhere.

The recovery record keeps the load-time failure. When recovery is impossible,
the read reports that failure; when the initializer is run from the read and
fails again, the read reports the initializer's own exception. Neither path
reports only the absence of a value.

## Testing

The first-read path is unreachable from an ordinary test, because ordinary
loads satisfy initialization order. `QORE_AOT_TEST_DEFER_CONSTANT_INIT` leaves
every AOT constant uninitialized at load, which forces every read through
recovery. Each test asserts that the deferred run produces values identical to
the eager run **and** that the run really deferred (the `QORE_AOT_INIT_TRACE`
output shows the recovery), so a build that ignored the hook cannot pass by
initializing everything at load.

The script test compiles a multi-file executable with `qcc` so the constants
are spread over several `.qo`s, which is what makes their initializers depend
on each other across compilation units. It also creates a child Program while
the initializers are still pending, so the external-data copy of a Program
holding retained contexts is covered.
