# Sandbox provenance

Copyright (C) 2026 Qore Technologies, s.r.o.

Objects and runtime call references retain the sandbox context in which they were created. Invoking their code
adds that context to the governing sandbox. Filesystem and network checks must satisfy both the existing
nearest-caller policy and every active provenance policy. Entering another Program, a policy barrier, or an
asynchronous continuation cannot discard provenance. Contexts retain managers, not Programs; modifying a retained
manager takes effect on the next check. As with existing policy APIs, configure filesystem/network rules and
non-atomic limits while their users are quiescent; interrupt requests and thread-limit changes are thread-safe. Replacing a Program's manager affects future captures; revoke old captures by
restricting or interrupting the old manager before replacing it.

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
`SandboxManager::bindCall()`. Shared clients are not mutated or automatically retagged by setters. This is execution
provenance, not data taint tracking. Qorus registration wrappers remain necessary for these cases.

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
