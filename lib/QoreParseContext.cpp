/*
  QoreParseContext.cpp

  Qore Programming Language

  Copyright (C) 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#include <qore/Qore.h>
#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>

//! Answers for IR lowering, which runs after the parse stack has been unwound.
/** A body local keeps its flag when it leaves the stack, but a signature parameter has it reset so
    the next parse of the same signature starts clean.  Asking isAssigned() here therefore reports
    every parameter as possibly-NOTHING, and a call taking one as an argument falls back from
    CallDirect to the generic Call opcode carrying an AST expression - which also keeps the
    parameter AST-visible and out of ir_only_locals.  isAssignedAtParseEnd() reads the state
    recorded as the variable left the stack, which is still flow-sensitive: a parameter cleared by
    \c delete or \c remove is reported unassigned.
*/
bool QoreParseContext::isLocalDefinitelyAssigned(LocalVar* local) const {
    return local && local->isAssignedAtParseEnd();
}

bool QoreParseContext::needsGuardForLocal(LocalVar* local) const {
    static constinit QoreLazyValue<bool> debug_setting([]() -> bool {
        QoreEnvironmentValue debug_env_holder("QORE_IR_DEBUG");
        const char* debug_env = debug_env_holder.c_str();
        return debug_env && strstr(debug_env, "guard");
    });
    const bool debug = debug_setting.get();

    if (!local) {
        return false;
    }

    // Variables with non-optional types CAN be NOTHING until assigned
    // Parse analysis tells us if variable is definitely assigned
    // If NOT definitely assigned: variable can legitimately be NOTHING, don't guard
    // If definitively assigned: check if type allows NOTHING

    if (!local->isAssigned()) {
        if (debug) {
            fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-UNASSIGNED] Variable not assigned, can be NOTHING, no guard\n");
            fflush(stderr);
        }
        // Unassigned variables CAN be NOTHING - no guard needed (NOTHING is valid state)
        return false;
    }

    // Variable IS definitely assigned - check if type allows NOTHING
    const QoreTypeInfo* type = local->parseGetTypeInfo();
    if (debug) {
        fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-ASSIGNED] Variable is assigned, type=%p\n", type);
        fflush(stderr);
    }

    // If type allows NOTHING (is optional like *Type), no guard needed
    if (type && QoreTypeInfo::parseReturns(type, NT_NOTHING) != QTI_NOT_EQUAL) {
        if (debug) {
            fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-OPTIONAL] Type allows NOTHING, no guard needed\n");
            fflush(stderr);
        }
        return false;
    }

    // Variable is assigned AND type doesn't allow NOTHING
    // This case shouldn't need a guard because value was assigned from non-NOTHING source
    // But we must be conservative about what gets assigned
    if (debug) {
        fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-ASSIGNED-NON-OPTIONAL] Assigned non-optional var, no guard needed\n");
        fflush(stderr);
    }
    return false;
}

const QoreTypeInfo* QoreParseContext::guaranteedType(LocalVar* local) const {
    return local ? local->parseGetTypeInfo() : nullptr;
}

void QoreParseContext::markLocalAssignment(LocalVar* local, bool assigned, const QoreTypeInfo* type) {
    if (!local) {
        return;
    }

    if (assigned) {
        local->parseAssigned();
        if (type) {
            local->parseSetNarrowedType(type);
        }
    } else {
        local->parseUnassigned();
    }
}
