/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CallReferenceNode.h

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

#ifndef _QORE_INTERN_FUNCTIONREFERENCENODE_H

#define _QORE_INTERN_FUNCTIONREFERENCENODE_H

#include <string>

class LocalFunctionCallReferenceNode;

struct CallReferenceNodeLocation {
    const QoreProgramLocation* loc;

    DLLLOCAL CallReferenceNodeLocation(const QoreProgramLocation* loc) : loc(loc) {
    }
};

class AbstractCallReferenceNodeIntern : public AbstractCallReferenceNode, public CallReferenceNodeLocation {
public:
    DLLLOCAL AbstractCallReferenceNodeIntern(const QoreProgramLocation* loc, bool n_needs_eval) :
        AbstractCallReferenceNode(n_needs_eval), CallReferenceNodeLocation(loc) {
    }
};

class ResolvedCallReferenceNodeIntern : public ResolvedCallReferenceNode, public CallReferenceNodeLocation {
public:
    DLLLOCAL ResolvedCallReferenceNodeIntern(const QoreProgramLocation* loc, bool n_needs_eval = false) :
        ResolvedCallReferenceNode(n_needs_eval), CallReferenceNodeLocation(loc) {
    }
};

class AbstractUnresolvedCallReferenceNode : public AbstractCallReferenceNodeIntern {
public:
    DLLLOCAL AbstractUnresolvedCallReferenceNode(const QoreProgramLocation* loc, bool n_needs_eval) : AbstractCallReferenceNodeIntern(loc, n_needs_eval) {
    }
};

//! an unresolved call reference, only present temporarily in the parse tree
class UnresolvedProgramCallReferenceNode : public AbstractUnresolvedCallReferenceNode {
public:
    char* str;

    DLLLOCAL UnresolvedProgramCallReferenceNode(const QoreProgramLocation* loc, char* n_str);

    DLLLOCAL virtual ~UnresolvedProgramCallReferenceNode();

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);
};

class UnresolvedCallReferenceNode : public UnresolvedProgramCallReferenceNode {
public:
    DLLLOCAL UnresolvedCallReferenceNode(const QoreProgramLocation* loc, char* n_str) : UnresolvedProgramCallReferenceNode(loc, n_str) {
    }

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);
};

//! a call reference to a static user method
class LocalStaticMethodCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL LocalStaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method)
            : ResolvedCallReferenceNodeIntern(loc, true), method(n_method) {
        //printd(5, "LocalStaticMethodCallReferenceNode::LocalStaticMethodCallReferenceNode() this: %p %s::%s() "
        //    "pgm: %p\n", this, method->getClass()->getName(), method->getName(), pgm);
    }

    DLLLOCAL virtual ~LocalStaticMethodCallReferenceNode() {
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalStaticMethodCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction();

protected:
    const QoreMethod* method;

    // constructor for subclasses
    DLLLOCAL LocalStaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
            bool n_needs_eval) : ResolvedCallReferenceNodeIntern(loc, n_needs_eval), method(n_method) {
    }

    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;
};

class StaticMethodCallReferenceNode : public LocalStaticMethodCallReferenceNode {
protected:
    QoreProgram* pgm;
    const qore_class_private* class_ctx;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

public:
    DLLLOCAL StaticMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
            QoreProgram* n_pgm, const qore_class_private* n_class_ctx);

    DLLLOCAL ~StaticMethodCallReferenceNode() {
        assert(!pgm);
    }

    DLLLOCAL virtual QoreValue execValue(const QoreListNode *args, ExceptionSink *xsink) const;
};

//! a call reference to a static user method
class LocalMethodCallReferenceNode : public LocalStaticMethodCallReferenceNode {
protected:
    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL LocalMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method, bool n_needs_eval) : LocalStaticMethodCallReferenceNode(loc, n_method, n_needs_eval) {
        //printd(5, "LocalMethodCallReferenceNode::LocalStaticMethodCallReferenceNode() this: %p %s::%s() pgm: %p\n", this, method->getClass()->getName(), method->getName(), pgm);
    }

public:
    DLLLOCAL LocalMethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method) : LocalStaticMethodCallReferenceNode(loc, n_method) {
        //printd(5, "LocalMethodCallReferenceNode::LocalStaticMethodCallReferenceNode() this: %p %s::%s() pgm: %p\n", this, method->getClass()->getName(), method->getName(), pgm);
    }

    DLLLOCAL virtual ~LocalMethodCallReferenceNode() {
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalMethodCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;
};

class MethodCallReferenceNode : public LocalMethodCallReferenceNode {
protected:
    QoreObject* obj;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

public:
    DLLLOCAL MethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method, QoreProgram* n_pgm);

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;
};

//! a call reference to a user function from within the same QoreProgram object
class LocalFunctionCallReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL LocalFunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf);

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return LocalFunctionCallReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() {
        return const_cast<QoreFunction*>(uf);
    }

protected:
    const QoreFunction* uf;

    // constructor for subclasses
    DLLLOCAL LocalFunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf,
            bool n_needs_eval);

    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink* xsink) const;
};

//! a call reference to a user function
class FunctionCallReferenceNode : public LocalFunctionCallReferenceNode {
public:
    DLLLOCAL FunctionCallReferenceNode(const QoreProgramLocation* loc, const QoreFunction* n_uf,
            QoreProgram* n_pgm) : LocalFunctionCallReferenceNode(loc, n_uf, false), pgm(n_pgm) {
        assert(pgm);
        // make a weak reference to the Program - a strong reference (QoreProgram::ref()) could cause a recursive
        // reference
        pgm->depRef();
    }

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

protected:
    QoreProgram* pgm;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);
};

//! an unresolved static method call reference, only present temporarily in the parse tree
class UnresolvedStaticMethodCallReferenceNode : public AbstractUnresolvedCallReferenceNode {
public:
    DLLLOCAL UnresolvedStaticMethodCallReferenceNode(const QoreProgramLocation* loc, NamedScope* n_scope);

    DLLLOCAL virtual ~UnresolvedStaticMethodCallReferenceNode();

    using AbstractQoreNode::deref;
    DLLLOCAL void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

protected:
    NamedScope* scope;
};

class ImportedFunctionEntry;

//! a run-time call reference to a method of a particular object
class RunTimeObjectMethodReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL RunTimeObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
            const char* n_method);

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return RunTimeObjectMethodReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction() {
        // FIXME: implement type checking and method matching in ParseObjectMethodReferenceNode::parseInit()
        return nullptr;
    }

private:
    QoreObject* obj;
    std::string method;
    const qore_class_private* qc;

    DLLLOCAL virtual ~RunTimeObjectMethodReferenceNode();
};

//! a run-time call reference to a method of a particular object where the method's class
// is known when the method reference node object is created
class RunTimeResolvedMethodReferenceNode : public ResolvedCallReferenceNodeIntern {
public:
    DLLLOCAL RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
            const QoreMethod* n_method, const qore_class_private* ctx = runtime_get_class());

    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreProgram* getProgram() const;

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return RunTimeResolvedMethodReferenceNode::is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreFunction* getFunction();

private:
    QoreObject* obj;
    const QoreMethod* method;
    const qore_class_private* qc;
    QoreProgram* pgm = getProgram();

    DLLLOCAL virtual ~RunTimeResolvedMethodReferenceNode();
};

#endif
