/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTRuntime.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/Variable.h"
#include "qore/intern/GlobalVariableList.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"

#include "qore/intern/ModuleInfo.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/qore_thread_intern.h"

#include <cassert>
#include <cstring>
#include <string>
#include <unordered_map>

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

// Defined in QoreAOT.cpp - generates unique variant key with parameter types
extern std::string getVariantKey(const char* name, const AbstractQoreFunctionVariant* variant);

// ---- Slot Map Context Builder (V2 — no IR re-lowering) ----

//! Helper to convert a QoreValue to NaN-boxed bits
static inline uint64_t toBitsNB(QoreValue v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
}

//! Resolve an expression slot identity to NaN-boxed QoreValue bits
/** Looks up the referenced function/method/class in the program's namespace tree
    and creates the appropriate AST node.
    @param kind the expression kind
    @param ref1 primary reference (function name, class path)
    @param ref2 secondary reference (method name)
    @param pgm the QoreProgram for namespace lookups
    @return NaN-boxed bits, or 0 if unresolvable
*/
static uint64_t resolveExprSlot(AOTExprKind kind, const char* ref1, const char* ref2,
        QoreProgram* pgm) {
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    switch (kind) {
        case AOTExprKind::FUNC_CALL: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            // Look up function by name
            const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                *pp->RootNS, ref1);
            if (!fe) {
                printd(0, "AOT v2: cannot resolve function '%s' for expr slot\n", ref1);
                return 0;
            }
            // Create a FunctionCallNode with no args (args handled by native code)
            FunctionCallNode* fcn = new FunctionCallNode(&loc_builtin, fe, (QoreListNode*)nullptr, pgm);
            return toBitsNB(QoreValue(fcn));
        }

        case AOTExprKind::STATIC_METHOD_CALL: {
            if (!ref1 || !ref2) {
                return 0;
            }
            // Look up class, then find static method
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for static method '%s'\n", ref1, ref2);
                return 0;
            }
            const QoreMethod* m = qc->findStaticMethod(ref2);
            if (!m) {
                printd(0, "AOT v2: cannot find static method '%s::%s'\n", ref1, ref2);
                return 0;
            }
            // Create StaticMethodCallNode
            StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, (QoreParseListNode*)nullptr);
            return toBitsNB(QoreValue(smcn));
        }

        case AOTExprKind::NEW_OBJECT:
        case AOTExprKind::SELF_VARREF:
        case AOTExprKind::RUNTIME_CONST_REF:
        case AOTExprKind::SELF_METHOD_CALL:
            // These need more complex resolution; function needs source fallback
            printd(1, "AOT v2: expression kind %d not yet fully implemented\n", (int)kind);
            return 0;

        case AOTExprKind::GENERIC_EVAL:
        default:
            // Unsupported — function needs source fallback
            return 0;
    }
}

//! Build QoreAOTContext from deserialized slot map identities (no IR re-lowering needed)
/** Resolves local/global/expression slot identities by looking up objects
    in the program's namespace tree and the function's UserSignature.
    @param reader the binary reader
    @param func_data pointer to the function's slot data in the binary
    @param func_data_end pointer past end of valid data
    @param uvb the user variant base (provides UserSignature with LocalVar* for params)
    @param pgm the QoreProgram
    @param aot_func the AOT function descriptor (for slot counts)
    @param name the function name (for debug output)
    @return heap-allocated QoreAOTContext, or nullptr on failure
*/
static QoreAOTContext* buildContextFromSlotMap(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb, QoreProgram* pgm,
        const QoreAOTFunc& aot_func, const char* name) {
    // Read the per-function slot map header
    // Format: name_ref(u32), num_locals(u16), num_globals(u16), num_exprs(u16),
    //         num_stmts(u16), num_body_locals(u16), has_unsupported(u8), padding(u8)
    /*const char* func_name =*/ reader.readStringRef(ptr);
    uint16_t num_locals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_globals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_exprs = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_stmts = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_body_locals = QoreAOTBinaryReader::readU16(ptr);
    uint8_t has_unsupported = QoreAOTBinaryReader::readU8(ptr);
    QoreAOTBinaryReader::readU8(ptr); // padding

    // Validate slot counts match the AOT function descriptor
    if (num_locals != aot_func.num_locals || num_globals != aot_func.num_globals
            || num_exprs != aot_func.num_exprs || num_stmts != aot_func.num_stmts) {
        printd(0, "AOT v2: slot count mismatch for '%s': binary(%d,%d,%d,%d) vs func(%d,%d,%d,%d)\n",
            name, num_locals, num_globals, num_exprs, num_stmts,
            aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts);
        return nullptr;
    }

    printd(2, "AOT v2: buildContextFromSlotMap '%s': locals=%d globals=%d exprs=%d stmts=%d body_locals=%d "
        "has_unsupported=%d uvb=%p\n", name, num_locals, num_globals, num_exprs, num_stmts, num_body_locals,
        has_unsupported, (void*)uvb);

    auto* ctx = new QoreAOTContext();
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->num_stmts = num_stmts;
    ctx->allocate();

    qore_program_private* pp = qore_program_private::get(*pgm);

    // Get UserSignature for param resolution
    UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    printd(2, "AOT v2: '%s' sig=%p sig->lv.size()=%d\n", name,
        (void*)sig, sig ? (int)sig->lv.size() : -1);

    // Read and resolve local slot identities
    for (int i = 0; i < num_locals; ++i) {
        const char* lname = reader.readStringRef(ptr);
        const char* ltype = reader.readStringRef(ptr);
        uint8_t lflags = QoreAOTBinaryReader::readU8(ptr);
        uint16_t param_idx = QoreAOTBinaryReader::readU16(ptr);

        LocalVar* lv = nullptr;
        if (lflags & 0x04) {
            // is_self
            if (sig) {
                lv = sig->selfid;
            }
        } else if (lflags & 0x08) {
            // is_argv
            if (sig) {
                lv = sig->argvid;
            }
        } else if (lflags & 0x01) {
            // is_param
            if (sig && param_idx < sig->lv.size()) {
                lv = sig->lv[param_idx];
            }
        } else {
            // Body local — create a new LocalVar
            // Resolve type
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = nullptr;
            if (ltype && *ltype) {
                ti = type_resolver.resolve(ltype, type_error);
                if (!type_error.empty()) {
                    type_error.clear();
                }
            }
            lv = pp->createLocalVar(lname ? lname : "", ti);
        }

        if (lv) {
            ctx->locals[i] = lv;
            printd(3, "AOT v2: '%s' local[%d] = '%s' (flags=0x%x param_idx=%d) -> %p\n",
                name, i, lname ? lname : "", lflags, param_idx, (void*)lv);
        } else {
            printd(0, "AOT v2: '%s' unresolved local slot %d ('%s' flags=0x%x param_idx=%d)\n",
                name, i, lname ? lname : "", lflags, param_idx);
        }
    }

    // Read and resolve global slot identities
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    for (int i = 0; i < num_globals; ++i) {
        const char* gname = reader.readStringRef(ptr);
        const char* gtype = reader.readStringRef(ptr);
        uint8_t is_tl = QoreAOTBinaryReader::readU8(ptr);

        if (gname && *gname) {
            // Look up global variable by name in the namespace tree
            Var* v = root_ns->var_list.runtimeFindVar(gname);
            if (v) {
                ctx->globals[i] = v;
            } else {
                printd(2, "AOT v2: unresolved global slot %d ('%s') for '%s'\n",
                    i, gname, name);
            }
        }
        (void)gtype;
        (void)is_tl;
    }

    // Read and resolve expression slot identities
    for (int i = 0; i < num_exprs; ++i) {
        uint8_t kind_byte = QoreAOTBinaryReader::readU8(ptr);
        AOTExprKind kind = static_cast<AOTExprKind>(kind_byte);
        const char* ref1 = nullptr;
        const char* ref2 = nullptr;

        switch (kind) {
            case AOTExprKind::FUNC_CALL:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::SELF_METHOD_CALL:
            case AOTExprKind::STATIC_METHOD_CALL:
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::NEW_OBJECT:
            case AOTExprKind::RUNTIME_CONST_REF:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::LOCAL_VARREF:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::SELF_VARREF:
            case AOTExprKind::GENERIC_EVAL:
            default:
                break;
        }

        // Handle LOCAL_VARREF directly since it needs ctx->locals
        if (kind == AOTExprKind::LOCAL_VARREF && ref1) {
            int local_slot = std::atoi(ref1);
            if (local_slot >= 0 && local_slot < ctx->num_locals && ctx->locals[local_slot]) {
                // Create a VarRefNode pointing to the local variable
                VarRefNode* vrn = new VarRefNode(&loc_builtin, strdup(ctx->locals[local_slot]->getName()),
                    ctx->locals[local_slot], false);
                ctx->exprs[i] = toBitsNB(QoreValue(vrn));
                continue;
            } else {
                printd(0, "AOT v2: invalid local slot %d for LOCAL_VARREF expr slot %d (num_locals=%d)\n",
                    local_slot, i, ctx->num_locals);
            }
        }

        uint64_t bits = resolveExprSlot(kind, ref1, ref2, pgm);
        if (bits) {
            ctx->exprs[i] = bits;
        } else if (kind != AOTExprKind::GENERIC_EVAL) {
            printd(2, "AOT v2: unresolved expr slot %d (kind=%d) for '%s'\n",
                i, (int)kind, name);
        }
    }

    // Build a name-based map of locals for body local reuse (toplevel path)
    std::unordered_map<std::string, LocalVar*> local_name_map;
    if (!uvb) {
        for (int i = 0; i < num_locals; ++i) {
            if (ctx->locals[i]) {
                local_name_map[ctx->locals[i]->getName()] = ctx->locals[i];
            }
        }
    }

    // Read body locals
    for (int i = 0; i < num_body_locals; ++i) {
        const char* blname = reader.readStringRef(ptr);
        const char* bltype = reader.readStringRef(ptr);
        uint8_t bl_closure = QoreAOTBinaryReader::readU8(ptr);

        // For toplevel, reuse LocalVar* from ctx->locals[] if same name exists
        if (!uvb && blname) {
            auto it = local_name_map.find(blname);
            if (it != local_name_map.end()) {
                ctx->all_body_locals.push_back(it->second);
                (void)bl_closure;
                continue;
            }
        }

        std::string type_error;
        QoreAOTTypeResolver type_resolver(pgm);
        const QoreTypeInfo* ti = nullptr;
        if (bltype && *bltype) {
            ti = type_resolver.resolve(bltype, type_error);
            if (!type_error.empty()) {
                type_error.clear();
            }
        }

        LocalVar* lv = pp->createLocalVar(blname ? blname : "", ti);
        ctx->all_body_locals.push_back(lv);
        (void)bl_closure;
    }

    printd(2, "AOT v2: built context from slot map for '%s' "
        "(locals=%d, globals=%d, exprs=%d, body_locals=%d, unsupported=%d)\n",
        name, num_locals, num_globals, num_exprs, num_body_locals, has_unsupported);

    // If any expression slots have unsupported types (e.g., closures), skip AOT
    // registration for this function — it will fall through to JIT at runtime
    if (has_unsupported) {
        printd(2, "AOT v2: skipping '%s' due to unsupported expression slots\n", name);
        delete ctx;
        return nullptr;
    }

    return ctx;
}

//! Re-lower a user function variant to IR and build an AOT context.
/** @param uvb the user variant
    @param name the function name
    @param pgm the QoreProgram
    @param aot_func the AOT function descriptor (for slot counts)
    @return heap-allocated context (caller passes ownership to variant), or nullptr on failure
*/
static QoreAOTContext* buildContextForVariant(UserVariantBase* uvb, const char* name,
        QoreProgram* pgm, const QoreAOTFunc& aot_func) {
    StatementBlock* statements = uvb->getStatementBlock();
    if (!statements) {
        printd(1, "AOT: buildContextForVariant '%s': no statement block\n", name);
        return nullptr;
    }

    // Re-lower to IR (fresh AST from re-parsed source → same IR → same walk order)
    QoreIRFunction* ir_func = new QoreIRFunction(name);

    // Record pre-instantiated locals from signature
    UserSignature* sig = uvb->getUserSignature();
    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->lv[i]));
        }
        if (sig->argvid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->argvid));
        }
        if (sig->selfid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->selfid));
        }
    }

    QoreIRBuilder builder(ir_func);
    auto* entry = ir_func->createBlock("entry");
    builder.setBlock(entry);

    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string lower_error;
    if (!lowering.lowerStatementBlock(statements, lower_error)) {
        printd(1, "AOT: buildContextForVariant '%s': IR lowering failed: %s\n", name, lower_error.c_str());
        delete ir_func;
        return nullptr;
    }
    // Ensure terminator
    if (ir_func->blocks.back()->instructions.empty() ||
            (ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Return &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Br &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
        builder.createReturnNothing();
    }

    std::string verify_error;
    if (!QoreIRVerifier::verify(*ir_func, verify_error)) {
        printd(1, "AOT: buildContextForVariant '%s': verification failed: %s\n", name, verify_error.c_str());
        delete ir_func;
        return nullptr;
    }

    // Collect ALL body locals from the statement tree (includes nested blocks from
    // for/while/if/try/switch statements) so they can be instantiated at runtime.
    collectAllStatementLocals(statements, ir_func->all_body_locals);

    // Build the context from the fresh IR (same walk order → same slot indices)
    QoreAOTContext* ctx = buildAOTContext(*ir_func, aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts);
    delete ir_func;

    return ctx;
}

//! Register AOT functions using slot maps from deserialized metadata (V2 — no IR re-lowering)
/** Walks the SLOT_MAPS section, finds matching functions in the namespace tree,
    and builds context from slot identities.
*/
static void registerAOTFunctionsFromSlotMaps(
        const QoreAOTBinaryReader& reader,
        qore_ns_private* root_ns,
        QoreProgram* pgm,
        const std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    if (!sec) {
        printd(0, "AOT v2: no SLOT_MAPS section found\n");
        return;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        printd(0, "AOT v2: invalid SLOT_MAPS section data\n");
        return;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t num_funcs = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t f = 0; f < num_funcs; ++f) {
        // Peek at function name (first field in slot map entry)
        const uint8_t* entry_start = ptr;
        const char* func_name = reader.readStringRef(ptr);
        // Reset to entry start for buildContextFromSlotMap which reads the full entry
        ptr = entry_start;

        if (!func_name || !*func_name) {
            // Skip this entry by reading through it
            printd(2, "AOT v2: skipping unnamed slot map entry\n");
            // Need to skip the entry — read header + all slots
            reader.readStringRef(ptr); // name
            uint16_t nl = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ng = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ne = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU16(ptr); // num_stmts
            uint16_t nbl = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU8(ptr); // has_unsupported
            QoreAOTBinaryReader::readU8(ptr); // padding
            // Skip local entries
            for (int i = 0; i < nl; ++i) {
                reader.readStringRef(ptr);
                reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
                QoreAOTBinaryReader::readU16(ptr);
            }
            // Skip global entries
            for (int i = 0; i < ng; ++i) {
                reader.readStringRef(ptr);
                reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
            // Skip expression entries
            for (int i = 0; i < ne; ++i) {
                uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
                switch (static_cast<AOTExprKind>(kind)) {
                    case AOTExprKind::FUNC_CALL:
                    case AOTExprKind::NEW_OBJECT:
                    case AOTExprKind::RUNTIME_CONST_REF:
                    case AOTExprKind::LOCAL_VARREF:
                        reader.readStringRef(ptr);
                        break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
                        break;
                    case AOTExprKind::SELF_VARREF:
                    default:
                        break;
                }
            }
            // Skip body locals
            for (int i = 0; i < nbl; ++i) {
                reader.readStringRef(ptr);
                reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
            continue;
        }

        // Find matching AOT function
        auto it = func_map.find(func_name);
        if (it == func_map.end()) {
            // No AOT function for this entry — skip it
            // Same skip logic as above
            reader.readStringRef(ptr); // name
            uint16_t nl = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ng = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ne = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU16(ptr); // num_stmts
            uint16_t nbl = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU8(ptr);
            QoreAOTBinaryReader::readU8(ptr);
            for (int i = 0; i < nl; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr); QoreAOTBinaryReader::readU16(ptr);
            }
            for (int i = 0; i < ng; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
            for (int i = 0; i < ne; ++i) {
                uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
                switch (static_cast<AOTExprKind>(kind)) {
                    case AOTExprKind::FUNC_CALL:
                    case AOTExprKind::NEW_OBJECT:
                    case AOTExprKind::RUNTIME_CONST_REF:
                    case AOTExprKind::LOCAL_VARREF:
                        reader.readStringRef(ptr); break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                        reader.readStringRef(ptr); reader.readStringRef(ptr); break;
                    case AOTExprKind::SELF_VARREF:
                    default: break;
                }
            }
            for (int i = 0; i < nbl; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
            continue;
        }

        const QoreAOTFunc* aot_func = it->second;

        // Find the UserVariantBase in the namespace tree
        // Function names can be "funcName(types)" or "ClassName::methodName(types)"
        // We need to extract just the function name for lookup
        UserVariantBase* uvb = nullptr;
        std::string fname_str(func_name);

        // Strip signature suffix if present (e.g., "add(int,int)" -> "add")
        size_t paren = fname_str.find('(');
        if (paren != std::string::npos) {
            fname_str = fname_str.substr(0, paren);
        }

        size_t sep = fname_str.find("::");

        if (sep != std::string::npos) {
            // Method: ClassName::methodName
            std::string class_name = fname_str.substr(0, sep);
            std::string method_name = fname_str.substr(sep + 2);

            qore_program_private* pp = qore_program_private::get(*pgm);
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, class_name.c_str());
            if (qc) {
                // Use parseFindLocalMethod/parseFindLocalStaticMethod instead of
                // findMethod/findStaticMethod — the latter checks committedEmpty()
                // which returns true for deserialized (pending) variants
                qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
                const QoreMethod* m = qcp->parseFindLocalMethod(method_name.c_str());
                if (!m) {
                    m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                }
                if (m) {
                    MethodFunctionBase* mfb = qore_method_private::get(*m)->getFunction();
                    QoreFunctionIterator vi(*mfb);
                    while (vi.next()) {
                        uvb = const_cast<UserVariantBase*>(
                            dynamic_cast<const UserVariantBase*>(vi.getVariant()));
                        if (uvb) {
                            break;
                        }
                    }
                }
            }
        } else if (fname_str != "_toplevel") {
            // Regular function
            for (auto fi = root_ns->func_list.begin(), fe2 = root_ns->func_list.end(); fi != fe2; ++fi) {
                FunctionEntry* fe_entry = fi->second;
                QoreFunction* func = fe_entry->getFunction();
                if (!func) {
                    continue;
                }
                if (fname_str == func->getName()) {
                    QoreFunctionIterator vi(*func);
                    while (vi.next()) {
                        uvb = const_cast<UserVariantBase*>(
                            dynamic_cast<const UserVariantBase*>(vi.getVariant()));
                        if (uvb) {
                            break;
                        }
                    }
                    break;
                }
            }
        }

        // Build context from slot map
        QoreAOTContext* ctx = buildContextFromSlotMap(reader, ptr, end, uvb, pgm, *aot_func, func_name);
        if (ctx && uvb) {
            uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
            ++registered;
            printd(2, "AOT v2: registered '%s' from slot map\n", func_name);
        } else if (ctx) {
            // Toplevel or unresolved — handled separately
            delete ctx;
            printd(2, "AOT v2: built context for '%s' but no variant found\n", func_name);
        } else {
            printd(0, "AOT v2: failed to build slot map context for '%s'\n", func_name);
        }
    }
}

//! Walk a namespace tree and register pre-compiled AOT function pointers with context
/** Matches function names from the AOT function table against user function variants
    in the program's namespace tree. For each match, re-lowers to IR to build a
    QoreAOTContext, then registers via registerPrecompiledAOTFunction().
*/
static void registerAOTFunctionsInNamespace(qore_ns_private* ns, QoreProgram* pgm,
        const std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered) {
    // Walk functions in this namespace
    for (auto i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        // Check all variants
        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!uvb || !uvb->hasBody()) {
                continue;
            }

            const char* fname = func->getName();
            // Generate unique key including parameter types to match compiled variant
            std::string variant_key = getVariantKey(fname, variant);
            auto it = func_map.find(variant_key);
            if (it != func_map.end()) {
                const QoreAOTFunc* aot_func = it->second;
                QoreAOTContext* ctx = buildContextForVariant(uvb, fname, pgm, *aot_func);
                if (ctx) {
                    uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled function '%s' (locals=%d, globals=%d, exprs=%d)\n",
                        variant_key.c_str(), aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                } else {
                    printd(1, "AOT: failed to build context for function '%s'\n", variant_key.c_str());
                }
            }
        }
    }

    // Walk classes in this namespace
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);
        const char* class_name = qc->getName();

        // Skip system classes - they can't be modified and their methods are already set up
        if (qcp->sys) {
            printd(2, "AOT: skipping system class '%s' in method registration\n", class_name);
            continue;
        }

        // Helper lambda for method registration
        auto processMethod = [&](QoreMethod* meth) {
            if (!meth->isUser()) {
                return;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!uvb || !uvb->hasBody()) {
                    continue;
                }

                std::string method_name = std::string(class_name) + "::" + meth->getName();
                // Generate unique key including parameter types to match compiled variant
                std::string variant_key = getVariantKey(method_name.c_str(), variant);
                auto it = func_map.find(variant_key);
                if (it != func_map.end()) {
                    const QoreAOTFunc* aot_func = it->second;
                    QoreAOTContext* ctx = buildContextForVariant(uvb, method_name.c_str(), pgm, *aot_func);
                    if (ctx) {
                        uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                        ++registered;
                        printd(2, "AOT: registered pre-compiled method '%s' (locals=%d, globals=%d, exprs=%d)\n",
                            variant_key.c_str(), aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                    } else {
                        printd(1, "AOT: failed to build context for method '%s'\n", variant_key.c_str());
                    }
                }
            }
        };

        // Instance methods
        for (auto mi = qcp->hm.begin(), me = qcp->hm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
        // Static methods
        for (auto mi = qcp->shm.begin(), me = qcp->shm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
    }

    // Walk child namespaces recursively
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            registerAOTFunctionsInNamespace(qore_ns_private::get(*child_ns), pgm, func_map, registered);
        }
    }
}

extern "C" DLLEXPORT int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
) {
    // Check for -b flag (disable signal handling, useful for valgrind)
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
            break;
        }
    }

    // Set up ARGV from command-line arguments before qore_init (matches normal qore binary order)
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime with MIT license
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    {
        ExceptionSink xsink;
        ExceptionSink wsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            qore_cleanup();
            return 2;
        }

        // Set JIT execution mode so functions without pre-compiled code will JIT on demand
        qpgm->setExecMode(QEM_JIT);

        // Set script path from the label so %requires with relative paths resolve
        // correctly (label is the absolute path to the original source file)
        if (label) {
            qpgm->setScriptPath(label);
        }

        // Parse the embedded source
        QoreString src_str(source, source_len);
        qpgm->parse(src_str.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);

        // Display any warnings
        if (wsink.isException()) {
            wsink.handleWarnings();
        }

        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
        } else {
            // Register pre-compiled function pointers with AOT context tables
            if (num_functions > 0 && functions) {
                // Build lookup map: name → QoreAOTFunc*
                std::unordered_map<std::string, const QoreAOTFunc*> func_map;
                const QoreAOTFunc* toplevel_func = nullptr;
                for (int i = 0; i < num_functions; ++i) {
                    if (functions[i].name && functions[i].fn_ptr) {
                        if (strcmp(functions[i].name, "_toplevel") == 0) {
                            toplevel_func = &functions[i];
                        } else {
                            func_map[functions[i].name] = &functions[i];
                        }
                    }
                }

                // Walk namespace tree and register with context building
                qore_program_private* pp = qore_program_private::get(**qpgm);
                int registered = 0;
                qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
                registerAOTFunctionsInNamespace(root_ns, *qpgm, func_map, registered);

                // Register the _toplevel function with context
                if (toplevel_func) {
                    TopLevelStatementBlock& sb = pp->sb;

                    // Re-lower top-level code to IR to build context
                    QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

                    // Collect ALL body locals (top-level + nested blocks) as pre-instantiated
                    collectAllStatementLocals(&sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }

                    QoreIRBuilder builder(ir_func);
                    auto* entry = ir_func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(*qpgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string lower_error;
                    bool ctx_ok = false;
                    if (lowering.lowerStatementBlock(&sb, lower_error)) {
                        // Ensure terminator
                        auto& last_block = ir_func->blocks.back();
                        if (last_block->instructions.empty() ||
                                (last_block->instructions.back()->opcode != QoreIROpcode::Return &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Br &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
                            builder.createReturnNothing();
                        }
                        std::string verify_error;
                        if (QoreIRVerifier::verify(*ir_func, verify_error)) {
                            QoreAOTContext* ctx = buildAOTContext(*ir_func,
                                toplevel_func->num_locals, toplevel_func->num_globals,
                                toplevel_func->num_exprs, toplevel_func->num_stmts);
                            if (ctx) {
                                sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                ++registered;
                                ctx_ok = true;
                                printd(2, "AOT: registered pre-compiled _toplevel with context "
                                    "(locals=%d, globals=%d, exprs=%d, stmts=%d)\n",
                                    toplevel_func->num_locals, toplevel_func->num_globals,
                                    toplevel_func->num_exprs, toplevel_func->num_stmts);
                            }
                        } else {
                            printd(1, "AOT: _toplevel re-verification failed: %s\n", verify_error.c_str());
                        }
                    } else {
                        printd(1, "AOT: _toplevel re-lowering failed: %s\n", lower_error.c_str());
                    }
                    delete ir_func;

                    if (!ctx_ok) {
                        printd(1, "AOT: failed to build context for _toplevel\n");
                    }
                }

                printd(1, "AOT: registered %d/%d pre-compiled functions\n", registered, num_functions);
            }

            // Run the program
            QoreValue rv = qpgm->run(&xsink);
            rc = rv.getAsBigInt();
            rv.discard(&xsink);

            if (xsink.isException()) {
                rc = 3;
            }
        }

        xsink.handleExceptions();
    }

    qore_cleanup();
    return rc;
}

// ---- Source-Stripped AOT Runtime (V2) ----

extern "C" DLLEXPORT int qore_aot_run_v2(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
) {
    // Check for -b flag (disable signal handling, useful for valgrind)
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
            break;
        }
    }

    // Set up ARGV from command-line arguments
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    {
        ExceptionSink xsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            qore_cleanup();
            return 2;
        }

        // Set JIT execution mode
        qpgm->setExecMode(QEM_JIT);

        printd(2, "AOT v2: parse_options=0x%llx, PO_MODERN=0x%llx, has_modern=%d\n",
            (long long)parse_options, (long long)PO_MODERN,
            (int)((parse_options & PO_MODERN) == PO_MODERN));

        // Load module dependencies before deserialization so that module classes,
        // functions, etc. are available when resolving base classes and types
        {
            std::vector<std::string> deps;
            std::string dep_error;
            if (readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
                printd(2, "AOT v2: loading %d dependencies\n", (int)deps.size());
                for (const std::string& dep : deps) {
                    printd(2, "AOT v2: loading dependency '%s'\n", dep.c_str());
                    int dep_rc = MM.runTimeLoadModule(&xsink, dep.c_str(), *qpgm);
                    if (dep_rc < 0 || xsink.isException()) {
                        printd(2, "AOT v2: dependency '%s' load error (rc=%d)\n",
                            dep.c_str(), dep_rc);
                        xsink.clear();
                    }
                }
            }
        }

        // Deserialize namespace tree from metadata (replaces source parsing)
        // Must set the parse context so UserVariantBase constructor can
        // call parse_get_parse_options() which reads thread-local current_pgm
        QoreAOTBinaryDeserializer deserializer;
        std::string deser_error;
        {
            ProgramRuntimeParseContextHelper pch(&xsink, *qpgm);
            if (xsink.isException()) {
                xsink.handleExceptions();
                qore_cleanup();
                return 2;
            }
            if (!deserializer.deserializeIntoProgram(*qpgm,
                    metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
                fprintf(stderr, "AOT: metadata deserialization failed: %s\n", deser_error.c_str());
                qore_cleanup();
                return 2;
            }
        }

        // Register pre-compiled function pointers
        QoreProgram* fallback_pgm = nullptr;
        if (num_functions > 0 && functions) {
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            const QoreAOTFunc* toplevel_func = nullptr;
            for (int i = 0; i < num_functions; ++i) {
                if (functions[i].name && functions[i].fn_ptr) {
                    if (strcmp(functions[i].name, "_toplevel") == 0) {
                        toplevel_func = &functions[i];
                    } else {
                        func_map[functions[i].name] = &functions[i];
                    }
                }
            }

            // Register non-toplevel functions using slot maps (no IR re-lowering)
            qore_program_private* pp = qore_program_private::get(**qpgm);
            int registered = 0;
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

            // Use slot maps from the binary metadata to build contexts
            printd(2, "AOT v2: calling registerAOTFunctionsFromSlotMaps with %d func_map entries, "
                "toplevel=%p\n", (int)func_map.size(), (void*)toplevel_func);
            registerAOTFunctionsFromSlotMaps(
                deserializer.getReader(), root_ns, *qpgm, func_map, registered);
            printd(2, "AOT v2: after slot map registration: %d registered\n", registered);

            // Only parse fallback source if needed and available.
            // Module functions that weren't compiled are available from runtime module loading,
            // so fallback source is only needed for test-local functions that failed compilation.
            // Make fallback parsing failure non-fatal since module functions are still available.
            if (deserializer.hasFallbackSource()) {
                ExceptionSink wsink;
                // Strip PO_NO_TOP_LEVEL_STATEMENTS from fallback parse options because the
                // full source may have top-level statements before %exec-class directive;
                // the original parser processes directives sequentially but AOT bakes all
                // parse options (including %exec-class) upfront
                fallback_pgm = new QoreProgram(parse_options & ~PO_NO_TOP_LEVEL_STATEMENTS);
                // Set script path so %requires with relative paths resolve correctly
                fallback_pgm->setScriptPath(label);
                fallback_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
                    QP_WARN_DEFAULT);
                if (wsink.isException()) {
                    wsink.handleWarnings();
                }
                if (xsink.isException()) {
                    // Fallback source parsing failed (e.g., module class conflicts).
                    // This is non-fatal: module functions are available from runtime module
                    // loading, only test-local functions that failed compilation are lost.
                    printd(2, "AOT v2: fallback source parse failed (non-fatal), "
                        "module functions available from runtime loading\n");
                    xsink.clear();
                    fallback_pgm->waitForTerminationAndDeref(nullptr);
                    fallback_pgm = nullptr;
                } else {
                    int fallback_registered = 0;
                    registerAOTFunctionsInNamespace(
                        qore_ns_private::get(*qore_program_private::get(*fallback_pgm)->RootNS),
                        fallback_pgm, func_map, fallback_registered);
                    registered += fallback_registered;
                }
            }

            // Register the _toplevel function from slot maps
            if (toplevel_func) {
                // Find _toplevel in SLOT_MAPS section
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);
                bool toplevel_registered = false;

                if (sm_sec) {
                    const uint8_t* sm_ptr = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_ptr) {
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);
                            sm_ptr = entry_start;  // reset for buildContextFromSlotMap

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel");
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v2: registered _toplevel from slot map\n");
                                }
                                break;
                            } else {
                                // Skip this entry
                                deserializer.getReader().readStringRef(sm_ptr);
                                uint16_t nl = QoreAOTBinaryReader::readU16(sm_ptr);
                                uint16_t ng = QoreAOTBinaryReader::readU16(sm_ptr);
                                uint16_t ne = QoreAOTBinaryReader::readU16(sm_ptr);
                                QoreAOTBinaryReader::readU16(sm_ptr); // num_stmts
                                uint16_t nbl = QoreAOTBinaryReader::readU16(sm_ptr);
                                QoreAOTBinaryReader::readU8(sm_ptr);
                                QoreAOTBinaryReader::readU8(sm_ptr);
                                for (int i = 0; i < nl; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                    QoreAOTBinaryReader::readU16(sm_ptr);
                                }
                                for (int i = 0; i < ng; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                }
                                for (int i = 0; i < ne; ++i) {
                                    uint8_t kind = QoreAOTBinaryReader::readU8(sm_ptr);
                                    switch (static_cast<AOTExprKind>(kind)) {
                                        case AOTExprKind::FUNC_CALL:
                                        case AOTExprKind::NEW_OBJECT:
                                        case AOTExprKind::RUNTIME_CONST_REF:
                                        case AOTExprKind::LOCAL_VARREF:
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
                                        case AOTExprKind::SELF_METHOD_CALL:
                                        case AOTExprKind::STATIC_METHOD_CALL:
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
                                        case AOTExprKind::SELF_VARREF:
                                        default: break;
                                    }
                                }
                                for (int i = 0; i < nbl; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                }
                            }
                        }
                    }
                }

                if (!toplevel_registered && fallback_pgm) {
                    // Fall back to IR re-lowering path for toplevel using fallback source
                    qore_program_private* fb_pp = qore_program_private::get(*fallback_pgm);
                    TopLevelStatementBlock& fb_sb = fb_pp->sb;

                    QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

                    // Collect ALL body locals (top-level + nested blocks) as pre-instantiated
                    collectAllStatementLocals(&fb_sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }

                    QoreIRBuilder builder(ir_func);
                    auto* entry = ir_func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(fallback_pgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string lower_error;
                    bool ctx_ok = false;
                    if (lowering.lowerStatementBlock(&fb_sb, lower_error)) {
                        auto& last_block = ir_func->blocks.back();
                        if (last_block->instructions.empty() ||
                                (last_block->instructions.back()->opcode != QoreIROpcode::Return &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Br &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
                            builder.createReturnNothing();
                        }
                        std::string verify_error;
                        if (QoreIRVerifier::verify(*ir_func, verify_error)) {
                            QoreAOTContext* ctx = buildAOTContext(*ir_func,
                                toplevel_func->num_locals, toplevel_func->num_globals,
                                toplevel_func->num_exprs, toplevel_func->num_stmts);
                            if (ctx) {
                                pp->sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                pp->sb.setLVarsFromAOTContext(ctx);
                                ++registered;
                                ctx_ok = true;
                                printd(2, "AOT v2: registered _toplevel via fallback IR\n");
                            }
                        } else {
                            printd(0, "AOT v2: _toplevel re-verification failed: %s\n",
                                verify_error.c_str());
                        }
                    } else {
                        printd(0, "AOT v2: _toplevel re-lowering failed: %s\n", lower_error.c_str());
                    }
                    delete ir_func;

                    if (!ctx_ok) {
                        printd(0, "AOT v2: failed to build context for _toplevel\n");
                    }
                } else if (!toplevel_registered) {
                    printd(0, "AOT v2: _toplevel not registered (no slot map or fallback)\n");
                }

            }

            printd(2, "AOT v2: registered %d/%d pre-compiled functions\n", registered, num_functions);
        }

        // NOTE: Do NOT clean up fallback_pgm yet - it may contain StatementBlock* pointers
        // that are referenced by AOT contexts for on_exit/on_success/on_error blocks.
        // We must keep it alive until after the program finishes running.

        // Run the program
        QoreValue rv = qpgm->run(&xsink);
        rc = rv.getAsBigInt();
        rv.discard(&xsink);

        if (xsink.isException()) {
            rc = 3;
        }

        xsink.handleExceptions();

        // Clean up fallback program AFTER program execution - it contains StatementBlock*
        // pointers used by AOT contexts for on_exit/on_success/on_error blocks
        if (fallback_pgm) {
            fallback_pgm->waitForTerminationAndDeref(nullptr);
            fallback_pgm = nullptr;
        }
    }

    qore_cleanup();
    return rc;
}

// ---- AOT Module Runtime Functions ----

//! Per-module state for AOT-compiled modules
struct AotModuleState {
    QoreProgram* pgm = nullptr;
    const QoreAOTFunc* funcs = nullptr;
    int num_funcs = 0;
    //! Modules that should be reexported (from %requires(reexport) directives)
    std::vector<std::string> reexport_deps;
};

//! Map from module name to per-module state
/** Multiple AOT modules can be loaded simultaneously.  Each module's init creates a
    QoreProgram and stores it here keyed by module name.  The ns_init function uses
    get_module_context()->getName() to find the correct program for the module being
    imported.

    Thread safety: Access is serialized by QoreModuleManager's module loading mutex.
    All accesses (qore_aot_module_init, qore_aot_module_ns_init, qore_aot_module_delete)
    occur under the module manager lock, so no additional synchronization is needed.
*/
static std::unordered_map<std::string, AotModuleState> aot_module_map;

//! Current module being initialized (valid only during qore_aot_module_init / _init_v2)
static QoreProgram* aot_module_pgm = nullptr;
static std::string aot_module_name;
static const QoreAOTFunc* aot_module_funcs = nullptr;
static int aot_module_num_funcs = 0;

//! Extract dependency module names from source \%requires directives
/** Parses the source to find all \%requires directives and extracts the module names.
    Skips "qore" since it's always available.
    NOTE: Properly skips \%requires inside block comments and line comments.
    \param source the source text
    \param source_len length of source
    \param reexport_deps if non-null, receives names of deps with (reexport) flag
    \return vector of dependency module names
*/
static std::vector<std::string> extractDependencies(const char* source, int source_len,
        std::vector<std::string>* reexport_deps = nullptr) {
    std::vector<std::string> deps;
    const char* p = source;
    const char* end = source + source_len;
    bool in_block_comment = false;

    while (p < end) {
        // Check for block comment start/end
        if (!in_block_comment && p + 1 < end && p[0] == '/' && p[1] == '*') {
            in_block_comment = true;
            p += 2;
            continue;
        }
        if (in_block_comment && p + 1 < end && p[0] == '*' && p[1] == '/') {
            in_block_comment = false;
            p += 2;
            continue;
        }
        if (in_block_comment) {
            ++p;
            continue;
        }

        // Skip leading whitespace
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }

        // Skip line comments (# ...)
        if (p < end && *p == '#') {
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;  // skip newline
            }
            continue;
        }

        // Check for %requires directive
        if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
            p += 9;
            // Skip whitespace after %requires
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }
            // Check for optional (reexport) flag
            bool is_reexport = false;
            if (p + 10 <= end && strncmp(p, "(reexport)", 10) == 0) {
                is_reexport = true;
                p += 10;
                while (p < end && (*p == ' ' || *p == '\t')) {
                    ++p;
                }
            }
            // Extract module name (until whitespace, newline, or version operator)
            const char* name_start = p;
            while (p < end && *p != '\n' && *p != ' ' && *p != '\t' &&
                   *p != '<' && *p != '>' && *p != '=') {
                ++p;
            }
            if (p > name_start) {
                std::string dep_name(name_start, p - name_start);
                // Skip "qore" as it's always available
                if (dep_name != "qore") {
                    deps.push_back(dep_name);
                    if (is_reexport && reexport_deps) {
                        reexport_deps->push_back(dep_name);
                    }
                }
            }
        }

        // Skip to end of line
        while (p < end && *p != '\n') {
            // Also check for block comment start within the line
            if (p + 1 < end && p[0] == '/' && p[1] == '*') {
                in_block_comment = true;
                p += 2;
                break;
            }
            ++p;
        }
        if (p < end && *p == '\n') {
            ++p;  // skip newline
        }
    }

    return deps;
}

//! Strip %requires directives from source code
/** AOT modules have already resolved their dependencies at compile time, so we must
    not process %requires directives when parsing the embedded source at runtime.
    Processing %requires would cause a deadlock because module loading holds the
    module manager lock, and parsing %requires tries to acquire the same lock.

    For %try-module blocks, we need special handling:
    - Strip the %try-module and %endtry directives themselves
    - Try to load the module at runtime
    - If the module loads successfully, strip the fallback code inside the block
    - If the module fails to load, KEEP the fallback code (typically %define directives)
      so that conditional compilation works correctly
*/
static std::string stripRequiresDirectives(const char* source, int source_len,
        QoreProgram* target_pgm = nullptr) {
    std::string result;
    result.reserve(source_len);

    const char* p = source;
    const char* end = source + source_len;

    // Track try-module state: depth and whether current block's module loaded
    struct TryModuleState {
        bool module_loaded;
    };
    std::vector<TryModuleState> try_module_stack;

    while (p < end) {
        // Skip leading whitespace
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }

        // Check for %requires directive
        if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
            // Skip the entire line (including the newline)
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;  // skip the newline
            }
            // Replace with a comment to preserve line numbers
            result += "# [AOT: %requires stripped]\n";
        } else if (p + 11 <= end && strncmp(p, "%try-module", 11) == 0) {
            // Start of %try-module block - extract module name and try to load it
            p += 11;
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }
            const char* mod_start = p;
            while (p < end && *p != '\n' && *p != ' ' && *p != '\t') {
                ++p;
            }
            std::string mod_name(mod_start, p - mod_start);

            // Try to load the module (it may already be loaded)
            bool loaded = false;
            if (!mod_name.empty()) {
                ExceptionSink xsink;
                QoreProgram* load_target = target_pgm ? target_pgm : aot_module_pgm;
                int rc = MM.runTimeLoadModule(&xsink, mod_name.c_str(), load_target);
                loaded = (rc >= 0 && !xsink);
                xsink.clear();
            }

            try_module_stack.push_back({loaded});

            // Skip to end of line
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: %try-module ";
            result += mod_name;
            result += loaded ? " loaded]\n" : " not loaded - keeping fallback]\n";
        } else if (p + 7 <= end && strncmp(p, "%endtry", 7) == 0) {
            // End of %try-module block
            if (!try_module_stack.empty()) {
                try_module_stack.pop_back();
            }
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: %endtry]\n";
        } else if (!try_module_stack.empty() && try_module_stack.back().module_loaded) {
            // Inside a %try-module block where the module loaded - strip this line
            // (the fallback code is not needed)
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: try-module fallback stripped]\n";
        } else {
            // Either outside try-module, or inside a block where module didn't load
            // Copy the line as-is (keeping %define and other fallback directives)
            while (p < end && *p != '\n') {
                result += *p++;
            }
            if (p < end) {
                result += *p++;  // copy the newline
            }
        }
    }

    return result;
}

extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init(
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;
    ExceptionSink wsink;

    // Use a local variable for the program being created.  Loading dependencies
    // via runTimeLoadModule() can trigger nested calls to qore_aot_module_init()
    // for other AOT modules, which would overwrite the global aot_module_pgm.
    QoreProgram* local_pgm = new QoreProgram(parse_options);

    // Set JIT execution mode so functions without pre-compiled code will JIT on demand
    local_pgm->setExecMode(QEM_JIT);

    // Set script path from the label so get_script_dir() returns the module's source
    // directory during parsing.  This is needed for modules that read files at parse time
    // (e.g., DataProvider loads qore-q-logo.svg via get_script_dir() + filename).
    if (label) {
        local_pgm->setScriptPath(label);
    }

    // Extract dependencies from source and load/import their namespaces
    // Note: The init function is now called with the module manager lock unlocked
    // (via ModuleLoadMapHelper), so we can safely load dependencies here.
    std::vector<std::string> reexport_deps;
    std::vector<std::string> deps = extractDependencies(source, source_len, &reexport_deps);
    for (const std::string& dep : deps) {
        // Try to load the module (it may already be loaded, which is fine)
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            // Circular dependency or other issue - clear error and continue
            // The types might be resolved later when the requiring script is parsed
            xsink.clear();
        }
    }

    // Set up module context for the parser (must be QoreUserModuleDefContextHelper
    // because the parser static_casts to it)
    // Note: Do NOT call setNameInit() here - the scanner calls it when it parses
    // the "module Name" declaration, and calling it twice triggers an assertion.
    QoreUserModuleDefContextHelper mod_ctx(mod_name, label, local_pgm, xsink);
    {
        // Strip %requires directives from embedded source to avoid deadlock.
        // The module manager holds a lock when calling this init function, and
        // parsing %requires would try to acquire the same lock.
        // Note: We've already imported the dependency namespaces above.
        std::string stripped_src = stripRequiresDirectives(source, source_len, local_pgm);

        printd(2, "AOT module '%s': source_len=%d stripped_len=%d deps=%d\n",
            mod_name, source_len, (int)stripped_src.size(), (int)deps.size());

        // Split combined source at file boundary markers and parse each segment
        // separately with parsePending().  Split modules (directory-based) embed
        // "# __AOT_FILE_BREAK__\n" between the main .qm source and each .qc file.
        // Each parsePending() call creates a fresh scanner with line numbers starting
        // from 1, which is critical for correct BCANode location tracking.
        const char* marker = "# __AOT_FILE_BREAK__\n";
        const size_t marker_len = strlen(marker);
        std::vector<std::string> segments;
        {
            size_t pos = 0;
            size_t found;
            while ((found = stripped_src.find(marker, pos)) != std::string::npos) {
                segments.push_back(stripped_src.substr(pos, found - pos));
                pos = found + marker_len;
            }
            // Last segment (or entire source if no markers found)
            segments.push_back(stripped_src.substr(pos));
        }

        if (segments.size() > 1) {
            // Split module: parse each segment separately
            printd(2, "AOT module '%s': %d source segments\n", mod_name, (int)segments.size());
            for (size_t i = 0; i < segments.size(); ++i) {
                if (segments[i].empty()) {
                    continue;
                }
                local_pgm->parsePending(segments[i].c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);
                if (xsink.isException()) {
                    break;
                }
            }
            if (!xsink.isException()) {
                local_pgm->parseCommit(&xsink, &wsink, QP_WARN_DEFAULT);
            }
        } else {
            // Single-file module: parse as one blob
            local_pgm->parse(stripped_src.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);
        }

        mod_ctx.close();
    }

    if (wsink.isException()) {
        wsink.handleWarnings();
    }

    if (xsink.isException()) {
        QoreStringNode* err = new QoreStringNode("AOT module parse error: ");
        // Get error description
        QoreValue ex_err = xsink.getExceptionErr();
        QoreValue ex_desc = xsink.getExceptionDesc();
        QoreValue ex_arg = xsink.getExceptionArg();
        if (ex_err.getType() == NT_STRING) {
            err->concat(ex_err.get<const QoreStringNode>()->c_str());
        } else {
            err->concat("unknown parse error");
        }
        if (ex_desc.getType() == NT_STRING) {
            err->concat(", desc: ");
            err->concat(ex_desc.get<const QoreStringNode>()->c_str());
        }
        if (ex_arg.getType() == NT_STRING) {
            err->concat(", arg: ");
            err->concat(ex_arg.get<const QoreStringNode>()->c_str());
        }
        xsink.clear();
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Run module init closure if present (registers factories, etc.)
    if (mod_ctx.hasInit()) {
        if (mod_ctx.init(*local_pgm, xsink)) {
            QoreStringNode* err = new QoreStringNode("AOT module init closure error");
            if (xsink.isException()) {
                QoreValue ex_desc = xsink.getExceptionDesc();
                if (ex_desc.getType() == NT_STRING) {
                    err->concat(": ");
                    err->concat(ex_desc.get<const QoreStringNode>()->c_str());
                }
                xsink.clear();
            }
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Register pre-compiled AOT functions on the module's namespace tree
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*local_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        registerAOTFunctionsInNamespace(root_ns, local_pgm, func_map, registered);

        printd(1, "AOT module '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update the global for the fallback path in ns_init (for modules
    // that call ns_init during their own init before being stored in the map).
    aot_module_pgm = local_pgm;
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;
    aot_module_map[mod_name] = {local_pgm, functions, num_functions, std::move(reexport_deps)};

    return nullptr;  // success
}

extern "C" DLLEXPORT void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns) {
    // Look up the correct module program from the per-module map.
    // When QoreBuiltinModule::addToProgramImpl calls module_ns_init, it has set up
    // QoreModuleContextHelper with the module's name. We use get_module_context() to
    // find which module's namespace to merge.
    const char* mod_name = nullptr;
    QoreProgram* mod_pgm = nullptr;
    const std::vector<std::string>* reexport_deps = nullptr;

    QoreModuleContext* ctx = get_module_context();
    if (ctx) {
        mod_name = ctx->getName();
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end()) {
            mod_pgm = it->second.pgm;
            reexport_deps = &it->second.reexport_deps;
        }
    }

    // Fall back to the current module being initialized (for the module's own registration)
    if (!mod_pgm) {
        mod_pgm = aot_module_pgm;
        if (!mod_name) {
            mod_name = aot_module_name.c_str();
        }
    }

    if (!mod_pgm) {
        printd(5, "AOT module ns_init: no program for '%s'!\n", mod_name ? mod_name : "(unknown)");
        return;
    }

    // Get the module program's root namespace
    RootQoreNamespace* mod_root = mod_pgm->getRootNS();
    if (!mod_root) {
        printd(5, "AOT module ns_init '%s': no root namespace!\n", mod_name ? mod_name : "(unknown)");
        return;
    }

    // Use the same namespace merge mechanism as user modules to properly handle
    // class hierarchy references. Simple copy() doesn't work because base class
    // pointers would still reference classes from the AOT module's program.
    ExceptionSink xsink;
    RootQoreNamespace* target_root = static_cast<RootQoreNamespace*>(root_ns);

    // Set up the target program as the current program context.
    // scanMergeCommittedNamespace() calls parse_check_parse_option() which requires
    // current_pgm to be set. QoreBuiltinModule::addToProgramImpl() only sets
    // call_program_context (via ProgramCallContextHelper), not current_pgm.
    // Use the same approach as QoreUserModule::addToProgramImpl().
    QoreProgram* tpgm = getProgram();
    ProgramThreadCountContextHelper ptcch(&xsink, tpgm, false);
    if (xsink) {
        return;
    }

    // Load reexported dependencies into the target program BEFORE namespace merge.
    // When a source module has %requires(reexport) logger_bin, the reexport() mechanism
    // loads logger_bin directly into importing programs. For AOT binary modules, we must
    // replicate this behavior because scanMergeCommittedNamespace only copies user-public
    // items — system classes from binary modules (e.g., LoggerLevel from logger_bin)
    // are NOT user-public and would be skipped by the namespace merge.
    if (reexport_deps && !reexport_deps->empty()) {
        for (const std::string& dep : *reexport_deps) {
            if (!qore_program_private::get(*tpgm)->hasFeature(dep.c_str())) {
                printd(5, "AOT module ns_init '%s': loading reexported dep '%s' into target program\n",
                    mod_name, dep.c_str());
                // Use QMM.loadModuleForReexport() (like QoreAbstractModule::reexport()
                // uses QMM.loadModuleIntern()) instead of MM.runTimeLoadModule() which
                // would try to acquire the module manager mutex that is already held by
                // parseLoadModule/runTimeLoadModule
                QMM.loadModuleForReexport(xsink, dep.c_str(), tpgm);
                if (xsink) {
                    printd(0, "AOT module ns_init '%s': WARNING - failed to load reexported dep '%s'\n",
                        mod_name, dep.c_str());
                    xsink.clear();
                }
            }
        }
    }

    printd(5, "AOT module ns_init '%s': starting merge\n", mod_name);

    QoreModuleContext qmc(mod_name, qore_root_ns_private::get(*target_root), xsink);
    printd(5, "AOT module ns_init '%s': calling scanMergeCommittedNamespace\n", mod_name);
    qore_root_ns_private::scanMergeCommittedNamespace(*target_root, *mod_root, qmc);
    printd(5, "AOT module ns_init '%s': scanMergeCommittedNamespace done\n", mod_name);

    if (qmc.hasError()) {
        printd(5, "AOT module ns_init '%s': error during namespace scan/merge\n", mod_name);
        qmc.rollback();
        return;
    }

    printd(5, "AOT module ns_init '%s': calling copyMergeCommittedNamespace\n", mod_name);
    qore_root_ns_private::copyMergeCommittedNamespace(*target_root, *mod_root);
    printd(5, "AOT module ns_init '%s': copyMergeCommittedNamespace done\n", mod_name);

    // Rebuild indexes so the merged items can be found during name resolution
    // This is needed because copyMergeCommittedNamespace adds items directly without
    // going through the module commit mechanism that normally rebuilds indexes
    printd(5, "AOT module ns_init '%s': calling rebuildAllIndexes\n", mod_name);
    qore_root_ns_private::get(*target_root)->rebuildAllIndexes();

    // Check for exceptions during merge operations
    if (xsink) {
        const QoreValue err = xsink.getExceptionErr();
        const char* err_str = (err.getType() == NT_STRING) ? err.get<const QoreStringNode>()->c_str() : "(unknown)";
        printd(0, "AOT module ns_init '%s': WARNING - exception during namespace merge: %s\n",
            mod_name, err_str);
    }

    printd(5, "AOT module ns_init '%s': merge complete\n", mod_name);
}

extern "C" DLLEXPORT void qore_aot_module_delete() {
    // Use the module context name to clean up only the module being unloaded
    // (set by QoreBuiltinModule::~QoreBuiltinModule via QoreModuleNameContextHelper)
    const char* mod_name = get_module_context_name();
    if (mod_name) {
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end()) {
            if (it->second.pgm) {
                it->second.pgm->waitForTerminationAndDeref(nullptr);
            }
            aot_module_map.erase(it);
        }
        // Clear per-module state if it matches the module being deleted
        if (aot_module_name == mod_name) {
            aot_module_pgm = nullptr;
            aot_module_name.clear();
            aot_module_funcs = nullptr;
            aot_module_num_funcs = 0;
        }
    } else {
        // Fallback: no module context — clean up all (shutdown path)
        for (auto& entry : aot_module_map) {
            if (entry.second.pgm) {
                entry.second.pgm->waitForTerminationAndDeref(nullptr);
            }
        }
        aot_module_map.clear();
        aot_module_pgm = nullptr;
        aot_module_name.clear();
        aot_module_funcs = nullptr;
        aot_module_num_funcs = 0;
    }
}

extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init_v2(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;

    // Use a local variable for the program being created (see qore_aot_module_init
    // for explanation of nested init overwrite issue)
    QoreProgram* local_pgm = new QoreProgram(parse_options);

    // Set JIT execution mode
    local_pgm->setExecMode(QEM_JIT);

    // Load dependencies from serialized metadata BEFORE deserializing namespace tree.
    // Dependencies must be loaded first because deserialization may need to resolve
    // base classes, types, and other references from dependency modules.
    std::vector<std::string> deps;
    std::string dep_error;
    if (!readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module dependency read error: ");
        err->concat(dep_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Read reexported module names from metadata
    std::vector<std::string> reexport_deps;
    std::string reexport_error;
    if (!readReexportModules(metadata, static_cast<uint32_t>(metadata_len), reexport_deps, reexport_error)) {
        printd(0, "AOT module v2 '%s': WARNING - failed to read reexport modules: %s\n",
            mod_name, reexport_error.c_str());
        // Non-fatal — continue without reexport info
    }

    // Load each dependency module into this program.
    // runTimeLoadModule will call addToProgram which imports the namespace.
    for (const std::string& dep : deps) {
        printd(5, "AOT module v2 '%s': loading dependency '%s'\n", mod_name, dep.c_str());
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            // Circular dependency or other issue - clear error and continue
            // The types might be resolved later when the requiring script is parsed
            printd(5, "AOT module v2 '%s': dependency '%s' load error (rc=%d)\n", mod_name, dep.c_str(), rc);
            xsink.clear();
        }
    }

    printd(5, "AOT module v2 '%s': loaded %d dependencies\n", mod_name, (int)deps.size());

    // Deserialize namespace tree from metadata (replaces source parsing).
    // The metadata contains complete class/namespace structure.  Functions that were
    // successfully compiled to native code will be registered below.  Functions that
    // couldn't be compiled are present as stubs in the metadata.
    // Note: Modules should be compiled with source-first QORE_MODULE_DIR so that all
    // dependency classes/methods are complete during metadata serialization.
    printd(5, "AOT module v2 '%s': starting namespace deserialization\n", mod_name);
    QoreAOTBinaryDeserializer deserializer;
    std::string deser_error;
    if (!deserializer.deserializeIntoProgram(local_pgm,
            metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
        err->concat(deser_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Register pre-compiled AOT functions
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*local_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        registerAOTFunctionsInNamespace(root_ns, local_pgm, func_map, registered);

        printd(1, "AOT module v2 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update the global for the fallback path in ns_init.
    aot_module_pgm = local_pgm;
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;
    aot_module_map[mod_name] = {local_pgm, functions, num_functions, std::move(reexport_deps)};

    return nullptr;  // success
}

extern "C" DLLEXPORT void qore_aot_fill_module_desc(QoreModuleInfo* mod_info,
        const char* name, const char* version, const char* desc,
        const char* author, const char* url, const char* license_str,
        int api_major, int api_minor, int license,
        void* init_fn, void* ns_init_fn, void* del_fn,
        const char** deps, int num_deps) {
    mod_info->name = name;
    mod_info->version = version;
    mod_info->desc = desc;
    mod_info->author = author;
    if (url) {
        mod_info->url = url;
    }
    mod_info->api_major = api_major;
    mod_info->api_minor = api_minor;
    mod_info->license = static_cast<qore_license_t>(license);
    if (license_str) {
        mod_info->license_str = license_str;
    }
    mod_info->init = reinterpret_cast<qore_module_init_t>(init_fn);
    mod_info->ns_init = reinterpret_cast<qore_module_ns_init_t>(ns_init_fn);
    mod_info->del = reinterpret_cast<qore_module_delete_t>(del_fn);
    for (int i = 0; i < num_deps; ++i) {
        if (deps[i]) {
            mod_info->dependencies.push_back(deps[i]);
        }
    }
}

extern "C" DLLEXPORT void qore_aot_raise_init_error(ExceptionSink* xsink, QoreStringNode* err) {
    if (err) {
        xsink->raiseException("MODULE-LOAD-ERROR", err);
    }
}
