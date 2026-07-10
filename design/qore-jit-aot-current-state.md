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

## Tiered Execution

Tiered mode starts functions on AST and promotes by call count:

- AST -> IR after `--jit-ir-threshold` calls.
- IR -> JIT after `--jit-jit-threshold` calls.

Native compilation always runs asynchronously on a background thread; promotion
is non-blocking, so a function keeps executing through the IR interpreter until
its compiled form is ready. Program teardown drains only that Program's own
pending background compiles (matched via `QoreIRFunction::pgm`; a compile with no
owning program is treated as matching any teardown), so destroying one Program
never serializes behind unrelated Programs' compilation.

Failed lowering or compilation disables promotion for that variant rather than
retrying on every call. Debug attach can force dispatch back to AST so debugger
semantics remain predictable; JIT-compiled native code also emits
statement-boundary debug events (gated on `PO_ALLOW_DEBUGGER` and dormant until a
debugger attaches) so an already-running native frame stays debuggable.

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
`llvm::SplitModule(PreserveLocals=false)` partitions the optimized module into N
balanced pieces along whole-function boundaries, codegens each piece concurrently on
a worker thread (each with its own `LLVMContext` via a bitcode round-trip, since a
`Value*` may not cross contexts, plus its own `TargetMachine`), and `ld -r`
partial-links the per-partition objects back into one relocatable object.
`SplitModule` externalizes internal symbols, so after the merge qcc re-internalizes
the extras (ELF: `objcopy --localize-symbols`) to keep the split object
symbol-equivalent to the single-object build. Output is byte-deterministic wherever
the single-object build is.

- `--jobs=N` selects the number of codegen threads (default: CPU count, capped at
  32; `1` = single-object, the reference path). `qcc` propagates it to the backend
  via the `QCC_JOBS` environment variable.
- Splitting is gated by a size threshold (`QCC_SPLIT_THRESHOLD`, default `50000`
  optimized-IR instructions — a CPU-speed-invariant metric) so trivially small
  modules are never split.
- Concurrency is bounded by a **GNU make jobserver client**: qcc parses `MAKEFLAGS`
  and acquires tokens *before* splitting; if no spare tokens are available (a
  saturated `make -jN`), it falls back to single codegen so a parallel build is
  never oversubscribed. The client supports both the make 4.4+ named-pipe form
  (`--jobserver-auth=fifo:PATH`) and the older inherited-fd form
  (`--jobserver-auth=R,W` / `--jobserver-fds=R,W`); each qcc process owns one
  implicit job slot and uses extra codegen threads only for tokens it can acquire
  non-blockingly. This makes parallel codegen safe to leave default-on under
  `make -j`.
- `QORE_AOT_SIZE_DEBUG` traces the module instruction count and the split decision.

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

## IR Analysis And Optimization

Lowered functions carry transient facts for each SSA value: known type,
assigned state, NOTHING exclusion, and runtime representation. Assignment is a
separate fact from the declared type. In particular, a typed lvalue is not
assigned before its first write and becomes NOTHING again after remove/delete;
reference parameters retain reference assignment semantics while value reads
use the declared target type.

`QoreIRAnalysis` provides normalized SSA-operand and normal-successor visitors,
plus reusable reachability, predecessor/successor, dominator, and natural-loop
analysis. The IR interpreter uses the normalized operand visitor for ownership
use counts, preventing dedicated instruction fields from diverging from generic
operand handling.

The first generic optimization built on this layer is conservative scalar
loop-invariant code motion. It moves only native scalar constants, proven
assigned and non-NOTHING non-reference IR-only local loads, and non-throwing
native scalar operations. A local written in the loop is never hoisted. Loops
must have a unique unconditional preheader, and the preheader must precede the
loop in IR block-list order while the LLVM emitter still resolves ordinary SSA
operands in that order. The post-optimization verifier is mandatory for both
runtime and AOT source lowering. `QORE_DISABLE_IR_OPT=1` disables the pass for
same-binary diagnostics and performance comparisons; `QORE_IR_OPT_STATS=1`
reports analyzed loops and hoisted instructions.

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

## Exception Source Locations

AOT-compiled code reports real per-frame source file and line numbers in
exceptions, matching interpreted execution. The throwing location and full
backtrace are reconstructed lazily at throw from line-number tables embedded in
the artifact (emitted by default with debug info), so non-throwing code carries
no per-statement location-updater cost. `--strip-debug-info` emits no line
tables and retains the per-statement updater instead. Source text is not
required: stripped-source artifacts still report file and line from the line
tables.

For the innermost-frame detection mechanism that decides, at throw, whether the
innermost user frame is AOT (and therefore whether to resolve lazily), see
[`aot-lazy-loc-innermost-frame.md`](aot-lazy-loc-innermost-frame.md).

## Exception Cleanup Dominance

LLVM `invoke` results that own Qore references may be cleaned up directly from
SSA only when the defining block dominates every cleanup use. The general path
therefore keeps the safer cleanup-alloca representation unless dominance is
proven.

Do not relax this rule without a real dominator-tree or equivalent
scope/finalization design. See
[`aot-eh-cleanup-dominance.md`](aot-eh-cleanup-dominance.md).
