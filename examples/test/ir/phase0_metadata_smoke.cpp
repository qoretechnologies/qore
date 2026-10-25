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
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreIRVerifier.h>
#include <qore/intern/QoreOpcodeRegistry.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/VarRefNode.h>

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

// Copyright 2026 Qore Technologies, s.r.o.
// Upstream v21 assigned opcode 409 to the return check; performance v20-v22 used
// 409/410 for snapshots. Exercise the actual serializer and reader for both histories.
static bool checkAOTOpcodeCompatibility() {
    ExceptionSink xsink;
    QoreProgramHelper pgm(xsink);
    if (xsink) {
        std::cerr << "opcode compatibility Program creation failed\n";
        return false;
    }
    struct Case {
        QoreIROpcode opcode;
        uint16_t version;
        uint16_t max_opcode;
        bool legacy_return;
    } cases[] = {
        {QoreIROpcode::CheckReturnValue, 21, 409, true},
        {QoreIROpcode::CheckReturnValue, 22, 411, false},
        {QoreIROpcode::SnapshotInt, 20, 410, false},
        {QoreIROpcode::SnapshotFloat, 20, 410, false},
        {QoreIROpcode::SnapshotInt, 21, 410, false},
        {QoreIROpcode::SnapshotFloat, 21, 410, false},
        {QoreIROpcode::SnapshotInt, 22, 410, false},
        {QoreIROpcode::SnapshotFloat, 22, 410, false},
    };
    for (const auto& test : cases) {
        QoreIRFunction func("opcode-compatibility");
        QoreIRBuilder builder(&func);
        builder.setBlock(func.createBlock("entry"));
        bool floating = test.opcode == QoreIROpcode::SnapshotFloat;
        auto* value = floating ? builder.createConstFloat(2.5) : builder.createConstInt(7);
        QoreIRValue result = value->result;
        if (test.opcode == QoreIROpcode::CheckReturnValue) {
            builder.createCheckReturnValue(result, bigIntTypeInfo, nullptr, nullptr);
        } else {
            result = builder.createUnaryOp(test.opcode, result)->result;
        }
        builder.createReturn(result);
        QoreAOTBinaryWriter writer;
        uint32_t section = writer.beginSection(QoreAOTSectionType::FUNCTIONS);
        auto write_expr = [](QoreAOTBinaryWriter&, const QoreValue&) { return false; };
        if (!serializeIRFunction(writer, func, write_expr)) {
            std::cerr << "opcode compatibility serialization failed\n";
            return false;
        }
        writer.endSection(section);
        QoreAOTBinaryHeader header = {};
        header.magic = QORE_AOT_BINARY_MAGIC;
        header.version = test.version;
        header.max_opcode_id = test.max_opcode;
        header.label_offset = writer.strings.add("opcode-compatibility");
        std::vector<uint8_t> blob;
        if (!writer.finalize(header, blob)) {
            std::cerr << "opcode compatibility binary finalization failed\n";
            return false;
        }
        QoreAOTBinaryReader reader;
        std::string error;
        if (!reader.open(blob.data(), static_cast<uint32_t>(blob.size()), error)) {
            std::cerr << "opcode compatibility header failed: " << error << "\n";
            return false;
        }
        const auto* data_section = reader.findSection(QoreAOTSectionType::FUNCTIONS);
        if (!data_section) {
            std::cerr << "opcode compatibility functions section missing\n";
            return false;
        }
        const uint8_t* section_data = reader.getSectionData(*data_section);
        if (reader.getHeader().compression != QORE_AOT_COMPRESSION_NONE || !section_data) {
            std::cerr << "opcode compatibility fixture must have uncompressed section data\n";
            return false;
        }
        // Section offsets are relative to the data area, after the header, directory and string pool.
        size_t section_offset = section_data - blob.data();
        if (section_offset > blob.size() || data_section->size > blob.size() - section_offset) {
            std::cerr << "opcode compatibility section is outside its fixture\n";
            return false;
        }
        if (test.legacy_return) {
            size_t matches = 0;
            auto opcode = static_cast<uint16_t>(QoreIROpcode::CheckReturnValue);
            for (size_t i = section_offset; i + 1 < section_offset + data_section->size; ++i) {
                if (i && i % 100 == 0 && qore_check_cancel(&xsink, "AOT opcode fixture")) {
                    return false;
                }
                if (blob[i] == (opcode & 255) && blob[i + 1] == (opcode >> 8)) {
                    blob[i] = 409 & 255;
                    blob[i + 1] = 409 >> 8;
                    ++matches;
                }
            }
            if (matches != 1) {
                std::cerr << "legacy opcode fixture did not identify one return check\n";
                return false;
            }
        }
        const uint8_t* ptr = reader.getSectionData(*data_section);
        const uint8_t* end = ptr + data_section->size;
        auto read_expr = [](const QoreAOTBinaryReader&, const uint8_t*&, const uint8_t*, std::string& err) {
            err = "unexpected expression in opcode compatibility fixture";
            return QoreValue();
        };
        auto restored = deserializeIRFunction(reader, ptr, end, *pgm, read_expr, nullptr, error);
        if (!restored || !error.empty() || ptr != end || restored->blocks.size() != 1
                || restored->blocks[0]->instructions.size() != 3
                || restored->blocks[0]->instructions[1]->opcode != test.opcode
                || (test.opcode == QoreIROpcode::CheckReturnValue
                    && restored->blocks[0]->instructions[1]->element_type != bigIntTypeInfo)) {
            std::cerr << "opcode compatibility round trip failed: " << error << "\n";
            return false;
        }
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

static bool checkReadonlyCreateParseRefVerifier() {
    LocalVar readonly_ref_local("readonly_ref_local", bigIntTypeInfo);
    readonly_ref_local.setReadOnly();

    QoreIRFunction func("ir_create_parse_ref_readonly_verify_fail");
    QoreIRBuilder builder(&func);
    auto* entry = func.createBlock("entry");
    builder.setBlock(entry);

    QoreValue ref_exp(new ParseReferenceNode(nullptr,
        QoreValue(new VarRefNode(nullptr, strdup("readonly_ref_local"), &readonly_ref_local, false))));
    ValueHolder ref_holder(ref_exp, nullptr);
    builder.createCreateParseRef(ref_holder->get<ParseReferenceNode>(), *ref_holder, nullptr);
    builder.createReturnNothing();

    std::string error;
    if (QoreIRVerifier::verify(func, error)
            || error.find("readonly local 'readonly_ref_local' used as write target by CreateParseRef")
                == std::string::npos) {
        std::cerr << "Readonly CreateParseRef verifier check failed";
        if (!error.empty()) {
            std::cerr << ": " << error;
        }
        std::cerr << "\n";
        return false;
    }
    return true;
}

static bool checkPhiOrderVerifier() {
    QoreIRFunction func("ir_verify_phi_order");
    QoreIRBuilder builder(&func);
    auto* entry = func.createBlock("entry");
    auto* then_block = func.createBlock("then");
    auto* else_block = func.createBlock("else");
    auto* merge_block = func.createBlock("merge");
    builder.setBlock(entry);
    auto* cond = builder.createConstBool(true);
    builder.createBranchIf(cond->result, then_block, else_block);
    builder.setBlock(then_block);
    auto* c1 = builder.createConstInt(1);
    builder.createBranch(merge_block);
    builder.setBlock(else_block);
    auto* c2 = builder.createConstInt(2);
    builder.createBranch(merge_block);
    builder.setBlock(merge_block);
    builder.createConstInt(0);
    auto* phi = builder.createPhi({
        {c1->result, then_block},
        {c2->result, else_block},
    });
    builder.createReturn(phi->result);

    std::string error;
    if (QoreIRVerifier::verify(func, error)
            || error.find("phi instruction not grouped") == std::string::npos) {
        std::cerr << "PHI order verifier check failed";
        if (!error.empty()) {
            std::cerr << ": " << error;
        }
        std::cerr << "\n";
        return false;
    }
    return true;
}

int main() {
    qore_init(QL_GPL);
    bool ok = checkOpcodeRegistry()
        && checkAOTOpcodeCompatibility()
        && checkQoreValueTagReservations()
        && checkTypeProfileBuiltins()
        && checkTypeProfileClassPayload()
        && checkReadonlyCreateParseRefVerifier()
        && checkPhiOrderVerifier();
    qore_cleanup();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
