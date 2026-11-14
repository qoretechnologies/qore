/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ObjectMethodReference.h

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

#ifndef _QORE_OBJECTMETHODREFERENCE_H

#define _QORE_OBJECTMETHODREFERENCE_H

#include <atomic>
#include <string>

class RuntimeConfig;

class AbstractParseObjectMethodReferenceNode : public ParseNode {
protected:
   //! optionally evaluates the argument
   /** return value requires a deref(xsink) if needs_deref is true
       @see AbstractQoreNode::eval()
   */
   DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const = 0;
   DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const = 0;

public:
   DLLLOCAL AbstractParseObjectMethodReferenceNode(const QoreProgramLocation* loc) : ParseNode(loc, NT_OBJMETHREF) {
   }

   DLLLOCAL AbstractParseObjectMethodReferenceNode(const AbstractParseObjectMethodReferenceNode& old)
         : ParseNode(old) {
   }

   DLLLOCAL virtual ~AbstractParseObjectMethodReferenceNode() {
   }

   DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
      str.sprintf("object method reference (%p)", this);
      return 0;
   }

   // if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
   DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
      del = true;
      QoreString* rv = new QoreString;
      getAsString(*rv, foff, xsink);
      return rv;
   }

   // returns the type name as a c string
   DLLLOCAL virtual const char* getTypeName() const {
      return "object method reference";
   }
};

class ParseObjectMethodReferenceNode : public AbstractParseObjectMethodReferenceNode {
private:
    QoreValue exp{};
    std::string method;
    //! the declared class of the object expression, if parse-time type information gives one
    const QoreClass *qc;
    //! the method of the declared class (qc), resolved when parsing; nullptr if qc is not set; used for an object of
    //! that class, and for a static method
    const QoreMethod* m;

    //! the single-class cache of the method resolved at run time for the class of the object
    /** Unless the method of the declared class (m) applies, the method is resolved for the class of each object, as
        AOT code does (qore#6062); the cache holds the result for one class, the first whose method is public, so the
        usual case of objects of one class does not look the method up again.  It is filled once, under lck: \a rt_m
        and \a rt_resolved are set before \a rt_class_id is published, and never change afterwards, so the objects of
        any other class are resolved each time, also after the cached class is gone.  It is keyed on the allocation ID
        of the class data (see qore_class_private::getAllocationId()), which no other class data can have, so a hit is
        the class itself and the method is valid as long as the object is.
    */
    mutable std::atomic<uint64_t> rt_class_id{0};
    //! the method of the cached class (see rt_class_id)
    mutable const QoreMethod* rt_m = nullptr;
    //! true if the reference to the method of the cached class is made with the method, false if by name
    mutable bool rt_resolved = false;
    //! serializes filling the cache
    mutable QoreThreadLock lck;

    DLLLOCAL virtual ~ParseObjectMethodReferenceNode();

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return callReferenceTypeInfo;
    }

protected:
    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL ParseObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreValue n_exp, char* n_method);

    //! Creates the copy of a background expression, with the object expression taken when the thread starts
    /** See copy_value_and_resolve_lvar_refs(); the new thread evaluates the copy
    */
    DLLLOCAL ParseObjectMethodReferenceNode(const ParseObjectMethodReferenceNode& old, QoreValue n_exp)
            : AbstractParseObjectMethodReferenceNode(old), exp(n_exp), method(old.method), qc(old.qc), m(old.m) {
        // the run-time cache of the copy starts empty (see rt_class_id)
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    //! Releases the object expression with the caller's exception sink
    /** A background copy holds the value of the expression, whose release can run its destructor; see
        QoreParseListNode::derefImpl()
    */
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        exp.discard(xsink);
        exp = QoreValue();
        return AbstractParseObjectMethodReferenceNode::derefImpl(xsink);
    }

    //! Returns the object expression (for IR local variable analysis)
    DLLLOCAL const QoreValue& getExp() const {
        return exp;
    }

    //! Returns the method name (for AOT serialization)
    DLLLOCAL const std::string& getMethodName() const {
        return method;
    }

    //! Returns the declared class of the receiver if the method is a static method of it, to which a reference
    //! through an object is bound (qore#5990), otherwise nullptr
    DLLLOCAL const QoreClass* getStaticBoundClass() const {
        return qc && m && m->isStatic() ? qc : nullptr;
    }

    //! Returns a reference to the method of the object that the object expression gave
    /** The IR interpreter and native code evaluate the object expression themselves and use this to create the
        reference, so the expression is evaluated once, as in the AST interpreter

        @param val the value of the object expression
        @param xsink for the exception raised if the value is not an object or the method cannot be referred to

        @return a RunTimeObjectMethodReference, or no value if an exception was raised
    */
    DLLLOCAL QoreValue makeReference(QoreValue val, ExceptionSink* xsink) const;

private:
    //! Resolves the method for the class of the object and makes the reference, filling the cache if it is empty
    DLLLOCAL QoreValue makeReferenceForClass(QoreObject* o, const QoreClass* oc, uint64_t class_id,
            ExceptionSink* xsink) const;
};

class ParseSelfMethodReferenceNode : public AbstractParseObjectMethodReferenceNode {
private:
    std::string method;
    const QoreMethod* meth;

    DLLLOCAL ~ParseSelfMethodReferenceNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return callReferenceTypeInfo;
    }

protected:
    // returns a RunTimeObjectMethodReference or NULL if there's an exception
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL ParseSelfMethodReferenceNode(const QoreProgramLocation* loc, char* n_method)
            : AbstractParseObjectMethodReferenceNode(loc), method(n_method), meth(0) {
        free(n_method);
    }

    DLLLOCAL ParseSelfMethodReferenceNode(const QoreProgramLocation* loc, const QoreMethod* m)
            : AbstractParseObjectMethodReferenceNode(loc), method(m->getName()), meth(m) {
    }

    //! Returns the method name (for AOT serialization)
    DLLLOCAL const std::string& getMethodName() const {
        return method;
    }
};

class ParseScopedSelfMethodReferenceNode : public AbstractParseObjectMethodReferenceNode {
private:
    NamedScope *nscope;
    const QoreMethod* method;

    DLLLOCAL virtual ~ParseScopedSelfMethodReferenceNode();

protected:
    // returns a RunTimeObjectMethodReference or NULL if there's an exception
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return callReferenceTypeInfo;
    }

public:
    DLLLOCAL ParseScopedSelfMethodReferenceNode(const QoreProgramLocation* loc, NamedScope *n_nscope);
};

#endif
