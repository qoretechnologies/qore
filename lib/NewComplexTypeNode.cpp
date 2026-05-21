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
            const QoreTypeInfo* object_type_info = parse_context.typeInfo;
            val = new ScopedObjectCallNode(loc, qc, takeArgs(), object_type_info);
            parse_context.typeInfo = nullptr;
            parse_context.analysis.clear();
            if (parse_init_value(val, parse_context) && !err) {
                err = -1;
            }
            return err;
        }
    }
    {
        // Check complex hash BEFORE hashdecl, since hash<HashdeclType> has both
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
    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexBuffer(parse_context.typeInfo);
        if (ti) {
            ReferenceHolder<> holder(this, nullptr);
            QoreValue new_args{};
            const QoreTypeInfo* returnTypeInfo = parse_context.typeInfo;
            parse_context.typeInfo = nullptr;
            int init_err = 0;
            if (!qore_list_private::parseInitListInitialization(loc, parse_context, takeArgs(), new_args, init_err)) {
                const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(ti);
                const QoreTypeInfo* element_type = bti ? bti->getElementTypeInfo() : autoTypeInfo;
                if (new_args.getType() == NT_PARSE_LIST) {
                    const QoreParseListNode* pln = new_args.get<const QoreParseListNode>();
                    if (pln->size() > 1) {
                        parseException(*loc, "PARSE-TYPE-ERROR", "cannot initialize '%s' with %d arguments; "
                            "buffer<T> construction takes zero arguments or exactly one list argument",
                            QoreTypeInfo::getName(returnTypeInfo), (int)pln->size());
                        init_err = -1;
                    } else if (pln->size() == 1) {
                        const QoreTypeInfo* arg_type = pln->getValueTypes()[0];
                        if (QoreTypeInfo::parseReturns(arg_type, NT_LIST) == QTI_NOT_EQUAL) {
                            parseException(*loc, "PARSE-TYPE-ERROR", "cannot initialize '%s' with argument type "
                                "'%s'; expected a list argument", QoreTypeInfo::getName(returnTypeInfo),
                                QoreTypeInfo::getName(arg_type));
                            init_err = -1;
                        } else if (qore_list_private::parseCheckComplexListInitialization(loc, element_type,
                                arg_type, pln->get(0), "initialize buffer", true)) {
                            init_err = -1;
                        }
                    }
                }
            }
            if (init_err && !err) {
                err = -1;
            }
            parse_context.typeInfo = returnTypeInfo;
            val = new NewComplexBufferNode(loc, parse_context.typeInfo, new_args);
            return err;
        }
    }

    if (!err) {
        parse_error(*loc, "type '%s' does not support instantiation with the new operator",
            QoreTypeInfo::getName(parse_context.typeInfo));
    }
    return -1;
}

QoreValue NewHashDeclNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return typed_hash_decl_private::get(*hd)->newHash(args, runtime_check, xsink);
}

QoreValue NewComplexHashNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return qore_hash_private::newComplexHash(typeInfo, args, xsink);
}

QoreValue NewComplexListNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return qore_list_private::newComplexList(typeInfo, args, xsink);
}

static int qore_get_buffer_factory_size(const QoreListNode* args_list, const char* factory, ExceptionSink* xsink,
        size_t& size) {
    QoreValue size_arg = args_list->retrieveEntry(0);
    if (size_arg.getType() != NT_INT) {
        xsink->raiseException("BUFFER-CONSTRUCTOR-ERROR", "buffer<T>::%s() expects argument 'size' to be an int; "
            "got type '%s'", factory, size_arg.getFullTypeName());
        return -1;
    }

    int64 count = size_arg.getAsBigInt();
    if (count < 0) {
        xsink->raiseException("BUFFER-SIZE-ERROR", "buffer<T>::%s() expects argument 'size' to be >= 0; got " QLLD,
            factory, count);
        return -1;
    }
    size = static_cast<size_t>(count);
    return 0;
}

static const QoreListNode* qore_get_buffer_factory_args(QoreValue val, const char* factory, size_t expected_args,
        ExceptionSink* xsink) {
    if (val.getType() != NT_LIST) {
        xsink->raiseException("BUFFER-CONSTRUCTOR-ERROR", "buffer<T>::%s() expects %d argument%s; got type '%s'",
            factory, (int)expected_args, expected_args == 1 ? "" : "s", val.getFullTypeName());
        return nullptr;
    }

    const QoreListNode* args_list = val.get<const QoreListNode>();
    if (args_list->size() != expected_args) {
        xsink->raiseException("BUFFER-CONSTRUCTOR-ERROR", "buffer<T>::%s() expects %d argument%s; got %d",
            factory, (int)expected_args, expected_args == 1 ? "" : "s", (int)args_list->size());
        return nullptr;
    }
    return args_list;
}

QoreBufferNode* qore_new_complex_buffer_from_value(const QoreTypeInfo* typeInfo, QoreValue val,
        ExceptionSink* xsink, QoreComplexBufferInitKind initKind) {
    const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(typeInfo);
    if (!bti) {
        xsink->raiseException("BUFFER-TYPE-ERROR", "cannot construct invalid buffer type '%s'",
            QoreTypeInfo::getName(typeInfo));
        return nullptr;
    }

    ValueHolder holder(val, xsink);
    if (initKind == QoreComplexBufferInitKind::Sized || initKind == QoreComplexBufferInitKind::Filled) {
        const char* factory = initKind == QoreComplexBufferInitKind::Sized ? "sized" : "filled";
        const QoreListNode* args_list = qore_get_buffer_factory_args(val, factory,
            initKind == QoreComplexBufferInitKind::Sized ? 1 : 2, xsink);
        if (!args_list) {
            return nullptr;
        }

        size_t size = 0;
        if (qore_get_buffer_factory_size(args_list, factory, xsink, size)) {
            return nullptr;
        }

        ReferenceHolder<QoreBufferNode> rv(new QoreBufferNode(bti->getBufferElementType(), bti->hasNullableElements(),
            size), xsink);
        if (initKind == QoreComplexBufferInitKind::Filled) {
            QoreValue fill_value = args_list->retrieveEntry(1);
            for (size_t i = 0; i < size; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "buffer filled construction")) {
                    return nullptr;
                }
                if ((*rv)->setEntry(i, fill_value, xsink)) {
                    return nullptr;
                }
            }
        }
        return rv.release();
    }

    if (val.isNothing()) {
        return new QoreBufferNode(bti->getBufferElementType(), bti->hasNullableElements());
    }

    if (val.getType() != NT_LIST) {
        xsink->raiseException("BUFFER-CONSTRUCTOR-ERROR", "buffer<T> construction expects zero arguments or exactly "
            "one list argument; got type '%s'", val.getFullTypeName());
        return nullptr;
    }

    const QoreListNode* args_list = val.get<const QoreListNode>();
    if (args_list->empty()) {
        return new QoreBufferNode(bti->getBufferElementType(), bti->hasNullableElements());
    }
    if (args_list->size() != 1 || args_list->retrieveEntry(0).getType() != NT_LIST) {
        xsink->raiseException("BUFFER-CONSTRUCTOR-ERROR", "buffer<T> construction expects zero arguments or exactly "
            "one list argument; got %d argument%s", (int)args_list->size(), args_list->size() == 1 ? "" : "s");
        return nullptr;
    }

    return new QoreBufferNode(bti->getBufferElementType(), bti->hasNullableElements(),
        args_list->retrieveEntry(0).get<const QoreListNode>(), xsink);
}

static QoreBufferNode* qore_new_complex_buffer(const QoreTypeInfo* typeInfo, const QoreValue args,
        ExceptionSink* xsink, QoreComplexBufferInitKind initKind) {
    QoreValue val{};
    if (!args.isNothing()) {
        ValueEvalOptimizedRefHolder a(args, xsink);
        if (*xsink) {
            return nullptr;
        }
        val = a.takeReferencedValue();
    }
    return qore_new_complex_buffer_from_value(typeInfo, val, xsink, initKind);
}

QoreValue NewComplexBufferNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return qore_new_complex_buffer(typeInfo, args, xsink, initKind);
}
