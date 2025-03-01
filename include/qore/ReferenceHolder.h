/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReferenceHolder.h

    Smart pointer like class that dereferences
    obtained pointer to a QoreReferenceCounter in its destructor.

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies

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

#ifndef QORE_REFERENCE_HOLDER_H_
#define QORE_REFERENCE_HOLDER_H_

#include <cstdlib>
#include <utility>

//! a templated class to manage a reference count of an object that can throw a Qore-language exception when dereferenced
/** the destructor will call deref(ExceptionSink *)
    @code
    ReferenceHolder<QoreQFont> holder(self->getReferencedPrivateData(CID_QFONT, xsink), xsink);
    // the call to deref(ExceptionSink *) is automatic when the object goes out of scope
    if (*xsink)
       return 0;
    return holder->styleHint();
    @endcode
*/
template<typename T = class AbstractQoreNode>
class ReferenceHolder {
public:
    //! creates an empty ReferenceHolder
    DLLLOCAL ReferenceHolder(ExceptionSink* xsink_) : xsink(xsink_) {}

    //! populates with object with data and the ExceptionSink pointer
    DLLLOCAL ReferenceHolder(T* p_, ExceptionSink* xsink_) : p(p_), xsink(xsink_) {}

    //! calls deref(ExceptionSink *) on the pointer being managed if not 0
    DLLLOCAL ~ReferenceHolder() { if (p) p->deref(xsink);}

    //! returns the pointer being managed
    DLLLOCAL T* operator->() { return p; }

    //! returns the pointer being managed
    DLLLOCAL T* operator*() { return p; }

    //! returns the pointer being managed
    DLLLOCAL const T* operator->() const { return p; }

    //! returns the pointer being managed
    DLLLOCAL const T* operator*() const { return p; }

    //! assigns a new pointer to the holder, dereferences the current pointer if any
    DLLLOCAL void operator=(T *nv) {
        if (p)
            p->deref(xsink);
        p = nv;
    }

    //! releases the pointer to the caller
    DLLLOCAL T* release() {
        T* rv = p;
        p = nullptr;
        return rv;
    }

    //! swaps the current value for a new one
    /** @since %Qore 1.14
    */
    DLLLOCAL T* swap(T* val) {
        T* rv = p;
        p = val;
        return rv;
    }

    //! returns true if a non-0 pointer is being managed
    DLLLOCAL operator bool() const { return p != nullptr; }

    //! returns a pointer to the pointer being managed
    DLLLOCAL T** getPtrPtr() { return &p; }

    //! returns a reference to the ptr being managed
    DLLLOCAL T*& getRef() { return p; }

protected:
    T* p = nullptr;
    ExceptionSink* xsink;

private:
    ReferenceHolder(const ReferenceHolder&) = delete;
    ReferenceHolder& operator=(const ReferenceHolder&) = delete;
    void* operator new(size_t) = delete;
};

//! manages a reference count of a pointer to a class that takes a simple "deref()" call with no arguments
/**
     @code
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(QCS_UTF8));
    // QoreString::concatUnicode() can raise a Qore-language exception if the code is invalid
    str->concatUnicode(code, xsink);
    return *xsink ? str.release() : 0;
    @endcode
*/
template<typename T = class SimpleQoreNode>
class SimpleRefHolder {
public:
    DLLLOCAL SimpleRefHolder() {}
    DLLLOCAL SimpleRefHolder(T* p_) : p(p_) {}

    //! move constructor
    /** @since %Qore 0.9
     */
    DLLLOCAL SimpleRefHolder(SimpleRefHolder&& old) : p(std::move(old.p)) {
        old.p = nullptr;
    }

    DLLLOCAL ~SimpleRefHolder() { if (p) p->deref(); }

    DLLLOCAL void discard() {
        if (p) {
            p->deref();
            p = nullptr;
        }
    }

    DLLLOCAL T* operator->() { return p; }
    DLLLOCAL T* operator*() { return p; }
    DLLLOCAL const T* operator->() const { return p; }
    DLLLOCAL const T* operator*() const { return p; }

    DLLLOCAL void operator=(T* nv) {
        if (p) {
            p->deref();
        }
        p = nv;
    }

    DLLLOCAL T* release() {
        T* rv = p;
        p = nullptr;
        return rv;
    }

    //! swaps the current value for a new one
    /** @since %Qore 1.14
    */
    DLLLOCAL T* swap(T* val) {
        T* rv = p;
        p = val;
        return rv;
    }

    DLLLOCAL operator bool() const {
        return p != nullptr;
    }

protected:
    T* p = nullptr;

private:
    SimpleRefHolder(const SimpleRefHolder&) = delete;
    SimpleRefHolder& operator=(const SimpleRefHolder&) = delete;
    void* operator new(size_t) = delete;
};

#endif
// EOF
