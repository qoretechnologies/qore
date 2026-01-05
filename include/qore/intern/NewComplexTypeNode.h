/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    NewComplexTypeNode.h

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

#ifndef _QORE_INTERN_NEWCOMPLEXTYPENODE_H

#define _QORE_INTERN_NEWCOMPLEXTYPENODE_H

#include "qore/intern/FunctionCallNode.h"

class ParseNewComplexTypeNode : public ParseNoEvalNode {
public:
    DLLLOCAL ParseNewComplexTypeNode(const QoreProgramLocation* loc, QoreParseTypeInfo* pti, QoreParseListNode* a)
            : ParseNoEvalNode(loc, NT_PARSE_NEW_COMPLEX_TYPE), pti(pti), args(a) {
    }

    DLLLOCAL ~ParseNewComplexTypeNode() {
        if (args)
            args->deref(nullptr);
        delete pti;
    }

    // do not know type until resolution
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return anyTypeInfo;
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new complex type operator expression ('%s')", QoreParseTypeInfo::getName(pti));
        return 0;
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "new complex type operator expression";
    }

protected:
    QoreParseTypeInfo* pti;
    QoreParseListNode* args;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
        assert(false);
        return QoreValue();
    }

    DLLLOCAL QoreParseListNode* takeArgs() {
        QoreParseListNode* rv = args;
        args = nullptr;
        return rv;
    }
};

class NewHashDeclNode : public ParseNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return hd ? hd->getTypeInfo() : hashTypeInfo;
    }

public:
    const TypedHashDecl* hd;
    QoreParseListNode* args;
    bool runtime_check;

    DLLLOCAL NewHashDeclNode(const QoreProgramLocation* loc, const TypedHashDecl* hd, QoreParseListNode* a,
            bool runtime_check) : ParseNode(loc, NT_SCOPE_REF), hd(hd), args(a), runtime_check(runtime_check) {
    }

    DLLLOCAL virtual ~NewHashDeclNode() {
        if (args)
            args->deref(nullptr);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new hashdecl operator expression (hashdecl '%s')", hd->getName());
        return 0;
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "new hashdecl operator expression";
    }
};

class NewComplexHashNode : public ParseNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    const QoreTypeInfo* typeInfo;
    QoreParseListNode* args;

    DLLLOCAL NewComplexHashNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo, QoreParseListNode* a)
            : ParseNode(loc, NT_SCOPE_REF), typeInfo(typeInfo), args(a) {
        assert(QoreTypeInfo::getUniqueReturnComplexHash(typeInfo));
    }

    DLLLOCAL virtual ~NewComplexHashNode() {
        if (args)
            args->deref(nullptr);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new complex hash operator expression ('%s')", QoreTypeInfo::getName(typeInfo));
        return 0;
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "new complex hash operator expression";
    }
};

class NewComplexListNode : public ParseNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    const QoreTypeInfo* typeInfo;
    QoreValue args{};

    DLLLOCAL NewComplexListNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo, QoreValue a)
            : ParseNode(loc, NT_SCOPE_REF), typeInfo(typeInfo), args(a) {
        assert(QoreTypeInfo::getUniqueReturnComplexList(typeInfo));
    }

    DLLLOCAL virtual ~NewComplexListNode() {
        args.discard(nullptr);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new complex list operator expression ('%s')", QoreTypeInfo::getName(typeInfo));
        return 0;
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "new complex list operator expression";
    }
};

#endif
