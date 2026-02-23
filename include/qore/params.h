/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    params.h

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

#ifndef _QORE_PARAMS_H

#define _QORE_PARAMS_H

#include <qore/AbstractQoreNode.h>
#include <qore/ReferenceHolder.h>

/** @file params.h
    Contains inline functions for accessing function and class method arguments.
 */

//! returns the number of arguments passed to the function
/** @param n a pointer to the argument list
    @return the number of arguments passed to the function
 */
static inline unsigned num_args(const QoreListNode* n) {
   return n ? (unsigned)n->size() : 0;
}

//! returns the number of arguments passed to the function
/** @param n a pointer to the argument list
    @return the number of arguments passed to the function
 */
static inline unsigned num_params(const QoreListNode* n) {
   return n ? (unsigned)n->size() : 0;
}

//! returns a ReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ReferenceNode
/**
   Note that this will also return a value for a closure; this is a synonym for test_funcref_param
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a ReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ReferenceNode
 */
static inline const ReferenceNode* test_reference_param(const QoreListNode* n, size_t i) {
    if (!n) return nullptr;
    QoreValue p = n->retrieveEntry(i);
    // the following is faster than a dynamic_cast
    return p.getType() == NT_REFERENCE ? p.get<const ReferenceNode>() : nullptr;
}

//! returns the argument in the position given or 0 if there is none
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return the argument in the position given or 0 if there is none
 */
static inline QoreValue get_param_value(const QoreListNode* n, size_t i) {
    if (!n)
        return QoreValue();
    return n->retrieveEntry(i);
}

//! returns the given type for hard typed parameters
template <typename T>
static inline T* get_hard_value_or_nothing_param(const QoreListNode* n, size_t i) {
    assert(n);
    return n->retrieveEntry(i).get<T>();
}

//! returns the given type for hard typed parameters
static QoreValue get_hard_value_param(const QoreListNode* n, size_t i) {
    assert(n);
    return n->retrieveEntry(i);
}

//! returns a hard typed parameter
#define HARD_QORE_VALUE_OR_NOTHING_PARAM(name, Type, list, i) Type* name = get_hard_value_or_nothing_param<Type>(list, i)

//! returns a hard typed parameter
#define HARD_QORE_VALUE_PARAM(name, Type, list, i) Type* name = get_hard_value_param(list, i).get<Type>()

//! returns an int64 from a hard typed int param
#define HARD_QORE_VALUE_INT(list, i) get_hard_value_param(list, i).getAsBigInt()

//! returns a double from a hard typed float param
#define HARD_QORE_VALUE_FLOAT(list, i) get_hard_value_param(list, i).getAsFloat()

//! returns a const QoreNumberNode* from a hard typed number or softnumber param
#define HARD_QORE_VALUE_NUMBER(list, i) get_hard_value_param(list, i).get<const QoreNumberNode>()

//! returns a bool from a hard typed bool param
#define HARD_QORE_VALUE_BOOL(list, i) get_hard_value_param(list, i).getAsBool()

//! returns a const QoreStringNode* from a hard typed string param
#define HARD_QORE_VALUE_STRING(list, i) get_hard_value_param(list, i).get<const QoreStringNode>()

//! returns a const DateTimeNode* from a hard typed date param
#define HARD_QORE_VALUE_DATE(list, i) get_hard_value_param(list, i).get<const DateTimeNode>()

//! returns a const BinaryNode* from a hard typed binary param
#define HARD_QORE_VALUE_BINARY(list, i) get_hard_value_param(list, i).get<const BinaryNode>()

//! returns a const QoreListNode* from a hard typed list param
#define HARD_QORE_VALUE_LIST(list, i) get_hard_value_param(list, i).get<const QoreListNode>()

//! returns a const QoreHashNode* from a hard typed hash param
#define HARD_QORE_VALUE_HASH(list, i) get_hard_value_param(list, i).get<const QoreHashNode>()

//! returns a const QoreHashNode* from a hard typed hash param
#define HARD_QORE_VALUE_REF(list, i) get_hard_value_param(list, i).get<const ReferenceNode>()

//! returns a QoreObject* from a hard typed object param
#define HARD_QORE_VALUE_OBJECT(list, i) const_cast<QoreObject*>(get_hard_value_param(list, i).get<const QoreObject>())

//! sets up an object pointer
#define HARD_QORE_VALUE_OBJ_DATA(vname, Type, list, i, cid, dname, cname, xsink) HARD_QORE_VALUE_PARAM(obj_##vname, const QoreObject, list, i); ReferenceHolder<Type> vname(reinterpret_cast<Type*>(obj_##vname->getReferencedPrivateData(cid, xsink)), xsink); if (!vname && !*xsink) xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot complete call setup to %s() because parameter %d (<class %s>) has already been deleted", cname, i + 1, dname)

//! destructively sets up an object pointer; caller owns the pointer
#define TAKE_HARD_QORE_VALUE_OBJ_DATA(vname, Type, list, i, cid, dname, cname, xsink) HARD_QORE_VALUE_PARAM(obj_##vname, const QoreObject, list, i); Type* vname = reinterpret_cast<Type*>(const_cast<QoreObject*>(obj_##vname)->getAndClearPrivateData(cid, xsink)); if (!vname && !*xsink) xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot complete call setup to %s() because parameter %d (<class %s>) has already been deleted", cname, i + 1, dname); else if (vname) const_cast<QoreObject*>(obj_##vname)->doDelete(xsink)

//! sets up an object pointer
#define HARD_QORE_VALUE_OBJ_OR_NOTHING_DATA(vname, Type, list, i, cid, xsink) HARD_QORE_VALUE_OR_NOTHING_PARAM(obj_##vname, const QoreObject, list, i); ReferenceHolder<Type> vname(obj_##vname ? reinterpret_cast<Type*>(obj_##vname->getReferencedPrivateData(cid, xsink)) : nullptr, xsink);

//! returns the QoreEncoding corresponding to the string passed or a default encoding
static inline const QoreEncoding* get_value_encoding_param(const QoreListNode* n, size_t i, const QoreEncoding* def = QCS_DEFAULT) {
    const QoreStringNode* str = HARD_QORE_VALUE_STRING(n, i);
    return str ? QEM.findCreate(str) : def;
}

static inline const QoreEncoding* get_hard_qore_value_encoding_param(const QoreListNode* n, size_t i) {
    HARD_QORE_VALUE_PARAM(str, const QoreStringNode, n, i);
    return QEM.findCreate(str);
}

#endif
