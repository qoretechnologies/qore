# Sandbox provenance

Copyright (C) 2026 Qore Technologies, s.r.o.

Objects and runtime call references retain the sandbox context in which they were created. Invoking their code
adds that context to the governing sandbox. A runtime call reference that sandboxed code gets from another Program
with `Program::getCallReference()` is created under the caller's sandbox: the switch to the target Program is
preceded by a `QoreCallerSandboxHelper`, which applies that sandbox as provenance (issue #6149). Filesystem and
network checks must satisfy both the existing nearest-caller policy and every active provenance policy. Entering
another Program, a policy barrier, or an asynchronous continuation cannot discard provenance. Contexts retain
managers, not Programs; modifying a retained manager takes effect on the next check. As with existing policy APIs,
configure filesystem/network rules and non-atomic limits while their users are quiescent; interrupt requests and
thread-limit changes are thread-safe. Replacing a Program's manager affects future captures; revoke old captures by
restricting or interrupting the old manager before replacing it.

## Program objects are capabilities

A `Program` object is a capability: code that holds one can use everything that the Program can do, through the
methods that run its code or return its callables (`callFunction()`, `callFunctionArgs()`, `callStaticMethod()`,
`callStaticMethodArgs()`, `run()`, `getCallReference()` and `getExpression()`). Parse options belong to each
Program: they restrict the code parsed in that Program and are not carried into another Program that the code
calls, whose code runs with that Program's own parse options. Only the runtime sandbox policy follows the caller:
the caller's `SandboxManager` governs the code that runs for it in the other Program, and what that code creates
for the caller keeps the caller's sandbox as provenance, as described above. A host must therefore not give tenant
or sandboxed code a Program object with more freedom than the tenant itself has. The user-facing description is
`sandbox_program_capability` in doxygen/lang/222_sandboxing.dox.tmpl and the `Program` class documentation in
lib/QC_Program.qpp.

## A Program keeps the sandbox of the code that created it

Object provenance and the caller walk govern the code of a Program only while it is reached through an object,
closure or caller that carries the sandbox. A Program that sandboxed code creates is reachable without them: its
code can import a function into a Program of the host, the host can copy its functions into an evaluation Program
(`ProgramControl::createEvalProgram()`), and a binary module's thread can enter it with
`QoreExternalProgramContextHelper`. The `sandboxed` flag alone (issue #5659) marks such code as untrusted for the
decisions made from it (what it creates, the paths reserved for trusted code), but applies no policy, interrupt or
limit, so the code called in those ways ran with the freedom of the trusted code that called it.

So the Program also keeps the sandbox itself: `qore_program_private::creator_sandbox_manager` and
`creator_policy_manager`, the managers that `qore_caller_sandbox_refs()` resolves for the creating code for all purposes
and for policy checks (none for policy under a policy barrier or a system-policy capability, so a helper Program that
trusted code creates there for a sandboxed caller keeps the caller's interrupts and limits but not its policy, as the
capability's own code does), are set in the constructor whenever the constructor marks the Program sandboxed, and when
code that sandboxed code parsed is committed (a later addition is combined with the one set, so it only adds
restrictions). A parse entry point resolves them before it switches to the Program (`getParseCreatorSandbox()`), records
them as the sandbox of the pending code once nothing can refuse the parse any more (`pendCreatorSandbox()`), and
`internParseCommit()` attaches them just before it merges the code, so the code is never committed without its sandbox
(an allocation failure refuses the commit); `internParseRollback()` drops them, so a refused or failed parse, or pending
code that the host rolls back, changes nothing. An expression (`parseExpression()`) attaches nothing: it is governed
through the provenance of its `Expression` object. The attached sandbox governs all of the Program's code, its earlier
code included. It is separate from the Program's own manager: `QoreProgram::setSandboxManager()` (also with `nullptr`)
and `Program::getSandboxManager()` act on the own manager only, and the two govern the Program's code together. Every
lookup of a Program's sandbox resolves both: `getSandboxManagerRefs()` returns references to the own manager and the
creator's manager (its policy manager for a policy check) without allocating, which the walk of the thread's sandbox
stack visits (`visit_thread_sandbox_stack()`), so the uncontrolled-API check still allocates nothing;
`getSandboxManagerRef()` combines them (a composition is allocated only when both are set and differ); and entering the
Program starts the call budgets of both (`ProgramThreadCountContextHelper`). Adding it increments
`qore_sandbox_manager_generation`, as setting a manager does, so that no thread's cached "no manager on the chain"
result hides it. In the walk, only a Program's own manager ends the lookup (the nearest Program with a manager of its
own governs alone, as before); the kept sandbox only adds to the sandbox of the callers, and the walk goes on, so a
stricter caller still governs the code that it calls in a Program that a laxer sandbox created. Reflection
(`QoreReflectionCaller::checkProgramAccess()`) compares the Program's own manager only: the kept sandbox gives the code
that created it no privileges in the Program. A manager that code a sandbox governs gives a Program
(`Program::setSandboxManager()`, also through trusted code that it calls) is additive
(`qore_program_private::own_manager_additive`): the walk visits it and goes on to the callers, as for the kept sandbox,
because a Program's own manager otherwise governs its code alone and a laxer manager would govern the calling code's own
calls into the Program; the host's own calls are governed by that manager alone, as before. A manager that trusted code
that no sandbox governs sets, and one set with the C++ API, governs alone. The kept managers of a Program are released
with its own manager when the Program is cleared, or when it is destroyed without being cleared. The Program constructor
throws `std::bad_alloc` if the creator's sandbox cannot be resolved (no Program is created); the parse entry points
raise an out-of-memory exception and do not parse.

An expression cannot define code: it is parsed as `return (<expression>);`, which code that closes the parenthesis could
extend with declarations left pending for the next commit, so a declaration in an expression (also a global or
`%try-module` variable) is a parse error (`check_expression_declaration()` in lib/parser.ypp, issue #6524) and the
declarations that the expression added are rolled back (qore_program_private::parseExpression()); an expression cannot
be created while the Program has code pending. Parse directives in an expression's code are refused. The walk's cache of
a context with no manager on its chain (`sandboxChainClear()`) is shared by policy and other lookups, so a Program with
a manager in any slot counts as having one: a Program created in a system-policy capability has a kept sandbox but no
kept policy manager, and a policy check must not cache "none" for the other lookups.

Trusted infrastructure that builds shared state lazily for whichever caller needs it first (a Program that every caller
then uses) would give it the first caller's sandbox; it builds it through a capability made during trusted startup with
`SandboxManager::createTrustedBuildCall()` (`qore_create_trusted_build_call()`), a `SandboxBoundCallReference` in build
mode. A call evaluates its arguments in the caller's context, sets a construction barrier (the module initialization
barrier with a trusted loader, `QoreModuleInitSandboxBarrierHelper(QoreTrustedBuildTag)`, `ModuleInitBarrier::build`),
then enters the Program that created it and runs the callback (no policy mask, unlike `createSystemPolicyCall()`: the
barrier hides the caller's policy already). Under the barrier nothing of the caller below it is seen:
`qore_caller_is_sandboxed()` is false for the caller, so what the callback creates or commits keeps no sandbox of the
caller, threads that it starts inherit none, and objects that it creates have none of the caller's provenance. The
current Program is never hidden by a build barrier: it is the Program that created the capability, also when its own
code called the capability, so that Program's own sandbox governs the build. The provenance of the caller's objects and
callbacks that the build calls applies above the barrier: a frame of the same context as one that the barrier hides is
applied again (`QoreSandboxProvenanceHelper::apply()`). The time and stack budgets of the caller's call (frames of the
thread, not found by the walk) still count the build; the caller's interrupt requests do not reach it.

The memory that the build's own code allocates is shared state, charged to no caller (`QoreTrustedBuildChargeHelper`,
`QoreSandboxBudgetStack::suspendCharge()`): storage keeps the charge it was created with, so a charged build would bill
the first caller for as long as the state lives, and for its later growth by any caller. It is charged to the sandbox of
the Program that created the capability, if that Program has a sandbox with a memory limit: the helper enters that
Program's sandboxes in the build, as entering the Program does, so that its budgets apply also when its own code called
the capability (the Program is current then, and entering it again enters nothing). Before the barrier is set, the build
records the sandboxes that govern the caller (`qore_find_thread_sandbox_manager_ref()`) and the charge of the caller's
call. In the build, each entry into a sandbox (`QoreSandboxBudgetStack::enterInBuild()`) starts the budgets that its
sandboxes do not have yet, pushes again the budgets of its sandboxes that are below the build (the caller's calls in
progress: a continuation captures them, with their time and stack limits), and makes current a charge of the union of
the accounts of all of its budgets, of the charge that the same entry made current already (a Program is entered with
its own sandbox and the one that it keeps, in two calls), and, if a sandbox of the caller governs the code (the caller's
code), of the caller's call; that charge is carried by a budget of its own that is pushed last, so a thread that the
code starts makes it current too. The build's own entries of the creator's sandboxes (`enterBuildOwn()`) charge the
creator's own accounts only, never the caller's, also when the caller is the creator's code. Each entry charges the own
accounts of its sandboxes, not the chained charges of their budgets, whose accounts include those of the code that
called them when they started. The build's charge (the creator's) is never added: the caller's code is charged as when
the caller calls it directly, also in nested builds, whatever the order of the members of a composite sandbox, and the
code of other sandboxes that the build runs is charged to those sandboxes only. A capability's callback must therefore
not run code of another sandbox that the caller supplies: the caller would keep memory beyond its own limit through it
(a follow-up would give code passed in the arguments the caller's provenance). Code that the caller's code runs is a
part of the caller's call, as a nested call is: an entry under a carrier of the caller's code
(`QoreSandboxCallBudget::caller_carrier`) chains that carrier's charge and carries the caller's code itself, whatever
its sandboxes; under a carrier of the build's own code (`build_own_carrier`) or of other code that the build runs by
itself, nothing is chained. The sandboxes of the creator govern the code that the build's code calls (a stricter caller
governs the code that it calls), so they are members of the composite sandbox of that code; their accounts are charged
by the build's own entries only (`isBuildOwnSandbox()`), and an entry of build-own sandboxes only is the build's own
code when it is not under a carrier of the caller's code (the creator's code that the caller's code calls is a part of
the caller's call). A continuation of code in a build (a thread, a pool task, async I/O) attaches a floor with no caller
(`attach()`, `QoreSandboxContext::in_build`) and the carriers that it captured, so a thread that the caller's code
starts keeps the caller's charge, and one that the build's own code starts charges the code that it runs to that code's
own sandboxes only. Work that the build hands to another thread captures only the budgets entered in the build
(`capture()`), so it is not a part of the caller's call either.

Creating the capability is refused to code that a sandbox governs or that is sandboxed, and for a callback that has
provenance, belongs to a sandboxed or deleted Program, or is a bound callback or a capability; a trusted build
capability cannot be registered with `createSystemPolicyCall()` either.

The existing nearest-caller rule is otherwise unchanged. Interrupt checks include every contributing manager;
threads reserve a slot in each distinct contributing manager and roll back all reservations on failure. The
context's Program IDs preserve cancellation scope. Contexts are immutable after construction and restore the
previous context on all exits. Copies preserve provenance. Serialization does not grant authority: receiving code
must bind the callback under the receiving principal, using the normal registration boundary.

`SandboxManager::callWithSystemPolicy()` continues to mask inherited caller policy, but cannot remove provenance.
Trusted infrastructure that needs privileged resource access registers a fixed callback with
`SandboxManager::createSystemPolicyCall()` before entering any sandbox. Invoking that capability masks pre-existing
policy provenance for its fixed callback; callbacks supplied by a tenant acquire their own provenance again when
invoked. Interrupts and thread accounting remain active. Capability construction is rejected while any sandbox
governs execution. The registered callback must validate its arguments and must not expose a general arbitrary
path, host, or callback execution service. Giving a capability to a tenant is an explicit grant of its operation.

Creation alone cannot track a trusted-created object subsequently configured by a tenant. Registration code must
keep its interface-Program wrapper or bind the selected method/callback under that interface's context with
`SandboxManager::bindCall()`. Shared clients are not mutated or automatically retagged by setters. This is
execution provenance, not data taint tracking. Qorus registration wrappers remain necessary for these cases.

The native ABI is preserved: objects use private storage, runtime call references use a sparse registry, and
composite managers use the existing policy managers' private implementations. Native code invokes callbacks with
`ResolvedCallReferenceNode::invoke()` or applies `QoreSandboxProvenanceHelper` before a direct implementation call.
The public virtual `execValue()` slot is retained for existing subclasses. Native code remains trusted; I/O that a
foreign runtime performs without Qore sandbox checks is outside this mechanism.

Execution coverage includes interpreted and compiled methods, closures, implicit hooks, native callbacks, and
asynchronous captures. Optimized paths must either establish provenance or fall back to a dispatch path that does.
No language syntax changes are involved.

Shared modules and their constants must be initialized during trusted startup when they represent trusted
resources. Initialization first triggered by a tenant intentionally captures that tenant's context; module code
is not exempted. A capability enters its registration Program before establishing its policy boundary. The
registration Program is retained weakly, so a deleted Program cannot be resurrected by invoking its capability.
Bound callbacks expose their strong callback edge to the cycle collector, and repeated binding flattens the
wrapper while retaining all prior contexts.

Internal composite managers are enforcement views: filesystem/network checks, interruption and thread-slot
accounting fan out to the constituent managers. Configure, inspect and revoke the original managers, not these
temporary views. Cleanup retains provenance, including when releasing a bound closure runs destructors.

## Isolation domains (qore#6536)

A tenant is an isolation domain that trusted host code sets on a sandbox (`QoreSandboxManager::setIsolationDomain()`):
once, before the manager governs code (`setGoverns()`, under the same lock: a Program uses it, or a composite contains
it), so the checks read it without a lock. A manager that sandboxed code creates or copies inherits the domain of the
sandbox that governs the creating code (`inheritIsolationDomain()`). A composite has no domain of its own: its members
are checked. `qore_caller_isolation_domain()` (`SandboxManager::getCallerIsolationDomain()`) is the domain of the
calling code's sandbox, or, in a trusted build that a tenant entered (whose barrier hides the tenant), the tenant's.

Invariant: in a trusted build that sandboxed code entered, code of a sandbox of another domain than the caller's never
runs. The check is at the one point where a sandbox becomes current on a thread in a build, `QoreSandboxBudgetStack::
enter()`/`enterInBuild()`/`enterBuildOwn()`: the entry of a Program (`ProgramThreadCountContextHelper::
enterSandboxBudgets()`), the provenance of an object or callback (`QoreSandboxProvenanceHelper`), and continuations
(`attach()`); it is made also for a call whose limits cannot be enforced (`lost_owner`), whose later entries are not
recorded. Parsing or committing code in a Program switches into it without entering its sandboxes, so
`ProgramRuntimeParseContextHelper` and `ProgramRuntimeParseCommitContextHelper` check its sandboxes against the
isolation (`qore_sandbox_check_program_isolation()`).

Each build floor carries a `QoreSandboxIsolation`: the sandboxes of its caller (none for a build that the outer build's
own code entered), the sandboxes of the Program that created it, and the isolation of the outer build. An entry must
pass every level, so a nested build never widens what runs: a level allows a sandbox if it has no domain, is one of the
level's caller's sandboxes or of its domain, or is a sandbox of the level's creator; a level whose caller has no domain
allows everything. The build's own code is checked against the outer levels only. A continuation captures the
isolation of the innermost floor (`QoreSandboxContext::isolation`), also when no sandbox governs the capturing code (a
thread that a build starts: `QoreInheritedSandbox::capture()`; a pool task; an async I/O operation:
`capture_async_io_sandbox_context()`); a context that only keeps an isolation is applied as a floor, not as the thread's
sandbox context. Work that a thread runs for its submitter (a thread's start, a pool task, a step of an async
operation: `attach()` without `join`) is governed by the submitter's isolation only: its floor hides the isolation of a
build in which the thread itself started, so a shared worker that started in one tenant's build runs other work as
before. A continuation that runs inline in the current call (`QoreSandboxContextHelper` with `join`) keeps the
isolation of the build that it runs in as well (`QoreSandboxIsolation::join()`). If the floor cannot be recorded, the
continuation's stack is closed. Builds that trusted code entered have no isolation. A continuation's floor can have no
frames, so `QoreSandboxBudgetStack::empty()` counts the floors too.

A refused entry fails closed: it records nothing more until it is left (`refused_owner`), closes the stack (the stack
check at the entry of every function, method and closure, in each execution mode, raises `SANDBOX-ISOLATION-ERROR`),
sets the sandbox-limit cancellation bit (every cancellation point raises it), and the Program entry and the receiver's
provenance in `CodeEvaluationHelper` raise it at once. The fast paths that run IR without the stack check (the native
leaves, the inline IR closure and method calls) decline while the thread has a refused entry
(`qore_sandbox_isolation_refused()`, which reads a process-wide count first), so the regular call path raises it.

Not covered: builtin (trusted) code that runs on an object with another tenant's provenance, which is data that the
caller should not hold (see the tenant isolation issues, qore#6751); code that a build registers to run after it
returns (a signal handler, an at-exit callback), which runs without the build's context.
