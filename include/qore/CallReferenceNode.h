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

#ifndef _QORE_FUNCTIONREFERENCENODE_H

#define _QORE_FUNCTIONREFERENCENODE_H

//! base class for call references, reference-counted, dynamically allocated only
/** cannot be a ParseNode or SimpleQoreNode because we require deref(xsink)
 */
class AbstractCallReferenceNode : public AbstractQoreNode {
public:
    DLLEXPORT AbstractCallReferenceNode(bool n_needs_eval = false, qore_type_t n_type = NT_FUNCREF);

    DLLEXPORT virtual ~AbstractCallReferenceNode();

    //! returns false unless perl-boolean-evaluation is enabled, in which case it returns true
    /** @return false unless perl-boolean-evaluation is enabled, in which case it returns true
    */
    DLLEXPORT virtual bool getAsBoolImpl() const;

    //! concatenate the verbose string representation of the value to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink not used by this implementation of the function
        @return -1 for exception raised, 0 = OK
    */
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    //! returns a QoreString giving the verbose string representation of the value
    /** used for %n and %N printf formatting
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted,
        if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks
        @param xsink not used by this implementation of the function

        @note Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly

        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char* getTypeName() const;

    //! Returns a non-const ptr to the same object after increasing the reference count
    DLLLOCAL AbstractCallReferenceNode* refSelf() const {
        ref();
        return const_cast<AbstractCallReferenceNode*>(this);
    }

    DLLLOCAL static const char* getStaticTypeName() {
        return "call reference";
    }

protected:
    //! this function should never be called for function references; this function should never be called directly
    /** in debug mode this function calls assert(false)
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    //! protected constructor for subclasses that are not reference-counted
    DLLLOCAL AbstractCallReferenceNode(bool n_needs_eval, bool n_there_can_be_only_one,
            qore_type_t n_type = NT_FUNCREF);

private:
    //! this function will never be executed for parse types; this function should never be called directly
    /** in debug mode this function calls assert(false)
    */
    DLLLOCAL virtual AbstractQoreNode* realCopy() const;

    //! this function will never be executed for parse types; this function should never be called directly
    /** in debug mode this function calls assert(false)
    */
    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! this function will never be executed for parse types; this function should never be called directly
    /** in debug mode this function calls assert(false)
    */
    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;
};

class QoreFunction;

//! base class for resolved call references
class ResolvedCallReferenceNode : public AbstractCallReferenceNode {
public:
    //! public exported constructor function
    DLLEXPORT ResolvedCallReferenceNode();

    //! public destructor function
    DLLEXPORT virtual ~ResolvedCallReferenceNode();

    //! constructor is not exported outside the library
    DLLLOCAL ResolvedCallReferenceNode(bool n_needs_eval, qore_type_t n_type = NT_FUNCREF);

    //! pure virtual function for executing the function reference
    /** executes the function reference and returns the value returned

        @param args the arguments to the function
        @param xsink any Qore-language exception thrown (and not handled) will be added here

        @return a pointer to an AbstractQoreNode, the caller owns the reference count returned (can also be nullptr)
    */
    DLLLOCAL virtual QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const = 0;

    //! returns a pointer to the QoreProgram object associated with this reference (can be nullptr)
    /** this function is not exported in the library's public interface
        @return a pointer to the QoreProgram object associated with this reference (can be nullptr)
    */
    DLLEXPORT virtual QoreProgram* getProgram() const;

    //! Returns the internal function object, if any; can return nullptr
    DLLLOCAL virtual QoreFunction* getFunction() = 0;

    //! references itself and returns this
    DLLLOCAL ResolvedCallReferenceNode* refRefSelf() const {
        ref();
        return const_cast<ResolvedCallReferenceNode*>(this);
    }

    //! returns true if the other node is the same value
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const;

    //! returns true if the other node is the same value
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const;

    //! returns this with the ref count inmcremented; not a real copy
    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

    // must be defined but performs no action
    DLLEXPORT virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    // the following function must be defined, but is never called
    DLLEXPORT virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    //! Increments the weak reference count
    DLLLOCAL void weakRef() {
        weak_refs.ROreference();
    }

    //! Decrements the weak reference count
    DLLLOCAL void weakDeref() {
        if (weak_refs.ROdereference()) {
            delete this;
        }
    }

protected:
    //! weak references
    QoreReferenceCounter weak_refs;

    //! Called when the strong reference count reaches zero
    /** This function must free all memory owned by the object; the weak reference count is decremented, and if it
        reaches zero, the object is deleted

        @param xsink if an error occurs, the Qore-language exception information will be added here

        @return always returns false
    */
    DLLEXPORT virtual bool derefImpl(ExceptionSink* xsink);
};

class OptionalCallReferenceAccessHelper {
public:
    DLLEXPORT OptionalCallReferenceAccessHelper(ExceptionSink* xsink, ResolvedCallReferenceNode* ref);

    DLLLOCAL ~OptionalCallReferenceAccessHelper() {
        if (ref) {
            ref->deref(xsink);
        }
    }

    DLLLOCAL operator bool() const {
        return ref ? true : false;
    }

    DLLLOCAL const AbstractCallReferenceNode* operator*() const {
        return ref;
    }

    DLLLOCAL AbstractCallReferenceNode* operator*() {
        return ref;
    }

protected:
    ExceptionSink* xsink;
    ResolvedCallReferenceNode* ref;
};

#endif
