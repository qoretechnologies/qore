# Qore JIT and AOT Current Design

## Status

Qore has one LLVM-backed compilation pipeline shared by JIT and AOT:

```text
Qore AST -> Qore IR -> LLVM IR -> native code
```

Runtime modes:

- `--exec-mode=ast`: AST interpreter and default execution mode.
- `--exec-mode=ir`: eager IR lowering and IR interpreter execution.
- `--exec-mode=jit`: eager IR lowering; the program's own functions are compiled
  to native code on a background thread behind a parse-commit barrier, while
  functions imported from modules promote to native adaptively on first use.
- `--exec-mode=tiered`: explicit adaptive execution, promoting hot functions
  AST -> IR -> JIT.
- `qcc`: ahead-of-time compiler and artifact inspection tool for executables,
  `.qmod`, `.qo`, and `.qoa` artifacts.

This document is the durable index for the current IR/JIT/AOT architecture. It
should describe stable contracts and link to narrower design notes; transient
milestones and unfinished task lists belong under `docs/plans/`.

## JIT Runtime

JIT-compiled functions call C ABI helpers in `JITRuntime.cpp` for operations
that need Qore runtime semantics: calls, object/member access, exceptions,
closures, local writes, lvalue mutation, context handling, and cleanup.

The JIT path must preserve:

- `ExceptionSink` semantics through native exception unwinding.
- Qore reference ownership for every returned value.
- Local and closure variable coherence when runtime helpers can observe or
  mutate variables outside native SSA state.
- Debugger-driven AST override in tiered mode.

Implicit `copy()` calls are constructor operations, not ordinary methods. Both
parser paths that create `SelfFunctionCallNode` mark that identity before IR
lowering, including the bare-function rewrite that calls `parseInitCall()`
directly. The flag survives argument-list cloning and directs execution through
`execCopy()`, preserving the dynamic class, copy hooks, and exception cleanup.
Normal method dispatch cannot invoke a copy variant: its calling convention is
different. `ImplicitCopyDispatch.qtest` exercises polymorphic copies and failing
copy hooks in all four execution modes.

## Tiered Execution

Tiered mode lowers `%modern` functions to IR when their Program is committed, so
they start on the IR tier; a function that is not lowered then (a closure, for
example) starts on AST. Functions are promoted by call count:

- AST -> IR after `--jit-ir-threshold` calls; the call that promotes the function
  runs on the IR tier itself (`UserVariantBase::evalTiered()`), so with a threshold
  of 1 every call runs on the IR tier.
- IR -> JIT after `--jit-jit-threshold` calls.

With `QORE_IR_TRACE_AST_TIER` set, every call that tiered execution runs on the
AST tier is reported on stderr, and `QORE_IR_TRACE_FALLBACK` reports every fall
back from IR to AST; `ExecModeHarness::run_ir_tier()` fails a test run that shows
either, so a test of the IR tier cannot pass on the AST interpreter.

Native compilation always runs asynchronously on a background thread; promotion
is non-blocking, so a function keeps executing through the IR interpreter until
its compiled form is ready. Program teardown drains only that Program's own
pending background compiles (matched via `QoreIRFunction::pgm`; a compile with no
owning program is treated as matching any teardown), so destroying one Program
never serializes behind unrelated Programs' compilation.

Process shutdown has two ordered JIT phases. It first permanently stops and drains
the background compiler, making its raw references to Program-owned IR safe before
Program teardown begins. Published native code remains mapped while user-module
delete callbacks, registered shutdown handlers, atexit callbacks, and static
destructors can still execute Qore functions. Normal `qore_cleanup()` releases the
LLVM execution engine only after all module Programs have been destroyed. A direct
`exit()` stops compilation but leaves the immortal engine for the operating system to
reclaim, because callback registration order cannot prove that no later process
callback will invoke a published entry point. This boundary prevents both compiler
work from outliving its source IR and cached native function pointers from outliving
their executable mapping, without adding checks to the runtime dispatch path.

Threads that run LLVM are started through `q_start_llvm_compile_thread()` with an
explicit 8MB stack (`QORE_LLVM_COMPILE_STACK_SIZE`), never with `std::thread`, which
cannot request a stack size and takes the platform default: `RLIMIT_STACK` (8MB) on
glibc but a fixed 128KB on musl.  LLVM's analyses recurse over the IR without a depth
bound -- ScalarEvolution recurses once per chained affine add-recurrence -- so a
default-sized musl thread overflows its stack inside LLVM, with no qore frame beneath
the LLVM frames.  Recursion depth follows the number of chained induction variables
rather than function size, so the native-compilation budget is no substitute.  This
applies to the JIT background compile worker and to the AOT codegen thread pools alike.

Failed lowering or compilation disables promotion for that variant rather than
retrying on every call. Debug attach can force dispatch back to AST so debugger
semantics remain predictable; JIT-compiled native code also emits
statement-boundary debug events (gated on `PO_ALLOW_DEBUGGER` and dormant until a
debugger attaches) so an already-running native frame stays debuggable.

### Publication of Shared Tiering Caches

Promotion writes into state that every thread executing the function shares, while
other threads are already running it. Two caches carry that traffic, and both follow
the same contract: one writer publishes with a release store, and no reader touches
the payload without first taking the matching acquire edge. The caches are strictly
optimizations, so a thread that loses a race takes the generic path for that call
rather than blocking.

`UserVariantBase::cached_ir` holds the variant's lowered IR body. It is published
exactly once, by whichever writer wins a CAS from `nullptr`
(`UserVariantBase::setCachedIR()` and `UserVariantBase::attemptIRLowering()` are both
writers, and the `ir_lower_once` flag orders lowerings against each other but gives
no edge to `setCachedIR()`, which publishes without holding it). The winner's body is
never replaced: a published generation is kept alive by `jit_owned_ir` because JIT
code embeds raw pointers into its instructions, so overwriting the pointer would
strand readers on a body nothing owns. A losing writer deletes its own body, which is
safe precisely because that body was never published. Readers go through
`getCachedIR()`, whose acquire load is what orders the pointee's initialization; a
reader that dereferences the result repeatedly hoists it into a local, because the
value cannot change once published.

The interpreter's per-instruction inline call caches (`inline_ir_state` and the
`cached_callee_ir` / `cached_uvb` / `cached_return_type` / `cached_object_class` /
`cached_class_ctx` / `cached_method` payload on the direct-call instructions) are
governed by `QoreIRInlineStateClaim` in `include/qore/intern/QoreIR.h`. A thread may
write the payload only while holding the claim it takes by winning a CAS from
`QORE_IR_INLINE_UNCHECKED` to `QORE_IR_INLINE_CLAIMED`, and may read it only after an
acquire load of the state returns a value greater than zero. A claim holder writes the
payload only immediately before publishing its verdict, so a holder that bails out
early leaves the cache untouched and returns the state to `QORE_IR_INLINE_UNCHECKED`
for a later attempt; the release is itself a CAS from `QORE_IR_INLINE_CLAIMED`, so it
cannot undo a verdict that was published. Because nothing may read the payload at
`QORE_IR_INLINE_UNCHECKED`, IR lowering does not pre-seed it -- the interpreter
resolves the same values from the variant on first execution instead. The claim is
never held across execution of the callee, and a failed claim never blocks, so a call
site that re-enters itself recursively during resolution simply takes the generic
path.

Publication safety does not make the pointees live. A call site in one Program can
call an object of a class from another Program (the ncurses widget gallery calls
`QRepl::joinEvaluation()` on an untyped object whose class lives in a child Program
that each story reset replaces), so a published dot-eval cache can name a class, method
and callee that are freed with their Program, and a later class can be allocated at the
same address (qore#5631). A raw pointer is therefore never a cache key on its own:

- The dot-eval inline cache records `qore_class_private::getInstanceSerial()` of the
  receiver's class and of the class context next to their pointers
  (`cached_object_class_serial`, `cached_class_ctx_serial`) and a hit requires both to
  match the live receiver's class and the running code's class context, which can be
  read safely because they are in use. The serial is process-unique and never reused,
  unlike the class ID (which a class copy keeps) and the address; a `QoreClass` wrapper
  sharing its private class shares the serial, so it is the same class. A matching
  serial proves that the cached class is the live receiver's, so the cached method,
  which belongs to that class or one of its parents, is alive too. A mismatch takes the
  generic path; the published state is never replaced, so the call site stays generic
  after its class is gone, exactly as for any other receiver class.
- The AOT method dispatch cache (`QoreAOTMethodDispatchCache`) keys its entries by class,
  class context and argument type pointers, so `qore_get_method_dispatch_epoch()`, which
  every entry must match, also increments when a class (`~qore_class_private()`) or a
  hashdecl (`~typed_hash_decl_private()`) is destroyed: an entry naming one is stale
  before its memory can be reused. Parameterized and derived types are purged with
  their base class, so they are covered as well. The caller and object Programs are
  keyed by their never-reused `QoreProgram::getProgramId()`, as a Program can be
  destroyed without destroying a class.

`examples/test/ir/InlineMethodCacheClassLifetime.qtest` runs persistent call sites over
destroyed Programs and, with a debug build, the C++ unit tests
(`dbg_run_method_cache_lifetime_tests()`), which model the reused address
deterministically by giving a published entry the serial of an earlier class.

## AOT Compiler

`qcc` is the dedicated AOT compiler. It supports:

- Script executables.
- User modules as `.qmod`.
- Per-file `.qo` object files.
- `.qoa` archives for C/C++ hosts.
- Link mode: `qcc -o binary *.qo`.
- Metadata inspection: `qcc --dump-info`, `--dump-symbols`,
  `--dump-sections`.

AOT metadata uses the QORD binary format. Feature flags protect runtime
compatibility: readers reject artifacts requiring unsupported IR/AOT features.

For the object-file and module artifact contract, see
[`aot-object-files-and-module-artifacts.md`](aot-object-files-and-module-artifacts.md).

For multi-file script/application AOT, see
[`aot-script-context.md`](aot-script-context.md).

### Parallel Backend Codegen (split-after-opt)

`qcc` can parallelize LLVM backend codegen for a single compilation. It optimizes
the whole module once (so cross-function optimization is unaffected), then
`llvm::SplitModule(PreserveLocals=false)` partitions the optimized module into 4
(`QORE_AOT_SPLIT_PARTS`) balanced pieces along whole-function boundaries, codegens the
pieces concurrently on worker threads (each piece with its own `LLVMContext` via a
bitcode round-trip, since a `Value*` may not cross contexts, plus its own
`TargetMachine`), and `ld -r` partial-links the per-partition objects, in partition
order, back into one relocatable object.
`SplitModule` externalizes internal symbols, so after the merge qcc re-internalizes
the extras (ELF: `objcopy --localize-symbols`) to keep the split object
symbol-equivalent to the single-object build.

- The partition count is fixed, so the object depends only on the module: identical
  source gives a byte-identical qmod whatever the build's load, the machine's CPU count
  or the number of codegen threads. When the partition count followed the threads
  available (the jobserver tokens free at that moment, or the CPU count), 33 of 391 qlib
  qmods differed between two clean builds of the same source. Measured on DataProvider,
  the largest qlib module: 4 parts compile in 27.4 s on free cores against 39.3 s for a
  single object (16 and 32 parts: 25.5 s and 25.8 s), and 4 parts compiled one after
  another on one core take 43.3 s.
- The fixed partition count made split modules independent of the threads; it did not
  cover the objects of a batch compile (`qcc -c` with several sources), which still
  differed with the number of batch workers and between two parallel runs (7 of 262
  objects of a Qorus batch): the IR of each `on_block_exit` handler was named from a
  process-wide counter, and the IR serialized into the object carries that name.  A
  handler is now named `handler_<n>@<function>`, numbered within the function it belongs
  to (`qore_ir_next_handler_name()` in `lib/QoreIRLowering.cpp`), and
  `examples/test/ir/AOTBatchDeterminism.qtest` compares serial and parallel batch objects.
- `--jobs=N` selects the maximum number of codegen threads (default: CPU count, capped
  at 32; `1` compiles the parts one after another). `qcc` propagates it to the backend
  via the `QCC_JOBS` environment variable. It never changes the output.
- Splitting is gated by a size threshold (`QCC_SPLIT_THRESHOLD`, default `50000`
  optimized-IR instructions — a CPU-speed-invariant metric) so trivially small
  modules are never split.
- Concurrency is bounded by a **GNU make jobserver client**: qcc parses `MAKEFLAGS`
  and acquires tokens for the codegen threads; if no spare tokens are available (a
  saturated `make -jN`), it compiles the parts one after another on its own slot so
  a parallel build is never oversubscribed. The client supports both the make 4.4+
  named-pipe form
  (`--jobserver-auth=fifo:PATH`) and the older inherited-fd form
  (`--jobserver-auth=R,W` / `--jobserver-fds=R,W`); each qcc process owns one
  implicit job slot and uses extra codegen threads only for tokens it can acquire
  non-blockingly. This makes parallel codegen safe to leave default-on under
  `make -j`, and the tokens never change the output.
- `QORE_AOT_SIZE_DEBUG` traces the module instruction count and the split decision.
- On macOS a module is linked with its final path as its install name and without a
  debug map (`-Xlinker -S`): it is linked to a temporary file whose name has the process
  id in it, and its object files are deleted after the link, so the default install
  name and the debug map (object paths and modification times) made every link
  different. ELF modules have no soname and keep their debug information.

## User Module Build Behavior

`QORE_BUILD_AOT_MODULES` defaults to `ON`. The CMake build compiles user
modules to `.qmod` artifacts with `qcc` and installs those artifacts alongside
the source modules. Module lookup prefers the compiled `.qmod` over the `.qm`
when both are present in the same module location.

This makes the installed default fast while preserving source modules for
documentation, inspection, and development.

## Lowering And Fallback Policy

IR/JIT/AOT lowering must be fail-closed. Unsupported expressions,
instructions, or metadata combinations must produce diagnostics with enough
opcode, node, type, and source-location context to implement native lowering;
they must not silently emit `EXPR_TREE`, `GENERIC_EVAL`, or source fallback.

AOT artifacts require complete serialized metadata. Source text can be embedded
for diagnostics or inspection, but it is not a runtime fallback for missing AOT
metadata.

### Lvalues

Every lvalue that `check_lvalue()` accepts is lowered to an lvalue path
(`extractLValuePath()` in `lib/QoreIRLowering.cpp`, executed by
`LValueHelper::navigatePath()`): a root (local, closure-bound, global, thread-local
or static variable, or a member of `self`), then navigation steps (a hash key or object
member, a list index) and `Cast` steps, which check the value navigated to so far as the
AST checks `cast<T>(lvalue)` and keep the position. Every operator that modifies an
lvalue, apart from the fast paths for plain locals and their subscripts, emits an
`LValuePath*` instruction; if the path cannot be built, lowering fails. Lowering emits no
AST-evaluated lvalue operation (`StoreLValue`, `AddAssignLValue`, ...): those only
evaluated the lvalue's AST, keys included, at runtime.

A slice (a hash slice, a list slice or a range) is an lvalue only as the whole lvalue of
`remove` and `delete`, which take it as the terminal step of the path; `check_lvalue()`
rejects any other slice lvalue at parse time, and a key or index that is a list only at
runtime raises `ILLEGAL-SLICE` in every engine (`qore_check_lvalue_key_not_slice()`).

An expression statement is lowered as any other expression, also in a try block: the
operators that can raise an exception there are lowered with their own opcodes by
`lowerExprOpOrInvoke()`.

## Variables Other Threads Can Change

A global variable, and a local variable that is passed by reference or captured by a
closure (see `closure-bound-locals.md`), can be changed by other threads at any time.
The AST operators hold the variable's lock for the whole of an operation that reads and
writes it (`++`, `+=`, `-=`, ...), so no change made by another thread is lost, and they
read the variable again for every access. Every engine must give the same guarantees:

- IR lowering never lowers such an operation on such a variable as a load followed by a
  store (`isSharedVar()` in `lib/QoreIRLowering.cpp`). The compound assignment operators
  take the lvalue path (`LValuePathCompound`),
  typed integer pre- and post-increments and decrements take `IncrementLocalInt` (for a
  captured local, which every engine applies under the variable's lock) or
  `LValuePathUnary`, and the subscript fast paths (`isConstKeyHashSubscript()`,
  `isDynamicKeyHashSubscript()`, `isConstIndexListSubscript()`), which load a container,
  change it and store it back, only apply to plain locals.
- The IR interpreter caches no value of such a variable: `LoadGlobal` and `LoadClosure`
  read the variable every time, and `StoreGlobal` and `StoreClosure` only write it. Before
  this, a thread's loop incrementing a global worked on its own cached copy and overwrote
  the other threads' increments, and a loop polling a captured variable never saw another
  thread's write. Thread-local variables are not cached either (`LoadThreadLocal` reads
  the variable, `StoreThreadLocal` only writes it): the lvalue paths change a variable in
  place (`t.a = 1`, `t[0] += 1`, `push t, x`) without going through `StoreThreadLocal`, so
  a cached copy went stale (`examples/test/ir/ThreadLocalLValuePath.qtest`).

`examples/test/qore/threads/shared-var-updates` checks each of these operations from
several threads at once, and a loop seeing another thread's write, in every execution
mode and in a program compiled with `qcc`.

## Compound Assignment Results

A local or thread-local variable that only its own thread can change is changed by
loading it, computing the new value and storing it. The store applies the variable's
type, which can convert the value (an `any` or `auto!` variable stores a `hash<HD>` as a
plain `hash`, a `softint` variable stores 17.5 as 17), so the result of the operator must
be what the AST operator returns:

- `+=`, `-=`, `*=` and `/=` return the value the lvalue holds afterwards, as an assignment
  does (`x *= y` gives what `x = x * y` gives): when the result is used, the variable is
  read again after the store (`QoreIRLowering::compoundAssignmentResult()`), and the
  lvalue path's helper (`qore_rt_apply_lvalue_compound()`) returns the lvalue's value.
- The integer operators (`%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) apply only to lvalues that
  accept and return an integer (`check_lvalue_int()`), which store the integer computed as
  it is, so they return the value they compute.

`examples/test/ir/CompoundAssignmentMatrix.qtest` checks every operator on every kind of
lvalue, with typed and `auto` lvalues and operands, against the AST interpreter's output.

## IR Analysis And Optimization

Lowered functions carry transient facts for each SSA value: known type,
assigned state, NOTHING exclusion, and runtime representation. Assignment is a
separate fact from the declared type. In particular, a typed lvalue is not
assigned before its first write and becomes NOTHING again after remove/delete;
reference parameters retain reference assignment semantics while value reads
use the declared target type.

The LLVM `ListSize` lowering hoists typed-list data pointers only on the
nonempty path. A declared list may still be `NOTHING`, including a local that
is conditionally assigned inside an outer loop. The zero-size path carries a
null data pointer, which the loop's bounds check prevents from being read.
This preserves zero-iteration behavior without adding a check to each element
read. `AOTTypedForeachNothing.qtest` covers these locals, removed/deleted lists,
empty and populated lists, and sparse-list type errors across execution tiers
and source-stripped AOT artifacts.

`QoreIRAnalysis` provides normalized SSA-operand and normal-successor visitors,
plus reusable reachability, predecessor/successor, dominator, and natural-loop
analysis. Generic operands carry phi inputs and dynamic lvalue-path and slice
values, while the visitor also covers operands stored only in dedicated
instruction fields. The IR interpreter uses it for ownership use counts,
preventing dedicated instruction fields from diverging from generic operand
handling.

LLVM call-argument cleanup must preserve a temporary while its SSA value has
remaining uses. Virtual implicit arguments can share one mapped value across
several calls, as in `map (f($1), g($1)), (map make($1), xs)`. Only the final
use may pass the temporary's cleanup slot to a consuming call helper; earlier
calls borrow it. `AOTSharedMapArgs.qtest` covers this lifetime through calls,
branches, duplicate arguments, and exception unwinding.

The first generic optimization built on this layer is conservative scalar
loop-invariant code motion. It moves only native scalar constants, proven
assigned and non-NOTHING non-reference IR-only local loads, and non-throwing
native scalar operations. A local written in the loop is never hoisted. Loops
must have a unique unconditional preheader, and the preheader must precede the
loop in IR block-list order while the LLVM emitter still resolves ordinary SSA
operands in that order. Every hoisted result must have only repeated,
non-consuming scalar users inside the loop. Loads are not hoisted across calls
or other operations that can invalidate interpreter local slots. These rules
preserve the runtime ownership and assigned-state lifetime represented by each
SSA slot. The post-optimization verifier is mandatory for both runtime and AOT
source lowering. `QORE_DISABLE_IR_LICM=1` disables only LICM, while
`QORE_DISABLE_IR_OPT=1` disables all IR passes; `QORE_IR_OPT_STATS=1` reports
analyzed loops and hoisted instructions.

After LICM, a same-basic-block scalar pass forwards repeated loads of IR-only
locals and eliminates repeated native scalar constants and arithmetic or
comparison expressions. Load forwarding requires the value facts to prove an
assigned, non-NOTHING native `int`, `float`, or `bool`; declared type alone is
not sufficient. Explicit local writes invalidate that local, while references,
calls, lvalue operations, scope exits, and other operations that can mutate an
unknown local invalidate all available loads. Discovery is cancellation-safe
and no definition is erased until all replacements have been selected. A value
is merged only when every consumer is a non-consuming scalar operation or an
IR-only local store; call arguments, runtime-visible stores, phi inputs, and
lvalue paths retain distinct SSA ownership. Set `QORE_DISABLE_IR_CSE=1` to
retain the unoptimized same-binary path; the `QORE_IR_OPT_STATS=1` output
includes forwarded-load and eliminated-expression counts.

Work an engine does on its own behalf in the thread of a call (the IR interpreter's
analysis of a function on its first IR call, compilation) is never a cancellation point
that raises: a thread whose cancellation is pending, such as cleanup code that caught
`THREAD-CANCELLED`, gets the exception at the program's next check point (a loop, a
blocking call), as with the AST interpreter, not from the entry of a function that one
engine happens to analyse there (`examples/test/ir/PendingCancellationFirstCall.qtest`).

The optimizer also replaces a conditional branch whose SSA condition is a
literal boolean with an unconditional branch. It deliberately leaves the
unreachable block and phi structure intact; native backends remove that code,
while the IR interpreter avoids repeated condition dispatch in constant-path
loops. `QORE_DISABLE_IR_CONST_FOLD=1` retains the conditional form for
same-binary comparisons, and optimization statistics report folded branches.

Recognized builtin and pseudo-method operations also carry a stable
`QoreIRIntrinsic` identity. The builder assigns the identity when it creates a
resolved pseudo call, and artifact readers reconstruct it once from serialized
method metadata. Interpreter and LLVM fast-path selection dispatch on this
identity instead of repeatedly comparing class and method names. Runtime name
dispatch remains the fallback for unresolved or unsupported calls and retains
precedence for objects and hash-like values whose members can override pseudo
methods.

The IR interpreter also computes a transitive external-cache effect summary for
resolved direct user-function calls. Local writes to globals, thread-locals,
closures, references, and unknown call forms remain immediately effectful.
Known direct-call edges are solved to a fixed point, so pure recursive call
components are recognized without recursive-analysis guesses. Missing callee
IR, reference arguments, direct methods, and closures remain conservative. The
summary is published atomically after the complete reachable call graph is
known; until then callers invalidate caches as before.
`QORE_DISABLE_IR_EFFECT_SUMMARY=1` keeps the conservative direct-call policy for
same-binary comparisons, and `QORE_IR_EFFECT_SUMMARY_STATS=1` reports each
resolved graph's size and result.

## Validated Registries

The current implementation relies on explicit registries for extension safety:

- Opcode metadata is validated before IR verification.
- IR expression lowering uses explicit claim predicates; a handler that claims
  a node must either return IR or report why native lowering is impossible.
- AOT expression-slot, expression-node, and instruction-group registries are
  validated before serialization; unknown instruction subclasses fail
  serialization with the dynamic C++ type name.
- JIT runtime helpers are listed in one registry in `JITRuntime.cpp`; the JIT
  validates names, addresses, and duplicates before registering ORC symbols.
- Type-spec match dispatch validates complete, duplicate-free coverage of the
  closed built-in `q_typespec_t` set before dispatch.

## Current Hardening Rules

- Runtime-only changes should not force all `.qmod` files to rebuild. The
  `qcc-format` stamp tracks sources that affect emitted qmod format or compile
  semantics.
- Split-directory module qmods must live beside their resource files so
  `get_script_dir()` remains valid during AOT initialization.
- AOT local-slot tables preserve IR local slot IDs for deferred handler
  execution.
- Expression-tree member defaults are materialized after constants are
  registered so class and namespace constants can be referenced safely.
- `.qo` metadata preload creates declaration shells only; it must not execute
  user code during compile-time name/type resolution.
- Batch AOT registration must keep cross-session barriers: shells, type/base
  resolution, constants, members, functions, then final class fixups.
- A constant whose `__const_init` did not populate it at load must be
  recoverable from its first read, and a read that cannot recover must report
  why the initializer failed rather than only that the value is missing. See
  [`aot-constant-init-recovery.md`](aot-constant-init-recovery.md).

## Exception Source Locations

AOT-compiled code reports the source file and line of an exception and of each
call stack frame as AST execution does, with or without debug info and source
text. Each source location is a constant `QoreProgramLocation` in the image,
one per distinct location of the module (`QoreIRToLLVM::getAotLocationConstant()`),
and the code stores its address in the thread's runtime location when the line
changes, as JIT code stores its location pointer; the location does not depend
on the AOT context the code runs with, so code that runs with its caller's
context (a context-independent fast entry) reports its own lines. Every direct
call of user code and every call that the code runs inline as a summary of the
callee's body pushes the callee's frame; see "Frames of native calls" in
[`call-stack-publication.md`](call-stack-publication.md).

## Exception Cleanup Dominance

LLVM `invoke` results that own Qore references may be cleaned up directly from
SSA only when the defining block dominates every cleanup use. The general path
therefore keeps the safer cleanup-alloca representation unless dominance is
proven.

Do not relax this rule without a real dominator-tree or equivalent
scope/finalization design. See
[`aot-eh-cleanup-dominance.md`](aot-eh-cleanup-dominance.md).

## Publishing Locals Before Block-Exit Handlers

Before compiled code runs the `on_exit`, `on_success` or `on_error` handlers of
a block (at a scope exit, including the exit of a `try` block, and on the
exception paths), it publishes the locals it caches in LLVM allocas to the
runtime local stack (`syncLocalsToRuntimeForHandlers()`) and seeds the native
handler slot cache (`beginNativeHandlerSlotCache()`). Only a local whose alloca
is authoritative may be published. A weak-assigned local (a `foreach` variable
that is not IR-only, or a local assigned with `:=`) lives on the runtime stack:
every store writes it there, every load reads it from there, an lvalue-path
operation on it (`item.key = v`, `item[i] = v`, `delete item.key`,
`item += v`) changes only the runtime value, and its alloca is never reloaded.
Both functions therefore skip it, as they skip closure-use locals. Publishing
its alloca lost every in-place change made in a `try` block of a function with
a handler, and gave the loop body's own handler the old value
(`AOTWeakLocalHandlerSync.qtest`).

## Block-Scoped IR-Only Local Lifetime

An IR-only body local never has a runtime local-stack entry: the call wrapper
skips it and `StoreLocal` neither instantiates nor publishes it. In LLVM code a
boxed IR-only local that is neither a fast-entry parameter nor backed by a
pre-instantiated cleanup slot owns its current value in its local alloca; every
write that replaces that value stores an owned reference.

Two rules keep its lifetime lexical, matching the AST interpreter:

- `UninstantiateLocal` releases the local at every scope exit (normal end,
  `break`, `continue`, `return`, exception cleanup): it drops the owned alloca
  value and the local's cleanup slots and resets the alloca, but touches no
  runtime stack slot. The lazy-instantiation path, which pops a runtime stack
  entry, must never handle these locals: it returns early for a local that
  `StoreLocal` did not instantiate, which delayed destructors to function exit.
- Ownership is registered when `preCreateLocalAllocas()` creates the alloca,
  before any block is lowered, not when the first owning store is lowered.
  Blocks are lowered in layout order, so an exit can be lowered before a store
  that executes ahead of it (a `return` or `throw` after a `switch` whose case
  assigns the local); its inline cleanup would otherwise omit the local and leak
  the value.

Object-typed locals whose type names a unique class stay on the runtime stack, but
`*Class`, `auto`, and container-typed locals holding objects are IR-only, so
destructor timing depends on these rules. As IR-only locals, object-typed locals ran a loop
holding an object 2.5 times faster, but an object in a recursive graph held by such a local (a
Queue that the values pushed to it refer to) made the collector decide at every dereference of
the object, and such a loop ran 1.5 times the instructions of the runtime-stack variable; the
cause is not known yet (see `computeIROnlyLocals()`). A third rule completes the lexical
lifetime of IR-only locals: when a function is left, the values they own are released last
local first, as the AST interpreter releases the variables of a frame. When an exception leaves
the function, IR-only locals and runtime-stack locals are released together in one sequence, last
declared first: releasing all IR-only locals before the runtime-stack locals ran destructors out
of the AST order (a destructor that unlocks a lock held by an `AutoLock` released first raised
`LOCK-ERROR`). The IR interpreter's `SlotCacheCleanup` and `cleanupLocalCaches()` walk the slots
in reverse, a local's slot being assigned where it first appears, and release a runtime-stack
local at its slot (`releaseFrameRuntimeLocal()`, using `QoreIRFunction::interpreter_slot_locals`).
That map holds only the function's own body locals: a parameter is released by the caller after the
function's handlers, and an `on_block_exit` handler or closure body run as an IR function of its own
has slots for the locals of the function it belongs to, which that function's handlers still read
(releasing them there emptied them before an outer `on_error` handler ran).
Native code builds the sequence in `QoreIRToLLVM::getExitReleaseSequence()`: body locals in
declaration order, an owned IR-only cache as an alloca and a runtime-stack local as its `LocalVar`
(its native cache allocas follow it, so they are released before the runtime slot is cleared by
`qore_rt_clear_local()`). The inline form releases the sequence in reverse; the array form
(`qore_rt_cleanup_run_allocas()`, used above `QORE_JIT_CLEANUP_PHI_BUDGET`) stores it in order
with a runtime-stack local tagged with bit 1 and releases it last entry first. The inline form
includes runtime-stack locals only at an exception exit; the array form, shared by every exit,
includes them always, and a local already released at a normal exit is released again as no value.
AOT code includes no runtime-stack locals (the runtime stack releases them in that order).
Closure-bound body locals are in the sequence too, in JIT and AOT code: one that the frame
instantiated is popped from the closure stack at its place (`qore_rt_pop_closure_var()`, tag 3 in
the array form, which looks it up in the current frame only), so that a closure still holding it
keeps it and releases its value with an exception sink when the closure is released; one that the
caller instantiated (`pre_instantiated_locals` of a function entered by `evalTiered()`) only loses
its value if no closure holds it (`qore_rt_clear_local()`), as the caller pops it. AOT code knows a
closure-bound variable only at runtime: the array form reads it from the context at the function's
entry (`qore_rt_aot_local_var()`). Left to the frame boundary, such a variable was released after
the frame's other locals and without an exception sink, which lost an exception raised by a
destructor (a debug build aborted). The IR interpreter clears a closure-bound slot's variable at its
place when no closure holds it; its frame instantiation cleanup pops it afterwards.
The IR interpreter also releases the iterator of a foreach statement still running at its place:
the AST interpreter releases the value a foreach statement iterates when the statement ends, after
the locals of the loop body and before the loop variable and the locals declared before the
statement. `computeSlotIdsAndEmbed()` records, for each instruction creating such an iterator, the
slots assigned before it, and the loop variable's slot if it is the first assigned after it
(`QoreIRFunction::interpreter_iterator_slot_boundary`); `SlotCacheCleanup` and
`cleanupLocalCaches(false)` release an iterator after the slots from that boundary on
(`IRActiveIterator`, `releaseActiveIteratorsFrom()`), and the iterator guard only those left.
`ExceptionExitLocalOrder.qtest`, `ClosureLocalExitRelease.qtest` and
`ForeachExitIteratorOrder.qtest` check the AST order in every mode, a qcc executable included.
`IRBlockScopedObjectLifetime.qtest` and `ObjectLocalLifetime.qtest` check exact
constructor/destructor order for these shapes in every tier and in AOT (block exits, loops,
returns, exceptions within and across functions, reassignment, `AutoLock`, recursive graphs).

### References taken by compiled code

Each reference taken to an object and released is a dereference, and the dereference of an
object in a recursive graph is a garbage-collection decision that takes the object's locks. The
engines take no reference the AST interpreter does not take:

- a temporary argument that a call consumes is released right after the call's argument list is
  built, before the call (`clearConsumedArgCleanups()`; the IR interpreter passes the arguments a
  method call consumes the same way): released then, a temporary object is linked to nothing yet,
  and its dereference needs no collection decision, while the list's release after the call can be
  one (giving the temporary's reference to the list made an AOT loop pushing such values 13%
  slower); a constructor called from compiled code gets its argument list as a temporary
  (`execConstructor(..., tmp_args)`) instead of copying it;
- a temporary stored in an IR-only local, its only use, gives its reference to the local;
- in the IR interpreter, a load of an IR-only local whose only use is the base of a method call
  in the same block, with no write to the local between, borrows the local's value when it is an
  object (`interpreter_borrowed_bases`). A container is never borrowed (code receiving one with a
  single reference can take it over or change it in place), and neither is an argument (a direct
  or inlined call can take its argument's slot over as its parameter;
  `BorrowedLocalArguments.qtest`).

`benchmark_exec_modes.qr` (object_local, object_list_index, new_object_member, builtin_method,
queue_cycle) measures these patterns in every mode.

## Large Int Ownership in Fused Local Updates

With NaN boxing an int outside the 48-bit immediate range is a heap node, so any
code that produces a boxed int must give that reference an owner. Fused local
int updates (`AddAssignLocalInt`, `IncrementLocalInt`) follow `StoreLocal`:

- LLVM code hands a boxed result to whatever owns the local's value: the alloca
  of an IR-only local or fast-entry parameter (releasing the value it replaces),
  or the pre-instantiated cleanup slot the alloca borrows from. For a
  runtime-stack local the alloca only caches: an immediate is stored as is, a
  heap node is retained by the local's reload tracker with the usual deferred
  release, and the runtime assignment takes its own reference.
- The IR interpreter releases the slot-cache value it overwrites, uses the
  transferring assignment helpers so the variable consumes the new value, and
  registers the result slot for cleanup.
- An operand that a fused instruction only reads (the source of `AddAssignLocalInt`,
  both operands of `BranchIfLtLocalInt`) keeps its value: its slot may only cache the
  value the variable holds (as `LoadLocal` caches it), never the int it is read as, or an
  `int` local with no value would read as 0 afterwards; the slot of an IR-only local that
  the cold path marks instantiated is the variable itself
  (`examples/test/ir/FusedIntOperandValue.qtest`). `AddAssignLocalInt` takes only `int`
  sources, and neither instruction takes a reference-typed local, which reads as the
  referenced lvalue; `LoadLocal` does not cache the value of a local holding a reference
  (`examples/test/ir/ReferenceLocalRead.qtest`).

### Locals of no reference type that hold a reference

A local of a type that can hold a reference (`auto`, `any`, no type) holds one whenever a
value that is a reference is stored in it: an assignment of `\x`, of a reference element of
a container (`auto r = h.r`), of a reference returned by a call, or an argument `\x` given
to such a parameter. The AST interpreter then reads and writes the referenced lvalue
through the variable. The engines keep the same semantics without slowing locals that never
hold a reference (`examples/test/ir/ReferenceLocalReach.qtest`,
`examples/test/ir/ReferenceLocalStorage.qtest`):

- `QoreIRFunction::computeReferenceStoreLocals()` (QoreIRVerifier.cpp, run first in
  `computeIROnlyLocals()`) finds `reference_store_locals`, the locals that a `StoreLocal`
  may give a reference: a stored value whose definition is not an opcode that never gives
  one (`qore_ir_opcode_never_gives_reference()`: constants, loads of variables, new
  containers and objects, arithmetic, comparisons, conversions, `ListPush`...; an `Invoke` is
  judged by its opcode), followed through `Phi`, guards, `RefSelf` and `AddAssignAny`, whose
  value is its right operand as it is when the variable has no value (`auto t; t += h.r;`,
  `doPlusEqualsOnLValue()`; `qore_ir_opcode_forwarded_operand()`). They are
  reference-capable (`localMayHoldReference()`, the `is_ref` of their instructions), which
  keeps the optimizations keyed on `is_ref` away from them. Adding an opcode that can give a
  reference to the never-gives list is a correctness defect; leaving one out only costs
  speed. A debug build of the IR interpreter asserts that a plain store to an IR-only local
  that is not a reference slot local never stores a reference, which catches such a defect
  in the IR test suite. The values that the analysis finds never to be a reference
  (`non_reference_values`) let native code skip checking the value stored to a local with a
  reference flag (ex: a string constant).
- The variable of a foreach loop is given each element as it is (`AssignmentMode::Element`);
  an element can be a reference, which the variable reads through. As in the AST
  interpreter (`ForEachStatement`), the assignment of an element to a variable that holds a
  reference, given by an earlier element, assigns the referenced lvalue, in the IR
  interpreter and in native code (`qore_rt_element_local_replace()`). A loop variable that
  is also assigned in the loop is in `reference_store_locals` if an element can be a
  reference, and stays on the runtime stack.
- The value of an assignment or of `+=` to a variable of no type is the value the variable
  holds afterwards, as it is, which is the reference that a reference value made it hold;
  reading the variable again gives the referenced value instead. Such an assignment whose
  value is used is lowered as an lvalue path operation when the value assigned can be a
  reference (`QoreIRLowering::assignmentValueMayBeReference()`).
- A body local of `reference_store_locals` that would otherwise be IR-only, in a function
  that creates no reference, is a reference slot local (`reference_slot_locals`): it keeps
  its value in IR storage as an IR-only local does (the slot of the IR interpreter, the
  native cache), and a reference it holds is that value. A read reads the referenced lvalue
  with `qore_local_reference_eval()`, which evaluates the reference's lvalue expression as
  `LocalVarValue::eval()` does (never `ReferenceNode::eval()`, which resolves it as an lvalue
  and creates a missing hash key); a store that is not the local's declaration
  (`initial_assignment`) assigns the referenced lvalue through an `LValueHelper` on the
  reference (the IR interpreter; `qore_rt_reference_slot_write()` in native code), and the
  local keeps the reference. No variable of the function can be the referenced lvalue, as a
  reference to one is created only by `\var` in the function. Reference slot locals are not
  in `ir_only_locals`, so the IR optimizations keyed on that set (CSE, LICM, native SSA
  promotion, CoW pairing), which take an IR-only local's storage to be its value, leave
  them alone; the engines and the instantiation lists treat them as IR-only
  (`hasIRStorage()`, `areAllBodyLocalsIROnly()`), and the debugger clears them with
  `ir_only_locals`. Every other local of `reference_store_locals` stays on the runtime
  stack, whose variable follows the reference.
- Native code (JIT, tiered, AOT) keeps a reference flag for each local that can hold a
  reference and whose cache it owns (`QoreIRToLLVM::usesReferenceFlag()`): a parameter of
  such a type, a local of `reference_store_locals`, and any such local of a function that
  creates a reference. The flag is an i8 alloca that is never given to a helper, so the
  optimizer promotes it to a register. It is set wherever the cache is filled from the
  runtime: at entry (`qore_rt_load_local_raw()`, which returns a reference itself, and
  `qore_rt_value_is_reference()`; fast-entry parameters from the argument bits), on a stale
  reload (`qore_rt_reload_local_if_stale_raw()`, which returns the flag), and by a store.
  `LoadLocal` with the flag set reads the referenced lvalue (`qore_rt_read_reference_local()`);
  the branch is weighted as unlikely. A store to a runtime-stack local with the flag set
  reloads the cache and advances the reload epoch, so a write through the reference is seen
  by every other local; an AOT body local assigned a value as it is keeps the stored value
  in its cache unless the store wrote through a reference. A store to a reference slot local
  with the flag set writes through and advances the epoch; otherwise the cache takes the
  value and the flag is set from it (only a node is checked). `UninstantiateLocal` clears a
  reference slot local's flag with its cache. The flag is loop invariant in a loop that does
  not store the local, so LLVM's loop unswitching can hoist the test; with the default cost
  threshold (50) a loop with debugger step blocks is usually too large, and the read then
  costs one compare and branch on a register.

Code that only inspects an lvalue must not read it through `getValue()`, which
creates a new heap node for a large int stored natively;
`LValueHelper::getInternalNode()` returns the stored node without creating one.
`IRFusedIntLocalOwnership.qtest` checks values across the immediate boundary and
checks that repeated updates leave no heap int behind in every tier and in AOT.
Both this test and `NanBoxDoubleTagCollision.qtest` count the heap nodes
themselves: debug builds count the live `QoreBigIntNode` and `QoreBigFloatNode`
objects (`dbg_get_live_bigint_count()`, `dbg_get_live_bigfloat_count()`), and a
promoted call must leave the counts unchanged.  The leak checks need library
debugging and are skipped without it; the value checks always run.  A negative
control retains 200,000 boxed doubles and verifies that the float count rises by
exactly that many.

The checks used to compare the resident set size (`/proc/self/smaps_rollup`)
with a 2 MiB limit.  That is not a measure of leaked nodes: where transparent
huge pages back the heap, the first touch of one new page makes a whole 2 MiB
page resident, so a leak-free run that touched 4 kB of new memory reported
2052 kB (CI pipeline 62463, job 218748, arm64; job 206719 of pipeline 58541
reported exactly 2048 kB).  The resident set size also depends on the page size,
the allocator and what other threads allocate.

## Large Function Bodies and Cleanup Scaling

Every per-statement exception and thread-exit check in a function branches to
one shared `error_return` block, and that block releases each IR-only local that
owns its value in its own alloca. Emitting one `load` / `store NOTHING` /
`qore_rt_decref` triple per local there is correct but does not scale: SROA
promotes those slots to SSA, so each local then needs a PHI in `error_return`
for *every* edge reaching it. The PHI count is therefore
`IR-only locals x exit edges`, PHI elimination expands each PHI operand into a
machine `COPY`, and LLVM's register coalescer and allocator go superlinear on
the result. A 150-statement body produced 151 PHIs across 751 predecessors —
113k PHI operands and 235k `COPY`s — and took over ten seconds to code-generate;
at 400 statements it did not finish in two minutes. The same body lowered
without the LLVM optimizer, where the slots stay in memory, code-generated in
0.7 seconds, which is what identifies the cleanup shape rather than the function
size as the cause.

This is the same shape clang avoids in its own EH cleanups: clang keeps cleanup
state addressed through memory (`cleanup.dest.slot`, destructor objects
referenced by their alloca address) and emits each cleanup once, so no
cleanup-referenced value ever becomes a promoted SSA value needing a PHI per
exit. Qore now does the same thing where it matters.

`QoreIRToLLVM::prepareBoxedExitCleanupArray()` measures both factors at function
finalization — the number of boxed exit-cleanup slots and the predecessor count
of `error_return_block` — and, only when their product exceeds
`getCleanupPhiBudget()` (`QORE_JIT_CLEANUP_PHI_BUDGET`, default 4096), records
the slot addresses in an entry-block array and replaces the whole triple
sequence with a single `qore_rt_cleanup_run_allocas()` call. Taking the slot
addresses keeps them in memory, so no PHI is needed and code generation stays
linear. The slot set is everything `emitPreinstantiatedCleanup()` releases, in
emission order: pre-instantiated entry cleanup slots, fast-entry parameter
slots, then the IR-only locals that own their value directly — covering only one
of the three leaves the other two to rebuild the same web. Every function below
the budget — which is every ordinary function — keeps the inline triples and
full SSA promotion of its slots, so the trade-off is confined to bodies where
promotion could not pay off anyway.

`prepareIteratorCleanupArray()` applies the identical treatment to
`iterator_cleanup_allocas` and `qore_rt_iterator_cleanup_allocas()`. Iterator
cleanup is written both in the shared error-return block and, by
`emitLateExitCleanup()`, at every `ret`/`resume`, so its multiplier is the same
edge count.

A third instance of the pattern lives on the temp-boundary path.
`flushLocalReloadStateAtTempBoundary()` emitted a two-block NOTHING test per
tracked local at *every* temp boundary, so a function paid
`tracked locals x temp boundaries` basic blocks: the reflection test's
1621-line `namedCallReflectionTests()` reached 88062 rotations — 95% of its
184933 basic blocks and 74% of its instructions, before the optimizer ran at
all — and could not be code-generated in half an hour. Above
`getReloadChainCallThreshold()` (`QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD`, default
32 tracked locals) the same test and rotation run inside
`qore_rt_reload_chain_rotate()` / `qore_rt_reload_chain_clear()`, which emits one
call and no extra block. Smaller functions keep the inline form so LLVM can fold
it away.

Together these took that method from an unbounded hang to 29s.

Two details matter for correctness. The runtime helper releases slots in LIFO
order, so the addresses are stored in reverse and the values are released in
exactly the order the inline sequence used; destruction order of locals is
observable through destructors. And the choice is made before any cleanup is
written, so the shared error-return block and the shared EH common-cleanup block
agree on which form they emit.

The IR-side dataflow analyses that feed this path (`qore_ir_mark_local_list_pushes()`
and `qore_ir_mark_in_place_string_appends()`) use `QoreIRLocalBitSet`, a dense
bit vector, as their lattice element. A hash set copies one node per element on
every block merge and transfer, which costs `O(blocks x candidate locals)`
allocations and was the largest remaining cost of lowering a very large body.
