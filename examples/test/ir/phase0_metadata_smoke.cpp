/* -*- indent-tabs-mode: nil -*- */
/*
    phase0_metadata_smoke.cpp

    Qore Programming Language
*/

#include <cstdlib>
#include <iostream>
#include <string>

#include <qore/Qore.h>
#include <qore/QoreClass.h>
#include <qore/QoreObject.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreOpcodeRegistry.h>

static bool checkOpcodeRegistry() {
    std::string registry_error;
    if (!qore_ir_validate_opcode_registry(registry_error)) {
        std::cerr << "Opcode registry validation failed: " << registry_error << "\n";
        return false;
    }
    if (getOpcodeExpectedOperands(static_cast<int>(QoreIROpcode::AddInt)) != 2
            || !getOpcodeProducesResult(static_cast<int>(QoreIROpcode::AddInt))
            || getOpcodeProducesResult(static_cast<int>(QoreIROpcode::StoreLValue))) {
        std::cerr << "Opcode registry query smoke checks failed\n";
        return false;
    }
    if (!isCallInvokeOpcode(QoreIROpcode::CallDirect)
            || !isRegexInvokeOpcode(QoreIROpcode::RegexMatchBool)
            || !isDotEvalInvokeOpcode(QoreIROpcode::DotEvalObject)
            || !isRangeSliceOpcode(QoreIROpcode::RangeSliceFloat)
            || !isTerminator(QoreIROpcode::Invoke)
            || isTerminator(QoreIROpcode::AddInt)
            || isRangeSliceOpcode(QoreIROpcode::RangeInt)) {
        std::cerr << "Opcode registry property smoke checks failed\n";
        return false;
    }
    return true;
}

static bool checkQoreValueTagReservations() {
    if (QoreValue::firstPluginImmediateTag() != 0xFFFE
            || QoreValue::lastPluginImmediateTag() != 0xFFFF
            || !QoreValue::isReservedPluginImmediateTag(0xFFFE)
            || !QoreValue::isReservedPluginImmediateTag(0xFFFF)
            || QoreValue::isReservedPluginImmediateTag(0xFFFD)
            || !QoreValue::isBuiltinNanboxTag(0xFFC0)
            || !QoreValue::isBuiltinNanboxTag(0xFFCF)
            || !QoreValue::isBuiltinNanboxTag(0xFFF9)
            || !QoreValue::isBuiltinNanboxTag(0xFFFA)
            || !QoreValue::isBuiltinNanboxTag(0xFFFB)
            || !QoreValue::isBuiltinNanboxTag(0xFFFD)
            || QoreValue::isBuiltinNanboxTag(0xFFFE)) {
        std::cerr << "QoreValue tag reservation smoke checks failed\n";
        return false;
    }
    return true;
}

static bool checkTypeProfileBuiltins() {
    TypeProfile int_profile;
    for (int i = 0; i < 64; ++i) {
        int_profile.record(QoreValue(i));
    }
    QoreIRTypeProfileKey int_key = int_profile.dominantKey();
    if (!int_key.isBuiltin(NT_INT) || int_profile.dominantType() != NT_INT
            || !int_profile.dominantBuiltin(NT_INT)) {
        std::cerr << "Builtin TypeProfile smoke checks failed\n";
        return false;
    }

    TypeProfile mixed_profile;
    for (int i = 0; i < 90; ++i) {
        mixed_profile.record(QoreValue(i));
    }
    for (int i = 0; i < 10; ++i) {
        mixed_profile.record(QoreValue((double)i));
    }
    if (mixed_profile.dominantKey().isValid() || mixed_profile.dominantType() != NT_ALL
            || !mixed_profile.dominantKey(0.90f).isBuiltin(NT_INT)) {
        std::cerr << "Mixed TypeProfile threshold smoke checks failed\n";
        return false;
    }
    return true;
}

static bool checkTypeProfileClassPayload() {
    ExceptionSink xsink;
    QoreProgramHelper pgm_helper(xsink);
    if (xsink) {
        std::cerr << "Failed to initialize program for TypeProfile class checks\n";
        return false;
    }

    QoreClass* obj_class = new QoreClass("ProfileObject", "::ProfileObject");
    const QoreTypeInfo* obj_type = obj_class->getTypeInfo();
    {
        ValueHolder obj_holder(QoreValue(new QoreObject(obj_class, *pgm_helper)), &xsink);
        if (xsink) {
            std::cerr << "Failed to create object for TypeProfile class checks\n";
            qore_class_private::get(*obj_class)->deref(true, true);
            return false;
        }
        TypeProfile class_profile;
        for (int i = 0; i < 64; ++i) {
            class_profile.record(*obj_holder);
        }
        QoreIRTypeProfileKey class_key = class_profile.dominantKey();
        if (class_key.kind != QoreIRTypeProfileKind::QoreClass || class_key.type_info != obj_type
                || class_profile.dominantType() != NT_ALL || class_profile.getTypeInfoCount(obj_type) != 64) {
            std::cerr << "Class TypeProfile smoke checks failed\n";
            qore_class_private::get(*obj_class)->deref(true, true);
            return false;
        }
    }
    qore_class_private::get(*obj_class)->deref(true, true);
    return true;
}

int main() {
    qore_init(QL_GPL);
    bool ok = checkOpcodeRegistry()
        && checkQoreValueTagReservations()
        && checkTypeProfileBuiltins()
        && checkTypeProfileClassPayload();
    qore_cleanup();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
