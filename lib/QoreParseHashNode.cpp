/* -*- indent-tabs-mode: nil -*- */
/*
    QoreParseHashNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/typed_hash_decl_private.h"

static bool qore_parse_hash_literal_varref_may_be_nothing(const QoreValue& v) {
    if (v.getType() != NT_VARREF) {
        return false;
    }
    const VarRefNode* vr = v.get<const VarRefNode>();
    qore_var_t vt = vr->getType();
    if ((vt == VT_LOCAL || vt == VT_CLOSURE || vt == VT_LOCAL_TS) && vr->ref.id) {
        return true;
    }
    if ((vt == VT_GLOBAL || vt == VT_THREAD_LOCAL) && vr->ref.var) {
        return true;
    }
    return false;
}

void QoreParseHashNode::finalizeBlock(int sline, int eline) {
    QoreProgramLocation tl(sline, eline);
    if (tl.getFile() == loc->getFile()
        && tl.getSource() == loc->getSource()
        && (sline != loc->start_line || eline != loc->end_line)) {
        loc = qore_program_private::get(*getProgram())->getLocation(*loc, sline, eline);
    }
}

int QoreParseHashNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(keys.size() == values.size());
    bool needs_eval = false;

    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);

    // initialize value type vector
    vtypes.resize(keys.size());

    // try to find a common value type, if any
    bool vcommon = false;

    const QoreTypeInfo* expected_hash_type = parse_context.expected_type_info;
    const QoreTypeInfo* expected_hash_value_type = expected_hash_type
        ? QoreTypeInfo::getUniqueReturnComplexHash(expected_hash_type)
        : nullptr;
    if (!expected_hash_value_type && expected_hash_type) {
        expected_hash_value_type = QoreTypeInfo::getReturnComplexHashOrNothing(expected_hash_type);
    }
    const TypedHashDecl* expected_hash_decl = expected_hash_type
        ? QoreTypeInfo::getTypedHash(expected_hash_type)
        : nullptr;

    int err = 0;

    for (size_t i = 0; i < keys.size(); ++i) {
        QoreValue p = keys[i];
        parse_context.typeInfo = nullptr;
        parse_context.expected_type_info = nullptr;
        if (parse_init_value(keys[i], parse_context) && !err) {
            err = -1;
        }
        const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;

        if (!p.isEqualValue(keys[i]) && (!keys[i] || keys[i].isValue())) {
            QoreStringValueHelper key(keys[i]);
            checkDup(lvec[i], key->c_str());
        } else if (!needs_eval && keys[i] && keys[i].needsEval()) {
            needs_eval = true;
        }

        if (!QoreTypeInfo::canConvertToScalar(argTypeInfo)) {
            QoreStringMaker str("key number %ld (starting from 0) in the hash is ", i);
            argTypeInfo->doNonStringWarning(lvec[i], str.c_str());
        }

        const QoreTypeInfo* expected_value_type = expected_hash_value_type;
        if (expected_hash_decl && keys[i].getType() == NT_STRING) {
            QoreStringValueHelper key(keys[i]);
            const HashDeclMemberInfo* m = typed_hash_decl_private::get(*expected_hash_decl)->findMember(key->c_str());
            if (m) {
                expected_value_type = m->getTypeInfo();
            }
        }

        bool typed_varref_may_be_nothing = false;
        typed_varref_may_be_nothing = qore_parse_hash_literal_varref_may_be_nothing(values[i]);

        parse_context.typeInfo = nullptr;
        parse_context.expected_type_info = expected_value_type;
        if (parse_init_value(values[i], parse_context) && !err) {
            err = -1;
        }
        vtypes[i] = parse_context.typeInfo;
        typed_varref_may_be_nothing = typed_varref_may_be_nothing
            || qore_parse_hash_literal_varref_may_be_nothing(values[i]);

        // For variable refs, clear type info to prevent baking declared or
        // narrowed types into the hash literal; the actual runtime value may
        // be NOTHING, so the type must be determined at runtime.
        if (vtypes[i] && typed_varref_may_be_nothing) {
            vtypes[i] = nullptr;
        }
        if (vtypes[i] && expected_value_type
                && QoreTypeInfo::parseAcceptsReturns(expected_value_type, NT_NOTHING)
                && values[i].needsEval()) {
            vtypes[i] = nullptr;
        }

        //printd(5, "QoreParseHashNode::parseInitImpl() this: %p i: %d key type '%s': value type '%s'\n",
        //    this, i, keys[i].getFullTypeName(), values[i].getFullTypeName());

        if (!i) {
            if (vtypes[0] && vtypes[0] != anyTypeInfo) {
                vtype = vtypes[0];
                vcommon = true;
            }
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vtypes[i])) {
            vcommon = false;
        }

        if (!needs_eval && values[i].needsEval()) {
            needs_eval = true;
        }
    }

    kmap.clear();
    parse_context.expected_type_info = expected_hash_type;

    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    if (vtype && vtype != anyTypeInfo) {
        typeInfo = parse_context.typeInfo = qore_get_complex_hash_type(vtype);
    } else if (expected_hash_value_type) {
        // Inference would otherwise lock to autoHashTypeInfo; use the
        // lvalue's expected hash value-type as the narrowing target
        // when the caller supplied a hint.  Runtime coercion (softint,
        // softstring, per-value acceptInputKey softening) happens
        // during the hash store path, so adopting the expected type
        // is safe: if values don't fit at runtime, the existing accept
        // logic raises.  Keep the full hash value type, including
        // or-nothing element types such as `hash<string, *hash<auto>>`;
        // otherwise valid optional hash slots reject NOTHING.
        // See design/parser-lvalue-type-propagation.md.
        vtype = expected_hash_value_type;
        typeInfo = parse_context.typeInfo = qore_get_complex_hash_type(expected_hash_value_type);
    } else {
        typeInfo = autoHashTypeInfo;
        // issue #3740: must set to auto type info to avoid type stripping
        vtype = autoTypeInfo;
        // issue #2647: allow an empty hash to be assigned to any complex hash (but not hashdecls)
        // it will get folded at runtime into the desired type in any case
        parse_context.typeInfo = vtypes.empty() ? emptyHashTypeInfo : autoHashTypeInfo;
    }

    printd(5, "QoreParseHashNode::parseInitImpl() this: %p type: %s (%s)\n", this,
        QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(typeInfo));

    if (err) {
        parse_error = true;
        return err;
    }

    if (needs_eval) {
        return 0;
    }

    // evaluate immediately
    SimpleRefHolder<QoreParseHashNode> holder(this);
    ExceptionSink xsink;
    ValueEvalOptimizedRefHolder rv(this, &xsink);
    assert(!xsink);
    QoreValue result = rv.takeReferencedValue();
    // only use parse-time folding if we got a valid result
    // (constants may not be fully resolved at parse time, resulting in NOTHING)
    if (!result.isNothing()) {
        val = result;
        parse_context.typeInfo = val.getFullTypeInfo();
        return 0;
    }
    // constants not resolved - skip parse-time folding, let runtime handle it
    holder.release();
    return 0;
}

QoreValue QoreParseHashNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(keys.size() == values.size());

    // Use the parse-time narrowed value type when available so
    // setKeyValue triggers per-value softening (softint, softstring,
    // ...) on insert — matches what the IR lowering path already
    // does via `createMakeHashConstKeys(..., parse_ti)` at
    // lib/QoreIRLowering.cpp:7869.  Without this, AST-mode execution
    // would build a hash<auto> at runtime even when the parser
    // narrowed the node's typeInfo via the lvalue-hint channel — see
    // design/parser-lvalue-type-propagation.md.  Falls back to
    // autoTypeInfo when the parser left vtype as auto/null/any, and
    // the tail block below derives complexTypeInfo from runtime-
    // inferred values as before (issue #2106).
    const bool parse_time_narrowed = (this->vtype
        && this->vtype != autoTypeInfo
        && this->vtype != anyTypeInfo);

    ReferenceHolder<QoreHashNode> h(
        new QoreHashNode(parse_time_narrowed ? this->vtype : autoTypeInfo),
        xsink);

    // issue #2106 we must calculate the runtime type again because lvalues can return NOTHING despite their declared
    // type
    const QoreTypeInfo* vtype = nullptr;
    // try to find a common value type, if any
    bool vcommon = false;

    for (size_t i = 0; i < keys.size(); ++i) {
        ValueEvalOptimizedRefHolder k(keys[i], xsink);
        if (xsink && *xsink) {
            needs_deref = false;
            return QoreValue();
        }

        ValueEvalOptimizedRefHolder v(values[i], xsink);
        if (xsink && *xsink) {
            needs_deref = false;
            return QoreValue();
        }

        const QoreTypeInfo* vt = v->getFullTypeInfo();

        if (!i) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }

        QoreStringValueHelper key(*k);

        // issue #2791: ensure that type folding is performed at the source if necessary
        QoreValue val = v.takeReferencedValue();
        //printd(5, "QoreParseHashNode::evalImpl() '%s' this->vtype: '%s' (c: %d) vt: '%s' (c: %d)\n",
        //  key->c_str(), QoreTypeInfo::getName(this->vtype), QoreTypeInfo::hasComplexType(this->vtype),
        //  QoreTypeInfo::getName(vt), QoreTypeInfo::hasComplexType(vt));
        if (this->vtype && this->vtype != vt && !QoreTypeInfo::hasComplexType(this->vtype)
                && QoreTypeInfo::hasComplexType(vt)) {
            // this can never throw an exception; it's only used for type folding/stripping
            QoreTypeInfo::acceptInputKey(this->vtype, key->c_str(), val, xsink);
        }

        h->setKeyValue(key->c_str(), val, xsink);
        if (xsink && *xsink) {
            needs_deref = false;
            return QoreValue();
        }
    }

    ValueHolder rv(h.release(), xsink);

    // When the parser narrowed the value type, the hash was
    // constructed with the narrowed `complexTypeInfo` and
    // setKeyValue already softened values on insert — preserving it
    // here keeps AST-mode behaviour aligned with IR/AOT.  Otherwise
    // fall back to deriving from the runtime-inferred vtype (issue
    // #2791: use "auto" rather than "any").
    if (!parse_time_narrowed) {
        if (!vtype || vtype == anyTypeInfo) {
            vtype = autoTypeInfo;
        }
        const QoreTypeInfo* ti = qore_get_complex_hash_type(vtype);
        qore_hash_private::get(*rv->get<QoreHashNode>())->complexTypeInfo = ti;
    }

    return rv.release();
}
