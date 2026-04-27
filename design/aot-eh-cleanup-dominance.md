# AOT EH Cleanup Dominance

## Status

SSA-direct cleanup for LLVM EH values is currently disabled in the general path.
The implementation keeps the safer cleanup-alloca path when dominance cannot be
proven during lowering.

Relevant code:

- `include/qore/intern/QoreIRToLLVM.h`
- `lib/QoreIRToLLVM.cpp`
- `QoreIRToLLVM::canUseSsaCleanup()`
- `QoreIRToLLVM::emitCondBrWithSsaPreamble()`

## Problem

LLVM `invoke` results that own Qore node references must be released on:

- Normal fallthrough.
- Explicit return.
- Exception landing pads.
- Later branch edges that leave the scope where the value is live.

The fastest representation is an SSA value plus per-edge `qore_rt_decref()`.
That is only legal when the value's defining block dominates every cleanup use.

## Current Conservative Rule

If cleanup dominance is not trivially safe, the value is promoted to an entry
cleanup alloca initialized to `NOTHING`. Cleanup code then decrefs the alloca
contents through existing shared paths.

This costs extra allocas, but it is correct and avoids LLVM verifier failures
such as "Instruction does not dominate all uses" in functions with `try` /
`catch`, merged branches, and later exception checks.

## Why the Previous SSA-Direct Attempt Failed

A value defined in a catch or branch arm can remain in the pending SSA cleanup
list after control merges. A later cleanup preamble outside that arm can then
reference the value from a block that the defining arm does not dominate.

The old single-predecessor immediate-dominator approximation could not model
this correctly, so it either missed legal cleanup opportunities or allowed
illegal SSA uses.

## Correct Future Direction

Re-enabling SSA-direct cleanup needs one of these designs:

- Build a real LLVM `DominatorTree` after the relevant CFG is complete and
  emit per-edge cleanup only where dominance is proven.
- Track lexical/control scopes and promote pending SSA cleanup when leaving the
  scope that defines the value.
- Defer cleanup placement until function finalization, then use a complete CFG
  to decide which values can stay SSA-direct and which must be promoted.

Do not relax `canUseSsaCleanup()` without one of the above. Correctness and
verifier cleanliness are more important than reducing cleanup allocas.

## Review Checklist

Any future change in this area must prove:

- Every SSA cleanup use is dominated by the value definition.
- Exception landing pads and normal-return cleanup both release owned values.
- `on_block_exit`, iterator cleanup, local uninstantiation, and closure
  re-instantiation still run in the correct order.
- `HttpServer.qm`, `DataProvider.qtest`, and handler-heavy tests compile and
  run with LLVM verification enabled.
