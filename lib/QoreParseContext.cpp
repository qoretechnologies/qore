#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>

bool QoreParseContext::isLocalDefinitelyAssigned(LocalVar* local) const {
    return local && local->isAssigned();
}

bool QoreParseContext::needsGuardForLocal(LocalVar* local) const {
    return local && !local->isAssigned();
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
