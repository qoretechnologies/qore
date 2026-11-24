/* -*- mode: c++; indent-tabs-mode: nil -*- */
/* Copyright (C) 2026 Qore Technologies, s.r.o.; see COPYING.MIT for license terms. */

#ifndef _QORE_SANDBOX_PROVENANCE_H
#define _QORE_SANDBOX_PROVENANCE_H

#include <qore/QoreSandboxManager.h>
#include <qore/QoreParseOptions.h>
#include <vector>

class AbstractQoreNode;
class ResolvedCallReferenceNode;
class QoreObject;
class QoreSandboxContext;

//! Immutable intersections; the constituent managers remain live, referenced policies.
class QoreSandboxComposition {
public:
    //! Returns a referenced intersection, flattening and deduplicating its borrowed arguments.
    DLLLOCAL static QoreSandboxManager* combine(const std::vector<QoreSandboxManager*>& managers);
    DLLLOCAL static bool same(const QoreSandboxManager* a, const QoreSandboxManager* b);
    DLLLOCAL static const std::vector<QoreSandboxManager*>* members(const QoreSandboxManager* manager);
};

class QoreProgram;

//! Captures provenance, returning nullptr when no sandbox governs execution and no origin is recorded.
/** @param code_pgm the Program whose code an object created now runs (its class's), or nullptr: if it is a library
    module's, the functional-domain restrictions of the creating code are recorded too (qore#6520)
*/
DLLLOCAL QoreSandboxContext* qore_capture_sandbox_provenance(const QoreProgram* code_pgm = nullptr);
//! Returns true if the sandboxes that govern the current code governed the creation of an object (qore#6842)
/** For sandboxed code: true if every sandbox manager that governs the current code (the members of a composition) is
    one of those that governed the creation of the object, as recorded in its provenance \a creation (the
    provenance that an object created now would record is compared, see qore_capture_sandbox_provenance()).  False if
    no manager governs the current code (code that is sandboxed only by its Program's flag), if \a creation is
    nullptr, or if \a owned is false (an object that no sandbox owns: one that trusted code created, also while it
    served sandboxed code), and if the sandboxes cannot be resolved: it fails closed.  The caller decides that the
    code is sandboxed.
*/
DLLLOCAL bool qore_sandbox_current_code_created(const QoreSandboxContext* creation, bool owned);
//! Returns true if the owner rules apply to the code that \a sm governs: its enforcement level is tenant isolation
/** The owner rules are the rules that keep the objects that a sandbox's code created apart from the code of other
    sandboxes: the sandboxed write and read rules of non-public members and the owner checks of
    SandboxManager::checkObjectOwnedByCaller().  qore_sandbox_current_code_created() decides with the governing managers
    that the rules apply to only, and code that no such manager governs owns every object.  The one place that decides
    which sandboxes the rules apply to (QoreSandboxManager::setEnforcementLevel())
*/
DLLLOCAL bool qore_sandbox_owner_rules_apply(const QoreSandboxManager& sm);
//! Returns true if the calling code may change an object as its owner: trusted code, or sandboxed code whose
//! sandbox created the object (qore#6842)
/** The caller is sandboxed as decided by qore_caller_is_sandboxed(); see qore_sandbox_current_code_created()
*/
DLLLOCAL bool qore_caller_created_object(const QoreObject& obj);
//! Returns the context of the current code that an object whose provenance is \a creation is transferred to, or
//! nullptr (qore#6842)
/** Only for code that a sandbox at the enforcement level of tenant isolation governs, every such sandbox of which
    \a creation names (the object was created while that code was served); the context is referenced for the caller
    and keeps the functional-domain restrictions of \a creation.  Fails closed (nullptr) if the sandboxes cannot be
    compared
*/
DLLLOCAL QoreSandboxContext* qore_sandbox_transfer_owner_context(const QoreSandboxContext* creation);
//! Returns the provenance of an object that trusted class code creates in a method of an object that a sandbox owns
//! (qore#6842)
/** The object is owned as the object whose method creates it, whose owner context is \a owner: the result names the
    sandboxes of \a owner and also those of \a creation, the provenance captured for the creating code, that \a owner
    does not name (such as a sandbox at the enforcement level of code safety), so that the restrictions of the creating
    code keep governing the object; the sandboxes at the enforcement level of tenant isolation of \a creation are
    those of \a owner already (the rule applies only then), so the owner of the object is the same.  Keeps the
    functional-domain restrictions of \a creation (qore#6520).  Returns \a owner referenced if it names every sandbox
    of \a creation and has the same restrictions, otherwise a new referenced context; nullptr if an allocation fails
*/
DLLLOCAL QoreSandboxContext* qore_sandbox_owned_creation_context(const QoreSandboxContext* owner,
    const QoreSandboxContext* creation);
//! Records the functional-domain restrictions of the code that creates a call reference to library module code
/** Called by the constructor of each run-time call reference class once the reference is complete (qore#6520); does
    nothing if \a code_pgm, the Program whose code the reference runs, is not a library module's (or is nullptr) or
    if the creating code has no restrictions
*/
DLLLOCAL void qore_capture_call_reference_origin(const ResolvedCallReferenceNode* ref, const QoreProgram* code_pgm);
//! Returns the Program whose code a closure runs: the Program that defines it (qore#6520)
DLLLOCAL const QoreProgram* qore_closure_code_program(const ResolvedCallReferenceNode* closure);
//! Returns the functional-domain restrictions of the code that creates an object or call reference now (qore#6520)
DLLLOCAL QoreParseOptions qore_creation_origin_restrictions();
//! Captures a runtime call reference without changing the public subclass layout.
DLLLOCAL void qore_capture_call_reference_provenance(const ResolvedCallReferenceNode* ref);
DLLLOCAL void qore_release_call_reference_provenance(const ResolvedCallReferenceNode* ref);
//! Returns a referenced context, or nullptr.
DLLLOCAL QoreSandboxContext* qore_get_call_reference_provenance(const ResolvedCallReferenceNode* ref);
DLLLOCAL ResolvedCallReferenceNode* qore_bind_sandbox_call(const ResolvedCallReferenceNode* callback, bool trusted,
    ExceptionSink* xsink);
//! Creates a capability that runs a fixed callback as a trusted build (see SandboxManager::createTrustedBuildCall())
/** Raises SANDBOX-CAPABILITY-ERROR and returns nullptr if the creation is refused
*/
DLLLOCAL ResolvedCallReferenceNode* qore_create_trusted_build_call(const ResolvedCallReferenceNode* callback,
    ExceptionSink* xsink);
//! Creates a capability that starts a fixed callback in a new thread that no sandbox governs
/** See SandboxManager::createSystemThreadCall(); raises SANDBOX-CAPABILITY-ERROR and returns nullptr if the creation is
    refused
*/
DLLLOCAL ResolvedCallReferenceNode* qore_create_system_thread_call(const ResolvedCallReferenceNode* callback,
    ExceptionSink* xsink);
//! The immutable strong callback edge of a bound call reference, or nullptr for other call references.
DLLLOCAL const ResolvedCallReferenceNode* qore_get_sandbox_bound_callback(const AbstractQoreNode* callback);

//! A capability established outside sandbox execution may mask only pre-existing policy provenance.
class QoreSandboxTrustedPolicyHelper {
public:
    DLLLOCAL QoreSandboxTrustedPolicyHelper();
    DLLLOCAL ~QoreSandboxTrustedPolicyHelper();
    QoreSandboxTrustedPolicyHelper(const QoreSandboxTrustedPolicyHelper&) = delete;
    QoreSandboxTrustedPolicyHelper& operator=(const QoreSandboxTrustedPolicyHelper&) = delete;
private:
    const QoreSandboxProvenanceHelper* old_boundary;
    unsigned old_depth;
    QoreSandboxPolicyBarrierHelper barrier;
};

#endif
