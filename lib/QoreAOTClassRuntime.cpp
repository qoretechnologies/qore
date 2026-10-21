/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTClassRuntime.cpp

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

// Keep specialized acceptance outside the SCU: adding its inline call graph to
// the shared runtime unit can change unrelated hot-path code generation/layout.
#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreException.h"
#include "qore/intern/QoreJITException.h"
#include "qore/intern/Variable.h"

namespace {
// Same live condition as ExceptionSink::operator bool(), including thread exit.
bool hasPendingException(const ExceptionSink& xsink) {
    const qore_es_private* data = qore_es_private::get(xsink);
    return data->head || data->thread_exit;
}

// Match the ordinary pinned initializer's strong-object admission. Weak wrappers
// are not strong objects; preserve generic handling of non-pointer encodings.
const QoreObject* getStrongObject(const QoreValue& value) {
    if (value.isPointer()) {
        const AbstractQoreNode* node = value.getPtr();
        return node && node->getType() == NT_OBJECT ? static_cast<const QoreObject*>(node) : nullptr;
    }
    return value.getType() == NT_OBJECT ? value.get<const QoreObject>() : nullptr;
}
}

bool QoreTypeInfo::acceptsRequiredClassObjectFirstPublicBase(const QoreClass& constraint,
        const QoreObject& object) {
    const qore_class_private* target = qore_class_private::get(constraint);
    // These are the same live fields returned by getInstantiatedTypeInfo() and
    // getClass(); do not add out-of-line getter calls in this separate unit.
    const qore_object_private* object_data = qore_object_private::get(object);
    if (object_data->instantiated_type && target->hasTypeParams() && !target->rawAcceptsParameterized()) {
        return false;
    }
    const QoreClass* object_class = object_data->theclass;
    const qore_class_private* actual = qore_class_private::get(*object_class);
    if (actual == target) {
        return true;
    }
    if (actual->scl && !actual->scl->empty()) {
        const BCNode* first = actual->scl->front();
        if (first->access == Public && first->sclass && qore_class_private::get(*first->sclass) == target) {
            // Self equivalence would also accept publicly. Otherwise the first
            // edge is the ordinary lookup's first match. Never scan later edges:
            // an earlier private structural/injected equivalent can take priority.
            return true;
        }
    }
    // Keep all other structural/injected identity and current private-access
    // rules in the ordinary implementation. Its raw-object gate remains live.
    return acceptsRequiredClassObject(constraint, object);
}

extern "C" DLLEXPORT int qore_rt_try_assign_pinned_class_local_aot_v4(QoreAOTContext* ctx,
        int32_t idx, uint64_t value, void* prepared_slot, const QoreClass* constraint, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals && xsink);
    if (!constraint) {
        return qore_rt_try_assign_pinned_class_local_aot_v2(ctx, idx, value, prepared_slot, xsink);
    }
    auto* slot = static_cast<LocalVarValue*>(prepared_slot);
    assert(slot && slot->local_var == ctx->locals[idx] && !slot->frame_boundary);
    if (hasPendingException(*xsink) || slot->finalized || slot->val.assigned || slot->val.static_assignment
            || slot->val.fixed_type) {
        return 0;
    }
    const QoreObject* object = getStrongObject(fromBits(value));
    if (!object || !QoreTypeInfo::acceptsRequiredClassObjectFirstPublicBase(*constraint, *object)) {
        return 0;
    }
    // Preserve v3's actual owner, custom handlers, tracing and publication order.
    // The admitted node is a strong object, so its real-reference marker does not
    // need the generic node-type dispatch. No ownership or mark is elided.
#ifdef DEBUG
    object->ref();
#else
    object->refImpl();
#endif
    slot->val.assignInitialNodeWithMarker(const_cast<QoreObject*>(object), [](AbstractQoreNode* node) {
        qore_object_private* data = qore_object_private::get(*static_cast<QoreObject*>(node));
#ifdef DEBUG
        data->setRealReference();
#else
        data->setRealReferenceImpl();
#endif
    });
    return 1;
}

extern "C" DLLEXPORT int qore_rt_try_assign_pinned_class_local_aot_throwing_v4(QoreAOTContext* ctx,
        int32_t idx, uint64_t value, void* prepared_slot, const QoreClass* constraint, ExceptionSink* xsink) {
    int handled = qore_rt_try_assign_pinned_class_local_aot_v4(ctx, idx, value, prepared_slot, constraint, xsink);
    if (hasPendingException(*xsink)) {
        throw QoreJITException();
    }
    return handled;
}
