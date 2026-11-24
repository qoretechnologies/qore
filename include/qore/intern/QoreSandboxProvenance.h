/* -*- mode: c++; indent-tabs-mode: nil -*- */
/* Copyright (C) 2026 Qore Technologies, s.r.o.; see COPYING.MIT for license terms. */

#ifndef _QORE_SANDBOX_PROVENANCE_H
#define _QORE_SANDBOX_PROVENANCE_H

#include <qore/QoreSandboxManager.h>
#include <qore/QoreParseOptions.h>
#include <vector>

class AbstractQoreNode;
class ResolvedCallReferenceNode;
class QoreSandboxContext;

//! Immutable intersections; the constituent managers remain live, referenced policies.
class QoreSandboxComposition {
public:
    //! Returns a referenced intersection, flattening and deduplicating its borrowed arguments.
    DLLLOCAL static QoreSandboxManager* combine(const std::vector<QoreSandboxManager*>& managers);
    DLLLOCAL static bool same(const QoreSandboxManager* a, const QoreSandboxManager* b);
    DLLLOCAL static const std::vector<QoreSandboxManager*>* members(const QoreSandboxManager* manager);

    //! Gives a new manager the isolation domain of the sandbox that governs the code that creates it, if any
    //! (qore#6536)
    /** @return 0 for OK, -1 if the creating code's sandbox combines the sandboxes of different isolation domains
        (or one with a domain and one without), which cannot create a sandbox (\c SANDBOX-ISOLATION-ERROR is raised)
    */
    DLLLOCAL static int inheritIsolationDomain(QoreSandboxManager* sm, ExceptionSink* xsink);

    //! Gives the isolation domain of a manager, folding a composite (qore#6536, qore#7009)
    /** A composite has the domain that its members with a domain have (a member without one adds nothing); if they
        have different domains, or one of them is mixed (a copy of such a composite), it is mixed.

        @param sm the manager
        @param domain the domain; empty if none, MixedIsolationDomain if mixed

        @return 1 if the manager has a domain, 0 if it has none, -1 if it is mixed
    */
    DLLLOCAL static int foldIsolationDomain(const QoreSandboxManager* sm, std::string& domain);

    //! Returns true if the calling code's governing sandbox is mixed (see foldIsolationDomain()) (qore#7009)
    /** Fails closed: true if the sandbox cannot be resolved for lack of memory while isolation domains are in use
    */
    DLLLOCAL static bool callerIsMixed();

    //! The mixed isolation domain value: never a domain that can be set (see qore_isolation_domain_is_mixed())
    DLLLOCAL static const std::string MixedIsolationDomain;

    //! Returns the isolation domain of a manager that governs code, or nullptr if it has none (qore#6536)
    /** The domain of a manager cannot change once it governs code, so it is read without a lock; a composite has no
        domain of its own (see its members)
    */
    DLLLOCAL static const std::string* governingIsolationDomain(const QoreSandboxManager* sm);

    //! Marks the manager as governing code: its isolation domain is fixed from then on (qore#6536)
    DLLLOCAL static void setGoverns(QoreSandboxManager* sm);
};

class QoreProgram;
class QoreObject;

//! Captures provenance, returning nullptr when no sandbox governs execution and no origin is recorded.
/** @param code_pgm the Program whose code an object created now runs (its class's), or nullptr for an object of a
    builtin class: the functional-domain restrictions of the creating code are recorded too if the code runs where the
    creating code is not on the stack (see qore_creation_origin_for()) (qore#6520, qore#6844)
    @param spans true if a base class of the object's class has the code of another Program
*/
DLLLOCAL QoreSandboxContext* qore_capture_sandbox_provenance(const QoreProgram* code_pgm = nullptr,
    bool spans = false);
//! Returns the functional-domain restrictions that an object or call reference created now records (qore#6844)
/** Its code runs for the code that creates it: the restrictions of that code are recorded if the code that it runs
    is the code of a module (a library module or one loaded by its path, also when module code creates it), whose
    Program does not decide for the code that calls it, of a base class of a module (\a spans), or builtin code
    (\a code_pgm is nullptr, for a call reference); empty for the code of a Program that is not a module's (the
    creating Program's own, or the host's that it imported), which runs with that Program on the stack, or if the
    creating code has no restrictions

    @param code_pgm the Program whose code the object or call reference runs, or nullptr for builtin code
    @param spans true if a base class of the object's class has the code of another Program
*/
DLLLOCAL QoreParseOptions qore_creation_origin_for(const QoreProgram* code_pgm, bool spans = false);
//! Records the functional-domain restrictions of the code that creates a call reference (qore_creation_origin_for())
/** Called by the constructor of each run-time call reference class once the reference is complete (qore#6520,
    qore#6844); does nothing if they are empty
*/
DLLLOCAL void qore_capture_call_reference_origin(const ResolvedCallReferenceNode* ref, const QoreProgram* code_pgm);
//! Returns true if a sandbox governs the Program of the class of the object, now (qore#7305)
/** The object holds a dependency reference to the Program of its class (the Program in whose context it was created,
    see qore_class_private::execConstructor()), not that of the code that created it; the Program's own sandbox and
    that of the code that created the Program count (qore_program_private::getSandboxManagerRef())
*/
DLLLOCAL bool qore_object_program_is_sandboxed(const QoreObject* obj);
//! Returns the Program whose code a closure runs: the Program that defines it (qore#6520)
DLLLOCAL const QoreProgram* qore_closure_code_program(const ResolvedCallReferenceNode* closure);
//! Returns the restrictions of a callable value or object as a mask of parse options (see the public
//! qore_callable_origin_restrictions()), for code of the library that gates the callables it accepts (qore#7056)
DLLLOCAL QoreParseOptions qore_callable_origin_restrictions_po(const QoreValue v);
//! Calls host code with the host's parse options under the sandbox of another Program (SandboxManager::callGoverned())
/** @param factory the host code to call: it must have no restrictions (qore_callable_origin_restrictions_po())
    @param governed the Program whose sandbox governs the call and whose application the work belongs to (referenced
    by the caller for the call)
    @param args the arguments for \a factory
    @param xsink for exceptions: SANDBOX-CALL-GOVERNED-ERROR if the call is refused, or those of \a factory
*/
DLLLOCAL QoreValue qore_sandbox_call_governed(const ResolvedCallReferenceNode* factory, QoreProgram* governed,
    const QoreListNode* args, ExceptionSink* xsink);

//! Returns the restrictions of the code that authored a callable value or object as parse options (qore#7056)
/** As qore_callable_origin_restrictions_po(), without the restrictions that module or builtin code recorded when it
    created the value for the code that called it: the restrictions of a closure, function or method of a Program that
    is not a module's, of a call reference or object that the code of such a Program created (also of a builtin class
    or of a class that the host imported), and of the code of a child Program that it created; a closure is judged by
    the Program that defines it and an object by the code of its class (an object of a module or builtin class runs
    only that code, whichever code creates it); a closure, call reference or object that module code creates for a
    restricted caller is the module's capability, which the sandbox of the caller governs (its thread limit and
    policy), and gives 0.  The schedulers that refuse code that may not
    control threads use this (qore_check_thread_callable()).
*/
DLLLOCAL QoreParseOptions qore_callable_authored_restrictions_po(const QoreValue v);

//! Returns the restricting options of a Program that is not a module's: those of the code that it authors (qore#7087)
DLLLOCAL QoreParseOptions qore_program_authored_restrictions_po(const QoreProgram* pgm);

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
