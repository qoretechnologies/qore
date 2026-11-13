/*
    QoreType.cpp

    extensible and type system for the Qore programming language

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

#include <qore/Qore.h>

#include "qore/intern/QoreClassIntern.h"

#include <cassert>
#include <cstring>
#include <typeinfo>

QoreString NothingTypeString("<NOTHING>");
QoreString NullTypeString("<NULL>");
QoreString TrueString("True");
QoreString FalseString("False");
QoreString EmptyHashString("<EMPTY HASH>");
QoreString EmptyListString("<EMPTY LIST>");

static qore_type_t lastid = QORE_NUM_TYPES;

class QoreTypeManager {
public:
   DLLLOCAL QoreTypeManager() {}
   DLLLOCAL ~QoreTypeManager() {}
};

static QoreTypeManager QTM;

// default value nodes for builtin types
QoreNothingNode Nothing;
QoreNullNode Null;

qore_type_t get_next_type_id() {
   return lastid++;
}

// 0 = equal, 1 = not equal
bool compareHard(const AbstractQoreNode* l, const AbstractQoreNode* r, ExceptionSink *xsink) {
   assert(xsink);
   if (is_nothing(l)) {
      if (is_nothing(r))
         return 0;
      else
         return 1;
   }

   if (is_nothing(r))
      return 1;

   return !l->is_equal_hard(r, xsink);
}

// this function calls the operator function that will
// convert values to do the conversion
// false = equal, true = not equal
bool compareSoft(const AbstractQoreNode* l, const AbstractQoreNode* r, ExceptionSink *xsink) {
   return !QoreLogicalEqualsOperatorNode::softEqualNative(QoreValue(l), QoreValue(r), xsink);
}

bool q_compare_soft(const QoreValue l, const QoreValue r, ExceptionSink *xsink) {
   return !QoreLogicalEqualsOperatorNode::softEqualNative(QoreValue(l), QoreValue(r), xsink);
}

int testObjectClassAccess(const QoreObject* obj, const QoreClass* shouldbeclass) {
   return qore_class_private::runtimeCheckCompatibleClass(*shouldbeclass, *(obj->getClass()));
}

const QoreClass* typeInfoGetUniqueReturnClass(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::getUniqueReturnClass(typeInfo);
}

const QoreClass* type_info_get_return_class(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::getReturnClass(typeInfo);
}

qore_type_result_e typeInfoAcceptsType(const QoreTypeInfo* typeInfo, const QoreTypeInfo* otherTypeInfo) {
   return QoreTypeInfo::parseAccepts(typeInfo, otherTypeInfo);
}

qore_type_result_e typeInfoReturnsType(const QoreTypeInfo* typeInfo, const QoreTypeInfo* otherTypeInfo) {
   return QoreTypeInfo::parseAccepts(otherTypeInfo, typeInfo);
}

bool typeInfoHasType(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::hasType(typeInfo);
}

const char* typeInfoGetName(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::getName(typeInfo);
}

const QoreTypeInfo* typeInfoGetElementType(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::getElementType(typeInfo);
}

const TypedHashDecl* typeInfoGetTypedHash(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::getTypedHash(typeInfo);
}
