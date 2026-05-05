# Qore JIT and AOT Current Design

## Status

Qore has one LLVM-backed compilation pipeline shared by JIT and AOT:

```text
Qore AST -> Qore IR -> LLVM IR -> native code
```

Runtime modes:

- `--exec-mode=ast`: AST interpreter.
- `--exec-mode=ir`: eager IR lowering and IR interpreter execution.
- `--exec-mode=jit`: eager IR lowering and LLVM JIT compilation.
- `--exec-mode=tiered`: default adaptive execution, promoting hot functions
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

Failed lowering or compilation disables promotion for that variant rather than
retrying on every call. Debug attach can force dispatch back to AST so debugger
semantics remain predictable.

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

## Exception Cleanup Dominance

LLVM `invoke` results that own Qore references may be cleaned up directly from
SSA only when the defining block dominates every cleanup use. The general path
therefore keeps the safer cleanup-alloca representation unless dominance is
proven.

Do not relax this rule without a real dominator-tree or equivalent
scope/finalization design. See
[`aot-eh-cleanup-dominance.md`](aot-eh-cleanup-dominance.md).
