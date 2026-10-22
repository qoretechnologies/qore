# on_block_exit handlers in native code

The body of an `on_exit`, `on_error` or `on_success` handler is lowered to its own IR function
(`QoreIROnBlockExitInstruction::handler_ir`, see `QoreIRLowering::compileBlockHandlerIRs()`). On the normal exit of
its block the handler is also lowered inline, so most executions run the inline code; the handler function runs when
the block is left by an exception, and when the block's handlers are not lowered inline (a `ScopeExit` that is not
`inline_lowered`).

## Registration and execution

Native code (JIT and AOT) registers each handler when its statement is reached (`OnBlockExit`) on a thread-local
stack, and runs the handlers registered since a scope or function was entered at its exit
(`qore_rt_exec_on_block_exit_impl()`, `lib/JITRuntime.cpp`), in reverse order of registration, with the exception of
an `on_error` handler as its implicit argument.

The handler function is always native code, emitted with the function it belongs to by
`QoreIRToLLVM::lowerNestedFunction()` and named `<function>_obe_<n>` (deterministic: `n` counts the handlers of the
function, and a nested handler is named after the handler containing it, such as `<function>_obe_1_obe_0`):

- JIT: `qore_rt_push_compiled_handler()` registers the function, which is called with the exception sink;
- AOT: `qore_rt_push_compiled_handler_aot()` registers the function with the AOT context of the function it belongs
  to, which it shares, as the helper of a background expression does (`background-helpers.md`): `buildAOTSlotMap()`
  walks the handler's IR without seeding its own slot ids, so its locals, expressions and nested handlers take slots of
  that function, and its PC-to-location map is attached to that function's context
  (`qore_aot_ensure_helper_pc_loc_map()`) before it first runs. Native code reports an exception by unwinding to the
  runtime with the exception already in the sink (`QoreJITException`).

  The runtime binds the local slots of a native closure body to the variables of the closure's serialized IR by
  slot index (`AOTClosureRuntimeBinding::local_slots`), so the compiler prepares the closure's IR slots before it
  builds the slot map (`qoreAOTPrepareClosureIRLocalSlots()`, as the serializer does: parameters, `argv`, then the
  captures by name). A captured variable that only a handler of the closure uses has no IR slot of the closure
  otherwise, and would take a slot in walk order that binds another variable.

The handler reads the locals of its function from the thread-local variable stack, to which the function publishes
them before it runs its handlers (`syncLocalsToRuntimeForHandlers()`), and the function reloads them afterwards.

If a handler cannot be lowered, the function cannot be compiled: there is no other way to run the handler. The IR
interpreter runs handlers only as part of IR it executes itself (the IR tier, and the debug IR of a compiled module
under a debugger).

## History

AOT-compiled code used to run these handlers with the IR interpreter: each function record carried a statement slot
per handler with the handler's serialized IR, which the runtime read back when it built the function's context, and
the JIT ran a handler with the IR interpreter if it could not lower it. Since AOT binary format version 20 a function
record has no statement slots (the `num_stmts` field of the native function descriptor is kept and is 0), and a record
with statement slots from an older version is refused.

## Regression coverage

- `examples/test/ir/AOTNativeBlockExitHandlers.qtest`: handlers on the normal and the exception path, nested, with
  locals of their own, closures, a change to a local of their function, a background expression, `rethrow` and a new
  exception, and the handlers of a closure using captured variables that its body does not use, in each execution
  mode and from a source-stripped compiled module, whose image must contain the native handler functions.
- The many `examples/test/ir` tests with handlers in compiled code.
