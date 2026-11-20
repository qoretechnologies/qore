/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeDeclPin.h

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

#ifndef _QORE_INTERN_QORETYPEDECLPIN_H
#define _QORE_INTERN_QORETYPEDECLPIN_H

#include <qore/common.h>
#include <qore/QoreReflection.h>

#include <memory>

//! Returns true if a type names a user (not builtin) class, hashdecl or enum declaration, or may name one (a type
//! with type arguments or type parameters); see QoreTypeDeclPin
DLLLOCAL bool qore_type_needs_decl_pin(const QoreTypeInfo* ti);

//! Keeps the user declarations that a type names while it lives (issue #6419)
/** For an object that keeps a type that it read through a derived type (the element type of a container's type, the
    type argument of an iterator's type) for its lifetime: a derived type does not keep the declarations that it
    names, which can be freed once they are retired.  Nothing is allocated, and no lock is taken, for a type that names
    no user declaration.
*/
class QoreTypeDeclPin {
public:
    DLLLOCAL QoreTypeDeclPin(const QoreTypeInfo* ti = nullptr) {
        set(ti);
    }

    DLLLOCAL QoreTypeDeclPin(const QoreTypeDeclPin&) = delete;
    DLLLOCAL QoreTypeDeclPin& operator=(const QoreTypeDeclPin&) = delete;

    //! Keeps the declarations that \a ti names instead of the ones kept before
    DLLLOCAL void set(const QoreTypeInfo* ti) {
        if (ti == pinned) {
            return;
        }
        pinned = ti;
        refs.reset(ti && qore_type_needs_decl_pin(ti) ? new QoreExternalTypeDeclRefs(ti) : nullptr);
    }

private:
    std::unique_ptr<QoreExternalTypeDeclRefs> refs;
    const QoreTypeInfo* pinned = nullptr;
};

//! QoreTypeInfo::matchCommonType() that keeps the declarations that the common type names when it changes
DLLLOCAL bool qore_match_common_type_pinned(const QoreTypeInfo*& ctype, const QoreTypeInfo* ntype,
        QoreTypeDeclPin& pin);

#endif
