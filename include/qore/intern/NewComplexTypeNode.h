/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    NewComplexTypeNode.h

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

#ifndef _QORE_INTERN_NEWCOMPLEXTYPENODE_H

#define _QORE_INTERN_NEWCOMPLEXTYPENODE_H

#include "qore/intern/FunctionCallNode.h"

class QoreBufferNode;

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

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(false);
        return QoreValue();
    }

    DLLLOCAL QoreParseListNode* takeArgs() {
        QoreParseListNode* rv = args;
        args = nullptr;
        return rv;
    }
};

enum class QoreComplexBufferInitKind : uint8_t {
    Constructor = 0,
    Sized = 1,
    Filled = 2,
};

class NewHashDeclNode : public ParseNode {
protected:
    template <bool Lexical>
    DLLLOCAL QoreValue evalImplInternal(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return hd ? hd->getTypeInfo() : (typeInfo ? typeInfo : hashTypeInfo);
    }

public:
    const TypedHashDecl* hd;
    QoreParseListNode* args;
    const QoreTypeInfo* typeInfo = nullptr;
    std::string dynamic_hashdecl_name;
    bool runtime_check;

    DLLLOCAL NewHashDeclNode(const QoreProgramLocation* loc, const TypedHashDecl* hd, QoreParseListNode* a,
            bool runtime_check) : ParseNode(loc, NT_SCOPE_REF), hd(hd), args(a),
            typeInfo(hd ? hd->getTypeInfo() : nullptr), runtime_check(runtime_check) {
    }

    //! Creates a construction of an AOT build-group hashdecl deferred at parse time
    /** The hashdecl is resolved by name when the hash is constructed; the initializer's keys could not be checked
        against it when parsed, so they are always checked then.
    */
    DLLLOCAL NewHashDeclNode(const QoreProgramLocation* loc, const char* dynamic_hashdecl_name,
            const QoreTypeInfo* typeInfo, QoreParseListNode* a)
            : ParseNode(loc, NT_SCOPE_REF), hd(nullptr), args(a), typeInfo(typeInfo),
            dynamic_hashdecl_name(dynamic_hashdecl_name ? dynamic_hashdecl_name : ""), runtime_check(true) {
    }

    //! Creates the copy of a background expression, with the arguments taken when the thread starts
    /** See copy_value_and_resolve_lvar_refs(); the new thread evaluates the copy
    */
    DLLLOCAL NewHashDeclNode(const NewHashDeclNode& old, QoreParseListNode* a) : ParseNode(old), hd(old.hd),
            args(a), typeInfo(old.typeInfo), dynamic_hashdecl_name(old.dynamic_hashdecl_name),
            runtime_check(old.runtime_check) {
    }

    DLLLOCAL virtual ~NewHashDeclNode() {
        if (args)
            args->deref(nullptr);
    }

    //! Releases the arguments with the caller's exception sink
    /** A background copy holds runtime values, whose release can run destructors; see QoreParseListNode::derefImpl()
    */
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        if (args) {
            args->deref(xsink);
            args = nullptr;
        }
        return ParseNode::derefImpl(xsink);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new hashdecl operator expression (hashdecl '%s')",
            hd ? hd->getName() : dynamic_hashdecl_name.c_str());
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

    DLLLOCAL bool isDynamicHashDeclConstruct() const {
        return !hd && !dynamic_hashdecl_name.empty();
    }

    DLLLOCAL const std::string& getDynamicHashDeclName() const {
        return dynamic_hashdecl_name;
    }
};

class NewComplexHashNode : public ParseNode {
protected:
    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    const QoreTypeInfo* typeInfo;
    QoreParseListNode* args;
    //! true if the type has type parameters (generic code), which are substituted when the node is evaluated
    /** decided once: a non-generic `new hash<string, int>()` pays nothing for the substitution
    */
    bool needs_substitution;

    DLLLOCAL NewComplexHashNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo, QoreParseListNode* a)
            : ParseNode(loc, NT_SCOPE_REF), typeInfo(typeInfo), args(a),
            needs_substitution(qore_type_contains_type_parameter(typeInfo)) {
        assert(QoreTypeInfo::getUniqueReturnComplexHash(typeInfo));
    }

    //! Creates the copy of a background expression, with the arguments taken when the thread starts
    /** See copy_value_and_resolve_lvar_refs(); the new thread evaluates the copy
    */
    DLLLOCAL NewComplexHashNode(const NewComplexHashNode& old, QoreParseListNode* a) : ParseNode(old),
            typeInfo(old.typeInfo), args(a), needs_substitution(old.needs_substitution) {
    }

    DLLLOCAL virtual ~NewComplexHashNode() {
        if (args)
            args->deref(nullptr);
    }

    //! Releases the arguments with the caller's exception sink; see NewHashDeclNode::derefImpl()
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        if (args) {
            args->deref(xsink);
            args = nullptr;
        }
        return ParseNode::derefImpl(xsink);
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
    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    const QoreTypeInfo* typeInfo;
    QoreValue args{};
    //! true if the type has type parameters (generic code); see NewComplexHashNode::needs_substitution
    bool needs_substitution;

    DLLLOCAL NewComplexListNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo, QoreValue a)
            : ParseNode(loc, NT_SCOPE_REF), typeInfo(typeInfo), args(a),
            needs_substitution(qore_type_contains_type_parameter(typeInfo)) {
        assert(QoreTypeInfo::getUniqueReturnComplexList(typeInfo));
    }

    //! Creates the copy of a background expression, with the arguments taken when the thread starts
    /** See copy_value_and_resolve_lvar_refs(); the new thread evaluates the copy
    */
    DLLLOCAL NewComplexListNode(const NewComplexListNode& old, QoreValue a) : ParseNode(old),
            typeInfo(old.typeInfo), args(a), needs_substitution(old.needs_substitution) {
    }

    DLLLOCAL virtual ~NewComplexListNode() {
        args.discard(nullptr);
    }

    //! Releases the arguments with the caller's exception sink; see NewHashDeclNode::derefImpl()
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        args.discard(xsink);
        args = QoreValue();
        return ParseNode::derefImpl(xsink);
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

class NewComplexBufferNode : public ParseNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

public:
    const QoreTypeInfo* typeInfo;
    QoreValue args{};
    QoreComplexBufferInitKind initKind;

    DLLLOCAL NewComplexBufferNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo, QoreValue a,
            QoreComplexBufferInitKind initKind = QoreComplexBufferInitKind::Constructor)
            : ParseNode(loc, NT_SCOPE_REF), typeInfo(typeInfo), args(a), initKind(initKind) {
        assert(QoreTypeInfo::getUniqueReturnComplexBuffer(typeInfo));
    }

    //! Creates the copy of a background expression, with the arguments taken when the thread starts
    /** See copy_value_and_resolve_lvar_refs(); the new thread evaluates the copy
    */
    DLLLOCAL NewComplexBufferNode(const NewComplexBufferNode& old, QoreValue a) : ParseNode(old),
            typeInfo(old.typeInfo), args(a), initKind(old.initKind) {
    }

    DLLLOCAL virtual ~NewComplexBufferNode() {
        args.discard(nullptr);
    }

    //! Releases the arguments with the caller's exception sink; see NewHashDeclNode::derefImpl()
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        args.discard(xsink);
        args = QoreValue();
        return ParseNode::derefImpl(xsink);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new complex buffer operator expression ('%s')", QoreTypeInfo::getName(typeInfo));
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "new complex buffer operator expression";
    }

    DLLLOCAL bool shouldEvaluateWithNode() const {
        return initKind != QoreComplexBufferInitKind::Constructor;
    }
};

DLLLOCAL QoreBufferNode* qore_new_complex_buffer_from_value(const QoreTypeInfo* typeInfo, QoreValue val,
    ExceptionSink* xsink, QoreComplexBufferInitKind initKind = QoreComplexBufferInitKind::Constructor);

//! Source and background-copy factories select a legacy entry before execution.
template <typename Base>
class LexicalNewComplexNode : public Base {
public:
    using Base::Base;

protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return this->evalLexical(needs_deref, xsink);
    }
};

template <typename Base, typename... Args>
Base* qore_make_new_complex(LexicalSemanticPolicy policy, Args&&... args) {
    return policy.needsFilter()
        ? new LexicalNewComplexNode<Base>(std::forward<Args>(args)...)
        : new Base(std::forward<Args>(args)...);
}

#endif
