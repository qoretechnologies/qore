# Background expressions in the compiled tiers

The `background` operator evaluates its whole expression in the new thread, in every execution mode (issue #5482):
the call's arguments and its target (the object of a method call, the expression giving a call reference), or any
other expression. What the expression takes from the code that starts the thread is evaluated when the thread starts:
its local and thread-local variables, the members of `self` it reads (`SelfVarrefNode`), find expressions, closures,
references and the implicit arguments (`$1`, `$$`, `$#`) of the starting code. An implicit argument of an operator in the expression that
binds its own (map, select, foldl, foldr, the hash forms of map and select, the streaming operators) is that
operator's and is evaluated in the new thread.

The AST interpreter implements this with `copy_value_and_resolve_lvar_refs()` (`lib/AbstractQoreNode.cpp`): the
expression is copied with those values substituted, and the new thread evaluates the copy (`do_op_background()`,
`lib/thread.cpp`). The IR interpreter, the JIT and AOT-compiled code use a helper function lowered from the
expression.

## The helper

`QoreIRLowering::lowerBackground()` (`lib/QoreIRLowering.cpp`) emits a `BackgroundInt` instruction of kind
`QoreIRBackgroundKind::Helper` (`QoreIRBackgroundInstruction`, `include/qore/intern/QoreIR.h`):

- `helper_ir` is a compiler-private IR function lowered from the expression by a lowering of its own
  (`lowerBackgroundHelper()`); it returns the expression's value, which the new thread releases.
- `helper_params` are the helper's parameters, local variables that the new thread instantiates on its own local
  variable stack before calling the helper, as a call instantiates its parameters
  (`LocalVar::instantiate()`); they are pre-instantiated locals of the helper.
- the operands are the parameters' values, evaluated by the starting code.

A parameter is either:

- a local variable of the starting code that the expression uses: the helper refers to the same `LocalVar`, so every
  lowering path of the helper, including the fused local-variable paths that do not go through `lowerExpression()`,
  reads the value the new thread instantiated; the starting code loads it (`load.local`, or `load.closure` for a
  closure-bound variable), so the variable stays an ordinary frame local of the starting code and is never made
  closure-bound by the background expression;
- or a synthetic local owned by the instruction (`owned_params`, named `%background_<helper>_<n>`, which is not a
  Qore identifier and so never collides with a variable of the code) holding any other value the expression takes
  when the thread starts (type `auto`; a thread-local variable's value is its value in the starting thread). While the helper is lowered, `lowerBackgroundLeaf()`, called first by
  `lowerExpression()`, replaces such an expression with a load of its parameter and records it; the starting code
  then lowers the recorded expression itself (a closure is created there, so its object-scope capture is checked
  then, an implicit argument is read there, and so on). The same implicit argument or member read used twice is one
  parameter. An implicit argument is such a value only while `virtual_implicit.active` is false: every per-element
  operand of an iterating operator is lowered with it set, and a fresh helper lowering starts with it clear.
- A reference (`\lvalue`) is taken by the starting code and held by a parameter of the reference's type; the helper
  uses a reference to that parameter (a `ParseReferenceNode` over a `VarRefNode` to it, made thread-safe as the parser
  makes a referenced local), which refers to the same lvalue, as a reference to a reference parameter does. The
  parameter is closure-bound, so a reference that the called code keeps outlives the helper's frame.

The helper's parameters are the locals in its `local_var_slots` (the starting code's locals, in slot order), followed
by the synthetic ones in the order recorded. A background expression declares no local variables of its own.

If the expression calls a method of the starting code's object (`SelfFunctionCallNode` with a non-static or
unresolved method, see `lowerSelfCall()`), `helper_uses_self` is set: the new thread runs with that object as its
context object and holds it with a real reference for the thread's duration, as a background call of the method
does. `self` itself is an ordinary local (the signature's `selfid`) and so a parameter.

A background expression nested in a helper is lowered by the helper's lowering: its values are taken from the
helper's frame, so the outer thread's start captures them, and its use of the object makes the outer helper use it.

### The decomposed calls

A call whose target and arguments are all values that the expression takes when the thread starts - immediate
values, local variables (not reference-typed), thread-local variables, references and implicit arguments
(`qore_ir_is_background_start_value()`) - keeps
the decomposed form: the starting code evaluates them, which is the same, and the runtime builds the call node with
the values (`runDecomposedBackground()`, `qore_rt_background_*()`). Every other expression is lowered to a helper;
there is no AST fallback.

## Runtime

`do_op_background_helper()` (`lib/thread.cpp`) starts the thread: it captures the sandbox, applies the closure
capture rules to each value (`background_check_value_capture()`, with a `BackgroundClosureCaptureHelper`, as
`copy_value_and_resolve_lvar_refs()` does), and gives `BGThreadParams` a `BGThreadHelper` holding the code, the
parameters and a reference to each value. The new thread instantiates the parameters, runs the helper with
`qore_run_background_helper()` (`lib/JITRuntime.cpp`) and uninstantiates them in reverse order, however the helper
returns. If the thread cannot be started, the values are released.

The new thread's call stack is that of AST execution: `*thread start*` followed by the code the expression calls; the
helper pushes no frame of its own.

`qore_run_background_helper()` runs, in that order of preference:

- the AOT helper (`QoreBackgroundHelperCode::aot_fn`) with the starting function's AOT context;
- the JIT helper (`jit_fn`);
- the helper's IR with the IR interpreter, when the starting code runs on the IR tier.

Native code reports an exception by unwinding to this boundary (`QoreJITException`), with the exception already in
the sink, as `UserVariantBase::evalTiered()` handles it. A helper is compiled without speculative guards, so it never
requests a deoptimization; there is no other tier to run it on, and a request is reported as an internal error.

## Native code

`QoreIRToLLVM::emitBackgroundHelper()` lowers the helper with a nested lowering into the same LLVM module, as a
function named `<function>_bg_<n>` (deterministic: `n` counts the helpers of the function), and emits a call of
`qore_rt_background_helper()` (JIT: the instruction and the helper's function pointer) or
`qore_rt_background_helper_aot()` (AOT: the context, the helper's function pointer and the parameters' local slots,
a private constant array). The nested lowering takes the parent's settings (AOT slot map, debug info, batch callees,
exception checking).

In AOT the helper shares the starting function's `QoreAOTContext`, which holds only per-function metadata (slot
tables, expression slots, locations) and is shared by every thread running the function anyway; the values are on the
new thread's local variable stack. Therefore:

- `buildAOTSlotMap()` (`lib/QoreAOT.cpp`) walks the helper at its instruction, without seeding the helper's own IR
  slot ids (`seed_ir_local_slots = false`), so its synthetic parameters and its expressions get slots of the starting
  function in a deterministic order, and a metadata-only compile (`qcc -m --from-objects`) assigns the same slots
  without lowering it;
- the helper's runtime-location indices address the starting function's location table: its lowering starts after
  the parent's entries so far, and the parent appends the helper's entries;
- the lazy PC-to-location map has an entry per helper symbol (`buildAOTPcLocMaps()`, attributing `_bg_<n>` symbols to
  their function with its location table), which `qore_aot_ensure_helper_pc_loc_map()` attaches to the starting
  function's context before the helper first runs.

## Serialization

Since AOT binary format version 20 (`QORE_AOT_BACKGROUND_HELPER_VERSION`), a serialized background instruction of the
helper kind (`writeBackground()` / `readBackground()`, `lib/QoreAOTInstRegistry.cpp`) carries whether it uses the
object, each parameter's slot in the helper, and the helper's IR. The helper's locals are all variables of the
enclosing AOT context, so their slot records bind to them (`AOTIRLocalBinding::ENCLOSING_SLOT`), as the locals of a
nested `on_block_exit` handler do. This is what debug IR and the lazily materialized IR of compiled functions use.

## Parse restrictions

A background expression may not modify a local variable of the starting code, nor directly assign or modify a member
of its object (`checkLValue()` in `include/qore/intern/QoreOperatorNode.h`, and list assignment): the expression takes
their values when the thread starts. A local variable may not be passed by reference anywhere in the expression.

## The AST copy of a per-element operand

A call in an operand that an operator of the expression evaluates for each element is evaluated more than once in
the new thread. Such a copy is marked reusable (`AbstractFunctionCallNode::setReusableTmpArgs()`, by
`crlr_call_copy_done()`): evaluating a temporary argument list consumes it (`QoreListNodeEvalOptionalRefHolder`
swaps its values out), so each evaluation uses a copy of the list (`getEvalTmpArgs()`), and a reusable
`SetSelfFunctionCallNode` keeps its object until it is released.

## Regression coverage

- `examples/test/qore/operators/background-thread-eval/background-thread-eval.qtest`: in each execution mode and from
  a compiled module, the arguments, the call target and the object expression are evaluated in the new thread; a
  blocking argument does not block the starting code; locals, members and implicit arguments are taken when the
  thread starts, bound implicit arguments are the operators'; non-call expressions; self calls in a map; exceptions
  of an argument and of the target are exceptions of the new thread (call stack checked); with `--ir-dump`, a local
  used in a hot loop and a background call stays `store.local` and `add.assign.local.int`, the helper evaluates the
  argument expression, and a closure capturing the local is the control (`store.closure`).
- The existing background tests in `examples/test/qore/{operators,threads,classes,closures}` and
  `examples/test/ir/BackgroundOperator.qtest` in each execution mode.
