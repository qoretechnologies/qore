/* -*- mode: c++ -*- */
/*
  QoreAOTExprHandlers.cpp

  Expression kind handlers for AOT binary serialization registry

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

  Note that the Qore library is dual-licensed under a choice of two
  licenses.
*/

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreAOTExprRegistry.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"

// ============================================================================
// FUNC_CALL (1)
// ============================================================================

static bool write_expr_func_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL));
        ctx.writer.writeStringRef(call->getName());
        return true;
    }
    return false;
}

static QoreValue read_expr_func_call(AOTExprReadCtx& ctx) {
    const char* func_name = ctx.reader.readStringRef(ctx.ptr);
    if (!func_name || !*func_name) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, func_name);
    if (!fe) {
        return QoreValue();
    }
    FunctionCallNode* fcn = new FunctionCallNode(&loc_builtin, fe, (QoreListNode*)nullptr, ctx.pgm);
    return QoreValue(fcn);
}

// ============================================================================
// SELF_METHOD_CALL (2)
// ============================================================================

static bool write_expr_self_method_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            ctx.writer.writeStringRef(qc ? qc->getPath() : "");
        } else {
            ctx.writer.writeStringRef("");
        }
        ctx.writer.writeStringRef(call->getName());
        return true;
    }
    return false;
}

static QoreValue read_expr_self_method_call(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    if (!method_name || !*method_name) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        const qore_ns_private* found_ns = nullptr;
        qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
    }
    if (!qc) {
        return QoreValue();
    }
    const QoreMethod* m = qc->findMethod(method_name);
    if (!m) {
        m = qc->findStaticMethod(method_name);
    }
    if (!m) {
        qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
        m = qcp->parseFindLocalMethod(method_name);
        if (!m) {
            m = qcp->parseFindLocalStaticMethod(method_name);
        }
    }
    if (!m) {
        return QoreValue();
    }
    SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin, strdup(method_name), nullptr, m,
        m->getClass(), qore_class_private::get(*m->getClass()));
    return QoreValue(sfcn);
}

// ============================================================================
// STATIC_METHOD_CALL (3)
// ============================================================================

static bool write_expr_static_method_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            ctx.writer.writeStringRef(qc ? qc->getPath() : "");
        } else {
            ctx.writer.writeStringRef("");
        }
        ctx.writer.writeStringRef(call->getName());
        // Serialize method args
        const QoreListNode* args = call->getArgs();
        if (args && args->size() > 0) {
            ctx.writer.writeU8(static_cast<uint8_t>(args->size()));
            for (size_t j = 0; j < args->size(); ++j) {
                ::classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j),
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
        } else {
            ctx.writer.writeU8(0);
        }
        return true;
    }
    return false;
}

static QoreValue read_expr_static_method_call(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !method_name) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path, found_ns);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreMethod* m = qc->findStaticMethod(method_name);
    if (!m) {
        qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
        m = qcp->parseFindLocalStaticMethod(method_name);
    }
    if (!m) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    // Create with evaluated args list directly via the copy constructor pattern
    // StaticMethodCallNode needs QoreParseListNode for its primary constructor,
    // so we create a minimal node and set the args list for evaluation
    StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, (QoreParseListNode*)nullptr);
    if (args_list) {
        // Set the args as a member; since StaticMethodCallNode inherits FunctionCallBase,
        // we use resolveParseArgs after setting parse_args, or set args directly.
        // The args_list already has needs_eval_flag=true for proper evaluation.
        // We need to set the args field directly — create a temporary parse_args list
        // from the args, then resolve.
        // Actually, just delete the smcn and use NewObjectCallNode approach:
        // The simplest correct path is to create a wrapper that holds the args.
        delete smcn;

        // Create a new StaticMethodCallNode by first building a QoreParseListNode
        // from the evaluated args (they may contain AST nodes needing evaluation)
        QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(args_list);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        args_list->deref(nullptr);
        smcn = new StaticMethodCallNode(&loc_builtin, m, pln);
        smcn->resolveParseArgs();
    }
    return QoreValue(smcn);
}

// ============================================================================
// NEW_OBJECT (4)
// ============================================================================

static bool write_expr_new_object(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
            ctx.writer.writeStringRef(qc->getPath());
            const QoreListNode* args = vrn->getArgs();
            if (args && args->size() > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(args->size()));
                for (size_t j = 0; j < args->size(); ++j) {
                    classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
        const QoreClass* qc = no->getClass();
        ctx.writer.writeStringRef(qc ? qc->getPath() : "");
        ctx.writer.writeU8(0);
        return true;
    }
    return false;
}

static QoreValue read_expr_new_object(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !*class_path) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path, found_ns);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    NewObjectCallNode* nocn = new NewObjectCallNode(qc, args_list);
    return QoreValue(nocn);
}

// ============================================================================
// RUNTIME_CONST_REF (5)
// ============================================================================

static bool write_expr_runtime_const_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (ctx.const_reverse_map) {
        auto it = ctx.const_reverse_map->find(node);
        if (it != ctx.const_reverse_map->end()) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            ctx.writer.writeStringRef(it->second.c_str());
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_runtime_const_ref(AOTExprReadCtx& ctx) {
    const char* const_name = ctx.reader.readStringRef(ctx.ptr);
    if (!const_name || !*const_name) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* cns = nullptr;
    const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(
        *pp->RootNS, const_name, cns);
    if (!ce) {
        // Try class constant lookup: path format "ClassName::ConstName"
        std::string path(const_name);
        size_t sep = path.rfind("::");
        if (sep != std::string::npos && sep > 0) {
            std::string class_path = path.substr(0, sep);
            std::string cname = path.substr(sep + 2);
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, class_path.c_str(), found_ns);
            if (qc) {
                ce = qore_class_private::get(*qc)->constlist.findEntry(cname.c_str());
            }
        }
    }
    if (!ce) {
        printd(0, "AOT: cannot resolve constant '%s'\n", const_name);
        return QoreValue();
    }
    // For constants with values already available (builtin module constants,
    // already-initialized user constants), return the value directly.
    // For constants with pending init functions (saved_val empty because
    // the init function hasn't run yet), create a RuntimeConstantRefNode
    // for deferred evaluation — the init function will populate saved_val
    // via setRuntimeValue() before any user code evaluates the expression.
    if (ce->hasValue()) {
        return ce->getReferencedValue();
    }
    return QoreValue(new RuntimeConstantRefNode(&loc_builtin, const_cast<ConstantEntry*>(ce), true));
}

// ============================================================================
// SELF_VARREF (6)
// ============================================================================

static bool write_expr_self_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* svn = dynamic_cast<const SelfVarrefNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_VARREF));
        ctx.writer.writeStringRef(svn->str ? svn->str : "");
        return true;
    }
    return false;
}

static QoreValue read_expr_self_varref(AOTExprReadCtx& ctx) {
    const char* member_name = ctx.reader.readStringRef(ctx.ptr);
    SelfVarrefNode* svn = new SelfVarrefNode(&loc_builtin, strdup(member_name ? member_name : ""));
    return QoreValue(svn);
}

// ============================================================================
// LOCAL_VARREF (7)
// ============================================================================

static bool write_expr_local_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_LOCAL || varref->getType() == VT_LOCAL_TS ||
                varref->getType() == VT_CLOSURE) {
            // First pass: match by pointer identity (handles same-named variables
            // in different scopes correctly)
            const void* var_ptr = varref->ref.id;
            if (var_ptr) {
                for (size_t i = 0; i < ctx.parent_locals.size(); ++i) {
                    if (ctx.parent_locals[i].local_var_ptr == var_ptr) {
                        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        ctx.writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
            }
            // Second pass: fall back to name match (for cases where pointer isn't available)
            for (size_t i = 0; i < ctx.parent_locals.size(); ++i) {
                if (varref->getName() && ctx.parent_locals[i].name == varref->getName()) {
                    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                    ctx.writer.writeStringRef(std::to_string(i).c_str());
                    return true;
                }
            }
        }
    }
    return false;
}

static QoreValue read_expr_local_varref(AOTExprReadCtx& ctx) {
    const char* index_str = ctx.reader.readStringRef(ctx.ptr);
    if (!index_str) {
        return QoreValue();
    }
    int local_slot = std::atoi(index_str);
    if (local_slot < 0 || local_slot >= ctx.num_locals || !ctx.locals || !ctx.locals[local_slot]) {
        return QoreValue();
    }
    LocalVar* lv = ctx.locals[local_slot];
    // NOTE: always use false for in_closure — VT_LOCAL type calls ref.id->eval() which
    // internally checks closure_use and uses the correct lookup (local stack vs closure stack).
    // VT_CLOSURE uses thread_get_runtime_closure_var() (pointer-based runtime closure env lookup)
    // which returns null outside a closure execution context.
    VarRefNode* vrn = new VarRefNode(&loc_builtin, strdup(lv->getName()), lv, false);
    return QoreValue(vrn);
}

// ============================================================================
// GLOBAL_VARREF (8)
// ============================================================================

static bool write_expr_global_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_GLOBAL || varref->getType() == VT_THREAD_LOCAL) {
            Var* global_var = varref->ref.var;
            if (global_var) {
                for (size_t i = 0; i < ctx.parent_globals.size(); ++i) {
                    if (ctx.parent_globals[i].name == global_var->getName()) {
                        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                        ctx.writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static QoreValue read_expr_global_varref(AOTExprReadCtx& ctx) {
    const char* index_str = ctx.reader.readStringRef(ctx.ptr);
    if (!index_str) {
        return QoreValue();
    }
    int global_slot = std::atoi(index_str);
    if (global_slot < 0 || global_slot >= ctx.num_globals || !ctx.globals || !ctx.globals[global_slot]) {
        return QoreValue();
    }
    Var* gvar = ctx.globals[global_slot];
    GlobalVarRefNode* vrn = new GlobalVarRefNode(&loc_builtin, strdup(gvar->getName()), gvar);
    return QoreValue(vrn);
}

// ============================================================================
// CONST_NUMBER (9)
// ============================================================================

static bool write_expr_const_number(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NUMBER));
        QoreString str;
        num->toString(str);
        ctx.writer.writeStringRef(str.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_number(AOTExprReadCtx& ctx) {
    const char* num_str = ctx.reader.readStringRef(ctx.ptr);
    if (!num_str || !*num_str) {
        return QoreValue();
    }
    QoreNumberNode* num = new QoreNumberNode(num_str);
    return QoreValue(num);
}

// ============================================================================
// CONST_BINARY (10)
// ============================================================================

static bool write_expr_const_binary(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BINARY));
        std::string hex;
        const unsigned char* data = static_cast<const unsigned char*>(bin->getPtr());
        size_t len = bin->size();
        hex.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", data[i]);
            hex.append(buf);
        }
        ctx.writer.writeStringRef(hex.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_binary(AOTExprReadCtx& ctx) {
    const char* hex_str = ctx.reader.readStringRef(ctx.ptr);
    if (!hex_str) {
        return QoreValue();
    }
    size_t hex_len = strlen(hex_str);
    size_t bin_len = hex_len / 2;
    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
    if (bin_len > 0) {
        bin->preallocate(bin_len);
        unsigned char* dst = static_cast<unsigned char*>(
            const_cast<void*>(bin->getPtr()));
        for (size_t i = 0; i < bin_len; ++i) {
            unsigned int byte;
            sscanf(hex_str + i * 2, "%2x", &byte);
            dst[i] = static_cast<unsigned char>(byte);
        }
    }
    return QoreValue(bin.release());
}

// ============================================================================
// CLOSURE_CREATE (11)
// ============================================================================

static bool write_expr_closure_create(AOTExprWriteCtx& ctx) {
    // Closures in expression context are now serialized by classifyAndWriteExpr()
    // in QoreAOTBinary.cpp. This handler is only called from the expression registry
    // path which is not used for closure writes.
    return false;
}

static QoreValue read_expr_closure_create(AOTExprReadCtx& ctx) {
    // Read flags
    const char* flags_str = ctx.reader.readStringRef(ctx.ptr);
    const char* class_type_path = ctx.reader.readStringRef(ctx.ptr);

    bool is_lambda = false, is_in_method = false;
    if (flags_str) {
        is_lambda = (flags_str[0] == '1');
        is_in_method = (strlen(flags_str) >= 3 && flags_str[2] == '1');
    }

    // Read return type
    const char* ret_type_path = ctx.reader.readStringRef(ctx.ptr);
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ret_type = (ret_type_path && *ret_type_path)
        ? type_resolver.resolve(ret_type_path, type_error) : nullptr;

    // Read params
    uint16_t closure_num_params = QoreAOTBinaryReader::readU16(ctx.ptr);
    std::vector<std::string> param_names(closure_num_params);
    std::vector<const QoreTypeInfo*> param_types(closure_num_params);
    std::vector<QoreValue> defaults(closure_num_params);
    for (uint16_t p = 0; p < closure_num_params; ++p) {
        const char* pname = ctx.reader.readStringRef(ctx.ptr);
        param_names[p] = pname ? pname : "";
        const char* ptype = ctx.reader.readStringRef(ctx.ptr);
        param_types[p] = (ptype && *ptype)
            ? type_resolver.resolve(ptype, type_error) : nullptr;
        uint8_t has_default = QoreAOTBinaryReader::readU8(ctx.ptr);
        if (has_default) {
            std::string val_error;
            defaults[p] = ctx.reader.readValue(ctx.ptr, ctx.end, val_error);
        }
    }
    bool closure_has_varargs = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;

    // Read captured variable names and parent slot indices
    uint16_t num_captured = QoreAOTBinaryReader::readU16(ctx.ptr);
    std::vector<std::string> captured_names(num_captured);
    std::vector<int32_t> captured_parent_slots(num_captured, -1);
    for (uint16_t c = 0; c < num_captured; ++c) {
        const char* cname = ctx.reader.readStringRef(ctx.ptr);
        captured_names[c] = cname ? cname : "";
        captured_parent_slots[c] = static_cast<int32_t>(QoreAOTBinaryReader::readU32(ctx.ptr));
    }

    // Read closure body IR
    uint8_t has_ir = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!has_ir) {
        ctx.error = "closure has no IR data";
        return QoreValue();
    }

    uint32_t ir_size = QoreAOTBinaryReader::readU32(ctx.ptr);
    const uint8_t* ir_end_ptr = ctx.ptr + ir_size;

    // Resolve class for method context
    const QoreClass* closure_class = nullptr;
    if (class_type_path && *class_type_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        const qore_ns_private* found_ns = nullptr;
        closure_class = qore_root_ns_private::runtimeFindClass(
            *pp->RootNS, class_type_path, found_ns);
    }

    // Construct UserClosureFunction + UserClosureVariant
    ExceptionSink closure_xsink;
    ProgramRuntimeParseContextHelper closure_pch(&closure_xsink, ctx.pgm);
    if (closure_xsink.isException()) {
        closure_xsink.clear();
        ctx.ptr = ir_end_ptr;
        ctx.error = "failed to acquire parse context for closure";
        return QoreValue();
    }
    auto* ucf = new UserClosureFunction(nullptr, 0, 0, QoreValue(), nullptr);
    auto* closure_variant = static_cast<UserClosureVariant*>(
        const_cast<AbstractQoreFunctionVariant*>(ucf->first()));
    UserSignature* closure_sig = closure_variant->getUserSignature();
    closure_sig->setupFromAOTMetadata(
        ctx.pgm, ret_type, param_names, param_types, defaults, closure_has_varargs,
        closure_class);

    // Build enclosing locals map for IR deserialization
    std::unordered_map<std::string, LocalVar*> enclosing_locals;
    // 1. Parent function's locals
    for (int l = 0; l < ctx.num_locals; ++l) {
        if (ctx.locals[l] && ctx.locals[l]->getName()) {
            enclosing_locals[ctx.locals[l]->getName()] = ctx.locals[l];
        }
    }
    // 1b. Override with slot-indexed captured variables for disambiguation
    for (uint16_t ci = 0; ci < num_captured; ++ci) {
        int32_t parent_slot = captured_parent_slots[ci];
        if (parent_slot >= 0 && parent_slot < ctx.num_locals
                && ctx.locals[parent_slot]) {
            enclosing_locals[captured_names[ci]] = ctx.locals[parent_slot];
        }
    }
    // 2. Closure's own parameter locals from its signature
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        const char* pname = closure_sig->getName(p);
        if (pname && *pname) {
            enclosing_locals[pname] = closure_sig->lv[p];
        }
    }
    if (closure_sig->argvid) {
        enclosing_locals["argv"] = closure_sig->argvid;
    }
    if (closure_sig->selfid) {
        enclosing_locals["self"] = closure_sig->selfid;
    }

    // Deserialize closure body IR
    std::string ir_error;
    auto readExprCb = [&ctx]
            (const QoreAOTBinaryReader& rdr, const uint8_t*& p,
            const uint8_t* e, std::string& err) -> QoreValue {
        return readOneExpr(rdr, p, e, err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    };
    auto closure_ir = deserializeIRFunction(ctx.reader, ctx.ptr, ir_end_ptr, ctx.pgm,
        readExprCb, &enclosing_locals, ir_error,
        ctx.locals, ctx.num_locals);
    ctx.ptr = ir_end_ptr;  // Ensure we advance past IR data

    if (!closure_ir) {
        printd(2, "AOT: closure IR deser failed in expr context: %s\n", ir_error.c_str());
        delete ucf;
        ctx.error = "closure IR deserialization failed: " + ir_error;
        return QoreValue();
    }

    // Set up captured variables in LVarSet and ensure closureUse
    LVarSet* closure_vlist = ucf->getVList();
    for (uint16_t ci = 0; ci < num_captured; ++ci) {
        int32_t parent_slot = captured_parent_slots[ci];
        LocalVar* lv = nullptr;
        if (parent_slot >= 0 && parent_slot < ctx.num_locals
                && ctx.locals[parent_slot]) {
            lv = ctx.locals[parent_slot];
        } else {
            auto it = enclosing_locals.find(captured_names[ci]);
            if (it != enclosing_locals.end()) {
                lv = it->second;
            }
        }
        if (lv) {
            closure_vlist->add(lv);
            if (!lv->closureUse()) {
                lv->setClosureUse();
            }
        }
    }

    // Populate pre_instantiated_locals
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        if (closure_sig->lv[p]) {
            closure_ir->pre_instantiated_locals.insert(
                reinterpret_cast<const void*>(closure_sig->lv[p]));
        }
    }
    if (closure_sig->argvid) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(closure_sig->argvid));
    }
    if (closure_sig->selfid) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(closure_sig->selfid));
    }
    for (LocalVar* lv : closure_ir->all_body_locals) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(lv));
    }

    // Build cached_pre_instantiated for the IR interpreter
    auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        if (closure_sig->lv[p]) {
            cached_pre_inst->insert(closure_sig->lv[p]);
        }
    }
    if (closure_sig->argvid) {
        cached_pre_inst->insert(closure_sig->argvid);
    }
    if (closure_sig->selfid) {
        cached_pre_inst->insert(closure_sig->selfid);
    }
    closure_ir->cached_pre_instantiated = cached_pre_inst;

    // Set cached IR on variant
    closure_ir->computeSlotIdsAndEmbed();
    closure_variant->setCachedIR(closure_ir.release());
    closure_variant->pgm = ctx.pgm;

    // Set class type if in a method context
    if (class_type_path && *class_type_path) {
        ucf->setClassType(type_resolver.resolve(class_type_path, type_error));
    }

    // Create QoreClosureParseNode
    auto* closure_node = new QoreClosureParseNode(nullptr, ucf, is_lambda, is_in_method);
    return QoreValue(closure_node);
}

// ============================================================================
// CALL_REF (12)
// ============================================================================

static bool write_expr_call_ref(AOTExprWriteCtx& ctx) {
    // Call references need the full AST context for proper resolution
    return false;
}

static QoreValue read_expr_call_ref(AOTExprReadCtx& ctx) {
    // Requires source fallback
    return QoreValue();
}

// ============================================================================
// OBJ_METHOD_REF (13)
// ============================================================================

static bool write_expr_obj_method_ref(AOTExprWriteCtx& ctx) {
    // Object method references need the full AST context for proper resolution
    return false;
}

static QoreValue read_expr_obj_method_ref(AOTExprReadCtx& ctx) {
    // Requires source fallback
    return QoreValue();
}

// ============================================================================
// STATIC_VARREF (14)
// ============================================================================

static bool write_expr_static_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* sv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_VARREF));
        ctx.writer.writeStringRef(sv->qc.getName());
        ctx.writer.writeStringRef(sv->str.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_static_varref(AOTExprReadCtx& ctx) {
    const char* class_name = ctx.reader.readStringRef(ctx.ptr);
    const char* var_name = ctx.reader.readStringRef(ctx.ptr);
    if (!class_name || !var_name) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_name, found_ns);
    if (!qc) {
        return QoreValue();
    }
    const QoreExternalStaticMember* m = qc->findLocalStaticMember(var_name);
    if (!m) {
        return QoreValue();
    }
    QoreVarInfo* vi = const_cast<QoreVarInfo*>(
        reinterpret_cast<const QoreVarInfo*>(m));
    StaticClassVarRefNode* node = new StaticClassVarRefNode(&loc_builtin, strdup(var_name),
        *qc, *vi);
    return QoreValue(node);
}

// ============================================================================
// SCOPED_NEW_OBJECT (15)
// ============================================================================

static bool write_expr_scoped_new_object(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        if (socn->oc) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SCOPED_NEW_OBJECT));
            ctx.writer.writeStringRef(socn->oc->getPath());
            const QoreListNode* args = socn->getArgs();
            uint8_t num_args = args && args->size() <= 255 ? static_cast<uint8_t>(args->size()) : 0;
            ctx.writer.writeU8(num_args);
            for (uint8_t j = 0; j < num_args; ++j) {
                classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_scoped_new_object(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !*class_path) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path, found_ns);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    // ScopedObjectCallNode expects QoreParseListNode, not QoreListNode
    QoreParseListNode* pln = nullptr;
    if (args_list) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(args_list);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        args_list->deref(nullptr);
    }
    ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, pln);
    // Convert parse_args to args so evalImpl() doesn't hit the assertion
    if (pln) {
        socn->resolveParseArgs();
    }
    return QoreValue(socn);
}

// ============================================================================
// HASHDECL_NEW (16)
// ============================================================================

static bool write_expr_hashdecl_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
        if (nhd->hd) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            ctx.writer.writeStringRef(nhd->hd->getNamespacePath().c_str());
            // Serialize constructor args (typically a single hash initializer)
            if (nhd->args && nhd->args->size() > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(nhd->args->size()));
                for (size_t j = 0; j < nhd->args->size(); ++j) {
                    ::classifyAndWriteExpr(ctx.writer, nhd->args->get(j),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_hashdecl_new(AOTExprReadCtx& ctx) {
    const char* hashdecl_path = ctx.reader.readStringRef(ctx.ptr);
    if (!hashdecl_path || !*hashdecl_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* call_args = nullptr;
    if (num_args > 0) {
        call_args = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                arg.discard(nullptr);
                call_args->push(QoreValue(), nullptr);
            } else {
                call_args->push(arg, nullptr);
            }
        }
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
        *pp->RootNS, hashdecl_path, found_ns);
    if (!hd) {
        if (call_args) {
            call_args->deref(nullptr);
        }
        return QoreValue();
    }
    // Convert call_args to QoreParseListNode for NewHashDeclNode
    QoreParseListNode* pln = nullptr;
    if (call_args) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(call_args);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        call_args->deref(nullptr);
    }
    NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd, pln, false);
    return QoreValue(nhd);
}

// ============================================================================
// COMPLEX_HASH_NEW (17)
// ============================================================================

static bool write_expr_complex_hash_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (nch->typeInfo) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            ctx.writer.writeStringRef(QoreTypeInfo::getPath(nch->typeInfo));
            // Serialize constructor args
            if (nch->args && nch->args->size() > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(nch->args->size()));
                for (size_t j = 0; j < nch->args->size(); ++j) {
                    ::classifyAndWriteExpr(ctx.writer, nch->args->get(j),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_complex_hash_new(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* call_args = nullptr;
    if (num_args > 0) {
        call_args = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                arg.discard(nullptr);
                call_args->push(QoreValue(), nullptr);
            } else {
                call_args->push(arg, nullptr);
            }
        }
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        if (call_args) {
            call_args->deref(nullptr);
        }
        return QoreValue();
    }
    // Convert call_args to QoreParseListNode for NewComplexHashNode
    QoreParseListNode* pln = nullptr;
    if (call_args) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(call_args);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        call_args->deref(nullptr);
    }
    NewComplexHashNode* nch = new NewComplexHashNode(&loc_builtin, ti, pln);
    return QoreValue(nch);
}

// ============================================================================
// COMPLEX_LIST_NEW (18)
// ============================================================================

static bool write_expr_complex_list_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (ncl->typeInfo) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            ctx.writer.writeStringRef(QoreTypeInfo::getPath(ncl->typeInfo));
            // Serialize constructor arg (single QoreValue)
            if (ncl->args.hasNode()) {
                ctx.writer.writeU8(1);
                ::classifyAndWriteExpr(ctx.writer, ncl->args,
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_complex_list_new(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue arg_val;
    if (num_args > 0) {
        std::string arg_err;
        arg_val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!arg_err.empty()) {
            arg_val.discard(nullptr);
            arg_val = QoreValue();
        }
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        arg_val.discard(nullptr);
        return QoreValue();
    }
    NewComplexListNode* ncl = new NewComplexListNode(&loc_builtin, ti, arg_val);
    return QoreValue(ncl);
}

// ============================================================================
// CONST_ENUM (19)
// ============================================================================

static bool write_expr_const_enum(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.isEnum()) {
        const QoreEnumMember* member = ctx.expr.getEnumMember();
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_ENUM));
        std::string ns_path = member->getEnumDecl()->getNamespacePath();
        ctx.writer.writeStringRef(ns_path.c_str());
        ctx.writer.writeStringRef(member->getName());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_enum(AOTExprReadCtx& ctx) {
    const char* enum_path = ctx.reader.readStringRef(ctx.ptr);
    const char* member_name = ctx.reader.readStringRef(ctx.ptr);
    if (!enum_path || !member_name) {
        return QoreValue();
    }
    const QoreNamespace* pns = nullptr;
    const QoreEnumDecl* ed = ctx.pgm->findEnum(enum_path, pns);
    if (!ed) {
        return QoreValue();
    }
    const QoreEnumMember* member = ed->findMember(member_name);
    if (!member) {
        return QoreValue();
    }
    return QoreValue::makeEnum(member);
}

// ============================================================================
// CONST_STRING (20)
// ============================================================================

static bool write_expr_const_string(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        ctx.writer.writeStringRef(str->c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_string(AOTExprReadCtx& ctx) {
    const char* str_content = ctx.reader.readStringRef(ctx.ptr);
    return QoreValue(new QoreStringNode(str_content ? str_content : ""));
}

// ============================================================================
// HASH_LITERAL (21)
// ============================================================================

static bool write_expr_hash_literal(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* qhn = dynamic_cast<const QoreHashNode*>(node)) {
        if (qhn->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(qhn->size()));
            ConstHashIterator it(qhn);
            while (it.next()) {
                ctx.writer.writeStringRef(it.getKey());
                classifyAndWriteExpr(ctx.writer, it.get(), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
            return true;
        }
    }
    if (auto* phn = dynamic_cast<const QoreParseHashNode*>(node)) {
        const QoreParseHashNode::nvec_t& keys = phn->getKeys();
        const QoreParseHashNode::nvec_t& vals = phn->getValues();
        if (keys.size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                QoreStringValueHelper key(keys[i]);
                ctx.writer.writeStringRef(key->c_str());
                classifyAndWriteExpr(ctx.writer, vals[i], ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_hash_literal(AOTExprReadCtx& ctx) {
    uint8_t num_pairs = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
    for (uint8_t j = 0; j < num_pairs; ++j) {
        const char* key_str = ctx.reader.readStringRef(ctx.ptr);
        std::string val_err;
        QoreValue val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!val_err.empty()) {
            ctx.error = val_err;
            val.discard(nullptr);
            phn->deref(nullptr);
            return QoreValue();
        }
        phn->add(new QoreStringNode(key_str ? key_str : ""), val, &loc_builtin);
    }
    return QoreValue(phn);
}

// ============================================================================
// HASH_DEREF (22)
// ============================================================================

static bool write_expr_hash_deref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_DEREF));
        classifyAndWriteExpr(ctx.writer, hd->getLeft(), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        classifyAndWriteExpr(ctx.writer, hd->getRight(), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        return true;
    }
    return false;
}

static QoreValue read_expr_hash_deref(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty()) {
        ctx.error = left_err;
        return QoreValue();
    }
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!right_err.empty()) {
        ctx.error = right_err;
        left.discard(nullptr);
        return QoreValue();
    }
    return QoreValue(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// PARSE_REF (23)
// ============================================================================

static bool write_expr_parse_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* prn = dynamic_cast<const ParseReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_REF));
        classifyAndWriteExpr(ctx.writer, prn->getLVExp(), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
        return true;
    }
    return false;
}

static QoreValue read_expr_parse_ref(AOTExprReadCtx& ctx) {
    std::string inner_err;
    QoreValue inner = readOneExpr(ctx.reader, ctx.ptr, ctx.end, inner_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!inner_err.empty()) {
        ctx.error = inner_err;
        return QoreValue();
    }
    return QoreValue(new ParseReferenceNode(&loc_builtin, inner));
}

// ============================================================================
// CAST_HASHDECL (24)
// ============================================================================

static bool write_expr_cast_hashdecl(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hdc->getCastTypeInfo());
        if (hd) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_HASHDECL));
            ctx.writer.writeStringRef(hd->getNamespacePath().c_str());
            ctx.writer.writeU8(hdc->isOrNothing() ? 1 : 0);
            // Serialize the inner expression being cast
            QoreValue inner = hdc->getExp();
            if (inner.hasNode()) {
                ctx.writer.writeU8(1);
                ::classifyAndWriteExpr(ctx.writer, inner,
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_cast_hashdecl(AOTExprReadCtx& ctx) {
    const char* hashdecl_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    // Read inner expression (added for sub-expression serialization)
    uint8_t has_inner = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (has_inner) {
        std::string inner_err;
        inner = readOneExpr(ctx.reader, ctx.ptr, ctx.end, inner_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!inner_err.empty()) {
            ctx.error = inner_err;
            return QoreValue();
        }
    }
    if (!hashdecl_path || !*hashdecl_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
        *pp->RootNS, hashdecl_path, found_ns);
    if (!hd) {
        inner.discard(nullptr);
        return QoreValue();
    }
    // If no inner expression, use empty hash for the cast (common case: <HashdeclType>{})
    if (!inner.hasNode()) {
        inner = QoreValue(new QoreHashNode(autoTypeInfo));
    }
    auto* node = new QoreHashDeclCastOperatorNode(&loc_builtin, hd, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_COMPLEX_HASH (25)
// ============================================================================

static bool write_expr_cast_complex_hash(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* chc = dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_HASH));
        ctx.writer.writeStringRef(QoreTypeInfo::getPath(chc->getCastTypeInfo()));
        ctx.writer.writeU8(chc->isOrNothing() ? 1 : 0);
        return true;
    }
    return false;
}

static QoreValue read_expr_cast_complex_hash(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        return QoreValue();
    }
    auto* node = new QoreComplexHashCastOperatorNode(&loc_builtin, ti, QoreValue(), or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_COMPLEX_LIST (26)
// ============================================================================

static bool write_expr_cast_complex_list(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* clc = dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_LIST));
        ctx.writer.writeStringRef(QoreTypeInfo::getPath(clc->getCastTypeInfo()));
        ctx.writer.writeU8(clc->isOrNothing() ? 1 : 0);
        return true;
    }
    return false;
}

static QoreValue read_expr_cast_complex_list(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        return QoreValue();
    }
    auto* node = new QoreComplexListCastOperatorNode(&loc_builtin, ti, QoreValue(), or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_CLASS (27)
// ============================================================================

static bool write_expr_cast_class(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* cc = dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(cc->getCastTypeInfo());
        if (qc) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_CLASS));
            ctx.writer.writeStringRef(qc->getPath());
            ctx.writer.writeU8(cc->isOrNothing() ? 1 : 0);
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_cast_class(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!class_path || !*class_path) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path, found_ns);
    if (!qc) {
        return QoreValue();
    }
    auto* node = new QoreClassCastOperatorNode(&loc_builtin, qc, QoreValue(), or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_ENUM (28)
// ============================================================================

static bool write_expr_cast_enum(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* ec = dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_ENUM));
        ctx.writer.writeStringRef(QoreTypeInfo::getPath(ec->getCastTypeInfo()));
        ctx.writer.writeU8(ec->isOrNothing() ? 1 : 0);
        return true;
    }
    return false;
}

static QoreValue read_expr_cast_enum(AOTExprReadCtx& ctx) {
    const char* enum_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!enum_path || !*enum_path) {
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(enum_path, type_error);
    if (!ti) {
        return QoreValue();
    }
    const QoreEnumDecl* ed = QoreTypeInfo::getUniqueReturnEnum(ti);
    if (!ed) {
        return QoreValue();
    }
    auto* node = new QoreEnumCastOperatorNode(&loc_builtin, ed, ti, QoreValue(), or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CONST_INT (29)
// ============================================================================

static bool write_expr_const_int(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_INT) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
        ctx.writer.writeI64(ctx.expr.getAsBigInt());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_int(AOTExprReadCtx& ctx) {
    return QoreValue(QoreAOTBinaryReader::readI64(ctx.ptr));
}

// ============================================================================
// CONST_FLOAT (30)
// ============================================================================

static bool write_expr_const_float(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_FLOAT) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
        ctx.writer.writeF64(ctx.expr.getAsFloat());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_float(AOTExprReadCtx& ctx) {
    return QoreValue(QoreAOTBinaryReader::readF64(ctx.ptr));
}

// ============================================================================
// CONST_BOOL (31)
// ============================================================================

static bool write_expr_const_bool(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_BOOLEAN) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BOOL));
        ctx.writer.writeU8(ctx.expr.getAsBool() ? 1 : 0);
        return true;
    }
    return false;
}

static QoreValue read_expr_const_bool(AOTExprReadCtx& ctx) {
    return QoreValue((bool)QoreAOTBinaryReader::readU8(ctx.ptr));
}

// ============================================================================
// CONST_NOTHING (32)
// ============================================================================

static bool write_expr_const_nothing(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_NOTHING) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    }
    return false;
}

static QoreValue read_expr_const_nothing(AOTExprReadCtx& ctx) {
    return QoreValue();
}

// ============================================================================
// LIST_LITERAL (33)
// ============================================================================

static bool write_expr_list_literal(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* qln = dynamic_cast<const QoreListNode*>(node)) {
        if (qln->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(qln->size()));
            for (size_t i = 0; i < qln->size(); ++i) {
                classifyAndWriteExpr(ctx.writer, qln->retrieveEntry(i), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
            return true;
        }
    }
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
        if (pln->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(pln->size()));
            for (size_t i = 0; i < pln->size(); ++i) {
                classifyAndWriteExpr(ctx.writer, pln->get(i), ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_list_literal(AOTExprReadCtx& ctx) {
    uint8_t count = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
    for (uint8_t j = 0; j < count; ++j) {
        std::string val_err;
        QoreValue val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!val_err.empty()) {
            ctx.error = val_err;
            val.discard(nullptr);
            pln->deref();
            return QoreValue();
        }
        pln->add(val, &loc_builtin);
    }
    return QoreValue(pln);
}

// ============================================================================
// EXPR_TREE (254)
// ============================================================================

static bool write_expr_expr_tree(AOTExprWriteCtx& ctx) {
    // Recursive expression tree serialization — handled specially in buildContextFromSlotMap
    // This is a stub for now; expression trees are resolved separately
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::EXPR_TREE));
    return true;
}

static QoreValue read_expr_expr_tree(AOTExprReadCtx& ctx) {
    // Handled inline in buildContextFromSlotMap via ExprTreeDeserializer
    return QoreValue();
}

// ============================================================================
// GENERIC_EVAL (255)
// ============================================================================

static bool write_expr_generic_eval(AOTExprWriteCtx& ctx) {
    // Fallback for unsupported expression types
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
    return true;
}

static QoreValue read_expr_generic_eval(AOTExprReadCtx& ctx) {
    // Signal that this expression cannot be deserialized — the calling code
    // must set has_unsupported to trigger source fallback for the function.
    // Without this error, GENERIC_EVAL in nested expressions (e.g., constructor args)
    // silently becomes NOTHING, corrupting argument lists at runtime.
    ctx.error = "unsupported nested expression (GENERIC_EVAL)";
    return QoreValue();
}
