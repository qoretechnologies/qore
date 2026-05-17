/* -*- mode: c++ -*- */
/*
  QoreAOTExprSlotHandlers.cpp

  Expression slot metadata write handlers for AOT binary serialization

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

namespace {

// Forward declarations for recursive functions (will use :: to disambiguate)
// classifyAndWriteExpr is declared in this namespace as extern,
// but the real definition is at global scope in QoreAOTBinary.cpp

// ============================================================================
// Handler Functions for Expression Slot Metadata Serialization
// ============================================================================

static bool write_slot_inline_expr(AOTExprSlotWriteCtx& ctx, const QoreValue& expr) {
    return ::classifyAndWriteExpr(ctx.writer, expr, ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_parse_args(AOTExprSlotWriteCtx& ctx, const QoreParseListNode* args) {
    size_t nargs = args ? args->size() : 0;
    if (nargs > 255) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(nargs));
    for (size_t j = 0; j < nargs; ++j) {
        if (!write_slot_inline_expr(ctx, args->get(j))) {
            return false;
        }
    }
    return true;
}

static bool write_slot_call_args(AOTExprSlotWriteCtx& ctx, const QoreListNode* args) {
    size_t nargs = args ? args->size() : 0;
    if (nargs > 255) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(nargs));
    for (size_t j = 0; j < nargs; ++j) {
        if (!write_slot_inline_expr(ctx, args->retrieveEntry(j))) {
            return false;
        }
    }
    return true;
}

static bool write_slot_args_prefer_parse(AOTExprSlotWriteCtx& ctx) {
    if (ctx.expr.parse_args && ctx.expr.parse_args->size() > 0) {
        return write_slot_parse_args(ctx, ctx.expr.parse_args);
    }
    return write_slot_call_args(ctx, ctx.expr.call_args);
}

static bool write_slot_args_prefer_call(AOTExprSlotWriteCtx& ctx) {
    if (ctx.expr.call_args && ctx.expr.call_args->size() > 0) {
        return write_slot_call_args(ctx, ctx.expr.call_args);
    }
    return write_slot_parse_args(ctx, ctx.expr.parse_args);
}

//! NEW_OBJECT and SCOPED_NEW_OBJECT
//! ref1 = class path, ref2 = variant signature (e.g. "(string,int)" or "()" ),
//! ref3 = instantiated object type path when QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO is set.
//! No inline args — constructor args are computed by separate IR instructions
//! that feed the NewObject's operand values at runtime.
static bool write_slot_NEW_OBJECT(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    if ((ctx.writer.feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
        ctx.writer.writeStringRef(ctx.expr.ref3.c_str());
    }
    return true;
}

//! SCOPED_NEW_OBJECT (same handler as NEW_OBJECT)
static bool write_slot_SCOPED_NEW_OBJECT(AOTExprSlotWriteCtx& ctx) {
    return write_slot_NEW_OBJECT(ctx);
}

//! FUNC_CALL: ref1 = function name, ref2 = optional "sig:" + parse-time variant signature
static bool write_slot_FUNC_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! RUNTIME_CONST_REF: ref1 = constant name
static bool write_slot_RUNTIME_CONST_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! LOCAL_VARREF: ref1 = local slot index (as string)
static bool write_slot_LOCAL_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! GLOBAL_VARREF: ref1 = global slot index (as string)
static bool write_slot_GLOBAL_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_NUMBER: ref1 = string representation
static bool write_slot_CONST_NUMBER(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_BINARY: ref1 = hex-encoded bytes
static bool write_slot_CONST_BINARY(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_INT: ref1 = i64 value as string
static bool write_slot_CONST_INT(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_FLOAT: ref1 = f64 value as string
static bool write_slot_CONST_FLOAT(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_BOOL: ref1 = "0" or "1"
static bool write_slot_CONST_BOOL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_STRING: ref1 = string content
static bool write_slot_CONST_STRING(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CONST_VALUE: serialized QoreValue payload
static bool write_slot_CONST_VALUE(AOTExprSlotWriteCtx& ctx) {
    return ctx.writer.writeValue(ctx.expr.child_expr);
}

//! HASHDECL_NEW: ref1 = hashdecl path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_HASHDECL_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return write_slot_args_prefer_parse(ctx);
}

//! COMPLEX_HASH_NEW: ref1 = type path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_COMPLEX_HASH_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return write_slot_args_prefer_parse(ctx);
}

//! COMPLEX_LIST_NEW: ref1 = type path + u8 num_args + N×classifyAndWriteExpr-encoded args
static bool write_slot_COMPLEX_LIST_NEW(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    if (ctx.expr.child_expr.hasNode()) {
        ctx.writer.writeU8(1);
        return write_slot_inline_expr(ctx, ctx.expr.child_expr);
    }
    if ((ctx.expr.parse_args && ctx.expr.parse_args->size())
            || (ctx.expr.call_args && ctx.expr.call_args->size())) {
        return false;
    }
    ctx.writer.writeU8(0);
    return true;
}

//! CONST_NOTHING: no additional data
static bool write_slot_CONST_NOTHING(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! CONST_NULL: NULL constant — no payload
static bool write_slot_CONST_NULL(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! DOT_EVAL_TARGET: ref1 = class path, ref2 = method name[\nvariant class\nvariant signature], flags = is_pseudo
static bool write_slot_DOT_EVAL_TARGET(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    ctx.writer.writeU8(ctx.expr.flags);
    return true;
}

//! DOT_EVAL_EXPR: full inline dot-eval expression payload for AST consumers such as Context
static bool write_slot_DOT_EVAL_EXPR(AOTExprSlotWriteCtx& ctx) {
    return write_slot_inline_expr(ctx, ctx.expr.child_expr);
}

//! FUNC_CALL_REF: ref1 = function name
static bool write_slot_FUNC_CALL_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! BOUND_METHOD_REF: ref1 = class path, ref2 = method name
static bool write_slot_BOUND_METHOD_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! STATIC_METHOD_REF: ref1 = class path, ref2 = method name
static bool write_slot_STATIC_METHOD_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! SELF_METHOD_REF: ref1 = method name
static bool write_slot_SELF_METHOD_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! OBJ_METHOD_REF_EXPR: ref1 = method name + inline child expression
static bool write_slot_OBJ_METHOD_REF_EXPR(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    // The child expression (target object) is serialized inline using classifyAndWriteExpr
    return write_slot_inline_expr(ctx, ctx.expr.child_expr);
}

//! HASH_LITERAL: num_pairs(u8) + [key_str + value(AOTExprKind)] * N
static bool write_slot_HASH_LITERAL(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    if (auto* qhn = dynamic_cast<const QoreHashNode*>(node)) {
        if (qhn->size() > 255) {
            return false;
        }
        ctx.writer.writeU8(static_cast<uint8_t>(qhn->size()));
        ConstHashIterator it(qhn);
        uint16_t count = 0;
        while (it.next()) {
            if (++count % 100 == 0 && qore_check_cancel(nullptr, "AOT hash literal serialization")) {
                return false;
            }
            ctx.writer.writeStringRef(it.getKey());
            if (!write_slot_inline_expr(ctx, it.get())) {
                return false;
            }
        }
        return true;
    }
    if (auto* phn = dynamic_cast<const QoreParseHashNode*>(node)) {
        const QoreParseHashNode::nvec_t& keys = phn->getKeys();
        const QoreParseHashNode::nvec_t& vals = phn->getValues();
        if (keys.size() > 255 || keys.size() != vals.size()) {
            return false;
        }
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT hash literal key validation")) {
                return false;
            }
            const QoreValue& key = keys[i];
            if (key.needsEval()) {
                return false;
            }
        }
        ctx.writer.writeU8(static_cast<uint8_t>(keys.size()));
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT hash literal serialization")) {
                return false;
            }
            QoreStringValueHelper key(keys[i]);
            ctx.writer.writeStringRef(key->c_str());
            if (!write_slot_inline_expr(ctx, vals[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}

//! PARSE_HASH: count(u8) + [key(AOTExprKind) + value(AOTExprKind)] * N
static bool write_slot_PARSE_HASH(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* phn = dynamic_cast<const QoreParseHashNode*>(node);
    if (!phn) {
        return false;
    }

    const QoreParseHashNode::nvec_t& keys = phn->getKeys();
    const QoreParseHashNode::nvec_t& vals = phn->getValues();
    if (keys.size() > 255 || keys.size() != vals.size()) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(keys.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT parse hash serialization")) {
            return false;
        }
        if (!classifyAndWriteExpr(ctx.writer, keys[i],
                ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
                || !classifyAndWriteExpr(ctx.writer, vals[i],
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
            return false;
        }
    }
    return true;
}

static bool write_slot_PLUS(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QorePlusOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_SQUARE_BRACKET(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_SQUARE_BRACKET_RANGE(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->get(0),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_EXISTS(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreExistsOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_IMPLICIT_ARG(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* ia = dynamic_cast<const QoreImplicitArgumentNode*>(node);
    if (!ia) {
        return false;
    }
    ctx.writer.writeI64(static_cast<int64_t>(ia->getOffset()));
    return true;
}

static bool write_slot_MINUS(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMinusOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_KEYS(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreKeysOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_MULTIPLY(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_DIVIDE(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDivisionOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_MODULO(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreModuloOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_IMPLICIT_ELEM(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    return dynamic_cast<const QoreImplicitElementNode*>(node) != nullptr;
}

static bool write_slot_INSTANCEOF(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* inst = dynamic_cast<const QoreInstanceOfOperatorNode*>(node);
    if (!inst) {
        return false;
    }
    const QoreTypeInfo* ti = inst->getInstanceTypeInfo();
    std::string type_path = ctx.expr.ref1.empty()
        ? qore_get_aot_serializable_type_path(ti) : ctx.expr.ref1;
    if (type_path.empty()) {
        return false;
    }
    ctx.writer.writeStringRef(type_path.c_str());
    return classifyAndWriteExpr(ctx.writer, inst->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_regex_match_slot_payload(AOTExprSlotWriteCtx& ctx, const QoreRegexMatchOperatorNode* op) {
    QoreRegex* re = op->getRegex();
    const char* pattern = re ? re->getPatternCStr() : nullptr;
    if (!pattern) {
        return false;
    }
    ctx.writer.writeStringRef(pattern);
    ctx.writer.writeI64(re->getOptions() | (re->isGlobal() ? QRE_GLOBAL : 0));
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_REGEX_MATCH(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexMatchOperatorNode*>(node);
    if (!op || dynamic_cast<const QoreRegexNMatchOperatorNode*>(node)
            || dynamic_cast<const QoreRegexExtractOperatorNode*>(node)) {
        return false;
    }
    return write_regex_match_slot_payload(ctx, op);
}

static bool write_slot_REGEX_NMATCH(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexNMatchOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return write_regex_match_slot_payload(ctx, op);
}

static bool write_slot_REGEX_EXTRACT(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexExtractOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return write_regex_match_slot_payload(ctx, op);
}

static bool write_slot_PRE_INC(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node);
    if (!op || dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_PRE_DEC(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_POST_INC(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    if (dynamic_cast<const QorePostDecrementOperatorNode*>(node)
            || dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        return false;
    }
    QoreValue operand;
    if (auto* op = dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, operand,
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_POST_DEC(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    QoreValue operand;
    if (auto* op = dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, operand,
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

template <typename NodeT>
static bool write_binary_slot_payload(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const NodeT*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

template <typename NodeT>
static bool write_unary_slot_payload(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const NodeT*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_LOG_EQ(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreLogicalEqualsOperatorNode>(ctx);
}

static bool write_slot_LOG_NE(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalNotEqualsOperatorNode>(ctx);
}

static bool write_slot_LOG_AEQ(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QoreLogicalAbsoluteNotEqualsOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreLogicalAbsoluteEqualsOperatorNode>(ctx);
}

static bool write_slot_LOG_ANE(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalAbsoluteNotEqualsOperatorNode>(ctx);
}

static bool write_slot_LOG_LT(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalLessThanOperatorNode>(ctx);
}

static bool write_slot_LOG_GT(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalGreaterThanOperatorNode>(ctx);
}

static bool write_slot_LOG_LE(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalLessThanOrEqualsOperatorNode>(ctx);
}

static bool write_slot_LOG_GE(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalGreaterThanOrEqualsOperatorNode>(ctx);
}

static bool write_slot_BIT_AND(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreBinaryAndOperatorNode>(ctx);
}

static bool write_slot_BIT_OR(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreBinaryOrOperatorNode>(ctx);
}

static bool write_slot_BIT_XOR(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreBinaryXorOperatorNode>(ctx);
}

static bool write_slot_SHIFT_LEFT(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreShiftLeftOperatorNode>(ctx);
}

static bool write_slot_SHIFT_RIGHT(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreShiftRightOperatorNode>(ctx);
}

static bool write_slot_LOG_AND(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QoreLogicalOrOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreLogicalAndOperatorNode>(ctx);
}

static bool write_slot_LOG_OR(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreLogicalOrOperatorNode>(ctx);
}

static bool write_slot_CALLREF_CALL(AOTExprSlotWriteCtx& ctx) {
    const QoreListNode* args = ctx.expr.call_args;
    const QoreParseListNode* pargs = ctx.expr.parse_args;
    size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
    if (nargs > 255) {
        return false;
    }
    if (!::classifyAndWriteExpr(ctx.writer, ctx.expr.child_expr,
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(nargs));
    for (size_t j = 0; j < nargs; ++j) {
        QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
        if (!::classifyAndWriteExpr(ctx.writer, arg, ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map)) {
            return false;
        }
    }
    return true;
}

static bool write_slot_LOG_NOT(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreLogicalNotOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_UNARY_MINUS(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreUnaryMinusOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->getExp(),
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_NULL_COAL(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreNullCoalescingOperatorNode>(ctx);
}

static bool write_slot_VALUE_COAL(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreValueCoalescingOperatorNode>(ctx);
}

static bool write_slot_QUESTION(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->get(0),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_FOLDL(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QoreFoldrOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreFoldlOperatorNode>(ctx);
}

static bool write_slot_FOLDR(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreFoldrOperatorNode>(ctx);
}

static bool write_slot_MAP(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreMapOperatorNode>(ctx);
}

static bool write_slot_MAP_SELECT(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMapSelectOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->get(0),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_HASH_MAP(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreHashMapOperatorNode*>(node);
    if (!op || dynamic_cast<const QoreHashMapSelectOperatorNode*>(node)) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->get(0),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_HASH_MAP_SELECT(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* op = dynamic_cast<const QoreHashMapSelectOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, op->get(0),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(3),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_slot_SELECT(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreSelectOperatorNode>(ctx);
}

static bool write_slot_RANGE(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QoreRangeOperatorNode>(ctx);
}

static bool write_slot_ASSIGN(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QoreWeakAssignmentOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreAssignmentOperatorNode>(ctx);
}

static bool write_slot_TRIM(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreTrimOperatorNode>(ctx);
}

static bool write_slot_CHOMP(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreChompOperatorNode>(ctx);
}

static bool write_slot_POP(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QorePopOperatorNode>(ctx);
}

static bool write_slot_SHIFT(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QorePopOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_unary_slot_payload<QoreShiftOperatorNode>(ctx);
}

static bool write_slot_PUSH(AOTExprSlotWriteCtx& ctx) {
    return write_binary_slot_payload<QorePushOperatorNode>(ctx);
}

static bool write_slot_UNSHIFT(AOTExprSlotWriteCtx& ctx) {
    if (dynamic_cast<const QorePushOperatorNode*>(
            ctx.expr.child_expr.getInternalNode())) {
        return false;
    }
    return write_binary_slot_payload<QoreUnshiftOperatorNode>(ctx);
}

static bool write_slot_ELEMENTS(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreElementsOperatorNode>(ctx);
}

static bool write_slot_DELETE(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreDeleteOperatorNode>(ctx);
}

static bool write_slot_REMOVE(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreRemoveOperatorNode>(ctx);
}

static bool write_slot_BACKGROUND(AOTExprSlotWriteCtx& ctx) {
    return write_unary_slot_payload<QoreBackgroundOperatorNode>(ctx);
}

static bool write_slot_CONTEXT_REF(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* cr = dynamic_cast<const ContextrefNode*>(node);
    if (!cr && ctx.expr.ref1.empty()) {
        return false;
    }
    ctx.writer.writeStringRef(cr && cr->str ? cr->str : ctx.expr.ref1.c_str());
    return true;
}

static bool write_slot_CONTEXT_ROW(AOTExprSlotWriteCtx& ctx) {
    return dynamic_cast<const ContextRowNode*>(ctx.expr.child_expr.getInternalNode()) != nullptr;
}

static bool write_slot_COMPLEX_CONTEXT_REF(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* ccr = dynamic_cast<const ComplexContextrefNode*>(node);
    if (!ccr) {
        return false;
    }
    ctx.writer.writeStringRef(ccr->name ? ccr->name : "");
    ctx.writer.writeStringRef(ccr->member ? ccr->member : "");
    ctx.writer.writeI64(static_cast<int64_t>(ccr->stack_offset));
    return true;
}

//! HASH_DEREF: left(AOTExprKind) + right(AOTExprKind)
static bool write_slot_HASH_DEREF(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!hd) {
        return false;
    }
    return classifyAndWriteExpr(ctx.writer, hd->getLeft(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, hd->getRight(),
            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

//! LIST_LITERAL: count(u8) + [value(AOTExprKind)] * N
static bool write_slot_LIST_LITERAL(AOTExprSlotWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.child_expr.getInternalNode();
    if (auto* qln = dynamic_cast<const QoreListNode*>(node)) {
        if (qln->size() > 255) {
            return false;
        }
        ctx.writer.writeU8(static_cast<uint8_t>(qln->size()));
        for (size_t i = 0; i < qln->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT list literal serialization")) {
                return false;
            }
            if (!write_slot_inline_expr(ctx, qln->retrieveEntry(i))) {
                return false;
            }
        }
        return true;
    }
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
        if (pln->size() > 255) {
            return false;
        }
        ctx.writer.writeU8(static_cast<uint8_t>(pln->size()));
        for (size_t i = 0; i < pln->size(); ++i) {
            if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT parse-list literal serialization")) {
                return false;
            }
            if (!write_slot_inline_expr(ctx, pln->get(i))) {
                return false;
            }
        }
        return true;
    }
    return false;
}

//! CAST_HASHDECL/CAST_COMPLEX_HASH/CAST_COMPLEX_LIST/CAST_CLASS/CAST_ENUM:
//! ref1 = type/class path, flags bit0 = or_nothing
static bool write_slot_CAST(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeU8(ctx.expr.flags);
    return true;
}

//! SELF_METHOD_CALL: ref1 = class path, ref2 = method name
static bool write_slot_SELF_METHOD_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! STATIC_METHOD_CALL: ref1 = class path, ref2 = method name
static bool write_slot_STATIC_METHOD_CALL(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    if ((ctx.writer.feature_flags & QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE) != 0) {
        ctx.writer.writeStringRef(ctx.expr.ref3.c_str());
    }
    // Serialize method args (may contain sub-expressions like string constants)
    return write_slot_args_prefer_call(ctx);
}

//! STATIC_VARREF: ref1 = class path, ref2 = var name
static bool write_slot_STATIC_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! CONST_ENUM: ref1 = enum path, ref2 = member name
static bool write_slot_CONST_ENUM(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());
    return true;
}

//! SELF_VARREF: ref1 = member name
static bool write_slot_SELF_VARREF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return true;
}

//! CLOSURE_CREATE: closure signature, params, captured vars, and IR code
static bool write_slot_CLOSURE_CREATE(AOTExprSlotWriteCtx& ctx) {
    // Write flags and class type path
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());  // "lambda,in_method"
    ctx.writer.writeStringRef(ctx.expr.ref2.c_str());  // class_type_path

    // Serialize closure signature metadata
    const UserClosureFunction* ucf = ctx.expr.closure_func;
    if (!ucf) {
        return false;
    }
    auto* variant = static_cast<const UserClosureVariant*>(ucf->first());
    const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();

    // Write return type
    std::string ret_path = qore_get_aot_serializable_type_path(sig->getReturnTypeInfo());
    ctx.writer.writeStringRef(ret_path.c_str());

    // Write params: count, then (name, type_path, default) per param
    unsigned num_params = sig->numParams();
    ctx.writer.writeU16(static_cast<uint16_t>(num_params));
    for (unsigned p = 0; p < num_params; ++p) {
        const char* pname = sig->getName(p);
        ctx.writer.writeStringRef(pname ? pname : "");
        std::string ptype = qore_get_aot_serializable_type_path(sig->getParamTypeInfo(p));
        ctx.writer.writeStringRef(ptype.c_str());
        // Default value: write has_default flag + value if present
        bool has_default = sig->hasDefaultArg(p);
        ctx.writer.writeU8(has_default ? 1 : 0);
        if (has_default) {
            std::string default_error;
            if (!qoreAOTWriteDefaultArgValuePayload(ctx.writer, sig->getDefaultArgList()[p],
                    "closure", ucf->getName(), pname, &default_error)) {
                return false;
            }
        }
    }
    uint16_t closure_flags = 0;
    if (variant->hasVarargs()) {
        closure_flags |= 0x0001;
    }
    if (sig->hasVarargs()) {
        closure_flags |= 0x0004;
    }
    ctx.writer.writeU16(closure_flags);

    // Lower closure before writing captures so embedded expression payloads
    // can contribute parent locals not present in the parser's closure vlist.
    const QoreIRFunction* closure_ir = const_cast<UserClosureVariant*>(variant)->getCachedIR();
    QoreIRFunction* owned_ir = nullptr;
    if (!closure_ir) {
        // Lower on the fly for serialization
        owned_ir = ::lowerClosureForSerialization(variant);
        closure_ir = owned_ir;
    }

    const LVarSet* vlist = const_cast<UserClosureFunction*>(ucf)->getVList();
    ::qoreAOTPruneClosureIRBodyLocals(const_cast<QoreIRFunction*>(closure_ir), sig, vlist);
    if (!::qoreAOTWriteClosureCaptures(ctx.writer, vlist, closure_ir, ctx.parent_locals)) {
        delete owned_ir;
        return false;
    }

    if (closure_ir) {
        ctx.writer.writeU8(1);  // has_ir
        uint32_t size_pos = ctx.writer.position();
        ctx.writer.writeU32(0);  // placeholder

        // Expression trees inside serialized closure IR use the closure IR's
        // own local slot IDs.  The reader fills closure_locals_vec from the
        // serialized local slot table before instruction expressions are read.
        std::vector<AOTLocalSlotId> closure_locals;
        uint32_t max_slot = 0;
        bool has_slots = false;
        for (const auto& [lv, slot_id] : closure_ir->local_var_slots) {
            if (lv) {
                if (!has_slots || slot_id > max_slot) {
                    max_slot = slot_id;
                }
                has_slots = true;
            }
        }
        if (has_slots) {
            closure_locals.resize(static_cast<size_t>(max_slot) + 1);
            for (const auto& [lv, slot_id] : closure_ir->local_var_slots) {
                if (lv) {
                    AOTLocalSlotId& slot = closure_locals[slot_id];
                    slot.local_var_ptr = reinterpret_cast<const void*>(lv);
                    slot.name = lv->getName() ? lv->getName() : "";
                }
            }
        }

        auto writeExpr = [&closure_locals, &ctx](
                QoreAOTBinaryWriter& w, const QoreValue& e) -> bool {
            return classifyAndWriteExpr(w, e, closure_locals, ctx.parent_globals,
                ctx.const_reverse_map);
        };

        ::serializeIRFunction(ctx.writer, *closure_ir, writeExpr);
        uint32_t end_pos = ctx.writer.position();
        ctx.writer.patchU32(size_pos, end_pos - size_pos - 4);
    } else {
        delete owned_ir;
        return false;
    }
    delete owned_ir;
    return true;
}

//! CALL_REF: no additional data
static bool write_slot_CALL_REF(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! OBJ_METHOD_REF: no additional data
static bool write_slot_OBJ_METHOD_REF(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;  // Unused
    return true;
}

//! PARSE_REF: reference type path + inline lvalue expression
static bool write_slot_PARSE_REF(AOTExprSlotWriteCtx& ctx) {
    ctx.writer.writeStringRef(ctx.expr.ref1.c_str());
    return ::classifyAndWriteExpr(ctx.writer, ctx.expr.child_expr,
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

//! EXPR_TREE: read-compatible legacy marker; new AOT output must not emit it
static bool write_slot_EXPR_TREE(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;
    return false;
}

//! GENERIC_EVAL: no additional data — unsupported marker rejected by AOT compilation
static bool write_slot_GENERIC_EVAL(AOTExprSlotWriteCtx& ctx) {
    (void)ctx;
    return false;
}

}  // namespace
