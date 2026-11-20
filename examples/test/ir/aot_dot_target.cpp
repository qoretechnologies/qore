/* -*- mode: c++; indent-tabs-mode: nil -*- */
// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <qore/Qore.h>
#include <qore/QoreAOT.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/QoreAOTExprSlotRegistry.h>
#include <qore/intern/QoreAOTInstRegistry.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>

static bool finalize(QoreAOTBinaryWriter& writer, uint16_t version, std::vector<uint8_t>& image) {
    QoreAOTBinaryHeader header{};
    header.magic = QORE_AOT_BINARY_MAGIC;
    header.version = version;
    header.feature_flags = writer.feature_flags;
    return writer.finalize(header, image);
}

static bool previousFormat() {
    if (QORE_AOT_BINARY_VERSION < 51) {
        puts("waiting for AO format v51");
        return true;
    }
    QoreAOTBinaryWriter writer;
    std::vector<uint8_t> image;
    QoreAOTBinaryReader reader;
    std::string error;
    if (!finalize(writer, 50, image) || reader.open(image.data(), image.size(), error)
            || error.find("unsupported binary format version 50 ") != 0
            || error.find("rebuild the artifact") == std::string::npos) {
        fprintf(stderr, "expected v50 to be below the reader floor, got '%s'\n", error.c_str());
        return false;
    }
    puts("v50 refused as obsolete");
    return true;
}

// Exercise the real slot writer or the inline expression writer, then read from the finalized image's string pool.
static bool roundTrip(QoreProgram* pgm, const QoreClass* owner, const QoreClass* receiver,
        const QoreMethod* method, bool selected, bool slot) {
    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    const auto* variant_method = method ? method : owner->findLocalMethod("pick");
    const auto* variant = selected ? qore_method_private::get(*variant_method)->getFunction()->first() : nullptr;
    const uint8_t kind = static_cast<uint8_t>(AOTExprKind::DOT_EVAL_TARGET);
    AOTExprSlotId id;
    id.kind = AOTExprKind::DOT_EVAL_TARGET;
    id.ref1 = method ? receiver->getNamespacePath() : "";
    id.ref2 = "pick";
    if (method) {
        id.flags = method->isStatic() ? QORE_AOT_DOT_EVAL_TARGET_STATIC : QORE_AOT_DOT_EVAL_TARGET_INSTANCE;
        id.ref3 = owner->getNamespacePath();
        if (variant) {
            id.ref2 += "\n" + id.ref3 + "\n" + variant->getSignature()->getSignatureText();
        }
    }
    // Build the input AST through the image reader. This also exercises the compiler's handling of a dynamic call
    // that has only a parse-time argument-checking variant. The writer must leave its dispatch by name.
    if (!slot) {
        QoreAOTBinaryWriter seed;
        uint32_t part = seed.beginSection(QoreAOTSectionType::TOPLEVEL);
        seed.writeU8(kind);
        seed.writeStringRef(id.ref1.c_str());
        seed.writeStringRef(id.ref2.c_str());
        seed.writeU8(id.flags);
        if (method) {
            seed.writeStringRef(id.ref3.c_str());
        }
        seed.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        seed.writeU8(0);
        seed.endSection(part);
        std::vector<uint8_t> data;
        QoreAOTBinaryReader reader;
        std::string error;
        if (!finalize(seed, QORE_AOT_BINARY_VERSION, data) || !reader.open(data.data(), data.size(), error)) {
            return false;
        }
        const auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
        const uint8_t* ptr = reader.getSectionData(*sh);
        const uint8_t* end = ptr + sh->size;
        ValueHolder expr(readOneExpr(reader, ptr, end, error, pgm, nullptr, 0, nullptr, 0), nullptr);
        auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(expr->getInternalNode());
        if (!de || !error.empty() || ptr != end) {
            fprintf(stderr, "cannot read input expression: %s\n", error.c_str());
            return false;
        }
        if (!method && selected) {
            de->getMethodCall()->setVariant(variant);
        }
        if (!classifyAndWriteExpr(writer, *expr, {}, {})) {
            return false;
        }
    } else {
        writer.writeU8(kind);
        AOTExprSlotWriteCtx ctx{writer, id, {}, {}, nullptr};
        if (!getAOTExprSlotKindInfo(kind)->write_fn(ctx)) {
            return false;
        }
    }
    writer.endSection(section);
    QoreAOTBinaryReader reader;
    std::vector<uint8_t> image;
    std::string error;
    if (!finalize(writer, QORE_AOT_BINARY_VERSION, image) || !reader.open(image.data(), image.size(), error)) {
        return false;
    }
    auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    const uint8_t* ptr = reader.getSectionData(*sh);
    const uint8_t* end = ptr + sh->size;
    if (ptr == end || *ptr != kind) {
        return false;
    }
    const QoreClass* actual_receiver = nullptr;
    const QoreMethod* actual_method = nullptr;
    const AbstractQoreFunctionVariant* actual_variant = nullptr;
    if (slot) {
        ++ptr;
        QoreAOTDotTarget target;
        if (qore_aot_read_dot_target(reader, ptr, end, pgm, true, target, error)) {
            actual_receiver = target.receiver;
            actual_method = target.method;
            actual_variant = target.variant;
        }
    } else {
        ValueHolder expr(readOneExpr(reader, ptr, end, error, pgm, nullptr, 0, nullptr, 0), nullptr);
        if (error.empty()) {
            auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(expr->getInternalNode());
            if (!de) {
                return false;
            }
            const auto* mc = de->getMethodCall();
            actual_receiver = mc->getClass();
            actual_method = mc->getMethod();
            actual_variant = mc->getVariant();
        }
    }
    return error.empty() && ptr == end && actual_receiver == (method ? receiver : nullptr)
        && actual_method == method && actual_variant == (method ? variant : nullptr);
}

static bool refused(QoreProgram* pgm, bool slot, uint8_t flags, const char* name, const char* owner,
        const char* reason) {
    QoreAOTBinaryWriter writer;
    uint32_t section = writer.beginSection(QoreAOTSectionType::TOPLEVEL);
    writer.writeStringRef("::Receiver");
    writer.writeStringRef(name);
    writer.writeU8(flags);
    if (owner) {
        writer.writeStringRef(owner);
    } else if (flags & QORE_AOT_DOT_EVAL_TARGET_FAMILY) {
        writer.writeU32(UINT32_MAX);
    }
    writer.endSection(section);
    std::vector<uint8_t> data;
    QoreAOTBinaryReader reader;
    std::string error;
    if (!finalize(writer, QORE_AOT_BINARY_VERSION, data) || !reader.open(data.data(), data.size(), error)) {
        return false;
    }
    auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
    const uint8_t* ptr = reader.getSectionData(*sh);
    const uint8_t* end = ptr + sh->size;
    QoreAOTDotTarget target;
    return !qore_aot_read_dot_target(reader, ptr, end, pgm, slot, target, error)
        && error.find(reason) != std::string::npos;
}

// Exercise the group-specific payload through the same readers and writers used by serialized closure IR.
static bool instructionRecord(QoreProgram* pgm, const QoreClass* receiver, QoreIRInstGroup group,
        uint8_t flags, const std::string& method_ref, const char* owner,
        const QoreMethod* method, const AbstractQoreFunctionVariant* variant,
        const char* expected_error = nullptr, size_t trim_tail = 0, bool invalid_successor = false) {
    const bool invoke = group == QoreIRInstGroup::InvokeDotEvalMethodDirect;
    const auto* codec = getAOTInstGroupInfo(static_cast<uint8_t>(group));
    QoreAOTBinaryWriter seed;
    seed.feature_flags = QORE_AOT_FEAT_DOT_EVAL_PSEUDO_FLAGS;
    uint32_t section = seed.beginSection(QoreAOTSectionType::TOPLEVEL);
    seed.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
    seed.writeStringRef("::Receiver");
    seed.writeStringRef(method_ref.c_str());
    seed.writeU8(flags);
    if (owner) {
        seed.writeStringRef(owner);
    } else if (flags & QORE_AOT_DOT_EVAL_TARGET_FAMILY) {
        seed.writeU32(UINT32_MAX);
    }
    seed.writeU8(1); // has_ref_args
    seed.writeU8(0x21); // two distinct pseudo optimization flags
    std::vector<std::unique_ptr<QoreIRBasicBlock>> blocks;
    std::unordered_map<const QoreIRBasicBlock*, uint16_t> indexes;
    if (invoke) {
        for (const char* name : {"normal", "catch"}) {
            blocks.push_back(std::make_unique<QoreIRBasicBlock>(name));
            uint16_t index = static_cast<uint16_t>(blocks.size() - 1);
            indexes.emplace(blocks.back().get(), index);
            seed.writeU16(invalid_successor && index == 1 ? 2 : index);
        }
    }
    seed.endSection(section);

    AOTExprReadFunc read_expr = [pgm](const QoreAOTBinaryReader& r, const uint8_t*& ptr, const uint8_t* end,
            std::string& error) { return readOneExpr(r, ptr, end, error, pgm, nullptr, 0, nullptr, 0); };
    AOTExprWriteFunc write_expr = [](QoreAOTBinaryWriter& w, const QoreValue& v) {
        return classifyAndWriteExpr(w, v, {}, {});
    };
    QoreAOTBinaryWriter encoded;
    encoded.feature_flags = seed.feature_flags;
    const auto opcode = invoke ? QoreIROpcode::InvokeDotEvalMethodDirect : QoreIROpcode::DotEvalMethodDirect;
    for (auto* writer : {&seed, &encoded}) {
        std::vector<uint8_t> image;
        QoreAOTBinaryReader reader;
        std::string error;
        if (!finalize(*writer, QORE_AOT_BINARY_VERSION, image) || !reader.open(image.data(), image.size(), error)) {
            return false;
        }
        auto* sh = reader.findSection(QoreAOTSectionType::TOPLEVEL);
        const uint8_t* ptr = reader.getSectionData(*sh);
        if (trim_tail > sh->size) {
            return false;
        }
        const uint8_t* end = ptr + sh->size - trim_tail;
        AOTInstReadCtx ctx{reader, ptr, end, blocks, {}, read_expr, pgm, pgm, error};
        auto inst = codec->read_fn(static_cast<uint16_t>(opcode), nullptr, {}, 3, ctx);
        if (invalid_successor) {
            // The enclosing function reader refuses an instruction that set this flag.
            return ctx.bad_block_index;
        }
        if (expected_error) {
            return !inst && error.find(expected_error) != std::string::npos;
        }
        if (!inst || ptr != end || !error.empty() || ctx.bad_block_index) {
            fprintf(stderr, "%s decode: %s\n", codec->name, error.c_str());
            return false;
        }
        auto matches = [&](const auto& dot) {
            return dot.qc == receiver && dot.method == method && dot.variant == variant && !dot.pseudo && dot.has_ref_args
                && dot.pseudo_base_known_string && dot.pseudo_arg0_known_assigned_int
                && dot.fallback_method_name && !strcmp(dot.fallback_method_name, "pick");
        };
        if (invoke) {
            const auto& dot = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction&>(*inst);
            if (!matches(dot) || dot.normal_target != blocks[0].get() || dot.exception_target != blocks[1].get()) {
                return false;
            }
        } else if (!matches(static_cast<const QoreIRDotEvalMethodDirectInstruction&>(*inst))) {
            return false;
        }
        if (writer == &seed) {
            uint32_t part = encoded.beginSection(QoreAOTSectionType::TOPLEVEL);
            AOTInstWriteCtx output{encoded, inst.get(), indexes, write_expr};
            if (!codec->write_fn(output)) {
                return false;
            }
            encoded.endSection(part);
        }
    }
    return true;
}

static bool records() {
    ExceptionSink xsink;
    QoreProgramHelper pgm(PO_MODERN, xsink);
    pgm->parse("class Base { int pick(int v) { return v; } static string pick(string v) { return v; }"
        " int onlyInstance() { return 1; } } class Receiver inherits Base {}", "method family test", &xsink);
    const QoreClass* owner = pgm->findClass("Base", &xsink);
    const QoreClass* receiver = pgm->findClass("Receiver", &xsink);
    if (xsink || !owner || !receiver) {
        xsink.handleExceptions();
        return false;
    }
    const QoreMethod* methods[] = {nullptr, owner->findLocalMethod("pick"), owner->findLocalStaticMethod("pick")};
    if (!methods[1] || !methods[2]) {
        return false;
    }
    for (bool slot : {false, true}) {
        for (const auto* method : methods) {
            for (bool selected : {false, true}) {
                if (!roundTrip(*pgm, owner, receiver, method, selected, slot)) {
                    fprintf(stderr, "%s %s, selected=%d failed\n", slot ? "slot" : "inline",
                        method ? (method->isStatic() ? "static" : "instance") : "unbound", selected);
                    return false;
                }
            }
        }
        if (!refused(*pgm, slot, 0x0c, "pick", "", "flags") || !refused(*pgm, slot, 0x10, "pick", "", "flags")
                || !refused(*pgm, slot, 0, "pick\n::Base\nint v", nullptr, "unbound overload")
                || !refused(*pgm, slot, 4, "pick", nullptr, "class reference")
                || !refused(*pgm, slot, 4, "pick", "::Missing", "declaration")
                || !refused(*pgm, slot, 8, "onlyInstance", "::Base", "declaration")
                || !refused(*pgm, slot, 4, "pick\n::Base\nstring v", "::Base", "overload")) {
            fprintf(stderr, "%s invalid target accepted or wrong diagnostic\n", slot ? "slot" : "inline");
            return false;
        }
    }
    for (auto group : {QoreIRInstGroup::DotEvalMethodDirect, QoreIRInstGroup::InvokeDotEvalMethodDirect}) {
        for (const auto* m : methods) {
            uint8_t family = 0;
            if (m) {
                family = m->isStatic() ? QORE_AOT_DOT_EVAL_TARGET_STATIC : QORE_AOT_DOT_EVAL_TARGET_INSTANCE;
            }
            for (bool selected : {false, true}) {
                if (!m && selected) {
                    continue; // a genuinely dynamic invocation cannot record a selected overload
                }
                const auto* v = selected ? qore_method_private::get(*m)->getFunction()->first() : nullptr;
                std::string name("pick");
                if (v) {
                    name += "\n::Base\n";
                    name += v->getSignature()->getSignatureText();
                }
                if (!instructionRecord(*pgm, receiver, group, family, name, m ? "::Base" : nullptr, m, v)) {
                    fprintf(stderr, "instruction group %u, family %u, selected=%d failed\n",
                        static_cast<unsigned>(group), family, selected);
                    return false;
                }
            }
        }
        auto rejects = [&](uint8_t flags, const char* name, const char* decl, const char* error) {
            return instructionRecord(*pgm, receiver, group, flags, name, decl, nullptr, nullptr, error);
        };
        bool invoke = group == QoreIRInstGroup::InvokeDotEvalMethodDirect;
        if (invoke && !instructionRecord(*pgm, receiver, group, 0x04, "pick", "::Base",
                nullptr, nullptr, nullptr, 0, true)) {
            fprintf(stderr, "invoke accepted an invalid exception successor\n");
            return false;
        }
        size_t suffix_size = invoke ? 6 : 2;
        if (!instructionRecord(*pgm, receiver, group, 0x04, "pick", "::Base", nullptr, nullptr,
                    "truncated direct dot instruction", 1)
                || !instructionRecord(*pgm, receiver, group, 0x04, "pick", "::Base", nullptr, nullptr,
                    "declaring class reference", suffix_size + 1)
                || !rejects(0x02, "pick", nullptr, "flags") || !rejects(0x06, "pick", "::Base", "flags")
                || !rejects(0x0c, "pick", "::Base", "flags") || !rejects(0x10, "pick", nullptr, "flags")
                || !rejects(0x04, "pick", nullptr, "class reference")
                || !rejects(0x08, "onlyInstance", "::Base", "declaration")
                || !rejects(0x04, "pick\n::Base\nstring v", "::Base", "overload")
                || !rejects(0, "pick\n::Base\nint v", nullptr, "unbound overload")) {
            fprintf(stderr, "invalid method identity accepted in instruction group %u\n", static_cast<unsigned>(group));
            return false;
        }
    }
    puts("dot target records passed");
    return true;
}

int main(int argc, char** argv) {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    bool ok = argc == 2 && (strcmp(argv[1], "records") == 0 ? records()
        : strcmp(argv[1], "floor") == 0 && previousFormat());
    qore_cleanup();
    return ok ? 0 : 1;
}
