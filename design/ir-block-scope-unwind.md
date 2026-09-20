# IR Block-Scope Unwind Cleanup

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  A lexical block carrying `on_exit` / `on_error` / `on_success`
handlers establishes an exception target, so that the inner lexical scopes an
exception unwinds out of uninstantiate their locals *before* that block's
handlers run.

Relevant code:

- `lib/QoreIRLowering.cpp` — `lowerStatementBlock()` creates
  `block.lvars.exception.cleanup` (and `.cont` when the block has handlers),
  pushes it on `exception_stack`, and emits its body at the end of the function.
- `lib/QoreIRInterpreter.cpp` — `ScopeExit`, `CheckException`, and the
  function-exit `ScopeExitGuard` / `fireScopeExits()` that handle whatever is
  still pending when the exception leaves the frame.
- `lib/StatementBlock.cpp` — `StatementBlock::execImpl()`, the AST behaviour
  every compiled tier has to reproduce.

## The contract

`design/qore-ir-spec.md` requires the compiled tiers to preserve "same
`on_block_exit`, `try` / `catch`, `context`, `foreach`, lvalue, and destructor
ordering", and that "every owned value path needs a normal-exit and
exception-exit cleanup".  This document is about the exception-exit half of that
for block-scoped locals.

## Why AST gets the ordering for free

`StatementBlock::execImpl()` holds an `LVListInstantiator` for the block's own
locals:

    int StatementBlock::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
        LVListInstantiator lvi(xsink, lvars, pwo.parse_options);
        return execIntern(rc, return_value, xsink);
    }

Every nested block is its own C++ frame.  When an exception propagates, the
inner `execImpl()` returns, its `lvi` destructs, the block's objects die — and
only then does the enclosing block's `execIntern()` reach its `on_block_exit`
section.  The ordering is a consequence of C++ scoping.

The IR tiers flatten every local in the function into one frame-wide slot array
that is released once, at frame exit.  They therefore have to reconstruct the
per-scope ordering explicitly.

## The synthetic cleanup block

`lowerStatementBlock()` builds an unwind path for the block:

    block.lvars.exception.cleanup:
        ScopeExit(scope_id, is_error = true)     # only when the block has handlers
        br block.lvars.exception.cleanup.cont
    block.lvars.exception.cleanup.cont:
        UninstantiateLocal(...)                  # this block's own locals, reverse order
        <terminator>

The order inside it matters and is deliberate: a block's **own** handlers fire
while its **own** locals are still alive.  That is AST behaviour — an `on_error`
in the same lexical scope as an `AutoLock` legitimately sees the lock still
held — and the fix must not "correct" it.

The terminator continues the chain outward:

- with an enclosing exception target, `CheckException` + a branch to it;
- with none, a synthetic `Rethrow` (`synthetic = true`, null target).  That is a
  real block terminator whose null-target path runs
  `returnAfterUnhandledException()`, which fires any scope handlers still pending
  further out and leaves the frame.  The `RefForeach` and `Context` cleanup
  blocks in the same file terminate the same way.

## The anchor

The cleanup path is emitted when

    has_on_block_exit || (lvars && getCurrentExceptionTarget())

The second term alone is not enough.  `exception_stack` is pushed at only four
places in `lowerStatementBlock()`'s file: three in `try` / `catch` lowering, and
the cleanup block itself — which is what lets the chain nest once one level
exists.  A block that merely carries handlers allocates a `scope_id` and emits
`ScopeEnter`, but `ScopeEnter` only records a watermark into the *handler* list;
it is not an exception target.

So with no `try` / `catch` anywhere in the function, `getCurrentExceptionTarget()
` stayed null at every level and **no** inner block emitted unwind-path cleanup
at all.  The throw left the execute loop directly, and the function-exit
`ScopeExitGuard` called `fireScopeExits()`, which fires handlers but cannot
uninstantiate anything: `scope_stack` holds only handler-list indices, and at
runtime there is no record of which locals belong to which lexical scope.  Every
pending handler therefore ran with *every* local in the frame still alive.

The first term makes a handler-bearing block anchor the chain, so inner blocks
satisfy the second term and clean up ahead of it.  A block with handlers but no
locals of its own still needs the anchor — it is the enclosing scope whose
handler would otherwise observe the interim state — which is why the emission
code tolerates a null `lvars`.

## What it looked like

    Mutex m();
    sub f() {
        on_error {
            AutoLock a(m);          # LOCK-ERROR: al still owns m
        }
        {
            AutoLock al(m);
            throw "BOOM";
        }
    }

AST released `al` before the handler ran; IR, JIT and AOT did not, and the
handler failed with `LOCK-ERROR` — which, being assimilated onto the sink, could
also displace the primary exception.  The three compiled tiers share this one
lowering, so they were wrong in the same way and are fixed by the same change.

The shape needs all three of: an exception, an inner lexical scope holding a
local with a side-effecting destructor, and an enclosing scope-exit handler that
observes the interim state.  Remove any one and it is invisible, which is why an
exception that left the whole function looked healthy — frame-exit cleanup
released everything before the caller resumed.

`on_exit` and `on_error` behave identically here; handler *ordering* (innermost
first) was always correct, and so was the `try` / `catch` landing-pad path.

## Cost

Measured by compiling `qlib` modules with the same debug build of `qcc`, with and
without the change: +2.25% total AOT code across 13 modules.  It is concentrated
in heavy `on_exit` users (`HttpServer` +4.3%, `WebSocketHandler` +3.9%,
`RestClient` +3.3%), exactly zero for modules that do not use the pattern
(`FsUtil`, `Qdx`, `MailMessage` are byte-identical), and slightly negative for
two.  The growth is linear in the number of handler-bearing blocks and the
scopes nested inside them.

## Coverage

`examples/test/qore/misc/block-scope-unwind/` compares the AST, IR, JIT, tiered
and AOT tiers over: nested, doubly and quadruply nested blocks; `foreach`,
`while` and `if` bodies; the top-level block; static-method, closure and
background-thread contexts; `on_success` and normal block exits; `try` / `catch`
in the same function; handler firing order and firing exactly once per scope
exit; `return` and `rethrow` from inside a handler body; a handler that raises on
both the normal and the unwind path; `return` / `break` / `continue` out of a
nested block; exception identity through a caught-and-rethrown error; and a
handler running under `defer_thread_cancel()` with a cancellation pending.

It also asserts the two cases that must *not* change: a handler in the same
lexical scope as the local still sees it alive, at one level and when nested.
