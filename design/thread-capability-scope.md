# Thread Capability Scope

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented (qorus#867, the Qore part; follows qorus#766 and qore#5943); see Decisions at the end.

- the scope: `ThreadCapabilityScope` (`lib/QC_ThreadCapabilityScope.qpp`), the thread's stack of sets in
  `ThreadData::capability_scopes` (`lib/thread.cpp`: `thread_push_capability_scope()`,
  `thread_pop_capability_scope()`, released at the thread's end before its Program data);
- the lookup: `get_thread_capability()` and `get_thread_capabilities()` (`lib/ql_thread.qpp`),
  `thread_capabilities_visible()`;
- the tests: `examples/test/qore/threads/thread-capability-scope.qtest` (with the module fixture
  `ThreadCapabilityFixture.qm`), and a capability case in each module's test.

## Problem

Library modules that start threads or wrap callbacks for their callers take that code as an option: `start_thread`
(`code<int(code<nothing()>)>`) and `call_wrapper` (`code<auto(code<auto()>)>`) of `WebSocketClient`,
`DiscordWebSocketClient`, `FilePoller`, `FtpPoller`, `ServerSentEventClient`, `ParallelToolDispatcher` and
`ExtractionPipeline` (QoreStructuredExtraction). A host
that runs code for an interface, such as Qorus for a service, wants the threads those modules start to be counted,
shown and waited for as the interface's threads, and to run in the interface's context. Today every caller has to
pass the host's implementations explicitly; a caller that does not gets untracked threads without context.

The host should be able to make its implementations available to the module code that runs for an interface,
without the interface's code passing them, and without letting the interface's code, which can be sandboxed,
replace or read them.

## Design

### The scope

A thread has a capability set: a map from a capability name to the code that implements it.  It is empty when a
thread starts.  A host sets it for the code it runs with a scope object:

```
ThreadCapabilityScope scope({
    "start_thread": \startServiceThread(),
    "call_wrapper": \callInServiceContext(),
});
svc.start();
# the scope is left when the object is destroyed: at the end of the block, or when an exception leaves it
```

- `ThreadCapabilityScope::constructor(hash<auto> capabilities, *bool replace)` (each value must be code) makes the thread's set the
  outer set with `capabilities` added or replaced by name (with `replace`: `capabilities` only); destroying the object
  restores the outer set.  The class is in the `THREAD_CONTROL` domain: code whose Program, or whose caller's
  restrictions (module code running for a restricted caller), forbid thread control cannot set a scope.  The caller's
  restrictions reach only a module that the restricted Program loaded first; the code of a module that the host loaded
  runs with its own options and can set a scope for a restricted caller that no sandbox governs, as it can start
  threads for it (`PO_NO_THREAD_CONTROL` without a sandbox is no thread boundary for library modules; see below).
- A scope cannot be set by code that a sandbox governs, and each capability's code must pass the guard's check
  (below): a host cannot pass on, by mistake, a closure of the restricted code it runs.  Its signature is not checked
  by the scope; the module that looks it up checks it as it checks an option.
- Scopes nest.  A scope belongs to the thread that created it; it is left by its destruction in that thread, which
  also leaves any scope created inside it that is still open (an object kept beyond its block).  Destroying it in
  another thread, or after an outer scope has already left it, does not change any thread's set.
- A thread's open scopes are released when the thread ends.

### The call-bound scope (qore#6737)

`ThreadCapabilityScope::call(hash<auto> capabilities, code call, ...)` runs `call` with `capabilities` added to the
thread's set and restores the outer set when the call returns or throws (a guard object pops the scope on every path).
A host whose interfaces all have a `SandboxManager` (Qorus: also for unrestricted code, for interrupts and limits)
cannot create a scope object for the interface that it calls for another one, as the caller's Program is on the stack;
the call-bound scope is allowed there, because its set cannot outlive the host's frame: it is not in effect when control
returns to the sandboxed caller, which can therefore neither reach nor keep nor change it.

Only the code that makes the call is judged (`thread_capability_call_refusal()` in `lib/thread.cpp`):

- the innermost stack frame must be the method's own builtin frame (fail closed otherwise), and the frame below it Qore
  code (`CT_USER`): a call through another builtin (`call_static_method()`, `call_function()`) is refused;
- the Program of the method's own frame is the Program of the code that called it, also when it was called through a
  call reference that host code created (the builtin then runs in the host's Program, but its frame records the
  caller's): it must not be sandboxed (by a manager or by its flag); nor may the Program running on the thread;
- the calling code must be allowed to control threads: the effective options of the Program of the method's own frame
  (the caller's, also through a host's call reference, whose builtin runs with the reference's Program's options),
  and `PO_NO_THREAD_CONTROL` at run time; the class's `THREAD_CONTROL` domain refuses a direct reference at parse
  time;
- sandboxed callers further up the stack, the call-program context, provenance and continued operations are not
  considered: they are what a scope object is refused for, as its set would stay in effect for them.

What the call bounds is the set, not what code in the call does with it: module code in the call can take a guard (or
an object that keeps one) that stays usable after the call, as with a scope object, and a host function that sandboxed
code can call and that uses `call()` runs that code with the capabilities; a guard still runs only code that no sandbox
governs.  Trusted code is refused too where its call is made by a builtin (`background ThreadCapabilityScope::call(...)`,
`Expression::eval()`).

The capabilities' code passes `thread_capability_check_code()` as for the constructor.  The code that `call` runs keeps
its own restrictions: sandboxed code in it cannot see the set or set a scope; module code in it sees the set; a thread
started by a capability in it is the capability's, and an object that takes a capability in it keeps it after the call.
Tests: `examples/test/qore/threads/thread-capability-scope-call.qtest` (every execution mode, sync JIT, qcc).

### The lookup and the guard

`*code get_thread_capability(string name)` returns the **guard** of a capability of the current thread's set, or
`NOTHING` if the set has none of that name or the caller may not see it.  The capability's code itself is never
returned: no Qore code can reach it once the scope has taken it.

The guard is a call reference (`ThreadCapabilityGuardReferenceNode`).  A call of it passes its arguments to the
capability's code (with `invoke()`) once they have been checked:

- the arguments are evaluated by the guard when the call passes them unevaluated (a direct call `guard(...)` in the
  AST interpreter), so that the values are checked, not the expressions that produce them;
- an argument that is code (`thread_capability_check_code()`) must be of a Program that may control threads and that
  no sandbox governs, and not a call reference without a Program of its own (builtin code, or another guard), which
  would run with no restrictions at all; this is the module-owned code rule of `qore_check_module_owned_code()`
  (c320f09f0b) for timers and tasks.  A call reference to a builtin function that code creates at run time carries the
  Program that created it and is judged by that Program's options, as in `qore_check_module_owned_code()`;
- every other argument must be a plain value (`NOTHING`, `NULL`, a number, a boolean, a string, a date or a binary):
  a reference, a container or an object could carry code that the capability would run unchecked, so they are
  refused.

The lookup is visible to:

- **user module code** (a Program with `PO_IN_MODULE`) whose Program is not sandboxed: a Program that module code
  creates inherits `PO_IN_MODULE`, also in a module that sandboxed code loaded, so the option alone does not tell that
  the code is trusted;
- **other code** that may control threads itself and that no sandbox governs (the host, unsandboxed applications);

and returns `NOTHING` to code a sandbox governs and to code whose restrictions forbid thread control.  This is defence
in depth only, not the boundary.

`*hash<string, code> get_thread_capabilities()` returns the guards of the whole set by the same rule.  Neither function
changes anything, so they are `RET_VALUE_ONLY`.

### The boundary

**A capability runs only code that could control threads itself, whoever holds its guard.**  Code that obtains the
guard, by whatever path, cannot use the host's implementation to run its own code:

- a restricted or sandboxed Program that loads a user module of its own (S1 of the review), which returns or passes
  on what it looks up: the module's Program inherits the restrictions of the Program that loads it, so its code is
  refused too, and the restricted Program's own code is refused anyway;
- a restricted subclass of a module class that reads the member where the module keeps the capability (S2), and calls
  it with its own code: refused;
- the restricted code passed by reference, in a container, or in an object: refused, as is a direct call of the guard
  in the AST interpreter (whose arguments the guard evaluates before it checks them).

Module code keeps passing its own closures (`\run()`, a closure that calls the user's callback inside), which are of
the module's Program and so may control threads.  A consequence of the rule: a module that a restricted Program loads
first is parsed with that Program's restrictions, so its code may not control threads, and the capability does not run
it either; such a module gets `THREAD-CAPABILITY-ERROR` when it starts its thread.  A host that wants its capabilities
used by a module loads the module itself before the restricted code does.

A scope cannot be set by code that a sandbox governs (S3): its set stays in effect for the code that runs on the thread
after the call that set it, the host's code included.  The capability code given to a scope must pass the same check.

### Who is governed: the creator, not the thread's inheritance

**Whether a sandbox governs the code that creates a scope is decided by that code, never by the sandbox that the thread
inherited from the code that started it** (`thread_capability_check_creator()`,
`qore_scope_creator_is_sandboxed()`).  The creating code is governed, and refused, if a sandbox governs:

- its Program, or a Program of the code that called it on the thread (sandboxed code that calls host code, by a call
  reference or a closure, or through module code that calls the host code back), also a Program that is sandboxed by
  its flag only: one that sandboxed code created, which has no sandbox manager of its own (qore#5659), counts wherever
  it is on the thread's program-context stack;
- an operation that the thread continues for another thread (`QoreSandboxContextHelper`), or a task that a native
  bridge runs for a sandboxed submitter (`QoreSandboxCaptureHelper`: the capture takes the place of the thread's
  inherited sandbox, but it is the submitter's operation and counts);
- the object whose method it is, or the callback it runs in (sandbox provenance), when sandboxed code governed their
  creation.

The sandbox that the thread inherited from the code that started it (design/thread-sandbox-inheritance.md;
`ThreadData::thread_start_sandbox_context`, applied by `QoreInheritedSandboxHelper`) does not count by itself when the
starting code's current Program was not sandboxed (`QoreSandboxContext::capturer_sandboxed`), nor
does a continued operation or a provenance that only that inherited sandbox governed when it was captured
(`QoreSandboxContext::inherited_only`, never set for a capture made in a Program that is sandboxed, by a manager or its
flag; it is computed only when such a sandbox or context is present, so the common capture of sandboxed code does no
extra lookup): host code that runs in a thread started for sandboxed code, such as the thread
that a host's API (an application server's `startThread()`) starts for a sandboxed caller, sets the capabilities of
the code it runs in that thread, as it does in any other.  Everything else is unchanged:

- the sandboxed code that runs in that thread is governed by its own Program and cannot set a scope;
- the capabilities' code must still be code that no sandbox governs (`thread_capability_check_code()`), so a scope
  never runs sandboxed code as a capability;
- the lookup (`get_thread_capability()`) follows the same rule for code that is not module code
  (`thread_capabilities_visible()` asks `qore_scope_creator_is_sandboxed()`, so a Program that sandboxed code created,
  whose code is governed by the sandbox that it keeps of that code, sees none either; see "A Program keeps the sandbox
  of the code that created it" in design/sandbox-provenance.md): the host's code in that thread sees its
  capabilities, sandboxed code sees none; the code of a user module (`PO_IN_MODULE`) sees them only if the module's
  Program is not sandboxed, as a Program that module code creates inherits `PO_IN_MODULE`, also in a module that
  sandboxed code loaded;
- the threads that a capability starts there inherit the sandbox as before: the sandbox still governs everything that
  the module code does for the sandboxed caller.

A thread that sandboxed code starts itself (its own `background`) begins in the sandboxed Program: host code that it
calls is called by sandboxed code and is refused.  A thread that sandboxed code starts with `call_async()`,
`parallel_map()` or `parallel_foreach()` enters only the Program of the callable, which can be host code that the host
gave it (a call reference or a closure without provenance); the thread's start context was captured in the sandboxed
Program (`capturer_sandboxed`), so it counts, and that host code is refused as well.  This holds for a Program that
sandboxed code created, whose code is governed by the sandbox that it keeps of its creator wherever it runs (also
when trusted code calls it without the Program object: an imported function, a function copied into an evaluation
Program), in a thread, a `parallel_map()` worker or a `ThreadPool` task that it starts.

A `Program` object is a capability (see the Program class documentation): code that holds an unsandboxed Program, or a
call reference that host code created to `call_async()` (`\call_async()`), and calls it, runs that code as host code,
and the thread it starts is a host's thread.  This is `call_async()` only: a host's `\parallel_map()` or
`\parallel_foreach()` reference that sandboxed code calls runs its workers for the sandboxed caller's code, whose
thread start context counts, so the host code in them is refused.  A host must not give sandboxed code such an object or
reference for code that it would not run for that code itself.

### Independent objects

A module that creates a shared or independent object (a module-wide cache, poller or connection) from inside the code
that a host runs for one of its interfaces creates it in an independent scope, so that the object takes none of the
capabilities of that code and its threads belong to no interface:

```
{
    ThreadCapabilityScope independent = ThreadCapabilityScope::independent();
    shared_poller = new SharedPoller();
}
```

`ThreadCapabilityScope::independent()` is the named form of an empty replacing scope (`ThreadCapabilityScope({},
True)`); a thread with an empty set has no capabilities (`get_thread_capability()` returns `NOTHING`).  Both need
thread control and cannot be used by code that a sandbox governs, like any scope, so sandboxed code cannot hide the
threads of its objects from the host this way.

### Attribution

A host attributes the threads that a capability starts by the guard: its provider is code that the host creates for
one interface, so a guard always starts threads for the interface whose scope the module took it from.  Modules take
the guard when their object is created inside the interface's code, so the threads that the object starts later,
outside any scope (reconnects, timers, other threads), are attributed to that interface too.  Threads that modules
start without a capability (at module initialization, for objects created outside any scope or in an independent
scope) belong to no interface.

### Domain declarations

Thread-starting library modules declare the `THREAD_CONTROL` domain in their module header (`domains = ...`: the
WebSocketClient, DiscordWebSocketClient, FilePoller, FtpPoller, ServerSentEventClient, QoreAgentStrategies and
QoreStructuredExtraction modules).  A module of the process-wide module path that declares a domain keeps it however it
is loaded (see the `domains` key of the module declaration), so its code may start threads for a caller restricted with
`PO_NO_THREAD_CONTROL`, and the guard runs its code.  The module is responsible for the safety of what its threads do
for such callers.

The effective restrictions of a module's thread that runs for a caller:

- **parse-option domains**: the caller's restrictions minus exactly the domains that the module declares, for the
  module's own code; a module without a declaration cannot widen anything (a caller that may not control threads
  cannot even load a module that uses `background`);
- **sandbox policy** (the caller's `SandboxManager`: filesystem and network allow-lists, private ranges, thread
  limits, interrupts): always the caller's, inherited by the thread, and **never widened** by a declaration, so a
  tenant never reaches what its sandbox denies (private addresses, other tenants' resources);
- the caller's own code that the module calls back keeps the restrictions of its own Program.

`PO_NO_THREAD_CONTROL` without a sandbox is therefore no thread boundary for these modules, as for any module that
starts threads for its callers (see the Qore sandboxing documentation): such a caller can cause module threads that
run its callbacks, and only a sandbox's thread limit bounds how many run at once.

`examples/test/qore/threads/restricted-first-modules.qtest` checks each module loaded first by a restricted,
sandboxed Program (its threads start, and the sandbox's thread limit governs them), and the domain rule (a declared
`NETWORK` domain is granted to the module's thread, an undeclared `FILESYSTEM` domain is not, a module without the
declaration cannot be loaded, and a sandbox that denies the network denies it in the module's thread).

### Modules

When an option is not given (or is `NOTHING`), a module looks the capability up when it is constructed; an option given
always wins:

```
*code<int(code<nothing()>)> start_thread = opts.hasKey("start_thread")
    ? opts.start_thread
    : get_thread_capability("start_thread");
```

The modules: `WebSocketClient`, `DiscordWebSocketClient`, `FilePoller`, `FtpPoller`, `ServerSentEventClient`,
`ParallelToolDispatcher`, `ExtractionPipeline`.  The capability is taken at construction, so an object created in the
scope keeps it after
the scope is left, as an option given to the constructor is kept.

### Names

`start_thread` and `call_wrapper`, with the signatures of the module options above.  The names are documented
with the lookup function; a host may set other names, which no module looks up.

### ABI

A new class and two builtin functions, and a member of the thread's internal data: no module-API or public-ABI
change.

## Tests

- set, nested, replace, leave on exception, out-of-order destruction, destruction in another thread, thread end with
  open scopes; a new thread starts with an empty set;
- the lookup from module code, host code, sandboxed code and code without thread control; a scope set by code
  without thread control, and with a capability of code without thread control, is refused;
- sandboxed code cannot set a scope directly, through a call reference or a closure of the host, from a callback that
  module code makes, through a host object created for it, nor in a thread it starts itself; nor can the code of a
  Program it creates (sandboxed by its flag only), in a thread of its own or in a closure or call reference that the
  host calls with its capabilities set; host code run for a sandboxed capture (`sb_capture_applied_call()`,
  `module-cpp-api/sandbox-capture`) cannot either; host code sets one in a
  thread started for sandboxed code by host code, where sandboxed code still cannot set one or see the capabilities and
  sandboxed capability code is refused (`thread-capability-scope.qtest`);
- each module takes the capability when no option is given, keeps an explicit option, and keeps the capability after
  the scope;
- every execution mode; valgrind.

## Decisions

- The boundary is the guard (option C, David 2026-11-14): visibility of the lookup is defence in depth only.
- A scope object, as qorus#766 asks; a new thread starts with an empty set.
- Attribution by the guard, and the independent form (`ThreadCapabilityScope::independent()` and the empty replacing
  scope) for objects that belong to no interface (qorus#867, decision 1).
- Domain declarations widen only the parse-option domains of the module's own code, never the caller's sandbox
  (decision 4); the thread-starting library modules declare `THREAD_CONTROL`.
- The creator of a scope decides whether a sandbox governs it, never the thread's inherited sandbox; the capability
  code must still be unsandboxed, and sandboxed code can never create a scope or reach a provider (prof for David,
  2026-11-15, with an independent security review of the change).
