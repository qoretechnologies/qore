/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreInstanceOfOperatorNode.h

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

#ifndef _QORE_QOREINSTANCEOFOPERATORNODE_H

#define _QORE_QOREINSTANCEOFOPERATORNODE_H

class QoreInstanceOfOperatorNode : public QoreSingleExpressionOperatorNode<QoreOperatorNode> {
public:
    DLLLOCAL static QoreInstanceOfOperatorNode* create(LexicalSemanticPolicy policy,
        const QoreProgramLocation* loc, QoreValue exp, QoreParseTypeInfo* type);
    DLLLOCAL static QoreInstanceOfOperatorNode* create(LexicalSemanticPolicy policy,
        const QoreProgramLocation* loc, QoreValue exp, const QoreTypeInfo* type);
    DLLLOCAL QoreInstanceOfOperatorNode(const QoreProgramLocation* loc, QoreValue exp, QoreParseTypeInfo* ref)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp), r(ref) {
    }

    DLLLOCAL QoreInstanceOfOperatorNode(const QoreProgramLocation* loc, QoreValue exp, const QoreTypeInfo* ti)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp), ti(ti) {
    }

    DLLLOCAL virtual ~QoreInstanceOfOperatorNode() {
        delete r;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return InstanceOf_str.c_str();
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return boolTypeInfo;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        ValueHolder n_exp(copy_value_and_resolve_lvar_refs(exp, xsink), xsink);
        if (*xsink)
            return nullptr;
        assert(ti);
        return create(getSemanticPolicy(), loc, n_exp.release(), ti);
    }

    //! Returns the resolved type info for the instanceof check (for AOT serialization)
    DLLLOCAL const QoreTypeInfo* getInstanceTypeInfo() const { return ti; }

    //! Returns true if \a ti accepts \a val as the instanceof operator checks it in every execution mode
    /** A weak reference is checked as its target.  The type parameters of a generic class or of generic code in
        \a ti are those of the code that runs, as for a variable or parameter declared with them: the type arguments
        of the instantiation (qore#6016)

        @param ti the type of the operator; a specialized IR function, whose type parameters are bound statically and
        not current at run time, passes it with them substituted (QoreIRFunction::specializeType())
        @param val the value checked
    */
    DLLLOCAL static bool runtimeInstanceOf(const QoreTypeInfo* ti, QoreValue val);

protected:
    template <bool Lexical>
    DLLLOCAL QoreValue evalCommon(bool& needs_deref, ExceptionSink* xsink) const;
    QoreParseTypeInfo* r = nullptr;
    const QoreTypeInfo* ti = nullptr;

    DLLLOCAL static QoreString InstanceOf_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);
};

#endif
