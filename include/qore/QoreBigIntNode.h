/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBigIntNode.h

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

#ifndef _QORE_QOREBIGINTNODE_H
#define _QORE_QOREBIGINTNODE_H

#include <qore/AbstractQoreNode.h>

//! Heap-allocated 64-bit integer node for values outside inline 48-bit range
/** This class is used by QoreValue64 to store integer values that cannot
    fit in the 48-bit inline representation (values outside the range
    +/-140,737,488,355,328).

    In normal Qore code, integers in the 48-bit range are stored directly
    in QoreValue64's 8-byte representation without heap allocation.
    Only very large integers require this heap-allocated node.

    @note This class is used internally by QoreValue64 and should not
    normally be used directly in user code.

    @since Qore 2.0 (QoreValue64 implementation)
*/
class QoreBigIntNode : public SimpleValueQoreNode {
private:
    //! the integer value
    int64 val;

    //! returns the value as a bool
    DLLLOCAL virtual bool getAsBoolImpl() const override;

    //! returns the value as an int
    DLLLOCAL virtual int getAsIntImpl() const override;

    //! returns the value as a 64-bit int
    DLLLOCAL virtual int64 getAsBigIntImpl() const override;

    //! returns the value as a double
    DLLLOCAL virtual double getAsFloatImpl() const override;

protected:
    //! the destructor is protected because it should not be called directly
    DLLEXPORT virtual ~QoreBigIntNode();

public:
    //! creates a new big integer value
    /** @param n the value for the object
    */
    DLLEXPORT QoreBigIntNode(int64 n = 0);

    //! copy constructor
    DLLEXPORT QoreBigIntNode(const QoreBigIntNode& old);

    //! returns the integer value
    DLLEXPORT int64 getValue() const;

    //! concatenate the verbose string representation of the value to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const override;

    //! returns a QoreString giving the verbose string representation of the value
    /** Used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const override;

    //! returns a copy of the object; the caller owns the reference count
    DLLEXPORT virtual AbstractQoreNode* realCopy() const override;

    //! tests for equality with type conversion (soft compare)
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const override;

    //! tests for equality without type conversion (hard compare)
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const override;

    //! returns the type name
    DLLEXPORT virtual const char* getTypeName() const override;

    //! returns the type name as a static string
    DLLLOCAL static const char* getStaticTypeName() {
        return "integer";
    }
};

#endif // _QORE_QOREBIGINTNODE_H
