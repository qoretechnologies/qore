/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_Type.h QC_Type class definition */
/*
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

#ifndef _QORE_INTERN_QC_TYPE_H

#define _QORE_INTERN_QC_TYPE_H

//! Private data class for Reflection::Type objects
/** Holds a reference to a QoreTypeInfo and keeps the source program alive to prevent
    the program's type data from being freed while Type objects exist.

    Reference strategy to avoid cycles while keeping cross-program data alive:
    - Same-program storage (container program == source_pgm): Uses weak ref (depRef)
      to avoid cycles. The Type will be destroyed with the program anyway.
    - Cross-program storage (container program != source_pgm): Uses strong ref (ref)
      to keep source_pgm's type data alive as long as the Type exists.

    @see issue #4816: module init failure can leave dirty data in foreign modules
*/
class QoreType : public AbstractPrivateData {
public:
    //! The type info pointer
    const QoreTypeInfo* typeInfo;

    //! Source program that owns the type (nullptr for built-in types)
    QoreProgram* source_pgm;

    //! Back-pointer to containing QoreObject (for cycle detection)
    QoreObject* container;

    //! Whether we currently hold a ref to source_pgm
    bool ref_held;

    //! Whether the ref is strong (true) or weak (false)
    bool strong_ref;

    //! Constructor - does NOT acquire ref yet (call setContainer after wrapping in QoreObject)
    /** @param typeInfo the type info pointer
        @param source_pgm the program that owns the type (nullptr for built-in types)
    */
    DLLLOCAL QoreType(const QoreTypeInfo* typeInfo, QoreProgram* source_pgm = nullptr)
            : typeInfo(typeInfo), source_pgm(source_pgm), container(nullptr),
              ref_held(false), strong_ref(false) {
    }

    //! Sets the container QoreObject and acquires the appropriate ref
    /** Must be called after wrapping QoreType in a QoreObject.
        Uses weak ref for same-program storage (to avoid cycles) and
        strong ref for cross-program storage (to keep data alive).
        @param obj the containing QoreObject
    */
    DLLLOCAL void setContainer(QoreObject* obj);

    //! Releases the ref to source_pgm
    DLLLOCAL void releaseSourceRef();

    //! Destructor releases the program reference if held
    DLLLOCAL ~QoreType();
};

DLLEXPORT extern qore_classid_t CID_TYPE;
DLLLOCAL extern QoreClass* QC_TYPE;

DLLLOCAL void preinitTypeClass();
DLLLOCAL QoreClass* initTypeClass(QoreNamespace& ns);

//! Creates a Type object for the given type info
/** @param t the type info
    @param source_pgm the program that owns the type (nullptr for built-in types)
    @return a new Type object
*/
DLLLOCAL QoreObject* get_type_object(const QoreTypeInfo* t, QoreProgram* source_pgm = nullptr);

#endif