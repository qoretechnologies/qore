/* -*- mode: c++; indent-tabs-mode: nil -*- */
/* Copyright (C) 2026 Qore Technologies, s.r.o.; see COPYING.MIT for license terms. */

#ifndef _QORE_SANDBOX_PROVENANCE_H
#define _QORE_SANDBOX_PROVENANCE_H

#include <qore/QoreSandboxManager.h>
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
};

//! Captures provenance, returning nullptr when no sandbox governs execution.
DLLLOCAL QoreSandboxContext* qore_capture_sandbox_provenance();
//! Captures a runtime call reference without changing the public subclass layout.
DLLLOCAL void qore_capture_call_reference_provenance(const ResolvedCallReferenceNode* ref);
DLLLOCAL void qore_release_call_reference_provenance(const ResolvedCallReferenceNode* ref);
//! Returns a referenced context, or nullptr.
DLLLOCAL QoreSandboxContext* qore_get_call_reference_provenance(const ResolvedCallReferenceNode* ref);
DLLLOCAL ResolvedCallReferenceNode* qore_bind_sandbox_call(const ResolvedCallReferenceNode* callback, bool trusted,
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
