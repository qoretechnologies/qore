/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreNullNode.h

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

#ifndef _QORE_QORENULLNODE_H

#define _QORE_QORENULLNODE_H

#include <qore/AbstractQoreNode.h>

/** @file QoreNullNode.h
    defines the QoreNullNode class
*/

//! Qore's SQL "NULL" parse tree/value type, not-referenced counted, not dynamically allocated
/** This class cannot be instantiated; there will only be one single QoreNullNode object instantiated and used
    everywhere in the Qore library; use the null() function or simply &Null to acquire a pointer to an object
    of this class.
    This value can be represented in Qore code as the keyword "NULL".
    NOTE: Qore's "NULL" is not equal to "NOTHING"
    @see QoreNothingNode
 */
class QoreNullNode : public UniqueValueQoreNode {
private:
    //! returns the value as a boolean
    DLLLOCAL virtual bool getAsBoolImpl() const;

    //! returns the value as an integer
    DLLLOCAL virtual int getAsIntImpl() const;

    //! returns the value as a 64-bit integer
    DLLLOCAL virtual int64 getAsBigIntImpl() const;

    //! returns the value as a float
    DLLLOCAL virtual double getAsFloatImpl() const;

protected:
    //! this function is never called for this type
    /** @see AbstractQoreNode::evalImpl()
    */
    using SimpleValueQoreNode::evalImpl;
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLEXPORT QoreNullNode();

    DLLEXPORT virtual ~QoreNullNode();

    //! concatenate "<NULL>" to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink if an error occurs, the Qore-language exception information will be added here
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT virtual int getAsString(QoreString &str, int foff, class ExceptionSink *xsink) const;

    //! returns a QoreString with the text: "<NULL>"
    /** used for %n and %N printf formatting
        @param del is always set to true for this implementation of the function, meaning that the returned QoreString pointer should be deleted
        @param foff for multi-line formatting offset, -1 = no line breaks (ignored by this version of the function)
        @param xsink ignored by this version of the function
        NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString *getAsString(bool &del, int foff, class ExceptionSink *xsink) const;

    //! tests for equality with possible type conversion (soft compare)
    /** since no type can be implicitly converted to a NULL, this comparison is the same as is_equal_hard() for QoreNullNode
        @param v the value to compare
        @param xsink ignored for this version of the function
    */
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const;

    //! tests for equality without type conversions (hard compare)
    /**
        @param v the value to compare
        @param xsink ignored for this version of the function
    */
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char *getTypeName() const;

    //! returns the type information
    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    //! returns the type name (useful in templates)
    DLLLOCAL static const char *getStaticTypeName() {
        return "NULL";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_NULL;
    }

    static const qore_type_t TYPE = NT_NULL;
};

//! use this function to test for NULL
static inline bool is_null(const AbstractQoreNode *n) {
    // this is faster than a dynamic_cast<const QoreNullNode *> operation
    return n && n->getType() == NT_NULL;
}

//! the global and unique NULL value in Qore
DLLEXPORT extern QoreNullNode Null;

//! returns a pointer to Null
static inline QoreNullNode *null() {
    return &Null;
}

#endif
