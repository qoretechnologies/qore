/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AbstractPrivateData.h

  abstract class for private data in objects

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

#ifndef _QORE_ABSTRACTPRIVATEDATA_H

#define _QORE_ABSTRACTPRIVATEDATA_H

#include <qore/QoreReferenceCounter.h>

class QoreValue;
class RObject;
class RSetHelper;

//! defined when AbstractScannablePrivateData and QorePrivateDataScanner are available
#define _QORE_HAS_SCANNABLE_PRIVATE_DATA 1

//! the base class for all data to be used as private data of Qore objects
/** C++ constructor code for Qore classes must set private data of the class
    against the class ID using QoreObject::setPrivate()
 */
class AbstractPrivateData : public QoreReferenceCounter {
protected:
    //! as these objects are reference counted, the destructor should be called only when the reference count = 0 and not manually
    DLLLOCAL virtual ~AbstractPrivateData() {}

public:
    //! increments the reference count of the object
    DLLLOCAL void ref() const {
        ROreference();
    }

    //! decrements the reference count of the object
    /**
        @param xsink any Qore-language exception information is stored here
    */
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference())
            delete this;
    }

    //! decrements the reference count of the object without the possibility of throwing a Qore-language exception
    DLLLOCAL virtual void deref() {
        if (ROdereference())
            delete this;
    }
};

//! Reports the values that private data holds in C++ containers to the garbage collector
/** The garbage collector passes an object of this class to AbstractScannablePrivateData::scanValues(), which calls
    scan() for each value it holds.

    @see AbstractScannablePrivateData

    @since %Qore 3.0
*/
class QorePrivateDataScanner {
public:
    //! Reports a value held by the private data
    /** A value that cannot be part of a cycle (see AbstractScannablePrivateData::needsScan()) is ignored, so it can be
        reported or skipped.

        @param value the value; it must remain referenced by the private data until scanValues() returns

        @return true if the scan must end: AbstractScannablePrivateData::scanValues() must then return true at once
    */
    DLLEXPORT bool scan(const QoreValue& value);

private:
    friend class qore_object_private;

    RObject& obj;
    RSetHelper& rsh;

    DLLLOCAL QorePrivateDataScanner(RObject& obj, RSetHelper& rsh) : obj(obj), rsh(rsh) {
    }

    DLLLOCAL QorePrivateDataScanner(const QorePrivateDataScanner&) = delete;
    DLLLOCAL QorePrivateDataScanner& operator=(const QorePrivateDataScanner&) = delete;
};

//! The base class for private data that holds Qore values in C++ containers
/** The garbage collector only sees the members of an object; a value that the object's private data holds in a C++
    container is invisible to it, so a cycle through that value is never collected.  Private data that holds values
    that can be part of a cycle - objects, closures, references, and lists and hashes containing them (see
    needsScan()) - derives from this class and reports them to the collector with scanValues().  This is Pattern B
    for modules in design/dgc.md; a single reference to another object is better held as an internal member of the
    object (Pattern A).

    The collector only calls scanValues() for the private data of an object while the object has registered it with
    QoreObject::addPrivateDataScan(); the private data does this when it starts holding a value that needs a scan,
    and calls QoreObject::removePrivateDataScan() when it stops.  It must also mark each change that the collector
    cannot see:
    - QoreObject::privateDataValueStored() for each value that needs a scan, after it is stored and while it is
      still referenced: no scan follows the new edge, so the recursive sets that it changes must not trust their
      reference counts until one has
    - QoreObject::privateDataValuesRemoved() before a value that needs a scan can be released or handed out with
      the container's own reference: the recursive set of the object counted that reference as internal

    The registration and the marks are made under the container's own lock, in the same critical section as the
    change that requires them, as scanValues() reads the container under that lock.

    Each value must be reported by one object only: the collector counts each report as a reference, so a value
    reported by two objects but referenced once looks to it as if no reference to it were held from outside, and it
    can be deleted while still in use.  Private data whose containers are shared with other private data (copy on
    write, for example) must make sure that only one of them reports a shared value, or give each its own reference
    by copying the container.

    The class keeps the layout of AbstractPrivateData, which has no virtual function for this, so that modules
    built against an earlier version of this header can still be used.

    @since %Qore 3.0
*/
class AbstractScannablePrivateData : public AbstractPrivateData {
public:
    //! Reports the values that can be part of a cycle to the collector
    /** Called with the scan's locks held, so it must not block, raise an exception, or call Qore code or the code of
        any other language:
        - it tries the container's lock and returns false at once if the lock is held: a container that the scan
          skips keeps its object from being collected for that scan, but blocking can deadlock
        - it calls QorePrivateDataScanner::scan() for each value and returns true at once if that call does
        - it does not release a reference: the last reference to a value would run its destructor in the scan

        @param scanner reports each value to the collector

        @return true if the scan must end, false if it continues
    */
    DLLLOCAL virtual bool scanValues(QorePrivateDataScanner& scanner) = 0;

    //! Returns true if the value can be part of a cycle and must be reported by scanValues()
    /** These are objects, closures and references, and lists and hashes that contain them; a value assigned as an
        opaque reference ('@=') is not followed by the collector.
    */
    DLLEXPORT static bool needsScan(const QoreValue& value);
};

#endif // _QORE_ABSTRACTPRIVATEDATA_H
