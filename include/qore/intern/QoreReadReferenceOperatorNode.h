/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreReadReferenceOperatorNode.h

    Qore Programming Language

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QOREREADREFERENCEOPERATORNODE_H

#define _QORE_QOREREADREFERENCEOPERATORNODE_H

//! Reads the value of the lvalue that a reference element names (qore#6662)
/** The parser places this node around the right-hand side of a typed assignment when the right-hand side is a member
    or an element access (QoreHashObjectDereferenceOperatorNode, QoreSquareBracketsOperatorNode) whose parse type is a
    reference type (\c reference<T> or \c *reference<T>) and the type of the target does not accept a reference:
    <tt>hash<string, reference<*T>> h = {"a": \\g}; *T o = h.a;</tt>.  A container element that holds a reference is
    read as the reference itself, which such a target does not accept; the node reads the referenced value instead, as
    a value of the right-hand side, before the assignment locks its target.

    The value of the operand is returned as it is when it is not a reference.  Its parse type is the referenced type:
    \c T for \c reference<T>, \c *T for \c *reference<T>.

    The node is created after its operand is parse-initialized; its constructor takes its type and the parser marks it
    as parse-initialized (ParseNode::setParseAnalysis()), so it is not parse-initialized again.  IR code reads the
    reference with QoreIROpcode::ReadReferenceThrough.
*/
class QoreReadReferenceOperatorNode : public QoreSingleExpressionOperatorNode<QoreOperatorNode> {
public:
    //! Creates the node
    /** @param loc the location of the expression
        @param exp the operand, whose reference the node takes
        @param n_return_type the type of the node's value: the type of the referenced value (see getTargetType())
    */
    DLLLOCAL QoreReadReferenceOperatorNode(const QoreProgramLocation* loc, QoreValue exp,
            const QoreTypeInfo* n_return_type = nullptr)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp), returnTypeInfo(n_return_type) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return readreference_str.c_str();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        QoreReadReferenceOperatorNode* rv = copyBackgroundExplicit<QoreReadReferenceOperatorNode>(xsink);
        if (rv) {
            rv->returnTypeInfo = returnTypeInfo;
        }
        return rv;
    }

    //! Returns the type of the value read through a value of the reference type given
    /** @param ti a reference type (\c reference<T> or \c *reference<T>)

        @return \c T for \c reference<T>, \c *T for \c *reference<T> (\c T if it has no or-nothing type), or \a ti
        itself if it is not a reference type
    */
    DLLLOCAL static const QoreTypeInfo* getTargetType(const QoreTypeInfo* ti);

    //! Returns true if \a ti is a reference type (\c reference<T> or \c *reference<T>)
    DLLLOCAL static bool isReferenceType(const QoreTypeInfo* ti);

protected:
    //! the type of the referenced value, set when the node is created or parse-initialized
    const QoreTypeInfo* returnTypeInfo = nullptr;

    DLLLOCAL static QoreString readreference_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }
};

#endif
