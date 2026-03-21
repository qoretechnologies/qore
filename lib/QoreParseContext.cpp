#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>

bool QoreParseContext::isLocalDefinitelyAssigned(LocalVar* local) const {
    return local && local->isAssigned();
}

bool QoreParseContext::needsGuardForLocal(LocalVar* local) const {
    static bool debug = [] {
        const char* debug_env = getenv("QORE_IR_DEBUG");
        return debug_env && strstr(debug_env, "guard");
    }();

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
