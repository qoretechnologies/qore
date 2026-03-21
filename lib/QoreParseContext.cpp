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
    // Check the variable's declared type
    const QoreTypeInfo* type = local->parseGetTypeInfo();
    if (debug) {
        fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL] Checking local var, type=%p, assigned=%d\n",
                type, local->isAssigned());
        fflush(stderr);
    }
    // If type allows NOTHING (is optional like *Type), guard is not needed
    // NOTHING is a valid value for optional types
    if (type && QoreTypeInfo::parseReturns(type, NT_NOTHING) != QTI_NOT_EQUAL) {
        if (debug) {
            fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-OPTIONAL] Local var type allows NOTHING, no guard needed\n");
            fflush(stderr);
        }
        return false;
    }
    if (debug) {
        fprintf(stderr, "[GUARD-NEEDSGUARD-LOCAL-NON-OPTIONAL] Local var type is non-optional, assigned=%d\n",
                local->isAssigned());
        fflush(stderr);
    }
    // Type doesn't allow NOTHING - need guard only if not definitely assigned
    // (uninitialized variable would be NOTHING, violating the type contract)
    return !local->isAssigned();
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
