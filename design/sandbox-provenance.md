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

## Objects that the host lends are grants (qore#7052)

An object of a class of a library module (or a class with a module base class, `qore_class_private::spansPrograms()`)
that code of a non-module Program creates, while no restriction other than `PO_NO_UNCONTROLLED_APIS` applies to that
code, has `qore_object_private::capability_origin` set; so does an object that a method of such an object creates
(nested lending). The flag is computed with the creation origin in `qore_capture_sandbox_provenance()`;
`qore_creation_by_lending_code()` requires a non-module Program on the stack, or a capability frame that is newer
(by grant `seq`) than the innermost module initialization barrier, so the state that a library creates in its
initialization, also in a first load inside a lent method, or for nobody is not lent.

The grant is not set per call: `ProgramThreadCountContextHelper::set()` with the receiver (the method-call entry points:
`CodeEvaluationHelper`, the JIT fast method paths and the IR interpreter's inline method calls) pushes a
`ThreadData::CapabilityFrame` only when the call switches to the Program of the method's code, the object has the flag,
and the Program that calls it is not a module's and can be restricted (`qore_lent_method_call_needs_frame()`): only the
code that the object was handed to uses it as a grant, while library code acting on an object that it holds or was given
lends nothing. `qore_program_restrictions_seen` (no Program ever restricted a domain) and the monotonic per-Program
kinds of `SyncParseOptions::optionsSeen()` (a domain restriction, a module's Program) make the decision without reading
the 128-bit options. Such a call sets the call's runtime parse options override (the restrictions of a sandboxed caller)
before the frame (`CodeEvaluationHelper`), so the frame's snapshot holds it in every mode: the JIT and IR fast method
paths, which would set it after the Program was entered, are not taken by calls that enter another Program, and would
only fail closed. Outside a lent method, an object created while a policy barrier is active (also that of a
system-policy capability, whose code runs for the host that created it) is not lent, so a grant does not outlive itself.
Calls within a Program never read the flag, and the object's provenance pointer (which the JIT/AOT direct-call guards
test) is unchanged, so calls of lent and other objects cost what they did; `restoreContext()`, which runs only for a
switched context, pops the frame. No frame is pushed when the innermost provenance frame (apart from the object's own)
is a call reference that restricted code created (`QoreSandboxContext::reference`), so such a reference cannot launder
its restrictions through a lent object; `std::bad_alloc` leaves the frame out (fail closed). No frame is pushed for a
call made while the object's destructor runs (the object's status is no longer `OS_OK` once its deletion or the release
of its last reference started): the code that deletes the object or releases its last reference does not call it as a
capability. The frame records the Program of the lent method's code (`CapabilityFrame::code_pgm`, the Program of the
class that declares the method, so for an inherited method the base class's module): in a lent method, only an object
of a class of that module Program is lent, not one of another module's class or of a class that the caller supplies,
so a factory that creates an object of a class that its caller names cannot mint a capability of another module's
class; every class of its own module stays reachable through it (and through a deserializer, with caller-chosen member
state). A call from module code pushes no frame, so a base-class method that calls an override in the subclass's
module keeps the base module's frame, and the objects the override creates are not lent (fails closed). A thread that
a lent method starts keeps the grant for its life; the objects it creates are not lent (no frame on that thread).

The innermost capability frame is a grant like a policy barrier, ordered with them by `GrantSnapshot::seq`
(`qore_get_innermost_capability_frame()`): the walk stops at the context entered for the method, so the caller's
Program is not counted, the provenance frames from the one innermost at entry down are not counted, and only
`QORE_GRANT_LENT_DOMAINS` (file system, network, external information) are lent; the other domains and the loading
of native code are decided as without it. The user-facing text is `sandbox_lent_objects` in
doxygen/lang/222_sandboxing.dox.tmpl. Tests: examples/test/qore/security/caller-restriction-capability.qtest.
