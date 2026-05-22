/*
    QoreTypeInfo.cpp

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

#include <qore/Qore.h>
#include <qore/QoreRWLock.h>
#include <unordered_set>
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_number_private.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/QorePluginRegistry.h"
#include "qore/intern/QoreTypeSpecMatchRegistry.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/QoreIteratorBase.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>

const QoreAnyTypeInfo staticAnyTypeInfo;
const QoreAutoTypeInfo staticAutoTypeInfo;
const QoreAutoNoNarrowTypeInfo staticAutoNoNarrowTypeInfo;

const QoreBigIntTypeInfo staticBigIntTypeInfo;
const QoreBigIntOrNothingTypeInfo staticBigIntOrNothingTypeInfo;

const QoreStringTypeInfo staticStringTypeInfo;
const QoreStringOrNothingTypeInfo staticStringOrNothingTypeInfo;

const QoreBoolTypeInfo staticBoolTypeInfo;
const QoreBoolOrNothingTypeInfo staticBoolOrNothingTypeInfo;

const QoreBinaryTypeInfo staticBinaryTypeInfo;
const QoreBinaryOrNothingTypeInfo staticBinaryOrNothingTypeInfo;

const QoreSoftBinaryTypeInfo staticSoftBinaryTypeInfo;
const QoreSoftBinaryOrNothingTypeInfo staticSoftBinaryOrNothingTypeInfo;

const QoreHexBinaryTypeInfo staticHexBinaryTypeInfo;
const QoreHexBinaryOrNothingTypeInfo staticHexBinaryOrNothingTypeInfo;
const QoreBase64BinaryTypeInfo staticBase64BinaryTypeInfo;
const QoreBase64BinaryOrNothingTypeInfo staticBase64BinaryOrNothingTypeInfo;
const QoreBase64UrlBinaryTypeInfo staticBase64UrlBinaryTypeInfo;
const QoreBase64UrlBinaryOrNothingTypeInfo staticBase64UrlBinaryOrNothingTypeInfo;

const QoreObjectTypeInfo staticObjectTypeInfo;
const QoreObjectOrNothingTypeInfo staticObjectOrNothingTypeInfo;

const QoreDateTypeInfo staticDateTypeInfo;
const QoreDateOrNothingTypeInfo staticDateOrNothingTypeInfo;
const QoreAbsoluteDateTypeInfo staticAbsoluteDateTypeInfo;
const QoreAbsoluteDateOrNothingTypeInfo staticAbsoluteDateOrNothingTypeInfo;
const QoreRelativeDateTypeInfo staticRelativeDateTypeInfo;
const QoreRelativeDateOrNothingTypeInfo staticRelativeDateOrNothingTypeInfo;

const QoreHashTypeInfo staticHashTypeInfo;
const QoreHashOrNothingTypeInfo staticHashOrNothingTypeInfo;
const QoreEmptyHashTypeInfo staticEmptyHashTypeInfo;

const QoreAutoHashTypeInfo staticAutoHashTypeInfo;
const QoreAutoHashOrNothingTypeInfo staticAutoHashOrNothingTypeInfo;
const QoreAutoNoNarrowHashTypeInfo staticAutoNoNarrowHashTypeInfo;
const QoreAutoNoNarrowHashOrNothingTypeInfo staticAutoNoNarrowHashOrNothingTypeInfo;

const QoreListTypeInfo staticListTypeInfo;
const QoreListOrNothingTypeInfo staticListOrNothingTypeInfo;
const QoreEmptyListTypeInfo staticEmptyListTypeInfo;

const QoreBufferTypeInfo staticBufferTypeInfo;
const QoreBufferOrNothingTypeInfo staticBufferOrNothingTypeInfo;

const QoreAutoListTypeInfo staticAutoListTypeInfo;
const QoreAutoListOrNothingTypeInfo staticAutoListOrNothingTypeInfo;
const QoreAutoNoNarrowListTypeInfo staticAutoNoNarrowListTypeInfo;
const QoreAutoNoNarrowListOrNothingTypeInfo staticAutoNoNarrowListOrNothingTypeInfo;

const QoreNothingTypeInfo staticNothingTypeInfo;

const QoreNullTypeInfo staticNullTypeInfo;
const QoreNullOrNothingTypeInfo staticNullOrNothingTypeInfo;

const QoreClosureTypeInfo staticClosureTypeInfo;
const QoreClosureOrNothingTypeInfo staticClosureOrNothingTypeInfo;

const QoreCallReferenceTypeInfo staticCallReferenceTypeInfo;
const QoreCallReferenceOrNothingTypeInfo staticCallReferenceOrNothingTypeInfo;

const QoreReferenceTypeInfo staticReferenceTypeInfo;
const QoreReferenceOrNothingTypeInfo staticReferenceOrNothingTypeInfo;

const QoreHardReferenceTypeInfo staticHardReferenceTypeInfo;
const QoreHardReferenceOrNothingTypeInfo staticHardReferenceOrNothingTypeInfo;

const QoreNumberTypeInfo staticNumberTypeInfo;
const QoreNumberOrNothingTypeInfo staticNumberOrNothingTypeInfo;

const QoreFloatTypeInfo staticFloatTypeInfo;
const QoreFloatOrNothingTypeInfo staticFloatOrNothingTypeInfo;

const QoreCodeTypeInfo staticCodeTypeInfo;
const QoreCodeOrNothingTypeInfo staticCodeOrNothingTypeInfo;

const QoreDataTypeInfo staticDataTypeInfo;
const QoreDataOrNothingTypeInfo staticDataOrNothingTypeInfo;

const QoreSoftBigIntTypeInfo staticSoftBigIntTypeInfo;
const QoreSoftBigIntOrNothingTypeInfo staticSoftBigIntOrNothingTypeInfo;

const QoreSoftFloatTypeInfo staticSoftFloatTypeInfo;
const QoreSoftFloatOrNothingTypeInfo staticSoftFloatOrNothingTypeInfo;

const QoreSoftNumberTypeInfo staticSoftNumberTypeInfo;
const QoreSoftNumberOrNothingTypeInfo staticSoftNumberOrNothingTypeInfo;

const QoreSoftBoolTypeInfo staticSoftBoolTypeInfo;
const QoreSoftBoolOrNothingTypeInfo staticSoftBoolOrNothingTypeInfo;

const QoreSoftStringTypeInfo staticSoftStringTypeInfo;
const QoreSoftStringOrNothingTypeInfo staticSoftStringOrNothingTypeInfo;

const QoreSoftDateTypeInfo staticSoftDateTypeInfo;
const QoreSoftDateOrNothingTypeInfo staticSoftDateOrNothingTypeInfo;

const QoreSoftListTypeInfo staticSoftListTypeInfo;
const QoreSoftListOrNothingTypeInfo staticSoftListOrNothingTypeInfo;

const QoreSoftAutoListTypeInfo staticSoftAutoListTypeInfo;
const QoreSoftAutoListOrNothingTypeInfo staticSoftAutoListOrNothingTypeInfo;

const QoreTimeoutTypeInfo staticTimeoutTypeInfo;
const QoreTimeoutOrNothingTypeInfo staticTimeoutOrNothingTypeInfo;

const QoreIntOrFloatTypeInfo staticIntOrFloatTypeInfo;

const QoreIntFloatOrNumberTypeInfo staticIntFloatOrNumberTypeInfo;

const QoreFloatOrNumberTypeInfo staticFloatOrNumberTypeInfo;

const QoreTypeInfo* anyTypeInfo = &staticAnyTypeInfo,
   *autoTypeInfo = &staticAutoTypeInfo,
   *autoNoNarrowTypeInfo = &staticAutoNoNarrowTypeInfo,
   *bigIntTypeInfo = &staticBigIntTypeInfo,
   *floatTypeInfo = &staticFloatTypeInfo,
   *boolTypeInfo = &staticBoolTypeInfo,
   *stringTypeInfo = &staticStringTypeInfo,
   *binaryTypeInfo = &staticBinaryTypeInfo,
   *dateTypeInfo = &staticDateTypeInfo,
   *dateAbsoluteTypeInfo = &staticAbsoluteDateTypeInfo,
   *dateRelativeTypeInfo = &staticRelativeDateTypeInfo,
   *objectTypeInfo = &staticObjectTypeInfo,
   *hashTypeInfo = &staticHashTypeInfo,
   *emptyHashTypeInfo = &staticEmptyHashTypeInfo,
   *autoHashTypeInfo = &staticAutoHashTypeInfo,
   *autoNoNarrowHashTypeInfo = &staticAutoNoNarrowHashTypeInfo,
   *listTypeInfo = &staticListTypeInfo,
   *bufferTypeInfo = &staticBufferTypeInfo,
   *autoListTypeInfo = &staticAutoListTypeInfo,
   *autoNoNarrowListTypeInfo = &staticAutoNoNarrowListTypeInfo,
   *emptyListTypeInfo = &staticEmptyListTypeInfo,
   *nothingTypeInfo = &staticNothingTypeInfo,
   *nullTypeInfo = &staticNullTypeInfo,
   *numberTypeInfo = &staticNumberTypeInfo,
   *runTimeClosureTypeInfo = &staticClosureTypeInfo,
   *callReferenceTypeInfo = &staticCallReferenceTypeInfo,
   *referenceTypeInfo = &staticReferenceTypeInfo,
   *codeTypeInfo = &staticCodeTypeInfo,
   *hexBinaryTypeInfo = &staticHexBinaryTypeInfo,
   *base64BinaryTypeInfo = &staticBase64BinaryTypeInfo,
   *base64UrlBinaryTypeInfo = &staticBase64UrlBinaryTypeInfo,
   *softBinaryTypeInfo = &staticSoftBinaryTypeInfo,
   *softBigIntTypeInfo = &staticSoftBigIntTypeInfo,
   *softFloatTypeInfo = &staticSoftFloatTypeInfo,
   *softNumberTypeInfo = &staticSoftNumberTypeInfo,
   *softBoolTypeInfo = &staticSoftBoolTypeInfo,
   *softStringTypeInfo = &staticSoftStringTypeInfo,
   *softDateTypeInfo = &staticSoftDateTypeInfo,
   *softListTypeInfo = &staticSoftListTypeInfo,
   *softAutoListTypeInfo = &staticSoftAutoListTypeInfo,
   *dataTypeInfo = &staticDataTypeInfo,
   *timeoutTypeInfo = &staticTimeoutTypeInfo,
   *bigIntOrFloatTypeInfo = &staticIntOrFloatTypeInfo,
   *bigIntFloatOrNumberTypeInfo = &staticIntFloatOrNumberTypeInfo,
   *floatOrNumberTypeInfo = &staticFloatOrNumberTypeInfo,

   *bigIntOrNothingTypeInfo = &staticBigIntOrNothingTypeInfo,
   *floatOrNothingTypeInfo = &staticFloatOrNothingTypeInfo,
   *numberOrNothingTypeInfo = &staticNumberOrNothingTypeInfo,
   *stringOrNothingTypeInfo = &staticStringOrNothingTypeInfo,
   *boolOrNothingTypeInfo = &staticBoolOrNothingTypeInfo,
   *binaryOrNothingTypeInfo = &staticBinaryOrNothingTypeInfo,
   *objectOrNothingTypeInfo = &staticObjectOrNothingTypeInfo,
   *dateOrNothingTypeInfo = &staticDateOrNothingTypeInfo,
   *dateAbsoluteOrNothingTypeInfo = &staticAbsoluteDateOrNothingTypeInfo,
   *dateRelativeOrNothingTypeInfo = &staticRelativeDateOrNothingTypeInfo,
   *hashOrNothingTypeInfo = &staticHashOrNothingTypeInfo,
   *autoHashOrNothingTypeInfo = &staticAutoHashOrNothingTypeInfo,
   *autoNoNarrowHashOrNothingTypeInfo = &staticAutoNoNarrowHashOrNothingTypeInfo,
   *listOrNothingTypeInfo = &staticListOrNothingTypeInfo,
   *bufferOrNothingTypeInfo = &staticBufferOrNothingTypeInfo,
   *autoListOrNothingTypeInfo = &staticAutoListOrNothingTypeInfo,
   *autoNoNarrowListOrNothingTypeInfo = &staticAutoNoNarrowListOrNothingTypeInfo,
   *nullOrNothingTypeInfo = &staticNullOrNothingTypeInfo,
   *codeOrNothingTypeInfo = &staticCodeOrNothingTypeInfo,
   *dataOrNothingTypeInfo = &staticDataOrNothingTypeInfo,
   *referenceOrNothingTypeInfo = &staticReferenceOrNothingTypeInfo,

   *hexBinaryOrNothingTypeInfo = &staticHexBinaryOrNothingTypeInfo,
   *base64BinaryOrNothingTypeInfo = &staticBase64BinaryOrNothingTypeInfo,
   *base64UrlBinaryOrNothingTypeInfo = &staticBase64UrlBinaryOrNothingTypeInfo,
   *softBinaryOrNothingTypeInfo = &staticSoftBinaryOrNothingTypeInfo,
   *softBigIntOrNothingTypeInfo = &staticSoftBigIntOrNothingTypeInfo,
   *softFloatOrNothingTypeInfo = &staticSoftFloatOrNothingTypeInfo,
   *softNumberOrNothingTypeInfo = &staticSoftNumberOrNothingTypeInfo,
   *softBoolOrNothingTypeInfo = &staticSoftBoolOrNothingTypeInfo,
   *softStringOrNothingTypeInfo = &staticSoftStringOrNothingTypeInfo,
   *softDateOrNothingTypeInfo = &staticSoftDateOrNothingTypeInfo,
   *softListOrNothingTypeInfo = &staticSoftListOrNothingTypeInfo,
   *softAutoListOrNothingTypeInfo = &staticSoftAutoListOrNothingTypeInfo,
   *timeoutOrNothingTypeInfo = &staticTimeoutOrNothingTypeInfo;

QoreListNode* emptyList;
QoreHashNode* emptyHash;
QoreStringNode* NullString;
DateTimeNode* ZeroDate, * OneDate;
QoreNumberNode* ZeroNumber, * NaNumber, * InfinityNumber, * piNumber;

// map from names used when parsing to types
typedef std::map<const char* , const QoreTypeInfo* , ltstr> str_typeinfo_map_t;
static str_typeinfo_map_t str_typeinfo_map;
static str_typeinfo_map_t str_ornothingtypeinfo_map;

// map from types to type info
typedef std::map<qore_type_t, const QoreTypeInfo* > type_typeinfo_map_t;
static type_typeinfo_map_t type_typeinfo_map;
static type_typeinfo_map_t type_ornothingtypeinfo_map;

// global external type map
static type_typeinfo_map_t extern_type_info_map;

// map from types to names
typedef std::map<qore_type_t, const char* > type_str_map_t;
static type_str_map_t type_str_map;

// map from simple types to "or nothing" types
typedef std::map<const QoreTypeInfo*, const QoreTypeInfo*> typeinfo_map_t;
static typeinfo_map_t typeinfo_map, typeinfo_or_nothing_map;

static QoreThreadLock ctl; // complex type lock

// True if ti is auto or auto! - both behave identically for runtime acceptance and
// parse-time match logic; auto! is only distinct as a parse-time narrowing marker.
static inline bool is_auto_vti(const QoreTypeInfo* ti) {
    return ti == autoTypeInfo || ti == autoNoNarrowTypeInfo;
}

typedef std::map<const QoreTypeInfo*, QoreTypeInfo*> tmap_t;
tmap_t ch_map,          // complex hash map
   chon_map,            // complex hash or nothing map
   cl_map,              // complex list map
   clon_map,            // complex list or nothing map
   chr_map,             // complex hard reference map
   cr_map,              // complex reference map
   cron_map,            // complex reference or nothing map
   csl_map,             // complex softlist map
   cslon_map;           // complex softlist or nothing map

struct ComplexBufferCacheKey {
    QoreBufferElementType elementType;
    bool nullableElements;

    bool operator<(const ComplexBufferCacheKey& other) const {
        if (elementType != other.elementType) {
            return elementType < other.elementType;
        }
        return nullableElements < other.nullableElements;
    }
};

typedef std::map<ComplexBufferCacheKey, QoreTypeInfo*> complex_buffer_map_t;
static complex_buffer_map_t cb_map,       // complex buffer map
                            cbon_map;     // complex buffer or nothing map

// Cache for union types - keyed by sorted vector of member types
typedef std::map<type_vec_t, QoreUnionTypeInfo*> union_map_t;
static union_map_t union_map,       // union types
                   union_on_map;    // union or-nothing types

// Cache key for typed callable types: (returnType, paramTypes, varargs, or_nothing)
struct ComplexCodeCacheKey {
    const QoreTypeInfo* returnType;
    type_vec_t paramTypes;
    bool varargs;
    bool orNothing;

    bool operator<(const ComplexCodeCacheKey& other) const {
        if (returnType != other.returnType) return returnType < other.returnType;
        if (paramTypes.size() != other.paramTypes.size()) return paramTypes.size() < other.paramTypes.size();
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (paramTypes[i] != other.paramTypes[i]) return paramTypes[i] < other.paramTypes[i];
        }
        if (varargs != other.varargs) return varargs < other.varargs;
        return orNothing < other.orNothing;
    }
};

// Cache for typed callable types
typedef std::map<ComplexCodeCacheKey, QoreComplexCodeTypeInfo*> complex_code_map_t;
static complex_code_map_t complex_code_map;

struct ParameterizedClassCacheKey {
    const QoreClass* baseClass;
    type_vec_t typeArgs;
    bool orNothing;

    bool operator<(const ParameterizedClassCacheKey& other) const {
        if (baseClass != other.baseClass) return baseClass < other.baseClass;
        if (typeArgs.size() != other.typeArgs.size()) return typeArgs.size() < other.typeArgs.size();
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (typeArgs[i] != other.typeArgs[i]) return typeArgs[i] < other.typeArgs[i];
        }
        return orNothing < other.orNothing;
    }
};

typedef std::map<ParameterizedClassCacheKey, QoreParameterizedClassTypeInfo*> parameterized_class_map_t;
static parameterized_class_map_t parameterized_class_map;

enum TypeParameterOwnerKind : unsigned {
    TPO_CLASS = 0,
    TPO_HASHDECL = 1,
    TPO_SIGNATURE = 2,
};

struct TypeParameterCacheKey {
    const void* owner;
    unsigned ownerKind;
    size_t index;
    bool orNothing;

    bool operator<(const TypeParameterCacheKey& other) const {
        if (ownerKind != other.ownerKind) return ownerKind < other.ownerKind;
        if (owner != other.owner) return owner < other.owner;
        if (index != other.index) return index < other.index;
        return orNothing < other.orNothing;
    }
};

typedef std::map<TypeParameterCacheKey, QoreTypeParameterTypeInfo*> type_parameter_map_t;
static type_parameter_map_t type_parameter_map;

struct WildcardTypeCacheKey {
    QoreWildcardKind kind;
    const QoreTypeInfo* bound;

    bool operator<(const WildcardTypeCacheKey& other) const {
        if (kind != other.kind) {
            return kind < other.kind;
        }
        return bound < other.bound;
    }
};

typedef std::map<WildcardTypeCacheKey, QoreWildcardTypeInfo*> wildcard_type_map_t;
static wildcard_type_map_t wildcard_type_map;

// rwlock for global type map
static QoreRWLock extern_type_info_map_lock;

static void do_maps(qore_type_t t, const char* name, const QoreTypeInfo* typeInfo,
        const QoreTypeInfo* orNothingTypeInfo) {
   str_typeinfo_map[name]                     = typeInfo;
   str_ornothingtypeinfo_map[name]            = orNothingTypeInfo;
   type_typeinfo_map[t]                       = typeInfo;
   type_ornothingtypeinfo_map[t]              = orNothingTypeInfo;
   type_str_map[t]                            = name;
   typeinfo_map[typeInfo]                     = orNothingTypeInfo;
   typeinfo_or_nothing_map[orNothingTypeInfo] = typeInfo;
}

static void do_maps(const QoreTypeInfo* typeInfo, const QoreTypeInfo* orNothingTypeInfo) {
   typeinfo_map[typeInfo]                     = orNothingTypeInfo;
   typeinfo_or_nothing_map[orNothingTypeInfo] = typeInfo;
}

// at least the NullString must be created after the default character encoding is set
void init_qore_types() {
    // initialize global default values
    NullString     = new QoreStringNode;
    ZeroDate       = DateTimeNode::makeAbsolute(0, 0, 0);
    OneDate        = DateTimeNode::makeAbsolute(0, 0, 0, 0, 0, 1);
    ZeroNumber     = new QoreNumberNode;
    NaNumber       = qore_number_private::getNaNumber();
    InfinityNumber = qore_number_private::getInfinity();
    piNumber       = qore_number_private::getPi();

    emptyList      = new QoreListNode;
    emptyHash      = new QoreHashNode;

    do_maps(NT_INT,             "int", bigIntTypeInfo, bigIntOrNothingTypeInfo);
    do_maps(NT_STRING,          "string", stringTypeInfo, stringOrNothingTypeInfo);
    do_maps(NT_BOOLEAN,         "bool", boolTypeInfo, boolOrNothingTypeInfo);
    do_maps(NT_FLOAT,           "float", floatTypeInfo, floatOrNothingTypeInfo);
    do_maps(NT_NUMBER,          "number", numberTypeInfo, numberOrNothingTypeInfo);
    do_maps(NT_BINARY,          "binary", binaryTypeInfo, binaryOrNothingTypeInfo);
    do_maps(NT_LIST,            "list", listTypeInfo, listOrNothingTypeInfo);
    do_maps(NT_BUFFER,          "buffer", bufferTypeInfo, bufferOrNothingTypeInfo);
    do_maps(NT_HASH,            "hash", hashTypeInfo, hashOrNothingTypeInfo);
    do_maps(NT_OBJECT,          "object", objectTypeInfo, objectOrNothingTypeInfo);
    do_maps(NT_ALL,             "any", anyTypeInfo, anyTypeInfo);
    do_maps(NT_ALL,             "auto", autoTypeInfo, autoTypeInfo);
    // auto! - marker type for auto with no type narrowing; uses same or_nothing type as auto
    // (auto! already accepts nothing, so *auto! is the same as auto!)
    str_typeinfo_map["auto!"] = autoNoNarrowTypeInfo;
    str_ornothingtypeinfo_map["auto!"] = autoNoNarrowTypeInfo;
    do_maps(NT_DATE,            "date", dateTypeInfo, dateOrNothingTypeInfo);
    do_maps(NT_CODE,            "code", codeTypeInfo, codeOrNothingTypeInfo);
    do_maps(NT_DATA,            "data", dataTypeInfo, dataOrNothingTypeInfo);
    do_maps(NT_REFERENCE,       "reference", referenceTypeInfo, referenceOrNothingTypeInfo);
    do_maps(NT_NULL,            "null", nullTypeInfo, nullOrNothingTypeInfo);
    do_maps(NT_NOTHING,         "nothing", nothingTypeInfo, nothingTypeInfo);

    do_maps(NT_SOFTINT,         "softint", softBigIntTypeInfo, softBigIntOrNothingTypeInfo);
    do_maps(NT_SOFTFLOAT,       "softfloat", softFloatTypeInfo, softFloatOrNothingTypeInfo);
    do_maps(NT_SOFTNUMBER,      "softnumber", softNumberTypeInfo, softNumberOrNothingTypeInfo);
    do_maps(NT_SOFTBOOLEAN,     "softbool", softBoolTypeInfo, softBoolOrNothingTypeInfo);
    do_maps(NT_SOFTSTRING,      "softstring", softStringTypeInfo, softStringOrNothingTypeInfo);
    do_maps(NT_SOFTDATE,        "softdate", softDateTypeInfo, softDateOrNothingTypeInfo);
    do_maps(NT_SOFTLIST,        "softlist", softListTypeInfo, softListOrNothingTypeInfo);
    do_maps(NT_SOFTBINARY,      "softbinary", softBinaryTypeInfo, softBinaryOrNothingTypeInfo);
    do_maps(NT_HEXBINARY,       "hexbinary", hexBinaryTypeInfo, hexBinaryOrNothingTypeInfo);
    do_maps(NT_BASE64BINARY,    "base64binary", base64BinaryTypeInfo, base64BinaryOrNothingTypeInfo);
    do_maps(NT_BASE64URLBINARY, "base64urlbinary", base64UrlBinaryTypeInfo, base64UrlBinaryOrNothingTypeInfo);

    do_maps(NT_TIMEOUT,         "timeout", timeoutTypeInfo, timeoutOrNothingTypeInfo);

    // map the closure and callref strings to codeTypeInfo to ensure that these
    // types are always interchangeable
    do_maps(NT_RUNTIME_CLOSURE, "closure", codeTypeInfo, codeOrNothingTypeInfo);
    do_maps(NT_FUNCREF, "callref", codeTypeInfo, codeOrNothingTypeInfo);

    do_maps(autoListTypeInfo, autoListOrNothingTypeInfo);
    do_maps(autoHashTypeInfo, autoHashOrNothingTypeInfo);
    do_maps(softAutoListTypeInfo, softAutoListOrNothingTypeInfo);
}

void delete_qore_types() {
    // dereference global default values
    NullString->deref();
    piNumber->deref();
    InfinityNumber->deref();
    NaNumber->deref();
    ZeroNumber->deref();
    OneDate->deref();
    ZeroDate->deref();
    emptyList->deref(nullptr);
    emptyHash->deref(nullptr);

    // delete stored type information
    for (auto& i : ch_map)
        delete i.second;
    for (auto& i : chon_map)
        delete i.second;
    for (auto& i : cl_map)
        delete i.second;
    for (auto& i : clon_map)
        delete i.second;
    for (auto& i : chr_map)
        delete i.second;
    for (auto& i : cr_map)
        delete i.second;
    for (auto& i : cron_map)
        delete i.second;
    for (auto& i : csl_map)
        delete i.second;
    for (auto& i : cslon_map)
        delete i.second;
    for (auto& i : cb_map)
        delete i.second;
    for (auto& i : cbon_map)
        delete i.second;
    // Clean up union type cache
    for (auto& i : union_map)
        delete i.second;
    for (auto& i : union_on_map)
        delete i.second;
    // Clean up typed callable type cache
    for (auto& i : complex_code_map)
        delete i.second;
    // Clean up parameterized class type cache
    for (auto& i : parameterized_class_map)
        delete i.second;
    // Clean up symbolic class type parameter cache
    for (auto& i : type_parameter_map)
        delete i.second;
    // Clean up wildcard type argument cache
    for (auto& i : wildcard_type_map)
        delete i.second;
}

void add_to_type_map(qore_type_t t, const QoreTypeInfo* typeInfo) {
   QoreAutoRWWriteLocker al(extern_type_info_map_lock);
   assert(extern_type_info_map.find(t) == extern_type_info_map.end());
   extern_type_info_map[t] = typeInfo;
}

static const QoreTypeInfo* get_value_type_intern(const QoreTypeInfo* typeInfo) {
    assert(QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING));

    typeinfo_map_t::iterator i = typeinfo_or_nothing_map.find(typeInfo);
    if (i != typeinfo_or_nothing_map.end()) {
        return i->second;
    }

    // see if we have a complex type
    {
        const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(typeInfo);
        if (hd) {
            return hd->getTypeInfo();
        }
    }

    {
        const QoreClass* qc = QoreTypeInfo::getReturnClass(typeInfo);
        if (qc) {
            return qc->getTypeInfo();
        }
    }

    {
        const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(typeInfo);
        if (pti) {
            return qore_get_parameterized_class_type(pti->getBaseClass(), pti->getTypeArgs(), false);
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
        if (ti) {
            return qore_get_complex_hash_type(ti);
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
        if (ti) {
            return qore_get_complex_list_type(ti);
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexBufferOrNothing(typeInfo);
        if (ti) {
            const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(ti);
            assert(bti);
            return qore_get_complex_buffer_type(bti->getBufferElementType(), bti->hasNullableElements());
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReferenceTarget(typeInfo);
        if (ti) {
            return qore_get_complex_reference_type(ti);
        }
    }

    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    return autoTypeInfo;
}

const QoreTypeInfo* get_value_type(const QoreTypeInfo* typeInfo) {
   return !QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING) ? typeInfo : get_value_type_intern(typeInfo);
}

// public API
const QoreTypeInfo* qore_get_value_type(const QoreTypeInfo* typeInfo) {
    return get_value_type(typeInfo);
}

// public API
const QoreTypeInfo* qore_get_or_nothing_type(const QoreTypeInfo* typeInfo) {
   return get_or_nothing_type_check(typeInfo);
}

const QoreTypeInfo* get_or_nothing_type_check(const QoreTypeInfo* typeInfo) {
   return QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING) ? typeInfo : get_or_nothing_type(typeInfo);
}

const QoreTypeInfo* get_or_nothing_type(const QoreTypeInfo* typeInfo) {
    assert(!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING));

    typeinfo_map_t::iterator i = typeinfo_map.find(typeInfo);
    if (i != typeinfo_map.end())
        return i->second;

    // see if we have a complex type
    {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
        if (hd)
            return hd->getTypeInfo(true);
    }

    {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(typeInfo);
        if (qc) {
            return qc->getOrNothingTypeInfo();
        }
    }

    {
        const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(typeInfo);
        if (pti) {
            return qore_get_parameterized_class_type(pti->getBaseClass(), pti->getTypeArgs(), true);
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
        if (ti)
            return qore_get_complex_hash_or_nothing_type(ti);
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexSoftList(typeInfo);
        if (ti)
            return qore_get_complex_softlist_or_nothing_type(ti);
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexList(typeInfo);
        if (ti)
            return qore_get_complex_list_or_nothing_type(ti);
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexBuffer(typeInfo);
        if (ti) {
            const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(ti);
            assert(bti);
            return qore_get_complex_buffer_or_nothing_type(bti->getBufferElementType(),
                bti->hasNullableElements());
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexReference(typeInfo);
        if (ti)
            return qore_get_complex_reference_or_nothing_type(ti);
    }

    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    return autoTypeInfo;
}

const QoreTypeInfo* qore_get_complex_hash_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return autoHashTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return hashTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = ch_map.lower_bound(vti);
    if (i != ch_map.end() && i->first == vti)
        return i->second;

    QoreComplexHashTypeInfo* ti = new QoreComplexHashTypeInfo(vti);
    ch_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_hash_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return autoHashOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return hashOrNothingTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = chon_map.lower_bound(vti);
    if (i != chon_map.end() && i->first == vti)
        return i->second;

    QoreComplexHashOrNothingTypeInfo* ti = new QoreComplexHashOrNothingTypeInfo(vti);
    chon_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_list_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return autoListTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return listTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = cl_map.lower_bound(vti);
    if (i != cl_map.end() && i->first == vti)
        return i->second;

    QoreComplexListTypeInfo* ti = new QoreComplexListTypeInfo(vti);
    cl_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_list_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return autoListOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return listOrNothingTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = clon_map.lower_bound(vti);
    if (i != clon_map.end() && i->first == vti)
        return i->second;

    QoreComplexListOrNothingTypeInfo* ti = new QoreComplexListOrNothingTypeInfo(vti);
    clon_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

static QoreString qore_make_complex_buffer_name(QoreBufferElementType element_type, bool nullable_elements,
        bool or_nothing) {
    QoreString rv;
    if (or_nothing) {
        rv.concat('*');
    }
    rv.concat("buffer<");
    if (nullable_elements) {
        rv.concat('*');
    }
    rv.concat(qore_buffer_element_type_name(element_type));
    rv.concat('>');
    return rv;
}

const QoreTypeInfo* qore_get_complex_buffer_type(QoreBufferElementType elementType, bool nullableElements) {
    assert(elementType != QoreBufferElementType::Invalid);

    AutoLocker al(ctl);

    ComplexBufferCacheKey key{elementType, nullableElements};
    complex_buffer_map_t::iterator i = cb_map.lower_bound(key);
    if (i != cb_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreComplexBufferTypeInfo* ti = new QoreComplexBufferTypeInfo(elementType, nullableElements);
    cb_map.insert(i, complex_buffer_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_buffer_or_nothing_type(QoreBufferElementType elementType,
        bool nullableElements) {
    assert(elementType != QoreBufferElementType::Invalid);

    const QoreTypeInfo* value_type = qore_get_complex_buffer_type(elementType, nullableElements);

    AutoLocker al(ctl);

    ComplexBufferCacheKey key{elementType, nullableElements};
    complex_buffer_map_t::iterator i = cbon_map.lower_bound(key);
    if (i != cbon_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreComplexBufferOrNothingTypeInfo* ti = new QoreComplexBufferOrNothingTypeInfo(elementType, nullableElements,
        value_type);
    cbon_map.insert(i, complex_buffer_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_softlist_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return softAutoListTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return softListTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = csl_map.lower_bound(vti);
    if (i != csl_map.end() && i->first == vti)
        return i->second;

    QoreComplexSoftListTypeInfo* ti = new QoreComplexSoftListTypeInfo(vti);
    csl_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_softlist_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo) {
        return softAutoListOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return softListOrNothingTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = cslon_map.lower_bound(vti);
    if (i != cslon_map.end() && i->first == vti)
        return i->second;

    QoreComplexSoftListOrNothingTypeInfo* ti = new QoreComplexSoftListOrNothingTypeInfo(vti);
    cslon_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_hard_reference_type(const QoreTypeInfo* vti) {
    AutoLocker al(ctl);

    tmap_t::iterator i = chr_map.lower_bound(vti);
    if (i != chr_map.end() && i->first == vti)
        return i->second;

    QoreComplexHardReferenceTypeInfo* ti = new QoreComplexHardReferenceTypeInfo(vti);
    chr_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_reference_type(const QoreTypeInfo* vti) {
    AutoLocker al(ctl);

    tmap_t::iterator i = cr_map.lower_bound(vti);
    if (i != cr_map.end() && i->first == vti)
        return i->second;

    QoreComplexReferenceTypeInfo* ti = new QoreComplexReferenceTypeInfo(vti);
    cr_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_reference_or_nothing_type(const QoreTypeInfo* vti) {
    AutoLocker al(ctl);

    tmap_t::iterator i = cron_map.lower_bound(vti);
    if (i != cron_map.end() && i->first == vti)
        return i->second;

    QoreComplexReferenceOrNothingTypeInfo* ti = new QoreComplexReferenceOrNothingTypeInfo(vti);
    cron_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

static type_vec_t normalize_parameterized_class_args(const std::vector<const QoreTypeInfo*>& args) {
    type_vec_t rv;
    rv.reserve(args.size());
    for (const QoreTypeInfo* arg : args) {
        rv.push_back(arg == autoNoNarrowTypeInfo ? autoTypeInfo : arg);
    }
    return rv;
}

static QoreString build_parameterized_class_name(const QoreClass* qc, const type_vec_t& args, bool or_nothing,
        bool path) {
    QoreString rv;
    if (or_nothing) {
        rv.concat('*');
    }
    rv.concat("object<");
    rv.concat(path ? qc->getPath() : qc->getName());
    rv.concat('<');
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) {
            rv.concat(", ");
        }
        rv.concat(path ? QoreTypeInfo::getPath(args[i]) : QoreTypeInfo::getName(args[i]));
    }
    rv.concat(">>");
    return rv;
}

static q_accept_vec_t make_parameterized_class_accept_vec(const QoreParameterizedClassTypeInfo* ti,
        bool or_nothing) {
    q_accept_vec_t rv {{QoreParameterizedClassTypeSpec(ti), nullptr, !or_nothing}};
    if (or_nothing) {
        rv.emplace_back(QoreTypeSpec(NT_NOTHING), nullptr);
        rv.emplace_back(QoreTypeSpec(NT_NULL),
            [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); });
    }
    return rv;
}

static q_return_vec_t make_parameterized_class_return_vec(const QoreParameterizedClassTypeInfo* ti,
        bool or_nothing) {
    q_return_vec_t rv {{QoreParameterizedClassTypeSpec(ti), !or_nothing}};
    if (or_nothing) {
        rv.emplace_back(QoreTypeSpec(NT_NOTHING));
    }
    return rv;
}

QoreParameterizedClassTypeInfo::QoreParameterizedClassTypeInfo(const QoreClass* qc,
        const std::vector<const QoreTypeInfo*>& type_args, bool n_or_nothing,
        const QoreParameterizedClassTypeInfo* value_type)
        : QoreTypeInfo(
            make_parameterized_class_accept_vec(n_or_nothing ? value_type : this, n_or_nothing),
            make_parameterized_class_return_vec(n_or_nothing ? value_type : this, n_or_nothing),
            build_parameterized_class_name(qc, normalize_parameterized_class_args(type_args), n_or_nothing, false)),
        base_class(qc),
        type_args(normalize_parameterized_class_args(type_args)),
        or_nothing(n_or_nothing) {
    assert(base_class);
    assert(!this->type_args.empty());
    assert(!or_nothing || value_type);
    pname = build_parameterized_class_name(base_class, this->type_args, or_nothing, true).c_str();
}

QoreParameterizedClassTypeSpec::QoreParameterizedClassTypeSpec(const QoreParameterizedClassTypeInfo* ti)
        : QoreTypeSpec(static_cast<const QoreTypeInfo*>(ti), QTS_PARAMCLASS) {
}

static QoreString build_type_parameter_name(const char* name, bool or_nothing) {
    QoreString rv;
    if (or_nothing) {
        rv.concat('*');
    }
    rv.concat(name);
    return rv;
}

static q_accept_vec_t make_type_parameter_accept_vec(const QoreTypeParameterTypeInfo* ti, bool or_nothing) {
    q_accept_vec_t rv {{QoreTypeParameterTypeSpec(ti), nullptr, !or_nothing}};
    if (or_nothing) {
        rv.emplace_back(QoreTypeSpec(NT_NOTHING), nullptr);
        rv.emplace_back(QoreTypeSpec(NT_NULL),
            [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); });
    }
    return rv;
}

static q_return_vec_t make_type_parameter_return_vec(const QoreTypeParameterTypeInfo* ti, bool or_nothing) {
    q_return_vec_t rv {{QoreTypeParameterTypeSpec(ti), !or_nothing}};
    if (or_nothing) {
        rv.emplace_back(QoreTypeSpec(NT_NOTHING));
    }
    return rv;
}

QoreTypeParameterTypeInfo::QoreTypeParameterTypeInfo(const QoreClass* owner, size_t n_index, const char* name,
        bool n_or_nothing, const QoreTypeParameterTypeInfo* value_type)
        : QoreTypeInfo(
            make_type_parameter_accept_vec(n_or_nothing ? value_type : this, n_or_nothing),
            make_type_parameter_return_vec(n_or_nothing ? value_type : this, n_or_nothing),
            build_type_parameter_name(name, n_or_nothing)),
        owner_class(owner),
        index(n_index),
        param_name(name),
        or_nothing(n_or_nothing) {
    assert(owner_class);
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
}

QoreTypeParameterTypeInfo::QoreTypeParameterTypeInfo(const TypedHashDecl* owner, size_t n_index,
        const char* name, bool n_or_nothing, const QoreTypeParameterTypeInfo* value_type)
        : QoreTypeInfo(
            make_type_parameter_accept_vec(n_or_nothing ? value_type : this, n_or_nothing),
            make_type_parameter_return_vec(n_or_nothing ? value_type : this, n_or_nothing),
            build_type_parameter_name(name, n_or_nothing)),
        owner_hashdecl(owner),
        index(n_index),
        param_name(name),
        or_nothing(n_or_nothing) {
    assert(owner_hashdecl);
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
}

QoreTypeParameterTypeInfo::QoreTypeParameterTypeInfo(const UserSignature* owner, size_t n_index,
        const char* name, bool n_or_nothing, const QoreTypeParameterTypeInfo* value_type)
        : QoreTypeInfo(
            make_type_parameter_accept_vec(n_or_nothing ? value_type : this, n_or_nothing),
            make_type_parameter_return_vec(n_or_nothing ? value_type : this, n_or_nothing),
            build_type_parameter_name(name, n_or_nothing)),
        owner_signature(owner),
        index(n_index),
        param_name(name),
        or_nothing(n_or_nothing) {
    assert(owner_signature);
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
}

QoreTypeParameterTypeSpec::QoreTypeParameterTypeSpec(const QoreTypeParameterTypeInfo* ti)
        : QoreTypeSpec(static_cast<const QoreTypeInfo*>(ti), QTS_TYPEPARAM) {
}

static QoreString build_wildcard_type_name(QoreWildcardKind kind, const QoreTypeInfo* bound, bool path) {
    QoreString rv("?");
    if (kind == QoreWildcardKind::Extends) {
        rv.concat(" extends ");
        rv.concat(path ? QoreTypeInfo::getPath(bound) : QoreTypeInfo::getName(bound));
    } else if (kind == QoreWildcardKind::Super) {
        rv.concat(" super ");
        rv.concat(path ? QoreTypeInfo::getPath(bound) : QoreTypeInfo::getName(bound));
    }
    return rv;
}

static q_accept_vec_t make_wildcard_accept_vec(const QoreWildcardTypeInfo* ti, QoreWildcardKind kind,
        const QoreTypeInfo* bound) {
    q_accept_vec_t rv {{QoreWildcardTypeSpec(ti), nullptr, true}};
    if (kind != QoreWildcardKind::Super || !bound) {
        return rv;
    }
    rv.reserve(bound->getAcceptVecSize() + 1);
    for (const QoreAcceptSpec& spec : bound->getAcceptSpecs()) {
        rv.push_back(spec);
    }
    return rv;
}

static q_return_vec_t make_wildcard_return_vec(QoreWildcardKind kind, const QoreTypeInfo* bound) {
    if (kind == QoreWildcardKind::Extends && bound) {
        q_return_vec_t rv;
        rv.reserve(bound->return_vec.size());
        for (const QoreReturnSpec& spec : bound->return_vec) {
            rv.push_back(spec);
        }
        return rv;
    }
    return q_return_vec_t {{NT_ALL}};
}

QoreWildcardTypeInfo::QoreWildcardTypeInfo(QoreWildcardKind n_kind, const QoreTypeInfo* n_bound)
        : QoreTypeInfo(make_wildcard_accept_vec(this, n_kind, n_bound),
            make_wildcard_return_vec(n_kind, n_bound),
            build_wildcard_type_name(n_kind, n_bound, false)),
        kind(n_kind),
        bound(n_bound) {
    assert(kind == QoreWildcardKind::Unbounded || bound);
    pname = build_wildcard_type_name(kind, bound, true).c_str();
}

QoreWildcardTypeSpec::QoreWildcardTypeSpec(const QoreWildcardTypeInfo* ti)
        : QoreTypeSpec(static_cast<const QoreTypeInfo*>(ti), QTS_WILDCARD) {
}

const QoreTypeInfo* qore_get_parameterized_class_type(const QoreClass* qc,
        const std::vector<const QoreTypeInfo*>& args, bool or_nothing) {
    assert(qc);
    assert(!args.empty());

    type_vec_t normalized_args = normalize_parameterized_class_args(args);
    const QoreParameterizedClassTypeInfo* value_type = nullptr;
    if (or_nothing) {
        value_type = static_cast<const QoreParameterizedClassTypeInfo*>(
            qore_get_parameterized_class_type(qc, normalized_args, false));
    }

    AutoLocker al(ctl);

    ParameterizedClassCacheKey key {qc, normalized_args, or_nothing};
    parameterized_class_map_t::iterator i = parameterized_class_map.lower_bound(key);
    if (i != parameterized_class_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreParameterizedClassTypeInfo* ti = new QoreParameterizedClassTypeInfo(qc, normalized_args, or_nothing,
        value_type);
    parameterized_class_map.insert(i, parameterized_class_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_type_parameter_type(const QoreClass* owner, size_t index, const char* name,
        bool or_nothing) {
    assert(owner);
    assert(name && *name);

    const QoreTypeParameterTypeInfo* value_type = nullptr;
    if (or_nothing) {
        value_type = static_cast<const QoreTypeParameterTypeInfo*>(
            qore_get_type_parameter_type(owner, index, name, false));
    }

    AutoLocker al(ctl);

    TypeParameterCacheKey key {owner, TPO_CLASS, index, or_nothing};
    type_parameter_map_t::iterator i = type_parameter_map.lower_bound(key);
    if (i != type_parameter_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreTypeParameterTypeInfo* ti = new QoreTypeParameterTypeInfo(owner, index, name, or_nothing, value_type);
    type_parameter_map.insert(i, type_parameter_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_hashdecl_type_parameter_type(const TypedHashDecl* owner, size_t index,
        const char* name, bool or_nothing) {
    assert(owner);
    assert(name && *name);

    const QoreTypeParameterTypeInfo* value_type = nullptr;
    if (or_nothing) {
        value_type = static_cast<const QoreTypeParameterTypeInfo*>(
            qore_get_hashdecl_type_parameter_type(owner, index, name, false));
    }

    AutoLocker al(ctl);

    TypeParameterCacheKey key {owner, TPO_HASHDECL, index, or_nothing};
    type_parameter_map_t::iterator i = type_parameter_map.lower_bound(key);
    if (i != type_parameter_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreTypeParameterTypeInfo* ti = new QoreTypeParameterTypeInfo(owner, index, name, or_nothing, value_type);
    type_parameter_map.insert(i, type_parameter_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_signature_type_parameter_type(const UserSignature* owner, size_t index,
        const char* name, bool or_nothing) {
    assert(owner);
    assert(name && *name);

    const QoreTypeParameterTypeInfo* value_type = nullptr;
    if (or_nothing) {
        value_type = static_cast<const QoreTypeParameterTypeInfo*>(
            qore_get_signature_type_parameter_type(owner, index, name, false));
    }

    AutoLocker al(ctl);

    TypeParameterCacheKey key {owner, TPO_SIGNATURE, index, or_nothing};
    type_parameter_map_t::iterator i = type_parameter_map.lower_bound(key);
    if (i != type_parameter_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreTypeParameterTypeInfo* ti = new QoreTypeParameterTypeInfo(owner, index, name, or_nothing, value_type);
    type_parameter_map.insert(i, type_parameter_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeParameterTypeInfo* qore_get_type_parameter_type_info(const QoreTypeInfo* ti) {
    return dynamic_cast<const QoreTypeParameterTypeInfo*>(ti);
}

static const QoreTypeInfo* qore_get_wildcard_type_intern(QoreWildcardKind kind, const QoreTypeInfo* bound) {
    assert(kind == QoreWildcardKind::Unbounded || bound);

    if (kind == QoreWildcardKind::Unbounded) {
        bound = nullptr;
    } else if (bound == autoNoNarrowTypeInfo) {
        bound = autoTypeInfo;
    }

    AutoLocker al(ctl);

    WildcardTypeCacheKey key {kind, bound};
    wildcard_type_map_t::iterator i = wildcard_type_map.lower_bound(key);
    if (i != wildcard_type_map.end() && !(key < i->first)) {
        return i->second;
    }

    QoreWildcardTypeInfo* ti = new QoreWildcardTypeInfo(kind, bound);
    wildcard_type_map.insert(i, wildcard_type_map_t::value_type(key, ti));
    return ti;
}

const QoreTypeInfo* qore_get_wildcard_type() {
    return qore_get_wildcard_type_intern(QoreWildcardKind::Unbounded, nullptr);
}

const QoreTypeInfo* qore_get_wildcard_extends_type(const QoreTypeInfo* bound) {
    assert(bound);
    return qore_get_wildcard_type_intern(QoreWildcardKind::Extends, bound);
}

const QoreTypeInfo* qore_get_wildcard_super_type(const QoreTypeInfo* bound) {
    assert(bound);
    return qore_get_wildcard_type_intern(QoreWildcardKind::Super, bound);
}

bool qore_type_contains_wildcard(const QoreTypeInfo* ti) {
    if (!ti) {
        return false;
    }
    if (QoreTypeInfo::getWildcardType(ti)) {
        return true;
    }
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            if (qore_type_contains_wildcard(arg)) {
                return true;
            }
        }
        return false;
    }
    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti)) {
        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        if (hp->isParameterizedHashDecl()) {
            for (const QoreTypeInfo* arg : hp->getTypeArgs()) {
                if (qore_type_contains_wildcard(arg)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool qore_type_contains_type_parameter(const QoreTypeInfo* ti) {
    if (!ti || !QoreTypeInfo::hasType(ti)) {
        return false;
    }

    if (qore_get_type_parameter_type_info(ti)) {
        return true;
    }

    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            if (qore_type_contains_type_parameter(arg)) {
                return true;
            }
        }
        return false;
    }

    if (const QoreWildcardTypeInfo* wti = QoreTypeInfo::getWildcardType(ti)) {
        return qore_type_contains_type_parameter(wti->getBound());
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti)) {
        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        if (hp->isParameterizedHashDecl()) {
            for (const QoreTypeInfo* arg : hp->getTypeArgs()) {
                if (qore_type_contains_type_parameter(arg)) {
                    return true;
                }
            }
        }
        return false;
    }

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        if (qore_type_contains_type_parameter(cti->getReturnType())) {
            return true;
        }
        for (const QoreTypeInfo* param : cti->getParamTypes()) {
            if (qore_type_contains_type_parameter(param)) {
                return true;
            }
        }
        return false;
    }

    if (QoreTypeInfo::getUnionType(ti)) {
        for (const QoreReturnSpec& rt : ti->return_vec) {
            const QoreTypeInfo* member = rt.spec.getTypeInfo();
            if (member && member != nothingTypeInfo && qore_type_contains_type_parameter(member)) {
                return true;
            }
        }
        return false;
    }

    const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexHash(ti);
    if (subtype && qore_type_contains_type_parameter(subtype)) {
        return true;
    }

    subtype = QoreTypeInfo::getUniqueReturnComplexList(ti);
    if (subtype && qore_type_contains_type_parameter(subtype)) {
        return true;
    }

    subtype = QoreTypeInfo::getUniqueReturnComplexReference(ti);
    return subtype && qore_type_contains_type_parameter(subtype);
}

const QoreTypeInfo* qore_substitute_type_params_if_needed(const QoreTypeInfo* ti) {
    return qore_type_contains_type_parameter(ti)
        ? qore_substitute_type_params(ti, qore_get_current_receiver_type_info())
        : ti;
}

const QoreTypeInfo* qore_substitute_type_params_if_needed(const QoreTypeInfo* ti,
        const QoreTypeInfo* receiver_type_info, const QoreTypeParamInstantiation* type_param_inst) {
    return qore_type_contains_type_parameter(ti)
        ? qore_substitute_type_params(ti, receiver_type_info, type_param_inst)
        : ti;
}

static qore_type_result_e qore_wildcard_accepts(const QoreWildcardTypeInfo* target_wc,
        const QoreTypeInfo* source_arg, bool& may_not_match, bool& may_need_filter) {
    assert(target_wc);
    if (target_wc->getKind() == QoreWildcardKind::Unbounded) {
        may_not_match = true;
        return QTI_AMBIGUOUS;
    }

    const QoreTypeInfo* bound = target_wc->getBound();
    assert(bound);
    if (target_wc->getKind() == QoreWildcardKind::Extends) {
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        qore_type_result_e rv = QoreTypeInfo::parseAccepts(bound, source_arg, may_not_match, may_need_filter,
            max_result);
        return rv == QTI_NOT_EQUAL ? rv : QTI_AMBIGUOUS;
    }

    qore_type_result_e max_result = QTI_NOT_EQUAL;
    qore_type_result_e rv = QoreTypeInfo::parseAccepts(source_arg, bound, may_not_match, may_need_filter,
        max_result);
    return rv == QTI_NOT_EQUAL ? rv : QTI_AMBIGUOUS;
}

qore_type_result_e qore_generic_type_arg_accepts(const QoreTypeInfo* target_arg,
        const QoreTypeInfo* source_arg, bool& may_not_match, bool& may_need_filter) {
    if (QoreTypeInfo::equal(target_arg, source_arg)) {
        return QTI_IDENT;
    }

    const QoreWildcardTypeInfo* target_wc = QoreTypeInfo::getWildcardType(target_arg);
    if (!target_wc) {
        return QTI_NOT_EQUAL;
    }

    const QoreWildcardTypeInfo* source_wc = QoreTypeInfo::getWildcardType(source_arg);
    if (!source_wc) {
        return qore_wildcard_accepts(target_wc, source_arg, may_not_match, may_need_filter);
    }

    if (target_wc->getKind() == QoreWildcardKind::Unbounded) {
        may_not_match = true;
        return QTI_AMBIGUOUS;
    }

    if (target_wc->getKind() == source_wc->getKind() && source_wc->getBound()) {
        return qore_wildcard_accepts(target_wc, source_wc->getBound(), may_not_match, may_need_filter);
    }

    return QTI_NOT_EQUAL;
}

qore_type_result_e qore_parameterized_class_accepts(const QoreParameterizedClassTypeInfo* target_pti,
        const QoreTypeInfo* source_type, bool& may_not_match, bool& may_need_filter) {
    assert(target_pti);
    const QoreParameterizedClassTypeInfo* source_pti = QoreTypeInfo::getParameterizedClassType(source_type);
    if (!source_pti) {
        return QTI_NOT_EQUAL;
    }

    const QoreTypeInfo* mapped_type = qore_class_private::get(*source_pti->getBaseClass())
        ->getParameterizedBaseTypeInfo(source_pti, target_pti->getBaseClass());
    const QoreParameterizedClassTypeInfo* mapped_pti = QoreTypeInfo::getParameterizedClassType(mapped_type);
    if (!mapped_pti || mapped_pti->getArgCount() != target_pti->getArgCount()) {
        return QTI_NOT_EQUAL;
    }

    qore_type_result_e rv = QTI_IDENT;
    const type_vec_t& target_args = target_pti->getTypeArgs();
    const type_vec_t& source_args = mapped_pti->getTypeArgs();
    for (size_t i = 0, e = target_args.size(); i < e; ++i) {
        qore_type_result_e arg_rv = qore_generic_type_arg_accepts(target_args[i], source_args[i], may_not_match,
            may_need_filter);
        if (arg_rv == QTI_NOT_EQUAL) {
            return QTI_NOT_EQUAL;
        }
        if (arg_rv < rv) {
            rv = arg_rv;
        }
    }
    return rv;
}

static const TypedHashDecl* qore_get_hashdecl_base(const TypedHashDecl* hd) {
    const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
    return hp->isParameterizedHashDecl() ? hp->getParameterizedBase() : hd;
}

static const TypedHashDecl* qore_get_hashdecl_mapped_to_base(const TypedHashDecl* source_hd,
        const TypedHashDecl* target_base) {
    const TypedHashDecl* current = source_hd;
    while (current) {
        const TypedHashDecl* current_base = qore_get_hashdecl_base(current);
        if (typed_hash_decl_private::get(*current_base)->equal(*typed_hash_decl_private::get(*target_base))) {
            return current;
        }
        current = typed_hash_decl_private::get(*current)->getParentHashDecl();
    }
    return nullptr;
}

qore_type_result_e qore_parameterized_hashdecl_accepts(const TypedHashDecl* target_hd,
        const TypedHashDecl* source_hd, bool& may_not_match, bool& may_need_filter) {
    assert(target_hd);
    assert(source_hd);

    const typed_hash_decl_private* target_hp = typed_hash_decl_private::get(*target_hd);
    const typed_hash_decl_private* source_hp = typed_hash_decl_private::get(*source_hd);
    if (source_hp->equal(*target_hp)) {
        return QTI_IDENT;
    }

    const TypedHashDecl* target_base = qore_get_hashdecl_base(target_hd);
    const TypedHashDecl* mapped_source = qore_get_hashdecl_mapped_to_base(source_hd, target_base);
    if (!mapped_source) {
        return QTI_NOT_EQUAL;
    }

    const typed_hash_decl_private* mapped_hp = typed_hash_decl_private::get(*mapped_source);
    if (!target_hp->isParameterizedHashDecl() || !mapped_hp->isParameterizedHashDecl()) {
        return source_hp->isDescendantOf(*target_hp) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
    }

    const type_vec_t& target_args = target_hp->getTypeArgs();
    const type_vec_t& source_args = mapped_hp->getTypeArgs();
    if (target_args.size() != source_args.size()) {
        return QTI_NOT_EQUAL;
    }

    qore_type_result_e rv = source_hd == mapped_source ? QTI_IDENT : QTI_AMBIGUOUS;
    for (size_t i = 0, e = target_args.size(); i < e; ++i) {
        qore_type_result_e arg_rv = qore_generic_type_arg_accepts(target_args[i], source_args[i], may_not_match,
            may_need_filter);
        if (arg_rv == QTI_NOT_EQUAL) {
            return QTI_NOT_EQUAL;
        }
        if (arg_rv < rv) {
            rv = arg_rv;
        }
    }
    return rv == QTI_IDENT && source_hd != mapped_source ? QTI_AMBIGUOUS : rv;
}

// Helper function to create a normalized key for union type caching
// Preserve declaration order so union resolution honors the first matching type.
static type_vec_t normalize_union_key(const type_vec_t& member_types) {
    type_vec_t key;
    key.reserve(member_types.size());
    std::unordered_set<const QoreTypeInfo*> seen;
    for (const QoreTypeInfo* ti : member_types) {
        if (seen.insert(ti).second) {
            key.push_back(ti);
        }
    }
    return key;
}

// Helper function to build the union type name
static QoreString build_union_name(const type_vec_t& member_types, bool or_nothing) {
    QoreString name;
    if (or_nothing) {
        name.concat('*');
    }
    name.concat("union<");
    for (size_t i = 0; i < member_types.size(); ++i) {
        if (i > 0) {
            name.concat(", ");
        }
        name.concat(QoreTypeInfo::getName(member_types[i]));
    }
    name.concat('>');
    return name;
}

const QoreTypeInfo* qore_get_union_type(const type_vec_t& member_types, bool or_nothing) {
    // Handle edge cases
    if (member_types.empty()) {
        return autoTypeInfo;  // empty union accepts anything
    }
    if (member_types.size() == 1) {
        const QoreTypeInfo* ti = member_types[0];
        if (or_nothing) {
            const QoreTypeInfo* orn = qore_get_or_nothing_type(ti);
            return orn ? orn : ti;
        }
        return ti;
    }

    // Normalize the key for consistent cache lookup
    type_vec_t key = normalize_union_key(member_types);

    // Check if any member already accepts NOTHING, if so, or_nothing is already covered
    bool has_nothing = false;
    for (const QoreTypeInfo* ti : key) {
        if (QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)) {
            has_nothing = true;
            break;
        }
    }

    // Use appropriate cache
    union_map_t& cache = (or_nothing && !has_nothing) ? union_on_map : union_map;

    AutoLocker al(ctl);

    union_map_t::iterator i = cache.find(key);
    if (i != cache.end()) {
        return i->second;
    }

    // Build accept and return vectors from all member types
    q_accept_vec_t accept_vec;
    q_return_vec_t return_vec;

    for (const QoreTypeInfo* ti : key) {
        // Add accept types from this member
        for (const auto& a : ti->getAcceptSpecs()) {
            accept_vec.push_back(a);
        }
        // Add return types from this member
        for (const auto& r : ti->return_vec) {
            return_vec.push_back(r);
        }
    }

    // If or_nothing and no member accepts NOTHING, add it
    if (or_nothing && !has_nothing) {
        accept_vec.push_back({NT_NOTHING, nullptr});
        accept_vec.push_back({NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }});
        return_vec.push_back({NT_NOTHING});
    }

    // Build the name
    QoreString name = build_union_name(member_types, or_nothing && !has_nothing);

    // Create the union type
    QoreUnionTypeInfo* ti = new QoreUnionTypeInfo(std::move(accept_vec), std::move(return_vec), name,
        or_nothing || has_nothing);
    cache[key] = ti;
    return ti;
}

const QoreTypeInfo* qore_get_union_or_nothing_type(const type_vec_t& member_types) {
    return qore_get_union_type(member_types, true);
}

// Helper function to parse a type string into a QoreParseTypeInfo with proper subtypes
QoreParseTypeInfo* qore_parse_type_string_to_pti(const char* type_str) {
    // Trim whitespace
    while (*type_str && isspace(*type_str)) ++type_str;
    if (!*type_str) return nullptr;

    std::string str(type_str);
    while (!str.empty() && isspace(str.back())) str.pop_back();
    if (str.empty()) return nullptr;

    if (str == "?") {
        return new QoreParseTypeInfo(QoreWildcardKind::Unbounded);
    }

    const char* extends_kw = "? extends ";
    const char* super_kw = "? super ";
    if (!strncmp(str.c_str(), extends_kw, strlen(extends_kw))
            || !strncmp(str.c_str(), super_kw, strlen(super_kw))) {
        QoreWildcardKind kind = str[2] == 'e' ? QoreWildcardKind::Extends : QoreWildcardKind::Super;
        const char* bound_str = str.c_str() + (kind == QoreWildcardKind::Extends
            ? strlen(extends_kw) : strlen(super_kw));
        while (*bound_str && isspace(*bound_str)) {
            ++bound_str;
        }
        QoreParseTypeInfo* bound = qore_parse_type_string_to_pti(bound_str);
        return new QoreParseTypeInfo(kind, bound ? bound : new QoreParseTypeInfo(strdup("auto")));
    }

    // Check for or-nothing prefix
    bool or_nothing = false;
    if (str[0] == '*') {
        or_nothing = true;
        str.erase(0, 1);
    }

    // Check if this is a complex type (has angle brackets)
    size_t angle_pos = str.find('<');
    if (angle_pos == std::string::npos) {
        // Simple type
        return new QoreParseTypeInfo(strdup(str.c_str()), or_nothing);
    }

    // Check if this is a callable signature like "ReturnType(Params...)" where Params
    // may themselves contain '<' (e.g. "nothing(hash<auto>)"). If '(' occurs before the
    // first '<', the whole string is a callable signature, not a complex type — keep it
    // as a single opaque name so the outer code<> handler can re-parse it.
    size_t paren_pos = str.find('(');
    if (paren_pos != std::string::npos && paren_pos < angle_pos) {
        return new QoreParseTypeInfo(strdup(str.c_str()), or_nothing);
    }

    // Complex type - find matching '>'
    int depth = 1;
    size_t close_pos = angle_pos + 1;
    while (close_pos < str.size() && depth > 0) {
        if (str[close_pos] == '<') ++depth;
        else if (str[close_pos] == '>') --depth;
        ++close_pos;
    }
    if (depth != 0) {
        // Unbalanced brackets - return as-is
        return new QoreParseTypeInfo(strdup(str.c_str()), or_nothing);
    }
    --close_pos;  // Point to the closing '>'

    // Extract base type and subtype content
    std::string base_type = str.substr(0, angle_pos);
    std::string subtype_content = str.substr(angle_pos + 1, close_pos - angle_pos - 1);

    // Parse subtypes (split on commas, respecting nested brackets)
    parse_type_vec_t subtypes;
    std::string current;
    int angle_depth = 0;
    for (size_t i = 0; i <= subtype_content.size(); ++i) {
        char c = i < subtype_content.size() ? subtype_content[i] : '\0';
        if (c == '<') {
            ++angle_depth;
            current += c;
        } else if (c == '>') {
            --angle_depth;
            current += c;
        } else if ((c == ',' || c == '\0') && angle_depth == 0) {
            // Trim whitespace
            while (!current.empty() && isspace(current.back())) current.pop_back();
            while (!current.empty() && isspace(current.front())) current.erase(0, 1);
            if (!current.empty()) {
                // Recursively parse this subtype
                QoreParseTypeInfo* sub_pti = qore_parse_type_string_to_pti(current.c_str());
                if (sub_pti) {
                    subtypes.push_back(sub_pti);
                }
            }
            current.clear();
        } else {
            current += c;
        }
    }

    return new QoreParseTypeInfo(strdup(base_type.c_str()), or_nothing, std::move(subtypes));
}

// Helper function to parse a type string and resolve it (for use in code<> signature parsing)
// This handles complex types like "hash<string, int>" and "list<string>"
static const QoreTypeInfo* parse_and_resolve_type_string(const char* type_str, const QoreProgramLocation* loc, int& err) {
    // Trim whitespace
    while (*type_str && isspace(*type_str)) ++type_str;
    if (!*type_str) return nullptr;

    std::string str(type_str);
    while (!str.empty() && isspace(str.back())) str.pop_back();
    if (str.empty()) return nullptr;

    // Check for or-nothing prefix
    bool or_nothing = false;
    const char* check_str = str.c_str();
    if (str[0] == '*') {
        or_nothing = true;
        check_str++;
    }

    // Check for "nothing" type
    if (!strcmp(check_str, "nothing")) {
        return nothingTypeInfo;
    }

    // Check for "auto" type
    if (!strcmp(check_str, "auto")) {
        return autoTypeInfo;
    }

    // Check if this is a simple type (no angle brackets)
    if (!strchr(str.c_str(), '<')) {
        // Simple type - try to resolve as builtin first
        const QoreTypeInfo* rv = or_nothing
            ? getBuiltinUserOrNothingTypeInfo(check_str)
            : getBuiltinUserTypeInfo(check_str);
        if (rv) return rv;
    }

    // Parse the type string into a QoreParseTypeInfo with proper structure
    QoreParseTypeInfo* pti = qore_parse_type_string_to_pti(type_str);
    if (!pti) return nullptr;

    return QoreParseTypeInfo::resolveAndDelete(pti, loc, err);
}

// Helper function to build the typed callable type name
static QoreString build_complex_code_name(const QoreTypeInfo* returnType, const type_vec_t& paramTypes,
        bool varargs, bool or_nothing) {
    QoreString name;
    if (or_nothing) {
        name.concat('*');
    }
    name.concat("code<");
    // Return type
    if (returnType) {
        name.concat(QoreTypeInfo::getName(returnType));
    } else {
        name.concat("nothing");
    }
    name.concat('(');
    // Parameter types
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) {
            name.concat(", ");
        }
        name.concat(QoreTypeInfo::getName(paramTypes[i]));
    }
    if (varargs) {
        if (!paramTypes.empty()) {
            name.concat(", ");
        }
        name.concat("...");
    }
    name.concat(")>");
    return name;
}

// Helper function to build the path name for typed callable
static QoreString build_complex_code_path(const QoreTypeInfo* returnType, const type_vec_t& paramTypes,
        bool varargs, bool or_nothing) {
    QoreString name;
    if (or_nothing) {
        name.concat('*');
    }
    name.concat("code<");
    // Return type
    if (returnType) {
        name.concat(QoreTypeInfo::getPath(returnType));
    } else {
        name.concat("nothing");
    }
    name.concat('(');
    // Parameter types
    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (i > 0) {
            name.concat(", ");
        }
        name.concat(QoreTypeInfo::getPath(paramTypes[i]));
    }
    if (varargs) {
        if (!paramTypes.empty()) {
            name.concat(", ");
        }
        name.concat("...");
    }
    name.concat(")>");
    return name;
}

// Helper to build accept_vec for typed callable
static q_accept_vec_t build_complex_code_accept_vec(bool or_nothing) {
    q_accept_vec_t avec;
    avec.push_back({NT_RUNTIME_CLOSURE, nullptr});
    avec.push_back({NT_FUNCREF, nullptr});
    if (or_nothing) {
        avec.push_back({NT_NOTHING, nullptr});
        avec.push_back({NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }});
    }
    return avec;
}

// Helper to build return_vec for typed callable
static q_return_vec_t build_complex_code_return_vec(bool or_nothing) {
    q_return_vec_t rvec;
    rvec.push_back({NT_RUNTIME_CLOSURE});
    rvec.push_back({NT_FUNCREF});
    if (or_nothing) {
        rvec.push_back({NT_NOTHING});
    }
    return rvec;
}

QoreComplexCodeTypeInfo::QoreComplexCodeTypeInfo(const QoreTypeInfo* ret_type, type_vec_t&& param_types,
        bool varargs_arg, bool or_nothing_arg)
        : QoreTypeInfo(build_complex_code_accept_vec(or_nothing_arg),
            build_complex_code_return_vec(or_nothing_arg),
            build_complex_code_name(ret_type, param_types, varargs_arg, or_nothing_arg)),
          returnType(ret_type), paramTypes(std::move(param_types)),
          varargs(varargs_arg), orNothing(or_nothing_arg) {
    // Set path name
    pname = build_complex_code_path(ret_type, paramTypes, varargs, orNothing);
}

bool QoreComplexCodeTypeInfo::isSignatureCompatible(const AbstractFunctionSignature* sig) const {
    if (!sig) {
        return false;
    }

    // Check return type compatibility (covariant)
    // The callable's return type must be assignable to our return type
    const QoreTypeInfo* sigReturnType = sig->getReturnTypeInfo();
    if (returnType) {
        // We expect a specific return type - check if the callable's return type is compatible
        // Covariant: the callable can return a more specific type
        if (!QoreTypeInfo::parseAccepts(returnType, sigReturnType)) {
            return false;
        }
    }
    // If returnType is nullptr (nothing), any return type is accepted

    // If we accept varargs, any parameter count is OK
    if (varargs) {
        return true;
    }

    // Check parameter count
    unsigned sigNumParams = sig->numParams();
    size_t ourParamCount = paramTypes.size();

    // Compute minimum required params for the signature (params without defaults)
    unsigned sigMinParams = 0;
    for (unsigned i = 0; i < sigNumParams; ++i) {
        if (!sig->hasDefaultArg(i)) {
            sigMinParams = i + 1;
        }
    }

    // The callable must be able to accept our parameter count
    // - We need at least sigMinParams
    // - We can have at most sigNumParams unless the callable has varargs
    if (ourParamCount < sigMinParams) {
        return false;
    }
    if (ourParamCount > sigNumParams && !sig->hasVarargs()) {
        return false;
    }

    // Check each parameter type (contravariant)
    // Our param types must be assignable to the callable's param types
    for (size_t i = 0; i < ourParamCount && i < sigNumParams; ++i) {
        const QoreTypeInfo* sigParamType = sig->getParamTypeInfo(i);
        const QoreTypeInfo* ourParamType = paramTypes[i];

        if (ourParamType) {
            // We specify a param type - check contravariance
            // The callable's param type must accept our param type
            if (!QoreTypeInfo::parseAccepts(sigParamType, ourParamType)) {
                return false;
            }
        }
        // If ourParamType is nullptr, any param type is accepted
    }

    return true;
}

const QoreTypeInfo* qore_get_complex_code_type(const QoreTypeInfo* return_type,
        const type_vec_t& param_types, bool varargs, bool or_nothing) {
    // Create cache key
    ComplexCodeCacheKey key{return_type, param_types, varargs, or_nothing};

    AutoLocker al(ctl);

    complex_code_map_t::iterator i = complex_code_map.find(key);
    if (i != complex_code_map.end()) {
        return i->second;
    }

    // Create copy of param_types for the constructor
    type_vec_t params_copy(param_types);

    // Create the typed callable type
    QoreComplexCodeTypeInfo* ti = new QoreComplexCodeTypeInfo(return_type, std::move(params_copy),
        varargs, or_nothing);
    complex_code_map[key] = ti;
    return ti;
}

const QoreTypeInfo* qore_get_complex_code_or_nothing_type(const QoreTypeInfo* return_type,
        const type_vec_t& param_types, bool varargs) {
    return qore_get_complex_code_type(return_type, param_types, varargs, true);
}

static const QoreTypeInfo* get_substituted_type_param_arg(const QoreTypeParameterTypeInfo* tpi,
        const QoreTypeInfo* receiver_type_info, const QoreTypeParamInstantiation* type_param_inst) {
    if (const UserSignature* owner_signature = tpi->getOwnerSignature()) {
        if (!type_param_inst || type_param_inst->owner != owner_signature
                || tpi->getIndex() >= type_param_inst->type_args.size()) {
            return nullptr;
        }

        const QoreTypeInfo* arg = type_param_inst->type_args[tpi->getIndex()];
        return tpi->isOrNothing() ? get_or_nothing_type_check(arg) : arg;
    }

    if (const TypedHashDecl* owner_hashdecl = tpi->getOwnerHashDecl()) {
        const TypedHashDecl* receiver_hashdecl = QoreTypeInfo::getTypedHash(receiver_type_info);
        if (!receiver_hashdecl) {
            return nullptr;
        }

        const typed_hash_decl_private* receiver_hp = typed_hash_decl_private::get(*receiver_hashdecl);
        const TypedHashDecl* base_hashdecl = receiver_hp->getParameterizedBase();
        if (!base_hashdecl || !base_hashdecl->equal(owner_hashdecl)) {
            return nullptr;
        }

        const std::vector<const QoreTypeInfo*>& args = receiver_hp->getTypeArgs();
        if (tpi->getIndex() >= args.size()) {
            return nullptr;
        }

        const QoreTypeInfo* arg = args[tpi->getIndex()];
        return tpi->isOrNothing() ? get_or_nothing_type_check(arg) : arg;
    }

    const QoreParameterizedClassTypeInfo* receiver_pti = QoreTypeInfo::getParameterizedClassType(receiver_type_info);
    if (!receiver_pti) {
        const QoreClass* receiver_class = QoreTypeInfo::getUniqueReturnClass(receiver_type_info);
        if (receiver_class) {
            const QoreTypeInfo* owner_type = qore_class_private::get(*receiver_class)
                ->getConcreteParameterizedBaseTypeInfo(tpi->getOwnerClass());
            const QoreParameterizedClassTypeInfo* owner_pti = QoreTypeInfo::getParameterizedClassType(owner_type);
            if (owner_pti && tpi->getIndex() < owner_pti->getArgCount()) {
                const QoreTypeInfo* arg = owner_pti->getTypeArgs()[tpi->getIndex()];
                return tpi->isOrNothing() ? get_or_nothing_type_check(arg) : arg;
            }
        }
        const qore_class_private* owner = qore_class_private::get(*tpi->getOwnerClass());
        if (receiver_class && owner->rawConstructionDefaultsToAuto()
                && qore_class_private::runtimeCheckCompatibleClass(*tpi->getOwnerClass(), *receiver_class)
                    != QTI_NOT_EQUAL) {
            return tpi->isOrNothing() ? get_or_nothing_type_check(autoTypeInfo) : autoTypeInfo;
        }
        return nullptr;
    }

    const QoreTypeInfo* owner_type = qore_class_private::get(*receiver_pti->getBaseClass())
        ->getParameterizedBaseTypeInfo(receiver_pti, tpi->getOwnerClass());
    const QoreParameterizedClassTypeInfo* owner_pti = QoreTypeInfo::getParameterizedClassType(owner_type);
    if (!owner_pti || tpi->getIndex() >= owner_pti->getArgCount()) {
        return nullptr;
    }

    const QoreTypeInfo* arg = owner_pti->getTypeArgs()[tpi->getIndex()];
    return tpi->isOrNothing() ? get_or_nothing_type_check(arg) : arg;
}

const QoreTypeInfo* qore_get_object_receiver_type_info(const QoreObject* self) {
    if (!self) {
        return nullptr;
    }
    const QoreTypeInfo* ti = self->getInstantiatedTypeInfo();
    return ti ? ti : self->getClass()->getTypeInfo();
}

const QoreTypeInfo* qore_get_current_receiver_type_info() {
    const QoreTypeInfo* ti = runtime_get_receiver_type_info();
    return ti ? ti : qore_get_object_receiver_type_info(runtime_get_stack_object());
}

const QoreTypeInfo* qore_substitute_type_params(const QoreTypeInfo* ti, const QoreTypeInfo* receiver_type_info) {
    return qore_substitute_type_params(ti, receiver_type_info, runtime_get_type_param_instantiation());
}

const QoreTypeInfo* qore_substitute_type_params(const QoreTypeInfo* ti, const QoreTypeInfo* receiver_type_info,
        const QoreTypeParamInstantiation* type_param_inst) {
    if (!ti || (!receiver_type_info && (!type_param_inst || type_param_inst->empty()))) {
        return ti;
    }

    if (const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti)) {
        const QoreTypeInfo* subst = get_substituted_type_param_arg(tpi, receiver_type_info, type_param_inst);
        return subst ? subst : ti;
    }

    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        type_vec_t args;
        args.reserve(pti->getArgCount());
        bool changed = false;
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            const QoreTypeInfo* subst = qore_substitute_type_params(arg, receiver_type_info, type_param_inst);
            args.push_back(subst);
            if (subst != arg) {
                changed = true;
            }
        }
        return changed ? qore_get_parameterized_class_type(pti->getBaseClass(), args, pti->isOrNothing()) : ti;
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti)) {
        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        if (hp->isParameterizedHashDecl()) {
            type_vec_t args;
            args.reserve(hp->getTypeArgs().size());
            bool changed = false;
            for (const QoreTypeInfo* arg : hp->getTypeArgs()) {
                const QoreTypeInfo* subst = qore_substitute_type_params(arg, receiver_type_info, type_param_inst);
                args.push_back(subst);
                if (subst != arg) {
                    changed = true;
                }
            }
            if (changed) {
                const typed_hash_decl_private* base = typed_hash_decl_private::get(*hp->getParameterizedBase());
                return base->getParameterizedTypeInfo(args, QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING));
            }
        }
    }

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        const QoreTypeInfo* ret = qore_substitute_type_params(cti->getReturnType(), receiver_type_info,
            type_param_inst);
        type_vec_t params;
        params.reserve(cti->getParamTypes().size());
        bool changed = ret != cti->getReturnType();
        for (const QoreTypeInfo* param : cti->getParamTypes()) {
            const QoreTypeInfo* subst = qore_substitute_type_params(param, receiver_type_info, type_param_inst);
            params.push_back(subst);
            if (subst != param) {
                changed = true;
            }
        }
        return changed ? qore_get_complex_code_type(ret, params, cti->hasVarArgs(), cti->isOrNothing()) : ti;
    }

    if (const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(ti)) {
        type_vec_t members;
        bool changed = false;
        for (const QoreReturnSpec& rt : ti->return_vec) {
            const QoreTypeInfo* member = rt.spec.getTypeInfo();
            if (member == nothingTypeInfo) {
                continue;
            }
            const QoreTypeInfo* subst = qore_substitute_type_params(member, receiver_type_info, type_param_inst);
            members.push_back(subst);
            if (subst != member) {
                changed = true;
            }
        }
        return changed ? qore_get_union_type(members, uti->isOrNothing()) : ti;
    }

    const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexHash(ti);
    if (subtype) {
        const QoreTypeInfo* subst = qore_substitute_type_params(subtype, receiver_type_info, type_param_inst);
        if (subst != subtype) {
            return QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)
                ? qore_get_complex_hash_or_nothing_type(subst)
                : qore_get_complex_hash_type(subst);
        }
    }

    subtype = QoreTypeInfo::getUniqueReturnComplexList(ti);
    if (subtype) {
        const QoreTypeInfo* subst = qore_substitute_type_params(subtype, receiver_type_info, type_param_inst);
        if (subst != subtype) {
            bool is_soft = !strncmp(QoreTypeInfo::getName(ti), "softlist<", 9)
                || !strncmp(QoreTypeInfo::getName(ti), "*softlist<", 10);
            if (QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)) {
                return is_soft ? qore_get_complex_softlist_or_nothing_type(subst)
                    : qore_get_complex_list_or_nothing_type(subst);
            }
            return is_soft ? qore_get_complex_softlist_type(subst) : qore_get_complex_list_type(subst);
        }
    }

    subtype = QoreTypeInfo::getUniqueReturnComplexReference(ti);
    if (subtype) {
        const QoreTypeInfo* subst = qore_substitute_type_params(subtype, receiver_type_info, type_param_inst);
        if (subst != subtype) {
            return QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)
                ? qore_get_complex_reference_or_nothing_type(subst)
                : qore_get_complex_reference_type(subst);
        }
    }

    return ti;
}

const QoreTypeInfo* qore_get_complex_code_type_from_signature(const AbstractFunctionSignature* sig,
        bool or_nothing) {
    if (!sig) {
        return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
    }

    // Extract return type
    const QoreTypeInfo* return_type = sig->getReturnTypeInfo();

    // Extract parameter types
    type_vec_t param_types;
    unsigned num_params = sig->numParams();
    for (unsigned i = 0; i < num_params; ++i) {
        param_types.push_back(sig->getParamTypeInfo(i));
    }

    // Check for varargs
    bool varargs = sig->hasVarargs();

    return qore_get_complex_code_type(return_type, param_types, varargs, or_nothing);
}

const QoreComplexCodeTypeInfo* QoreTypeInfo::getComplexCodeType(const QoreTypeInfo* ti) {
    if (!ti || !hasType(ti)) {
        return nullptr;
    }
    // Check if the type is a QoreComplexCodeTypeInfo by attempting a dynamic_cast
    return dynamic_cast<const QoreComplexCodeTypeInfo*>(ti);
}

const QoreUnionTypeInfo* QoreTypeInfo::getUnionType(const QoreTypeInfo* ti) {
    if (!ti || !hasType(ti)) {
        return nullptr;
    }
    // Check if the type is a QoreUnionTypeInfo by attempting a dynamic_cast
    return dynamic_cast<const QoreUnionTypeInfo*>(ti);
}

bool QoreTypeInfo::checkComplexCodeCompatibility(const QoreTypeInfo* target, const QoreTypeInfo* source) {
    // Get complex code types
    const QoreComplexCodeTypeInfo* target_code = getComplexCodeType(target);
    const QoreComplexCodeTypeInfo* source_code = getComplexCodeType(source);

    // If target is not a typed callable, no additional checking needed
    if (!target_code) {
        return true;
    }

    // If source is not a typed callable but target is, it's compatible
    // (the basic parseAccepts already checked that source is a code type)
    if (!source_code) {
        // Source is generic code type - compatible at parse time, runtime check needed
        return true;
    }

    // Both are typed callables - compare signatures
    // Check return type compatibility (covariant)
    // Source's return type must be assignable to target's return type
    const QoreTypeInfo* target_ret = target_code->getReturnType();
    const QoreTypeInfo* source_ret = source_code->getReturnType();

    if (target_ret) {
        // Target expects a specific return type
        if (!parseAccepts(target_ret, source_ret)) {
            return false;
        }
    }

    // If target accepts varargs, skip param count checking
    if (target_code->hasVarArgs()) {
        return true;
    }

    // Check parameter count
    const type_vec_t& target_params = target_code->getParamTypes();
    const type_vec_t& source_params = source_code->getParamTypes();

    // Source must accept at least as many params as target specifies
    // (contravariance means source can have fewer required params)
    if (source_params.size() < target_params.size() && !source_code->hasVarArgs()) {
        // Source doesn't have varargs and has fewer params than target
        return false;
    }

    // Check each parameter type (contravariant)
    // Source's param types must accept target's param types
    size_t check_count = std::min(target_params.size(), source_params.size());
    for (size_t i = 0; i < check_count; ++i) {
        const QoreTypeInfo* target_param = target_params[i];
        const QoreTypeInfo* source_param = source_params[i];

        if (target_param) {
            // Target specifies a param type - check contravariance
            // Source's param type must accept target's param type
            if (!parseAccepts(source_param, target_param)) {
                return false;
            }
        }
    }

    return true;
}

static const QoreTypeInfo* getExternalTypeInfoForType(qore_type_t t) {
    QoreAutoRWReadLocker al(extern_type_info_map_lock);
    type_typeinfo_map_t::iterator i = extern_type_info_map.find(t);
    return (i == extern_type_info_map.end() ? nullptr : i->second);
}

const QoreTypeInfo* getTypeInfoForType(qore_type_t t) {
    type_typeinfo_map_t::iterator i = type_typeinfo_map.find(t);
    return i != type_typeinfo_map.end() ? i->second : getExternalTypeInfoForType(t);
}

const QoreTypeInfo* getTypeInfoForValue(const AbstractQoreNode* n) {
    qore_type_t t = get_node_type(n);
    switch (t) {
        case NT_OBJECT: {
            const QoreObject* obj = static_cast<const QoreObject*>(n);
            return obj->getInstantiatedTypeInfo() ? obj->getInstantiatedTypeInfo() : obj->getClass()->getTypeInfo();
        }
        case NT_WEAKREF: {
            const QoreObject* obj = static_cast<const WeakReferenceNode*>(n)->get();
            return obj->getInstantiatedTypeInfo() ? obj->getInstantiatedTypeInfo() : obj->getClass()->getTypeInfo();
        }
        case NT_HASH:
            return static_cast<const QoreHashNode*>(n)->getTypeInfo();
        case NT_WEAKREF_HASH:
            return static_cast<const WeakHashReferenceNode*>(n)->get()->getTypeInfo();
        case NT_LIST:
            return static_cast<const QoreListNode*>(n)->getTypeInfo();
        case NT_WEAKREF_LIST:
            return static_cast<const WeakListReferenceNode*>(n)->get()->getTypeInfo();
        case NT_BUFFER:
            return static_cast<const QoreBufferNode*>(n)->getTypeInfo();
        case NT_PLUGIN_VALUE:
            return qore_plugin_get_value_type_info(n);
        case NT_REFERENCE:
            return static_cast<const ReferenceNode*>(n)->getTypeInfo();
        case NT_RUNTIME_CLOSURE: {
            const QoreClosureBase* cb = dynamic_cast<const QoreClosureBase*>(n);
            if (cb) {
                const QoreTypeInfo* ti = cb->getCallTypeInfo();
                if (ti) {
                    return ti;
                }
            }
            break;
        }
        case NT_FUNCREF: {
            // Call references have getFunction() that returns the underlying function;
            // if it has a unique signature, return the typed code type
            const ResolvedCallReferenceNode* cr =
                dynamic_cast<const ResolvedCallReferenceNode*>(n);
            if (cr) {
                QoreFunction* f = cr->getFunction();
                if (f) {
                    AbstractFunctionSignature* sig = f->getUniqueSignature();
                    if (sig) {
                        return qore_get_complex_code_type_from_signature(sig);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return getTypeInfoForType(t);
}

const QoreTypeInfo* getBuiltinUserTypeInfo(const char* str) {
    str_typeinfo_map_t::iterator i = str_typeinfo_map.find(str);
    if (i == str_typeinfo_map.end())
        return nullptr;

    const QoreTypeInfo* rv = i->second;
    // return type "any" for reference types if PO_BROKEN_REFERENCES is set
    if (rv == referenceTypeInfo && (parse_get_parse_options() & PO_BROKEN_REFERENCES))
        rv = anyTypeInfo;
    return rv;
}

const QoreTypeInfo* getBuiltinUserOrNothingTypeInfo(const char* str) {
    str_typeinfo_map_t::iterator i = str_ornothingtypeinfo_map.find(str);
    if (i == str_ornothingtypeinfo_map.end())
        return nullptr;

    const QoreTypeInfo* rv = i->second;
    // return type "any" for reference types if PO_BROKEN_REFERENCES is set
    if (rv == referenceOrNothingTypeInfo && (parse_get_parse_options() & PO_BROKEN_REFERENCES))
        rv = anyTypeInfo;

    return rv;
}

const char* getBuiltinTypeName(qore_type_t type) {
    type_str_map_t::iterator i = type_str_map.find(type);
    if (i != type_str_map.end())
        return i->second;

    const QoreTypeInfo* typeInfo = getExternalTypeInfoForType(type);
    if (typeInfo)
        return QoreTypeInfo::getName(typeInfo);
    return "<unknown type>";
}

// only called for complex hashes and lists
qore_type_result_e match_type(const QoreTypeInfo* this_type, const QoreTypeInfo* that_type,
        bool& may_not_match, bool& may_need_filter) {
    //printd(5, "match_type() '%s' <- '%s'\n", QoreTypeInfo::getName(this_type), QoreTypeInfo::getName(that_type));
    qore_type_result_e res = QoreTypeInfo::parseAccepts(this_type, that_type, may_not_match, may_need_filter);

    // even if types are 100% compatible, if they are not equal, then we perform type folding
    // however if we interpret "may not match" as "no match" here, then we introduce an incompatibility with
    // non-complex types
    // even if types are 100% compatible, if they are not equal, then we perform type folding
    if (res == QTI_IDENT && !may_need_filter && !QoreTypeInfo::equal(this_type, that_type)) {
        may_need_filter = true;
        res = QTI_AMBIGUOUS;
    }
    return res;
}

const char* QoreTypeSpec::getName() const {
    return QoreTypeInfo::getName(getTypeInfo());
}

const QoreClass* QoreTypeSpec::getClass() const {
    if (typespec == QTS_CLASS) {
        return u.qc;
    }
    if (typespec == QTS_PARAMCLASS) {
        return getParameterizedClassTypeInfo()->getBaseClass();
    }
    return nullptr;
}

const QoreParameterizedClassTypeInfo* QoreTypeSpec::getParameterizedClassTypeInfo() const {
    return typespec == QTS_PARAMCLASS ? static_cast<const QoreParameterizedClassTypeInfo*>(u.ti) : nullptr;
}

const char* QoreTypeSpec::getTypeName() const {
    switch (typespec) {
        case QTS_CLASS:
            return u.qc->getName();

        default:
            return QoreTypeInfo::getName(getTypeInfo());
    }
    assert(false);
    return nullptr;
}

const char* QoreTypeSpec::getSimpleTypeName() const {
    switch (typespec) {
        case QTS_CLASS:
            return u.qc->getName();

        default:
            return QoreTypeInfo::getSimpleName(getTypeInfo());
    }
    assert(false);
    return nullptr;
}

static const std::string& getQoreTypeSpecMatchRegistryValidationError() {
    static const std::string error = []() {
        std::string validation_error;
        if (!qore_type_spec_validate_match_registry(validation_error)) {
            return validation_error;
        }
        return std::string();
    }();
    return error;
}

qore_type_result_e QoreTypeSpec::match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter,
        qore_type_result_e& max_result, bool known_initial_assignment) const {
    //printd(5, "QoreTypeSpec::match() typespec: %d t.typespec: %d\n", (int)typespec, (int)t.typespec);
    const std::string& registry_error = getQoreTypeSpecMatchRegistryValidationError();
    assert(registry_error.empty());
    if (!registry_error.empty()) {
        max_result = QTI_NOT_EQUAL;
        return QTI_NOT_EQUAL;
    }

    const QoreTypeSpecMatchHandlerInfo* info = getQoreTypeSpecMatchHandler(typespec);
    if (info && info->handler) {
        QoreTypeSpecMatchCtx ctx{t, may_not_match, may_need_filter, max_result, known_initial_assignment};
        return info->handler(*this, ctx);
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeSpec::checkMatchType(const QoreTypeSpec& t, bool& may_not_match,
        qore_type_result_e& max_result) const {
    qore_type_t ot = t.getType();
    if (u.t == NT_ALL) {
        // issue #3887 if both sides are "ALL" then the match is "NEAR" and not "WILDCARD"
        qore_type_result_e rv = ot == NT_ALL ? QTI_NEAR : QTI_WILDCARD;
        max_result = rv;
        return rv;
    }
    if (ot == NT_ALL) {
        may_not_match = true;
        max_result = QTI_IDENT;
        return QTI_AMBIGUOUS;
    }
    if (u.t == ot) {
        // check special cases
        if ((u.t == NT_LIST || u.t == NT_HASH || u.t == NT_BUFFER) && t.typespec != QTS_TYPE && t.typespec != QTS_EMPTYLIST
                && t.typespec != QTS_EMPTYHASH) {
            max_result = QTI_NEAR;
            return QTI_NEAR;
        }
        max_result = QTI_IDENT;
        return QTI_IDENT;
    }
    max_result = QTI_NOT_EQUAL;
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeSpec::tryMatchReferenceType(const QoreTypeSpec& t, bool& may_not_match) const {
    switch (t.typespec) {
        case QTS_TYPE: {
            return t.u.t == NT_REFERENCE ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
        }

        case QTS_COMPLEXREF: {
            // check if types match
            if (!t.u.ti || t.u.ti->return_vec.empty()) {
                return QTI_AMBIGUOUS;
            }
            qore_type_result_e rv = QTI_NOT_EQUAL;
            for (auto& rt : t.u.ti->return_vec) {
                qore_type_result_e max_result = QTI_NOT_EQUAL;
                qore_type_result_e res = checkMatchType(rt.spec, may_not_match, max_result);
                if (res == QTI_NOT_EQUAL && !may_not_match) {
                    may_not_match = true;
                }
                if (res > rv) {
                    rv = res;
                }
            }
            if (t.u.ti->return_vec.size() == 1) {
                return rv;
            }
            if (rv > QTI_AMBIGUOUS) {
                return QTI_AMBIGUOUS;
            }
            return rv;
        }

        default:
            break;
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeSpec::matchType(qore_type_t t) const {
    if (typespec == QTS_TYPEPARAM) {
        return QTI_AMBIGUOUS;
    } else if (typespec == QTS_WILDCARD) {
        return QTI_NOT_EQUAL;
    } else if (typespec == QTS_CLASS || typespec == QTS_PARAMCLASS) {
        return t == NT_OBJECT ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_HASHDECL || typespec == QTS_COMPLEXHASH) {
        return t == NT_HASH ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXLIST) {
        return t == NT_LIST ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXBUFFER) {
        return t == NT_BUFFER ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXSOFTLIST) {
        if (t == NT_LIST) {
            return QTI_IDENT;
        }
        return QoreTypeInfo::parseAcceptsReturns(u.ti, t) ? QTI_NEAR : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXHARDREF || typespec == QTS_HARDREF) {
        return t == NT_REFERENCE ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXREF) {
        if (t == NT_REFERENCE) {
            return QTI_NEAR;
        }
        if (QoreTypeInfo::hasType(u.ti)) {
            return QoreTypeInfo::parseAcceptsReturns(u.ti, t) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
        }
        return QTI_WILDCARD;
    } else if (typespec == QTS_ENUM) {
        // Enum types match their underlying base type at runtime
        qore_type_t bt = QoreTypeInfo::getBaseType(u.ed->getBaseTypeInfo());
        return t == bt ? QTI_IDENT : QTI_NOT_EQUAL;
    }
    if (u.t == NT_ALL || u.t == NT_REFERENCE) {
        return QTI_WILDCARD;
    }
    return u.t == t ? QTI_IDENT : QTI_NOT_EQUAL;
}

static bool type_spec_accept_object(const QoreClass& type_class, const QoreClass& object_class, bool& priv_error) {
    assert(!priv_error);

    bool priv;
    if (!object_class.getClass(type_class, priv)) {
        return false;
    }
    if (!priv) {
        return true;
    }
    // check access
    if (qore_class_private::runtimeCheckPrivateClassAccess(type_class)) {
        return true;
    }
    priv_error = true;
    return false;
}

static bool qore_generic_container_value_may_fold_to(const QoreTypeInfo* target_ti,
        const QoreTypeInfo* source_ti) {
    if (!QoreTypeInfo::hasType(target_ti) || !QoreTypeInfo::hasType(source_ti)) {
        return false;
    }

    const QoreTypeInfo* hash_value_type = QoreTypeInfo::getComplexHashValueType(source_ti);
    if ((hash_value_type == autoTypeInfo || hash_value_type == autoNoNarrowTypeInfo)
            && QoreTypeInfo::parseAcceptsReturns(target_ti, NT_HASH)) {
        return true;
    }

    const QoreTypeInfo* list_value_type = QoreTypeInfo::getComplexListValueType(source_ti);
    return (list_value_type == autoTypeInfo || list_value_type == autoNoNarrowTypeInfo)
        && QoreTypeInfo::parseAcceptsReturns(target_ti, NT_LIST);
}

bool QoreTypeSpec::acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
        bool& err) const {
    assert(!err);
    const QoreTypeInfo* ti = h->getValueTypeInfo();
    if (QoreTypeInfo::equal(u.ti, ti)) {
        return true;
    }
    if (ti && QoreTypeInfo::hasType(ti) && !QoreTypeInfo::parseAccepts(u.ti, ti)
            && !qore_generic_container_value_may_fold_to(u.ti, ti)) {
        return false;
    }

    // try to fold values into our type; value types are not identical;
    // we have to get a new hash
    if (!h->is_unique()) {
        AbstractQoreNode* p = n.assign(h = qore_hash_private::get(*h)->copy(get_value_type(&typeInfo)));
        if (lvhelper) {
            lvhelper->saveTemp(p);
        } else {
            discard(p, xsink);
            if (xsink && *xsink) {
                err = true;
                return true;
            }
        }
    } else {
        // Use the base complex type, not the optional field type
        const QoreTypeInfo* complex_type = qore_get_complex_hash_type(u.ti);
        qore_hash_private* hp = qore_hash_private::get(*h);
        hp->complexTypeInfo = complex_type;
    }

    // now we have to fold the value types into our type
    HashIterator i(h);
    while (i.next()) {
        hash_assignment_priv ha(*qore_hash_private::get(*h), *qhi_priv::get(i)->i);
        QoreValue hn(ha.swap(QoreValue()));

        if (QoreTypeInfo::runtimeAcceptsValue(u.ti, hn) == QTI_NOT_EQUAL
                && !QoreTypeInfo::retypeValue(hn, u.ti, xsink)) {
            ha.swap(hn);
            err = true;
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while folding values into type 'hash<%s>')",
                    QoreTypeInfo::getName(u.ti));
            }
            return true;
        }
        u.ti->acceptInputIntern(xsink, arg_type, obj, param_num, param_name, hn, lvhelper);
        ha.swap(hn);
        if (xsink && *xsink) {
            err = true;
            // enrich exception so that it's not confusing
            xsink->appendLastDescription(" (while folding values into type 'hash<%s>')", QoreTypeInfo::getName(u.ti));
            return true;
        }
    }

    return true;
}

bool QoreTypeSpec::acceptInputComplexList(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
        bool& err) const {
    const QoreTypeInfo* ti = l->getValueTypeInfo();
    if (QoreTypeInfo::equal(u.ti, ti)) {
        return true;
    }
    if (ti && QoreTypeInfo::hasType(ti) && !QoreTypeInfo::parseAccepts(u.ti, ti)
            && !qore_generic_container_value_may_fold_to(u.ti, ti)) {
        return false;
    }

    // try to fold values into our type; value types are not identical;
    // we have to get a new list
    qore_list_private* lp;
    if (!l->is_unique()) {
        AbstractQoreNode* p = n.assign(l = qore_list_private::get(*l)->copy(get_value_type(&typeInfo)));
        if (lvhelper) {
            lvhelper->saveTemp(p);
        } else {
            discard(p, xsink);
            if (xsink && *xsink) {
                err = true;
                return true;
            }
        }
        lp = qore_list_private::get(*l);
    } else {
        lp = qore_list_private::get(*l);
        // Use the base complex type, not the optional field type
        const QoreTypeInfo* complex_type = qore_get_complex_list_type(u.ti);
        lp->complexTypeInfo = complex_type;
    }

    // now we have to fold the value types into our type
    for (size_t i = 0; i < l->size(); ++i) {
        QoreValue ln(lp->takeExists(i));
        if (QoreTypeInfo::runtimeAcceptsValue(u.ti, ln) == QTI_NOT_EQUAL
                && !QoreTypeInfo::retypeValue(ln, u.ti, xsink)) {
            lp->swap(i, ln);
            err = true;
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while folding values into type 'list<%s>')",
                    QoreTypeInfo::getName(u.ti));
            }
            return true;
        }
        u.ti->acceptInputIntern(xsink, arg_type, obj, param_num, param_name, ln, lvhelper);
        lp->swap(i, ln);
        if (xsink && *xsink) {
            err = true;
            // enrich exception so that it's not confusing
            xsink->appendLastDescription(" (while folding values into type 'list<%s>')", QoreTypeInfo::getName(u.ti));
            return true;
        }
    }

    return true;
}

static bool qore_complex_buffer_accepts_runtime(const QoreTypeInfo* target_ti, const QoreBufferNode& source,
        bool& needs_nullable_copy) {
    needs_nullable_copy = false;
    const QoreComplexBufferTypeInfo* target = QoreTypeInfo::getComplexBufferType(target_ti);
    if (!target || target->getBufferElementType() != source.getElementType()) {
        return false;
    }
    if (target->hasNullableElements() == source.hasNullableElements()) {
        return true;
    }
    if (target->hasNullableElements() && !source.hasNullableElements()) {
        needs_nullable_copy = true;
        return true;
    }
    return false;
}

bool QoreTypeSpec::acceptInput(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, q_type_map_t map,
        const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
        LValueHelper* lvhelper) const {
    bool priv_error = false;
    bool ok = false;

    //printd(5, "QoreTypeInfo::acceptInput() typeInfo: %s spec: %s arg_type: %s val: %s: OK\n", QoreTypeInfo::getName(&typeInfo), getName(), arg_type, n.getFullTypeName());

    qore_type_t t = n.getType();
    switch (typespec) {
        case QTS_CLASS: {
            if (t == NT_OBJECT) {
                const QoreObject* obj = n.get<const QoreObject>();
                const qore_class_private* target = qore_class_private::get(*u.qc);
                if (!obj->getInstantiatedTypeInfo() || !target->hasTypeParams()
                        || target->rawAcceptsParameterized()) {
                    ok = type_spec_accept_object(*u.qc, *obj->getClass(), priv_error);
                }
            } else if (t == NT_WEAKREF) {
                const QoreObject* obj = n.get<const WeakReferenceNode>()->get();
                const qore_class_private* target = qore_class_private::get(*u.qc);
                if (!obj->getInstantiatedTypeInfo() || !target->hasTypeParams()
                        || target->rawAcceptsParameterized()) {
                    ok = type_spec_accept_object(*u.qc, *obj->getClass(), priv_error);
                }
            }
            break;
        }
        case QTS_PARAMCLASS: {
            const QoreObject* obj = nullptr;
            if (t == NT_OBJECT) {
                obj = n.get<const QoreObject>();
            } else if (t == NT_WEAKREF) {
                obj = n.get<const WeakReferenceNode>()->get();
            }
            if (obj && obj->getInstantiatedTypeInfo() == u.ti) {
                ok = true;
            } else if (obj && obj->getInstantiatedTypeInfo()) {
                const QoreParameterizedClassTypeInfo* target_pti = getParameterizedClassTypeInfo();
                if (target_pti) {
                    bool type_may_not_match = false;
                    bool type_may_need_filter = false;
                    ok = qore_parameterized_class_accepts(target_pti, obj->getInstantiatedTypeInfo(),
                        type_may_not_match, type_may_need_filter) != QTI_NOT_EQUAL;
                }
            } else if (obj) {
                const QoreParameterizedClassTypeInfo* target_pti = getParameterizedClassTypeInfo();
                if (target_pti) {
                    const QoreTypeInfo* mapped_type = qore_class_private::get(*obj->getClass())
                        ->getConcreteParameterizedBaseTypeInfo(target_pti->getBaseClass());
                    bool type_may_not_match = false;
                    bool type_may_need_filter = false;
                    if (QoreTypeInfo::equal(mapped_type, u.ti)
                            || qore_parameterized_class_accepts(target_pti, mapped_type, type_may_not_match,
                                type_may_need_filter) != QTI_NOT_EQUAL) {
                        ok = true;
                    }
                }
            }
            break;
        }
        case QTS_HASHDECL: {
            const TypedHashDecl* hd = nullptr;
            if (t == NT_HASH) {
                hd = n.get<const QoreHashNode>()->getHashDecl();
            } else if (t == NT_WEAKREF_HASH) {
                hd = n.get<const WeakHashReferenceNode>()->get()->getHashDecl();
            }
            if (hd) {
                const typed_hash_decl_private* target = typed_hash_decl_private::get(*u.hd);
                const typed_hash_decl_private* source = typed_hash_decl_private::get(*hd);
                bool type_may_not_match = false;
                bool type_may_need_filter = false;
                // Accept if same hashdecl or source is a descendant of target
                if (source->equal(*target)
                        || qore_parameterized_hashdecl_accepts(u.hd, hd, type_may_not_match, type_may_need_filter)
                            != QTI_NOT_EQUAL
                        || (!target->isParameterizedHashDecl() && source->isDescendantOf(*target))) {
                    ok = true;
                }
            }
            break;
        }
        case QTS_COMPLEXHASH: {
            if (t == NT_HASH) {
                if (is_auto_vti(u.ti)) {
                    ok = true;
                    break;
                }
                QoreHashNode* h = n.get<QoreHashNode>();
                bool err = false;
                ok = acceptInputComplexHash(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, h,
                    err);
                if (err) {
                    return true;
                }
            } else if (t == NT_WEAKREF_HASH) {
                if (is_auto_vti(u.ti)) {
                    ok = true;
                    break;
                }
                QoreHashNode* h = n.get<WeakHashReferenceNode>()->get();
                bool err = false;
                ok = acceptInputComplexHash(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, h,
                    err);
                if (err) {
                    return true;
                }
            }
            break;
        }
        case QTS_COMPLEXSOFTLIST:
        case QTS_COMPLEXLIST: {
            if (n.getType() == NT_LIST) {
                if (is_auto_vti(u.ti)) {
                    ok = true;
                    break;
                }
                QoreListNode* l = n.get<QoreListNode>();
                bool err = false;
                ok = acceptInputComplexList(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, l,
                    err);
            } else if (typespec == QTS_COMPLEXSOFTLIST) {
                // see if value matches
                if (QoreTypeInfo::runtimeAcceptsValue(u.ti, n) > 0) {
                    ok = true;
                }
            }
            break;
        }
        case QTS_COMPLEXBUFFER: {
            if (n.getType() == NT_BUFFER) {
                QoreBufferNode* b = n.get<QoreBufferNode>();
                bool needs_nullable_copy = false;
                ok = qore_complex_buffer_accepts_runtime(u.ti, *b, needs_nullable_copy);
                if (needs_nullable_copy) {
                    QoreBufferNode* copy = b->copy(true, xsink);
                    if (!copy) {
                        return true;
                    }
                    AbstractQoreNode* old = n.assign(copy);
                    if (lvhelper) {
                        lvhelper->saveTemp(old);
                    } else {
                        discard(old, xsink);
                        if (xsink && *xsink) {
                            return true;
                        }
                    }
                }
            }
            break;
        }
        case QTS_HARDREF: {
            if (n.getType() == NT_REFERENCE) {
                ok = true;
            }
            break;
        }
        case QTS_COMPLEXHARDREF:
        case QTS_COMPLEXREF: {
            if (n.getType() == NT_REFERENCE) {
                // issue #2889 cannot assign a reference while assigning an lvalue and holding a write lock
                assert(!lvhelper);
                ReferenceNode* r = n.get<ReferenceNode>();
                const QoreTypeInfo* ti = r->getLValueTypeInfo();
                //printd(5, "cr: %p '%s' == %p '%s': %d\n", u.ti, QoreTypeInfo::getName(u.ti), ti, QoreTypeInfo::getName(ti), QoreTypeInfo::outputSuperSetOf(ti, u.ti));
                // first check types before instantiating reference
                if (QoreTypeInfo::outputSuperSetOf(ti, u.ti)) {
                    // issue #2891: do not create a value in the source reference if none already exists
                    // do not process if there is no type restriction
                    LValueHelper lvh(*r, xsink, true);
                    //printd(5, "lvh: %d *xsink: %d\n", (bool)lvh, (bool)*xsink);
                    if (lvh) {
                        QoreValue val = lvh.getReferencedValue();
                        if (!val.isNothing()) {
                            lvh.setTypeInfo(u.ti);
                            //printd(5, "ref assign '%s' to '%s'\n", QoreTypeInfo::getName(val.getTypeInfo()), QoreTypeInfo::getName(u.ti));
                            lvh.assign(val, "<reference>");
                        }
                        // we set ok unconditionally here, because any exception thrown above is enough if there is an error
                        ok = true;
                    } else if (!xsink || !*xsink) {
                        // issue #2891 the lvalue may not exist, but we can still perform the assignment
                        ok = true;
                    }
                }
            }
            break;
        }
        case QTS_TYPE: {
            qore_type_t t = n.getType();
            // Unwrap TAG_ENUM to base type only for direct lvalue/parameter assignments
            // (e.g. "string s = EnumVal"), NOT for hash key or member assignments where
            // the TAG_ENUM must be preserved through containers for later typed retrieval
            if (n.isEnum() && u.t != NT_ALL
                && strcmp(arg_type, "key") && strcmp(arg_type, "member")) {
                QoreValue base_val = n.getEnumMember()->getValue();
                base_val.ref();
                n = base_val;
            }
            // special handling for objects; check for object validity; if it's already been deleted,
            // then we use NT_NOTHING
            if (t == NT_OBJECT && !n.get<QoreObject>()->isValid()) {
                t = NT_NOTHING;
            }
            if (u.t == NT_ALL || u.t == t) {
                ok = true;
            }
            break;
        }

        case QTS_EMPTYLIST:
        case QTS_EMPTYHASH:
            if (u.t == NT_ALL || u.t == n.getType()) {
                ok = true;
            }
            break;

        case QTS_ENUM: {
            // Accept TAG_ENUM values from same enum declaration
            if (n.isEnum()) {
                const QoreEnumMember* member = n.getEnumMember();
                if (member->getEnumDecl() == u.ed
                    || qore_enum_decl_private::get(*member->getEnumDecl())->parseEqual(
                        *qore_enum_decl_private::get(*u.ed))) {
                    ok = true;
                }
            }
            break;
        }

    }

    if (ok) {
        assert(!priv_error);
        if (map) {
            map(n, xsink);
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while converting types for type '%s')",
                    QoreTypeInfo::getName(&typeInfo));
            }
        }
        return true;
    }

    if (priv_error) {
        typeInfo.doAcceptError(true, arg_type, obj, param_num, param_name, n, xsink);
        return true;
    }
    return false;
}

bool QoreTypeSpec::operator==(const QoreTypeSpec& other) const {
    if (typespec != other.typespec)
        return false;
    switch (typespec) {
        case QTS_TYPE:
        case QTS_EMPTYLIST:
        case QTS_EMPTYHASH:
            return u.t == other.u.t;
        case QTS_CLASS:
            return qore_class_private::get(*u.qc)->equal(*qore_class_private::get(*other.u.qc));
        case QTS_HASHDECL:
            return typed_hash_decl_private::get(*u.hd)->equal(*typed_hash_decl_private::get(*other.u.hd));
        case QTS_COMPLEXHASH:
        case QTS_COMPLEXLIST:
        case QTS_COMPLEXSOFTLIST:
        case QTS_COMPLEXHARDREF:
        case QTS_COMPLEXREF:
            return QoreTypeInfo::equal(u.ti, other.u.ti);
        case QTS_COMPLEXBUFFER:
            return u.ti == other.u.ti;
        case QTS_PARAMCLASS:
        case QTS_TYPEPARAM:
        case QTS_WILDCARD:
            return u.ti == other.u.ti;
        case QTS_HARDREF:
             return true;
        case QTS_ENUM:
            return u.ed == other.u.ed;
    }
    return false;
}

bool QoreTypeSpec::operator!=(const QoreTypeSpec& other) const {
   return !(*this == other);
}

qore_type_result_e QoreTypeSpec::runtimeAcceptsValue(const QoreValue& n, bool exact) const {
    qore_type_t ot = n.getType();
    // issue #2928 we must ensure that each typespec only access its own data in the union
    switch (typespec) {
        case QTS_CLASS: {
            const QoreObject* obj = nullptr;
            if (ot == NT_OBJECT) {
                obj = n.get<const QoreObject>();
            } else if (ot == NT_WEAKREF) {
                obj = n.get<const WeakReferenceNode>()->get();
            }
            if (obj) {
                const qore_class_private* target = qore_class_private::get(*u.qc);
                if (obj->getInstantiatedTypeInfo() && target->hasTypeParams()
                        && !target->rawAcceptsParameterized()) {
                    return QTI_NOT_EQUAL;
                }
                qore_type_result_e rv = qore_class_private::runtimeCheckCompatibleClass(*u.qc, *obj->getClass());
                if (rv == QTI_NOT_EQUAL) {
                    return rv;
                }
                // issue #3272: do not return a match for deleted objects
                if (!obj->isValid()) {
                    return QTI_NOT_EQUAL;
                }
                return (rv == QTI_IDENT && exact) ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_PARAMCLASS: {
            const QoreObject* obj = nullptr;
            if (ot == NT_OBJECT) {
                obj = n.get<const QoreObject>();
            } else if (ot == NT_WEAKREF) {
                obj = n.get<const WeakReferenceNode>()->get();
            }
            if (!obj || !obj->isValid()) {
                return QTI_NOT_EQUAL;
            }
            if (obj->getInstantiatedTypeInfo() == u.ti) {
                return exact ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            if (obj->getInstantiatedTypeInfo()) {
                const QoreParameterizedClassTypeInfo* target_pti = getParameterizedClassTypeInfo();
                if (target_pti) {
                    bool type_may_not_match = false;
                    bool type_may_need_filter = false;
                    qore_type_result_e rv = qore_parameterized_class_accepts(target_pti, obj->getInstantiatedTypeInfo(),
                        type_may_not_match, type_may_need_filter);
                    if (rv != QTI_NOT_EQUAL) {
                        return exact && rv == QTI_IDENT ? QTI_IDENT : QTI_AMBIGUOUS;
                    }
                }
            } else {
                const QoreParameterizedClassTypeInfo* target_pti = getParameterizedClassTypeInfo();
                if (target_pti) {
                    const QoreTypeInfo* mapped_type = qore_class_private::get(*obj->getClass())
                        ->getConcreteParameterizedBaseTypeInfo(target_pti->getBaseClass());
                    bool type_may_not_match = false;
                    bool type_may_need_filter = false;
                    if (QoreTypeInfo::equal(mapped_type, u.ti)
                            || qore_parameterized_class_accepts(target_pti, mapped_type, type_may_not_match,
                                type_may_need_filter) != QTI_NOT_EQUAL) {
                        return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                    }
                }
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_HASHDECL: {
            const TypedHashDecl* hd = nullptr;
            if (ot == NT_HASH) {
                hd = n.get<const QoreHashNode>()->getHashDecl();
            } else if (ot == NT_WEAKREF_HASH) {
                hd = n.get<const WeakHashReferenceNode>()->get()->getHashDecl();
            }
            if (hd) {
                const typed_hash_decl_private* target = typed_hash_decl_private::get(*u.hd);
                const typed_hash_decl_private* source = typed_hash_decl_private::get(*hd);
                if (source->equal(*target)) {
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
                bool type_may_not_match = false;
                bool type_may_need_filter = false;
                qore_type_result_e rv = qore_parameterized_hashdecl_accepts(u.hd, hd, type_may_not_match,
                    type_may_need_filter);
                if (rv != QTI_NOT_EQUAL) {
                    return exact && rv == QTI_IDENT ? QTI_IDENT : QTI_AMBIGUOUS;
                }
                if (target->isParameterizedHashDecl()) {
                    return QTI_NOT_EQUAL;
                }
                // Accept if source is a descendant of target (derived → base)
                if (source->isDescendantOf(*target)) {
                    return QTI_AMBIGUOUS;
                }
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_COMPLEXHASH: {
            const QoreTypeInfo* ti = nullptr;
            const QoreHashNode* h = nullptr;
            if (ot == NT_HASH) {
                h = n.get<const QoreHashNode>();
                ti = h->getValueTypeInfo();
            } else if (ot == NT_WEAKREF_HASH) {
                h = n.get<const WeakHashReferenceNode>()->get();
                ti = h->getValueTypeInfo();
            }
            if (!h) {
                return QTI_NOT_EQUAL;
            }
            if (is_auto_vti(u.ti)) {
                return QTI_NEAR;
            }
            // CRITICAL: A hasddecl-typed hash (with a specific structure) cannot match a complex hash type
            // with a SPECIFIC value type (a flexible map with string keys). They are mutually exclusive types.
            // This prevents incorrect variant selection when a hasddecl is passed where a complex hash
            // with specific value type is expected. However, hash<auto> should accept any hash including hasddecls.
            if (h->getHashDecl() && u.ti != autoTypeInfo) {
                return QTI_NOT_EQUAL;
            }
            if (ti && QoreTypeInfo::hasType(ti) && QoreTypeInfo::parseAccepts(u.ti, ti)) {
                return exact ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            // issue #2647: allow an empty hash with no specific type to be passed to any complex hash type
            // it will get folded at runtime into the desired type in any case
            // NOTE: if the hash has a specific type (ti with hasType), it must be compatible - checked above
            if (h->empty() && !h->getHashDecl() && (!ti || !QoreTypeInfo::hasType(ti))) {
                return QTI_NEAR;
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_COMPLEXLIST:
        case QTS_COMPLEXSOFTLIST: {
            const QoreTypeInfo* ti = nullptr;
            const QoreListNode* l = nullptr;
            if (ot == NT_LIST) {
                l = n.get<const QoreListNode>();
                ti = l->getValueTypeInfo();
            } else if (ot == NT_WEAKREF_LIST) {
                l = n.get<const WeakListReferenceNode>()->get();
                ti = l->getValueTypeInfo();
            }
            if (l) {
                if (is_auto_vti(u.ti)) {
                    return QTI_NEAR;
                }
                if (ti && QoreTypeInfo::hasType(ti) && QoreTypeInfo::parseAccepts(u.ti, ti)) {
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
                // issue #2647: allow an empty list with no specific type to be passed to any complex list type
                // it will get folded at runtime into the desired type in any case
                // NOTE: if the list has a specific type (ti with hasType), it must be compatible - checked above
                if (l->empty() && (!ti || !QoreTypeInfo::hasType(ti))) {
                    return QTI_NEAR;
                }
            }
            if (typespec == QTS_COMPLEXSOFTLIST) {
                qore_type_result_e rv = QoreTypeInfo::runtimeAcceptsValue(u.ti, n);
                if (rv > 0) {
                    // do not return an exact match if we have to convert to a list
                    if (rv == QTI_IDENT) {
                        rv = QTI_NEAR;
                    }
                    return rv;
                }
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_COMPLEXBUFFER: {
            if (ot != NT_BUFFER) {
                return QTI_NOT_EQUAL;
            }
            const QoreBufferNode* b = n.get<const QoreBufferNode>();
            bool needs_nullable_copy = false;
            if (!qore_complex_buffer_accepts_runtime(u.ti, *b, needs_nullable_copy)) {
                return QTI_NOT_EQUAL;
            }
            return needs_nullable_copy ? QTI_NEAR : (exact ? QTI_IDENT : QTI_AMBIGUOUS);
        }

        case QTS_COMPLEXREF:
            if (ot == NT_REFERENCE) {
                const QoreTypeInfo* ti = n.get<const ReferenceNode>()->getLValueTypeInfo();
                //printd(5, "QoreTypeSpec::runtimeAcceptsValue() cr ti: '%s' typeInfo: '%s' eq: %d ss: %d\n",
                //    QoreTypeInfo::getName(ti), QoreTypeInfo::getName(u.ti), QoreTypeInfo::equal(u.ti, ti),
                //    QoreTypeInfo::outputSuperSetOf(ti, u.ti));
                if (QoreTypeInfo::equal(u.ti, ti))
                    return QTI_IDENT;
                if (QoreTypeInfo::outputSuperSetOf(ti, u.ti))
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            return QTI_NOT_EQUAL;

        case QTS_EMPTYLIST:
        case QTS_EMPTYHASH:
        case QTS_TYPE:
            // check special cases
            if (u.t == NT_HASH) {
                const qore_hash_private* h = nullptr;
                if (ot == NT_HASH) {
                    h = qore_hash_private::get(*n.get<const QoreHashNode>());
                } else if (ot == NT_WEAKREF_HASH) {
                    h = qore_hash_private::get(*n.get<const WeakHashReferenceNode>()->get());
                }
                if (h) {
                    if (h->hashdecl || h->complexTypeInfo) {
                        return QTI_NEAR;
                    }
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
            }
            if (u.t == NT_LIST) {
                const qore_list_private* l = nullptr;
                if (ot == NT_LIST) {
                    l = qore_list_private::get(*n.get<const QoreListNode>());
                } else if (ot == NT_WEAKREF_LIST) {
                    l = qore_list_private::get(*n.get<const WeakListReferenceNode>()->get());
                }
                if (l) {
                    if (l->complexTypeInfo) {
                        return QTI_NEAR;
                    }
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
            }
            if (u.t == NT_BUFFER && ot == NT_BUFFER) {
                return QTI_NEAR;
            }

            if (u.t == NT_ALL || u.t == ot) {
                return exact ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            break;

        case QTS_ENUM: {
            // Accept TAG_ENUM values from same enum declaration
            if (n.isEnum()) {
                const QoreEnumMember* member = n.getEnumMember();
                if (member->getEnumDecl() == u.ed
                    || qore_enum_decl_private::get(*member->getEnumDecl())->parseEqual(
                        *qore_enum_decl_private::get(*u.ed))) {
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_TYPEPARAM:
            return QTI_NOT_EQUAL;

        default:
            assert(false);
    }

    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeInfo::runtimeAcceptsValue(const QoreValue& n) const {
    for (auto& t : accept_vec) {
        qore_type_result_e rv = t.spec.runtimeAcceptsValue(n, t.exact);
        if (rv != QTI_NOT_EQUAL)
            return rv;
    }
    return QTI_NOT_EQUAL;
}

void QoreTypeInfo::doNonNumericWarning(const QoreProgramLocation* loc, const char* preface) const {
    QoreStringNode* desc = new QoreStringNode(preface);
    getThisTypeImpl(*desc);
    desc->sprintf(", which does not evaluate to a numeric type, therefore will always evaluate to 0 at runtime");
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", desc);
}

void QoreTypeInfo::doNonBooleanWarning(const QoreProgramLocation* loc, const char* preface) const {
    QoreStringNode* desc = new QoreStringNode(preface);
    getThisTypeImpl(*desc);
    desc->sprintf(", which does not evaluate to a numeric or boolean type, therefore will always evaluate to False at runtime");
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", desc);
}

void QoreTypeInfo::doNonStringWarning(const QoreProgramLocation* loc, const char* preface) const {
    QoreStringNode* desc = new QoreStringNode(preface);
    getThisTypeImpl(*desc);
    desc->sprintf(", which cannot be converted to a string, therefore will always evaluate to an empty string at runtime");
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", desc);
}

void QoreTypeInfo::doNonStringError(const QoreProgramLocation* loc, const char* preface) const {
    QoreStringNode* desc = new QoreStringNode(preface);
    getThisTypeImpl(*desc);
    desc->sprintf(", which cannot be converted to a string, therefore will always evaluate to an empty string at runtime");
    parseException(*loc, "INVALID-OPERATION", desc);
}

void QoreTypeInfo::stripTypeInfo(QoreValue& n, ExceptionSink* xsink, LValueHelper* lvhelper) {
    // strips complex typeinfo for an assignment to an untyped lvalue
    switch (n.getType()) {
        case NT_HASH: {
            if (lvhelper) {
                map_get_plain_hash_lvalue(n, xsink, lvhelper);
            } else {
                map_get_plain_hash(n, xsink);
            }
            break;
        }
        case NT_LIST: {
            if (lvhelper) {
                map_get_plain_list_lvalue(n, xsink, lvhelper);
            } else {
                map_get_plain_list(n, xsink);
            }
            break;
        }
    }
}

const QoreTypeInfo* QoreTypeInfo::getHardReference(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return ti;
    }
    if (!QoreTypeInfo::parseAcceptsReturns(ti, NT_REFERENCE)) {
        return ti;
    }
    if (ti == referenceTypeInfo) {
        return &staticHardReferenceTypeInfo;
    }
    if (ti == referenceOrNothingTypeInfo) {
        return &staticHardReferenceOrNothingTypeInfo;
    }
    {
        const QoreComplexReferenceTypeInfo* type = dynamic_cast<const QoreComplexReferenceTypeInfo*>(ti);
        if (type) {
            return type->getHardReference();
        }
    }
    const QoreComplexReferenceOrNothingTypeInfo* type = dynamic_cast<const QoreComplexReferenceOrNothingTypeInfo*>(ti);
    assert(type);
    return type->getHardReference();
}

#if 0
const QoreTypeInfo* QoreTypeInfo::getRuntimeType(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return ti;
    }
    if (!QoreTypeInfo::parseAcceptsReturns(ti, NT_REFERENCE)) {
        return ti;
    }
    if (dynamic_cast<const QoreReferenceTypeInfo*>(ti) || dynamic_cast<const QoreReferenceOrNothingTypeInfo*>(ti)) {
        return autoTypeInfo;
    }
    {
        const QoreComplexReferenceTypeInfo* type = dynamic_cast<const QoreComplexReferenceTypeInfo*>(ti);
        if (type) {
            return getRuntimeType(type->getRuntimeType());
        }
    }
    const QoreComplexReferenceOrNothingTypeInfo* type = dynamic_cast<const QoreComplexReferenceOrNothingTypeInfo*>(ti);
    assert(type);
    return getRuntimeType(type->getRuntimeType());
}
#endif

template <typename T>
bool typespec_vec_compare(const T& a, const T& b) {
    if (a.size() != b.size())
        return false;
    for (unsigned i = 0; i < a.size(); ++i) {
        if (a[i].spec != b[i].spec)
            return false;
    }
    return true;
}

bool accept_vec_compare(const q_accept_vec_t& a, const q_accept_vec_t& b) {
    return typespec_vec_compare<q_accept_vec_t>(a, b);
}

bool return_vec_compare(const q_return_vec_t& a, const q_return_vec_t& b) {
    return typespec_vec_compare<q_return_vec_t>(a, b);
}

static const char* qore_type_arg_plural(size_t count) {
    return count == 1 ? "" : "s";
}

static void qore_raise_type_arg_count_error(const QoreProgramLocation* loc, const char* resolved_name,
        const char* kind, const char* generic_name, size_t expected, size_t required, size_t actual,
        const char* missing_param, int& err) {
    if (expected == required) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; generic %s '%s' declares %d type "
            "parameter%s, but %d type argument%s %s provided", resolved_name, kind, generic_name, (int)expected,
            qore_type_arg_plural(expected), (int)actual, qore_type_arg_plural(actual),
            actual == 1 ? "was" : "were");
    } else if (actual < required && missing_param) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; generic %s '%s' declares %d type "
            "parameter%s (%d required, %d defaulted), but %d type argument%s %s provided; missing required type "
            "parameter '%s'", resolved_name, kind, generic_name, (int)expected, qore_type_arg_plural(expected),
            (int)required, (int)(expected - required), (int)actual, qore_type_arg_plural(actual),
            actual == 1 ? "was" : "were", missing_param);
    } else {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; generic %s '%s' declares %d type "
            "parameter%s (%d required, %d defaulted), but %d type argument%s %s provided", resolved_name, kind,
            generic_name, (int)expected, qore_type_arg_plural(expected), (int)required,
            (int)(expected - required), (int)actual, qore_type_arg_plural(actual),
            actual == 1 ? "was" : "were");
    }
    err = -1;
}

static const QoreTypeInfo* qore_resolve_parse_default_type_arg(const char* default_type,
        const QoreProgramLocation* loc, int& err) {
    std::unique_ptr<QoreParseTypeInfo> default_pti(qore_parse_type_string_to_pti(default_type));
    if (!default_pti) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve generic type parameter default '%s'",
            default_type ? default_type : "");
        err = -1;
        return autoTypeInfo;
    }
    const QoreTypeInfo* arg = QoreParseTypeInfo::resolveAny(default_pti.get(), loc, err);
    return arg ? arg : autoTypeInfo;
}

static const QoreTypeInfo* qore_resolve_runtime_default_type_arg(const char* default_type) {
    std::unique_ptr<QoreParseTypeInfo> default_pti(qore_parse_type_string_to_pti(default_type));
    return default_pti ? QoreParseTypeInfo::resolveRuntime(default_pti.get()) : nullptr;
}

static int qore_check_parse_type_arg_bound(const QoreProgramLocation* loc, const char* resolved_name,
        const char* kind, const char* generic_name, const char* param_name, const char* bound_type,
        const QoreTypeInfo* arg) {
    if (!bound_type || !*bound_type) {
        return 0;
    }

    int err = 0;
    std::unique_ptr<QoreParseTypeInfo> bound_pti(qore_parse_type_string_to_pti(bound_type));
    if (!bound_pti) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve bound type '%s' for type parameter '%s' of "
            "generic %s '%s'", bound_type, param_name, kind, generic_name);
        return -1;
    }

    const QoreTypeInfo* bound = QoreParseTypeInfo::resolveAny(bound_pti.get(), loc, err);
    if (err) {
        return -1;
    }

    bool may_not_match = false;
    qore_type_result_e res = QoreTypeInfo::parseAccepts(bound, arg, may_not_match);
    if (res == QTI_NOT_EQUAL || may_not_match) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; type argument '%s' for type parameter "
            "'%s' of generic %s '%s' does not satisfy bound '%s'", resolved_name,
            QoreTypeInfo::getName(arg), param_name, kind, generic_name, bound_type);
        return -1;
    }

    return 0;
}

static bool qore_runtime_type_arg_satisfies_bound(const char* bound_type, const QoreTypeInfo* arg) {
    if (!bound_type || !*bound_type) {
        return true;
    }

    std::unique_ptr<QoreParseTypeInfo> bound_pti(qore_parse_type_string_to_pti(bound_type));
    if (!bound_pti) {
        return false;
    }

    const QoreTypeInfo* bound = QoreParseTypeInfo::resolveRuntime(bound_pti.get());
    if (!bound) {
        return false;
    }

    bool may_not_match = false;
    qore_type_result_e res = QoreTypeInfo::parseAccepts(bound, arg, may_not_match);
    return res != QTI_NOT_EQUAL && !may_not_match;
}

static const QoreTypeInfo* qore_resolve_parse_type_parameter(const NamedScope& cscope, bool or_nothing) {
    if (cscope.size() != 1) {
        return nullptr;
    }

    const char* name = cscope.getIdentifier();
    const typed_hash_decl_private* hd = parse_get_hashdecl_type_param_context();
    if (hd && hd->hasTypeParams()) {
        for (size_t i = 0, e = hd->getTypeParamCount(); i < e; ++i) {
            const char* param = hd->getTypeParamName(i);
            if (!strcmp(name, param)) {
                return qore_get_hashdecl_type_parameter_type(hd->getHashDecl(), i, param, or_nothing);
            }
        }
    }

    const UserSignature* sig = parse_get_signature_type_param_context();
    if (sig && sig->hasTypeParameters()) {
        for (size_t i = 0, e = sig->getTypeParameterCount(); i < e; ++i) {
            const char* param = sig->getTypeParameterName(i);
            if (!strcmp(name, param)) {
                return qore_get_signature_type_parameter_type(sig, i, param, or_nothing);
            }
        }
    }

    qore_class_private* qc = parse_get_class_priv();
    if (qc && qc->hasTypeParams()) {
        for (size_t i = 0, e = qc->getTypeParamCount(); i < e; ++i) {
            const char* param = qc->getTypeParamName(i);
            if (!strcmp(name, param)) {
                return qore_get_type_parameter_type(qc->cls, i, param, or_nothing);
            }
        }
    }

    return nullptr;
}

static TypedHashDecl* qore_parse_try_find_hashdecl(const NamedScope& nscope) {
    return qore_root_ns_private::get(*getRootNS())->parseTryFindHashDecl(nscope);
}

static const TypedHashDecl* qore_resolve_parse_hashdecl_type(const QoreParseTypeInfo& pti,
        const QoreProgramLocation* loc, int& err) {
    const TypedHashDecl* hd = qore_parse_try_find_hashdecl(*pti.cscope);
    if (!hd) {
        return nullptr;
    }

    const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
    if (!pti.hasExplicitSubtypeList()) {
        if (hp->hasTypeParams()) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; generic hashdecl '%s' declares %d "
                "type parameter%s and must be used with explicit type arguments", QoreParseTypeInfo::getName(&pti),
                pti.cscope->ostr, (int)hp->getTypeParamCount(), qore_type_arg_plural(hp->getTypeParamCount()));
            err = -1;
            return hd;
        }
        return hd;
    }

    if (!hp->hasTypeParams()) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; hashdecl '%s' does not declare type "
            "parameters, so it cannot be used with %d type argument%s", QoreParseTypeInfo::getName(&pti),
            pti.cscope->ostr, (int)pti.subtypes.size(), qore_type_arg_plural(pti.subtypes.size()));
        err = -1;
        return hd;
    }

    size_t expected = hp->getTypeParamCount();
    size_t required = hp->getTypeParamRequiredCount();
    size_t actual = pti.subtypes.size();
    if (actual < required || actual > expected) {
        const char* missing = actual < required ? hp->getTypeParamName(actual) : nullptr;
        qore_raise_type_arg_count_error(loc, QoreParseTypeInfo::getName(&pti), "hashdecl", pti.cscope->ostr,
            expected, required, actual, missing, err);
        return hd;
    }

    type_vec_t args;
    args.reserve(expected);
    for (const auto& st : pti.subtypes) {
        const QoreTypeInfo* arg = QoreParseTypeInfo::resolveAny(st, loc, err);
        if (err) {
            return hd;
        }
        args.push_back(arg);
    }
    for (size_t i = actual; i < expected; ++i) {
        const char* default_type = hp->getTypeParamDefaultType(i);
        assert(default_type);
        args.push_back(qore_resolve_parse_default_type_arg(default_type, loc, err));
        if (err) {
            return hd;
        }
    }

    for (size_t i = 0; i < expected; ++i) {
        if (qore_check_parse_type_arg_bound(loc, QoreParseTypeInfo::getName(&pti), "hashdecl", pti.cscope->ostr,
                hp->getTypeParamName(i), hp->getTypeParamBoundType(i), args[i])) {
            err = -1;
            return hd;
        }
    }

    const TypedHashDecl* parameterized_hd = hp->getParameterizedHashDecl(args);
    return parameterized_hd ? parameterized_hd : hd;
}

static const QoreTypeInfo* qore_resolve_parse_parameterized_class_type(const QoreParseTypeInfo& pti,
        const QoreProgramLocation* loc, int& err, bool or_nothing) {
    const QoreClass* qc = qore_root_ns_private::parseFindScopedClass(loc, *pti.cscope, false);
    if (!qc) {
        return nullptr;
    }

    if (!qc->hasTypeParameters()) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; class '%s' does not declare type "
            "parameters, so it cannot be used with %d type argument%s", QoreParseTypeInfo::getName(&pti),
            pti.cscope->ostr, (int)pti.subtypes.size(), qore_type_arg_plural(pti.subtypes.size()));
        err = -1;
        return autoTypeInfo;
    }

    size_t expected = qc->getTypeParameterCount();
    size_t required = qc->getTypeParameterRequiredCount();
    size_t actual = pti.subtypes.size();
    if (actual < required || actual > expected) {
        const char* missing = actual < required ? qc->getTypeParameterName(actual) : nullptr;
        qore_raise_type_arg_count_error(loc, QoreParseTypeInfo::getName(&pti), "class", pti.cscope->ostr,
            expected, required, actual, missing, err);
        return autoTypeInfo;
    }

    type_vec_t args;
    args.reserve(expected);
    for (const auto& st : pti.subtypes) {
        const QoreTypeInfo* arg = QoreParseTypeInfo::resolveAny(st, loc, err);
        if (err) {
            return autoTypeInfo;
        }
        args.push_back(arg);
    }
    for (size_t i = actual; i < expected; ++i) {
        const char* default_type = qc->getTypeParameterDefaultType(i);
        assert(default_type);
        args.push_back(qore_resolve_parse_default_type_arg(default_type, loc, err));
        if (err) {
            return autoTypeInfo;
        }
    }

    for (size_t i = 0; i < expected; ++i) {
        if (qore_check_parse_type_arg_bound(loc, QoreParseTypeInfo::getName(&pti), "class", pti.cscope->ostr,
                qc->getTypeParameterName(i), qc->getTypeParameterBoundType(i), args[i])) {
            err = -1;
            return autoTypeInfo;
        }
    }

    return qc->getTypeInfo(args, or_nothing);
}

static const QoreTypeInfo* qore_resolve_runtime_parameterized_class_type(const QoreParseTypeInfo& pti,
        bool or_nothing) {
    const QoreClass* qc = qore_root_ns_private::get(*getRootNS())->runtimeFindScopedClass(*pti.cscope);
    if (!qc || !qc->hasTypeParameters()) {
        return nullptr;
    }

    size_t expected = qc->getTypeParameterCount();
    size_t actual = pti.subtypes.size();
    if (actual < qc->getTypeParameterRequiredCount() || actual > expected) {
        return nullptr;
    }

    type_vec_t args;
    args.reserve(expected);
    for (const auto& st : pti.subtypes) {
        const QoreTypeInfo* arg = QoreParseTypeInfo::resolveRuntime(st);
        if (!arg) {
            return nullptr;
        }
        args.push_back(arg);
    }
    for (size_t i = actual; i < expected; ++i) {
        const QoreTypeInfo* arg = qore_resolve_runtime_default_type_arg(qc->getTypeParameterDefaultType(i));
        if (!arg) {
            return nullptr;
        }
        args.push_back(arg);
    }

    for (size_t i = 0; i < expected; ++i) {
        if (!qore_runtime_type_arg_satisfies_bound(qc->getTypeParameterBoundType(i), args[i])) {
            return nullptr;
        }
    }

    return qc->getTypeInfo(args, or_nothing);
}

static const TypedHashDecl* qore_resolve_runtime_hashdecl_type(const QoreParseTypeInfo& pti) {
    const qore_ns_private* ns;
    const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->runtimeFindHashDeclIntern(*pti.cscope, ns);
    if (!hd) {
        return nullptr;
    }

    const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
    if (!pti.hasExplicitSubtypeList()) {
        return hp->hasTypeParams() ? nullptr : hd;
    }

    if (!hp->hasTypeParams()) {
        return nullptr;
    }

    size_t expected = hp->getTypeParamCount();
    size_t actual = pti.subtypes.size();
    if (actual < hp->getTypeParamRequiredCount() || actual > expected) {
        return nullptr;
    }

    type_vec_t args;
    args.reserve(expected);
    for (const auto& st : pti.subtypes) {
        const QoreTypeInfo* arg = QoreParseTypeInfo::resolveRuntime(st);
        if (!arg) {
            return nullptr;
        }
        args.push_back(arg);
    }
    for (size_t i = actual; i < expected; ++i) {
        const QoreTypeInfo* arg = qore_resolve_runtime_default_type_arg(hp->getTypeParamDefaultType(i));
        if (!arg) {
            return nullptr;
        }
        args.push_back(arg);
    }

    for (size_t i = 0; i < expected; ++i) {
        if (!qore_runtime_type_arg_satisfies_bound(hp->getTypeParamBoundType(i), args[i])) {
            return nullptr;
        }
    }

    return hp->getParameterizedHashDecl(args);
}

static const QoreTypeInfo* qore_resolve_parse_wildcard_type_arg(const QoreParseTypeInfo& pti,
        const QoreProgramLocation* loc, int& err) {
    if (!pti.isWildcardTypeArg()) {
        return nullptr;
    }
    if (pti.wildcard_kind == QoreWildcardKind::Unbounded) {
        return qore_get_wildcard_type();
    }

    const QoreTypeInfo* bound = QoreParseTypeInfo::resolveAny(pti.wildcard_bound, loc, err);
    if (err) {
        return autoTypeInfo;
    }
    if (QoreTypeInfo::getWildcardType(bound)) {
        parseException(*loc, "PARSE-TYPE-ERROR", "wildcard type argument '%s' cannot use another wildcard as its "
            "bound", QoreParseTypeInfo::getName(&pti));
        err = -1;
        return autoTypeInfo;
    }
    return pti.wildcard_kind == QoreWildcardKind::Extends
        ? qore_get_wildcard_extends_type(bound)
        : qore_get_wildcard_super_type(bound);
}

static const QoreTypeInfo* qore_resolve_runtime_wildcard_type_arg(const QoreParseTypeInfo& pti) {
    if (!pti.isWildcardTypeArg()) {
        return nullptr;
    }
    if (pti.wildcard_kind == QoreWildcardKind::Unbounded) {
        return qore_get_wildcard_type();
    }
    const QoreTypeInfo* bound = QoreParseTypeInfo::resolveRuntime(pti.wildcard_bound);
    if (!bound || QoreTypeInfo::getWildcardType(bound)) {
        return nullptr;
    }
    return pti.wildcard_kind == QoreWildcardKind::Extends
        ? qore_get_wildcard_extends_type(bound)
        : qore_get_wildcard_super_type(bound);
}

static constexpr const char* qore_buffer_supported_element_types =
    "int8, int16, int32, int64, float32, float64, bool, string";

static const QoreTypeInfo* qore_resolve_runtime_buffer_type(const QoreParseTypeInfo& pti, bool or_nothing) {
    if (pti.subtypes.size() != 1 || pti.subtypes[0]->hasExplicitSubtypeList()) {
        return nullptr;
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (!qore_buffer_element_type_from_name(pti.subtypes[0]->cscope->ostr, element_type)) {
        return nullptr;
    }

    return or_nothing
        ? qore_get_complex_buffer_or_nothing_type(element_type, pti.subtypes[0]->or_nothing)
        : qore_get_complex_buffer_type(element_type, pti.subtypes[0]->or_nothing);
}

static const QoreTypeInfo* qore_resolve_parse_buffer_type(const QoreParseTypeInfo& pti,
        const QoreProgramLocation* loc, int& err) {
    if (pti.subtypes.size() != 1) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s' with %d type arguments; base type 'buffer' "
            "takes exactly one storage element type: %s. Use 'buffer<*T>' for nullable elements and '*buffer<T>' "
            "for an optional buffer value", QoreParseTypeInfo::getName(&pti), (int)pti.subtypes.size(),
            qore_buffer_supported_element_types);
        err = -1;
        return pti.or_nothing ? bufferOrNothingTypeInfo : bufferTypeInfo;
    }

    const QoreParseTypeInfo* element = pti.subtypes[0];
    if (element->hasExplicitSubtypeList()) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; buffer<T> element type cannot itself have "
            "type arguments; supported storage element types are: %s", QoreParseTypeInfo::getName(&pti),
            qore_buffer_supported_element_types);
        err = -1;
        return pti.or_nothing ? bufferOrNothingTypeInfo : bufferTypeInfo;
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    if (!qore_buffer_element_type_from_name(element->cscope->ostr, element_type)) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; invalid buffer element type '%s'. "
            "Supported storage element types are: %s. Use 'buffer<*T>' for nullable elements and '*buffer<T>' for "
            "an optional buffer value", QoreParseTypeInfo::getName(&pti), element->cscope->ostr,
            qore_buffer_supported_element_types);
        err = -1;
        return pti.or_nothing ? bufferOrNothingTypeInfo : bufferTypeInfo;
    }

    return pti.or_nothing
        ? qore_get_complex_buffer_or_nothing_type(element_type, element->or_nothing)
        : qore_get_complex_buffer_type(element_type, element->or_nothing);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveRuntime() const {
    if (isWildcardTypeArg()) {
        return qore_resolve_runtime_wildcard_type_arg(*this);
    }
    if (hasExplicitSubtypeList())
        return resolveRuntimeSubtype();

    const QoreTypeInfo* rv = or_nothing ? getBuiltinUserOrNothingTypeInfo(cscope->ostr) : getBuiltinUserTypeInfo(cscope->ostr);
    return rv ? rv : resolveRuntimeClass(*cscope, or_nothing);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveRuntimeSubtype() const {
    if (!strcmp(cscope->ostr, "buffer")) {
        return qore_resolve_runtime_buffer_type(*this, or_nothing);
    }
    if (!strcmp(cscope->ostr, "hash")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
            // resolve hashdecl
            const TypedHashDecl* hd = qore_resolve_runtime_hashdecl_type(*subtypes[0]);
            if (!hd)
                return nullptr;
            return hd->getTypeInfo(or_nothing);
        }
        if (subtypes.size() == 2) {
            if (strcmp(subtypes[0]->cscope->ostr, "string")) {
                return nullptr;
            } else {
                if (!strcmp(subtypes[1]->cscope->ostr, "auto"))
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            if (!strcmp(subtypes[1]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;

                // resolve value type
                const QoreTypeInfo* valueType = subtypes[1]->resolveRuntime();
                if (!valueType)
                return nullptr;
                if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                    ? qore_get_complex_hash_type(valueType)
                    : qore_get_complex_hash_or_nothing_type(valueType);
                }
            }
        } else {
            return nullptr;
        }
        return or_nothing ? hashOrNothingTypeInfo : hashTypeInfo;
    }
    if (!strcmp(cscope->ostr, "list")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoListOrNothingTypeInfo : autoListTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowListOrNothingTypeInfo : autoNoNarrowListTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = subtypes[0]->resolveRuntime();
            if (!valueType)
                return nullptr;
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                ? qore_get_complex_list_type(valueType)
                : qore_get_complex_list_or_nothing_type(valueType);
            }
        } else {
            return nullptr;
        }
        return or_nothing ? listOrNothingTypeInfo : listTypeInfo;
    }
    if (!strcmp(cscope->ostr, "softlist")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? softAutoListOrNothingTypeInfo : softAutoListTypeInfo;
            // softlist<auto!> - treated same as softlist<auto> since softlist already implies flexible typing
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? softAutoListOrNothingTypeInfo : softAutoListTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = subtypes[0]->resolveRuntime();
            if (!valueType)
                return nullptr;
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                    ? qore_get_complex_softlist_type(valueType)
                    : qore_get_complex_softlist_or_nothing_type(valueType);
            }
        } else {
            return nullptr;
        }
        return or_nothing ? softListOrNothingTypeInfo : softListTypeInfo;
    }
    if (!strcmp(cscope->ostr, "reference")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
            // reference<auto!> - treated same as reference<auto> since reference doesn't narrow
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = subtypes[0]->resolveRuntime();
            if (!valueType)
                return nullptr;
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                ? qore_get_complex_reference_type(valueType)
                : qore_get_complex_reference_or_nothing_type(valueType);
            }
        } else {
            return nullptr;
        }
        return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
    }
    if (!strcmp(cscope->ostr, "date")) {
        if (subtypes.size() != 1) {
            return nullptr;
        }

        if (!strcmp(subtypes[0]->cscope->ostr, "absolute")) {
            return or_nothing ? dateAbsoluteOrNothingTypeInfo : dateAbsoluteTypeInfo;
        }
        if (!strcmp(subtypes[0]->cscope->ostr, "relative")) {
            return or_nothing ? dateRelativeOrNothingTypeInfo : dateRelativeTypeInfo;
        }
        return nullptr;
    }

    if (!strcmp(cscope->ostr, "object")) {
        if (subtypes.size() != 1) {
            return nullptr;
        }

        if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
            return or_nothing ? objectOrNothingTypeInfo : objectTypeInfo;

        if (subtypes[0]->hasExplicitSubtypeList()) {
            return qore_resolve_runtime_parameterized_class_type(*subtypes[0], or_nothing);
        }

        // resolve class
        return resolveRuntimeClass(*subtypes[0]->cscope, or_nothing);
    }
    if (!strcmp(cscope->ostr, "enum")) {
        if (subtypes.size() != 1) {
            return nullptr;
        }

        // resolve enum
        const qore_ns_private* ns;
        const QoreEnumDecl* ed = qore_root_ns_private::get(*getRootNS())->runtimeFindEnumIntern(*subtypes[0]->cscope, ns);
        if (!ed) {
            return nullptr;
        }
        return ed->getTypeInfo(or_nothing);
    }
    if (!strcmp(cscope->ostr, "union")) {
        if (subtypes.empty() || subtypes.size() > QORE_MAX_UNION_MEMBERS) {
            return nullptr;
        }

        // Resolve all member types
        type_vec_t member_types;
        member_types.reserve(subtypes.size());

        for (const auto& st : subtypes) {
            if (!strcmp(st->cscope->ostr, "auto")) {
                return autoTypeInfo;  // union<auto> = auto
            }

            const QoreTypeInfo* member = st->resolveRuntime();
            if (!member) {
                return nullptr;
            }
            member_types.push_back(member);
        }

        return qore_get_union_type(member_types, or_nothing);
    }
    if (!strcmp(cscope->ostr, "code")) {
        // Parse typed callable: code<ReturnType(ParamType1, ParamType2, ...)>
        // Expected format: subtypes[0] contains "ReturnType(ParamType1, ParamType2, ...)"
        // NOTE: Similar parsing logic exists in resolveSubtype() for parse-time resolution with error messages
        if (subtypes.size() != 1) {
            return nullptr;
        }

        const char* sig_str = subtypes[0]->cscope->ostr;

        // Find the opening parenthesis to split return type from params
        // Handle complex return types like hash<string, int> by tracking angle bracket depth
        const char* paren_open = nullptr;
        int angle_depth = 0;
        for (const char* p = sig_str; *p; ++p) {
            if (*p == '<') {
                ++angle_depth;
            } else if (*p == '>') {
                --angle_depth;
            } else if (*p == '(' && angle_depth == 0) {
                paren_open = p;
                break;
            }
        }
        if (!paren_open) {
            return nullptr;
        }

        // Find closing parenthesis
        const char* paren_close = strrchr(sig_str, ')');
        if (!paren_close || paren_close < paren_open) {
            return nullptr;
        }

        // Extract return type string
        std::string return_type_str(sig_str, paren_open - sig_str);
        // Trim whitespace
        while (!return_type_str.empty() && isspace(return_type_str.back())) {
            return_type_str.pop_back();
        }
        while (!return_type_str.empty() && isspace(return_type_str.front())) {
            return_type_str.erase(0, 1);
        }
        // If the subtype was flagged as or-nothing (a leading `*` on the
        // callable's return type — e.g. `code<*Foo(string)>`), getSubTypes
        // consumed the `*` into subtypes[0]->or_nothing instead of leaving
        // it on the ostr string.  The `*` applies to the callable's return
        // type, not to the whole callable, so re-prepend it here before
        // resolving so the return type is resolved as or-nothing.
        if (subtypes[0]->or_nothing && !return_type_str.empty()
                && return_type_str != "nothing"
                && return_type_str[0] != '*') {
            return_type_str.insert(0, "*");
        }

        // Resolve return type
        const QoreTypeInfo* returnType = nullptr;
        if (!return_type_str.empty() && return_type_str != "nothing") {
            returnType = qore_get_type_from_string_intern(return_type_str.c_str());
            if (!returnType) {
                return nullptr;
            }
        }
        // If empty or "nothing", returnType stays nullptr (means nothing return)

        // Extract parameter types string
        std::string params_str(paren_open + 1, paren_close - paren_open - 1);

        // Check for varargs
        bool has_varargs = false;
        size_t dotdotdot_pos = params_str.rfind("...");
        if (dotdotdot_pos != std::string::npos) {
            has_varargs = true;
            // Remove "..." from params_str
            params_str = params_str.substr(0, dotdotdot_pos);
            // Trim trailing comma and whitespace
            while (!params_str.empty() && (isspace(params_str.back()) || params_str.back() == ',')) {
                params_str.pop_back();
            }
        }

        // Parse parameter types
        type_vec_t param_types;
        if (!params_str.empty()) {
            // Split on commas, handling nested angle brackets and parentheses
            std::string current_param;
            int param_angle_depth = 0;
            int param_paren_depth = 0;
            for (size_t i = 0; i <= params_str.size(); ++i) {
                char c = i < params_str.size() ? params_str[i] : '\0';
                if (c == '<') {
                    ++param_angle_depth;
                    current_param += c;
                } else if (c == '>') {
                    --param_angle_depth;
                    current_param += c;
                } else if (c == '(') {
                    ++param_paren_depth;
                    current_param += c;
                } else if (c == ')') {
                    --param_paren_depth;
                    current_param += c;
                } else if ((c == ',' || c == '\0') && param_angle_depth == 0 && param_paren_depth == 0) {
                    // Trim whitespace from current_param
                    while (!current_param.empty() && isspace(current_param.back())) {
                        current_param.pop_back();
                    }
                    while (!current_param.empty() && isspace(current_param.front())) {
                        current_param.erase(0, 1);
                    }
                    if (!current_param.empty()) {
                        const QoreTypeInfo* param_type = qore_get_type_from_string_intern(current_param.c_str());
                        if (!param_type) {
                            return nullptr;
                        }
                        param_types.push_back(param_type);
                    }
                    current_param.clear();
                } else {
                    current_param += c;
                }
            }
        }

        return qore_get_complex_code_type(returnType, param_types, has_varargs, or_nothing);
    }
    const QoreTypeInfo* rv = qore_resolve_runtime_parameterized_class_type(*this, or_nothing);
    if (rv) {
        return rv;
    }
    const TypedHashDecl* hd = qore_resolve_runtime_hashdecl_type(*this);
    if (hd) {
        return hd->getTypeInfo(or_nothing);
    }
    return nullptr;
}

const QoreTypeInfo* QoreParseTypeInfo::resolveRuntimeClass(const NamedScope& cscope, bool or_nothing) {
    // resolve class
    const QoreClass* qc = qore_root_ns_private::get(*getRootNS())->runtimeFindScopedClass(cscope);
    if (qc)
        return or_nothing ? qc->getOrNothingTypeInfo() : qc->getTypeInfo();

    // check for hashdecl (must be checked after class lookup)
    const qore_ns_private* ns;
    const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->runtimeFindHashDeclIntern(cscope, ns);
    if (hd) {
        if (typed_hash_decl_private::get(*hd)->hasTypeParams()) {
            return nullptr;
        }
        return hd->getTypeInfo(or_nothing);
    }

    return nullptr;
}

const QoreTypeInfo* QoreParseTypeInfo::resolveSubtype(const QoreProgramLocation* loc, int& err) const {
    if (isWildcardTypeArg()) {
        return qore_resolve_parse_wildcard_type_arg(*this, loc, err);
    }
    if (!strcmp(cscope->ostr, "buffer")) {
        return qore_resolve_parse_buffer_type(*this, loc, err);
    }
    if (!strcmp(cscope->ostr, "hash")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
            // resolve hashdecl
            const TypedHashDecl* hd = qore_resolve_parse_hashdecl_type(*subtypes[0], loc, err);
            if (hd) {
                return hd->getTypeInfo(or_nothing);
            }
            // Unknown hashdecl - raise parse error
            parseException(*loc, "PARSE-EXCEPTION", "illegal access to unknown member in undefined hashdecl '%s'",
                subtypes[0]->cscope->ostr);
            err = -1;
            return hashTypeInfo;
        }
        if (subtypes.size() == 2) {
            if (strcmp(subtypes[0]->cscope->ostr, "string")) {
                parseException(*loc, "PARSE-TYPE-ERROR", "invalid complex hash type '%s'; hash key type must be " \
                    "'string'; cannot declare a hash with key type '%s'", getName(), subtypes[0]->cscope->ostr);
                err = -1;
            } else {
                if (!strcmp(subtypes[1]->cscope->ostr, "auto"))
                    return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
                if (!strcmp(subtypes[1]->cscope->ostr, "auto!"))
                    return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;

                // resolve value type
                const QoreTypeInfo* valueType = QoreParseTypeInfo::resolveAny(subtypes[1], loc, err);
                if (QoreTypeInfo::hasType(valueType)) {
                    return !or_nothing
                        ? qore_get_complex_hash_type(valueType)
                        : qore_get_complex_hash_or_nothing_type(valueType);
                }
            }
        } else {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s' with %d type arguments; base type 'hash' " \
                "takes a single hashdecl name as a subtype argument or two type names giving the key and value types",
                getName(), (int)subtypes.size());
            err = -1;
        }
        return or_nothing ? hashOrNothingTypeInfo : hashTypeInfo;
    }
    if (!strcmp(cscope->ostr, "list")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoListOrNothingTypeInfo : autoListTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowListOrNothingTypeInfo : autoNoNarrowListTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = QoreParseTypeInfo::resolveAny(subtypes[0], loc, err);
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                    ? qore_get_complex_list_type(valueType)
                    : qore_get_complex_list_or_nothing_type(valueType);
            }
        } else {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s' with %d type arguments; base type 'list' " \
                "takes a single type name giving list element value type", getName(), (int)subtypes.size());
            err = -1;
        }
        return or_nothing ? listOrNothingTypeInfo : listTypeInfo;
    }
    if (!strcmp(cscope->ostr, "softlist")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? softAutoListOrNothingTypeInfo : softAutoListTypeInfo;
            // softlist<auto!> - treated same as softlist<auto> since softlist already implies flexible typing
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? softAutoListOrNothingTypeInfo : softAutoListTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = QoreParseTypeInfo::resolveAny(subtypes[0], loc, err);
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                    ? qore_get_complex_softlist_type(valueType)
                    : qore_get_complex_softlist_or_nothing_type(valueType);
            }
        } else {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s' with %d type arguments; base type " \
                "'softlist' takes a single type name giving list element value type", getName(),
                (int)subtypes.size());
            err = -1;
        }
        return or_nothing ? softListOrNothingTypeInfo : softListTypeInfo;
    }
    if (!strcmp(cscope->ostr, "reference")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
            // reference<auto!> - treated same as reference<auto> since reference doesn't narrow
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
            // resolve value type
            const QoreTypeInfo* valueType = QoreParseTypeInfo::resolveAny(subtypes[0], loc, err);
            if (QoreTypeInfo::hasType(valueType)) {
                return !or_nothing
                    ? qore_get_complex_reference_type(valueType)
                    : qore_get_complex_reference_or_nothing_type(valueType);
            }
        } else {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s' with %d type arguments; base type " \
                "'reference' takes a single type name giving referenced lvalue type", getName(),
                (int)subtypes.size());
            err = -1;
        }
        return or_nothing ? referenceOrNothingTypeInfo : referenceTypeInfo;
    }
    if (!strcmp(cscope->ostr, "date")) {
        if (subtypes.size() != 1) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; base type 'date' takes a single subtype " \
                "argument of 'absolute' or 'relative'", getName());
            err = -1;
            return or_nothing ? dateOrNothingTypeInfo : dateTypeInfo;
        }

        if (!strcmp(subtypes[0]->cscope->ostr, "absolute")) {
            return or_nothing ? dateAbsoluteOrNothingTypeInfo : dateAbsoluteTypeInfo;
        }
        if (!strcmp(subtypes[0]->cscope->ostr, "relative")) {
            return or_nothing ? dateRelativeOrNothingTypeInfo : dateRelativeTypeInfo;
        }

        parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; base type 'date' subtype must be " \
            "'absolute' or 'relative'", getName());
        err = -1;
        return or_nothing ? dateOrNothingTypeInfo : dateTypeInfo;
    }
    if (!strcmp(cscope->ostr, "object")) {
        if (subtypes.size() != 1) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; base type 'object' takes a single class " \
                "name as a subtype argument", getName());
            err = -1;
            return or_nothing ? objectOrNothingTypeInfo : objectTypeInfo;
        }

        if (!strcmp(subtypes[0]->cscope->ostr, "auto")) {
            return or_nothing ? objectOrNothingTypeInfo : objectTypeInfo;
        }

        if (subtypes[0]->hasExplicitSubtypeList()) {
            const QoreTypeInfo* rv = qore_resolve_parse_parameterized_class_type(*subtypes[0], loc, err,
                or_nothing);
            if (rv || err) {
                return rv ? rv : autoTypeInfo;
            }
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; object subtype '%s' must name a class",
                getName(), subtypes[0]->getName());
            err = -1;
            return or_nothing ? objectOrNothingTypeInfo : objectTypeInfo;
        }

        // resolve class
        return resolveClass(loc, *subtypes[0]->cscope, or_nothing, err);
    }
    if (!strcmp(cscope->ostr, "enum")) {
        if (subtypes.size() != 1) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; base type 'enum' takes a single enum " \
                "name as a subtype argument", getName());
            err = -1;
            return or_nothing ? bigIntOrNothingTypeInfo : bigIntTypeInfo;
        }

        // resolve enum
        const QoreEnumDecl* ed = qore_root_ns_private::get(*getRootNS())->parseFindEnum(loc, *subtypes[0]->cscope);
        if (!ed) {
            return or_nothing ? bigIntOrNothingTypeInfo : bigIntTypeInfo;
        }
        return ed->getTypeInfo(or_nothing);
    }
    if (!strcmp(cscope->ostr, "union")) {
        // Validate union constraints
        if (subtypes.empty()) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; union type requires at least one member " \
                "type", getName());
            err = -1;
            return autoTypeInfo;  // return auto on error
        }
        if (subtypes.size() > QORE_MAX_UNION_MEMBERS) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; union type has %d member types but " \
                "maximum allowed is %d", getName(), (int)subtypes.size(), (int)QORE_MAX_UNION_MEMBERS);
            err = -1;
            return autoTypeInfo;  // return auto on error
        }

        // Resolve all member types
        type_vec_t member_types;
        member_types.reserve(subtypes.size());

        // Track soft types for validation
        const QoreTypeInfo* soft_type = nullptr;
        std::set<qore_type_t> base_types;

        for (const auto& st : subtypes) {
            if (!strcmp(st->cscope->ostr, "auto")) {
                // union<auto> is equivalent to auto
                return autoTypeInfo;
            }

            const QoreTypeInfo* member = QoreParseTypeInfo::resolveAny(st, loc, err);
            if (err) {
                return autoTypeInfo;  // return auto on error
            }

            // Check for soft type constraints
            qore_type_t base_type = QoreTypeInfo::getBaseType(member);
            bool is_soft = QoreTypeInfo::getName(member)[0] == 's' &&
                           (strncmp(QoreTypeInfo::getName(member), "soft", 4) == 0);

            if (is_soft) {
                if (soft_type && soft_type != member) {
                    parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; union type can have at most one " \
                        "soft type but found '%s' and '%s'", getName(), QoreTypeInfo::getName(soft_type),
                        QoreTypeInfo::getName(member));
                    err = -1;
                    return autoTypeInfo;  // return auto on error
                }
                soft_type = member;

                // Check for soft+non-soft of same base type
                if (base_types.find(base_type) != base_types.end()) {
                    parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; union type cannot have both soft " \
                        "and non-soft versions of the same base type", getName());
                    err = -1;
                    return autoTypeInfo;  // return auto on error
                }
            } else {
                // Check if we already have a soft version of this base type
                if (soft_type && QoreTypeInfo::getBaseType(soft_type) == base_type) {
                    parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; union type cannot have both soft " \
                        "and non-soft versions of the same base type", getName());
                    err = -1;
                    return autoTypeInfo;  // return auto on error
                }
                base_types.insert(base_type);
            }

            member_types.push_back(member);
        }

        return qore_get_union_type(member_types, or_nothing);
    }
    if (!strcmp(cscope->ostr, "code")) {
        // Parse typed callable: code<ReturnType(ParamType1, ParamType2, ...)>
        // Expected format: subtypes[0] contains "ReturnType(ParamType1, ParamType2, ...)"
        // NOTE: Similar parsing logic exists in resolveRuntimeSubtype() for runtime resolution without error messages
        if (subtypes.size() != 1) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; 'code' type takes a single callable " \
                "signature specification in the form 'ReturnType(ParamTypes...)'", getName());
            err = -1;
            return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
        }

        // Use getName() to get the full type string including angle brackets and parentheses
        const char* sig_str = subtypes[0]->getName();

        // Find the opening parenthesis to split return type from params
        const char* paren_open = strchr(sig_str, '(');
        if (!paren_open) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; invalid callable signature '%s'; " \
                "expected format 'ReturnType(ParamTypes...)' with parentheses", getName(), sig_str);
            err = -1;
            return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
        }

        // Find closing parenthesis
        const char* paren_close = strrchr(sig_str, ')');
        if (!paren_close || paren_close < paren_open) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; invalid callable signature '%s'; " \
                "missing closing parenthesis", getName(), sig_str);
            err = -1;
            return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
        }

        // Extract return type string
        std::string return_type_str(sig_str, paren_open - sig_str);
        // Trim whitespace
        while (!return_type_str.empty() && isspace(return_type_str.back())) {
            return_type_str.pop_back();
        }
        while (!return_type_str.empty() && isspace(return_type_str.front())) {
            return_type_str.erase(0, 1);
        }

        // Resolve return type
        const QoreTypeInfo* returnType = nullptr;
        if (!return_type_str.empty() && return_type_str != "nothing") {
            // Use the helper to properly parse and resolve complex types
            returnType = parse_and_resolve_type_string(return_type_str.c_str(), loc, err);
            if (err) {
                return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
            }
        }
        // If empty or "nothing", returnType stays nullptr (means nothing return)

        // Extract parameter types string
        std::string params_str(paren_open + 1, paren_close - paren_open - 1);

        // Check for varargs
        bool has_varargs = false;
        size_t dotdotdot_pos = params_str.rfind("...");
        if (dotdotdot_pos != std::string::npos) {
            has_varargs = true;
            // Remove "..." from params_str
            params_str = params_str.substr(0, dotdotdot_pos);
            // Trim trailing comma and whitespace
            while (!params_str.empty() && (isspace(params_str.back()) || params_str.back() == ',')) {
                params_str.pop_back();
            }
        }

        // Parse parameter types
        type_vec_t param_types;
        if (!params_str.empty()) {
            // Split on commas, handling nested angle brackets
            std::string current_param;
            int angle_depth = 0;
            int paren_depth = 0;
            for (size_t i = 0; i <= params_str.size(); ++i) {
                char c = i < params_str.size() ? params_str[i] : '\0';
                if (c == '<') {
                    ++angle_depth;
                    current_param += c;
                } else if (c == '>') {
                    --angle_depth;
                    current_param += c;
                } else if (c == '(') {
                    ++paren_depth;
                    current_param += c;
                } else if (c == ')') {
                    --paren_depth;
                    current_param += c;
                } else if ((c == ',' || c == '\0') && angle_depth == 0 && paren_depth == 0) {
                    // Trim whitespace from current_param
                    while (!current_param.empty() && isspace(current_param.back())) {
                        current_param.pop_back();
                    }
                    while (!current_param.empty() && isspace(current_param.front())) {
                        current_param.erase(0, 1);
                    }
                    if (!current_param.empty()) {
                        // Use the helper to properly parse and resolve complex types
                        const QoreTypeInfo* param_type = parse_and_resolve_type_string(current_param.c_str(), loc, err);
                        if (err) {
                            return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
                        }
                        param_types.push_back(param_type);
                    }
                    current_param.clear();
                } else {
                    current_param += c;
                }
            }
        }

        return qore_get_complex_code_type(returnType, param_types, has_varargs, or_nothing);
    }

    const QoreTypeInfo* rv = qore_resolve_parse_parameterized_class_type(*this, loc, err, or_nothing);
    if (rv || err) {
        return rv ? rv : autoTypeInfo;
    }

    const TypedHashDecl* hd = qore_resolve_parse_hashdecl_type(*this, loc, err);
    if (hd || err) {
        return hd ? hd->getTypeInfo(or_nothing) : autoTypeInfo;
    }

    parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; type '%s' does not take subtype declarations",
        getName(), cscope->getIdentifier());
    err = -1;
    return autoTypeInfo;
}

const QoreTypeInfo* QoreParseTypeInfo::resolve(const QoreProgramLocation* loc, int& err) const {
    if (isWildcardTypeArg()) {
        return qore_resolve_parse_wildcard_type_arg(*this, loc, err);
    }
    if (hasExplicitSubtypeList()) {
        return resolveSubtype(loc, err);
    }

    return resolveClass(loc, *cscope, or_nothing, err);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveAny(const QoreProgramLocation* loc, int& err) const {
    if (isWildcardTypeArg()) {
        return qore_resolve_parse_wildcard_type_arg(*this, loc, err);
    }
    if (hasExplicitSubtypeList()) {
        return resolveSubtype(loc, err);
    }

    const QoreTypeInfo* type_param = qore_resolve_parse_type_parameter(*cscope, or_nothing);
    if (type_param) {
        return type_param;
    }

    const QoreTypeInfo* rv = or_nothing
        ? getBuiltinUserOrNothingTypeInfo(cscope->ostr)
        : getBuiltinUserTypeInfo(cscope->ostr);
    return rv ? rv : resolveClass(loc, *cscope, or_nothing, err);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveAndDelete(const QoreProgramLocation* loc, int& err) {
    std::unique_ptr<QoreParseTypeInfo> holder(this);
    return resolve(loc, err);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveClass(const QoreProgramLocation* loc, const NamedScope& cscope,
        bool or_nothing, int& err) {
    const QoreTypeInfo* type_param = qore_resolve_parse_type_parameter(cscope, or_nothing);
    if (type_param) {
        return type_param;
    }

    // check for typedef first (both simple names and scoped names)
    TypedefEntry* td = qore_root_ns_private::parseFindTypedef(cscope);
    if (td) {
        // resolve the typedef's underlying type
        const QoreTypeInfo* rv;
        if (td->typeInfo) {
            // already resolved
            rv = td->typeInfo;
        } else if (td->parseTypeInfo) {
            // resolve the parse type info
            rv = td->parseTypeInfo->resolve(loc, err);
            // cache the resolved type for future lookups
            td->typeInfo = rv;
        } else {
            // should not happen - typedef without type info
            parse_error(*loc, "internal error: typedef '%s' has no type information", cscope.ostr);
            err = -1;
            return autoTypeInfo;
        }

        // apply or_nothing if needed and the underlying type doesn't already accept NOTHING
        if (or_nothing && !QoreTypeInfo::parseAcceptsReturns(rv, NT_NOTHING)) {
            // need to get the or-nothing variant of this type
            const QoreTypeInfo* orn = qore_get_or_nothing_type(rv);
            if (orn) {
                return orn;
            }
            // if we can't get or-nothing variant, just return the base type
            // the caller should handle the or_nothing semantics
        }
        return rv;
    }

    // resolve class (don't raise error - we'll check for enum as fallback)
    const QoreClass* qc = qore_root_ns_private::parseFindScopedClass(loc, cscope, false);

    if (qc && or_nothing) {
        const QoreTypeInfo* rv = qc->getOrNothingTypeInfo();
        if (!rv) {
            parse_error(*loc, "class %s cannot be typed with '*' as the class's type handler has an input filter " \
                "and the filter does not accept NOTHING", qc->getName());
            err = -1;
            return objectOrNothingTypeInfo;
        }
        return rv;
    }

    if (qc) {
        return qc->getTypeInfo();
    }

    // if not a class, check for enum (use internal lookup to avoid parse error)
    const QoreEnumDecl* ed = qore_root_ns_private::get(*getRootNS())->parseTryFindEnum(cscope);
    if (ed) {
        return ed->getTypeInfo(or_nothing);
    }

    // check for hashdecl (must be checked after class/enum lookup)
    const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->parseFindHashDecl(loc, cscope);
    if (hd) {
        if (typed_hash_decl_private::get(*hd)->hasTypeParams()) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve generic hashdecl '%s' without explicit type "
                "arguments", cscope.ostr);
            err = -1;
            return autoTypeInfo;
        }
        return hd->getTypeInfo(or_nothing);
    }

    // type not found - raise error
    parse_error(*loc, "reference to undefined type '%s'", cscope.ostr);
    err = -1;
    return objectTypeInfo;
}

QoreValue QoreHashDeclTypeInfo::getDefaultQoreValueImpl() const {
    return qore_hash_private::newHashDecl(getFirstAcceptSpec().spec.getHashDecl());
    //return new QoreHashNode(getFirstAcceptSpec().spec.getHashDecl(), xsink);
}

QoreComplexBufferTypeInfo::QoreComplexBufferTypeInfo(QoreBufferElementType element_type, bool nullable_elements)
        : QoreTypeInfo(q_accept_vec_t {{QoreComplexBufferTypeSpec(this), nullptr, true}},
            q_return_vec_t {{QoreComplexBufferTypeSpec(this), true}},
            qore_make_complex_buffer_name(element_type, nullable_elements, false)),
        element_type(element_type),
        nullable_elements(nullable_elements) {
    assert(element_type != QoreBufferElementType::Invalid);
    pname = qore_make_complex_buffer_name(element_type, nullable_elements, false);
}

QoreComplexBufferTypeInfo::QoreComplexBufferTypeInfo(const q_accept_vec_t&& a_vec, const q_return_vec_t&& r_vec,
        const QoreString& tname, QoreBufferElementType element_type, bool nullable_elements)
        : QoreTypeInfo(std::move(a_vec), std::move(r_vec), tname), element_type(element_type),
        nullable_elements(nullable_elements) {
    assert(element_type != QoreBufferElementType::Invalid);
}

const QoreTypeInfo* QoreComplexBufferTypeInfo::getElementTypeInfo() const {
    return qore_buffer_element_scalar_type_info(element_type, nullable_elements);
}

void QoreComplexBufferTypeInfo::getThisTypeImpl(QoreString& str) const {
    qore_string_private::get(str)->concat(&tname);
}

QoreValue QoreComplexBufferTypeInfo::getDefaultQoreValueImpl() const {
    return new QoreBufferNode(element_type, nullable_elements);
}

QoreComplexBufferOrNothingTypeInfo::QoreComplexBufferOrNothingTypeInfo(QoreBufferElementType element_type,
        bool nullable_elements, const QoreTypeInfo* value_type)
        : QoreComplexBufferTypeInfo(q_accept_vec_t {
            {QoreComplexBufferTypeSpec(value_type), nullptr},
            {NT_NOTHING, nullptr},
            {NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }},
        }, q_return_vec_t {{QoreComplexBufferTypeSpec(value_type)}, {NT_NOTHING}},
            qore_make_complex_buffer_name(element_type, nullable_elements, true), element_type, nullable_elements) {
    assert(value_type);
    pname = qore_make_complex_buffer_name(element_type, nullable_elements, true);
}

void QoreComplexBufferOrNothingTypeInfo::getThisTypeImpl(QoreString& str) const {
    str.sprintf("%s or no value (NOTHING)", QoreTypeInfo::getName(getFirstAcceptSpec().spec.getComplexBuffer()));
}

QoreComplexSoftListTypeInfo::QoreComplexSoftListTypeInfo(const QoreTypeInfo* vti) : QoreComplexListTypeInfo(q_accept_vec_t {
            {
                QoreComplexSoftListTypeSpec(vti),
                [vti] (QoreValue& n, ExceptionSink* xsink) {
                    if (n.getType() != NT_LIST || n.get<const QoreListNode>()->getValueTypeInfo() != vti) {
                        QoreValue val{};
                        n.swap(val);
                        n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val,
                            xsink));
                    }
                },
                true
            },
            {
                NT_LIST,
                [vti] (QoreValue& n, ExceptionSink* xsink) {
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink));
                }
            },
            {
                NT_NOTHING,
                [vti] (QoreValue& n, ExceptionSink* xsink) {
                    QoreListNode* l = new QoreListNode(vti);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {
            { QoreComplexSoftListTypeSpec(vti), true }
        }, QoreStringMaker("softlist<%s>", QoreTypeInfo::getName(vti))) {
    assert(vti);
    pname = QoreStringMaker("softlist<%s>", QoreTypeInfo::getPath(vti));
}

QoreComplexSoftListOrNothingTypeInfo::QoreComplexSoftListOrNothingTypeInfo(const QoreTypeInfo* vti)
    : QoreComplexListOrNothingTypeInfo(q_accept_vec_t {
            {
                QoreComplexSoftListTypeSpec(vti),
                [vti] (QoreValue& n, ExceptionSink* xsink) {
                    switch (n.getType()) {
                        case NT_NOTHING:
                            break;
                        case NT_NULL: {
                            QoreValue val{};
                            n.swap(val);
                            n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val,
                                xsink));
                            break;
                        }
                        default:
                           if (n.getType() != NT_LIST || n.get<const QoreListNode>()->getValueTypeInfo() != vti) {
                                QoreValue val{};
                                n.swap(val);
                                n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti),
                                    val, xsink));
                            }
                            break;
                    }
                },
                true
            },
            {
                NT_LIST,
                [vti] (QoreValue& n, ExceptionSink* xsink) {
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink));
                }
            },
            {
                NT_NOTHING,
                nullptr
            },
            {
                NT_NULL,
                [] (QoreValue& n, ExceptionSink* xsink) {
                    n.assignNothing();
                }
            },
        }, q_return_vec_t {{QoreComplexSoftListTypeSpec(vti)}, {NT_NOTHING}
        }, QoreStringMaker("*softlist<%s>", QoreTypeInfo::getName(vti))) {
    assert(vti);
    pname = QoreStringMaker("*softlist<%s>", QoreTypeInfo::getPath(vti));
}

void map_get_plain_hash_lvalue(QoreValue& n, ExceptionSink* xsink, LValueHelper* lvhelper) {
    // issue #2889 do not pass a QoreValue to LValueHelper::saveTemp() as it will remove the node from the QoreValue
    // instead pass an AbstractQoreNode*
    QoreHashNode* h = n.get<QoreHashNode>();
    lvhelper->saveTemp(h);
    n.assign(copy_strip_complex_types(h));
}

void map_get_plain_list_lvalue(QoreValue& n, ExceptionSink* xsink, LValueHelper* lvhelper) {
    // issue #2889 do not pass a QoreValue to LValueHelper::saveTemp() as it will remove the node from the QoreValue
    // instead pass an AbstractQoreNode*
    QoreListNode* l = n.get<QoreListNode>();
    lvhelper->saveTemp(l);
    n.assign(copy_strip_complex_types(l));
}

void map_get_plain_hash(QoreValue& n, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(n.get<QoreHashNode>(), xsink);
    n.assign(copy_strip_complex_types(*h));
}

void map_get_plain_list(QoreValue& n, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> l(n.get<QoreListNode>(), xsink);
    n.assign(copy_strip_complex_types(*l));
}

const QoreTypeInfo* QoreTypeInfo::getHashPairType(const QoreTypeInfo* valueType) {
    // For auto/auto!/any value types, return autoHashTypeInfo as the element type
    if (!valueType || is_auto_vti(valueType) || valueType == anyTypeInfo || !hasType(valueType)
            || !hashdeclKeyValueInfo) {
        return autoHashTypeInfo;
    }

    type_vec_t typeArgs;
    typeArgs.push_back(valueType);
    return hashdeclKeyValueInfo->getParameterizedHashDecl(typeArgs)->getTypeInfo();
}

static const QoreTypeInfo* get_known_iterator_element_type(const QoreClass* qc, const QoreTypeInfo* valueType) {
    if (!qc) {
        return nullptr;
    }

    const char* className = qc->getName();
    if (!strcmp(className, "HashPairIterator") || !strcmp(className, "HashPairReverseIterator")
            || !strcmp(className, "ObjectPairIterator") || !strcmp(className, "ObjectPairReverseIterator")) {
        return QoreTypeInfo::getHashPairType(valueType);
    }

    if (!strcmp(className, "HashKeyIterator") || !strcmp(className, "HashKeyReverseIterator")
            || !strcmp(className, "ObjectKeyIterator") || !strcmp(className, "ObjectKeyReverseIterator")) {
        return stringTypeInfo;
    }

    return nullptr;
}

const QoreTypeInfo* QoreTypeInfo::getIteratorElementType(const QoreClass* iteratorClass,
        const QoreTypeInfo* sourceTypeInfo) {
    if (!iteratorClass || !sourceTypeInfo) {
        return nullptr;
    }

    const char* className = iteratorClass->getName();

    // Handle object iterators. Object members do not have a single static value
    // type, but pair/key iterator element shapes are still known.
    if (!strcmp(className, "ObjectPairIterator") || !strcmp(className, "ObjectPairReverseIterator")) {
        return autoHashTypeInfo;
    }
    if (!strcmp(className, "ObjectKeyIterator") || !strcmp(className, "ObjectKeyReverseIterator")) {
        return stringTypeInfo;
    }
    if (!strcmp(className, "ObjectIterator") || !strcmp(className, "ObjectReverseIterator")) {
        return autoTypeInfo;
    }

    // Handle hash iterators
    const QoreTypeInfo* hashValueType = getUniqueReturnComplexHash(sourceTypeInfo);
    if (!hashValueType) {
        // Also try hash-or-nothing types
        hashValueType = getReturnComplexHashOrNothing(sourceTypeInfo);
    }
    if (!hashValueType && parseReturns(sourceTypeInfo, NT_HASH) != QTI_NOT_EQUAL) {
        hashValueType = autoTypeInfo;
    }

    if (hashValueType) {
        // HashPairIterator / HashPairReverseIterator: return hash with key: string, value: T
        if (!strcmp(className, "HashPairIterator") || !strcmp(className, "HashPairReverseIterator")) {
            return getHashPairType(hashValueType);
        }
        // HashKeyIterator / HashKeyReverseIterator: return string
        if (!strcmp(className, "HashKeyIterator") || !strcmp(className, "HashKeyReverseIterator")) {
            return stringTypeInfo;
        }
        // HashIterator / HashReverseIterator: return value type
        if (!strcmp(className, "HashIterator") || !strcmp(className, "HashReverseIterator")) {
            return hashValueType;
        }
        // HashListIterator / HashListReverseIterator: return row hashes
        if (!strcmp(className, "HashListIterator") || !strcmp(className, "HashListReverseIterator")) {
            return autoHashTypeInfo;
        }
        // AbstractIterator from `<hash>::iterator()` — the pseudo-method
        // declares AbstractIterator as its static return type but at
        // runtime creates a HashIterator (Pseudo_QC_Hash.qpp:303-305).
        // Surface the value type anyway so the common `map {...},
        // h.iterator()` pattern sees $1 typed correctly rather than
        // degrading through the abstract-base signature.
        if (!strcmp(className, "AbstractIterator")) {
            return hashValueType;
        }
    }

    // Handle list iterators
    const QoreTypeInfo* listElementType = getUniqueReturnComplexList(sourceTypeInfo);
    if (!listElementType) {
        // Also try list-or-nothing types
        listElementType = getReturnComplexListOrNothing(sourceTypeInfo);
    }

    if (listElementType) {
        // ListIterator / ListReverseIterator: return element type
        if (!strcmp(className, "ListIterator") || !strcmp(className, "ListReverseIterator")) {
            return listElementType;
        }
        // DEBUG: re-enable AbstractIterator-on-list to probe regression
        if (!strcmp(className, "AbstractIterator")) {
            return listElementType;
        }
    }

    return nullptr;
}

const QoreTypeInfo* QoreTypeInfo::getAbstractIteratorElementType(const QoreTypeInfo* iteratorTypeInfo) {
    if (!QC_ABSTRACTITERATOR || !iteratorTypeInfo || !hasType(iteratorTypeInfo)) {
        return nullptr;
    }

    const QoreTypeInfo* abstract_iterator_type = nullptr;

    const QoreParameterizedClassTypeInfo* pti = getParameterizedClassType(iteratorTypeInfo);
    if (pti) {
        const QoreTypeInfo* valueType = pti->getArgCount() ? pti->getTypeArgs()[0] : autoTypeInfo;
        const QoreTypeInfo* knownType = get_known_iterator_element_type(pti->getBaseClass(), valueType);
        if (knownType) {
            return knownType;
        }

        abstract_iterator_type = qore_class_private::get(*pti->getBaseClass())
            ->getParameterizedBaseTypeInfo(pti, QC_ABSTRACTITERATOR);
    }

    if (!abstract_iterator_type) {
        const QoreClass* qc = getUniqueReturnClass(iteratorTypeInfo);
        if (qc) {
            const QoreTypeInfo* knownType = get_known_iterator_element_type(qc, autoTypeInfo);
            if (knownType) {
                return knownType;
            }

            abstract_iterator_type = qore_class_private::get(*qc)
                ->getConcreteParameterizedBaseTypeInfo(QC_ABSTRACTITERATOR);
        }
    }

    const QoreParameterizedClassTypeInfo* abstract_pti = getParameterizedClassType(abstract_iterator_type);
    if (!abstract_pti || abstract_pti->getBaseClass() != QC_ABSTRACTITERATOR
            || abstract_pti->getTypeArgs().empty()) {
        return nullptr;
    }

    return abstract_pti->getTypeArgs()[0];
}

const QoreTypeInfo* QoreTypeInfo::getImplicitArgTypeForIterator(const QoreValue& iteratorExpr,
        const QoreTypeInfo* iteratorTypeInfo) {
    // First try list element type (existing behavior for list<T> sources)
    const QoreTypeInfo* implicitArgType = getUniqueReturnComplexList(iteratorTypeInfo);

    // For iterator-valued pseudo-method calls, prefer source-aware typing before
    // generic iterator base classes.  This preserves precise types for calls
    // like h.pairIterator() and avoids leaking unresolved legacy raw base type
    // parameters from iterators such as HashListIterator.
    if (!implicitArgType) {
        const QoreClass* qc = getUniqueReturnClass(iteratorTypeInfo);
        if (qc) {
            const QoreTypeInfo* sourceType = nullptr;
            if (iteratorExpr.getType() == NT_OPERATOR) {
                const QoreDotEvalOperatorNode* dotOp =
                    dynamic_cast<const QoreDotEvalOperatorNode*>(iteratorExpr.getInternalNode());
                if (dotOp) {
                    MethodCallNode* mcn = dotOp->getMethodCall();
                    if (mcn) {
                        sourceType = mcn->getSourceType();
                    }
                }
            }
            if (sourceType) {
                implicitArgType = getIteratorElementType(qc, sourceType);
            }
        }
    }

    if (!implicitArgType) {
        implicitArgType = getAbstractIteratorElementType(iteratorTypeInfo);
    }

    // If not a list or iterator, check if it's a hashdecl type (for "map expr, <Decl>{}")
    // When iterating over a hashdecl hash, $1 gets the hashdecl type
    if (!implicitArgType && iteratorTypeInfo) {
        if (parseReturns(iteratorTypeInfo, NT_LIST) == QTI_NOT_EQUAL) {
            const TypedHashDecl* thd = getUniqueReturnHashDecl(iteratorTypeInfo);
            if (thd) {
                implicitArgType = iteratorTypeInfo;
            }
        }
    }

    // If we still don't have an element type, but know the iterator is list-like or an iterator class,
    // return autoTypeInfo instead of null. This ensures $1 gets a definite type (auto) for hash literal
    // type inference, allowing "map {$1: $1}, untyped_list" to produce hash<auto> instead of plain hash.
    if (!implicitArgType && iteratorTypeInfo) {
        // Check if it's a list type (even without element type info)
        if (parseReturns(iteratorTypeInfo, NT_LIST) != QTI_NOT_EQUAL) {
            implicitArgType = autoTypeInfo;
        }
    }

    return implicitArgType;
}

const QoreTypeInfo* QoreTypeInfo::getReturnComplexHashOrNothing(const QoreTypeInfo* ti) {
    if (!ti || !hasType(ti)) {
        return nullptr;
    }
    if (ti->return_vec.size() > 1) {
        if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT))
            return nullptr;
    }
    return ti == autoHashTypeInfo || ti == autoHashOrNothingTypeInfo
        ? autoTypeInfo
        : ti->return_vec[0].spec.getComplexHash();
}

const QoreTypeInfo* QoreTypeInfo::getReturnComplexBufferOrNothing(const QoreTypeInfo* ti) {
    if (!ti || !hasType(ti)) {
        return nullptr;
    }
    if (ti->return_vec.size() > 1) {
        if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT)) {
            return nullptr;
        }
    }
    return ti->return_vec[0].spec.getComplexBuffer();
}

void QoreTypeInfo::getNodeType(QoreString& str, const QoreValue& n) {
    qore_type_t nt = n.getType();
    if (nt == NT_NOTHING) {
        str.concat("no value");
        return;
    }
    if (nt != NT_OBJECT) {
        str.sprintf("type '%s'", n.getFullTypeName());
        return;
    }
    const QoreObject* obj = n.get<const QoreObject>();
    if (!obj->isValid()) {
        str.sprintf("no value (deleted object of class '%s')", obj->getClassName());
        return;
    }
    str.sprintf("an object of class '%s'", obj->getClassName());
}

void QoreTypeInfo::ptext(QoreString& str, const char* arg_type, int param_num, const char* param_name) {
    if (!param_num && param_name && param_name[0] == '<') {
        str.concat(param_name);
        str.concat(' ');
        return;
    }
    if (param_name && param_name[0] == '<') {
        str.concat(param_name);
        str.concat(' ');
    }
    if (param_num) {
        str.sprintf("parameter %d ", param_num);
        if (param_name && param_name[0] != '<')
            str.sprintf("('%s') ", param_name);
    } else if (param_name)
        str.sprintf("%s '%s' ", arg_type, param_name);
    else {
        str.concat(arg_type);
        str.concat(' ');
    }
}

int QoreTypeInfo::doAcceptError(bool priv_error, const char* arg_type, bool obj, int param_num, const char* param_name,
        const QoreValue& n, ExceptionSink* xsink) const {
    if (priv_error) {
        if (obj) {
            doObjectPrivateClassException(param_name, xsink);
        } else {
            doPrivateClassException(arg_type, param_num + 1, param_name, xsink);
        }
    } else {
        if (obj) {
            doObjectHashDeclTypeException(arg_type, param_name, n, xsink);
        } else {
            doTypeException(arg_type, param_num + 1, param_name, n, xsink);
        }
    }
    return -1;
}

int QoreTypeInfo::doTypeException(const char* arg_type, int param_num, const char* param_name, const QoreValue& n,
        ExceptionSink* xsink) const {
    // xsink may be null in case parse exceptions have been disabled in the QoreProgram object
    // for example if there was a "requires" error
    if (!xsink)
        return -1;

    QoreStringNode* desc = new QoreStringNode;
    QoreTypeInfo::ptext(*desc, arg_type, param_num, param_name);
    desc->sprintf("expects type '%s', but got ", tname.c_str());
    QoreTypeInfo::getNodeType(*desc, n);
    desc->concat(" instead");
    // Add a hint about type narrowing for lvalue/key assignments when the expected type is a simple type
    // that could have come from type narrowing of hash<auto>
    if (!strcmp(arg_type, "lvalue") && isSimpleTypeNarrowed()) {
        desc->concat("; this may be due to type narrowing from hash<auto>; to store values of "
            "different types, use 'hash<auto!> h()' or 'hash h = cast<hash>({...})'");
    }
    xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
    return -1;
}

const QoreTypeInfo* QoreTypeInfo::getElementType(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return nullptr;
    }
    assert(!ti->return_vec.empty());
    if (ti == autoListTypeInfo
        || ti == autoListOrNothingTypeInfo
        || ti == softAutoListTypeInfo
        || ti == softAutoListOrNothingTypeInfo
        || ti == autoHashTypeInfo
        || ti == autoHashOrNothingTypeInfo) {
        return autoTypeInfo;
    }
    return ti->return_vec[0].spec.getElementType();
}

const TypedHashDecl* QoreTypeInfo::getTypedHash(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return nullptr;
    }
    assert(!ti->return_vec.empty());
    // CRITICAL FIX Phase 2: Don't extract hashdecl from optional complex hash types
    // For optional types like *hash<string, hash<DataProviderExpressionInfo>>,
    // return_vec has 2 specs: [actual_type, NOTHING].
    // Only reject if the base type is a complex hash (not a hashdecl itself)
    if (ti->return_vec.size() > 1) {
        // For optional types, check if base type is a complex hash by examining typespec
        if (ti->return_vec[0].spec.getTypeSpec() == QTS_COMPLEXHASH) {
            // It's an optional complex hash type - don't extract hashdecl from it
            // This prevents the inner hashdecl from being incorrectly applied to the outer hash
            return nullptr;
        }
        // It's an optional type but not a complex hash (e.g., *hashdecl)
        // Fall through to extract the hashdecl
    }
    return ti->return_vec[0].spec.getHashDecl();
}

const QoreTypeInfo* QoreTypeInfo::getComplexHashValueType(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return nullptr;
    }
    assert(!ti->return_vec.empty());
    if (ti == autoHashTypeInfo || ti == autoHashOrNothingTypeInfo) {
        return autoTypeInfo;
    }
    return ti->return_vec[0].spec.getComplexHash();
}

const QoreTypeInfo* QoreTypeInfo::getComplexListValueType(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return nullptr;
    }
    assert(!ti->return_vec.empty());
    if (ti == autoListTypeInfo || ti == autoListOrNothingTypeInfo) {
        return autoTypeInfo;
    }
    // Handle or_nothing types which have 2 return specs (actual type + NOTHING)
    // CRITICAL FIX: Validate structure for optional types
    if (ti->return_vec.size() > 1) {
        // Optional type must have exactly 2 specs: [actual_type, NOTHING]
        if (ti->return_vec.size() != 2 || (ti->return_vec[1].spec.match(NT_NOTHING) != QTI_IDENT)) {
            // Malformed optional type - reject it to prevent type corruption
            return nullptr;
        }
    }
    return ti->return_vec[0].spec.getComplexList();
}

const QoreComplexBufferTypeInfo* QoreTypeInfo::getComplexBufferType(const QoreTypeInfo* ti) {
    if (!hasType(ti)) {
        return nullptr;
    }
    assert(!ti->return_vec.empty());
    return dynamic_cast<const QoreComplexBufferTypeInfo*>(ti->return_vec[0].spec.getComplexBuffer());
}

const QoreTypeInfo* qore_get_complex_buffer_value_type(const QoreTypeInfo* ti) {
    const QoreComplexBufferTypeInfo* bti = QoreTypeInfo::getComplexBufferType(ti);
    return bti ? bti->getElementTypeInfo() : nullptr;
}

const QoreTypeInfo* QoreTypeInfo::getComplexBufferValueType(const QoreTypeInfo* ti) {
    return qore_get_complex_buffer_value_type(ti);
}

bool QoreTypeInfo::retypeValue(QoreValue& v, const QoreTypeInfo* target_ti,
        ExceptionSink* xsink) {
    if (!v.hasNode() || !target_ti || target_ti == autoTypeInfo) {
        return true;
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(target_ti)) {
        if (v.getType() != NT_HASH) {
            return true;
        }
        const QoreHashNode* src = v.get<const QoreHashNode>();
        if (src->getHashDecl() == hd) {
            return true;
        }
        QoreHashNode* coerced = typed_hash_decl_private::get(*hd)->newHash(
            src, /*runtime_check=*/true, xsink);
        if (!coerced || (xsink && xsink->isException())) {
            if (coerced) {
                coerced->deref(xsink);
            }
            return false;
        }
        v.discard(nullptr);
        v = coerced;
        return true;
    }

    const QoreTypeInfo* inner_h_vt = QoreTypeInfo::getComplexHashValueType(target_ti);
    if (inner_h_vt && inner_h_vt != autoTypeInfo) {
        if (v.getType() != NT_HASH) {
            return true;
        }
        QoreHashNode* h = v.get<QoreHashNode>();
        if (!h->is_unique()) {
            QoreHashNode* copy = h->copy();
            v.discard(nullptr);
            v = copy;
            h = copy;
        }
        HashIterator hi(h);
        while (hi.next()) {
            hash_assignment_priv ha(*qore_hash_private::get(*h), *qhi_priv::get(hi)->i);
            QoreValue cur(ha.swap(QoreValue()));
            if (!QoreTypeInfo::retypeValue(cur, inner_h_vt, xsink)) {
                ha.swap(cur);
                return false;
            }
            ha.swap(cur);
        }
        qore_hash_private::get(*h)->complexTypeInfo
            = qore_get_complex_hash_type(inner_h_vt);
        return true;
    }

    const QoreTypeInfo* inner_l_vt = QoreTypeInfo::getComplexListValueType(target_ti);
    if (inner_l_vt && inner_l_vt != autoTypeInfo) {
        if (v.getType() != NT_LIST) {
            return true;
        }
        QoreListNode* l = v.get<QoreListNode>();
        if (!l->is_unique()) {
            QoreListNode* copy = l->copy();
            v.discard(nullptr);
            v = copy;
            l = copy;
        }
        qore_list_private* lp = qore_list_private::get(*l);
        size_t n = l->size();
        for (size_t i = 0; i < n; ++i) {
            QoreValue cur = lp->swap(i, QoreValue());
            if (!QoreTypeInfo::retypeValue(cur, inner_l_vt, xsink)) {
                lp->swap(i, cur);
                return false;
            }
            lp->swap(i, cur);
        }
        lp->complexTypeInfo = qore_get_complex_list_type(inner_l_vt);
        return true;
    }

    return true;
}

qore_type_result_e QoreTypeInfo::parseAccepts(const QoreTypeInfo* first, const QoreTypeInfo* second,
        bool& may_not_match, bool& may_need_filter, qore_type_result_e& max_result,
        bool known_initial_assignment) {
    /*
    if (first == second) {
        // issue
        max_result = QTI_IDENT;
        return QTI_IDENT;
    }
    */
    if (first == autoTypeInfo) {
        // If parameter is auto and argument is untyped (also auto),
        // require runtime matching so specific types can be preferred
        if (!hasType(second)) {
            may_not_match = true;
            max_result = QTI_AMBIGUOUS;
            return QTI_AMBIGUOUS;
        }
        // For union types, check all components against auto via instance method
        // This ensures proper scoring for union variant selection
        if (second && second->return_vec.size() > 1) {
            return first->parseAccepts(second, may_not_match, may_need_filter, max_result, known_initial_assignment);
        }
        max_result = QTI_WILDCARD;
        return QTI_WILDCARD;
    }
    if (!hasType(first)) {
        if (!may_need_filter && isComplex(second))
            may_need_filter = true;
        max_result = QTI_WILDCARD;
        return QTI_WILDCARD;
    }
    if (!hasType(second)) {
        if (!may_need_filter) {
            // check if we could need a runtime filter
            for (auto& i : first->getAcceptSpecs()) {
                if (i.map) {
                    may_need_filter = true;
                    break;
                }
            }
        }
        max_result = QTI_IDENT;
        may_not_match = true;
        return QTI_AMBIGUOUS;
    }
    return first->parseAccepts(second, may_not_match, may_need_filter, max_result, known_initial_assignment);
}

qore_type_result_e QoreTypeInfo::parseAccepts(const QoreTypeInfo* typeInfo, bool& may_not_match,
        bool& may_need_filter, qore_type_result_e& max_result, bool known_initial_assignment) const {
    //printd(5, "QoreTypeInfo::parseAccepts() '%s' <- '%s'\n", tname.c_str(), typeInfo->tname.c_str());
    // Restore develop's logic: iterate source return_vec components against target accept_vec.
    // For union types (return_vec.size() > 1), all components are checked; return QTI_AMBIGUOUS
    // when ok=true (not QTI_IDENT, which incorrectly signals exact identity for unions and
    // changes variant resolution behavior).
    if (typeInfo->return_vec.size() > getAcceptSpecs().size()) {
        may_not_match = true;
    }

    bool ok = false;
    for (auto& rt : typeInfo->return_vec) {
        bool t_no_match = true;
        for (auto& at : getAcceptSpecs()) {
            qore_type_result_e t_max_result = QTI_NOT_EQUAL;
            qore_type_result_e res = parseAcceptsIntern(at, rt, may_not_match, may_need_filter, t_no_match, ok,
                t_max_result, known_initial_assignment);
            if (res == QTI_IDENT) {
                max_result = t_max_result;
                return res;
            } else if (res == QTI_AMBIGUOUS || res == QTI_NEAR || res == QTI_WILDCARD) {
                max_result = t_max_result;
                assert(ok);
                if (may_not_match) {
                    return res;
                }
                break;
            }
        }
        if (t_no_match) {
            if (!may_not_match) {
                may_not_match = true;
                if (ok) {
                    return QTI_AMBIGUOUS;
                }
            }
        }
    }
    if (ok) {
        return QTI_AMBIGUOUS;
    }
    may_not_match = false;
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeInfo::runtimeTypeMatch(const QoreTypeInfo* typeInfo) const {
    //printd(5, "QoreTypeInfo::runtimeTypeMatch() '%s' <=> '%s'\n", tname.c_str(), typeInfo->tname.c_str());
    if (typeInfo->return_vec.size() != return_vec.size() || typeInfo->getAcceptVecSize() != getAcceptVecSize()) {
        return QTI_NOT_EQUAL;
    }

    // check accept types
    qore_type_result_e rc = QTI_IDENT;
    for (size_t i = 0; i < getAcceptVecSize(); ++i) {
        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e res = getAcceptSpec(i).spec.match(typeInfo->getAcceptSpec(i).spec, may_not_match, may_need_filter);
        //printd(5, " + accept: %s %d <=> %s %d: %d\n", QoreTypeInfo::getName(getAcceptSpec(i).spec.getBaseTypeInfo()), getAcceptSpec(i).spec.getTypeSpec(), QoreTypeInfo::getName(typeInfo->getAcceptSpec(i).spec.getBaseTypeInfo()), typeInfo->getAcceptSpec(i).spec.getTypeSpec(), res);
        if (res < QTI_NEAR) {
            return QTI_NOT_EQUAL;
        }
        if (res < rc) {
            rc = res;
        }
    }

    // check return types
    for (size_t i = 0; i < return_vec.size(); ++i) {
        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e res = return_vec[i].spec.match(typeInfo->return_vec[i].spec, may_not_match, may_need_filter);
        //printd(5, " + return: %s %d <=> %s %d: %d\n", QoreTypeInfo::getName(return_vec[i].spec.getBaseTypeInfo()), return_vec[i].spec.getTypeSpec(), QoreTypeInfo::getName(typeInfo->return_vec[i].spec.getBaseTypeInfo()), typeInfo->return_vec[i].spec.getTypeSpec(), res);
        if (res < QTI_NEAR) {
            return QTI_NOT_EQUAL;
        }
        if (res < rc) {
            rc = res;
        }
    }

    return rc;
}

bool QoreTypeInfo::matchCommonType(const QoreTypeInfo*& ctype, const QoreTypeInfo* ntype) {
    // issue #3005: if the first element had no type, then there is no common type
    if (!ctype || ctype == anyTypeInfo) {
        ctype = nullptr;
        return false;
    }
    if (ctype == ntype) {
        return true;
    }
    if (!QoreTypeInfo::hasType(ntype)) {
        // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
        ctype = ntype == anyTypeInfo ? nullptr : ntype;
        return false;
    }

    // ctype |* NOTHING -> *type
    // only call get_or_nothing_type() if ctype doesn't already accept NOTHING
    if (!QoreTypeInfo::parseAcceptsReturns(ctype, NT_NOTHING) && QoreTypeInfo::isType(ntype, NT_NOTHING)) {
        const QoreTypeInfo* ti = get_or_nothing_type(ctype);
        ctype = ti;
        return ctype != autoTypeInfo ? true : false;
    }

    // ctype==NOTHING | type -> *type
    if (QoreTypeInfo::isType(ctype, NT_NOTHING)) {
        ctype = get_or_nothing_type_check(ntype);
        return ctype != autoTypeInfo ? true : false;
    }

    // ctype |* *ctype -> *ctype
    // if the new type is a superset of the existing common type, then use the new type
    if (ntype->superSetOf(ctype)) {
        ctype = ntype;
        return true;
    }

    // try to find a common base type
    // if we're dealing with types that return multiple types, then they are not compatible
    if (ctype->return_vec.size() > 1 || ntype->return_vec.size() > 1) {
        // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
        ctype = autoTypeInfo;
        return false;
    }

    // see if we have a complex type
    const QoreTypeInfo* bti = ctype->return_vec[0].spec.getBaseTypeInfo();
    if (bti == ntype->return_vec[0].spec.getBaseTypeInfo()) {
        // issue #3429: when performing type folding with complex types, do not use subtype "any" but rather use "auto"
        if (bti == hashTypeInfo) {
            // Check if both are hashdecls before degrading to autoHashTypeInfo
            // Fixes issue where hashdecl type infos from different module load instances are incorrectly
            // degraded to hash<auto>, causing overload resolution failures and type checking errors
            const TypedHashDecl* hd1 = ctype->return_vec[0].spec.getHashDecl();
            const TypedHashDecl* hd2 = ntype->return_vec[0].spec.getHashDecl();
            if (hd1 && hd2) {
                const typed_hash_decl_private* thd1 = typed_hash_decl_private::get(*hd1);
                const typed_hash_decl_private* thd2 = typed_hash_decl_private::get(*hd2);
                // Preserve hashdecl structure only when both types describe
                // the same declaration or a parent/child hierarchy.  Unrelated
                // hashdecls must fall back to hash<auto>; otherwise the first
                // element's hashdecl is incorrectly applied to later elements.
                if (thd1->equal(*thd2) || thd2->isDescendantOf(*thd1)) {
                    return true;
                }
                if (thd1->isDescendantOf(*thd2)) {
                    ctype = ntype;
                    return true;
                }
            }
            ctype = autoHashTypeInfo;
        } else if (bti == listTypeInfo) {
            ctype = autoListTypeInfo;
        } else if (bti == softListTypeInfo) {
            ctype = softAutoListTypeInfo;
        } else if (bti == hashOrNothingTypeInfo) {
            ctype = autoHashOrNothingTypeInfo;
        } else if (bti == listOrNothingTypeInfo) {
            ctype = autoListOrNothingTypeInfo;
        } else if (bti == softListOrNothingTypeInfo) {
            ctype = softAutoListOrNothingTypeInfo;
        } else {
            ctype = bti;
        }
        return true;
    }
    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    ctype = autoTypeInfo;
    return false;
}

bool QoreTypeInfo::superSetOf(const QoreTypeInfo* t) const {
    if (getAcceptVecSize() < t->getAcceptVecSize() || return_vec.size() < t->return_vec.size())
        return false;

    for (unsigned i = 0; i < t->getAcceptVecSize(); ++i) {
        if (t->getAcceptSpec(i).spec != getAcceptSpec(i).spec)
            return false;
    }

    for (unsigned i = 0; i < t->return_vec.size(); ++i) {
        if (t->return_vec[i].spec != return_vec[i].spec)
            return false;
    }

    return true;
}

bool QoreTypeInfo::outputSuperSetOf(const QoreTypeInfo* t) const {
    if (return_vec.size() < t->return_vec.size())
        return false;

    for (unsigned i = 0; i < t->return_vec.size(); ++i) {
        bool may_not_match = false;
        bool may_need_filter = false;
        if (!return_vec[i].spec.match(t->return_vec[i].spec, may_not_match, may_need_filter))
            return false;
        if (may_not_match)
            return false;
    }

    return true;
}

void QoreTypeInfo::acceptInputIntern(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
        const char* param_name, QoreValue& n, LValueHelper* lvhelper) const {
    // CRITICAL FIX Phase 2: Prioritize complex hash/list handlers for optional types
    // For optional types (return_vec.size() > 1), we need to ensure complex hash/list
    // handlers are tried BEFORE hashdecl handlers to prevent incorrect routing.
    // This prevents inner hashdecls from being extracted and applied to outer containers.

    if (return_vec.size() > 1 && return_vec[1].spec.match(NT_NOTHING) == QTI_IDENT) {
        // This is a proper optional type. For optional complex hashes/lists, try those first.
        // Check if return_vec[0] indicates a complex hash or list
        q_typespec_t base_typespec = return_vec[0].spec.getTypeSpec();
        if (base_typespec == QTS_COMPLEXHASH || base_typespec == QTS_COMPLEXLIST ||
            base_typespec == QTS_COMPLEXSOFTLIST) {
            // Try complex hash/list specs first
            for (auto& t : getAcceptSpecs()) {
                q_typespec_t spec_type = t.spec.getTypeSpec();
                if (spec_type == QTS_COMPLEXHASH || spec_type == QTS_COMPLEXLIST ||
                    spec_type == QTS_COMPLEXSOFTLIST) {
                    if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
                        return;
                    }
                }
            }
            // If complex handlers didn't work, try hashdecl/other handlers
            for (auto& t : getAcceptSpecs()) {
                q_typespec_t spec_type = t.spec.getTypeSpec();
                if (spec_type != QTS_COMPLEXHASH && spec_type != QTS_COMPLEXLIST &&
                    spec_type != QTS_COMPLEXSOFTLIST) {
                    if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
                        return;
                    }
                }
            }
            doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink);
            return;
        }
    }

    // Normal routing for non-optional or non-complex types
    for (auto& t : getAcceptSpecs()) {
        if (t.spec.acceptInput(xsink, *this, t.map, arg_type, obj, param_num, param_name, n, lvhelper)) {
            return;
        }
    }
    doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink);
}

qore_type_result_e QoreTypeInfo::parseAcceptsIntern(const QoreAcceptSpec& at, const QoreReturnSpec& rt,
        bool& may_not_match, bool& may_need_filter, bool& t_no_match, bool& ok, qore_type_result_e& max_result,
        bool known_initial_assignment) {
    //printd(5, "QoreTypeInfo::parseAcceptsIntern() at: %d rt: %d rc: %d\n", (int)at.spec.getTypeSpec(),
    //    (int)rt.spec.getTypeSpec(), at.spec.match(rt.spec, may_not_match, may_need_filter));
    qore_type_result_e res = at.spec.match(rt.spec, may_not_match, may_need_filter, max_result,
        known_initial_assignment);
    switch (res) {
        case QTI_IDENT:
            if (at.exact && rt.exact) {
                if (at.map && !may_need_filter) {
                    may_need_filter = true;
                }
                return QTI_IDENT;
            }
        // fall down to next case
        case QTI_NEAR:
        case QTI_AMBIGUOUS:
        case QTI_WILDCARD:
            if (at.map && !may_need_filter) {
                may_need_filter = true;
            }
            if (t_no_match) {
                t_no_match = false;
                if (!ok) {
                    ok = true;
                    if (may_not_match) {
                        return res;
                    }
                }
            }

        // fall down to default
        default:
            break;
    }
    return QTI_NOT_EQUAL;
}

bool QoreParseTypeInfo::paramTypesIdentical(
        const QoreTypeInfo* ti_a, const QoreParseTypeInfo* pti_a,
        const QoreTypeInfo* ti_b, const QoreParseTypeInfo* pti_b,
        bool& recheck) {
    // Both resolved types: direct comparison
    if (ti_a && ti_b) {
        return QoreTypeInfo::isInputIdentical(ti_a, ti_b);
    }
    // a resolved, b unresolved
    if (ti_a && pti_b) {
        return parseStageOneIdenticalWithParsed(pti_b, ti_a, recheck);
    }
    // a unresolved, b resolved
    if (pti_a && ti_b) {
        return parseStageOneIdenticalWithParsed(pti_a, ti_b, recheck);
    }
    // Both unresolved: compare parse-time types
    if (pti_a && pti_b) {
        return parseStageOneIdentical(pti_a, pti_b, recheck);
    }
    // Both untyped (nullptr): identical
    return true;
}
