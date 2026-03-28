/* -*- mode: c++ -*- */
/*
  QoreAOTExprNodeHandlers.cpp

  Read handlers for expression tree nodes in AOT binary deserialization

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

// This file is #include'd from QoreAOTExprNodeRegistry.cpp and compiled in the SCU context
// after QoreAOTRuntime.cpp. All necessary type definitions (VarRefNode, QoreStringNode, etc.)
// are available from the SCU context. NO #include directives needed here.

namespace {

// ---- Binary read helpers (free functions taking const uint8_t*& ptr, const uint8_t* end) ----

inline uint8_t readU8(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr >= end) return 0;
    return *ptr++;
}

inline uint16_t readU16(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 2 > end) return 0;
    uint16_t v = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
    ptr += 2;
    return v;
}

inline uint32_t readU32(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) return 0;
    uint32_t v = ptr[0] | (static_cast<uint32_t>(ptr[1]) << 8)
        | (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
    ptr += 4;
    return v;
}

inline int32_t readI32(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 4 > end) return 0;
    uint32_t v = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    int32_t r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline int64_t readI64(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 8 > end) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    }
    ptr += 8;
    int64_t r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline double readF64(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 8 > end) return 0.0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    }
    ptr += 8;
    double r;
    memcpy(&r, &v, sizeof(r));
    return r;
}

inline std::string readStr(const uint8_t*& ptr, const uint8_t* end) {
    uint16_t len = readU16(ptr, end);
    if (len == 0 || ptr + len > end) {
        if (len > 0) ptr = end;  // mark exhausted
        return {};
    }
    std::string s(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return s;
}

// ---- Class resolution helper ----

static const QoreClass* en_resolveClass(QoreProgram* pgm, const std::string& name) {
    if (name.empty()) return nullptr;
    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    return qore_root_ns_private::runtimeFindClass(*pp->RootNS, name.c_str(), found_ns);
}

// ============================================================================
// Category 1: Leaf Constants (10 handlers)
// ============================================================================

static QoreValue read_node_EN_NOTHING(AOTExprNodeReadCtx& ctx) {
    readU16(ctx.ptr, ctx.end);  // num_children (0)
    return QoreValue();
}

static QoreValue read_node_EN_NULL(AOTExprNodeReadCtx& ctx) {
    readU16(ctx.ptr, ctx.end);
    return QoreValue(null());
}

static QoreValue read_node_EN_INT(AOTExprNodeReadCtx& ctx) {
    int64_t v = readI64(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue(v);
}

static QoreValue read_node_EN_FLOAT(AOTExprNodeReadCtx& ctx) {
    double v = readF64(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue(v);
}

static QoreValue read_node_EN_STRING(AOTExprNodeReadCtx& ctx) {
    std::string s = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue(new QoreStringNode(s));
}

static QoreValue read_node_EN_BOOL(AOTExprNodeReadCtx& ctx) {
    uint8_t v = readU8(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue((bool)v);
}

static QoreValue read_node_EN_NUMBER(AOTExprNodeReadCtx& ctx) {
    std::string s = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue(new QoreNumberNode(s.c_str()));
}

static QoreValue read_node_EN_BINARY(AOTExprNodeReadCtx& ctx) {
    uint32_t len = readU32(ctx.ptr, ctx.end);
    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
    if (len > 0 && ctx.ptr + len <= ctx.end) {
        bin->append(ctx.ptr, len);
        ctx.ptr += len;
    }
    readU16(ctx.ptr, ctx.end);
    return QoreValue(bin.release());
}

static QoreValue read_node_EN_DATE(AOTExprNodeReadCtx& ctx) {
    uint8_t is_relative = readU8(ctx.ptr, ctx.end);
    if (is_relative) {
        int32_t year = readI32(ctx.ptr, ctx.end);
        int32_t month = readI32(ctx.ptr, ctx.end);
        int32_t day = readI32(ctx.ptr, ctx.end);
        int32_t hour = readI32(ctx.ptr, ctx.end);
        int32_t minute = readI32(ctx.ptr, ctx.end);
        int32_t second = readI32(ctx.ptr, ctx.end);
        int32_t us = readI32(ctx.ptr, ctx.end);
        readU16(ctx.ptr, ctx.end);
        return QoreValue(DateTimeNode::makeRelative(
            year, month, day, hour, minute, second, us));
    } else {
        int64_t epoch = readI64(ctx.ptr, ctx.end);
        int32_t us = readI32(ctx.ptr, ctx.end);
        std::string zname = readStr(ctx.ptr, ctx.end);
        readU16(ctx.ptr, ctx.end);
        ExceptionSink tz_xsink;
        const AbstractQoreZoneInfo* zone = QTZM.findLoadRegion(
            zname.c_str(), &tz_xsink);
        return QoreValue(DateTimeNode::makeAbsolute(
            zone, epoch, us));
    }
}

static QoreValue read_node_EN_ENUM(AOTExprNodeReadCtx& ctx) {
    std::string enum_path = readStr(ctx.ptr, ctx.end);
    std::string member_name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    const QoreNamespace* pns = nullptr;
    const QoreEnumDecl* ed = ctx.pgm->findEnum(enum_path.c_str(), pns);
    if (!ed) {
        printd(0, "AOT expr tree: cannot resolve enum '%s'\n", enum_path.c_str());
        return QoreValue();
    }
    const QoreEnumMember* member = ed->findMember(member_name.c_str());
    if (!member) {
        printd(0, "AOT expr tree: cannot find enum member '%s::%s'\n",
            enum_path.c_str(), member_name.c_str());
        return QoreValue();
    }
    return QoreValue::makeEnum(member);
}

// ============================================================================
// Category 2: Variable References (5 handlers)
// ============================================================================

static QoreValue read_node_EN_LOCAL_VAR(AOTExprNodeReadCtx& ctx) {
    uint16_t slot = readU16(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    if (ctx.aot_ctx && slot < ctx.aot_ctx->num_locals && ctx.aot_ctx->locals[slot]) {
        LocalVar* lv = ctx.aot_ctx->locals[slot];
        return QoreValue(new VarRefNode(&loc_builtin, strdup(lv->getName()), lv, false));
    }
    printd(0, "AOT EXPR_TREE: invalid local slot %d\n", slot);
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_GLOBAL_VAR(AOTExprNodeReadCtx& ctx) {
    std::string name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    if (!name.empty()) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        const qore_ns_private* found_ns = nullptr;
        Var* v = qore_root_ns_private::runtimeFindGlobalVar(
            *pp->RootNS, name.c_str(), found_ns);
        if (!v) {
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            v = root_ns->var_list.runtimeFindVar(name.c_str());
        }
        if (v) {
            return QoreValue(new GlobalVarRefNode(&loc_builtin, strdup(name.c_str()), v));
        }
        printd(0, "AOT EXPR_TREE: cannot resolve global var '%s'\n", name.c_str());
    }
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_SELF_REF(AOTExprNodeReadCtx& ctx) {
    std::string name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    return QoreValue(new SelfVarrefNode(&loc_builtin, strdup(name.c_str())));
}

static QoreValue read_node_EN_STATIC_VAR(AOTExprNodeReadCtx& ctx) {
    std::string class_name = readStr(ctx.ptr, ctx.end);
    std::string var_name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    const QoreClass* qc = en_resolveClass(ctx.pgm, class_name);
    if (qc) {
        const QoreExternalStaticMember* m = qc->findLocalStaticMember(var_name.c_str());
        if (m) {
            QoreVarInfo* vi = const_cast<QoreVarInfo*>(
                reinterpret_cast<const QoreVarInfo*>(m));
            return QoreValue(new StaticClassVarRefNode(&loc_builtin, strdup(var_name.c_str()),
                *qc, *vi));
        }
    }
    printd(0, "AOT EXPR_TREE: cannot resolve static var %s::%s\n",
        class_name.c_str(), var_name.c_str());
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_CONST_REF(AOTExprNodeReadCtx& ctx) {
    std::string name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);
    if (!name.empty()) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        const qore_ns_private* cns = nullptr;
        const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(
            *pp->RootNS, name.c_str(), cns);
        if (ce) {
            return ce->getReferencedValue();
        }
        printd(0, "AOT EXPR_TREE: cannot resolve constant '%s'\n", name.c_str());
    }
    ctx.failed = true;
    return QoreValue();
}

// ============================================================================
// Category 3: Call Nodes (7 handlers)
// ============================================================================

static QoreValue read_node_EN_FUNC_CALL(AOTExprNodeReadCtx& ctx) {
    std::string name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    SimpleRefHolder<QoreParseListNode> pln;
    if (num_children > 0) {
        pln = new QoreParseListNode(&loc_builtin);
        for (uint16_t i = 0; i < num_children && !ctx.failed; ++i) {
            pln->add(ctx.recurse(ctx), &loc_builtin);
        }
    }
    if (ctx.failed) {
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, name.c_str());
    if (!fe) {
        printd(0, "AOT EXPR_TREE: cannot resolve function '%s'\n", name.c_str());
        ctx.failed = true;
        return QoreValue();
    }
    FunctionCallNode* fcn = new FunctionCallNode(&loc_builtin, fe, pln.release());
    fcn->resolveParseArgs();
    return QoreValue(fcn);
}

static QoreValue read_node_EN_SELF_CALL(AOTExprNodeReadCtx& ctx) {
    std::string class_name = readStr(ctx.ptr, ctx.end);
    std::string method_name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreListNode* ql = nullptr;
    if (num_children > 0) {
        ql = qore_list_private::newList(true);
        for (uint16_t i = 0; i < num_children && !ctx.failed; ++i) {
            QoreValue v = ctx.recurse(ctx);
            if (ctx.failed) {
                if (v.hasNode()) {
                    v.discard(nullptr);
                }
                break;
            }
            ql->push(v, nullptr);
        }
    }
    if (ctx.failed) {
        if (ql) {
            ql->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreClass* qc = en_resolveClass(ctx.pgm, class_name);
    if (!qc) {
        printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for self call '%s'\n",
            class_name.c_str(), method_name.c_str());
        if (ql) {
            ql->deref(nullptr);
        }
        ctx.failed = true;
        return QoreValue();
    }
    const QoreMethod* m = qc->findMethod(method_name.c_str());
    if (!m) {
        m = qc->findStaticMethod(method_name.c_str());
    }
    if (!m) {
        qore_class_private* qcp = qore_class_private::get(
            *const_cast<QoreClass*>(qc));
        m = qcp->parseFindLocalMethod(method_name.c_str());
        if (!m) {
            m = qcp->parseFindLocalStaticMethod(method_name.c_str());
        }
    }
    if (!m) {
        printd(1, "AOT EXPR_TREE: cannot find method '%s::%s'\n",
            class_name.c_str(), method_name.c_str());
        if (ql) {
            ql->deref(nullptr);
        }
        ctx.failed = true;
        return QoreValue();
    }
    printd(5, "AOT EXPR_TREE: resolved self call '%s::%s' args=%d -> %p\n",
        class_name.c_str(), method_name.c_str(), (int)num_children, m);
    SelfFunctionCallNode* base = new SelfFunctionCallNode(&loc_builtin,
        strdup(method_name.c_str()), nullptr, m,
        m->getClass(), qore_class_private::get(*m->getClass()));
    SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(*base, ql);
    base->deref(nullptr);
    return QoreValue(sfcn);
}

static QoreValue read_node_EN_STATIC_CALL(AOTExprNodeReadCtx& ctx) {
    std::string class_name = readStr(ctx.ptr, ctx.end);
    std::string method_name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    SimpleRefHolder<QoreParseListNode> pln;
    if (num_children > 0) {
        pln = new QoreParseListNode(&loc_builtin);
        for (uint16_t i = 0; i < num_children && !ctx.failed; ++i) {
            pln->add(ctx.recurse(ctx), &loc_builtin);
        }
    }
    if (ctx.failed) {
        return QoreValue();
    }
    const QoreClass* qc = en_resolveClass(ctx.pgm, class_name);
    if (!qc) {
        printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for static call '%s'\n",
            class_name.c_str(), method_name.c_str());
        ctx.failed = true;
        return QoreValue();
    }
    const QoreMethod* m = qc->findStaticMethod(method_name.c_str());
    if (!m) {
        qore_class_private* qcp = qore_class_private::get(
            *const_cast<QoreClass*>(qc));
        m = qcp->parseFindLocalStaticMethod(method_name.c_str());
    }
    if (!m) {
        printd(0, "AOT EXPR_TREE: cannot find static method '%s::%s'\n",
            class_name.c_str(), method_name.c_str());
        ctx.failed = true;
        return QoreValue();
    }
    StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, pln.release());
    smcn->resolveParseArgs();
    return QoreValue(smcn);
}

static QoreValue read_node_EN_DOT_EVAL(AOTExprNodeReadCtx& ctx) {
    std::string method_name = readStr(ctx.ptr, ctx.end);
    std::string class_path = readStr(ctx.ptr, ctx.end);
    uint8_t is_pseudo = readU8(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue target;
    SimpleRefHolder<QoreParseListNode> pln;
    if (num_children > 0) {
        target = ctx.recurse(ctx);
        if (num_children > 1) {
            pln = new QoreParseListNode(&loc_builtin);
            for (uint16_t i = 1; i < num_children && !ctx.failed; ++i) {
                pln->add(ctx.recurse(ctx), &loc_builtin);
            }
        }
    }
    if (ctx.failed) {
        target.discard(nullptr);
        return QoreValue();
    }
    MethodCallNode* mc = new MethodCallNode(&loc_builtin,
        strdup(method_name.c_str()), pln.release());
    // AOT deserialization: parse_args contain already-evaluated constants from the
    // serialized EXPR_TREE.  Resolve them into evaluated args so that exec() finds
    // them — exec() uses args (QoreListNode), not parse_args (QoreParseListNode).
    mc->resolveParseArgs();
    if (!class_path.empty()) {
        const QoreClass* dot_qc = en_resolveClass(ctx.pgm, class_path);
        if (dot_qc) {
            const QoreMethod* dot_method = is_pseudo
                ? dot_qc->findMethod(method_name.c_str())
                : dot_qc->findMethod(method_name.c_str());
            if (dot_method) {
                mc->parseSetClassAndMethod(dot_qc, dot_method);
            }
        }
    }
    return QoreValue(new QoreDotEvalOperatorNode(&loc_builtin, target, mc));
}

static QoreValue read_node_EN_NEW(AOTExprNodeReadCtx& ctx) {
    std::string class_name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    ReferenceHolder<QoreListNode> args_list(nullptr, nullptr);
    if (num_children > 0) {
        // Use newList(true) to create a list that needs evaluation — deserialized args
        // may contain AST sub-expression nodes (e.g., StaticMethodCallNode) that must be
        // evaluated at runtime by CodeEvaluationHelper::evalList().
        // Matches the pattern in read_node_EN_SELF_CALL.
        args_list = qore_list_private::newList(true);
        for (uint16_t i = 0; i < num_children && !ctx.failed; ++i) {
            QoreValue v = ctx.recurse(ctx);
            args_list->push(v, nullptr);
        }
    }
    if (ctx.failed) {
        return QoreValue();
    }
    const QoreClass* qc = en_resolveClass(ctx.pgm, class_name);
    if (!qc) {
        printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for new\n",
            class_name.c_str());
        ctx.failed = true;
        return QoreValue();
    }
    {
        const QoreMethod* cons = qc->getConstructor();
        printd(5, "AOT EXPR_TREE EN_NEW: class='%s' id=%d nargs=%d constructor=%p\n",
            qc->getName(), qc->getID(), (int)num_children, (void*)cons);
        if (cons) {
            const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
            printd(5, "  constructor vlist=%d\n", (int)cf->numVariants());
        }
    }
    NewObjectCallNode* nocn = new NewObjectCallNode(qc, args_list.release());
    printd(5, "  nocn variant=%p\n", (void*)nocn->getVariant());
    return QoreValue(nocn);
}

static QoreValue read_node_EN_SCOPED_NEW(AOTExprNodeReadCtx& ctx) {
    std::string class_name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    SimpleRefHolder<QoreParseListNode> pln;
    if (num_children > 0) {
        pln = new QoreParseListNode(&loc_builtin);
        for (uint16_t i = 0; i < num_children && !ctx.failed; ++i) {
            pln->add(ctx.recurse(ctx), &loc_builtin);
        }
    }
    if (ctx.failed) {
        return QoreValue();
    }
    const QoreClass* qc = en_resolveClass(ctx.pgm, class_name);
    if (!qc) {
        printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for scoped new\n",
            class_name.c_str());
        ctx.failed = true;
        return QoreValue();
    }
    ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, pln.release());
    socn->resolveParseArgs();
    return QoreValue(socn);
}

static QoreValue read_node_EN_CALLREF_CALL(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue callref_expr;
    SimpleRefHolder<QoreParseListNode> pln;
    if (num_children > 0) {
        callref_expr = ctx.recurse(ctx);
        if (num_children > 1) {
            pln = new QoreParseListNode(&loc_builtin);
            for (uint16_t i = 1; i < num_children && !ctx.failed; ++i) {
                pln->add(ctx.recurse(ctx), &loc_builtin);
            }
        }
    }
    if (ctx.failed) {
        callref_expr.discard(nullptr);
        return QoreValue();
    }
    CallReferenceCallNode* crcn = new CallReferenceCallNode(&loc_builtin, callref_expr, pln.release());
    crcn->resolveParseArgs();
    return QoreValue(crcn);
}

// ============================================================================
// Category 4: Access Operators (2 handlers)
// ============================================================================

static QoreValue read_node_EN_HASH_DEREF(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_SQUARE_BRKT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreSquareBracketsOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// Category 5: Unary Operators (15 handlers)
// ============================================================================

static QoreValue read_node_EN_KEYS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreKeysOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_ELEMENTS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreElementsOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_EXISTS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreExistsOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_DELETE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreDeleteOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_REMOVE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreRemoveOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_BACKGROUND(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreBackgroundOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_TRIM(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreTrimOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_CHOMP(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreChompOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_POP(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QorePopOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_INSTANCEOF(AOTExprNodeReadCtx& ctx) {
    std::string type_path = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    if (!type_path.empty()) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(ctx.pgm);
        const QoreTypeInfo* ti = type_resolver.resolve(type_path.c_str(), type_error);
        if (ti) {
            return QoreValue(new QoreInstanceOfOperatorNode(&loc_builtin, operand, ti));
        }
        printd(0, "AOT EXPR_TREE: cannot resolve type '%s' for instanceof\n",
            type_path.c_str());
    }
    operand.discard(nullptr);
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_UNARY_MINUS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreUnaryMinusOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_UNARY_PLUS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreUnaryPlusOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_LOG_NOT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreLogicalNotOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_BIT_NOT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreBinaryNotOperatorNode(&loc_builtin, operand));
}

static QoreValue read_node_EN_SHIFT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    for (uint16_t i = 1; i < num_children; ++i) {
        ctx.recurse(ctx).discard(nullptr);
    }
    return QoreValue(new QoreShiftOperatorNode(&loc_builtin, operand));
}

// ---- Binary operators & assignments (grouped case with inner switch in original) ----

static QoreValue read_node_EN_PUSH(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QorePushOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_UNSHIFT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreUnshiftOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LIST_ASSIGN(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreListAssignmentOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_PLUS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QorePlusOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MINUS(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMinusOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MULTIPLY(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMultiplicationOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_DIVIDE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreDivisionOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MODULO(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreModuloOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_SHIFT_LEFT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreShiftLeftOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_SHIFT_RIGHT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreShiftRightOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_BIT_AND(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreBinaryAndOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_BIT_OR(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreBinaryOrOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_BIT_XOR(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreBinaryXorOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_CMP(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalComparisonOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_AND(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalAndOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_OR(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalOrOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_NE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalNotEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_AEQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalAbsoluteEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_ANE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalAbsoluteNotEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_LT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalLessThanOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_GT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalGreaterThanOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_LE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalLessThanOrEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_LOG_GE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreLogicalGreaterThanOrEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_NULL_COAL(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreNullCoalescingOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_VAL_COAL(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreValueCoalescingOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_ASSIGN(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreAssignmentOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_PLUS_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QorePlusEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MINUS_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMinusEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MULTIPLY_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMultiplyEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_DIVIDE_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreDivideEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_MODULO_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreModuloEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_AND_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreAndEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_OR_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreOrEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_XOR_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreXorEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_SHL_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreShiftLeftEqualsOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_SHR_EQ(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreShiftRightEqualsOperatorNode(&loc_builtin, left, right));
}

// ---- Ternary and range operators ----

static QoreValue read_node_EN_QUESTION(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue cond, true_expr, false_expr;
    if (num_children >= 1) {
        cond = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        true_expr = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        false_expr = ctx.recurse(ctx);
    }
    return QoreValue(new QoreQuestionMarkOperatorNode(&loc_builtin,
        cond, true_expr, false_expr));
}

static QoreValue read_node_EN_RANGE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreRangeOperatorNode(&loc_builtin, left, right));
}

// ---- Regex operators ----

static QoreValue read_node_EN_REGEX_MATCH(AOTExprNodeReadCtx& ctx) {
    std::string pattern = readStr(ctx.ptr, ctx.end);
    int64_t options = readI64(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    if (ctx.failed) {
        operand.discard(nullptr);
        return QoreValue();
    }
    ExceptionSink xsink;
    QoreRegex* re = new QoreRegex(pattern.c_str(), options, &xsink);
    if (xsink) {
        printd(0, "AOT EXPR_TREE: regex compile error for pattern '%s'\n",
            pattern.c_str());
        operand.discard(nullptr);
        ctx.failed = true;
        return QoreValue();
    }
    return QoreValue(new QoreRegexMatchOperatorNode(&loc_builtin, operand, re));
}

static QoreValue read_node_EN_REGEX_EXTRACT(AOTExprNodeReadCtx& ctx) {
    std::string pattern = readStr(ctx.ptr, ctx.end);
    int64_t options = readI64(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    if (ctx.failed) {
        operand.discard(nullptr);
        return QoreValue();
    }
    ExceptionSink xsink;
    QoreRegex* re = new QoreRegex(pattern.c_str(), options, &xsink);
    if (xsink) {
        printd(0, "AOT EXPR_TREE: regex compile error for pattern '%s'\n",
            pattern.c_str());
        operand.discard(nullptr);
        ctx.failed = true;
        return QoreValue();
    }
    return QoreValue(new QoreRegexExtractOperatorNode(&loc_builtin, operand, re));
}

static QoreValue read_node_EN_REGEX_NMATCH(AOTExprNodeReadCtx& ctx) {
    std::string pattern = readStr(ctx.ptr, ctx.end);
    int64_t options = readI64(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    if (ctx.failed) {
        operand.discard(nullptr);
        return QoreValue();
    }
    ExceptionSink xsink;
    QoreRegex* re = new QoreRegex(pattern.c_str(), options, &xsink);
    if (xsink) {
        operand.discard(nullptr);
        ctx.failed = true;
        return QoreValue();
    }
    return QoreValue(new QoreRegexNMatchOperatorNode(&loc_builtin, operand, re));
}

static QoreValue read_node_EN_REGEX_SUBST(AOTExprNodeReadCtx& ctx) {
    std::string pattern = readStr(ctx.ptr, ctx.end);
    std::string replacement = readStr(ctx.ptr, ctx.end);
    int64_t options = readI64(ctx.ptr, ctx.end);
    uint8_t global = readU8(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    if (ctx.failed) {
        operand.discard(nullptr);
        return QoreValue();
    }
    QoreRegexSubst* rs = new QoreRegexSubst();
    if (options & PCRE2_CASELESS) {
        rs->setCaseInsensitive();
    }
    if (options & PCRE2_DOTALL) {
        rs->setDotAll();
    }
    if (options & PCRE2_EXTENDED) {
        rs->setExtended();
    }
    if (options & PCRE2_MULTILINE) {
        rs->setMultiline();
    }
    ExceptionSink xsink;
    if (rs->parseRT(pattern.c_str(), &xsink)) {
        printd(0, "AOT EXPR_TREE: regex subst compile error for pattern '%s'\n",
            pattern.c_str());
        delete rs;
        operand.discard(nullptr);
        ctx.failed = true;
        return QoreValue();
    }
    if (global) {
        rs->setGlobal();
    }
    for (char c : replacement) {
        rs->concatTarget(c);
    }
    return QoreValue(new QoreRegexSubstOperatorNode(&loc_builtin, operand, rs));
}

static QoreValue read_node_EN_TRANSLIT(AOTExprNodeReadCtx& ctx) {
    std::string source = readStr(ctx.ptr, ctx.end);
    std::string target = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    QoreTransliteration* tr = new QoreTransliteration(&loc_builtin);
    for (char c : source) {
        tr->concatSource(c);
    }
    tr->finishSource();
    for (char c : target) {
        tr->concatTarget(c);
    }
    tr->finishTarget();
    return QoreValue(new QoreTransliterationOperatorNode(&loc_builtin, operand, tr));
}

// ---- Cast operator ----

static QoreValue read_node_EN_CAST(AOTExprNodeReadCtx& ctx) {
    std::string type_path = readStr(ctx.ptr, ctx.end);
    uint8_t or_nothing = readU8(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue operand;
    if (num_children >= 1) {
        operand = ctx.recurse(ctx);
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path.c_str(), type_error);
    if (!ti) {
        printd(0, "AOT EXPR_TREE: cannot resolve cast type '%s': %s\n",
            type_path.c_str(), type_error.c_str());
        operand.discard(nullptr);
        ctx.failed = true;
        return QoreValue();
    }
    const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(ti);
    if (qc) {
        return QoreValue(new QoreClassCastOperatorNode(&loc_builtin, qc, operand,
            or_nothing != 0));
    }
    qore_type_t bt = QoreTypeInfo::getBaseType(ti);
    if (bt == NT_HASH) {
        const QoreTypeInfo* ch = QoreTypeInfo::getUniqueReturnComplexHash(ti);
        if (ch) {
            return QoreValue(new QoreComplexHashCastOperatorNode(&loc_builtin, ti, operand,
                or_nothing != 0));
        }
    }
    if (bt == NT_LIST) {
        const QoreTypeInfo* cl = QoreTypeInfo::getUniqueReturnComplexList(ti);
        if (cl) {
            return QoreValue(new QoreComplexListCastOperatorNode(&loc_builtin, ti, operand,
                or_nothing != 0));
        }
    }
    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
    if (hd) {
        return QoreValue(new QoreHashDeclCastOperatorNode(&loc_builtin, hd, operand,
            or_nothing != 0));
    }
    return QoreValue(new QoreClassCastOperatorNode(&loc_builtin, nullptr, operand,
        or_nothing != 0));
}

// ---- Pre/post increment/decrement ----

static QoreValue read_node_EN_PRE_INC(AOTExprNodeReadCtx& ctx) {
    uint16_t n = readU16(ctx.ptr, ctx.end);
    QoreValue op;
    if (n >= 1) {
        op = ctx.recurse(ctx);
    }
    return QoreValue(new QorePreIncrementOperatorNode(&loc_builtin, op));
}

static QoreValue read_node_EN_PRE_DEC(AOTExprNodeReadCtx& ctx) {
    uint16_t n = readU16(ctx.ptr, ctx.end);
    QoreValue op;
    if (n >= 1) {
        op = ctx.recurse(ctx);
    }
    return QoreValue(new QorePreDecrementOperatorNode(&loc_builtin, op));
}

static QoreValue read_node_EN_POST_INC(AOTExprNodeReadCtx& ctx) {
    uint16_t n = readU16(ctx.ptr, ctx.end);
    QoreValue op;
    if (n >= 1) {
        op = ctx.recurse(ctx);
    }
    return QoreValue(new QorePostIncrementOperatorNode(&loc_builtin, op));
}

static QoreValue read_node_EN_POST_DEC(AOTExprNodeReadCtx& ctx) {
    uint16_t n = readU16(ctx.ptr, ctx.end);
    QoreValue op;
    if (n >= 1) {
        op = ctx.recurse(ctx);
    }
    return QoreValue(new QorePostDecrementOperatorNode(&loc_builtin, op));
}

// ---- Multi-child operators ----

static QoreValue read_node_EN_EXTRACT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue lvalue, offset, length, new_val;
    if (num_children >= 1) {
        lvalue = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        offset = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        length = ctx.recurse(ctx);
    }
    if (num_children >= 4) {
        new_val = ctx.recurse(ctx);
    }
    return QoreValue(new QoreExtractOperatorNode(&loc_builtin,
        lvalue, offset, length, new_val));
}

static QoreValue read_node_EN_SPLICE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue lvalue, offset, length, new_val;
    if (num_children >= 1) {
        lvalue = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        offset = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        length = ctx.recurse(ctx);
    }
    if (num_children >= 4) {
        new_val = ctx.recurse(ctx);
    }
    return QoreValue(new QoreSpliceOperatorNode(&loc_builtin,
        lvalue, offset, length, new_val));
}

static QoreValue read_node_EN_PARSE_LIST(AOTExprNodeReadCtx& ctx) {
    uint16_t count = readU16(ctx.ptr, ctx.end);
    QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
    for (uint16_t i = 0; i < count; ++i) {
        pln->add(ctx.recurse(ctx), &loc_builtin);
    }
    return QoreValue(pln);
}

// ---- Special method reference nodes ----

static QoreValue read_node_EN_OBJ_METH_REF(AOTExprNodeReadCtx& ctx) {
    std::string method_name = readStr(ctx.ptr, ctx.end);
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue target;
    if (num_children >= 1) {
        target = ctx.recurse(ctx);
    }
    return QoreValue(new ParseObjectMethodReferenceNode(&loc_builtin,
        target, strdup(method_name.c_str())));
}

static QoreValue read_node_EN_SELF_METH_REF(AOTExprNodeReadCtx& ctx) {
    std::string method_name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);  // 0 children
    return QoreValue(new ParseSelfMethodReferenceNode(&loc_builtin,
        strdup(method_name.c_str())));
}

static QoreValue read_node_EN_FUNC_REF(AOTExprNodeReadCtx& ctx) {
    std::string name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);  // 0 children
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, name.c_str());
    if (fe) {
        return QoreValue(new FunctionCallNode(&loc_builtin, fe, (QoreListNode*)nullptr, ctx.pgm));
    }
    printd(0, "AOT EXPR_TREE: cannot resolve function ref '%s'\n", name.c_str());
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_STATIC_METH_REF(AOTExprNodeReadCtx& ctx) {
    std::string class_path = readStr(ctx.ptr, ctx.end);
    std::string method_name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);  // 0 children
    ExceptionSink xsink;
    const QoreClass* qc = ctx.pgm->findClass(class_path.c_str(), &xsink);
    if (xsink.isException()) {
        xsink.clear();
    }
    if (qc) {
        const QoreMethod* m = qc->findStaticMethod(method_name.c_str());
        if (!m) {
            m = qc->findMethod(method_name.c_str());
        }
        if (m) {
            return QoreValue(new LocalStaticMethodCallReferenceNode(&loc_builtin, m));
        }
    }
    printd(0, "AOT EXPR_TREE: cannot resolve static method ref '%s::%s'\n",
        class_path.c_str(), method_name.c_str());
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_BOUND_METH_REF(AOTExprNodeReadCtx& ctx) {
    std::string class_path = readStr(ctx.ptr, ctx.end);
    std::string method_name = readStr(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);  // 0 children
    ExceptionSink xsink;
    const QoreClass* qc = ctx.pgm->findClass(class_path.c_str(), &xsink);
    if (xsink.isException()) {
        xsink.clear();
    }
    if (qc) {
        const QoreMethod* m = qc->findMethod(method_name.c_str());
        if (!m) {
            m = qc->findStaticMethod(method_name.c_str());
        }
        if (m) {
            return QoreValue(new LocalMethodCallReferenceNode(&loc_builtin, m));
        }
    }
    printd(0, "AOT EXPR_TREE: cannot resolve bound method ref '%s::%s'\n",
        class_path.c_str(), method_name.c_str());
    ctx.failed = true;
    return QoreValue();
}

static QoreValue read_node_EN_CLOSURE(AOTExprNodeReadCtx& ctx) {
    uint32_t slot = readU32(ctx.ptr, ctx.end);
    readU16(ctx.ptr, ctx.end);  // 0 children
    if (ctx.aot_ctx && slot < static_cast<uint32_t>(ctx.aot_ctx->num_exprs) &&
        ctx.aot_ctx->exprs[slot]) {
        QoreValue v;
        memcpy(&v, &ctx.aot_ctx->exprs[slot], sizeof(v));
        return v.refSelf();
    }
    printd(0, "AOT EXPR_TREE: cannot resolve closure expr slot %u\n", slot);
    ctx.failed = true;
    return QoreValue();
}

// ---- Collection and container operators ----

static QoreValue read_node_EN_LIST(AOTExprNodeReadCtx& ctx) {
    uint16_t count = readU16(ctx.ptr, ctx.end);
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
    for (uint16_t i = 0; i < count; ++i) {
        list->push(ctx.recurse(ctx), nullptr);
    }
    return QoreValue(list.release());
}

static QoreValue read_node_EN_HASH(AOTExprNodeReadCtx& ctx) {
    uint16_t count = readU16(ctx.ptr, ctx.end);
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), nullptr);
    for (uint16_t i = 0; i < count; ++i) {
        std::string key = readStr(ctx.ptr, ctx.end);
        QoreValue val = ctx.recurse(ctx);
        hash->setKeyValue(key.c_str(), val, nullptr);
    }
    return QoreValue(hash.release());
}

static QoreValue read_node_EN_PARSE_HASH(AOTExprNodeReadCtx& ctx) {
    uint16_t count = readU16(ctx.ptr, ctx.end);
    QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
    for (uint16_t i = 0; i < count; ++i) {
        QoreValue key = ctx.recurse(ctx);
        QoreValue val = ctx.recurse(ctx);
        phn->add(key, val, &loc_builtin);
    }
    return QoreValue(phn);
}

static QoreValue read_node_EN_IMPLICIT_ARG(AOTExprNodeReadCtx& ctx) {
    int16_t offset;
    uint16_t raw = readU16(ctx.ptr, ctx.end);
    memcpy(&offset, &raw, sizeof(offset));
    readU16(ctx.ptr, ctx.end);  // 0 children
    int ctor_offset = (offset >= 0) ? (offset + 1) : offset;
    return QoreValue(new QoreImplicitArgumentNode(&loc_builtin, ctor_offset));
}

static QoreValue read_node_EN_IMPLICIT_ELEM(AOTExprNodeReadCtx& ctx) {
    readU16(ctx.ptr, ctx.end);  // 0 children
    return QoreValue(new QoreImplicitElementNode(&loc_builtin));
}

static QoreValue read_node_EN_REF_TO_LVALUE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue lv_exp;
    if (num_children >= 1) {
        lv_exp = ctx.recurse(ctx);
    }
    return QoreValue(new ParseReferenceNode(&loc_builtin, lv_exp));
}

static QoreValue read_node_EN_SQ_BRKT_RANGE(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue target, start, end_val;
    if (num_children >= 1) {
        target = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        start = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        end_val = ctx.recurse(ctx);
    }
    return QoreValue(new QoreSquareBracketsRangeOperatorNode(&loc_builtin,
        target, start, end_val));
}

// ---- Map and fold operators ----

static QoreValue read_node_EN_MAP(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue map_expr, source;
    if (num_children >= 1) {
        map_expr = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        source = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMapOperatorNode(&loc_builtin, map_expr, source));
}

static QoreValue read_node_EN_MAP_SELECT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue map_expr, source, where_expr;
    if (num_children >= 1) {
        map_expr = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        source = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        where_expr = ctx.recurse(ctx);
    }
    return QoreValue(new QoreMapSelectOperatorNode(&loc_builtin,
        map_expr, source, where_expr));
}

static QoreValue read_node_EN_HASH_MAP(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue key_expr, val_expr, source;
    if (num_children >= 1) {
        key_expr = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        val_expr = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        source = ctx.recurse(ctx);
    }
    return QoreValue(new QoreHashMapOperatorNode(&loc_builtin,
        key_expr, val_expr, source));
}

static QoreValue read_node_EN_HASH_MAP_SELECT(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue key_expr, val_expr, source, where_expr;
    if (num_children >= 1) {
        key_expr = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        val_expr = ctx.recurse(ctx);
    }
    if (num_children >= 3) {
        source = ctx.recurse(ctx);
    }
    if (num_children >= 4) {
        where_expr = ctx.recurse(ctx);
    }
    return QoreValue(new QoreHashMapSelectOperatorNode(&loc_builtin,
        key_expr, val_expr, source, where_expr));
}

static QoreValue read_node_EN_FOLDL(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreFoldlOperatorNode(&loc_builtin, left, right));
}

static QoreValue read_node_EN_FOLDR(AOTExprNodeReadCtx& ctx) {
    uint16_t num_children = readU16(ctx.ptr, ctx.end);
    QoreValue left, right;
    if (num_children >= 1) {
        left = ctx.recurse(ctx);
    }
    if (num_children >= 2) {
        right = ctx.recurse(ctx);
    }
    return QoreValue(new QoreFoldrOperatorNode(&loc_builtin, left, right));
}


} // anonymous namespace
