/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreType.h

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

#ifndef _QORE_QORETYPE_H

#define _QORE_QORETYPE_H

#include <qore/common.h>
#include <qore/node_types.h>

#include <map>

// global default values
DLLEXPORT extern QoreListNode* emptyList;
DLLEXPORT extern QoreHashNode* emptyHash;
DLLEXPORT extern QoreStringNode* NullString;
DLLEXPORT extern DateTimeNode* ZeroDate;
DLLEXPORT extern QoreNumberNode* ZeroNumber, * InfinityNumber, * NaNumber, * piNumber;

DLLEXPORT extern QoreString NothingTypeString, NullTypeString, TrueString,
   FalseString, EmptyHashString, EmptyListString;

DLLEXPORT extern const QoreTypeInfo* anyTypeInfo,
   *autoTypeInfo,
   *autoNoNarrowTypeInfo,
   *bigIntTypeInfo,
   *floatTypeInfo,
   *boolTypeInfo,
   *stringTypeInfo,
   *binaryTypeInfo,
   *dateTypeInfo,
   *objectTypeInfo,
   *hashTypeInfo,
   *emptyHashTypeInfo,
   *autoHashTypeInfo,
   *autoNoNarrowHashTypeInfo,
   *listTypeInfo,
   *autoListTypeInfo,
   *autoNoNarrowListTypeInfo,
   *emptyListTypeInfo,
   *nothingTypeInfo,
   *nullTypeInfo,
   *numberTypeInfo,
   *runTimeClosureTypeInfo,
   *callReferenceTypeInfo,
   *referenceTypeInfo,
   *codeTypeInfo,                 // either closure or callref
   *hexBinaryTypeInfo,
   *base64BinaryTypeInfo,
   *base64UrlBinaryTypeInfo,
   *softBinaryTypeInfo,           // converts to binary from string and null
   *softBigIntTypeInfo,           // converts to int from float, string, bool, number, and null
   *softFloatTypeInfo,            // converts to float from int, string, bool, number, and null
   *softNumberTypeInfo,           // converts to number from int, string, bool, float, and null
   *softBoolTypeInfo,             // converts to bool from int, float, string, number, and null
   *softStringTypeInfo,           // converts to string from int, float, bool, number, and null
   *softDateTypeInfo,             // converts to date from int, float, bool, string, number, and null
   *softListTypeInfo,             // converts NOTHING -> empty list, list -> the same list, and everything else: list(arg)
   *softAutoListTypeInfo,
   *dataTypeInfo,                 // either string or binary
   *timeoutTypeInfo,              // accepts int or date and returns int giving timeout in milliseconds
   *bigIntOrFloatTypeInfo,        // accepts int or float and returns the same
   *bigIntFloatOrNumberTypeInfo,  // accepts int or float and returns the same
   *floatOrNumberTypeInfo,        // accepts float or number and returns the same

   *bigIntOrNothingTypeInfo,
   *floatOrNothingTypeInfo,
   *numberOrNothingTypeInfo,
   *stringOrNothingTypeInfo,
   *boolOrNothingTypeInfo,
   *binaryOrNothingTypeInfo,
   *objectOrNothingTypeInfo,
   *dateOrNothingTypeInfo,
   *hashOrNothingTypeInfo,
   *autoHashOrNothingTypeInfo,
   *autoNoNarrowHashOrNothingTypeInfo,
   *listOrNothingTypeInfo,
   *autoListOrNothingTypeInfo,
   *autoNoNarrowListOrNothingTypeInfo,
   *nullOrNothingTypeInfo,
   *codeOrNothingTypeInfo,
   *dataOrNothingTypeInfo,
   *referenceOrNothingTypeInfo,

   *hexBinaryOrNothingTypeInfo,
   *base64BinaryOrNothingTypeInfo,
   *base64UrlBinaryOrNothingTypeInfo,
   *softBinaryOrNothingTypeInfo,
   *softBigIntOrNothingTypeInfo,
   *softFloatOrNothingTypeInfo,
   *softNumberOrNothingTypeInfo,
   *softBoolOrNothingTypeInfo,
   *softStringOrNothingTypeInfo,
   *softDateOrNothingTypeInfo,
   *softListOrNothingTypeInfo,
   *softAutoListOrNothingTypeInfo,
   *timeoutOrNothingTypeInfo;

DLLEXPORT qore_type_t get_next_type_id();

//! true = not equal, false = equal
DLLEXPORT bool compareHard(const AbstractQoreNode* l, const AbstractQoreNode* r, ExceptionSink* xsink);
//! true = not equal, false = equal
DLLEXPORT bool compareSoft(const AbstractQoreNode* l, const AbstractQoreNode* r, ExceptionSink* xsink);
//! true = not equal, false = equal
DLLEXPORT bool q_compare_soft(const QoreValue l, const QoreValue r, ExceptionSink* xsink);

static inline QoreNumberNode* zero_number() {
   ZeroNumber->ref();
   return ZeroNumber;
}

static inline DateTimeNode* zero_date() {
   ZeroDate->ref();
   return ZeroDate;
}

static inline class QoreStringNode* null_string() {
   NullString->ref();
   return NullString;
}

static inline QoreListNode* empty_list() {
   emptyList->ref();
   return emptyList;
}

static inline QoreHashNode* empty_hash() {
   emptyHash->ref();
   return emptyHash;
}

static inline QoreNumberNode* pi_number() {
   piNumber->ref();
   return piNumber;
}

//! return type for type matching functions
enum qore_type_result_e {
   QTI_IGNORE      = -2,  //!< for internal use only
   QTI_UNASSIGNED  = -1,  //!< for internal use only

   QTI_NOT_EQUAL   =  0,  //!< types do not match
   QTI_WILDCARD    =  1,  //!< types match with a basic wildcard
   QTI_AMBIGUOUS   =  2,  //!< types match, but are not identical
   QTI_NEAR        =  3,  //!< types nearly match, but are not identical
   QTI_IDENT       =  4   //!< types match perfectly
};

DLLEXPORT int testObjectClassAccess(const QoreObject* obj, const QoreClass* classtoaccess);

DLLEXPORT const QoreClass* typeInfoGetUniqueReturnClass(const QoreTypeInfo* typeInfo);
DLLEXPORT bool typeInfoHasType(const QoreTypeInfo* typeInfo);
DLLEXPORT const char* typeInfoGetName(const QoreTypeInfo* typeInfo);
DLLEXPORT const QoreTypeInfo* typeInfoGetElementType(const QoreTypeInfo* typeInfo);
DLLEXPORT const TypedHashDecl* typeInfoGetTypedHash(const QoreTypeInfo* typeInfo);

DLLEXPORT qore_type_result_e typeInfoAcceptsType(const QoreTypeInfo* typeInfo, const QoreTypeInfo* otherTypeInfo);
DLLEXPORT qore_type_result_e typeInfoReturnsType(const QoreTypeInfo* typeInfo, const QoreTypeInfo* otherTypeInfo);

// Returns a non "or nothing" type for the given "or nothing" type
/** if the type is not an "or nothing" type, then the same type is returned

    @since %Qore 1.0
 */
DLLEXPORT const QoreTypeInfo* qore_get_value_type(const QoreTypeInfo* typeInfo);

DLLEXPORT const QoreTypeInfo* qore_get_or_nothing_type(const QoreTypeInfo* typeInfo);

// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_hash_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_hash_or_nothing_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_list_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_list_or_nothing_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_softlist_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_softlist_or_nothing_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_reference_type(const QoreTypeInfo* valueTypeInfo);
// @since %Qore 0.8.13
DLLEXPORT const QoreTypeInfo* qore_get_complex_reference_or_nothing_type(const QoreTypeInfo* valueTypeInfo);

// @since %Qore 0.9.4
DLLEXPORT const QoreTypeInfo* qore_get_type_from_string(const char* str, ExceptionSink& xsink);

// @since %Qore 1.0
DLLEXPORT const QoreClass* type_info_get_return_class(const QoreTypeInfo* typeInfo);

#endif // _QORE_QORETYPE_H
