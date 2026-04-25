/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIteratorBase.h

    abstract class for private data for iterators in objects

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

#ifndef _QORE_QOREITERATORBASE_H

#define _QORE_QOREITERATORBASE_H

#include <qore/AbstractPrivateData.h>
#include <qore/QoreValue.h>

DLLEXPORT extern QoreClass* QC_ABSTRACTITERATOR;
DLLEXPORT extern QoreClass* QC_ABSTRACTBIDIRECTIONALITERATOR;
DLLEXPORT extern QoreClass* QC_ABSTRACTQUANTIFIEDBIDIRECTIONALITERATOR;
DLLEXPORT extern QoreClass* QC_ABSTRACTQUANTIFIEDITERATOR;
DLLEXPORT extern QoreClass* QC_ABSTRACTLINEITERATOR;

class QoreAbstractIteratorBase {
protected:
    int tid;

public:
    //! creates the object and marks it as owned by the current thread
    DLLEXPORT QoreAbstractIteratorBase();

    //! destroys the object
    DLLEXPORT virtual ~QoreAbstractIteratorBase();

    //! checks for a valid operation, returns 0 if OK, -1 if not (exception thrown)
    DLLEXPORT int check(ExceptionSink* xsink) const;

    //! returns the name of the current iterator class
    DLLEXPORT virtual const char* getName() const = 0;

    //! returns the element type for the iterator
    DLLLOCAL virtual const QoreTypeInfo* getElementType() const = 0;
};

//! abstract base class for iterator private data
class QoreIteratorBase : public AbstractPrivateData, public QoreAbstractIteratorBase {
protected:
    //! destroys the object
    DLLEXPORT virtual ~QoreIteratorBase();

public:
    //! creates the object and marks it as owned by the current thread
    DLLEXPORT QoreIteratorBase();

    //! Returns true if this iterator provides C++-native iteration fast-paths
    //! (nativeNext() and nativeGetValue()) that can bypass the Qore method
    //! dispatch used by AbstractIteratorHelper.
    /**
        Default returns false; Qore-language iterators and C++ iterators that
        have not opted in are driven through the normal method-call path.

        C++-native iterators override both this and the two methods below to
        cut the per-element dispatch cost by ~5-10x when iterated through
        map/select/foldl/foreach.

        @since %Qore 2.3
     */
    DLLEXPORT virtual bool supportsNativeIteration() const {
        return false;
    }

    //! C++-native fast-path advance; only called when supportsNativeIteration() returns true
    /**
        Must behave identically to the iterator's Qore-visible next() method:
        returns true if a value is available via nativeGetValue(), false at end
        of iteration.

        @since %Qore 2.3
     */
    DLLEXPORT virtual bool nativeNext(ExceptionSink* xsink) {
        return false;
    }

    //! C++-native fast-path value accessor; only called when supportsNativeIteration() returns true
    /**
        Must behave identically to the iterator's Qore-visible getValue()
        method: returns the current element.  May raise INVALID-ITERATOR via
        xsink if the iterator is not positioned on a valid element.

        @since %Qore 2.3
     */
    DLLEXPORT virtual QoreValue nativeGetValue(ExceptionSink* xsink) {
        return QoreValue();
    }
};

//! Emits the standard native fast-path overrides for an iterator class.
/** Use in the public section of a class that inherits @ref QoreIteratorBase
    when:
      - the class has a @c bool @c next() method (no xsink),
      - the class has a @c QoreValue @c getValue(ExceptionSink*) method,
      - thread ownership should be enforced via @ref QoreAbstractIteratorBase::check().

    For classes whose @c next() takes an @c ExceptionSink* (e.g.
    @ref StringSplitIterator after the SPLIT_CHARS extension or
    @ref StringRegexSplitIterator), use
    @ref QORE_NATIVE_FAST_PATH_NEXT_XSINK instead.

    For classes that yield via @c getReferencedValue (the @c ConstListIterator
    family), use @ref QORE_NATIVE_FAST_PATH_REFVAL.

    @since %Qore 2.3
 */
#define QORE_NATIVE_FAST_PATH_DEFAULT()                                         \
    DLLLOCAL bool supportsNativeIteration() const override { return true; }     \
    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {                   \
        if (check(xsink)) {                                                     \
            return false;                                                       \
        }                                                                       \
        return next();                                                          \
    }                                                                           \
    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {          \
        if (check(xsink)) {                                                     \
            return QoreValue();                                                 \
        }                                                                       \
        return getValue(xsink);                                                 \
    }

//! Fast-path variant where @c next() takes an @c ExceptionSink*.
/** @since %Qore 2.3 */
#define QORE_NATIVE_FAST_PATH_NEXT_XSINK()                                      \
    DLLLOCAL bool supportsNativeIteration() const override { return true; }     \
    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {                   \
        if (check(xsink)) {                                                     \
            return false;                                                       \
        }                                                                       \
        return next(xsink);                                                     \
    }                                                                           \
    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {          \
        if (check(xsink)) {                                                     \
            return QoreValue();                                                 \
        }                                                                       \
        return getValue(xsink);                                                 \
    }

//! Fast-path variant for ConstListIterator-style classes that expose
//! @c getReferencedValue() (the wrapped iterator's terminal accessor).
/** @since %Qore 2.3 */
#define QORE_NATIVE_FAST_PATH_REFVAL()                                          \
    DLLLOCAL bool supportsNativeIteration() const override { return true; }     \
    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {                   \
        if (check(xsink)) {                                                     \
            return false;                                                       \
        }                                                                       \
        return next();                                                          \
    }                                                                           \
    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {          \
        if (check(xsink)) {                                                     \
            return QoreValue();                                                 \
        }                                                                       \
        return getReferencedValue(xsink);                                       \
    }

#endif
