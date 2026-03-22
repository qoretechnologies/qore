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
#include "qore/intern/QoreTypeSpecMatchRegistry.h"

#include <algorithm>
#include <set>

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

// Cache for parameterized HashPairInfo types based on value type
typedef std::map<const QoreTypeInfo*, TypedHashDecl*> hp_decl_map_t;
static hp_decl_map_t hp_decl_map;

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
    // Clean up union type cache
    for (auto& i : union_map)
        delete i.second;
    for (auto& i : union_on_map)
        delete i.second;
    // Clean up typed callable type cache
    for (auto& i : complex_code_map)
        delete i.second;
    // Clean up hash pair info decl cache
    for (auto& i : hp_decl_map)
        typed_hash_decl_private::get(*i.second)->deref();
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
            const QoreTypeInfo* orn = get_or_nothing_type(ti);
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
        for (const auto& a : ti->accept_vec) {
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

// Forward declaration
static QoreParseTypeInfo* parse_type_string_to_pti(const char* type_str);

// Helper function to parse a type string into a QoreParseTypeInfo with proper subtypes
static QoreParseTypeInfo* parse_type_string_to_pti(const char* type_str) {
    // Trim whitespace
    while (*type_str && isspace(*type_str)) ++type_str;
    if (!*type_str) return nullptr;

    std::string str(type_str);
    while (!str.empty() && isspace(str.back())) str.pop_back();
    if (str.empty()) return nullptr;

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
                QoreParseTypeInfo* sub_pti = parse_type_string_to_pti(current.c_str());
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
    QoreParseTypeInfo* pti = parse_type_string_to_pti(type_str);
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
        case NT_OBJECT:
            return static_cast<const QoreObject*>(n)->getClass()->getTypeInfo();
        case NT_WEAKREF:
            return static_cast<const WeakReferenceNode*>(n)->get()->getClass()->getTypeInfo();
        case NT_HASH:
            return static_cast<const QoreHashNode*>(n)->getTypeInfo();
        case NT_WEAKREF_HASH:
            return static_cast<const WeakHashReferenceNode*>(n)->get()->getTypeInfo();
        case NT_LIST:
            return static_cast<const QoreListNode*>(n)->getTypeInfo();
        case NT_WEAKREF_LIST:
            return static_cast<const WeakListReferenceNode*>(n)->get()->getTypeInfo();
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
static qore_type_result_e match_type(const QoreTypeInfo* this_type, const QoreTypeInfo* that_type,
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

qore_type_result_e QoreTypeSpec::match(const QoreTypeSpec& t, bool& may_not_match, bool& may_need_filter,
        qore_type_result_e& max_result, bool known_initial_assignment) const {
    //printd(5, "QoreTypeSpec::match() typespec: %d t.typespec: %d\n", (int)typespec, (int)t.typespec);
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
        if ((u.t == NT_LIST || u.t == NT_HASH) && t.typespec != QTS_TYPE && t.typespec != QTS_EMPTYLIST
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
    if (typespec == QTS_CLASS) {
        return t == NT_OBJECT ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_HASHDECL || typespec == QTS_COMPLEXHASH) {
        return t == NT_HASH ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXLIST) {
        return t == NT_LIST ? QTI_IDENT : QTI_NOT_EQUAL;
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

bool QoreTypeSpec::acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
        bool& err) const {
    assert(!err);
    const QoreTypeInfo* ti = h->getValueTypeInfo();
    if (QoreTypeInfo::equal(u.ti, ti)) {
        return true;
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
                ok = type_spec_accept_object(*u.qc, *n.get<const QoreObject>()->getClass(), priv_error);
            } else if (t == NT_WEAKREF) {
                ok = type_spec_accept_object(*u.qc, *n.get<const WeakReferenceNode>()->get()->getClass(), priv_error);
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
                // Accept if same hashdecl or source is a descendant of target
                if (source->equal(*target) || source->isDescendantOf(*target)) {
                    ok = true;
                }
            }
            break;
        }
        case QTS_COMPLEXHASH: {
            if (t == NT_HASH) {
                if (u.ti == autoTypeInfo) {
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
                if (u.ti == autoTypeInfo) {
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
                if (u.ti == autoTypeInfo) {
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
                    LValueHelper lvh(r, xsink, true);
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
            if (u.ti == autoTypeInfo) {
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
                if (u.ti == autoTypeInfo) {
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

const QoreTypeInfo* QoreParseTypeInfo::resolveRuntime() const {
    if (!subtypes.empty())
        return resolveRuntimeSubtype();

    const QoreTypeInfo* rv = or_nothing ? getBuiltinUserOrNothingTypeInfo(cscope->ostr) : getBuiltinUserTypeInfo(cscope->ostr);
    return rv ? rv : resolveRuntimeClass(*cscope, or_nothing);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveRuntimeSubtype() const {
    if (!strcmp(cscope->ostr, "hash")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
            // resolve hashdecl
            const qore_ns_private* ns;
            const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->runtimeFindHashDeclIntern(*subtypes[0]->cscope, ns);
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
    if (hd)
        return hd->getTypeInfo(or_nothing);

    return nullptr;
}

const QoreTypeInfo* QoreParseTypeInfo::resolveSubtype(const QoreProgramLocation* loc, int& err) const {
    if (!strcmp(cscope->ostr, "hash")) {
        if (subtypes.size() == 1) {
            if (!strcmp(subtypes[0]->cscope->ostr, "auto"))
                return or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo;
            if (!strcmp(subtypes[0]->cscope->ostr, "auto!"))
                return or_nothing ? autoNoNarrowHashOrNothingTypeInfo : autoNoNarrowHashTypeInfo;
            // resolve hashdecl
            const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->parseFindHashDecl(loc,
                *subtypes[0]->cscope);
            if (hd) {
                return hd->getTypeInfo(or_nothing);
            }
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

    parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; type '%s' does not take subtype declarations",
        getName(), cscope->getIdentifier());
    err = -1;
    return autoTypeInfo;
}

const QoreTypeInfo* QoreParseTypeInfo::resolve(const QoreProgramLocation* loc, int& err) const {
    if (!subtypes.empty()) {
        return resolveSubtype(loc, err);
    }

    return resolveClass(loc, *cscope, or_nothing, err);
}

const QoreTypeInfo* QoreParseTypeInfo::resolveAny(const QoreProgramLocation* loc, int& err) const {
    if (!subtypes.empty()) {
        return resolveSubtype(loc, err);
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
        return hd->getTypeInfo(or_nothing);
    }

    // type not found - raise error
    parse_error(*loc, "reference to undefined type '%s'", cscope.ostr);
    err = -1;
    return objectTypeInfo;
}

QoreValue QoreHashDeclTypeInfo::getDefaultQoreValueImpl() const {
    return qore_hash_private::newHashDecl(accept_vec[0].spec.getHashDecl());
    //return new QoreHashNode(accept_vec[0].spec.getHashDecl(), xsink);
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
    // For auto/any value types, return autoHashTypeInfo as the element type
    if (!valueType || valueType == autoTypeInfo || valueType == anyTypeInfo) {
        return autoHashTypeInfo;
    }

    // Check cache under lock
    {
        AutoLocker al(ctl);
        hp_decl_map_t::iterator i = hp_decl_map.lower_bound(valueType);
        if (i != hp_decl_map.end() && i->first == valueType) {
            return i->second->getTypeInfo();
        }
    }

    // Create a new TypedHashDecl for this value type
    // HashPairInfo has "key" (string) and "value" (valueType)
    TypedHashDecl* hd = new TypedHashDecl("HashPairInfo", "Qore::HashPairInfo");
    typed_hash_decl_private::get(*hd)->addMember("key", stringTypeInfo, QoreValue());
    typed_hash_decl_private::get(*hd)->addMember("value", valueType, QoreValue());
    typed_hash_decl_private::get(*hd)->setSystemPublic();

    // Cache it under lock
    {
        AutoLocker al(ctl);
        // Check again in case another thread added it
        hp_decl_map_t::iterator i = hp_decl_map.lower_bound(valueType);
        if (i != hp_decl_map.end() && i->first == valueType) {
            // Another thread added it - clean up and use theirs
            typed_hash_decl_private::get(*hd)->deref();
            return i->second->getTypeInfo();
        }
        hp_decl_map.insert(i, hp_decl_map_t::value_type(valueType, hd));
    }

    return hd->getTypeInfo();
}

const QoreTypeInfo* QoreTypeInfo::getIteratorElementType(const QoreClass* iteratorClass,
        const QoreTypeInfo* sourceTypeInfo) {
    if (!iteratorClass || !sourceTypeInfo) {
        return nullptr;
    }

    const char* className = iteratorClass->getName();

    // Handle hash iterators
    const QoreTypeInfo* hashValueType = getUniqueReturnComplexHash(sourceTypeInfo);
    if (!hashValueType) {
        // Also try hash-or-nothing types
        hashValueType = getReturnComplexHashOrNothing(sourceTypeInfo);
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
    }

    return nullptr;
}

const QoreTypeInfo* QoreTypeInfo::getImplicitArgTypeForIterator(const QoreValue& iteratorExpr,
        const QoreTypeInfo* iteratorTypeInfo) {
    // First try list element type (existing behavior for list<T> sources)
    const QoreTypeInfo* implicitArgType = getUniqueReturnComplexList(iteratorTypeInfo);

    // If not a list, check if it's an iterator class and get element type from source type
    if (!implicitArgType) {
        const QoreClass* qc = getUniqueReturnClass(iteratorTypeInfo);
        if (qc) {
            // Try to get the source type from the iterator expression if it's a method call
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
