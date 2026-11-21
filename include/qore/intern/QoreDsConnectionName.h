/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreDsConnectionName.h

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

#ifndef _QORE_INTERN_QOREDSCONNECTIONNAME_H
#define _QORE_INTERN_QOREDSCONNECTIONNAME_H

#include <qore/QoreStringNode.h>
#include <qore/ExceptionSink.h>

#include <atomic>

//! the name of the connection that a datasource was created for, set once (qore#7016)
/** The name identifies the datasource in serialized data, which then keeps no credentials of it: the datasource is
    restored by looking the connection up by name.  It is set once and never changed after, so that code that is lent
    a datasource cannot relabel it as another connection; it is read without a lock, as the string is immutable once
    set.
*/
class QoreDsConnectionName {
public:
    DLLLOCAL QoreDsConnectionName() = default;

    //! a copy of a datasource is a datasource of the same connection
    DLLLOCAL QoreDsConnectionName(const QoreDsConnectionName& old) : name(old.getReferencedName()) {
    }

    DLLLOCAL ~QoreDsConnectionName() {
        QoreStringNode* n = name.load(std::memory_order_acquire);
        if (n) {
            n->deref();
        }
    }

    QoreDsConnectionName& operator=(const QoreDsConnectionName&) = delete;

    //! sets the name; raises DATASOURCE-CONNECTION-NAME-ERROR for an empty name or a name that differs from one set
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int set(const QoreString& new_name, ExceptionSink* xsink) {
        if (new_name.empty()) {
            xsink->raiseException("DATASOURCE-CONNECTION-NAME-ERROR", "the connection name of a datasource cannot "
                "be empty");
            return -1;
        }
        SimpleRefHolder<QoreStringNode> n(new QoreStringNode(new_name));
        QoreStringNode* current = nullptr;
        if (name.compare_exchange_strong(current, *n, std::memory_order_acq_rel, std::memory_order_acquire)) {
            n.release();
            return 0;
        }
        // set again with the same name: nothing changes
        if (current->equal(**n)) {
            return 0;
        }
        xsink->raiseException("DATASOURCE-CONNECTION-NAME-ERROR", "the datasource already has another connection "
            "name; the connection name of a datasource is set once and cannot be changed");
        return -1;
    }

    //! returns the name with a reference, or nullptr if none is set
    DLLLOCAL QoreStringNode* getReferencedName() const {
        QoreStringNode* n = name.load(std::memory_order_acquire);
        return n ? n->stringRefSelf() : nullptr;
    }

private:
    //! the name, set once
    std::atomic<QoreStringNode*> name{nullptr};
};

#endif
