/* -*- indent-tabs-mode: nil -*- */
/*
    NewComplexTypeNode.cpp

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
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"

int ParseNewComplexTypeNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    int err = 0;
    parse_context.typeInfo = QoreParseTypeInfo::resolveAndDelete(pti, loc, err);
    pti = nullptr;

    {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(parse_context.typeInfo);
        if (qc) {
            ReferenceHolder<> holder(this, nullptr);
            val = new ScopedObjectCallNode(loc, qc, takeArgs());
            //return parse_init_value(val, parse_context) || err ? -1 : 0;
            if (parse_init_value(val, parse_context) && !err) {
                err = -1;
            }
            return err;
        }
    }
    {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(parse_context.typeInfo);
        if (hd) {
            ReferenceHolder<> holder(this, nullptr);
            bool runtime_check;
            const QoreTypeInfo* returnTypeInfo = parse_context.typeInfo;
            if (typed_hash_decl_private::get(*hd)->parseInitHashDeclInitialization(loc, parse_context, args,
                runtime_check) && !err) {
                err = -1;
            }
            parse_context.typeInfo = returnTypeInfo;
            val = new NewHashDeclNode(loc, hd, takeArgs(), runtime_check);
            return err;
        }
    }
    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexHash(parse_context.typeInfo);
        if (ti) {
            ReferenceHolder<> holder(this, nullptr);
            const QoreTypeInfo* returnTypeInfo = parse_context.typeInfo;
            parse_context.typeInfo = ti;
            if (qore_hash_private::parseInitComplexHashInitialization(loc, parse_context, args) && !err) {
                err = -1;
            }
            parse_context.typeInfo = returnTypeInfo;
            val = new NewComplexHashNode(loc, parse_context.typeInfo, takeArgs());
            return err;
        }
    }
    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexList(parse_context.typeInfo);
        if (ti) {
            ReferenceHolder<> holder(this, nullptr);
            QoreValue new_args{};
            const QoreTypeInfo* returnTypeInfo = parse_context.typeInfo;
            parse_context.typeInfo = ti;
            if (qore_list_private::parseInitComplexListInitialization(loc, parse_context, takeArgs(), new_args)
                && !err) {
                err = -1;
            }
            parse_context.typeInfo = returnTypeInfo;
            val = new NewComplexListNode(loc, parse_context.typeInfo, new_args);
            return err;
        }
    }

    if (!err) {
        parse_error(*loc, "type '%s' does not support instantiation with the new operator",
            QoreTypeInfo::getName(parse_context.typeInfo));
    }
    return -1;
}

QoreValue NewHashDeclNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return typed_hash_decl_private::get(*hd)->newHash(rc, args, runtime_check, xsink);
}

QoreValue NewComplexHashNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return qore_hash_private::newComplexHash(rc, typeInfo, args, xsink);
}

QoreValue NewComplexListNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return qore_list_private::newComplexList(rc, typeInfo, args, xsink);
}
