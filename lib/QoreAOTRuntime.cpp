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

    // Read body locals
    for (int i = 0; i < num_body_locals; ++i) {
        const char* blname = reader.readStringRef(ptr);
        const char* bltype = reader.readStringRef(ptr);
        uint8_t bl_closure = QoreAOTBinaryReader::readU8(ptr);

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
                        reader.readStringRef(ptr);
                        break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
                        break;
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
                        reader.readStringRef(ptr); break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                        reader.readStringRef(ptr); reader.readStringRef(ptr); break;
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
        // Function names can be "funcName" or "ClassName::methodName"
        UserVariantBase* uvb = nullptr;
        std::string fname_str(func_name);
        size_t sep = fname_str.find("::");

        if (sep != std::string::npos) {
            // Method: ClassName::methodName
            std::string class_name = fname_str.substr(0, sep);
            std::string method_name = fname_str.substr(sep + 2);

            qore_program_private* pp = qore_program_private::get(*pgm);
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, class_name.c_str());
            if (qc) {
                const QoreMethod* m = qc->findMethod(method_name.c_str());
                if (!m) {
                    m = qc->findStaticMethod(method_name.c_str());
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
            auto it = func_map.find(fname);
            if (it != func_map.end()) {
                const QoreAOTFunc* aot_func = it->second;
                QoreAOTContext* ctx = buildContextForVariant(uvb, fname, pgm, *aot_func);
                if (ctx) {
                    uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled function '%s' (locals=%d, globals=%d, exprs=%d)\n",
                        fname, aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                } else {
                    printd(1, "AOT: failed to build context for function '%s'\n", fname);
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
                auto it = func_map.find(method_name);
                if (it != func_map.end()) {
                    const QoreAOTFunc* aot_func = it->second;
                    QoreAOTContext* ctx = buildContextForVariant(uvb, method_name.c_str(), pgm, *aot_func);
                    if (ctx) {
                        uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                        ++registered;
                        printd(2, "AOT: registered pre-compiled method '%s' (locals=%d, globals=%d, exprs=%d)\n",
                            method_name.c_str(), aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                    } else {
                        printd(1, "AOT: failed to build context for method '%s'\n", method_name.c_str());
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

extern "C" int qore_aot_run(
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

extern "C" int qore_aot_run_v2(
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

        // If any functions need source fallback, parse the fallback source
        // to get a full AST for IR re-lowering
        QoreProgram* fallback_pgm = nullptr;
        if (deserializer.hasFallbackSource()) {
            ExceptionSink wsink;
            fallback_pgm = new QoreProgram(parse_options);
            fallback_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
                QP_WARN_DEFAULT);
            if (wsink.isException()) {
                wsink.handleWarnings();
            }
            if (xsink.isException()) {
                fprintf(stderr, "AOT: fallback source parse error\n");
                xsink.handleExceptions();
                fallback_pgm->waitForTerminationAndDeref(nullptr);
                qore_cleanup();
                return 2;
            }
        }

        // Register pre-compiled function pointers
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

            // For functions that needed source fallback, use the fallback
            // program's namespace tree with IR re-lowering
            if (fallback_pgm) {
                int fallback_registered = 0;
                registerAOTFunctionsInNamespace(
                    qore_ns_private::get(*qore_program_private::get(*fallback_pgm)->RootNS),
                    fallback_pgm, func_map, fallback_registered);
                registered += fallback_registered;
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
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
                                        case AOTExprKind::SELF_METHOD_CALL:
                                        case AOTExprKind::STATIC_METHOD_CALL:
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
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

//! Global state for the AOT-compiled module (set in init, used in ns_init/delete)
static QoreProgram* aot_module_pgm = nullptr;
static std::string aot_module_name;
static const QoreAOTFunc* aot_module_funcs = nullptr;
static int aot_module_num_funcs = 0;

extern "C" QoreStringNode* qore_aot_module_init(
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;
    ExceptionSink wsink;

    // Create a QoreProgram for the module with the embedded parse options
    aot_module_pgm = new QoreProgram(parse_options);
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;

    // Set JIT execution mode so functions without pre-compiled code will JIT on demand
    aot_module_pgm->setExecMode(QEM_JIT);

    // Set up module context for the parser (must be QoreUserModuleDefContextHelper
    // because the parser static_casts to it)
    // Note: Do NOT call setNameInit() here - the scanner calls it when it parses
    // the "module Name" declaration, and calling it twice triggers an assertion.
    {
        QoreUserModuleDefContextHelper mod_ctx(mod_name, label, aot_module_pgm, xsink);

        // Parse the embedded source
        QoreString src_str(source, source_len);
        aot_module_pgm->parse(src_str.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);

        mod_ctx.close();
    }

    if (wsink.isException()) {
        wsink.handleWarnings();
    }

    if (xsink.isException()) {
        QoreStringNode* err = new QoreStringNode("AOT module parse error: ");
        // Get error description
        QoreValue ex = xsink.getExceptionErr();
        if (ex.getType() == NT_STRING) {
            err->concat(ex.get<const QoreStringNode>()->c_str());
        } else {
            err->concat("unknown parse error");
        }
        xsink.clear();
        aot_module_pgm->waitForTerminationAndDeref(nullptr);
        aot_module_pgm = nullptr;
        return err;
    }

    // Register pre-compiled AOT functions on the module's namespace tree
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*aot_module_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        registerAOTFunctionsInNamespace(root_ns, aot_module_pgm, func_map, registered);

        printd(1, "AOT module '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    return nullptr;  // success
}

extern "C" void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns) {
    if (!aot_module_pgm) {
        return;
    }

    // Get the module program's root namespace
    QoreNamespace* mod_root = aot_module_pgm->getRootNS();
    if (!mod_root) {
        return;
    }

    // Find the module's namespace by name and add a copy to root_ns
    // User modules typically define a public namespace matching the module name
    qore_ns_private* mod_root_priv = qore_ns_private::get(*mod_root);

    // Copy all user-defined child namespaces from the module to root_ns
    for (auto ni = mod_root_priv->nsl.nsmap.begin(), ne = mod_root_priv->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (!child_ns) {
            continue;
        }
        // Skip system namespaces (Qore::, etc.)
        const char* ns_name = child_ns->getName();
        if (!strcmp(ns_name, "Qore") || !strcmp(ns_name, "::")) {
            continue;
        }

        // Copy the namespace and add it to root_ns
        QoreNamespace* ns_copy = child_ns->copy();
        root_ns->addNamespace(ns_copy);

        // Now register AOT functions on the copied namespace
        // The copy has fresh function variants that need AOT registration
        if (aot_module_num_funcs > 0 && aot_module_funcs) {
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            for (int i = 0; i < aot_module_num_funcs; ++i) {
                if (aot_module_funcs[i].name && aot_module_funcs[i].fn_ptr) {
                    func_map[aot_module_funcs[i].name] = &aot_module_funcs[i];
                }
            }
            int registered = 0;
            registerAOTFunctionsInNamespace(qore_ns_private::get(*ns_copy),
                aot_module_pgm, func_map, registered);
            printd(2, "AOT module ns_init: registered %d AOT functions in namespace '%s'\n",
                registered, ns_name);
        }
    }
}

extern "C" void qore_aot_module_delete() {
    if (aot_module_pgm) {
        aot_module_pgm->waitForTerminationAndDeref(nullptr);
        aot_module_pgm = nullptr;
    }
    aot_module_name.clear();
    aot_module_funcs = nullptr;
    aot_module_num_funcs = 0;
}

extern "C" QoreStringNode* qore_aot_module_init_v2(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;

    // Create a QoreProgram for the module
    aot_module_pgm = new QoreProgram(parse_options);
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;

    // Set JIT execution mode
    aot_module_pgm->setExecMode(QEM_JIT);

    // Deserialize namespace tree from metadata (replaces source parsing)
    QoreAOTBinaryDeserializer deserializer;
    std::string deser_error;
    if (!deserializer.deserializeIntoProgram(aot_module_pgm,
            metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
        err->concat(deser_error.c_str());
        aot_module_pgm->waitForTerminationAndDeref(nullptr);
        aot_module_pgm = nullptr;
        return err;
    }

    // If any functions need source fallback, parse the fallback source
    if (deserializer.hasFallbackSource()) {
        ExceptionSink wsink;
        // Set up module context for parsing fallback source
        // Note: Do NOT call setNameInit() here - the scanner calls it when it parses
        // the "module Name" declaration, and calling it twice triggers an assertion.
        QoreUserModuleDefContextHelper mod_ctx(mod_name, label, aot_module_pgm, xsink);

        aot_module_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
            QP_WARN_DEFAULT);
        mod_ctx.close();

        if (wsink.isException()) {
            wsink.handleWarnings();
        }
        if (xsink.isException()) {
            QoreStringNode* err = new QoreStringNode("AOT module fallback parse error: ");
            QoreValue ex = xsink.getExceptionErr();
            if (ex.getType() == NT_STRING) {
                err->concat(ex.get<const QoreStringNode>()->c_str());
            } else {
                err->concat("unknown parse error");
            }
            xsink.clear();
            aot_module_pgm->waitForTerminationAndDeref(nullptr);
            aot_module_pgm = nullptr;
            return err;
        }
    }

    // Register pre-compiled AOT functions
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*aot_module_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        registerAOTFunctionsInNamespace(root_ns, aot_module_pgm, func_map, registered);

        printd(1, "AOT module v2 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    return nullptr;  // success
}
