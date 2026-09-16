# AOT Module Initialization and Program Lifetime

## Status

Implemented. This document records which `QoreProgram` owns the runtime state an
AOT module's initialization builds, and why that ownership is not the same for
every import of the same module.

Relevant code:

- `lib/QoreAOTRuntime.cpp` — `runAOTModuleInitForProgram()`,
  `registerAOTFunctionsFromSlotMaps()`, `buildContextFromSlotMap()`,
  `executeInitFunctions()`, `aotInitDescriptorNeedsExecution()`
- `lib/Function.cpp` — `UserSignature::setupFromAOTMetadata()`
- `include/qore/intern/qore_program_private.h` — `LocalVariableList`,
  `qore_program_private::createLocalVar()`
- `examples/test/ir/AOTModuleSharedClosureLifetime.qtest` — the regression test
- `examples/test/qore/classes/Program/program-memory.qtest` — the reclamation test
- `include/qore/intern/ModuleInfo.h` — `QoreBuiltinModule::~QoreBuiltinModule()`
- `examples/test/qore/misc/module-loader/aot-module-reinjection.qtest` — the shadow teardown test

## Two Programs, not one

Loading an AOT user module involves two Programs:

| Program | Lifetime | Holds |
|---|---|---|
| the module's own Program (the *shadow*) | the process, once the module is registered | the module's canonical namespaces, classes and constant entries; the code compiled module functions resolve against |
| each importing Program (the *target*) | whatever the caller decides | that Program's copies of the module's entries, plus whatever its own initialization produced |

`runAOTModuleInitForProgram()` runs for every target Program that imports the
module. It always registers against the **shadow** Program's root namespace —
class, global and constant lookups all resolve there — and then executes only
the initializers that target Program still needs
(`aotInitDescriptorNeedsExecution()`).

Exactly one of those runs is different: the first one claims the one-time
population of the shadow (`write_shadow`, guarded by
`AotModuleState::shadow_init_state`). That pass writes the module's own entries,
which every later importer then shares and reads for the rest of the process.
Every later pass writes only its own Program's entries.

## The rule

`LocalVar` storage is a **per-Program arena** — `LocalVariableList`, a
`std::deque<LocalVar>` in `qore_program_private`, cleared wholesale by
`waitForTerminationAndClear()`. A `LocalVar*` is therefore valid exactly as long
as the Program that created it, and nothing that outlives that Program may hold
one.

**Local ownership follows what the pass can publish.**

- A per-Program pass owns its locals in the target Program. Nothing it builds
  escapes that Program, so the storage is reclaimed at teardown — which is what
  keeps repeated short-lived imports of an AOT module bounded (#5381).
- The shadow-populating pass owns its locals in the module's own Program. What
  it builds is reachable from process-lifetime state, so the storage has to live
  that long. The pass runs at most once per module per process, so this is a
  bounded one-time cost rather than per-import growth.

The same rule governs a retained execution context: a context is bound to the
Program whose locals its slots name, so a recovery record that keeps one past a
per-Program pass hands it to that Program (see
`design/aot-constant-init-recovery.md`).

## What the rule prevents

The escaping object that matters most is a closure inside a module constant —
`SqlUtil::DefaultOpMap` is the canonical example. Its `UserClosureFunction` is
built during AOT init from the slot map, its signature `LocalVar`s come from
`UserSignature::setupFromAOTMetadata()`, and its body IR is kept lazy: the IR is
deserialized on the closure's **first call**, which re-reads those `LocalVar`s
and the Program recorded as their owner (`QoreAOTLazyClosureIR::local_owner_pgm`).

The value produced by the shadow-populating pass is stored in the module's own
`ConstantEntry` and shared with every later importer, which does not re-run the
initializer because the entry already has a value. Owning those locals in the
first importing Program therefore left the shared closure pointing into a freed
arena the moment that Program was destroyed, and the next call anywhere in the
process read freed memory.

Nothing at the call site shows it. The failing code loads an ordinary module and
calls an ordinary constant closure; the Program that poisoned it may have been
created and destroyed by unrelated code much earlier, and a throwaway `Program`
used to inspect or validate something is an ordinary pattern. The result was a
process kill, not a failed request.

## Tearing down the shadow Program

The shadow Program is released when the module manager deletes the module at exit:
`~QoreBuiltinModule()` sets the module context name and calls the module's delete function,
`qore_aot_module_delete()`, which finds the module's state in `aot_module_map` by that name. The map is
keyed by the feature name the module was initialized with. A reinjection
(`Program::loadApplyToUserModule()` with the reinject flag) keeps the original module under a unique name
such as `!!orig--Qorize-1` (`QoreModuleManager::reinjectModule()`), so the destructor sets the feature name
it saved when the module was loaded (`QoreBuiltinModule::feature`), not the module's current name; with the
current name, the lookup found nothing and the original's shadow Program was never released.

`aot-module-reinjection.qtest` compiles a module whose constant initializer registers a thread
initialization closure holding an object that reports its destruction, reinjects the module twice in a
child process, and checks that each original and copy reports one destruction at exit.

## Testing

`AOTModuleSharedClosureLifetime.qtest` compiles a module whose public constant
holds closures, loads it into a Program that is then destroyed, and uses it from
another Program. The probe runs in its own process, so a crash shows up as a
missing completion sentinel instead of killing the test run, and the same module
loaded from source is asserted as a baseline.

Freed arena storage still reads back intact if nothing reuses it, which makes the
defect silent in a small script — the original report needed a large application
to crash. The probe therefore creates a run of throwaway Programs between the
destruction and the call: each loads the module and parses locals of its own,
recycling the freed arena so the stale read lands on unrelated data.
