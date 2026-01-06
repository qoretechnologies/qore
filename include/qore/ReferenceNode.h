/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReferenceNode.h

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

#ifndef _QORE_REFERENCENODE_H

#define _QORE_REFERENCENODE_H

#include <qore/AbstractQoreNode.h>

//! parse type: reference to a lvalue expression
/** This type could be passed to a builtin function.  To get and set the value of the reference,
    use the TypeSafeReferenceHelper class.  To create a reference argument to pass to a user or builtin
    function, use the ReferenceArgumentHelper class.
    @see TypeSafeReferenceHelper
    @see ReferenceArgumentHelper
 */
class ReferenceNode : public AbstractQoreNode {
    friend class RuntimeReferenceHelper;
    friend class lvalue_ref;

private:
    //! private implementation
    class lvalue_ref* priv;

protected:
    //! returns the value of the reference
    DLLEXPORT virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    //! returns the value of the reference
    DLLEXPORT QoreValue doEval(ExceptionSink* xsink) const;

    //! frees all memory and destroys the object
    DLLEXPORT virtual ~ReferenceNode();

public:
    //! creates the ReferenceNode object - internal function, not exported, not part of the Qore API
    DLLLOCAL ReferenceNode(QoreValue exp, const QoreTypeInfo* typeInfo, QoreObject* self, const void* lvalue_id, const qore_class_private* cls);

    //! creates a copy of the object
    /** @since %Qore 0.8.12.9
        */
    DLLLOCAL ReferenceNode(const ReferenceNode& old);

    //! returns a reference to itself
    DLLEXPORT ReferenceNode* refRefSelf() const;

    //! returns the type information for this object
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! returns the type information for the lvalue referenced
    /** @since %Qore 0.8.13
    */
    DLLEXPORT const QoreTypeInfo* getLValueTypeInfo() const;

    //! concatenate the verbose string representation of the value to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks (ignored in this version of the function)
        @param xsink ignored in this version of the function
        @return this implementation of the function always returns 0 for no error raised
    */
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    //! returns a QoreString giving the verbose string representation of the value
    /** Used for %n and %N printf formatting.  Do not call this function directly; use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks (ignored in this version of the function)
        @param xsink ignored in this version of the function
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! returns a copy of the object
    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

    //! compares the values
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! compares the values
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char* getTypeName() const;

    //! called when the object is deleted
    DLLEXPORT virtual bool derefImpl(ExceptionSink* xsink);
};

#endif
