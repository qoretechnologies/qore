# IR Local Slot Numbering

`QoreIRFunction::local_var_slots` maps each local variable of a function's IR to a slot index.  Much of the AOT and
IR machinery binds locals by these indices rather than by `LocalVar` identity:

- AOT native code: `buildAOTSlotMap()` seeds the AOT local table from the IR slots;
- serialized IR (debugger bodies, closure IR), whose local slot records are read back by index;
- handler IR, which inherits its function's slots (`parent_slot_count`);
- the closure runtime binding (`AOTClosureRuntimeBinding::local_slots`), which binds the locals of a closure's native
  body to the variables of its serialized IR by slot;
- the declaration rank of locals in native cleanup ordering (`QoreIRToLLVM::getLocalDeclarationRank()`).

A function is lowered several times, and every lowering has to number its locals alike.

## The problem with first-reference numbering

`computeSlotIdsAndEmbed()` used to keep any pre-seeded slots (signature locals, parent slots for handlers,
`reserveLocalSlot()`) and number every other local by its first reference in block order.  The lowerings of one
function differ in block order and in their references:

- the reflection-free lowering (`reflection_free`) does not activate the locals of nested blocks at their declaration
  (`QoreIRLowering::localsMayBeReflected()`);
- a block with a local unwind anchor splits at every call and places the invoke continuation after its siblings, and
  a function is lowered again without anchors when its cleanup alone releases its locals in order
  (`unwind_order_proven`);
- the debugger body (`emit_debug_statements`) is a separate, unoptimized lowering;
- the serialized IR of a closure (`lowerClosureForSerialization()`) is lowered apart from the closure's native body;
- handler IR is lowered while its function is still being lowered.

So the same local had different slots in different lowerings.  PgsqlSqlUtil's bulk closures read `stmt` as `sql`
(the native closure body was unanchored, its serialized IR anchored), and the Qorus build failed with `slot map
construction failed ...: cancelled or invalid slot numbering` for a reflection-free method of a `PO_ALLOW_DEBUGGER`
program.  Point fixes followed each case: a second lowering of every anchored closure only to learn whether its
native body was unanchored, seeding a re-lowering from the lowering it replaced, and preparing closure slots after
lowering.

## Canonical numbering

`qore_ir_seed_canonical_local_slots()` (`lib/Function.cpp`) numbers the locals of a function once, from the AST,
before anything is lowered:

1. the parameters, in order;
2. `argv`;
3. `self` (not for a closure, whose `self` is the captured `self` of the code creating it);
4. every local declared in the body, in lexical declaration (pre-order) order, as `collectAllStatementLocals()`
   gives them: the locals of the block and of nested blocks, `if`/`for`/`while`/`switch`/`context` locals, `foreach`
   variables, catch parameters and the locals of `on_exit`/`on_error`/`on_success` handlers (which run in the
   function's frame);
5. for a closure, its captured variables in declaration order (`LVarSet`), whether or not its code refers to them.

Every lowering seeds `local_var_slots` with it before lowering:

|!Lowering|!Where
|native, reflection-free, without unwind anchors, debugger body|`tryLowerFunction()` (`lib/QoreAOT.cpp`)
|serialized closure IR|`lowerClosureForSerialization()` (`lib/QoreAOTBinary.cpp`)
|JIT and IR tiers|`UserVariantBase::lowerIRFunction()` (`lib/Function.cpp`)
|handler IR|`QoreIRLowering::compileAllHandlerIRs()`/`compileBlockHandlerIRs()`: the function's slots

The numbering depends only on the AST and the signature, so it is identical in all of them by construction.  The
remaining agreement check, `buildAOTFunctionSlotMap()` comparing the native and debugger bodies, is kept as a cheap
internal check that cannot fail in practice.

Handler IR copies its function's slots, `canonical_local_slots` and `canonical_local_slot_count`; since the
function's slots already include every handler local, a handler adds none of its own.

The numbering is the declaration order of the locals, which is also what native cleanup ranks locals by when it
releases several at once: the order no longer depends on the order in which code first referred to a local.

## Locals without a canonical slot

`computeSlotIdsAndEmbed()` still numbers a local that the IR refers to and that has no canonical slot, after the
canonical ones (`QoreIRFunction::canonical_local_slots`).  It numbers them after the walk, by name (the first reference
breaks ties), so that the numbering does not depend on block order.  Lowering creates no local variables of its own
in a function's IR: the only `LocalVar`s it creates are the parameters of background-expression helpers, which live
in the helper's own IR (`background-helpers.md`).  The non-canonical locals that remain:

- the signature `self` of a closure created in a method, when the closure's code refers to it.  It is not reserved:
  a reserved, unreferenced `self` would be written as a lexical-self capture by `qoreAOTWriteClosureCaptures()`, and
  the closure would hold the object;
- a local of the enclosing code that a closure reaches only through an embedded expression and that is not in the
  closure's capture set.

Each is referenced by every lowering of the function that refers to it at all, and a function rarely has more than
one, so they agree too.  `QORE_IR_TRACE_LOCAL_SLOTS` marks them with `*` (see below); in the qlib modules only a
closure's `self` appears.

Code that is lowered once and not bound by index across lowerings keeps first-reference numbering: top-level code
(whose root locals belong to the `Program`; AOT top-level code and the runtime rebuild its context with the same
lowering) and background helpers.

## Cost

- Compile time: the extra lowering per anchored closure that decided how to lower its serialized IR is gone, and so
  are the post-lowering preparation passes over closure slots.
- Slots: almost every local declared in a body was referenced by every lowering already (an ordinary lowering
  instantiates each block local, and the closure preparation reserved all parameters and captures), so the AOT
  context slot tables are essentially unchanged.  A declared local that a lowering never refers to now has a slot,
  which costs one entry in the slot table and in the interpreter's local cache; it is never instantiated or
  published on its account, since instantiation, publication and frame layout are driven by the instructions and by
  `all_body_locals`, not by `local_var_slots`.  The JIT tiers already reserved every body local this way.
- Generated code: slots are indices, so their order does not affect code quality.

## Serialized format

The format of serialized IR and slot records is unchanged; only the numbers in them change.  Every source involved
(`Function.cpp`, `QoreAOT.cpp`, `QoreAOTBinary.cpp`, `QoreIRVerifier.cpp`, `QoreIR.h`, ...) is part of the `qcc-format`
fingerprint, so the AOT runtime identity changes with this numbering (`aot-object-files-and-module-artifacts.md` "AOT
Module Runtime Identity"): the loader refuses `.qmod` files and executables built with the old numbering and in-tree
qmods are rebuilt.  A module built with the old numbering is consistent in itself (its native code and its serialized
IR were numbered by the same lowering), but objects compiled by an older qcc must not be linked with
`--from-objects` by a newer one, which re-lowers the functions for the slot map walk.

## Tracing

With `QORE_IR_TRACE_LOCAL_SLOTS` set, every lowering of a function with canonical slots writes one line to stderr
(`qore_ir_trace_local_slots()`):

```
IR-LOCAL-SLOTS <file>:<line of the body>[:<column>] <lowering>: 0=<name> 1=<name> ...
```

`<lowering>` is one of `aot`, `aot-unanchored`, `aot-reflection-free`, `aot-reflection-free-unanchored`, `aot-debug`,
`aot-debug-unanchored`, `aot-reflection-analysis`, `closure-ir`, `handler`, `jit`, `jit-unanchored`,
`jit-reflection-free` and `jit-reflection-free-unanchored`.  `examples/test/ir/AOTCanonicalLocalSlots.qtest` checks
that all lowerings of each function agree in executables, modules, per-source objects linked with `--from-objects`,
split compilation and the JIT.
