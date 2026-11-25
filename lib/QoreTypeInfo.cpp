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
#include "qore/intern/QoreRuntimeDiagnostic.h"
#include "qore/intern/QoreArithmeticParseType.h"
#include <qore/QoreRWLock.h>
#include <optional>
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
#include "qore/intern/QoreTypeDeclPin.h"
#include "qore/intern/qore_aot_deps.h"
#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/QoreIteratorBase.h"
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/QoreBufferNodePrivate.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <typeinfo>
#include <unordered_map>

const QoreAnyTypeInfo staticAnyTypeInfo;
const QoreAutoTypeInfo staticAutoTypeInfo;
const QoreAutoNoNarrowTypeInfo staticAutoNoNarrowTypeInfo;

const QoreBigIntTypeInfo staticBigIntTypeInfo;
const QoreBigIntOrNothingTypeInfo staticBigIntOrNothingTypeInfo;

const QoreStringTypeInfo staticStringTypeInfo;
const QoreStringOrNothingTypeInfo staticStringOrNothingTypeInfo;

const QoreBoolTypeInfo staticBoolTypeInfo;
const QoreBoolOrNothingTypeInfo staticBoolOrNothingTypeInfo;

const QoreCharTypeInfo staticCharTypeInfo;
const QoreCharOrNothingTypeInfo staticCharOrNothingTypeInfo;

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

const QoreSoftCharTypeInfo staticSoftCharTypeInfo;
const QoreSoftCharOrNothingTypeInfo staticSoftCharOrNothingTypeInfo;

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
   *charTypeInfo = &staticCharTypeInfo,
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
   *softCharTypeInfo = &staticSoftCharTypeInfo,
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
   *charOrNothingTypeInfo = &staticCharOrNothingTypeInfo,
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
   *softCharOrNothingTypeInfo = &staticSoftCharOrNothingTypeInfo,
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

// map from "or nothing" types to simple types
typedef std::map<const QoreTypeInfo*, const QoreTypeInfo*> typeinfo_map_t;
static typeinfo_map_t typeinfo_or_nothing_map;

// the or-nothing types of the builtin types, found with QoreTypeInfo::getBuiltinOrNothingIndex() - 1; filled by
// init_qore_types() before other threads run
static std::vector<const QoreTypeInfo*> builtin_or_nothing_types;

static void set_builtin_or_nothing_type(const QoreTypeInfo* typeInfo, const QoreTypeInfo* orNothingTypeInfo) {
    builtin_or_nothing_types.push_back(orNothingTypeInfo);
    // the index is stored as a 16-bit value; there are a few dozen builtin types
    assert(builtin_or_nothing_types.size() < 0xffff);
    typeInfo->setBuiltinOrNothingIndex(static_cast<uint16_t>(builtin_or_nothing_types.size()));
}

static QoreThreadLock ctl; // complex type lock
// the type caches can be used in a child process created with fork() (see QoreForkHandlers.h)
static QoreForkThreadLock ctl_fork_handler(ctl, QFLR_LEAF, "complex_types");

const type_vec_t* QoreLegacyTypeVec::make(const orphanable_type_vec_t& src) const {
    // under the lock that refresh() takes: a copy made while the types are orphaned reads their tombstones
    AutoLocker al(ctl);
    type_vec_t* rv = vec.load(std::memory_order_relaxed);
    if (!rv) {
        std::unique_ptr<type_vec_t> copy(new type_vec_t(orphanable_type_vec_view_t(src).toVector()));
        rv = copy.release();
        vec.store(rv, std::memory_order_release);
    }
    return rv;
}

void QoreLegacyTypeVec::refresh(const orphanable_type_vec_t& src) const {
    AutoLocker al(ctl);
    type_vec_t* v = vec.load(std::memory_order_relaxed);
    if (!v) {
        return;
    }
    assert(v->size() == src.size());
    for (size_t i = 0, e = src.size(); i < e; ++i) {
        const QoreTypeInfo* ti = src[i].get();
        // the element is read without the lock by the callers of the deprecated reflection functions
        std::atomic_ref<const QoreTypeInfo*> elem((*v)[i]);
        if (elem.load(std::memory_order_relaxed) != ti) {
            elem.store(ti, std::memory_order_release);
        }
    }
}

namespace {
class QoreAOTDeferredTypeInfo : public QoreTypeInfo {
public:
    QoreAOTDeferredTypeInfo(std::string qore_path, std::string type_path, bool hashdecl,
            bool or_nothing)
            : QoreTypeInfo(type_path.c_str(), makeAcceptVec(hashdecl, or_nothing),
                makeReturnVec(hashdecl, or_nothing)),
            qore_path(std::move(qore_path)), type_path(std::move(type_path)), hashdecl(hashdecl),
            or_nothing(or_nothing) {
        aot_deferred_placeholder = true;
    }

    bool samePlaceholder(const QoreAOTDeferredTypeInfo& other) const {
        return hashdecl == other.hashdecl
            && or_nothing == other.or_nothing
            && qore_path == other.qore_path
            && type_path == other.type_path;
    }

    bool matchesResolvedType(const QoreTypeInfo* ti) const {
        if (!ti) {
            return false;
        }
        const char* path = QoreTypeInfo::getPath(ti);
        if (!path) {
            return false;
        }
        if (type_path == path) {
            return true;
        }
        // Deferred paths use the unrooted spelling serialized in AOT contracts, while a loaded
        // class can render as object<::Ns::Class>.  Ignore only that root qualifier: the kind,
        // nullable marker, and complete namespace-qualified name must still match.
        size_t prefix_size = strlen(hashdecl ? "hash<" : "object<") + (or_nothing ? 1 : 0);
        return !strncmp(path, type_path.c_str(), prefix_size)
            && path[prefix_size] == ':' && path[prefix_size + 1] == ':'
            && !strcmp(path + prefix_size + 2, type_path.c_str() + prefix_size);
    }

protected:
    std::string qore_path;
    std::string type_path;
    bool hashdecl;
    bool or_nothing;

    static q_accept_vec_t makeAcceptVec(bool hashdecl, bool or_nothing) {
        q_accept_vec_t rv;
        rv.emplace_back(QoreTypeSpec(hashdecl ? NT_HASH : NT_OBJECT), nullptr, !or_nothing);
        if (or_nothing) {
            rv.emplace_back(QoreTypeSpec(NT_NOTHING), nullptr);
            rv.emplace_back(QoreTypeSpec(NT_NULL),
                [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); });
        }
        return rv;
    }

    static q_return_vec_t makeReturnVec(bool hashdecl, bool or_nothing) {
        q_return_vec_t rv;
        rv.emplace_back(QoreTypeSpec(hashdecl ? NT_HASH : NT_OBJECT), !or_nothing);
        if (or_nothing) {
            rv.emplace_back(QoreTypeSpec(NT_NOTHING));
        }
        return rv;
    }

    virtual void getThisTypeImpl(QoreString& str) const {
        concatName(str);
    }

    virtual const char* getPathImpl() const {
        return type_path.c_str();
    }

    virtual bool hasDefaultValueImpl() const {
        return hashdecl && !or_nothing;
    }

    virtual QoreValue getDefaultQoreValueImpl() const {
        return hashdecl && !or_nothing ? emptyHash->hashRefSelf() : QoreValue();
    }

    virtual bool canConvertToScalarImpl() const {
        return false;
    }
};

static std::mutex aot_deferred_type_info_mutex;
static std::unordered_map<std::string, std::unique_ptr<QoreAOTDeferredTypeInfo>> aot_deferred_type_info_map;
static QoreForkStdMutex aot_deferred_type_info_fork_handler(aot_deferred_type_info_mutex, QFLR_LEAF,
    "aot_deferred_types");

static std::string aotDeferredTypeQorePath(const char* qore_path) {
    if (!qore_path) {
        return std::string();
    }
    while (qore_path[0] == ':' && qore_path[1] == ':') {
        qore_path += 2;
    }
    return qore_path;
}

//! Renders a deferred type using the unrooted spelling of whole-group AOT contracts.
/** A type this parse cannot see yet and the same type resolved in a whole-group parse must
    have the same serialized spelling, or every signature and declaration hash that names it differs
    between a whole-group parse and a parse against preloaded shells -- and the scheduler then
    rebuilds the whole closure of an edit for a difference that changes no declaration.

    A deferred CLASS previously used the rooted spelling (`object<::X>`), unlike the
    whole-group contract (`object<X>`); deferred hashdecls already used the unrooted form.
    A loaded live type can still return a rooted path, which matchesResolvedType() handles
    separately without changing the serialized spelling.
*/
static std::string aotDeferredTypePath(const std::string& qore_path, bool hashdecl, bool or_nothing) {
    std::string rv;
    if (or_nothing) {
        rv += '*';
    }
    rv += hashdecl ? "hash<" : "object<";
    rv += qore_path;
    rv += '>';
    return rv;
}
} // namespace

const QoreTypeInfo* qore_get_aot_deferred_type_info(const QoreProgramLocation* loc, const char* qore_path,
        bool or_nothing, bool hashdecl) {
    std::string deferred_path = qore_aot_get_deferred_source_symbol_path(loc, qore_path,
        hashdecl ? QoreAOTSourceSymbolKind::HashDecl : QoreAOTSourceSymbolKind::Class);
    std::string clean_path = aotDeferredTypeQorePath(deferred_path.empty() ? qore_path : deferred_path.c_str());
    if (clean_path.empty()) {
        return hashdecl
            ? (or_nothing ? hashOrNothingTypeInfo : hashTypeInfo)
            : (or_nothing ? objectOrNothingTypeInfo : objectTypeInfo);
    }

    std::string type_path = aotDeferredTypePath(clean_path, hashdecl, or_nothing);
    if (qore_aot_source_parse_active()) {
        if (QoreProgram* pgm = getProgram()) {
            qore_program_private::recordSourceParseTypeImport(pgm, loc, clean_path.c_str(), type_path.c_str(),
                hashdecl, or_nothing);
        }
    }

    std::string key = hashdecl ? "hashdecl:" : "class:";
    key += or_nothing ? "ornothing:" : "required:";
    key += clean_path;

    std::lock_guard<std::mutex> lock(aot_deferred_type_info_mutex);
    auto i = aot_deferred_type_info_map.find(key);
    if (i != aot_deferred_type_info_map.end()) {
        return i->second.get();
    }

    auto inserted = aot_deferred_type_info_map.emplace(key, std::make_unique<QoreAOTDeferredTypeInfo>(
        std::move(clean_path), std::move(type_path), hashdecl, or_nothing));
    return inserted.first->second.get();
}

bool qore_type_info_is_aot_deferred(const QoreTypeInfo* ti) {
    return ti && ti->isAotDeferredPlaceholder();
}

bool qore_type_info_aot_deferred_compare(const QoreTypeInfo* a, const QoreTypeInfo* b, bool& result) {
    // neither type is a placeholder: the comparison is not deferred (the common case, without a dynamic_cast)
    if (!qore_type_info_is_aot_deferred(a) && !qore_type_info_is_aot_deferred(b)) {
        return false;
    }
    const QoreAOTDeferredTypeInfo* da = qore_type_info_is_aot_deferred(a)
        ? static_cast<const QoreAOTDeferredTypeInfo*>(a) : nullptr;
    const QoreAOTDeferredTypeInfo* db = qore_type_info_is_aot_deferred(b)
        ? static_cast<const QoreAOTDeferredTypeInfo*>(b) : nullptr;
    if (!da && !db) {
        return false;
    }
    if (da && db) {
        result = da->samePlaceholder(*db);
        return true;
    }
    result = da ? da->matchesResolvedType(b) : db->matchesResolvedType(a);
    return true;
}

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

//! Every type and type-parameter owner some derived-type cache entry is keyed by, guarded by ctl
/** Lets qore_purge_derived_types() return at once for the great majority of destroyed types, which no
    derived type was ever built on, instead of scanning every cache for each one.
*/
static std::unordered_set<const void*> derived_type_keys;

//! The declarations (and signatures) that own a type parameter type of type_parameter_map, guarded by ctl
/** Kept apart from derived_type_keys: a declaration's own type parameter types do not name it for the decision to
    keep it after its last reference is released (type_owner_derived()), as only its own code and the types derived
    from them use them; they are orphaned and retired when it is destroyed (issue #6419)
*/
static std::unordered_set<const void*> type_param_owner_keys;

//! Records the keys of a new derived-type cache entry; the caller holds ctl
static void noteDerivedTypeKeys(const void* first, const type_vec_t& types) {
    if (first) {
        derived_type_keys.insert(first);
    }
    for (const QoreTypeInfo* t : types) {
        derived_type_keys.insert(t);
    }
}

// rwlock for global type map
static QoreRWLock extern_type_info_map_lock;
static QoreForkRWLock extern_type_info_map_fork_handler(extern_type_info_map_lock, QFLR_LEAF, "external_types");

static void do_maps(qore_type_t t, const char* name, const QoreTypeInfo* typeInfo,
        const QoreTypeInfo* orNothingTypeInfo) {
   str_typeinfo_map[name]                     = typeInfo;
   str_ornothingtypeinfo_map[name]            = orNothingTypeInfo;
   type_typeinfo_map[t]                       = typeInfo;
   type_ornothingtypeinfo_map[t]              = orNothingTypeInfo;
   type_str_map[t]                            = name;
   typeinfo_or_nothing_map[orNothingTypeInfo] = typeInfo;
   set_builtin_or_nothing_type(typeInfo, orNothingTypeInfo);
}

static void do_maps(const QoreTypeInfo* typeInfo, const QoreTypeInfo* orNothingTypeInfo) {
   typeinfo_or_nothing_map[orNothingTypeInfo] = typeInfo;
   set_builtin_or_nothing_type(typeInfo, orNothingTypeInfo);
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
    do_maps(NT_CHAR,            "char", charTypeInfo, charOrNothingTypeInfo);
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
    do_maps(NT_SOFTCHAR,        "softchar", softCharTypeInfo, softCharOrNothingTypeInfo);
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
    // Keep the canonical no-narrow marker when adding/removing NOTHING, including substitution of *T.
    do_maps(autoNoNarrowListTypeInfo, autoNoNarrowListOrNothingTypeInfo);
    do_maps(autoNoNarrowHashTypeInfo, autoNoNarrowHashOrNothingTypeInfo);
    do_maps(softAutoListTypeInfo, softAutoListOrNothingTypeInfo);
}

//! derived types removed from the caches because a type they refer to was destroyed; deleted at library cleanup
static std::vector<QoreTypeInfo*> retired_derived_types;
//! the retirement epoch of each element of retired_derived_types (see qore_type_retire_epoch), guarded by ctl
static std::vector<uint64_t> retired_derived_type_epochs;

std::atomic<uint64_t> qore_type_retire_epoch{1};

//! Adds a retired item and its epoch; an exception (from an allocation) leaves both vectors as they were
template <typename T>
static void push_retired(std::vector<T*>& items, std::vector<uint64_t>& epochs, T* item, uint64_t epoch) {
    if (epochs.size() == epochs.capacity()) {
        epochs.reserve(epochs.empty() ? 16 : epochs.size() * 2);
    }
    items.push_back(item);
    epochs.push_back(epoch);
}

namespace {
//! The registry of the generic caches by the types that their keys name (issue #5765)
class QoreGenericCacheRegistry {
public:
    void insert(QoreGenericTypeCache* cache, const std::vector<const QoreTypeInfo*>& types,
            const std::function<void()>& insert_entry) {
        std::lock_guard<std::mutex> lock(m);
        // the types are registered first: if that fails, no entry is added (a type registered without an entry is
        // only an index entry for nothing); if the entry is added, it is always registered
        std::unordered_set<const QoreTypeInfo*>& cache_types = by_cache[cache];
        for (const QoreTypeInfo* ti : types) {
            if (ti && cache_types.insert(ti).second) {
                by_type[ti].insert(cache);
            }
        }
        updateEmpty();
        insert_entry();
    }

    //! Registers the types of an entry that does not keep its types named (see qore_generic_cache_insert_weak())
    void insertWeak(QoreGenericTypeCache* cache, const std::vector<const QoreTypeInfo*>& types,
            const std::function<void()>& insert_entry) {
        std::lock_guard<std::mutex> lock(m);
        std::unordered_set<const QoreTypeInfo*>& cache_types = weak_by_cache[cache];
        for (const QoreTypeInfo* ti : types) {
            if (ti && cache_types.insert(ti).second) {
                weak_by_type[ti].insert(cache);
            }
        }
        updateEmpty();
        insert_entry();
    }

    void unregister(QoreGenericTypeCache* cache) {
        std::lock_guard<std::mutex> lock(m);
        unregisterIntern(cache, by_cache, by_type);
        unregisterIntern(cache, weak_by_cache, weak_by_type);
        updateEmpty();
    }

    size_t registeredTypeCount() {
        std::lock_guard<std::mutex> lock(m);
        return by_type.size();
    }

    //! Returns true if an entry of a cache names one of the given types
    bool hasType(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
        if (empty.load(std::memory_order_acquire)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m);
        return (ti1 && by_type.count(ti1)) || (ti2 && by_type.count(ti2));
    }

    //! Evicts the entries that name a dead type from every cache that registered one
    /** @param release receives what the caches removed, to be released by the caller after the lock is released
    */
    void evict(const std::set<const QoreTypeInfo*>& dead, qore_generic_cache_release_list_t& release) {
        // every class, hashdecl and enum destruction purges its types: no lock while no cache has an entry
        if (empty.load(std::memory_order_acquire)) {
            return;
        }
        std::lock_guard<std::mutex> lock(m);
        // this runs in destructors: if memory cannot be allocated, the entries are left in the caches (their keys
        // never match a later type), not released
        std::unordered_set<QoreGenericTypeCache*> caches;
        try {
            for (const QoreTypeInfo* ti : dead) {
                for (const type_map_t* tm : {&by_type, &weak_by_type}) {
                    auto t = tm->find(ti);
                    if (t != tm->end()) {
                        caches.insert(t->second.begin(), t->second.end());
                    }
                }
            }
        } catch (std::bad_alloc&) {
            return;
        }
        evictTypes(dead, by_cache, by_type);
        evictTypes(dead, weak_by_cache, weak_by_type);
        for (QoreGenericTypeCache* cache : caches) {
            cache->evictDeadTypes(dead, release);
        }
        updateEmpty();
    }

private:
    typedef std::unordered_map<const QoreTypeInfo*, std::unordered_set<QoreGenericTypeCache*>> type_map_t;
    typedef std::unordered_map<QoreGenericTypeCache*, std::unordered_set<const QoreTypeInfo*>> cache_map_t;

    std::mutex m;
    //! the caches that have an entry whose key names each type
    type_map_t by_type;
    //! the types that each cache registered
    cache_map_t by_cache;
    //! the caches that have an entry that identifies each type without naming it (qore_generic_cache_insert_weak()):
    //! evicted like the others, but they do not keep the declaration of the type (hasType() does not see them)
    type_map_t weak_by_type;
    //! the types that each cache registered weakly
    cache_map_t weak_by_cache;
    //! true while no type is registered
    std::atomic<bool> empty{true};

    //! the caller holds the lock
    void updateEmpty() {
        empty.store(by_type.empty() && weak_by_type.empty(), std::memory_order_release);
    }

    //! the caller holds the lock
    static void unregisterIntern(QoreGenericTypeCache* cache, cache_map_t& cm, type_map_t& tm) {
        auto i = cm.find(cache);
        if (i == cm.end()) {
            return;
        }
        for (const QoreTypeInfo* ti : i->second) {
            auto t = tm.find(ti);
            assert(t != tm.end());
            t->second.erase(cache);
            if (t->second.empty()) {
                tm.erase(t);
            }
        }
        cm.erase(i);
    }

    //! removes the registrations of the dead types; the caller holds the lock
    static void evictTypes(const std::set<const QoreTypeInfo*>& dead, cache_map_t& cm, type_map_t& tm) {
        for (const QoreTypeInfo* ti : dead) {
            auto t = tm.find(ti);
            if (t == tm.end()) {
                continue;
            }
            for (QoreGenericTypeCache* cache : t->second) {
                auto c = cm.find(cache);
                assert(c != cm.end());
                c->second.erase(ti);
            }
            tm.erase(t);
        }
    }
};

//! the generic cache registry; never destroyed, as caches unregister during library cleanup
QoreGenericCacheRegistry& qore_generic_cache_registry() {
    static QoreGenericCacheRegistry* registry = new QoreGenericCacheRegistry;
    return *registry;
}
}

void qore_generic_cache_insert(QoreGenericTypeCache* cache, const std::vector<const QoreTypeInfo*>& types,
        const std::function<void()>& insert) {
    qore_generic_cache_registry().insert(cache, types, insert);
}

void qore_generic_cache_insert_weak(QoreGenericTypeCache* cache, const std::vector<const QoreTypeInfo*>& types,
        const std::function<void()>& insert) {
    qore_generic_cache_registry().insertWeak(cache, types, insert);
}

std::atomic<int64_t> qore_live_generic_specializations{0};
std::atomic<int64_t> qore_live_parameterized_hashdecls{0};

size_t qore_generic_cache_registered_type_count() {
    return qore_generic_cache_registry().registeredTypeCount();
}

void qore_generic_cache_unregister(QoreGenericTypeCache* cache) {
    qore_generic_cache_registry().unregister(cache);
}

//! Purges the derived types; \a dead receives the destroyed types and every derived type retired with them
/** @param retired_out if not null, receives the derived types retired, which the caller orphans and then records
    (qore_record_retired_derived_types()); otherwise they are recorded here, with the epoch taken after they were
    removed from the caches
*/
static void qore_purge_derived_types_intern(const void* owner, std::set<const QoreTypeInfo*>& dead,
        std::vector<QoreTypeInfo*>* retired_out = nullptr) {

    auto any_dead = [&dead](const type_vec_t& types) -> bool {
        for (const QoreTypeInfo* t : types) {
            if (dead.count(t)) {
                return true;
            }
        }
        return false;
    };

    AutoLocker al(ctl);

    // dead holds the destroyed types here (at most two)
    bool keyed = owner && (derived_type_keys.count(owner) || type_param_owner_keys.count(owner));
    for (auto i = dead.begin(), e = dead.end(); !keyed && i != e; ++i) {
        keyed = derived_type_keys.count(*i);
    }
    if (!keyed) {
        return;
    }

    // one pass removes the entries built directly on a dead type or owner and marks what they produced as
    // dead in turn; passes repeat until one removes nothing, which reaches every derived type however deeply
    // it nests (list<list<hash<X>>>, a union containing one, a callable returning one, ...)
    //
    // There is deliberately no qore_check_cancel() in these loops: this runs in the destructor of the type's
    // owner, and stopping part way would leave entries keyed by an address about to be freed
    // the derived types retired here, recorded after the loop (see qore_type_retire_epoch)
    std::vector<QoreTypeInfo*> retired_now;
    bool removed;
    do {
        removed = false;
        auto retire = [&](QoreTypeInfo* ti) {
            retired_now.push_back(ti);
            dead.insert(ti);
            removed = true;
        };
        for (tmap_t* m : {&ch_map, &chon_map, &cl_map, &clon_map, &chr_map, &cr_map, &cron_map, &csl_map,
                &cslon_map}) {
            for (tmap_t::iterator i = m->begin(); i != m->end();) {
                if (dead.count(i->first)) {
                    // the key is not freed yet: a destroyed type is purged before it is freed, and a retired one
                    // is freed only after the threads that can read it have passed its epoch; a retired type
                    // keeps no cached reference type that cr_map no longer holds for it
                    if (m == &cr_map && i->first) {
                        i->first->setComplexReferenceTypeCache(nullptr);
                    }
                    retire(i->second);
                    i = m->erase(i);
                } else {
                    ++i;
                }
            }
        }
        for (union_map_t* m : {&union_map, &union_on_map}) {
            for (union_map_t::iterator i = m->begin(); i != m->end();) {
                if (any_dead(i->first)) {
                    retire(i->second);
                    i = m->erase(i);
                } else {
                    ++i;
                }
            }
        }
        for (complex_code_map_t::iterator i = complex_code_map.begin(); i != complex_code_map.end();) {
            if (dead.count(i->first.returnType) || any_dead(i->first.paramTypes)) {
                retire(i->second);
                i = complex_code_map.erase(i);
            } else {
                ++i;
            }
        }
        for (parameterized_class_map_t::iterator i = parameterized_class_map.begin();
                i != parameterized_class_map.end();) {
            if ((owner && i->first.baseClass == owner) || any_dead(i->first.typeArgs)) {
                retire(i->second);
                i = parameterized_class_map.erase(i);
            } else {
                ++i;
            }
        }
        for (type_parameter_map_t::iterator i = type_parameter_map.begin(); i != type_parameter_map.end();) {
            if (owner && i->first.owner == owner) {
                // the type outlives its declaration (issue #6419)
                i->second->orphan();
                retire(i->second);
                i = type_parameter_map.erase(i);
            } else {
                ++i;
            }
        }
        for (wildcard_type_map_t::iterator i = wildcard_type_map.begin(); i != wildcard_type_map.end();) {
            if (i->first.bound && dead.count(i->first.bound)) {
                retire(i->second);
                i = wildcard_type_map.erase(i);
            } else {
                ++i;
            }
        }
    } while (removed);

    // the dead addresses no longer key any entry; the retired types are never freed before cleanup, so their
    // addresses cannot be reused, but the destroyed owner's and types' addresses can
    if (owner) {
        derived_type_keys.erase(owner);
        type_param_owner_keys.erase(owner);
    }
    for (const QoreTypeInfo* t : dead) {
        derived_type_keys.erase(t);
    }

    if (retired_out) {
        retired_out->swap(retired_now);
    } else if (!retired_now.empty()) {
        uint64_t epoch = qore_type_retire_epoch.fetch_add(1, std::memory_order_seq_cst) + 1;
        for (QoreTypeInfo* ti : retired_now) {
            push_retired(retired_derived_types, retired_derived_type_epochs, ti, epoch);
        }
    }
}

//! The class, hashdecl and enum declarations retired and not freed yet (issue #5790; see QoreTypeOwner), guarded by
//! ctl
static std::vector<QoreTypeOwner*> retired_type_owners;
//! true while retired declarations wait to be freed, read without the lock (see qore_type_retirement_free())
std::atomic<bool> qore_type_retirement_pending{false};
//! true once library cleanup has started: retired declarations are no longer orphaned or freed before it (issue #6419)
static std::atomic<bool> type_retirement_off{false};

void qore_type_retirement_shutdown() {
    type_retirement_off.store(true, std::memory_order_release);
}
//! the number of retired declarations freed before library cleanup (issue #6419)
static std::atomic<int64_t> qore_freed_retired_type_owners{0};
//! true once library cleanup frees the retired declarations: no declaration is retired anymore
/** Read without the lock first: a Program held at static scope can be deleted by a static destructor after library
    cleanup, when the containers above can already have been destroyed
*/
static std::atomic<bool> type_owner_cleanup{false};

//! Returns true if a derived type or a generic cache entry names the types of the declaration; the caller holds ctl
static bool type_owner_derived(const QoreTypeOwner* o) {
    const void* key = o->getTypeOwnerKey();
    const QoreTypeInfo* t1 = o->getTypeOwnerType();
    const QoreTypeInfo* t2 = o->getTypeOwnerOrNothingType();
    if ((key && derived_type_keys.count(key)) || (t1 && derived_type_keys.count(t1))
            || (t2 && derived_type_keys.count(t2))) {
        return true;
    }
    // a generic cache entry is keyed by the types it names (ex: hash<HD<*Foo>>)
    return qore_generic_cache_registry().hasType(t1, t2);
}

#ifdef DEBUG
namespace {
//! the declarations that are not freed, for the assertions of the bookkeeping of the declarations that name each other
struct QoreTypeOwnerRegistry {
    std::mutex m;
    std::set<const QoreTypeOwner*> live;
};

// not destroyed: declarations are freed at library cleanup and at static destruction
QoreTypeOwnerRegistry& type_owner_registry() {
    static QoreTypeOwnerRegistry* registry = new QoreTypeOwnerRegistry;
    return *registry;
}
}

QoreTypeOwner::QoreTypeOwner() {
    QoreTypeOwnerRegistry& r = type_owner_registry();
    std::lock_guard<std::mutex> lock(r.m);
    r.live.insert(this);
}

QoreTypeOwner::~QoreTypeOwner() {
    QoreTypeOwnerRegistry& r = type_owner_registry();
    std::lock_guard<std::mutex> lock(r.m);
    r.live.erase(this);
}

size_t qore_count_live_user_type_owners() {
    QoreTypeOwnerRegistry& r = type_owner_registry();
    std::lock_guard<std::mutex> lock(r.m);
    size_t count = 0;
    for (const QoreTypeOwner* o : r.live) {
        if (!o->isTypeOwnerSystem()) {
            ++count;
        }
    }
    return count;
}

bool qore_type_owner_is_live(const QoreTypeOwner* owner) {
    QoreTypeOwnerRegistry& r = type_owner_registry();
    std::lock_guard<std::mutex> lock(r.m);
    return r.live.count(owner) > 0;
}

namespace {
//! the types that are not freed
struct QoreTypeInfoRegistry {
    std::mutex m;
    std::set<const QoreTypeInfo*> live;
};

// not destroyed: types are freed at library cleanup and at static destruction
QoreTypeInfoRegistry& type_info_registry() {
    static QoreTypeInfoRegistry* registry = new QoreTypeInfoRegistry;
    return *registry;
}
}

void qore_type_info_created(const QoreTypeInfo* ti) {
    QoreTypeInfoRegistry& r = type_info_registry();
    std::lock_guard<std::mutex> lock(r.m);
    r.live.insert(ti);
}

void qore_type_info_destroyed(const QoreTypeInfo* ti) {
    QoreTypeInfoRegistry& r = type_info_registry();
    std::lock_guard<std::mutex> lock(r.m);
    r.live.erase(ti);
}

bool qore_type_info_is_live(const QoreTypeInfo* ti) {
    QoreTypeInfoRegistry& r = type_info_registry();
    std::lock_guard<std::mutex> lock(r.m);
    return r.live.count(ti) > 0;
}
#endif

//! A group of declarations that survived the deletion of their Program together; guarded by ctl (see QoreTypeOwner)
struct QoreTypeOwnerGroup {
    //! the members
    std::vector<QoreTypeOwner*> members;
    //! the number of live members
    size_t live = 0;
    //! the number of holds since the last sweep (type_owner_group_hold())
    size_t holds_since_sweep = 0;
};

//! The groups not collected yet, guarded by ctl
static std::set<QoreTypeOwnerGroup*> type_owner_groups;

//! The declarations whose names are counted (they hold references to the declarations that they name), guarded by ctl
/** Not destroyed: declarations are freed at static destruction too.  Library cleanup frees the declarations that only
    each other keep (delete_qore_types())
*/
static std::set<QoreTypeOwner*>& type_owner_counted_registry() {
    static std::set<QoreTypeOwner*>* registry = new std::set<QoreTypeOwner*>;
    return *registry;
}

//! References the declarations that a declaration names, if they are known, except the members of its group; the
//! caller holds ctl
static void type_owner_count_names(QoreTypeOwner* o) {
    if (o->type_owner_names_counted || !o->type_owner_names_known) {
        return;
    }
    for (QoreTypeOwner* n : o->type_owner_names) {
        if (o->type_owner_group && n->type_owner_group == o->type_owner_group) {
            continue;
        }
        n->refTypeOwner();
        o->type_owner_refs.push_back(n);
    }
    o->type_owner_names_counted = true;
    type_owner_counted_registry().insert(o);
}

//! Makes \a keeper keep \a n (see QoreTypeOwner::type_owner_kept_by); the caller holds ctl
static void type_owner_keep(QoreTypeOwner* keeper, QoreTypeOwner* n) {
    if (n == keeper || std::find(keeper->type_owner_keeps.begin(), keeper->type_owner_keeps.end(), n)
            != keeper->type_owner_keeps.end()) {
        return;
    }
    keeper->type_owner_keeps.push_back(n);
    ++n->type_owner_kept_by;
    n->type_owner_forever_named = true;
}

//! Releases what a declaration keeps; the caller holds ctl
static void type_owner_release_keeps(QoreTypeOwner* keeper) {
    for (QoreTypeOwner* n : keeper->type_owner_keeps) {
        assert(n->type_owner_kept_by);
        if (!--n->type_owner_kept_by) {
            n->type_owner_forever_named = false;
        }
    }
    keeper->type_owner_keeps.clear();
}

//! Marks the declarations that a retired hashdecl names, which are retired, not freed, until it is freed; the caller
//! holds ctl
static void type_owner_mark_forever_names(QoreTypeOwner* o) {
    // transitively: a declaration marked so is retired, and a retired hashdecl keeps what it names in turn; the
    // keeper counts them all, as one that it names can be released before it is
    std::vector<QoreTypeOwner*> work;
    std::set<QoreTypeOwner*> seen;
    work.push_back(o);
    while (!work.empty()) {
        QoreTypeOwner* m = work.back();
        work.pop_back();
        if (!m->type_owner_names_known || !m->keepsTypeOwnerMembers()) {
            continue;
        }
        for (QoreTypeOwner* n : m->type_owner_names) {
            if (n == o || !seen.insert(n).second) {
                continue;
            }
            // a declaration marked already had what it names marked with it, when every one of them was alive: it
            // is not walked again, as one that it names can have been freed since
            bool marked = n->type_owner_forever_named;
            type_owner_keep(o, n);
            if (!marked) {
                work.push_back(n);
            }
        }
    }
}

//! Gives the references to the declarations that a declaration names to \a release_list; the caller holds ctl
/** The caller releases them after the lock is released (type_owner_release())
*/
static void type_owner_uncount_names(QoreTypeOwner* o, std::vector<QoreTypeOwner*>& release_list) {
    if (!o->type_owner_names_counted) {
        return;
    }
    o->type_owner_names_counted = false;
    release_list.insert(release_list.end(), o->type_owner_refs.begin(), o->type_owner_refs.end());
    o->type_owner_refs.clear();
}

//! Returns true if a member of a group that is not retired is referenced other than by its own hold for the group;
//! the caller holds ctl (a retired member names the members that it keeps without references: they are marked)
static bool type_owner_group_referenced(const QoreTypeOwner* m) {
    return !m->type_owner_retired && m->getTypeOwnerReferenceCount() > (m->type_owner_group_hold ? 1 : 0);
}

//! Returns the members of a group that its live members, or the members referenced otherwise, name, transitively,
//! with those roots; the caller holds ctl
static std::set<QoreTypeOwner*> type_owner_group_reach(QoreTypeOwnerGroup* g) {
    std::set<QoreTypeOwner*> reach;
    std::vector<QoreTypeOwner*> work;
    for (QoreTypeOwner* m : g->members) {
        if (m->type_owner_group_live || type_owner_group_referenced(m)) {
            work.push_back(m);
        }
    }
    while (!work.empty()) {
        QoreTypeOwner* m = work.back();
        work.pop_back();
        if (!reach.insert(m).second) {
            continue;
        }
        // a retired declaration keeps what it names until library cleanup itself (type_owner_mark_forever_names())
        if (m->type_owner_retired || !m->type_owner_names_known) {
            continue;
        }
        for (QoreTypeOwner* n : m->type_owner_names) {
            // a declaration that a member of a group names must not be freed while the member exists: its references
            // are held for the member, except those of the members of the group itself
            assert(qore_type_owner_is_live(n));
            if (n->type_owner_group == g) {
                work.push_back(n);
            }
        }
    }
    return reach;
}

//! Releases the members of a group with live members that no live or referenced member names anymore; the caller
//! holds ctl
/** A member that is reached keeps its hold; the others leave the group, and their holds are given to
    \a release_list, to be released by the caller after the lock is released
*/
static void type_owner_group_sweep(QoreTypeOwnerGroup* g, const std::set<QoreTypeOwner*>& reach,
        std::vector<QoreTypeOwner*>& release_list) {
    std::vector<QoreTypeOwner*> members;
    members.swap(g->members);
    for (QoreTypeOwner* m : members) {
        if (reach.count(m)) {
            g->members.push_back(m);
            continue;
        }
        m->type_owner_group = nullptr;
        if (m->type_owner_group_hold) {
            m->type_owner_group_hold = false;
            release_list.push_back(m);
        }
    }
}

//! A member of a group with live members that was released holds a reference to itself for the group; the caller
//! holds ctl
/** The group is swept (type_owner_group_sweep()) when the holds since the last sweep reach half the number of its
    members (at the first hold for a group of up to three members), so the cost of a sweep, which is proportional to
    the size of the group, is paid by as many holds and the cost of a release is constant on average however large the
    group, and a member that no live or referenced member reaches anymore is released after a bounded number of holds
*/
static void type_owner_group_hold(QoreTypeOwnerGroup* g, QoreTypeOwner* owner,
        std::vector<QoreTypeOwner*>& release_list) {
    owner->refTypeOwner();
    owner->type_owner_group_hold = true;
    if (++g->holds_since_sweep < std::max<size_t>(1, g->members.size() / 2)) {
        return;
    }
    g->holds_since_sweep = 0;
    type_owner_group_sweep(g, type_owner_group_reach(g), release_list);
}

//! Collects a group whose last live member was released; the caller holds ctl
/** A member that is referenced again (beyond its hold: ex: by the reflection API) becomes a live member again, and its
    hold is given to \a release_list (the release of that reference decides it again); if there is none, the group is
    dissolved and the holds of its members are given to \a release_list, to be released by the caller after the lock
    is released, which frees the members that nothing references then (or retires the marked ones)
*/
static void type_owner_collect_group(QoreTypeOwnerGroup* g, std::vector<QoreTypeOwner*>& release_list) {
    for (QoreTypeOwner* m : g->members) {
        if (type_owner_group_referenced(m)) {
            m->type_owner_group_live = true;
            ++g->live;
            if (m->type_owner_group_hold) {
                m->type_owner_group_hold = false;
                release_list.push_back(m);
            }
        }
    }
    if (g->live) {
        return;
    }
    for (QoreTypeOwner* m : g->members) {
        m->type_owner_group = nullptr;
        m->type_owner_group_live = false;
        if (m->type_owner_group_hold) {
            m->type_owner_group_hold = false;
            release_list.push_back(m);
        }
    }
    type_owner_groups.erase(g);
    delete g;
}

namespace {
//! The releases of the current thread that wait for the release being made (type_owner_release())
struct QoreTypeOwnerReleaseQueue {
    bool active = false;
    std::vector<QoreTypeOwner*> pending;
};

//! Marks the queue of the thread active while it is drained, also if a release throws
class QoreTypeOwnerReleaseQueueHelper {
public:
    QoreTypeOwnerReleaseQueueHelper(QoreTypeOwnerReleaseQueue& q) : q(q) {
        q.active = true;
    }

    ~QoreTypeOwnerReleaseQueueHelper() {
        q.active = false;
    }

private:
    QoreTypeOwnerReleaseQueue& q;
};
}

static thread_local QoreTypeOwnerReleaseQueue type_owner_release_queue;

//! Releases references to declarations; the caller does not hold ctl
/** A release can free a declaration whose destructor releases others in turn: they are queued and released here, so a
    chain of declarations is freed without recursion
*/
static void type_owner_release(std::vector<QoreTypeOwner*>& release_list) {
    if (release_list.empty()) {
        return;
    }
    QoreTypeOwnerReleaseQueue& q = type_owner_release_queue;
    if (q.active) {
        q.pending.insert(q.pending.end(), release_list.begin(), release_list.end());
        return;
    }
    QoreTypeOwnerReleaseQueueHelper qh(q);
    std::vector<QoreTypeOwner*> work;
    work.swap(release_list);
    while (!work.empty()) {
        for (QoreTypeOwner* o : work) {
            o->derefTypeOwner();
        }
        work.clear();
        work.swap(q.pending);
    }
}

namespace {
//! Returns the declaration of a user (not builtin) class, hashdecl or enum, or nullptr
static QoreTypeOwner* qore_user_class_owner(const QoreClass* qc) {
    if (!qc) {
        return nullptr;
    }
    qore_class_private* p = qore_class_private::get(*const_cast<QoreClass*>(qc));
    return p && !p->type_owner.isTypeOwnerSystem() ? &p->type_owner : nullptr;
}

static QoreTypeOwner* qore_user_hashdecl_owner(const TypedHashDecl* hd) {
    if (!hd) {
        return nullptr;
    }
    typed_hash_decl_private* p = typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hd));
    return !p->type_owner.isTypeOwnerSystem() ? &p->type_owner : nullptr;
}

static QoreTypeOwner* qore_user_enum_owner(const QoreEnumDecl* ed) {
    if (!ed) {
        return nullptr;
    }
    qore_enum_decl_private* p = qore_enum_decl_private::get(*const_cast<QoreEnumDecl*>(ed));
    return !p->type_owner.isTypeOwnerSystem() ? &p->type_owner : nullptr;
}

//! Returns the user declaration whose own type (or or-nothing type) \a ti is, or nullptr
static QoreTypeOwner* qore_user_decl_type_owner(const QoreTypeInfo* ti) {
    if (!ti) {
        return nullptr;
    }
    if (dynamic_cast<const QoreClassTypeInfo*>(ti)) {
        return qore_user_class_owner(ti->getAcceptSpecs()[0].spec.getClass());
    }
    if (dynamic_cast<const QoreHashDeclTypeInfo*>(ti)) {
        return qore_user_hashdecl_owner(ti->getAcceptSpecs()[0].spec.getHashDecl());
    }
    if (dynamic_cast<const QoreEnumTypeInfo*>(ti)) {
        return qore_user_enum_owner(ti->getAcceptSpecs()[0].spec.getEnum());
    }
    return nullptr;
}

//! Orphans retired derived types from the declaration being retired (issue #6419; see lib/QoreTypeTombstone.cpp)
/** Each pointer that a retired derived type has to the declaration being retired or to one of its own types is
    repointed to the declaration's tombstone.  A pointer to another user declaration, which is alive, is left as it is:
    the code that compares declarations by identity (the substitution of a generic class's type parameters, for example)
    must still find it; it is recorded (named), and orphaned when that declaration is retired in turn.  A pointer to a
    declaration being destroyed (skip_key) is left; one that cannot be orphaned (no tombstone can be made) makes its
    declaration unfreeable.  The caller holds no type lock.
*/
class QoreDerivedTypeOrphaner {
public:
    //! \a retiring: the declaration being retired, if any; \a skip_key, \a skip1 and \a skip2: the key and the
    //! types of a declaration being destroyed, whose pointers are left; compared without being read, as the
    //! declaration can be partly destroyed
    DLLLOCAL QoreDerivedTypeOrphaner(QoreTypeOwner* retiring, const void* skip_key = nullptr,
            const QoreTypeInfo* skip1 = nullptr, const QoreTypeInfo* skip2 = nullptr) : retiring(retiring),
            skip_key(skip_key), skip1(skip1), skip2(skip2) {
        // an enum has no key: its declaration is found in its type, which is destroyed after the purge
        if (skip1 && !skip_key && dynamic_cast<const QoreEnumTypeInfo*>(skip1)) {
            skip_enum = skip1->getAcceptSpecs()[0].spec.getEnum();
        }
    }

    //! the declarations that a pointer could not be orphaned from
    std::vector<QoreTypeOwner*> unfreeable;
    //! the other user declarations that the derived types name, with the derived types
    std::vector<std::pair<QoreTypeOwner*, const QoreTypeInfo*>> named;

    DLLLOCAL void orphan(const QoreTypeInfo* ti, int depth = 0) {
        if (!ti || depth > 4) {
            return;
        }
        if (!depth) {
            cur = ti;
        }
        for (const QoreAcceptSpec& as : ti->getAcceptSpecs()) {
            orphanSpec(as.spec);
        }
        for (const QoreReturnSpec& rs : ti->return_vec) {
            orphanSpec(rs.spec);
        }
        if (const QoreParameterizedClassTypeInfo* pti = dynamic_cast<const QoreParameterizedClassTypeInfo*>(ti)) {
            const QoreOrphanablePtr<const QoreClass>& base = pti->getOrphanableBaseClass();
            if (QoreTypeOwner* o = isSkipped(base.get()) ? nullptr : qore_user_class_owner(base.get())) {
                if (isRetiring(o)) {
                    const QoreClass* t = qore_class_tombstone(base.get());
                    if (t) {
                        base.orphan(t);
                    } else {
                        unfreeable.push_back(o);
                    }
                }
            }
            for (const QoreOrphanablePtr<const QoreTypeInfo>& arg : pti->getTypeArgs().getOrphanable()) {
                orphanType(arg);
            }
            // the copy that the deprecated reflection function returns (issue #6936)
            pti->refreshLegacyTypeArgs();
        } else if (const QoreUnionTypeInfo* uti = dynamic_cast<const QoreUnionTypeInfo*>(ti)) {
            for (const QoreOrphanablePtr<const QoreTypeInfo>& m : uti->getOrphanableMemberTypes()) {
                orphanType(m);
            }
            for (const QoreOrphanablePtr<const QoreTypeInfo>& m : uti->getOrphanableExactMembers()) {
                orphanType(m);
            }
        } else if (const QoreComplexCodeTypeInfo* cti = dynamic_cast<const QoreComplexCodeTypeInfo*>(ti)) {
            orphanType(cti->getOrphanableReturnType());
            for (const QoreOrphanablePtr<const QoreTypeInfo>& p : cti->getParamTypes().getOrphanable()) {
                orphanType(p);
            }
            // the copy that the deprecated reflection function returns (issue #6936)
            cti->refreshLegacyParamTypes();
        } else if (const QoreWildcardTypeInfo* wti = dynamic_cast<const QoreWildcardTypeInfo*>(ti)) {
            orphanType(wti->getOrphanableBound());
        } else if (const QoreComplexSoftListTypeInfo* sti = dynamic_cast<const QoreComplexSoftListTypeInfo*>(ti)) {
            // a soft list type made from its accept and return specs (the soft auto list singletons) has no element
            // cell: its specs, walked above, are all that it names (qore#7533)
            if (const QoreOrphanablePtr<const QoreTypeInfo>* cell = sti->getOrphanableElementCell()) {
                orphanType(*cell);
            }
        } else if (const QoreComplexSoftListOrNothingTypeInfo* sti
                = dynamic_cast<const QoreComplexSoftListOrNothingTypeInfo*>(ti)) {
            if (const QoreOrphanablePtr<const QoreTypeInfo>* cell = sti->getOrphanableElementCell()) {
                orphanType(*cell);
            }
        } else if (const QoreComplexReferenceTypeInfo* rti = dynamic_cast<const QoreComplexReferenceTypeInfo*>(ti)) {
            // the hard reference type made on demand and owned by the type names the same element type
            orphan(rti->getCreatedHardReference(), depth + 1);
        }
    }

private:
    QoreTypeOwner* retiring;
    //! the retired derived type being orphaned
    const QoreTypeInfo* cur = nullptr;
    const void* skip_key;
    const QoreTypeInfo* skip1;
    const QoreTypeInfo* skip2;
    const QoreEnumDecl* skip_enum = nullptr;

    DLLLOCAL bool isSkipped(const void* p) const {
        return p && p == skip_key;
    }

    //! Returns true if the pointer to \a o is orphaned; otherwise it is recorded
    DLLLOCAL bool isRetiring(QoreTypeOwner* o) {
        if (o == retiring) {
            return true;
        }
        named.emplace_back(o, cur);
        return false;
    }

    DLLLOCAL bool isSkippedType(const QoreTypeInfo* ti) const {
        return ti && (ti == skip1 || ti == skip2);
    }

    DLLLOCAL const QoreTypeInfo* tombType(const QoreTypeInfo* ti, QoreTypeOwner* o) {
        bool ok;
        const QoreTypeInfo* t = qore_type_tombstone(ti, ok);
        if (!ok || t == ti) {
            unfreeable.push_back(o);
            return nullptr;
        }
        return t;
    }

    DLLLOCAL void orphanType(const QoreOrphanablePtr<const QoreTypeInfo>& p) {
        const QoreTypeInfo* ti = p.get();
        if (isSkippedType(ti)) {
            return;
        }
        QoreTypeOwner* o = qore_user_decl_type_owner(ti);
        if (!o || !isRetiring(o)) {
            return;
        }
        if (const QoreTypeInfo* t = tombType(ti, o)) {
            p.orphan(t);
        }
    }

    DLLLOCAL void orphanSpec(const QoreTypeSpec& spec) {
        switch (spec.getTypeSpec()) {
            case QTS_CLASS: {
                const QoreClass* qc = spec.getClass();
                QoreTypeOwner* o = isSkipped(qc) ? nullptr : qore_user_class_owner(qc);
                if (o && isRetiring(o)) {
                    if (const QoreClass* t = qore_class_tombstone(qc)) {
                        spec.orphanClass(t);
                    } else {
                        unfreeable.push_back(o);
                    }
                }
                break;
            }
            case QTS_HASHDECL: {
                const TypedHashDecl* hd = spec.getHashDecl();
                QoreTypeOwner* o = isSkipped(hd) ? nullptr : qore_user_hashdecl_owner(hd);
                if (o && isRetiring(o)) {
                    if (const TypedHashDecl* t = qore_hashdecl_tombstone(hd)) {
                        spec.orphanHashDecl(t);
                    } else {
                        unfreeable.push_back(o);
                    }
                }
                break;
            }
            case QTS_ENUM: {
                const QoreEnumDecl* ed = spec.getEnum();
                QoreTypeOwner* o = ed && ed == skip_enum ? nullptr : qore_user_enum_owner(ed);
                if (o && isRetiring(o)) {
                    if (const QoreEnumDecl* t = qore_enum_tombstone(ed)) {
                        spec.orphanEnum(t);
                    } else {
                        unfreeable.push_back(o);
                    }
                }
                break;
            }
            case QTS_COMPLEXHASH:
            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
            case QTS_COMPLEXHARDREF:
            case QTS_COMPLEXREF: {
                const QoreTypeInfo* elem = spec.getOrphanableTypeInfo();
                QoreTypeOwner* o = isSkippedType(elem) ? nullptr : qore_user_decl_type_owner(elem);
                if (o && isRetiring(o)) {
                    if (const QoreTypeInfo* t = tombType(elem, o)) {
                        spec.orphanTypeInfo(t);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
};
}

//! The retired derived types that still name a live user declaration, by declaration, guarded by ctl (issue #6419)
/** A retired derived type is orphaned from the declaration whose retirement retired it; the pointers to the other user
    declarations that it names are orphaned when they are retired in turn (see QoreDerivedTypeOrphaner)
*/
static std::map<QoreTypeOwner*, std::set<const QoreTypeInfo*>> retired_derived_naming;

//! Purges the derived types built on a declaration's types and orphans the derived types retired (issue #6419)
/** @param key the declaration's key in the caches (see qore_purge_derived_types())
    @param retiring the declaration being retired, if any: the retired derived types that name it are orphaned from it
    @param destroyed if not null, the declaration is being destroyed, not retired: its own pointers are not orphaned;
    receives the dead types
*/
static void qore_purge_and_orphan_derived_types(const void* key, const QoreTypeInfo* ti1, const QoreTypeInfo* ti2,
        QoreTypeOwner* retiring, std::set<const QoreTypeInfo*>* destroyed) {
    std::set<const QoreTypeInfo*> local_dead;
    std::set<const QoreTypeInfo*>& dead = destroyed ? *destroyed : local_dead;
    if (ti1) {
        dead.insert(ti1);
    }
    if (ti2) {
        dead.insert(ti2);
    }
    // nothing is freed from library cleanup on: the derived types are recorded as before
    bool orphan = !type_owner_cleanup.load(std::memory_order_acquire)
        && !type_retirement_off.load(std::memory_order_acquire);
    std::vector<QoreTypeInfo*> retired;
    qore_purge_derived_types_intern(key, dead, orphan ? &retired : nullptr);
    // the derived types retired before that name the declaration being retired
    std::set<const QoreTypeInfo*> named_before;
    if (orphan && retiring) {
        AutoLocker al(ctl);
        auto i = retired_derived_naming.find(retiring);
        if (i != retired_derived_naming.end()) {
            named_before.swap(i->second);
            retired_derived_naming.erase(i);
        }
    }
    if (!retired.empty() || !named_before.empty()) {
        // the declaration being destroyed, if any, whose pointers are left
        QoreDerivedTypeOrphaner orphaner = destroyed ? QoreDerivedTypeOrphaner(nullptr, key, ti1, ti2)
            : QoreDerivedTypeOrphaner(retiring);
        {
            // the declarations read here are not freed while this thread reads them (see qore_type_retire_epoch)
            QoreTypeEpochEntryHelper epoch_entry;
            for (QoreTypeInfo* ti : retired) {
                orphaner.orphan(ti);
            }
            for (const QoreTypeInfo* ti : named_before) {
                orphaner.orphan(ti);
            }
        }
        AutoLocker al(ctl);
        for (QoreTypeOwner* o : orphaner.unfreeable) {
            o->type_owner_unfreeable = true;
        }
        for (auto& i : orphaner.named) {
            retired_derived_naming[i.first].insert(i.second);
        }
        // after the orphaning stores: a thread that reads this epoch reads the tombstones
        uint64_t epoch = qore_type_retire_epoch.fetch_add(1, std::memory_order_seq_cst) + 1;
        for (QoreTypeInfo* ti : retired) {
            push_retired(retired_derived_types, retired_derived_type_epochs, ti, epoch);
        }
    }
    // issue #5765: the generic caches drop the entries made for the destroyed types; what they removed is released
    // after the registry's lock is released, as releasing it can purge types in turn
    if (!dead.empty()) {
        qore_generic_cache_release_list_t release;
        qore_generic_cache_registry().evict(dead, release);
    }
}

bool qore_type_owner_released(QoreTypeOwner* owner) {
    if (type_owner_cleanup.load(std::memory_order_acquire) || owner->isTypeOwnerSystem()) {
        return false;
    }
    // a retired declaration freed by qore_type_retirement_free(), in this thread
    if (owner->type_owner_freeing) {
        return false;
    }
    std::vector<QoreTypeOwner*> release_list;
    bool rv;
    {
        AutoLocker al(ctl);
        // a reference taken meanwhile in another thread (ex: by the reflection API) decides when it is released
        if (owner->hasTypeOwnerReferences()) {
            return true;
        }
        if (owner->type_owner_retired) {
            // a reference taken to the declaration while it was being retired was released; the retiring thread
            // decides
            return true;
        }
        bool derived = owner->type_owner_forever_named || owner->type_owner_kept_by || type_owner_derived(owner);
        QoreTypeOwnerGroup* g = owner->type_owner_group;
        if (g && owner->type_owner_group_live) {
            owner->type_owner_group_live = false;
            --g->live;
            if (!derived) {
                if (g->live) {
                    // live members can name it: it is kept for the group until a sweep finds that none does
                    type_owner_group_hold(g, owner, release_list);
                    rv = true;
                    goto release;
                }
                // the last live member: the group is collected unless a member is referenced again; this
                // declaration is freed then (it is not referenced: checked above)
                type_owner_collect_group(g, release_list);
                if (owner->type_owner_group_live) {
                    // a reference to it was taken meanwhile in another thread: it is live again, and that reference
                    // decides it when it is released
                    rv = true;
                } else if (owner->type_owner_group) {
                    // a referenced member keeps the group, and this one is kept for it
                    type_owner_group_hold(g, owner, release_list);
                    rv = true;
                } else {
                    rv = false;
                }
                goto release;
            }
        } else if (!derived) {
            // a declaration that other declarations name directly is referenced by them; only a derived type, which
            // is never freed before library cleanup, can name the types of a declaration without a reference
            return false;
        }
        owner->type_owner_retired = true;
        // no other thread frees it while it is retired without the lock
        owner->type_owner_retiring = true;
        // its epoch is taken once the derived types that name it are orphaned (below)
        retired_type_owners.push_back(owner);
        // a retired declaration that keeps its members keeps what they name until it is freed, without references,
        // which would keep them whole: they are retired when they are released; marked now, while the lock decides
        // them
        if (owner->keepsTypeOwnerMembers()) {
            type_owner_mark_forever_names(owner);
        }
    }
    // the caches drop the entries built on the types; the derived types are retired, as their addresses are kept, and
    // orphaned: they name the declaration's tombstone from now on (issue #6419)
    qore_purge_and_orphan_derived_types(owner->getTypeOwnerKey(), owner->getTypeOwnerType(),
        owner->getTypeOwnerOrNothingType(), owner, nullptr);
    owner->retireTypeOwner();
    {
        AutoLocker al(ctl);
        owner->type_owner_retiring = false;
        // no derived type leads to it anymore: a thread that reads the epoch from now on can no longer reach it
        owner->type_owner_retire_epoch = qore_type_retire_epoch.fetch_add(1, std::memory_order_seq_cst) + 1;
        qore_type_retirement_pending.store(true, std::memory_order_relaxed);
        // what it names is marked (above), or it names nothing (a retired class keeps no members)
        type_owner_uncount_names(owner, release_list);
        // the references taken and released by other code from now on never bring its count to zero, so the
        // declaration is decided only at library cleanup
        owner->refTypeOwner();
        owner->type_owner_retirement_ref = true;
        // a group whose last live member was this one is collected; otherwise, the members that only it kept are
        // released
        QoreTypeOwnerGroup* g = owner->type_owner_group;
        if (g && !g->live) {
            type_owner_collect_group(g, release_list);
        }
    }
    rv = true;
release:
    type_owner_release(release_list);
    // no scan here: this thread usually runs Qore code since before the retirement, which holds it back, and the
    // caller can hold locks of the Program being deleted; the declaration is freed by a later scan (see
    // qore_type_retirement_free())
    return rv;
}

void qore_type_owner_destroyed(QoreTypeOwner* owner) {
    if (type_owner_cleanup.load(std::memory_order_acquire)) {
        // at library cleanup (or after it, at static destruction), the declarations that this one referenced are
        // released directly: the lock and the release queue may be gone; no other thread decides then, and the
        // registry is not locked for the same reason (the threads of the library have ended, and no declaration is
        // decided by another thread once type_owner_cleanup is set)
        type_owner_counted_registry().erase(owner);
        owner->type_owner_keeps.clear();
        if (owner->type_owner_names_counted) {
            owner->type_owner_names_counted = false;
            std::vector<QoreTypeOwner*> refs;
            refs.swap(owner->type_owner_refs);
            for (QoreTypeOwner* n : refs) {
                n->derefTypeOwner();
            }
        }
        // the binary module declarations that it named (qore#7533)
        std::vector<QoreTypeOwner*> mod_refs;
        mod_refs.swap(owner->type_owner_module_refs);
        for (QoreTypeOwner* n : mod_refs) {
            n->derefTypeOwner();
        }
        return;
    }
    std::vector<QoreTypeOwner*> release_list;
    // the binary module declarations that it named (qore#7533), released after the lock is released
    std::vector<QoreTypeOwner*> mod_refs;
    {
        AutoLocker al(ctl);
        type_owner_counted_registry().erase(owner);
        type_owner_release_keeps(owner);
        retired_derived_naming.erase(owner);
        type_owner_uncount_names(owner, release_list);
        mod_refs.swap(owner->type_owner_module_refs);
        // a member of a group is freed only when the group has been collected, unless it is deleted with its class
        // object (qore_class_private::deref() with in_del): it leaves its group then
        QoreTypeOwnerGroup* g = owner->type_owner_group;
        if (g) {
            auto i = std::find(g->members.begin(), g->members.end(), owner);
            assert(i != g->members.end());
            g->members.erase(i);
            if (owner->type_owner_group_live) {
                --g->live;
            }
            owner->type_owner_group = nullptr;
            if (!g->live) {
                type_owner_collect_group(g, release_list);
            }
        }
    }
    type_owner_release(release_list);
    for (QoreTypeOwner* n : mod_refs) {
        n->derefTypeOwner();
    }
}

//! Returns the declaration (class, hashdecl or enum) whose own type or or-nothing type \a ti is, or nullptr
static QoreTypeOwner* type_owner_of_decl_type(const QoreTypeInfo* ti) {
    if (dynamic_cast<const QoreClassTypeInfo*>(ti)) {
        const QoreClass* qc = QoreTypeInfo::getReturnClass(ti);
        if (qc) {
            qore_class_private* priv = qore_class_private::get(*const_cast<QoreClass*>(qc));
            if (priv) {
                return &priv->type_owner;
            }
        }
    } else if (dynamic_cast<const QoreHashDeclTypeInfo*>(ti)) {
        const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti);
        if (hd) {
            return &typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hd))->type_owner;
        }
    } else if (dynamic_cast<const QoreEnumTypeInfo*>(ti)) {
        const QoreEnumDecl* ed = QoreTypeInfo::getReturnEnum(ti);
        if (ed) {
            return &qore_enum_decl_private::get(*const_cast<QoreEnumDecl*>(ed))->type_owner;
        }
    }
    return nullptr;
}

namespace {
//! Collects the binary module declarations that a type names, directly or through the types that it is made of
/** A compound type (list<ModEnum>, hash<string, hash<ModHd>>, a union, a parameterized class, a code type) names the
    declarations of its element, member, argument, parameter and return types; the walk follows the types as
    QoreDerivedTypeOrphaner does, and not the declarations that it finds (what a declaration names is its own), nor
    type parameters, which belong to the declaration that declares them (qore#7533)
*/
class QoreModuleTypeOwnerCollector {
public:
    DLLLOCAL QoreModuleTypeOwnerCollector(std::vector<QoreTypeOwner*>& names) : names(names) {
    }

    DLLLOCAL void add(const QoreTypeInfo* ti) {
        // every type is walked once: a type can be reached by several paths
        if (!ti || !visited.insert(ti).second) {
            return;
        }
        if (QoreTypeOwner* o = type_owner_of_decl_type(ti)) {
            addOwner(o);
            return;
        }
        for (const QoreAcceptSpec& as : ti->getAcceptSpecs()) {
            addSpec(as.spec);
        }
        for (const QoreReturnSpec& rs : ti->return_vec) {
            addSpec(rs.spec);
        }
        // the orphanable views, which QoreDerivedTypeOrphaner walks, not the legacy copies kept for the deprecated
        // reflection functions (qore#7533)
        if (const QoreParameterizedClassTypeInfo* pti = dynamic_cast<const QoreParameterizedClassTypeInfo*>(ti)) {
            addClass(pti->getOrphanableBaseClass().get());
            for (const QoreOrphanablePtr<const QoreTypeInfo>& arg : pti->getTypeArgs().getOrphanable()) {
                add(arg.get());
            }
        } else if (const QoreUnionTypeInfo* uti = dynamic_cast<const QoreUnionTypeInfo*>(ti)) {
            for (const QoreOrphanablePtr<const QoreTypeInfo>& m : uti->getOrphanableMemberTypes()) {
                add(m.get());
            }
            for (const QoreOrphanablePtr<const QoreTypeInfo>& m : uti->getOrphanableExactMembers()) {
                add(m.get());
            }
        } else if (const QoreComplexCodeTypeInfo* cti = dynamic_cast<const QoreComplexCodeTypeInfo*>(ti)) {
            add(cti->getOrphanableReturnType().get());
            for (const QoreOrphanablePtr<const QoreTypeInfo>& p : cti->getParamTypes().getOrphanable()) {
                add(p.get());
            }
        } else if (const QoreWildcardTypeInfo* wti = dynamic_cast<const QoreWildcardTypeInfo*>(ti)) {
            add(wti->getOrphanableBound().get());
        } else if (const QoreComplexSoftListTypeInfo* sti = dynamic_cast<const QoreComplexSoftListTypeInfo*>(ti)) {
            // a soft list type made from its accept and return specs has no element cell (qore#7533)
            if (const QoreOrphanablePtr<const QoreTypeInfo>* cell = sti->getOrphanableElementCell()) {
                add(cell->get());
            }
        } else if (const QoreComplexSoftListOrNothingTypeInfo* sti
                = dynamic_cast<const QoreComplexSoftListOrNothingTypeInfo*>(ti)) {
            if (const QoreOrphanablePtr<const QoreTypeInfo>* cell = sti->getOrphanableElementCell()) {
                add(cell->get());
            }
        }
    }

private:
    std::vector<QoreTypeOwner*>& names;
    std::set<const QoreTypeInfo*> visited;

    DLLLOCAL void addOwner(QoreTypeOwner* o) {
        if (o->isTypeOwnerModule() && std::find(names.begin(), names.end(), o) == names.end()) {
            names.push_back(o);
        }
    }

    DLLLOCAL void addClass(const QoreClass* qc) {
        if (!qc) {
            return;
        }
        qore_class_private* priv = qore_class_private::get(*const_cast<QoreClass*>(qc));
        if (priv) {
            addOwner(&priv->type_owner);
        }
    }

    DLLLOCAL void addSpec(const QoreTypeSpec& spec) {
        switch (spec.getTypeSpec()) {
            case QTS_CLASS:
                addClass(spec.getClass());
                break;
            case QTS_HASHDECL:
                if (const TypedHashDecl* hd = spec.getHashDecl()) {
                    addOwner(&typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hd))->type_owner);
                }
                break;
            case QTS_ENUM:
                if (const QoreEnumDecl* ed = spec.getEnum()) {
                    addOwner(&qore_enum_decl_private::get(*const_cast<QoreEnumDecl*>(ed))->type_owner);
                }
                break;
            case QTS_COMPLEXHASH:
            case QTS_COMPLEXLIST:
            case QTS_COMPLEXSOFTLIST:
            case QTS_COMPLEXHARDREF:
            case QTS_COMPLEXREF:
            case QTS_COMPLEXBUFFER:
            case QTS_PARAMCLASS:
            case QTS_WILDCARD:
                add(spec.getOrphanableTypeInfo());
                break;
            default:
                break;
        }
    }
};
}

void qore_type_owner_add_module_names(std::vector<QoreTypeOwner*>& names, const QoreTypeInfo* ti) {
    if (!ti) {
        return;
    }
    assert(qore_type_info_is_live(ti));
    QoreModuleTypeOwnerCollector(names).add(ti);
}

void qore_type_owner_add_module_class_names(std::vector<QoreTypeOwner*>& names,
        qore_type_owner_module_names_cache_t* cache) {
    // only the declarations listed when called are walked: what a declaration that they name names is not
    std::vector<QoreTypeOwner*> sig_names;
    for (size_t i = 0, e = names.size(); i < e; ++i) {
        const QoreTypeOwner* o = names[i];
        assert(o->isTypeOwnerModule());
        const std::vector<QoreTypeOwner*>* found;
        if (cache) {
            qore_type_owner_module_names_cache_t::iterator ci = cache->find(o);
            if (ci == cache->end()) {
                std::vector<QoreTypeOwner*> v;
                o->getTypeOwnerModuleSignatureNames(v);
                ci = cache->emplace(o, std::move(v)).first;
            }
            found = &ci->second;
        } else {
            sig_names.clear();
            o->getTypeOwnerModuleSignatureNames(sig_names);
            found = &sig_names;
        }
        for (QoreTypeOwner* n : *found) {
            if (std::find(names.begin(), names.end(), n) == names.end()) {
                names.push_back(n);
            }
        }
    }
}

void qore_type_owner_add_name(std::vector<QoreTypeOwner*>& names, const QoreTypeInfo* ti, const QoreTypeOwner* self,
        bool module_names) {
    if (!ti) {
        return;
    }
    // a declaration names the types of its members by pointer: the owner of a type that it names must outlive it, or
    // be kept by a reference or retired (QoreTypeOwnerBatch)
    assert(qore_type_info_is_live(ti));
    QoreTypeOwner* o = type_owner_of_decl_type(ti);
    if (o && !o->isTypeOwnerSystem()) {
        if (o != self) {
            names.push_back(o);
        }
        return;
    }
    // the binary module declarations that it names, directly or through a compound type (qore#7533)
    if (module_names) {
        QoreModuleTypeOwnerCollector(names).add(ti);
    }
}

QoreTypeOwnerBatch::QoreTypeOwnerBatch(qore_ns_private& ns) {
    // after library cleanup, declarations are freed when their last reference is released
    if (type_owner_cleanup.load(std::memory_order_acquire)) {
        return;
    }
    std::vector<QoreTypeOwner*> all;
    ns.getTypeOwners(all);
    init(all);
}

QoreTypeOwnerBatch::QoreTypeOwnerBatch(const std::vector<QoreTypeOwner*>& all) {
    if (type_owner_cleanup.load(std::memory_order_acquire)) {
        return;
    }
    init(all);
}

void QoreTypeOwnerBatch::init(const std::vector<QoreTypeOwner*>& all) {
    std::set<QoreTypeOwner*> seen;
    owners.reserve(all.size());
    for (QoreTypeOwner* o : all) {
        if (seen.insert(o).second) {
            o->refTypeOwner();
            owners.push_back(o);
        }
    }
    // what each declaration names is taken now, while every type is valid; a declaration whose names are counted
    // (one that survived another Program sharing it) keeps them
    AutoLocker al(ctl);
    std::vector<QoreTypeOwner*> names;
    // what each binary module class named by the declarations names in its own types, walked once (qore#7533)
    qore_type_owner_module_names_cache_t module_class_names;
    for (QoreTypeOwner* o : owners) {
        bool user = !o->type_owner_names_counted;
        bool module = !o->type_owner_module_refs_taken;
        if (!user && !module) {
            continue;
        }
        names.clear();
        o->getTypeOwnerNames(names);
        if (user) {
            o->type_owner_names.clear();
            for (QoreTypeOwner* n : names) {
                if (!n->isTypeOwnerSystem()) {
                    o->type_owner_names.push_back(n);
                }
            }
            o->type_owner_names_known = true;
        }
        if (module) {
            // the binary module declarations that it names are not decided with the declarations of Programs: they
            // are referenced until it is destroyed, as their module can be deleted before it; a hashdecl copy, which
            // references them itself, does not list them (qore#7533)
            assert(o->type_owner_module_refs.empty());
            std::vector<QoreTypeOwner*> mod_names;
            for (QoreTypeOwner* n : names) {
                if (n->isTypeOwnerModule()
                        && std::find(mod_names.begin(), mod_names.end(), n) == mod_names.end()) {
                    mod_names.push_back(n);
                }
            }
            // a class's type is shared with its copies, so the declaration names a module's original class, whose
            // member and signature types the code of its Program reads (qore#7533)
            qore_type_owner_add_module_class_names(mod_names, &module_class_names);
            // nothing is thrown from here on
            for (QoreTypeOwner* n : mod_names) {
                n->refTypeOwner();
            }
            o->type_owner_module_refs.swap(mod_names);
            o->type_owner_module_refs_taken = true;
        }
    }
}

QoreTypeOwnerBatch::~QoreTypeOwnerBatch() {
    if (owners.empty()) {
        return;
    }
    // the tombstones of the declarations that derived types name are made now, while this batch keeps every
    // declaration of the Program: a tombstone is made from what the declaration names (the bounds of its type
    // parameters, for example), which can be freed before the declaration is retired (issue #6419)
    if (!type_owner_cleanup.load(std::memory_order_acquire) && !type_retirement_off.load(std::memory_order_acquire)) {
        // and the concrete declarations of the Program's generic hashdecls (HD<X>), which their caches hold and
        // release when the generic hashdecl is destroyed (referenced here while their tombstones are made)
        std::vector<QoreTypeOwner*> concrete;
        for (QoreTypeOwner* o : owners) {
            o->getTypeOwnerCachedDecls(concrete);
        }
        std::vector<QoreTypeOwner*> named;
        {
            AutoLocker al(ctl);
            for (std::vector<QoreTypeOwner*>* v : {&owners, &concrete}) {
                for (QoreTypeOwner* o : *v) {
                    if (o->type_owner_forever_named || o->type_owner_kept_by || type_owner_derived(o)) {
                        named.push_back(o);
                    }
                }
            }
        }
        if (!named.empty()) {
            QoreTypeEpochEntryHelper epoch_entry;
            for (QoreTypeOwner* o : named) {
                qore_type_owner_make_tombstone(o);
            }
        }
        std::vector<QoreTypeOwner*> release_concrete;
        release_concrete.swap(concrete);
        type_owner_release(release_concrete);
    }
    std::vector<QoreTypeOwner*> release_list;
    {
        AutoLocker al(ctl);
        // the declarations that something other than this batch still references survive the Program, and so do
        // the hashdecls retired below (derived types name their types), which keep their members; each one keeps
        // what it names with a reference, and a declaration of the Program kept so survives in turn; the others are
        // destroyed together and do not keep each other
        std::set<QoreTypeOwner*> batch(owners.begin(), owners.end());
        std::set<QoreTypeOwner*> survivors;
        std::vector<QoreTypeOwner*> work;
        for (QoreTypeOwner* o : owners) {
            if (o->getTypeOwnerReferenceCount() > 1) {
                survivors.insert(o);
                work.push_back(o);
            } else if (o->keepsTypeOwnerMembers() && (o->type_owner_forever_named || type_owner_derived(o))) {
                // a hashdecl retired below keeps what it names until library cleanup
                type_owner_mark_forever_names(o);
            }
        }

        while (!work.empty()) {
            QoreTypeOwner* o = work.back();
            work.pop_back();
            if (o->type_owner_names_counted) {
                continue;
            }
            for (QoreTypeOwner* n : o->type_owner_names) {
                if (batch.count(n) && survivors.insert(n).second) {
                    work.push_back(n);
                }
            }
        }
        // the survivors whose names are not counted yet (declarations that survived another Program sharing them
        // already belong to it) and that name or are named by another one form a group, in which they name each
        // other without references
        std::set<QoreTypeOwner*> linked;
        for (QoreTypeOwner* o : survivors) {
            if (o->type_owner_names_counted || o->type_owner_group || !o->type_owner_names_known) {
                continue;
            }
            for (QoreTypeOwner* n : o->type_owner_names) {
                if (n != o && survivors.count(n) && !n->type_owner_names_counted && !n->type_owner_group) {
                    linked.insert(o);
                    linked.insert(n);
                }
            }
        }
        QoreTypeOwnerGroup* g = nullptr;
        for (QoreTypeOwner* o : linked) {
            if (!g) {
                g = new QoreTypeOwnerGroup;
                type_owner_groups.insert(g);
            }
            g->members.push_back(o);
            o->type_owner_group = g;
            o->type_owner_group_live = true;
            ++g->live;
        }
        for (QoreTypeOwner* o : survivors) {
            type_owner_count_names(o);
        }
        // a declaration destroyed with the Program no longer keeps what it named as a survivor of another Program;
        // one that is a member of a group of another Program's survivors is released below like the others, but the
        // group can keep it (type_owner_group_hold()) with the names that it lists: it keeps its references to them,
        // which it gives up when it is freed (qore_type_owner_destroyed()) or retired, as the names that a group
        // sweeps are read while it exists
        for (QoreTypeOwner* o : owners) {
            if (!survivors.count(o) && !o->type_owner_group) {
                type_owner_uncount_names(o, release_list);
            }
        }
    }
    type_owner_release(release_list);
    for (QoreTypeOwner* o : owners) {
        o->derefTypeOwner();
    }
}

//! true while this thread runs qore_type_retirement_free(): the frees it makes can retire and release declarations,
//! which call it again
static thread_local bool qore_type_retirement_freeing = false;

size_t qore_type_retirement_free() {
    if (!qore_type_retirement_pending.load(std::memory_order_relaxed) || qore_type_retirement_freeing
            || type_owner_cleanup.load(std::memory_order_acquire)
            || type_retirement_off.load(std::memory_order_acquire)) {
        return 0;
    }
    qore_type_retirement_freeing = true;
    size_t freed = 0;
    // a free releases what the declaration keeps, which can make another one freeable: repeated while it does
    while (true) {
        // the candidates are read before the threads are scanned (see qore_type_retirement_info())
        std::vector<std::pair<QoreTypeOwner*, uint64_t>> candidates;
        {
            AutoLocker al(ctl);
            for (QoreTypeOwner* o : retired_type_owners) {
                // a member of a group of survivors is named by the other members without references: it is freed
                // once the group is collected
                if (o->type_owner_retire_epoch && !o->type_owner_retiring && !o->type_owner_unfreeable
                        && !o->type_owner_kept_by && !o->type_owner_group) {
                    candidates.emplace_back(o, o->type_owner_retire_epoch);
                }
            }
            if (retired_type_owners.empty()) {
                qore_type_retirement_pending.store(false, std::memory_order_relaxed);
            }
        }
        if (candidates.empty()) {
            break;
        }
        uint64_t min_epoch = qore_type_retirement_scan(0, nullptr);
        std::vector<QoreTypeOwner*> to_free;
        {
            AutoLocker al(ctl);
            for (auto& c : candidates) {
                QoreTypeOwner* o = c.first;
                if (min_epoch && min_epoch < c.second) {
                    continue;
                }
                auto i = std::find(retired_type_owners.begin(), retired_type_owners.end(), o);
                // only the retirement holds a reference: no thread can reach it anymore, so none can take one
                if (i == retired_type_owners.end() || o->type_owner_kept_by || o->type_owner_unfreeable
                        || o->type_owner_group || !o->type_owner_retirement_ref
                        || o->getTypeOwnerReferenceCount() != 1) {
                    continue;
                }
                retired_type_owners.erase(i);
                o->type_owner_freeing = true;
                o->type_owner_retirement_ref = false;
                type_owner_release_keeps(o);
                to_free.push_back(o);
            }
            if (retired_type_owners.empty()) {
                qore_type_retirement_pending.store(false, std::memory_order_relaxed);
            }
        }
        if (to_free.empty()) {
            break;
        }
        for (QoreTypeOwner* o : to_free) {
            // the last reference: qore_type_owner_released() returns false for it, and it is freed
            o->derefTypeOwner();
        }
        freed += to_free.size();
        qore_freed_retired_type_owners.fetch_add(to_free.size(), std::memory_order_relaxed);
    }
    qore_type_retirement_freeing = false;
    return freed;
}

//! Adds the user declarations that a type names to \a owners
static void type_owner_collect_named(std::vector<QoreTypeOwner*>& owners, const QoreTypeInfo* ti, int depth) {
    if (!ti || depth > 64) {
        return;
    }
    if (QoreTypeOwner* o = qore_user_decl_type_owner(ti)) {
        owners.push_back(o);
        return;
    }
    for (const QoreAcceptSpec& as : ti->getAcceptSpecs()) {
        const QoreTypeSpec& spec = as.spec;
        if (QoreTypeOwner* o = qore_user_class_owner(spec.getClass())) {
            owners.push_back(o);
        } else if (QoreTypeOwner* o = qore_user_hashdecl_owner(spec.getHashDecl())) {
            owners.push_back(o);
        } else if (QoreTypeOwner* o = qore_user_enum_owner(spec.getEnum())) {
            owners.push_back(o);
        } else if (const QoreParameterizedClassTypeInfo* pti = spec.getParameterizedClassTypeInfo()) {
            if (QoreTypeOwner* o = qore_user_class_owner(pti->getBaseClass())) {
                owners.push_back(o);
            }
            for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
                type_owner_collect_named(owners, arg, depth + 1);
            }
        } else if (const QoreWildcardTypeInfo* wti = spec.getWildcardTypeInfo()) {
            type_owner_collect_named(owners, wti->getBound(), depth + 1);
        } else if (const QoreTypeInfo* elem = spec.getElementType()) {
            type_owner_collect_named(owners, elem, depth + 1);
        } else if (const QoreTypeInfo* ref = spec.getComplexReference()) {
            type_owner_collect_named(owners, ref, depth + 1);
        }
    }
    if (const QoreUnionTypeInfo* uti = dynamic_cast<const QoreUnionTypeInfo*>(ti)) {
        for (const QoreTypeInfo* m : uti->getMemberTypes()) {
            type_owner_collect_named(owners, m, depth + 1);
        }
    } else if (const QoreComplexCodeTypeInfo* cti = dynamic_cast<const QoreComplexCodeTypeInfo*>(ti)) {
        type_owner_collect_named(owners, cti->getReturnType(), depth + 1);
        for (const QoreTypeInfo* p : cti->getParamTypes()) {
            type_owner_collect_named(owners, p, depth + 1);
        }
    }
}

void qore_type_owner_keep_type_args(QoreTypeOwner* keeper, const std::vector<const QoreTypeInfo*>& args) {
    std::vector<QoreTypeOwner*> owners;
    for (const QoreTypeInfo* arg : args) {
        type_owner_collect_named(owners, arg, 0);
    }
    if (owners.empty() || type_owner_cleanup.load(std::memory_order_acquire)) {
        return;
    }
    AutoLocker al(ctl);
    for (QoreTypeOwner* o : owners) {
        type_owner_keep(keeper, o);
    }
}

void qore_type_owner_release_kept(QoreTypeOwner* keeper) {
    if (type_owner_cleanup.load(std::memory_order_acquire)) {
        keeper->type_owner_keeps.clear();
        return;
    }
    AutoLocker al(ctl);
    type_owner_release_keeps(keeper);
}

size_t qore_retired_type_owner_count() {
    AutoLocker al(ctl);
    return retired_type_owners.size();
}

void qore_type_retirement_info(QoreTypeRetirementInfo& info) {
    info = QoreTypeRetirementInfo();
    std::vector<uint64_t> owner_epochs, derived_epochs;
    {
        AutoLocker al(ctl);
        owner_epochs.reserve(retired_type_owners.size());
        for (QoreTypeOwner* o : retired_type_owners) {
            // 0 while its derived types are orphaned: pending, not freeable
            owner_epochs.push_back(o->type_owner_retire_epoch);
        }
        derived_epochs = retired_derived_type_epochs;
    }
    info.freed_declarations = qore_freed_retired_type_owners.load(std::memory_order_relaxed);
    qore_type_tombstone_counts(info.tombstone_classes, info.tombstone_hashdecls, info.tombstone_enums);
    // the threads are scanned after the items are read: a thread that entered Qore code since then published an epoch
    // after theirs, which cannot read them (see qore_type_retire_epoch)
    info.min_active_epoch = qore_type_retirement_scan(0, nullptr);
    info.epoch = qore_type_retire_epoch.load(std::memory_order_seq_cst);
    auto count = [&info](const std::vector<uint64_t>& epochs, size_t& pending, size_t& freeable) {
        pending = epochs.size();
        for (uint64_t e : epochs) {
            if (!e) {
                continue;
            }
            if (!info.oldest_pending_epoch || e < info.oldest_pending_epoch) {
                info.oldest_pending_epoch = e;
            }
            if (e > info.newest_pending_epoch) {
                info.newest_pending_epoch = e;
            }
            if (!info.min_active_epoch || info.min_active_epoch >= e) {
                ++freeable;
            }
        }
    };
    count(owner_epochs, info.pending_declarations, info.freeable_declarations);
    count(derived_epochs, info.pending_derived_types, info.freeable_derived_types);
}

size_t qore_type_owner_group_count() {
    AutoLocker al(ctl);
    return type_owner_groups.size();
}

void qore_type_retention_breakdown(std::map<std::string, int64_t>& out) {
    AutoLocker al(ctl);
    // the declarations kept until library cleanup, by kind
    for (const QoreTypeOwner* o : retired_type_owners) {
        ++out[std::string("retired declaration: ") + typeid(*o).name()];
    }
    // the derived types removed from the caches and kept until library cleanup, by kind
    for (const QoreTypeInfo* ti : retired_derived_types) {
        ++out[std::string("retired derived type: ") + typeid(*ti).name()];
    }
    // the live entries of the derived type caches
    size_t complex = 0;
    for (tmap_t* m : {&ch_map, &chon_map, &cl_map, &clon_map, &chr_map, &cr_map, &cron_map, &csl_map, &cslon_map}) {
        complex += m->size();
    }
    out["cache: complex types"] = complex;
    out["cache: union types"] = union_map.size() + union_on_map.size();
    out["cache: code types"] = complex_code_map.size();
    out["cache: parameterized class types"] = parameterized_class_map.size();
    out["cache: type parameter types"] = type_parameter_map.size();
    out["cache: type parameter owners"] = type_param_owner_keys.size();
    out["cache: wildcard types"] = wildcard_type_map.size();
    out["cache: derived type keys"] = derived_type_keys.size();
    out["generic cache registry types"] = qore_generic_cache_registry().registeredTypeCount();
    out["live generic specializations"] = qore_live_generic_specializations.load();
    out["live parameterized hashdecls"] = qore_live_parameterized_hashdecls.load();
}

void qore_purge_derived_types(const void* owner, const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
    std::set<const QoreTypeInfo*> dead;
    if (ti1) {
        dead.insert(ti1);
    }
    if (ti2) {
        dead.insert(ti2);
    }
    if (!owner && dead.empty()) {
        return;
    }
    // the destroyed declaration's own pointers are not orphaned: it is destroyed with them (issue #6419)
    qore_purge_and_orphan_derived_types(owner, ti1, ti2, nullptr, &dead);
}

//! Finds the survivors of deleted Programs that only each other keep, with the references that they hold for their names
//! and the others that they hold (base classes, import sources, parents of hashdecls); the caller holds ctl
/** Nothing else can free such a cycle: each declaration is referenced by another one, and no value references any of
    them.  A declaration that is referenced beyond the references that the declarations of the registry hold, or that
    is held or named by a declaration that is, is not found.  A reference that a declaration does not list counts as
    held from outside, which keeps the declaration and what it holds
*/
static void type_owner_find_cyclic_survivors(std::vector<QoreTypeOwner*>& garbage) {
    std::set<QoreTypeOwner*> counted;
    for (QoreTypeOwner* o : type_owner_counted_registry()) {
        // a declaration without a reference is being destroyed (its destructor has not reached the registry yet)
        if (o->type_owner_names_counted && !o->type_owner_retired && o->getTypeOwnerReferenceCount() > 0) {
            counted.insert(o);
        }
    }
    // the references that each declaration holds on declarations of the set
    std::map<QoreTypeOwner*, std::vector<QoreTypeOwner*>> held;
    std::map<QoreTypeOwner*, int> internal;
    for (QoreTypeOwner* o : counted) {
        std::vector<QoreTypeOwner*> refs(o->type_owner_refs);
        o->getTypeOwnerHeldRefs(refs);
        std::vector<QoreTypeOwner*>& in_set = held[o];
        for (QoreTypeOwner* n : refs) {
            if (counted.find(n) != counted.end()) {
                in_set.push_back(n);
                ++internal[n];
            }
        }
    }
    std::set<QoreTypeOwner*> keep;
    std::vector<QoreTypeOwner*> work;
    for (QoreTypeOwner* o : counted) {
        auto i = internal.find(o);
        if (o->getTypeOwnerReferenceCount() > (i == internal.end() ? 0 : i->second)) {
            keep.insert(o);
            work.push_back(o);
        }
    }
    while (!work.empty()) {
        QoreTypeOwner* m = work.back();
        work.pop_back();
        for (QoreTypeOwner* n : held[m]) {
            if (keep.insert(n).second) {
                work.push_back(n);
            }
        }
        // a declaration that a kept one names must not be freed either, whether or not a reference is held for it
        // (the pointers are only compared with the set: a name can be a freed declaration)
        for (QoreTypeOwner* n : m->type_owner_names) {
            if (counted.find(n) != counted.end() && keep.insert(n).second) {
                work.push_back(n);
            }
        }
    }
    for (QoreTypeOwner* o : counted) {
        if (keep.find(o) == keep.end()) {
            garbage.push_back(o);
        }
    }
}

size_t qore_cyclic_type_owner_count() {
    std::vector<QoreTypeOwner*> garbage;
    AutoLocker al(ctl);
    type_owner_find_cyclic_survivors(garbage);
    return garbage.size();
}

namespace {
//! Releases the references to declarations that it holds when it goes out of scope, also if an exception is thrown
class QoreTypeOwnerReleaseGuard {
public:
    QoreTypeOwnerReleaseGuard() = default;
    QoreTypeOwnerReleaseGuard(const QoreTypeOwnerReleaseGuard&) = delete;
    QoreTypeOwnerReleaseGuard& operator=(const QoreTypeOwnerReleaseGuard&) = delete;

    ~QoreTypeOwnerReleaseGuard() {
        // a release can free declarations, whose destructors do not throw
        for (QoreTypeOwner* o : refs) {
            o->derefTypeOwner();
        }
    }

    //! the references to release, in the order of the release
    std::vector<QoreTypeOwner*> refs;
};
}

void delete_qore_types() {
    // the retired class, hashdecl and enum declarations are freed first, as their destructors purge their types
    // from the caches deleted below; from now on, a declaration whose last reference is released is freed
    {
        std::vector<QoreTypeOwner*> owners;
        std::vector<QoreTypeOwner*> holds;
        {
            AutoLocker al(ctl);
            type_owner_cleanup.store(true, std::memory_order_release);
            owners.swap(retired_type_owners);
            qore_type_retirement_pending.store(false, std::memory_order_relaxed);
            // the groups not collected yet are dissolved, and the holds of their members released below
            for (QoreTypeOwnerGroup* g : type_owner_groups) {
                for (QoreTypeOwner* m : g->members) {
                    m->type_owner_group = nullptr;
                    m->type_owner_group_live = false;
                    if (m->type_owner_group_hold) {
                        m->type_owner_group_hold = false;
                        holds.push_back(m);
                    }
                }
                delete g;
            }
            type_owner_groups.clear();
        }
        for (QoreTypeOwner* o : holds) {
            o->derefTypeOwner();
        }
        for (QoreTypeOwner* o : owners) {
            // the retirement reference is released: a declaration that is still referenced otherwise (ex: by a
            // reflection object of a Program deleted later) is freed when its last reference is released, as the
            // cleanup flag is set
            if (o->type_owner_retirement_ref) {
                o->type_owner_retirement_ref = false;
                o->derefTypeOwner();
            } else if (!o->hasTypeOwnerReferences() && !o->type_owner_retiring) {
                // one that another thread is retiring is freed when its last reference is released
                o->freeTypeOwner();
            }
        }
    }

    // the survivors of deleted Programs that only each other keep (a cycle of declarations that name each other, or
    // hold each other as base classes or sources) are freed now: they are in no list, and nothing else releases them
    // (the Program of a module that they belong to is freed with them); no other thread decides declarations now
    {
        // declared first, so that it is destroyed last: the holds keep every one of them until the references that they
        // hold for each other are released
        QoreTypeOwnerReleaseGuard holds;
        QoreTypeOwnerReleaseGuard names;
        {
            AutoLocker al(ctl);
            std::vector<QoreTypeOwner*> garbage;
            type_owner_find_cyclic_survivors(garbage);
            // a hold is recorded only when it is taken, so a failure leaves nothing to release that was not referenced
            holds.refs.reserve(garbage.size());
            names.refs.reserve(garbage.size());
            for (QoreTypeOwner* o : garbage) {
                o->refTypeOwner();
                holds.refs.push_back(o);
            }
            for (QoreTypeOwner* o : holds.refs) {
                names.refs.insert(names.refs.end(), o->type_owner_refs.begin(), o->type_owner_refs.end());
                o->type_owner_refs.clear();
                o->type_owner_names_counted = false;
            }
        }
    }

    // the tombstones of the declarations freed after their retirement (issue #6419)
    qore_free_type_tombstones();

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
    // derived types removed from the caches when a type they were built on was destroyed
    for (QoreTypeInfo* i : retired_derived_types)
        delete i;
    retired_derived_types.clear();
    retired_derived_type_epochs.clear();
}

void add_to_type_map(qore_type_t t, const QoreTypeInfo* typeInfo) {
   QoreAutoRWWriteLocker al(extern_type_info_map_lock);
   assert(extern_type_info_map.find(t) == extern_type_info_map.end());
   extern_type_info_map[t] = typeInfo;
}

// Given a parameterized class in the first return spec, distinguish *C<T> from a mixed union.
// A symbolic type parameter can also have NT_NOTHING as its type code.
static bool is_nullable_parameterized_class(const QoreTypeInfo* ti) {
    return ti->return_vec.size() == 2 && ti->return_vec[1].spec.isRuntimeBaseType(NT_NOTHING);
}

// A symbolic member may accept NOTHING. Until substitution, conversion between the declared value
// type and its or-nothing type must preserve the distinction between union<C<int>, T> and *union<C<int>, T>.
static bool type_accepts_nothing_for_conversion(const QoreTypeInfo* ti) {
    if (!QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)) {
        return false;
    }
    const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(ti);
    return !uti || uti->isOrNothing();
}

static const QoreTypeInfo* get_value_type_intern(const QoreTypeInfo* typeInfo) {
    assert(type_accepts_nothing_for_conversion(typeInfo));

    typeinfo_map_t::iterator i = typeinfo_or_nothing_map.find(typeInfo);
    if (i != typeinfo_or_nothing_map.end()) {
        return i->second;
    }

    if (const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(typeInfo)) {
        type_vec_t members = uti->getMemberTypes();
        members.erase(std::remove(members.begin(), members.end(), nothingTypeInfo), members.end());
        for (const QoreTypeInfo*& member : members) {
            if (qore_get_type_parameter_type_info(member)) {
                member = member->return_vec.front().spec.getTypeParameterTypeInfo();
            } else if (QoreTypeInfo::getUnionType(member)) {
                member = get_value_type(member);
            } else {
                member = get_or_nothing_base_type(member);
            }
        }
        return qore_get_union_type(members);
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
            // the spec of a parameterized class type (*C<int>) also gives its base class (QoreTypeSpec::getClass()),
            // whose type (object<C>) does not have the type arguments; tested in the class case, so that the value
            // type of a plain class costs only the test of the type spec (qore#7136)
            if (const QoreParameterizedClassTypeInfo* pti = typeInfo->return_vec[0].spec
                    .getParameterizedClassTypeInfo()) {
                if (is_nullable_parameterized_class(typeInfo)) {
                    return pti;
                }
            }
            return qc->getTypeInfo();
        }
    }

    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
        if (ti) {
            return qore_get_complex_hash_type(ti);
        }
    }

    // a soft list must be tested before a list, since its return spec also describes a complex list
    {
        const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexSoftListOrNothing(typeInfo);
        if (ti) {
            return qore_get_complex_softlist_type(ti);
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

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(typeInfo)) {
        return qore_get_complex_code_type(cti->getReturnType(), cti->getParamTypes(), cti->hasVarArgs());
    }

    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    return autoTypeInfo;
}

const QoreTypeInfo* get_value_type(const QoreTypeInfo* typeInfo) {
   return !type_accepts_nothing_for_conversion(typeInfo) ? typeInfo : get_value_type_intern(typeInfo);
}

const QoreTypeInfo* get_or_nothing_base_type(const QoreTypeInfo* typeInfo) {
    if (!QoreTypeInfo::hasType(typeInfo) || !type_accepts_nothing_for_conversion(typeInfo)) {
        return typeInfo;
    }
    // get_value_type_intern() does not resolve enum types
    {
        const QoreEnumDecl* ed = QoreTypeInfo::getReturnEnum(typeInfo);
        if (ed && ed->getTypeInfo(true) == typeInfo) {
            return ed->getTypeInfo();
        }
    }
    const QoreTypeInfo* vti = get_value_type_intern(typeInfo);
    // the type is *T only if T is a type without NOTHING whose or-nothing type is the type given; types such as
    // any, auto and nothing have no such T
    if (!QoreTypeInfo::hasType(vti) || type_accepts_nothing_for_conversion(vti)
        || get_or_nothing_type(vti) != typeInfo) {
        return typeInfo;
    }
    return vti;
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
   return type_accepts_nothing_for_conversion(typeInfo) ? typeInfo : get_or_nothing_type(typeInfo);
}

const QoreTypeInfo* get_or_nothing_type(const QoreTypeInfo* typeInfo) {
    assert(!type_accepts_nothing_for_conversion(typeInfo));

    // a builtin type holds its or-nothing type, which is read without a map lookup (issue #6126)
    if (unsigned index = typeInfo ? typeInfo->getBuiltinOrNothingIndex() : 0) {
        assert(index <= builtin_or_nothing_types.size());
        return builtin_or_nothing_types[index - 1];
    }

    if (const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(typeInfo)) {
        return qore_get_union_or_nothing_type(uti->getMemberTypes());
    }

    // see if we have a complex type
    {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
        if (hd)
            return hd->getTypeInfo(true);
    }

    {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(typeInfo);
        if (qc) {
            // the spec of a parameterized class type (C<int>) also gives its base class (QoreTypeSpec::getClass()),
            // whose or-nothing type (*object<C>) does not have the type arguments and does not accept the objects of
            // the specialized class (qore#7136); tested in the class case, so that the or-nothing type of a plain
            // class, which list literals fold at run time, costs only the test of the type spec
            const QoreTypeSpec& spec = typeInfo->return_vec[0].spec;
            if (const QoreParameterizedClassTypeInfo* pti = spec.getParameterizedClassTypeInfo()) {
                return qore_get_parameterized_class_type(pti->getBaseClass(), pti->getTypeArgs(), true);
            }
            return qc->getOrNothingTypeInfo();
        }
    }

    // a type whose first return alternative is a parameterized class but that returns other types too
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(typeInfo)) {
        return qore_get_parameterized_class_type(pti->getBaseClass(), pti->getTypeArgs(), true);
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

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(typeInfo)) {
        return qore_get_complex_code_or_nothing_type(cti->getReturnType(), cti->getParamTypes(), cti->hasVarArgs());
    }

    // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
    return autoTypeInfo;
}

// Container types are also derived from the types of the values in containers, so that building deeply nested data
// would create a type for each level, each with a name as long as its nesting; a type nested more deeply than this
// accepts values of any type instead
static constexpr unsigned QORE_MAX_CONTAINER_TYPE_NESTING = 64;

static bool qore_container_type_too_deep(const QoreTypeInfo* vti) {
    return QoreTypeInfo::getContainerNesting(vti) >= QORE_MAX_CONTAINER_TYPE_NESTING;
}

// returns a new container type for the given value type, with the nesting of the value type plus one
template <typename T>
static T* qore_new_container_type(const QoreTypeInfo* vti) {
    T* ti = new T(vti);
    ti->setContainerNesting(QoreTypeInfo::getContainerNesting(vti) + 1);
    return ti;
}

// The type of a reference expression (\x) is the hard reference type of its lvalue, which tells parse-time matching
// that the expression gives a reference; a container whose value type is derived from it (the literal {"a": \x})
// holds reference values, whose type at run time is the (soft) reference type of the same lvalue, as is the value
// type of a declared container type (hash<string, reference<T>>).  Both types are named reference<T>, so a container
// type is not built on a hard reference type but on the reference type: otherwise the values that have the
// container's value type would not be the same type, and each would be checked and accepted again when stored or
// passed (qore#6575).  No container type is cached for a hard reference type, so that only parse time, when the
// container types of reference expressions are derived, pays for the conversion

//! returns true if the type is a hard reference type (reference<T> or *reference<T> of a reference expression)
static bool qore_is_complex_hard_reference_type(const QoreTypeInfo* vti) {
    return !vti->return_vec.empty() && vti->return_vec[0].spec.getTypeSpec() == QTS_COMPLEXHARDREF;
}

//! returns the container type that \a get returns for the reference type of the hard reference type \a vti; must be
//! called without the complex type lock; out of line, as only parse time gets here
static __attribute__((noinline)) const QoreTypeInfo* qore_get_complex_container_type_of_reference(
        const QoreTypeInfo* (*get)(const QoreTypeInfo*), const QoreTypeInfo* vti) {
    assert(qore_is_complex_hard_reference_type(vti));
    const QoreTypeInfo* target = vti->return_vec[0].spec.getComplexReference();
    assert(target);
    if (vti->return_vec.size() == 1) {
        return get(qore_get_complex_reference_type(target));
    }
    assert(vti->return_vec.size() == 2 && vti->return_vec[1].spec.match(NT_NOTHING) == QTI_IDENT);
    return get(qore_get_complex_reference_or_nothing_type(target));
}

const QoreTypeInfo* qore_get_complex_hash_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return autoHashTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return hashTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = ch_map.lower_bound(vti);
        if (i != ch_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexHashTypeInfo* ti = qore_new_container_type<QoreComplexHashTypeInfo>(vti);
            derived_type_keys.insert(vti);
            ch_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_hash_type, vti);
}

const QoreTypeInfo* qore_get_complex_hash_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return autoHashOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return hashOrNothingTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = chon_map.lower_bound(vti);
        if (i != chon_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexHashOrNothingTypeInfo* ti = qore_new_container_type<QoreComplexHashOrNothingTypeInfo>(vti);
            derived_type_keys.insert(vti);
            chon_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_hash_or_nothing_type, vti);
}

const QoreTypeInfo* qore_get_complex_list_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return autoListTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return listTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = cl_map.lower_bound(vti);
        if (i != cl_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexListTypeInfo* ti = qore_new_container_type<QoreComplexListTypeInfo>(vti);
            derived_type_keys.insert(vti);
            cl_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_list_type, vti);
}

const QoreTypeInfo* qore_get_complex_list_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return autoListOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return listOrNothingTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = clon_map.lower_bound(vti);
        if (i != clon_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexListOrNothingTypeInfo* ti = qore_new_container_type<QoreComplexListOrNothingTypeInfo>(vti);
            derived_type_keys.insert(vti);
            clon_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_list_or_nothing_type, vti);
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
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return softAutoListTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return softListTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = csl_map.lower_bound(vti);
        if (i != csl_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexSoftListTypeInfo* ti = qore_new_container_type<QoreComplexSoftListTypeInfo>(vti);
            derived_type_keys.insert(vti);
            csl_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_softlist_type, vti);
}

const QoreTypeInfo* qore_get_complex_softlist_or_nothing_type(const QoreTypeInfo* vti) {
    if (vti == autoTypeInfo || qore_container_type_too_deep(vti)) {
        return softAutoListOrNothingTypeInfo;
    }
    if (vti == anyTypeInfo || !vti) {
        return softListOrNothingTypeInfo;
    }

    {
        AutoLocker al(ctl);

        tmap_t::iterator i = cslon_map.lower_bound(vti);
        if (i != cslon_map.end() && i->first == vti)
            return i->second;

        if (__builtin_expect(!qore_is_complex_hard_reference_type(vti), 1)) {
            QoreComplexSoftListOrNothingTypeInfo* ti
                = qore_new_container_type<QoreComplexSoftListOrNothingTypeInfo>(vti);
            derived_type_keys.insert(vti);
            cslon_map.insert(i, tmap_t::value_type(vti, ti));
            return ti;
        }
    }
    return qore_get_complex_container_type_of_reference(qore_get_complex_softlist_or_nothing_type, vti);
}

const QoreTypeInfo* qore_get_complex_hard_reference_type(const QoreTypeInfo* vti) {
    if (qore_container_type_too_deep(vti)) {
        vti = autoTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = chr_map.lower_bound(vti);
    if (i != chr_map.end() && i->first == vti)
        return i->second;

    QoreComplexHardReferenceTypeInfo* ti = qore_new_container_type<QoreComplexHardReferenceTypeInfo>(vti);
    derived_type_keys.insert(vti);
    chr_map.insert(i, tmap_t::value_type(vti, ti));
    return ti;
}

const QoreTypeInfo* qore_get_complex_reference_type(const QoreTypeInfo* vti) {
    // every reference created at run time looks up its type here: the type cached in the value type is the entry of
    // cr_map for it, so no lock is taken once the type exists (qore#7269); only a key of cr_map has it cached, which
    // a type nested too deeply never is
    if (vti) {
        const QoreTypeInfo* rv = vti->getComplexReferenceTypeCache();
        if (rv) {
            return rv;
        }
    }

    if (qore_container_type_too_deep(vti)) {
        vti = autoTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = cr_map.lower_bound(vti);
    if (i != cr_map.end() && i->first == vti) {
        if (vti) {
            vti->setComplexReferenceTypeCache(i->second);
        }
        return i->second;
    }

    QoreComplexReferenceTypeInfo* ti = qore_new_container_type<QoreComplexReferenceTypeInfo>(vti);
    derived_type_keys.insert(vti);
    cr_map.insert(i, tmap_t::value_type(vti, ti));
    if (vti) {
        vti->setComplexReferenceTypeCache(ti);
    }
    return ti;
}

const QoreTypeInfo* qore_get_complex_reference_or_nothing_type(const QoreTypeInfo* vti) {
    if (qore_container_type_too_deep(vti)) {
        vti = autoTypeInfo;
    }

    AutoLocker al(ctl);

    tmap_t::iterator i = cron_map.lower_bound(vti);
    if (i != cron_map.end() && i->first == vti)
        return i->second;

    QoreComplexReferenceOrNothingTypeInfo* ti = qore_new_container_type<QoreComplexReferenceOrNothingTypeInfo>(vti);
    derived_type_keys.insert(vti);
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
        type_args(qore_orphanable_vec(normalize_parameterized_class_args(type_args))),
        or_nothing(n_or_nothing) {
    assert(base_class);
    assert(!this->type_args.empty());
    assert(!or_nothing || value_type);
    std::vector<const QoreTypeInfo*> args = getTypeArgs().toVector();
    pname = build_parameterized_class_name(base_class, args, or_nothing, true).c_str();
    for (const QoreTypeInfo* arg : args) {
        if (qore_type_needs_decl_pin(arg)) {
            user_type_args = true;
            unsafe_receiver = true;
            // tested with the type parameter flags by the calls with this receiver
            setUnsafeReceiverCache();
            break;
        }
    }
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

const QoreTypeInfo* QoreTypeParameterTypeInfo::getBoundTypeInfo() const {
    if (const UserSignature* sig = getOwnerSignature()) {
        return sig->getTypeParameterBoundTypeInfo(index);
    }
    if (const QoreClass* cls = getOwnerClass()) {
        return qore_class_private::get(*cls)->getTypeParamBoundTypeInfo(index);
    }
    return nullptr;
}

const QoreTypeInfo* QoreTypeInfo::getTypeParameterSourceType(const QoreTypeInfo* ti) {
    if (!ti || ti->return_vec.size() > 2) {
        return nullptr;
    }
    const QoreTypeParameterTypeInfo* tp = ti->return_vec[0].spec.getTypeParameterTypeInfo();
    if (!tp) {
        return nullptr;
    }
    bool or_nothing = false;
    if (ti->return_vec.size() > 1) {
        // *T: the type parameter and NOTHING (see make_type_parameter_return_vec())
        if (ti->return_vec[1].spec.getTypeSpec() != QTS_TYPE || ti->return_vec[1].spec.getType() != NT_NOTHING) {
            return nullptr;
        }
        or_nothing = true;
    }
    const QoreTypeInfo* bound = tp->getBoundTypeInfo();
    if (!bound) {
        return autoTypeInfo;
    }
    // a bound that accepts values with a conversion (softint, number, softlist<...>) accepts type arguments that it
    // converts (C<string> for T: softint), but a value bound to T is not converted, so its type is only known at run
    // time
    for (auto& i : bound->getAcceptSpecs()) {
        if (i.map) {
            return autoTypeInfo;
        }
    }
    return or_nothing ? get_or_nothing_type(bound) : bound;
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
    assert(getOwnerClass());
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
    type_parameter = true;
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
    assert(getOwnerHashDecl());
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
    type_parameter = true;
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
    assert(getOwnerSignature());
    assert(name && *name);
    assert(!or_nothing || value_type);
    pname = tname.c_str();
    type_parameter = true;
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
    noteDerivedTypeKeys(key.baseClass, key.typeArgs);
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
    type_param_owner_keys.insert(key.owner);
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
    type_param_owner_keys.insert(key.owner);
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
    type_param_owner_keys.insert(key.owner);
    type_parameter_map.insert(i, type_parameter_map_t::value_type(key, ti));
    return ti;
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
    derived_type_keys.insert(key.bound);
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

    signed char cached = ti->getContainsTypeParameterCache();
    if (cached != -1) {
        return cached & 1;
    }

    if (const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(ti)) {
        for (const QoreTypeInfo* member : orphanable_type_vec_view_t(uti->getOrphanableMemberTypes())) {
            if (qore_type_contains_type_parameter(member)) {
                ti->setContainsTypeParameterCache(true);
                return true;
            }
        }
        ti->setContainsTypeParameterCache(false);
        return false;
    }

    if (qore_get_type_parameter_type_info(ti)) {
        ti->setContainsTypeParameterCache(true);
        return true;
    }

    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            if (qore_type_contains_type_parameter(arg)) {
                ti->setContainsTypeParameterCache(true);
                return true;
            }
        }
        ti->setContainsTypeParameterCache(false);
        return false;
    }

    if (const QoreWildcardTypeInfo* wti = QoreTypeInfo::getWildcardType(ti)) {
        bool rv = qore_type_contains_type_parameter(wti->getBound());
        ti->setContainsTypeParameterCache(rv);
        return rv;
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(ti)) {
        const typed_hash_decl_private* hp = typed_hash_decl_private::get(*hd);
        if (hp->isParameterizedHashDecl()) {
            for (const QoreTypeInfo* arg : hp->getTypeArgs()) {
                if (qore_type_contains_type_parameter(arg)) {
                    ti->setContainsTypeParameterCache(true);
                    return true;
                }
            }
        }
        ti->setContainsTypeParameterCache(false);
        return false;
    }

    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        if (qore_type_contains_type_parameter(cti->getReturnType())) {
            ti->setContainsTypeParameterCache(true);
            return true;
        }
        for (const QoreTypeInfo* param : cti->getParamTypes()) {
            if (qore_type_contains_type_parameter(param)) {
                ti->setContainsTypeParameterCache(true);
                return true;
            }
        }
        ti->setContainsTypeParameterCache(false);
        return false;
    }

    // a container of a type parameter, also an or-nothing one (*list<T>, *hash<string, T>, *reference<T>; qore#5849)
    const QoreTypeInfo* subtype = QoreTypeInfo::getReturnComplexHashOrNothing(ti);
    if (subtype && qore_type_contains_type_parameter(subtype)) {
        ti->setContainsTypeParameterCache(true);
        return true;
    }

    subtype = QoreTypeInfo::getReturnComplexListOrNothing(ti);
    if (subtype && qore_type_contains_type_parameter(subtype)) {
        ti->setContainsTypeParameterCache(true);
        return true;
    }

    subtype = QoreTypeInfo::getReturnComplexReferenceOrNothing(ti);
    bool rv = subtype && qore_type_contains_type_parameter(subtype);
    ti->setContainsTypeParameterCache(rv);
    return rv;
}

//! The declarations that a QoreExternalTypeDeclRefs object keeps
class qore_type_decl_refs_private {
public:
    std::vector<const TypedHashDecl*> hashdecls;
    std::vector<QoreClass*> classes;
    std::vector<const QoreEnumDecl*> enums;

    //! References the declarations that \a ti and the types in it refer to; \a p is created with the first one
    /** A type is a finite tree of types: the declarations are not entered, so no type is visited twice on a path

        @return true if the walk finds a declaration (also one that \a p references already), false if it finds none,
        in which case the type at the root of the walk is marked with QoreTypeInfo::setNamesNoDeclarations() (issue
        #6126)
    */
    DLLLOCAL static bool add(std::unique_ptr<qore_type_decl_refs_private>& p, const QoreTypeInfo* ti, int depth) {
        return walk<false>(&p, ti, depth);
    }

    //! Returns true if the walk of \a ti finds a declaration, ignoring the marks and without referencing anything
    /** A debug build checks the marks that it relies on with it (see qore_type_names_no_declarations()); the caller
        holds the type lock
    */
    DLLLOCAL static bool probe(const QoreTypeInfo* ti) {
        return walk<true>(nullptr, ti, 0);
    }

    DLLLOCAL ~qore_type_decl_refs_private() {
        for (QoreClass* c : classes) {
            qore_class_private::derefClass(*c, false, false, false);
        }
        for (const TypedHashDecl* hd : hashdecls) {
            hd->deref();
        }
        for (const QoreEnumDecl* ed : enums) {
            const_cast<qore_enum_decl_private*>(qore_enum_decl_private::get(*ed))->deref();
        }
    }

private:
    //! The walk of add() (\a Probe false) and probe() (\a Probe true: no reference is taken, no mark is read or set)
    template <bool Probe>
    DLLLOCAL static bool walk(std::unique_ptr<qore_type_decl_refs_private>* p, const QoreTypeInfo* ti, int depth) {
        if (!ti || depth > 64) {
            return false;
        }
        if (!Probe && ti->namesNoDeclarations()) {
#ifdef DEBUG
            // a wrong mark would drop references that the type needs: a debug build checks every mark it relies on
            assert(!probe(ti));
#endif
            return false;
        }
        bool found = false;
        for (const QoreReturnSpec& rs : ti->return_vec) {
            const QoreTypeSpec& spec = rs.spec;
            if (const TypedHashDecl* hd = spec.getHashDecl()) {
                if (!Probe) {
                    get(*p).addHashDecl(hd);
                }
                found = true;
            } else if (const QoreParameterizedClassTypeInfo* pti = spec.getParameterizedClassTypeInfo()) {
                if (!Probe) {
                    get(*p).addClass(pti->getBaseClass());
                }
                found = true;
                for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
                    walk<Probe>(p, arg, depth + 1);
                }
            } else if (const QoreClass* qc = spec.getClass()) {
                if (!Probe) {
                    get(*p).addClass(qc);
                }
                found = true;
            } else if (const QoreEnumDecl* ed = spec.getEnum()) {
                if (!Probe) {
                    get(*p).addEnum(ed);
                }
                found = true;
            } else if (const QoreWildcardTypeInfo* wti = spec.getWildcardTypeInfo()) {
                found |= walk<Probe>(p, wti->getBound(), depth + 1);
            } else if (const QoreTypeInfo* elem = spec.getElementType()) {
                found |= walk<Probe>(p, elem, depth + 1);
            }
        }
        if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
            found |= walk<Probe>(p, cti->getReturnType(), depth + 1);
            for (const QoreTypeInfo* param : cti->getParamTypes()) {
                found |= walk<Probe>(p, param, depth + 1);
            }
        }
        // the walk of a type that finds no declaration never finds one (an orphaned pointer is repointed to a
        // tombstone of the same kind only), so later references of it need neither the type lock nor this walk; only
        // the root of the walk is marked, also when the walk stopped at the depth limit (a later walk from the root
        // stops at the same types, so it finds none, as before): a type nested in it is walked with less depth left,
        // so its walk can have been cut short where a later walk from it would not be
        if (!Probe && !found && !depth) {
            ti->setNamesNoDeclarations();
        }
        return found;
    }

    DLLLOCAL static qore_type_decl_refs_private& get(std::unique_ptr<qore_type_decl_refs_private>& p) {
        if (!p) {
            p.reset(new qore_type_decl_refs_private);
        }
        return *p;
    }

    DLLLOCAL void addHashDecl(const TypedHashDecl* hd) {
        if (std::find(hashdecls.begin(), hashdecls.end(), hd) == hashdecls.end()) {
            hashdecls.push_back(hd);
            hd->ref();
        }
    }

    DLLLOCAL void addClass(const QoreClass* qc) {
        if (!qc) {
            return;
        }
        QoreClass* c = const_cast<QoreClass*>(qc);
        if (std::find(classes.begin(), classes.end(), c) == classes.end()) {
            // as an object holds its class (see QoreObject)
            classes.push_back(c);
            qore_class_private::refClass(*c);
        }
    }

    DLLLOCAL void addEnum(const QoreEnumDecl* ed) {
        if (std::find(enums.begin(), enums.end(), ed) == enums.end()) {
            enums.push_back(ed);
            qore_enum_decl_private::get(*ed)->ref();
        }
    }
};

//! Returns true if \a ti is null or marked as naming no declaration (QoreTypeInfo::namesNoDeclarations())
/** A debug build checks the mark with a walk of the type under the type lock, so that a wrong mark fails a debug run
    (issue #6126)
*/
static bool qore_type_names_no_declarations(const QoreTypeInfo* ti) {
    if (!ti) {
        return true;
    }
    if (!ti->namesNoDeclarations()) {
        return false;
    }
#ifdef DEBUG
    AutoLocker al(ctl);
    assert(!qore_type_decl_refs_private::probe(ti));
#endif
    return true;
}

QoreExternalTypeDeclRefs::QoreExternalTypeDeclRefs(const QoreTypeInfo* ti) {
    // a type known to name no declaration (ex: *int) has nothing to reference, so neither the lock nor the walk of
    // the type is needed (issue #6126)
    if (qore_type_names_no_declarations(ti)) {
        return;
    }
    // nothing is allocated for a type without declarations, such as a builtin type
    std::unique_ptr<qore_type_decl_refs_private> p;
    // the references are taken under the type lock: a declaration reached through a type can be a member of a group
    // of declarations of a deleted Program, whose collection and sweeps read the references of its members under the
    // lock (issue #5790)
    AutoLocker al(ctl);
    qore_type_decl_refs_private::add(p, ti, 0);
    priv = p.release();
}

QoreExternalTypeDeclRefs::~QoreExternalTypeDeclRefs() {
    delete priv;
}

QoreTypeDeclRefs::~QoreTypeDeclRefs() {
    delete priv;
}

void QoreTypeDeclRefs::add(const type_vec_t& types) {
    // the type arguments of generic code are mostly types that name no declaration (ex: int): then neither the lock
    // nor a walk of the types is needed (issue #6126)
    bool names = false;
    for (const QoreTypeInfo* ti : types) {
        if (!qore_type_names_no_declarations(ti)) {
            names = true;
            break;
        }
    }
    if (!names) {
        return;
    }
    std::unique_ptr<qore_type_decl_refs_private> p(priv);
    priv = nullptr;
    {
        // under the type lock, as for QoreExternalTypeDeclRefs
        AutoLocker al(ctl);
        for (const QoreTypeInfo* ti : types) {
            qore_type_decl_refs_private::add(p, ti, 0);
        }
    }
    priv = p.release();
}

void QoreTypeDeclRefs::add(const QoreTypeInfo* ti) {
    if (qore_type_names_no_declarations(ti)) {
        return;
    }
    std::unique_ptr<qore_type_decl_refs_private> p(priv);
    priv = nullptr;
    {
        AutoLocker al(ctl);
        qore_type_decl_refs_private::add(p, ti, 0);
    }
    priv = p.release();
}

const QoreTypeInfo* qore_substitute_type_params_if_needed(const QoreTypeInfo* ti) {
    return qore_type_contains_type_parameter(ti)
        ? qore_substitute_type_params(ti, qore_get_current_receiver_type_info())
        : ti;
}

const TypedHashDecl* qore_substitute_hashdecl_type_params(const TypedHashDecl* hd) {
    assert(hd);
    const TypedHashDecl* rv = QoreTypeInfo::getUniqueReturnHashDecl(
        qore_substitute_type_params_if_needed(hd->getTypeInfo()));
    return rv ? rv : hd;
}

bool qore_resolve_explicit_type_param_instantiation(const QoreTypeParamInstantiation& inst,
        QoreTypeParamInstantiation& resolved) {
    bool changed = false;
    for (const QoreTypeInfo* type_arg : inst.type_args) {
        if (qore_type_contains_type_parameter(type_arg)) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return false;
    }
    resolved.owner = inst.owner;
    resolved.type_args.clear();
    resolved.type_args.reserve(inst.type_args.size());
    for (const QoreTypeInfo* type_arg : inst.type_args) {
        resolved.type_args.push_back(qore_substitute_type_params_if_needed(type_arg));
    }
    return true;
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

//! makes a union type of the members given (see qore_get_union_type())
static QoreUnionTypeInfo* qore_make_union_type(const type_vec_t& key, bool or_nothing, bool has_nothing,
        const type_vec_t& value_types) {
    // Build accept and return vectors from all member types
    q_accept_vec_t accept_vec;
    q_return_vec_t return_vec;

    // a return type is exact only if the union returns no other value (qore#5857): with an exact member return
    // type, QoreTypeInfo::parseReturns() reports the union as returning only that type (QTI_IDENT), and code
    // specialized for it took a union<int, float> member value as an int; as with the builtin multi-type types (ex:
    // int|float), the member return types of a union with more than one value type are not exact; the same value
    // type returned by several members (ex: union<int, *int>) counts once
    std::vector<const QoreTypeSpec*> value_specs;
    for (const QoreTypeInfo* ti : key) {
        for (const auto& r : ti->return_vec) {
            if (r.spec.isRuntimeBaseType(NT_NOTHING)) {
                continue;
            }
            if (std::none_of(value_specs.begin(), value_specs.end(),
                    [&r](const QoreTypeSpec* s) { return *s == r.spec; })) {
                value_specs.push_back(&r.spec);
            }
        }
    }
    size_t value_returns = value_specs.size();
    for (const QoreTypeInfo* ti : key) {
        // Add accept types from this member
        for (const auto& a : ti->getAcceptSpecs()) {
            accept_vec.push_back(a);
        }
        // Add return types from this member
        for (const auto& r : ti->return_vec) {
            if (value_returns > 1 && r.exact) {
                QoreReturnSpec inexact = r;
                inexact.exact = false;
                return_vec.push_back(inexact);
            } else {
                return_vec.push_back(r);
            }
        }
    }

    // If or_nothing and no member accepts NOTHING, add it
    if (or_nothing && !has_nothing) {
        accept_vec.push_back({NT_NOTHING, nullptr});
        accept_vec.push_back({NT_NULL, [] (QoreValue& n, ExceptionSink* xsink) { n.assignNothing(); }});
        return_vec.push_back({NT_NOTHING});
    }

    // Build the name
    // the name is built from the members without duplicates, as an AOT image writes and reads the type (qore#5858)
    QoreString name = build_union_name(key, or_nothing && !has_nothing);

    // Create the union type
    QoreUnionTypeInfo* ti = new QoreUnionTypeInfo(std::move(accept_vec), std::move(return_vec), name, key,
        or_nothing || has_nothing);
    ti->initExactMembers(value_types);
    return ti;
}

const QoreTypeInfo* qore_get_union_type(const type_vec_t& member_types, bool or_nothing) {
    // Handle edge cases
    if (member_types.empty()) {
        return autoTypeInfo;  // empty union accepts anything
    }

    // Normalize the key for consistent cache lookup
    type_vec_t key = normalize_union_key(member_types);

    // a union of one type, also after its duplicates are removed (ex: union<int, int>), is that type, as an AOT image
    // that writes the union without the duplicates reads it back (qore#5858)
    if (key.size() == 1) {
        const QoreTypeInfo* ti = key[0];
        if (or_nothing) {
            const QoreTypeInfo* orn = qore_get_or_nothing_type(ti);
            return orn ? orn : ti;
        }
        return ti;
    }

    // Check if any member already accepts NOTHING, if so, or_nothing is already covered
    bool has_nothing = false;
    for (const QoreTypeInfo* ti : key) {
        // A symbolic T might specialize to a type that accepts NOTHING. Only a definite NOTHING
        // alternative makes the outer * redundant; otherwise the two declarations need distinct cache entries.
        if (QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)
                && (!qore_type_contains_type_parameter(ti)
                    || std::any_of(ti->return_vec.begin(), ti->return_vec.end(),
                        [](const QoreReturnSpec& rt) { return rt.spec.isRuntimeBaseType(NT_NOTHING); }))) {
            has_nothing = true;
            break;
        }
    }

    // Use appropriate cache
    union_map_t& cache = (or_nothing && !has_nothing) ? union_on_map : union_map;

    // the value types of the members, which a value of the union is accepted as (qore#6407), are taken outside the
    // lock, as get_value_type() takes it to make a type, and only for a union that is not made yet (a generic call
    // looks the union up for each call); the union is then looked up again, as another thread can have made it
    type_vec_t value_types;
    while (true) {
        {
            AutoLocker al(ctl);
            union_map_t::iterator i = cache.find(key);
            if (i != cache.end()) {
                return i->second;
            }
            if (!value_types.empty()) {
                QoreUnionTypeInfo* ti = qore_make_union_type(key, or_nothing, has_nothing, value_types);
                noteDerivedTypeKeys(nullptr, key);
                cache[key] = ti;
                return ti;
            }
        }
        value_types.reserve(key.size());
        for (const QoreTypeInfo* ti : key) {
            // a union member (also an or-nothing union, whose value type is not a union) gives its exact members
            value_types.push_back(dynamic_cast<const QoreUnionTypeInfo*>(ti) ? ti : get_value_type(ti));
        }
    }
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

    // a complex type followed by a parameter list is the callable signature of a nested code<> type whose return type
    // is complex (ex: "hash<auto>()" in "code<hash<auto>()>"): it is kept as a single opaque name, as above, so that
    // the outer code<> handler parses the return type and the parameters; parsing it as the complex type would drop
    // the parameter list (#6022)
    {
        size_t next = close_pos + 1;
        while (next < str.size() && isspace(str[next])) {
            ++next;
        }
        if (next < str.size() && str[next] == '(') {
            return new QoreParseTypeInfo(strdup(str.c_str()), or_nothing);
        }
    }

    // Extract base type and subtype content
    std::string base_type = str.substr(0, angle_pos);
    std::string subtype_content = str.substr(angle_pos + 1, close_pos - angle_pos - 1);

    // Parse subtypes (split on commas, respecting nested brackets and the parameter lists of callable signatures,
    // ex: the commas in "code<int(string, bool)>")
    parse_type_vec_t subtypes;
    std::string current;
    int angle_depth = 0;
    int paren_depth = 0;
    for (size_t i = 0; i <= subtype_content.size(); ++i) {
        char c = i < subtype_content.size() ? subtype_content[i] : '\0';
        if (c == '<') {
            ++angle_depth;
            current += c;
        } else if (c == '>') {
            --angle_depth;
            current += c;
        } else if (c == '(') {
            ++paren_depth;
            current += c;
        } else if (c == ')') {
            --paren_depth;
            current += c;
        } else if ((c == ',' && angle_depth == 0 && paren_depth == 0) || c == '\0') {
            // the last subtype is kept also if its brackets are unbalanced, so that its resolution reports it
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

// Returns any trailing text after a complete type in a code<> signature type string (ex: a parameter name)
/** Parameter names are not supported in code<> callable signatures; the signature declares types only (see
    design/named-arguments.md: carrying names in the callable type is deferred v2 work, and the syntax space
    must not be consumed accidentally).  Complex-type parsing stops at the '>' matching the first '<', so
    without this check a trailing name after a complex type (ex: "hash<auto> query") would be silently
    discarded, while the same name after a simple type (ex: "int x") fails with a confusing "reference to
    undefined type 'int x'" error.  Detecting the trailing text makes both cases fail consistently.

    @param str the trimmed type string to check
    @param trailing set to the trailing text if any is found

    @return true if trailing text was found, meaning that the type string is invalid
*/
static bool code_sig_type_trailing_text(const std::string& str, std::string& trailing) {
    const char* ws = " \t\n\r\f\v";
    // wildcard type arguments ("? extends X", "? super X") legitimately contain whitespace
    if (str.empty() || str[0] == '?') {
        return false;
    }
    size_t end;
    size_t angle_pos = str.find('<');
    if (angle_pos == std::string::npos) {
        // simple type: the type name ends at the first whitespace character
        end = str.find_first_of(ws);
        if (end == std::string::npos) {
            return false;
        }
    } else {
        // complex type: the type ends at the '>' matching the first '<'
        int depth = 0;
        end = std::string::npos;
        for (size_t i = angle_pos, e = str.size(); i < e; ++i) {
            if (str[i] == '<') {
                ++depth;
            } else if (str[i] == '>' && !--depth) {
                end = i + 1;
                break;
            }
        }
        if (end == std::string::npos) {
            // unbalanced angle brackets; reported elsewhere
            return false;
        }
    }
    size_t tpos = str.find_first_not_of(ws, end);
    if (tpos == std::string::npos) {
        return false;
    }
    trailing = str.substr(tpos);
    return true;
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

    // reject parameter names and any other trailing text after the type
    {
        std::string trailing;
        if (code_sig_type_trailing_text(str, trailing)) {
            parseException(*loc, "PARSE-TYPE-ERROR", "invalid type '%s' in a 'code<...>' callable signature; " \
                "unexpected text '%s' after the type; callable signatures declare types only, parameter " \
                "names are not supported (ex: 'code<hash<auto>(hash<auto>)>')", str.c_str(), trailing.c_str());
            err = -1;
            return nullptr;
        }
    }

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

// trims leading and trailing whitespace
static void qore_trim_type_component(std::string& str) {
    while (!str.empty() && isspace(str.back())) {
        str.pop_back();
    }
    size_t i = 0;
    while (i < str.size() && isspace(str[i])) {
        ++i;
    }
    str.erase(0, i);
}

//! Splits the callable signature "ReturnType(ParamTypes...)" of a code<> type (#6022)
/** The parameter list is the first one outside angle brackets, as the return type can be a callable type with a
    parameter list of its own (ex: "code<*hash<auto>()>()"), and ends at its matching parenthesis, after which only
    whitespace is allowed; the parameters are split at the commas outside angle brackets and parentheses, so that a
    nested callable's parameters stay with it; a "..." at the end of the last parameter (alone or after its type) makes
    the callable variadic, so that the "..." of a nested callable does not.  Empty parameters are skipped.

    @param sig the signature
    @param return_type the return type string (trimmed; empty if none)
    @param params the parameter type strings (trimmed)
    @param varargs set to true if the callable is variadic
    @param error the error message if the signature is malformed

    @return true if the signature is well formed, false (with @a error set) if not
*/
static bool qore_split_code_signature(const char* sig, std::string& return_type, std::vector<std::string>& params,
        bool& varargs, std::string& error) {
    params.clear();
    varargs = false;

    const char* open = nullptr;
    int angle_depth = 0;
    for (const char* p = sig; *p; ++p) {
        if (*p == '<') {
            ++angle_depth;
        } else if (*p == '>') {
            --angle_depth;
        } else if (*p == '(' && !angle_depth) {
            open = p;
            break;
        }
    }
    if (!open) {
        error = "expected format 'ReturnType(ParamTypes...)' with parentheses";
        return false;
    }

    // the matching closing parenthesis
    const char* close = nullptr;
    int paren_depth = 0;
    angle_depth = 0;
    for (const char* p = open; *p; ++p) {
        if (*p == '<') {
            ++angle_depth;
        } else if (*p == '>') {
            --angle_depth;
        } else if (*p == '(') {
            ++paren_depth;
        } else if (*p == ')' && !--paren_depth) {
            close = p;
            break;
        }
    }
    if (!close) {
        error = "missing closing parenthesis";
        return false;
    }
    if (angle_depth) {
        error = "unbalanced angle brackets in the parameter list";
        return false;
    }
    for (const char* p = close + 1; *p; ++p) {
        if (!isspace(*p)) {
            error = std::string("unexpected text '") + p + "' after the parameter list";
            return false;
        }
    }

    return_type.assign(sig, open - sig);
    qore_trim_type_component(return_type);

    std::string current;
    angle_depth = 0;
    paren_depth = 0;
    for (const char* p = open + 1; p <= close; ++p) {
        char c = (p == close) ? '\0' : *p;
        if (c == '<') {
            ++angle_depth;
        } else if (c == '>') {
            --angle_depth;
        } else if (c == '(') {
            ++paren_depth;
        } else if (c == ')') {
            --paren_depth;
        }
        if (angle_depth < 0 || paren_depth < 0) {
            error = "unbalanced angle brackets or parentheses in the parameter list";
            return false;
        }
        if ((c == ',' || !c) && !angle_depth && !paren_depth) {
            qore_trim_type_component(current);
            if (!current.empty()) {
                params.push_back(current);
            }
            current.clear();
        } else {
            current += c;
        }
    }

    // a "..." at the end of the last parameter makes the callable variadic
    if (!params.empty()) {
        std::string& last = params.back();
        if (last.size() >= 3 && !last.compare(last.size() - 3, 3, "...")) {
            varargs = true;
            last.erase(last.size() - 3);
            qore_trim_type_component(last);
            if (last.empty()) {
                params.pop_back();
            }
        }
    }
    return true;
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

// raises a RUNTIME-TYPE-ERROR if the closure or call reference \a n does not have a signature that the typed
// callable type \a ti accepts (qore#6660)
static void qore_complex_code_check_callable(const QoreComplexCodeTypeInfo* ti, QoreValue& n, ExceptionSink* xsink) {
    if (xsink && !ti->acceptsCallable(n)) {
        const QoreTypeInfo* vti = n.getTypeInfo();
        xsink->raiseException("RUNTIME-TYPE-ERROR", "type '%s' expects a callable with a compatible signature, but "
            "got type '%s'", QoreTypeInfo::getName(ti), QoreTypeInfo::getName(vti));
    }
}

// Helper to build accept_vec for typed callable; the specs check the signature of the callable with their map, so
// that the types of other values do not pay for the check (qore#6660)
static q_accept_vec_t build_complex_code_accept_vec(const QoreComplexCodeTypeInfo* ti, bool or_nothing) {
    q_accept_vec_t avec;
    avec.push_back({NT_RUNTIME_CLOSURE, [ti] (QoreValue& n, ExceptionSink* xsink) {
        qore_complex_code_check_callable(ti, n, xsink);
    }});
    avec.push_back({NT_FUNCREF, [ti] (QoreValue& n, ExceptionSink* xsink) {
        qore_complex_code_check_callable(ti, n, xsink);
    }});
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

//! returns the return type of a typed callable signature, or nullptr for a callable that returns nothing, whose
//! return type is nullptr in a declared type and the nothing type in the type of a closure (qore#6660)
static const QoreTypeInfo* qore_code_return_type(const QoreTypeInfo* ret) {
    return ret == nothingTypeInfo ? nullptr : ret;
}

QoreComplexCodeTypeInfo::QoreComplexCodeTypeInfo(const QoreTypeInfo* ret_type, type_vec_t&& param_types,
        bool varargs_arg, bool or_nothing_arg)
        : QoreTypeInfo(build_complex_code_accept_vec(this, or_nothing_arg),
            build_complex_code_return_vec(or_nothing_arg),
            build_complex_code_name(ret_type, param_types, varargs_arg, or_nothing_arg)),
          returnType(ret_type), paramTypes(qore_orphanable_vec(param_types)),
          varargs(varargs_arg), orNothing(or_nothing_arg) {
    complex_code = true;
    // Set path name
    pname = build_complex_code_path(ret_type, param_types, varargs, orNothing);
}

bool QoreComplexCodeTypeInfo::acceptsCallable(const QoreValue& n) const {
    qore_type_t t = n.getType();
    if (t != NT_RUNTIME_CLOSURE && t != NT_FUNCREF) {
        return true;
    }
    // a closure is always a resolved call reference
    const ResolvedCallReferenceNode* cr = t == NT_RUNTIME_CLOSURE
        ? static_cast<const ResolvedCallReferenceNode*>(n.getInternalNode())
        : dynamic_cast<const ResolvedCallReferenceNode*>(n.getInternalNode());
    const QoreFunction* f = cr ? cr->getFunction() : nullptr;
    if (!f) {
        return true;
    }
    // a call reference to a function with more than one variant calls the variant that its arguments select: it is
    // accepted if one of them has a compatible signature
    QoreFunctionIterator i(*f);
    bool has_variant = false;
    while (i.next()) {
        has_variant = true;
        const AbstractFunctionSignature* sig = i.getVariant()->getSignature();
        // the common case: the callable has this type
        if (sig && (sig->getRuntimeCodeTypeInfo() == this || isSignatureCompatible(sig))) {
            return true;
        }
    }
    return !has_variant;
}

const QoreTypeInfo* AbstractFunctionSignature::getRuntimeCodeTypeInfo() const {
    const QoreTypeInfo* ti = runtime_code_type_cache.load(std::memory_order_acquire);
    if (!ti) {
        ti = qore_get_complex_code_type_from_signature(this);
        runtime_code_type_cache.store(ti, std::memory_order_release);
    }
    return ti;
}

bool QoreTypeInfo::complexCodeAcceptsCallable(const QoreValue& n) const {
    return !complex_code || static_cast<const QoreComplexCodeTypeInfo*>(this)->acceptsCallable(n);
}

bool QoreTypeInfo::codeAcceptsTypedCallablesUnchecked(const QoreTypeInfo* target, const QoreTypeInfo* source) {
    if ((target != codeTypeInfo && target != codeOrNothingTypeInfo) || !source || !source->complex_code) {
        return false;
    }
    return target == codeOrNothingTypeInfo || !static_cast<const QoreComplexCodeTypeInfo*>(source)->isOrNothing();
}

bool QoreTypeInfo::complexCodeTypesEqual(const QoreTypeInfo* a, const QoreTypeInfo* b) {
    assert(a->complex_code || b->complex_code);
    if (!a->complex_code || !b->complex_code) {
        return false;
    }
    const QoreComplexCodeTypeInfo* ca = static_cast<const QoreComplexCodeTypeInfo*>(a);
    const QoreComplexCodeTypeInfo* cb = static_cast<const QoreComplexCodeTypeInfo*>(b);
    if (ca->hasVarArgs() != cb->hasVarArgs() || ca->isOrNothing() != cb->isOrNothing()
            || !equal(qore_code_return_type(ca->getReturnType()), qore_code_return_type(cb->getReturnType()))) {
        return false;
    }
    const type_vec_t& pa = ca->getParamTypes();
    const type_vec_t& pb = cb->getParamTypes();
    if (pa.size() != pb.size()) {
        return false;
    }
    for (size_t i = 0, e = pa.size(); i < e; ++i) {
        if (!equal(pa[i], pb[i])) {
            return false;
        }
    }
    return true;
}

//! returns true if the typed callable type \a first is a superset of the type \a second, of which at least one is
//! a typed callable type (qore#6660)
/** A typed callable type is a superset only of the typed callable types with the same signature (and of their
    non-NOTHING types, for an or-nothing type): a type does not record which parameters of a callable have default
    arguments, so the variance check of checkComplexCodeCompatibility() cannot tell whether every value of another
    signature is accepted, and a superset decides that values are kept unchecked and is the common type of folded
    values; the type \c code (and \c *code) is a superset of every typed callable type, which its specs give
*/
static __attribute__((noinline)) bool qore_complex_code_super_set_of(const QoreTypeInfo* first,
        const QoreTypeInfo* second) {
    if (!first->isComplexCode() || !second->isComplexCode()) {
        return false;
    }
    const QoreComplexCodeTypeInfo* cf = static_cast<const QoreComplexCodeTypeInfo*>(first);
    const QoreComplexCodeTypeInfo* cs = static_cast<const QoreComplexCodeTypeInfo*>(second);
    if (!cf->isOrNothing() && cs->isOrNothing()) {
        return false;
    }
    if (cf->isOrNothing() != cs->isOrNothing()) {
        // compare the signatures of *code<sig> and code<sig>
        return QoreTypeInfo::equal(qore_get_complex_code_type(cf->getReturnType(), cf->getParamTypes(),
            cf->hasVarArgs(), false), second);
    }
    return QoreTypeInfo::complexCodeTypesEqual(first, second);
}

bool QoreComplexCodeTypeInfo::isSignatureCompatible(const AbstractFunctionSignature* sig) const {
    if (!sig) {
        return false;
    }

    // Check return type compatibility (covariant)
    // The callable's return type must be assignable to our return type
    const QoreTypeInfo* sigReturnType = sig->getReturnTypeInfo();
    if (qore_code_return_type(returnType)) {
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

    // Compute minimum required params for the signature (params without defaults); a parameter that takes NOTHING
    // (*int) need not be given either
    unsigned sigMinParams = 0;
    for (unsigned i = 0; i < sigNumParams; ++i) {
        if (!sig->hasDefaultArg(i)) {
            const QoreTypeInfo* pt = sig->getParamTypeInfo(i);
            if (QoreTypeInfo::hasType(pt) && !QoreTypeInfo::parseAcceptsReturns(pt, NT_NOTHING)) {
                sigMinParams = i + 1;
            }
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
    noteDerivedTypeKeys(key.returnType, key.paramTypes);
    complex_code_map[key] = ti;
    return ti;
}

const QoreTypeInfo* qore_get_complex_code_or_nothing_type(const QoreTypeInfo* return_type,
        const type_vec_t& param_types, bool varargs) {
    return qore_get_complex_code_type(return_type, param_types, varargs, true);
}

static const QoreTypeInfo* get_substituted_type_param_arg(const QoreTypeParameterTypeInfo* tpi,
        const QoreTypeInfo* receiver_type_info, const QoreTypeParamInstantiation* type_param_inst) {
    // the declaration of the type parameter was destroyed: no receiver or instantiation can be of it (issue #6419)
    if (tpi->isOrphaned()) {
        return nullptr;
    }
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

    // Return specs describe values after filtering. Substitute the declared union members so their
    // conversions, signatures and priority survive specialization.
    if (const QoreUnionTypeInfo* uti = QoreTypeInfo::getUnionType(ti)) {
        type_vec_t members = uti->getMemberTypes();
        bool changed = false;
        bool nullable = uti->isOrNothing();
        for (const QoreTypeInfo*& member : members) {
            // A bare T can accept and return NOTHING before binding. Preserve that property
            // when specializing, regardless of T's position among the declared alternatives.
            if (!nullable && member->isTypeParameter()) {
                nullable = QoreTypeInfo::parseAcceptsReturns(member, NT_NOTHING);
            }
            const QoreTypeInfo* subst = qore_substitute_type_params(member, receiver_type_info, type_param_inst);
            if (subst != member) {
                member = subst;
                changed = true;
            }
        }
        return changed ? qore_get_union_type(members, nullable) : ti;
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
        // The class spec is the mandatory value type even for *C<T>. Read nullability from the full declaration.
        return changed ? qore_get_parameterized_class_type(pti->getBaseClass(), args,
            is_nullable_parameterized_class(ti)) : ti;
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

    // a container of a type parameter, also an or-nothing one (qore#5849)
    const QoreTypeInfo* subtype = QoreTypeInfo::getReturnComplexHashOrNothing(ti);
    if (subtype) {
        const QoreTypeInfo* subst = qore_substitute_type_params(subtype, receiver_type_info, type_param_inst);
        if (subst != subtype) {
            return QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)
                ? qore_get_complex_hash_or_nothing_type(subst)
                : qore_get_complex_hash_type(subst);
        }
    }

    subtype = QoreTypeInfo::getReturnComplexListOrNothing(ti);
    if (subtype) {
        const QoreTypeInfo* subst = qore_substitute_type_params(subtype, receiver_type_info, type_param_inst);
        if (subst != subtype) {
            // a softlist is told by its spec, not by comparing its name (issue #6126)
            bool is_soft = QoreTypeInfo::getReturnComplexSoftListOrNothing(ti) != nullptr;
            if (QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING)) {
                return is_soft ? qore_get_complex_softlist_or_nothing_type(subst)
                    : qore_get_complex_list_or_nothing_type(subst);
            }
            return is_soft ? qore_get_complex_softlist_type(subst) : qore_get_complex_list_type(subst);
        }
    }

    subtype = QoreTypeInfo::getReturnComplexReferenceOrNothing(ti);
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
    return ti->isComplexCode() ? static_cast<const QoreComplexCodeTypeInfo*>(ti) : nullptr;
}

bool QoreTypeInfo::unionAcceptsExactlyImpl(const QoreValue& n) const {
    // only a union sets the mask
    return static_cast<const QoreUnionTypeInfo*>(this)->hasExactMember(n);
}

//! returns true if a member type keeps a value of the scalar base type \a t as it is (qore#6407)
/** The plain type (ex: int), a soft type of the base type (ex: softint) and a type of the base type that converts
    other values (ex: timeout) accept a value of the base type without converting it; a date type with a subtype does
    not keep a date of the other subtype
*/
static bool qore_union_member_keeps_scalar(const QoreTypeInfo* vt, qore_type_t t) {
    switch (t) {
        case NT_INT:
        case NT_FLOAT:
        case NT_NUMBER:
        case NT_STRING:
        case NT_BOOLEAN:
        case NT_BINARY:
            break;
        case NT_DATE:
            if (QoreTypeInfo::getDateSubtype(vt)) {
                return false;
            }
            break;
        default:
            return false;
    }
    for (const QoreAcceptSpec& a : vt->getAcceptSpecs()) {
        if (!a.map && a.spec.isRuntimeBaseType(t)) {
            return true;
        }
    }
    return false;
}

void QoreUnionTypeInfo::initExactMembers(const type_vec_t& value_types) {
    for (const QoreTypeInfo* vt : value_types) {
        if (!hasType(vt) || vt == autoTypeInfo || vt == autoNoNarrowTypeInfo || vt == anyTypeInfo) {
            continue;
        }
        // a member that is a union (ex: a typedef of a union) gives its own exact members, so that the result does
        // not depend on where the nested union is in the union
        if (const QoreUnionTypeInfo* nested = dynamic_cast<const QoreUnionTypeInfo*>(vt)) {
            exact_members.insert(exact_members.end(), nested->exact_members.begin(), nested->exact_members.end());
            union_exact_mask |= nested->union_exact_mask;
            union_scalar_mask |= nested->union_scalar_mask;
            continue;
        }
        if (vt->return_vec.size() != 1) {
            continue;
        }
        // an enum value is kept by an enum type with qore_type_info_has_enum_spec_for() (qore#6277)
        if (vt->return_vec[0].spec.getEnum()) {
            continue;
        }
        qore_type_t t = vt->return_vec[0].spec.getType();
        if (t < 0 || t >= 64) {
            continue;
        }
        exact_members.push_back(vt);
        union_exact_mask |= (1ull << t);
        if (qore_union_member_keeps_scalar(vt, t)) {
            union_scalar_mask |= (1ull << t);
        }
    }
}

bool QoreUnionTypeInfo::hasExactMember(const QoreValue& n) const {
    // an enum value is kept by an enum type of the union with qore_type_info_has_enum_spec_for() (qore#6277)
    if (n.isEnum()) {
        return false;
    }
    const QoreTypeInfo* vt = n.getFullTypeInfo();
    for (const QoreTypeInfo* m : orphanable_type_vec_view_t(exact_members)) {
        // a list or hash member with no element type keeps any list or hash as it is
        if (m == vt || ((m == autoHashTypeInfo || m == hashTypeInfo) && n.getType() == NT_HASH)
                || ((m == autoListTypeInfo || m == listTypeInfo) && n.getType() == NT_LIST)) {
            return true;
        }
    }
    return false;
}

const QoreUnionTypeInfo* QoreTypeInfo::getUnionType(const QoreTypeInfo* ti) {
    if (!ti || !hasType(ti)) {
        return nullptr;
    }
    // classified with the type's flag, not a dynamic_cast (issue #6126)
    return ti->isUnion() ? static_cast<const QoreUnionTypeInfo*>(ti) : nullptr;
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
    const QoreTypeInfo* target_ret = qore_code_return_type(target_code->getReturnType());
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
            return static_cast<const WeakHashReferenceNode*>(n)->getTypeInfo();
        case NT_LIST:
            return static_cast<const QoreListNode*>(n)->getTypeInfo();
        case NT_WEAKREF_LIST:
            return static_cast<const WeakListReferenceNode*>(n)->getTypeInfo();
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
    if (!strcmp(str, "char") && (parse_get_parse_options() & QoreParseOptions::NO_CHAR_TYPE)) {
        return nullptr;
    }

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
    if (!strcmp(str, "char") && (parse_get_parse_options() & QoreParseOptions::NO_CHAR_TYPE)) {
        return nullptr;
    }

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

// decides whether the fold path can convert one container element into another; defined below
static bool qore_container_value_may_convert_to(const QoreTypeInfo* target_ti, const QoreTypeInfo* source_ti);

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

// Compares the element types of a source container against those of a target container the fold loops in
// QoreTypeSpec::acceptInputComplexHash()/acceptInputComplexList() will convert element by element.
//
// An element type that the target does not accept may still be converted by those loops.  The conversion runs
// for every container assignment, so rejecting the container here would make the parser refuse a value the
// runtime goes on to accept: the same literal was a parse error where its type was known and folded silently
// where it arrived through an 'auto' expression.  Such a pair is reported as a match that needs a runtime
// filter and may fail, so the verdict is reached once, by the conversion, against the key or index that
// actually failed.
//
// This is asked only where the source really is a container whose elements are folded one at a time.  A
// softlist that wraps a single value into a one-element list matches that value against the element type
// through match_type() instead: nothing folds it, so admitting it here would accept at parse time what
// assignment then refuses.  See design/container-element-folding.md.
qore_type_result_e match_container_element_type(const QoreTypeInfo* this_type, const QoreTypeInfo* that_type,
        bool& may_not_match, bool& may_need_filter) {
    qore_type_result_e res = match_type(this_type, that_type, may_not_match, may_need_filter);
    if (res == QTI_NOT_EQUAL && qore_container_value_may_convert_to(this_type, that_type)) {
        may_not_match = true;
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
        return uQc();
    }
    if (typespec == QTS_PARAMCLASS) {
        return getParameterizedClassTypeInfo()->getBaseClass();
    }
    return nullptr;
}

const QoreParameterizedClassTypeInfo* QoreTypeSpec::getParameterizedClassTypeInfo() const {
    return typespec == QTS_PARAMCLASS ? static_cast<const QoreParameterizedClassTypeInfo*>(uTi()) : nullptr;
}

const char* QoreTypeSpec::getTypeName() const {
    switch (typespec) {
        case QTS_CLASS:
            return uQc()->getName();

        default:
            return QoreTypeInfo::getName(getTypeInfo());
    }
    assert(false);
    return nullptr;
}

const char* QoreTypeSpec::getSimpleTypeName() const {
    switch (typespec) {
        case QTS_CLASS:
            return uQc()->getName();

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
    // an expression whose type is a reference (a call returning a reference) gives a reference at run time, and a
    // reference is not accepted for a target that is not a reference (a parameter, an lvalue), whose type is this
    // one; reading a variable of a reference type gives the referenced value, whose type is the referenced type.
    // So a match is possible at most, never guaranteed: a variant whose parameter is matched so is not committed at
    // parse time (see QoreFunction::parseFindVariant()), and the call is resolved at run time (issue #5686)
    switch (t.typespec) {
        case QTS_TYPE: {
            if (t.u.t == NT_REFERENCE) {
                may_not_match = true;
                return QTI_AMBIGUOUS;
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_COMPLEXREF: {
            may_not_match = true;
            // check if types match
            if (!t.uTi() || t.uTi()->return_vec.empty()) {
                return QTI_AMBIGUOUS;
            }
            qore_type_result_e rv = QTI_NOT_EQUAL;
            for (auto& rt : t.uTi()->return_vec) {
                qore_type_result_e max_result = QTI_NOT_EQUAL;
                qore_type_result_e res = checkMatchType(rt.spec, may_not_match, max_result);
                if (res == QTI_NOT_EQUAL && !may_not_match) {
                    may_not_match = true;
                }
                if (res > rv) {
                    rv = res;
                }
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
        // any value accepted by the element type is converted to a single-element list
        return QoreTypeInfo::parseAcceptsBaseType(uTi(), t) ? QTI_NEAR : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXHARDREF || typespec == QTS_HARDREF) {
        return t == NT_REFERENCE ? QTI_IDENT : QTI_NOT_EQUAL;
    } else if (typespec == QTS_COMPLEXREF) {
        if (t == NT_REFERENCE) {
            return QTI_NEAR;
        }
        if (QoreTypeInfo::hasType(uTi())) {
            return QoreTypeInfo::parseAcceptsReturns(uTi(), t) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
        }
        return QTI_WILDCARD;
    } else if (typespec == QTS_ENUM) {
        // Enum types match their underlying base type at runtime
        qore_type_t bt = QoreTypeInfo::getBaseType(uEd()->getBaseTypeInfo());
        return t == bt ? QTI_IDENT : QTI_NOT_EQUAL;
    }
    if (u.t == NT_ALL || u.t == NT_REFERENCE) {
        return QTI_WILDCARD;
    }
    return u.t == t ? QTI_IDENT : QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeSpec::matchReturnType(qore_type_t t) const {
    if (typespec == QTS_COMPLEXSOFTLIST) {
        return t == NT_LIST ? QTI_IDENT : QTI_NOT_EQUAL;
    }
    return matchType(t);
}

static QORE_INTERN_ALWAYS_INLINE bool type_spec_accept_object(const QoreClass& type_class, const QoreClass& object_class, bool& priv_error) {
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

// Decides whether a container element type that the target element type does not already accept may
// still be converted into it, one element at a time, by the fold loops in the two callers below.
//
// The fold path exists so that generically-typed data -- a parsed JSON document, a hash literal, the
// result of a map expression -- can be adopted into a declared structure; plain lvalue assignment
// does no such conversion, so this is deliberately more permissive than QoreTypeInfo::parseAccepts().
// Eligibility is therefore a question of *shape*: a hash-shaped source element may be folded into a
// hash-shaped target element, and a list-shaped one into a list-shaped target.  The source's value
// type is intentionally not consulted, because it is the per-element conversion in the callers that
// decides whether the values actually fit; a field that does not still fails there, with an error
// naming the offending key.
//
// Requiring the source value type to be exactly auto/auto!, as this once did, made acceptance depend
// on whether a literal's values happened to be homogeneous.  {"value": 1, "format": "a"} infers
// hash<auto> and was folded, while the equally valid {"value": "x", "format": "a"} infers
// hash<string, string> and was rejected outright -- before any field was examined -- purely because
// both of its values were strings.
//
// Hashdecl sources are not eligible and are left to parseAccepts() and the descendant/parameterized
// rules: reinterpreting one declared structure as another is not the adoption of generic data, and
// lvalue assignment rejects it too.  getComplexHashValueType() returns nullptr for QTS_HASHDECL, so
// they do not reach the hash branch here.
static bool qore_container_value_may_convert_to(const QoreTypeInfo* target_ti,
        const QoreTypeInfo* source_ti) {
    if (!QoreTypeInfo::hasType(target_ti) || !QoreTypeInfo::hasType(source_ti)) {
        return false;
    }

    if (QoreTypeInfo::getComplexHashValueType(source_ti)
            && QoreTypeInfo::parseAcceptsBaseType(target_ti, NT_HASH)) {
        return true;
    }

    return QoreTypeInfo::getComplexListValueType(source_ti)
        && QoreTypeInfo::parseAcceptsBaseType(target_ti, NT_LIST);
}

bool QoreTypeInfo::mayFoldContainerValueTo(const QoreTypeInfo* target_ti, const QoreTypeInfo* source_ti) {
    // both sides must be containers of the same shape; it is their element types that the fold loops
    // convert, so the eligibility question is asked about those
    const QoreTypeInfo* target_elem = QoreTypeInfo::getComplexListValueType(target_ti);
    const QoreTypeInfo* source_elem = QoreTypeInfo::getComplexListValueType(source_ti);
    if (target_elem && source_elem) {
        return qore_container_value_may_convert_to(target_elem, source_elem);
    }

    target_elem = QoreTypeInfo::getComplexHashValueType(target_ti);
    source_elem = QoreTypeInfo::getComplexHashValueType(source_ti);
    return target_elem && source_elem && qore_container_value_may_convert_to(target_elem, source_elem);
}

namespace {
// Default internal entries have canonical semantics, including when entered from a legacy native callback.
// Explicit lexical entries carry their value through recursive checks without changing a shared type.
struct DefaultTypeMatchPolicy {
    //! a deleted object does not match a class type (issue #3272: instanceof, cast<>)
    static constexpr bool deleted_object_keeps_class = false;
    //! true if SQL NULL can be accepted by a type that does not accept it (see QoreTypeInfo::acceptsBrokenSoftNull())
    static constexpr bool may_accept_broken_soft_null = false;

    qore_type_result_e value(const QoreTypeInfo* ti, const QoreValue& n, bool fold) const {
        return fold ? QoreTypeInfo::runtimeAcceptsValueOrFold(ti, n) : QoreTypeInfo::runtimeAcceptsValue(ti, n);
    }

    qore_type_result_e spec(const QoreTypeSpec& spec, const QoreValue& n, bool exact, bool fold) const {
        return spec.runtimeAcceptsValue(n, exact, fold);
    }
};

struct LexicalTypeMatchPolicy {
    static constexpr bool deleted_object_keeps_class = false;
    static constexpr bool may_accept_broken_soft_null = true;

    LexicalSemanticPolicy policy;

    qore_type_result_e value(const QoreTypeInfo* ti, const QoreValue& n, bool fold) const {
        return QoreTypeInfo::runtimeAcceptsValue(ti, n, policy, fold);
    }

    qore_type_result_e spec(const QoreTypeSpec& spec, const QoreValue& n, bool exact, bool fold) const {
        return spec.runtimeAcceptsValue(n, exact, fold, policy);
    }
};

//! the match of a call argument in the run-time selection of a variant: a deleted object keeps its class, as at parse
//! time and for a variant selected at parse time (issue #6159); using it in the called code raises
//! OBJECT-ALREADY-DELETED
template <typename BasePolicy>
struct CallArgTypeMatchPolicy : BasePolicy {
    static constexpr bool deleted_object_keeps_class = true;

    qore_type_result_e value(const QoreTypeInfo* ti, const QoreValue& n, bool fold) const {
        if (!QoreTypeInfo::hasType(ti)) {
            return QTI_WILDCARD;
        }
        for (auto& t : ti->getAcceptSpecs()) {
            qore_type_result_e rv = t.spec.runtimeAcceptsValueImpl(n, t.exact, fold, *this);
            if (rv != QTI_NOT_EQUAL) {
                // a typed callable selects only the callables of compatible signatures (qore#6660)
                return ti->complexCodeAcceptsCallable(n) ? rv : QTI_NOT_EQUAL;
            }
        }
        // SQL NULL, which no class type accepts, is matched as by the base policy
        if constexpr (BasePolicy::may_accept_broken_soft_null) {
            if (n.getType() == NT_NULL) {
                return BasePolicy::value(ti, n, fold);
            }
        }
        return QTI_NOT_EQUAL;
    }

    qore_type_result_e spec(const QoreTypeSpec& spec, const QoreValue& n, bool exact, bool fold) const {
        return spec.runtimeAcceptsValueImpl(n, exact, fold, *this);
    }
};


struct DefaultTypeFilterPolicy : DefaultTypeMatchPolicy {
    QoreHashNode* hash(const TypedHashDecl* hd, const QoreHashNode* value, ExceptionSink* xsink) const {
        return typed_hash_decl_private::get(*hd)->newHashFromType(value, xsink);
    }

    int push(QoreListNode& list, QoreValue value, ExceptionSink* xsink) const {
        return qore_list_private::pushCanonical(list, value, xsink);
    }

    bool retype(QoreValue& value, const QoreTypeInfo* ti, ExceptionSink* xsink) const {
        return QoreTypeInfo::retypeValue(value, ti, xsink);
    }

    void assignment(const QoreTypeInfo* ti, const char* desc, QoreValue& value, ExceptionSink* xsink) const {
        QoreTypeInfo::acceptAssignment(ti, desc, value, xsink);
    }

    int assign(LValueHelper& lvh, QoreValue value, const char* desc) const {
        return lvh.assign(value, desc);
    }

    void input(const QoreTypeInfo& ti, ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
            const char* param_name, QoreValue& n, LValueHelper* lvhelper) const {
        ti.acceptInputIntern(xsink, arg_type, obj, param_num, param_name, n, lvhelper);
    }

    bool brokenSoftTypes() const {
        return false;
    }

    void map(const QoreAcceptSpec& accept, QoreValue& n, ExceptionSink* xsink) const {
        accept.map(n, xsink);
    }
};

struct LexicalTypeFilterPolicy : LexicalTypeMatchPolicy {
    QoreHashNode* hash(const TypedHashDecl* hd, const QoreHashNode* value, ExceptionSink* xsink) const {
        return typed_hash_decl_private::get(*hd)->newHashFromType(value, xsink, policy);
    }

    int push(QoreListNode& list, QoreValue value, ExceptionSink* xsink) const {
        return qore_list_private::get(list)->push(value, xsink, policy);
    }

    bool retype(QoreValue& value, const QoreTypeInfo* ti, ExceptionSink* xsink) const {
        return QoreTypeInfo::retypeValue(value, ti, xsink, policy);
    }

    void assignment(const QoreTypeInfo* ti, const char* desc, QoreValue& value, ExceptionSink* xsink) const {
        QoreTypeInfo::acceptAssignment(ti, desc, value, xsink, policy);
    }

    int assign(LValueHelper& lvh, QoreValue value, const char* desc) const {
        return lvh.assign(value, desc, policy);
    }

    explicit LexicalTypeFilterPolicy(LexicalSemanticPolicy value) : LexicalTypeMatchPolicy{value} {
    }

    void input(const QoreTypeInfo& ti, ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
            const char* param_name, QoreValue& n, LValueHelper* lvhelper) const {
        ti.acceptInputIntern(xsink, arg_type, obj, param_num, param_name, n, policy, lvhelper);
    }

    bool brokenSoftTypes() const {
        return policy.brokenSoftTypes();
    }

    void map(const QoreAcceptSpec& accept, QoreValue& n, ExceptionSink* xsink) const {
        if (accept.lexical_map) {
            accept.lexical_map(n, xsink, policy);
        } else {
            accept.map(n, xsink);
        }
    }
};
}

void qore_lexical_soft_bool_filter(QoreValue& value, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
    discard(value.assign(policy.getAsBool(value)), xsink);
}

bool QoreTypeSpec::acceptInputComplexHash(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
        bool& err) const {
    return acceptInputComplexHashImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper,
        h, err, DefaultTypeFilterPolicy{});
}

template <typename FilterPolicy>
bool QoreTypeSpec::acceptInputComplexHashImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreHashNode* h,
        bool& err, FilterPolicy policy) const {
    assert(!err);
    const QoreTypeInfo* ti = h->getValueTypeInfo();
    // a container whose value type is accepted without a check is taken as it is; a value type with the same
    // signature that the target still validates (date for date<absolute> or date<relative>) is folded and checked
    // (issue #5782)
    if (QoreTypeInfo::acceptsInputUnchecked(uTi(), ti)) {
        return true;
    }
    if (ti && QoreTypeInfo::hasType(ti) && !QoreTypeInfo::parseAccepts(uTi(), ti)
            && !qore_container_value_may_convert_to(uTi(), ti)) {
        return false;
    }
    // a hash with a hashdecl type is not a hash with a specific value type, as for the selection of a variant
    // (runtimeAcceptsValue()): its member values are not folded into the value type (folding the values of a hash
    // with no other reference would break the member types of its hashdecl) (issue #5916)
    if (h->getHashDecl()) {
        return false;
    }

    // try to fold values into our type; value types are not identical;
    // we have to get a new hash
    // The resulting hash carries the selected member type, including when the declaration is a union.
    const QoreTypeInfo* complex_type = qore_get_complex_hash_type(uTi());
    if (!h->is_unique()) {
        AbstractQoreNode* p = n.assign(h = qore_hash_private::get(*h)->copy(complex_type));
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
        qore_hash_private* hp = qore_hash_private::get(*h);
        hp->complexTypeInfo = complex_type;
        // the hash converted to another type is a new hash, as the copy above is, which can be changed also when the
        // hash it was converted from was freed under a weak reference (qore#7199)
        hp->valid = true;
    }

    // now we have to fold the value types into our type
    HashIterator i(h);
    while (i.next()) {
        hash_assignment_priv ha(*qore_hash_private::get(*h), *qhi_priv::get(i)->i);
        QoreValue hn(ha.swap(QoreValue()));

        if (policy.value(uTi(), hn, true) == QTI_NOT_EQUAL
                && !policy.retype(hn, uTi(), xsink)) {
            ha.swap(hn);
            err = true;
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while folding values into type 'hash<%s>')",
                    QoreTypeInfo::getName(uTi()));
            }
            return true;
        }
        policy.input(*uTi(), xsink, arg_type, obj, param_num, param_name, hn, lvhelper);
        ha.swap(hn);
        if (xsink && *xsink) {
            err = true;
            // enrich exception so that it's not confusing
            xsink->appendLastDescription(" (while folding values into type 'hash<%s>')", QoreTypeInfo::getName(uTi()));
            return true;
        }
    }

    return true;
}

bool QoreTypeSpec::acceptInputComplexList(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
        bool& err) const {
    return acceptInputComplexListImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper,
        l, err, DefaultTypeFilterPolicy{});
}

template <typename FilterPolicy>
bool QoreTypeSpec::acceptInputComplexListImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const char* arg_type,
        bool obj, int param_num, const char* param_name, QoreValue& n, LValueHelper* lvhelper, QoreListNode* l,
        bool& err, FilterPolicy policy) const {
    const QoreTypeInfo* ti = l->getValueTypeInfo();
    // a container whose value type is accepted without a check is taken as it is; a value type with the same
    // signature that the target still validates (date for date<absolute> or date<relative>) is folded and checked
    // (issue #5782)
    if (QoreTypeInfo::acceptsInputUnchecked(uTi(), ti)) {
        return true;
    }
    if (ti && QoreTypeInfo::hasType(ti) && !QoreTypeInfo::parseAccepts(uTi(), ti)
            && !qore_container_value_may_convert_to(uTi(), ti)) {
        return false;
    }

    // try to fold values into our type; value types are not identical;
    // we have to get a new list
    qore_list_private* lp;
    if (!l->is_unique()) {
        // the copy is a list value, so it gets the list type even when the target type is a soft list
        AbstractQoreNode* p = n.assign(l = qore_list_private::get(*l)->copy(qore_get_complex_list_type(uTi())));
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
        const QoreTypeInfo* complex_type = qore_get_complex_list_type(uTi());
        lp->complexTypeInfo = complex_type;
        // the list converted to another type is a new list, as the copy above is, which can be changed also when the
        // list it was converted from was freed under a weak reference (qore#7199)
        lp->valid = true;
    }

    // now we have to fold the value types into our type
    for (size_t i = 0; i < l->size(); ++i) {
        QoreValue ln(lp->takeExists(i));
        if (policy.value(uTi(), ln, true) == QTI_NOT_EQUAL
                && !policy.retype(ln, uTi(), xsink)) {
            lp->swap(i, ln);
            err = true;
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while folding values into type 'list<%s>')",
                    QoreTypeInfo::getName(uTi()));
            }
            return true;
        }
        policy.input(*uTi(), xsink, arg_type, obj, param_num, param_name, ln, lvhelper);
        lp->swap(i, ln);
        if (xsink && *xsink) {
            err = true;
            // enrich exception so that it's not confusing
            xsink->appendLastDescription(" (while folding values into type 'list<%s>')", QoreTypeInfo::getName(uTi()));
            return true;
        }
    }

    return true;
}

template <typename FilterPolicy>
static bool qore_type_materialize_iterator_to_list(ExceptionSink* xsink, QoreValue& n, LValueHelper* lvhelper,
        const QoreTypeInfo* value_type, bool& err, FilterPolicy policy) {
    if (n.getType() != NT_OBJECT) {
        return false;
    }

    QoreObject* iterator_obj = n.get<QoreObject>();
    AbstractIteratorHelper iterator(xsink, "hard-list iterator materialization", iterator_obj);
    if (xsink && *xsink) {
        err = true;
        return true;
    }
    if (!iterator) {
        return false;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(value_type), xsink);
    size_t i = 0;
    while (iterator.next(xsink)) {
        if (xsink && *xsink) {
            err = true;
            return true;
        }
        if (i && !(i % 100) && qore_check_cancel(xsink, "hard-list iterator materialization")) {
            err = true;
            return true;
        }

        QoreValue value = iterator.getValue(xsink);
        if (xsink && *xsink) {
            err = true;
            return true;
        }
        if (policy.push(**list, value, xsink)) {
            err = true;
            if (xsink && *xsink) {
                xsink->appendLastDescription(" (while materializing iterator element %lu into type 'list<%s>')",
                    i, QoreTypeInfo::getName(value_type));
            }
            return true;
        }
        ++i;
    }
    if (xsink && *xsink) {
        err = true;
        return true;
    }

    AbstractQoreNode* old = n.assign(list.release());
    if (lvhelper) {
        lvhelper->saveTemp(old);
    } else {
        discard(old, xsink);
        if (xsink && *xsink) {
            err = true;
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

//! Returns true if a type's conversion left a value as it was: the same node, or the same type and bits
/** A value compared with itself: a float NaN is unchanged, although it is not equal to itself
*/
static bool qore_value_unchanged_by_conversion(const QoreValue& orig, const QoreValue& conv) {
    if (orig.hasNode() || conv.hasNode()) {
        return orig.getInternalNode() == conv.getInternalNode();
    }
    if (orig.getType() != conv.getType()) {
        return false;
    }
    if (orig.getType() == NT_FLOAT) {
        double a = orig.getAsFloat();
        double b = conv.getAsFloat();
        return !memcmp(&a, &b, sizeof(double));
    }
    return orig.isEqualHard(conv);
}

bool QoreTypeSpec::acceptInput(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const QoreAcceptSpec& accept,
        const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
        LValueHelper* lvhelper) const {
    return acceptInputImpl(xsink, typeInfo, accept, arg_type, obj, param_num, param_name, n, lvhelper,
        DefaultTypeFilterPolicy{});
}

namespace {
//! Carries only a declaration's initializer coercion; the expression and storage retain their original metadata.
class QoreInitializerFilterTypeInfo : public QoreTypeInfo {
public:
    QoreInitializerFilterTypeInfo(const QoreTypeInfo* declared_type, LexicalSemanticPolicy policy,
            bool instance_member)
            : QoreTypeInfo(QoreTypeInfo::getName(declared_type),
                q_accept_vec_t{{QoreInitializerFilterTypeSpec(this), nullptr}}, q_return_vec_t{{NT_ALL}}),
              declared_type(declared_type), policy(policy), instance_member(instance_member) {
        // Generic substitution belongs to the lexical conversion entry below. The ordinary initialization
        // sequence still substitutes its one type pointer, as before, without detecting a descriptor first.
        setContainsTypeParameterCache(false);
    }

    void filter(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num, const char* param_name,
            QoreValue& value, LValueHelper* lvhelper) const {
        // Instance initialization already installs the object's constructor context. Hashdecl specialization
        // rebinds the descriptor to its substituted member type before any default is evaluated.
        const QoreTypeInfo* target = declared_type;
        if (instance_member) {
            const QoreObject* object = runtime_get_stack_object();
            assert(object);
            // The caller's receiver remains active until the constructor's call frame is installed. Use the
            // object being initialized explicitly, just as the canonical initMember() entry does.
            // (an object of a class without type arguments gives those of the generic classes that it inherits:
            // qore#6027)
            const qore_object_private* opriv = qore_object_private::get(*object);
            target = qore_substitute_type_params_if_needed(declared_type,
                opriv->instantiated_type ? opriv->instantiated_type : opriv->theclass->getTypeInfo());
        }
        if (QoreTypeInfo::hasType(target)) {
            target->acceptInputIntern(xsink, arg_type, obj, param_num, param_name, value, policy, lvhelper);
        } else if (target != autoTypeInfo) {
            QoreTypeInfo::stripTypeInfo(value, xsink, lvhelper);
        }
    }

private:
    const QoreTypeInfo* declared_type;
    const LexicalSemanticPolicy policy;
    const bool instance_member;

    bool canConvertToScalarImpl() const override {
        return false;
    }
};
}

std::shared_ptr<const QoreTypeInfo> qore_make_initializer_filter(const QoreTypeInfo* type_info,
        LexicalSemanticPolicy policy, bool instance_member) {
    if (!policy.needsFilter() || !QoreTypeInfo::hasType(type_info)) {
        return {};
    }
    return std::make_shared<QoreInitializerFilterTypeInfo>(type_info, policy, instance_member);
}

bool QoreTypeSpec::acceptsEnumDecl(const QoreEnumDecl* ed) const {
    assert(typespec == QTS_ENUM);
    return ed == uEd() || qore_enum_decl_private::get(*ed)->parseEqual(*qore_enum_decl_private::get(*uEd()));
}

//! returns true if the type has an enum type that accepts the enum value \a n (qore#6277)
static bool qore_type_info_has_enum_spec_for(const QoreTypeInfo& typeInfo, const QoreValue& n) {
    assert(n.isEnum());
    const QoreEnumDecl* ed = n.getEnumMember()->getEnumDecl();
    for (auto& t : typeInfo.getAcceptSpecs()) {
        if (t.spec.getTypeSpec() == QTS_ENUM && t.spec.acceptsEnumDecl(ed)) {
            return true;
        }
    }
    return false;
}

bool QoreTypeInfo::unionHasEnumTypeFor(const QoreTypeInfo* ti, const QoreValue& n) {
    return n.isEnum() && getUnionType(ti) && qore_type_info_has_enum_spec_for(*ti, n);
}

//! checks the target of a weak reference that an lvalue holds for the binding of a reference to it (qore#7433)
/** The weak reference has the type of its target: the target is checked as the assignment of the reference's type
    checks it, and the lvalue keeps the weak reference.  A destroyed object is its target, as when a weak reference is
    read, and as for an lvalue that holds a destroyed object.  A target that the type would convert to another value
    (ex: a hash<string, int> for a reference<hash>) is refused, as the weak reference would not designate the converted
    value.

    @param weak the weak reference
    @param target set to the target, with a reference of its own; the caller releases it (an lvalue's lock can be held
    while a reference is bound to it, and the release of the last reference to the target runs its destructors)
    @param conv set to the target converted by the type, with a reference of its own; released as \a target

    @return 0 if the target is accepted as it is, -1 if an exception was raised
*/
template <typename FilterPolicy>
static __attribute__((noinline)) int qore_reference_bind_weak_check(const QoreTypeInfo* target_ti,
        const QoreValue& weak, FilterPolicy policy, ValueHolder& target, ValueHolder& conv, ExceptionSink* xsink) {
    switch (weak.getType()) {
        case NT_WEAKREF:
            // the object, also if it has been destroyed
            if (QoreObject* obj = weak.get<const WeakReferenceNode>()->get()) {
                target = QoreValue(obj).refSelf();
            }
            break;
        case NT_WEAKREF_HASH:
            // a hash freed under the weak reference is an empty one
            target = QoreValue(weak.get<const WeakHashReferenceNode>()->lock());
            break;
        default:
            assert(weak.getType() == NT_WEAKREF_LIST);
            // a list freed under the weak reference is an empty one
            target = QoreValue(weak.get<const WeakListReferenceNode>()->lock());
            break;
    }
    conv = target->refSelf();
    policy.assignment(target_ti, "<reference>", *conv, xsink);
    if (*xsink) {
        return -1;
    }
    if (!qore_value_unchanged_by_conversion(*target, *conv)) {
        xsink->raiseException("RUNTIME-TYPE-ERROR", "<reference> expects type '%s', but got a weak reference to type "
            "'%s', which the type would convert to another value that the weak reference does not designate",
            QoreTypeInfo::getName(target_ti), target->getFullTypeName());
        return -1;
    }
    return 0;
}

//! binds a reference to an lvalue that holds a weak reference (qore#7433)
/** The lvalue keeps the weak reference if its target is accepted (see qore_reference_bind_weak_check()): nothing is
    changed.

    @param weak the weak reference, with a reference that the call takes
*/
template <typename FilterPolicy>
static __attribute__((noinline)) void qore_reference_bind_weak(LValueHelper& lvh, const QoreTypeInfo* target_ti,
        QoreValue weak, FilterPolicy policy, ExceptionSink* xsink) {
    ValueHolder target(xsink);
    ValueHolder conv(xsink);
    qore_reference_bind_weak_check(target_ti, weak, policy, target, conv, xsink);
    // the references are released by the helper's destructor, after the lvalue's lock: the release of the last
    // reference to the target can run destructors, which can need the lock
    lvh.saveTemp(weak);
    lvh.saveTemp(target.release());
    lvh.saveTemp(conv.release());
    if (!*xsink) {
        lvh.suppressObjectScan();
    }
}

template <typename FilterPolicy>
bool QoreTypeSpec::acceptInputImpl(ExceptionSink* xsink, const QoreTypeInfo& typeInfo, const QoreAcceptSpec& accept,
        const char* arg_type, bool obj, int param_num, const char* param_name, QoreValue& n,
        LValueHelper* lvhelper, FilterPolicy policy) const {
    bool priv_error = false;
    bool ok = false;

    //printd(5, "QoreTypeInfo::acceptInput() typeInfo: %s spec: %s arg_type: %s val: %s: OK\n", QoreTypeInfo::getName(&typeInfo), getName(), arg_type, n.getFullTypeName());

    qore_type_t t = n.getType();
    switch (typespec) {
        case QTS_INITIALIZER_FILTER:
            // Delegate before the ordinary mapping-filter annotation: diagnostics describe the actual target,
            // at the existing initializer filter point, with its existing holder and exception sink still alive.
            static_cast<const QoreInitializerFilterTypeInfo*>(uTi())->filter(xsink, arg_type, obj, param_num,
                param_name, n, lvhelper);
            return true;

        case QTS_CLASS: {
            if (t == NT_OBJECT) {
                const QoreObject* obj = n.get<const QoreObject>();
                const qore_class_private* target = qore_class_private::get(*uQc());
                if (!obj->getInstantiatedTypeInfo() || !target->hasTypeParams()
                        || target->rawAcceptsParameterized()) {
                    ok = type_spec_accept_object(*uQc(), *obj->getClass(), priv_error);
                }
            } else if (t == NT_WEAKREF) {
                const QoreObject* obj = n.get<const WeakReferenceNode>()->get();
                const qore_class_private* target = qore_class_private::get(*uQc());
                if (!obj->getInstantiatedTypeInfo() || !target->hasTypeParams()
                        || target->rawAcceptsParameterized()) {
                    ok = type_spec_accept_object(*uQc(), *obj->getClass(), priv_error);
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
            if (obj && obj->getInstantiatedTypeInfo() == uTi()) {
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
                    if (QoreTypeInfo::equal(mapped_type, uTi())
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
                // a hash keeps its hashdecl when it is freed
                hd = n.get<const WeakHashReferenceNode>()->borrow()->getHashDecl();
            }
            if (hd) {
                const typed_hash_decl_private* target = typed_hash_decl_private::get(*uHd());
                const typed_hash_decl_private* source = typed_hash_decl_private::get(*hd);
                bool type_may_not_match = false;
                bool type_may_need_filter = false;
                // Accept if same hashdecl or source is a descendant of target
                if (source->equal(*target)
                        || qore_parameterized_hashdecl_accepts(uHd(), hd, type_may_not_match, type_may_need_filter)
                            != QTI_NOT_EQUAL
                        || (!target->isParameterizedHashDecl() && source->isDescendantOf(*target))) {
                    ok = true;
                }
            }
            break;
        }
        case QTS_COMPLEXHASH: {
            if (t == NT_HASH) {
                if (is_auto_vti(uTi())) {
                    ok = true;
                    break;
                }
                // a union with a member of the hash's own type keeps it as it is (qore#6407)
                if (typeInfo.unionAcceptsExactly(n, NT_HASH)) {
                    return true;
                }
                QoreHashNode* h = n.get<QoreHashNode>();
                bool err = false;
                ok = acceptInputComplexHashImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, h,
                    err, policy);
                if (err) {
                    return true;
                }
            } else if (t == NT_WEAKREF_HASH) {
                if (is_auto_vti(uTi())) {
                    ok = true;
                    break;
                }
                // checked with a reference of its own, or as a new empty hash if it was freed (qore#7198)
                ReferenceHolder<QoreHashNode> h(n.get<WeakHashReferenceNode>()->lock(), xsink);
                bool err = false;
                ok = acceptInputComplexHashImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, *h,
                    err, policy);
                if (err) {
                    return true;
                }
            }
            break;
        }
        case QTS_COMPLEXSOFTLIST:
        case QTS_COMPLEXLIST: {
            if (n.getType() == NT_LIST) {
                if (is_auto_vti(uTi())) {
                    ok = true;
                    break;
                }
                // a union with a member of the list's own type keeps it as it is (qore#6407)
                if (typeInfo.unionAcceptsExactly(n, NT_LIST)) {
                    return true;
                }
                QoreListNode* l = n.get<QoreListNode>();
                bool err = false;
                ok = acceptInputComplexListImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, l,
                    err, policy);
            } else if (n.getType() == NT_WEAKREF_LIST) {
                // a weak reference to a list is accepted like the list it refers to, as for a weak reference to a
                // hash above
                if (is_auto_vti(uTi())) {
                    ok = true;
                    break;
                }
                // checked with a reference of its own, or as a new empty list if it was freed (qore#7198)
                ReferenceHolder<QoreListNode> l(n.get<WeakListReferenceNode>()->lock(), xsink);
                bool err = false;
                ok = acceptInputComplexListImpl(xsink, typeInfo, arg_type, obj, param_num, param_name, n, lvhelper, *l,
                    err, policy);
                if (err) {
                    return true;
                }
            } else if (typespec == QTS_COMPLEXLIST) {
                bool err = false;
                ok = qore_type_materialize_iterator_to_list(xsink, n, lvhelper, uTi(), err, policy);
                if (err) {
                    return true;
                }
            } else if (typespec == QTS_COMPLEXSOFTLIST) {
                // an enum value is left to an enum type of the union, which keeps it as an enum value, and not
                // wrapped in a list (qore#6277)
                if (n.isEnum() && typeInfo.getAcceptSpecs().size() > 1
                        && qore_type_info_has_enum_spec_for(typeInfo, n)) {
                    break;
                }
                // a union with a member of the value's own type keeps it as it is, which a soft list would wrap in a
                // list (qore#6407)
                if (typeInfo.unionAcceptsExactly(n, n.getType())) {
                    return true;
                }
                // see if value matches
                if (policy.value(uTi(), n, true) > 0) {
                    ok = true;
                }
            }
            break;
        }
        case QTS_COMPLEXBUFFER: {
            if (n.getType() == NT_BUFFER) {
                QoreBufferNode* b = n.get<QoreBufferNode>();
                bool needs_nullable_copy = false;
                ok = qore_complex_buffer_accepts_runtime(uTi(), *b, needs_nullable_copy);
                if (needs_nullable_copy) {
                    QoreBufferNode* copy = qore_buffer_node_private(b).copy(true, xsink);
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
                //printd(5, "cr: %p '%s' == %p '%s': %d\n", uTi(), QoreTypeInfo::getName(uTi()), ti, QoreTypeInfo::getName(ti), QoreTypeInfo::outputSuperSetOf(ti, uTi()));
                // first check types before instantiating reference
                if (QoreTypeInfo::outputSuperSetOf(ti, uTi())) {
                    // a variable of the reference's type whose value the assignment below would leave as it is is
                    // not read and assigned (qore#7264); -1: the exception that resolving the reference raises, as
                    // for the error described below
                    if (qore_reference_binding_keeps_value(r, uTi(), xsink)) {
                        ok = true;
                        break;
                    }
                    // issue #2891: do not create a value in the source reference if none already exists
                    // do not process if there is no type restriction
                    // the value read from the lvalue is assigned back to the same lvalue in order to apply the
                    // reference's type restriction (which can fold container value types); remember the node so
                    // that the recursive-reference scan can be skipped when nothing changed.  Every path that
                    // replaces the value hands the old one to LValueHelper::saveTemp(), which defers the
                    // dereference to the helper's destructor, so this pointer is still backed by a live reference
                    // at the comparison below and its address cannot have been reused
                    auto assign_back = [target_ti = uTi(), policy, xsink](LValueHelper& lvh) {
                        QoreValue val = lvh.getReferencedValue();
                        if (val.isNothing()) {
                            return;
                        }
                        qore_type_t vt = val.getType();
                        if (vt == NT_WEAKREF || vt == NT_WEAKREF_HASH || vt == NT_WEAKREF_LIST) [[unlikely]] {
                            qore_reference_bind_weak(lvh, target_ti, val, policy, xsink);
                            return;
                        }
                        const AbstractQoreNode* orig = val.getInternalNode();
                        lvh.setTypeInfo(target_ti);
                        if (!policy.assign(lvh, val, "<reference>") && orig && lvh.getInternalNode() == orig) {
                            // the identical node is still in place, so the set of objects reachable from the
                            // lvalue cannot have changed; skip the cycle scan, which would otherwise walk the
                            // entire reachable object graph on every reference argument binding
                            lvh.suppressObjectScan();
                        }
                    };
                    // the variable is resolved for reading first: one that the reference cannot write, such as a
                    // read-only import, is left as it is when its value already has the reference's type
                    // (qore#5664)
                    // the lock of the assignment being checked can guard the referenced lvalue (qore#7380)
                    std::optional<LValueHelper> lvh;
                    lvh.emplace(*r, xsink, LValueHelper::ReadAccess(), LValueHelper::ReferenceBind());
                    if (*lvh && lvh->hasReusedBindLock()) {
                        // under a lock of the assignment being checked the value is only inspected, where it is:
                        // nothing is changed, so no object scan is needed, and no reference is taken, whose release
                        // could take the lock that the assignment holds (qore#7380)
                        lvh->suppressObjectScan();
                        const QoreValue current = lvh->getValue();
                        if (!current.isNothing()) {
                            qore_type_result_e rv = QoreTypeInfo::runtimeAcceptsValue(uTi(), current);
                            // only a value that certainly needs no conversion is accepted: a value of exactly the
                            // type, with or without NOTHING, or a valid object of a compatible class for a type that
                            // does not wrap it in a list; any other value could be converted or rejected by a bind
                            bool unchanged;
                            // a weak reference is not checked here: the check takes a reference to its target, whose
                            // release could need the lock that the assignment holds, so it is refused here although a
                            // bind without the lock accepts it (qore#7433); a bind resolves a reference that the lvalue
                            // holds
                            if (current.isEnum() || current.getType() == NT_WEAKREF
                                    || current.getType() == NT_WEAKREF_HASH || current.getType() == NT_WEAKREF_LIST
                                    || current.getType() == NT_REFERENCE) {
                                unchanged = false;
                            } else if (current.getType() == NT_OBJECT) {
                                unchanged = rv != QTI_NOT_EQUAL && current.get<const QoreObject>()->isValid()
                                    && !QoreTypeInfo::parseAcceptsReturns(uTi(), NT_LIST);
                            } else {
                                const QoreTypeInfo* value_ti = current.getTypeInfo();
                                qore_type_t vt = current.getType();
                                // a plain scalar that the type returns as it is (an int for a softint); a date is not
                                // one of them, as a type can restrict it to absolute or relative dates
                                unchanged = value_ti == uTi() || get_or_nothing_type_check(value_ti) == uTi()
                                    || (rv == QTI_IDENT && (vt == NT_INT || vt == NT_FLOAT || vt == NT_NUMBER
                                        || vt == NT_STRING || vt == NT_BOOLEAN || vt == NT_BINARY));
                            }
                            if (rv == QTI_NOT_EQUAL) {
                                xsink->raiseException("RUNTIME-TYPE-ERROR", "<reference> expects type '%s', but got "
                                    "type '%s' instead", QoreTypeInfo::getName(uTi()), current.getFullTypeName());
                            } else if (!unchanged) {
                                xsink->raiseException("REFERENCE-BIND-ERROR", "cannot convert the value of a reference "
                                    "to type '%s' while assigning to an lvalue whose lock its path goes through",
                                    QoreTypeInfo::getName(uTi()));
                            }
                        }
                    } else if (*lvh && lvh->isWriteProtected()) {
                        bool needs_write = false;
                        {
                            ValueHolder val(lvh->getReferencedValue(), xsink);
                            // the variable's lock is released before the value is converted, as for an assignment
                            lvh.reset();
                            if (!val->isNothing()) {
                                // the value is converted as the assignment below would convert it: a value that the
                                // type rejects raises the same error as for a variable that can be written, and only
                                // a value that the conversion changes has to be written back
                                qore_type_t vt = val->getType();
                                if (vt == NT_WEAKREF || vt == NT_WEAKREF_HASH || vt == NT_WEAKREF_LIST) [[unlikely]] {
                                    // the variable keeps a weak reference whose target is accepted (qore#7433); the
                                    // variable's lock is not held, so the references are released here
                                    ValueHolder target(xsink);
                                    ValueHolder conv(xsink);
                                    qore_reference_bind_weak_check(uTi(), *val, policy, target, conv, xsink);
                                } else {
                                    ValueHolder conv(val->refSelf(), xsink);
                                    policy.assignment(uTi(), "<reference>", *conv, xsink);
                                    if (!*xsink) {
                                        needs_write = !qore_value_unchanged_by_conversion(*val, *conv);
                                    }
                                }
                            }
                        }
                        if (needs_write && !*xsink) {
                            // the value has to be changed: resolving the variable for writing raises the exception
                            // that says why it cannot be, unless it has become writable in the meantime
                            lvh.emplace(*r, xsink, LValueHelper::ReferenceBind());
                            // the path can differ from the one read above and reach a lock that the assignment holds,
                            // under which nothing can be written (qore#7380)
                            if (*lvh && lvh->hasReusedBindLock()) {
                                lvh->suppressObjectScan();
                                xsink->raiseException("REFERENCE-BIND-ERROR", "cannot convert the value of a reference "
                                    "to type '%s' while assigning to an lvalue whose lock its path goes through",
                                    QoreTypeInfo::getName(uTi()));
                            } else if (*lvh) {
                                assign_back(*lvh);
                            }
                        }
                    } else if (*lvh) {
                        assign_back(*lvh);
                    }
                    // the type of the reference has been accepted at this point, so this is never a type mismatch:
                    // an exception raised above, including one raised while resolving the lvalue, such as
                    // STACK-LIMIT-EXCEEDED in a long chain of references, is the error to report, and reporting a
                    // type error as well would name two identical types as a mismatch; otherwise, issue #2891: the
                    // lvalue may not exist, but we can still perform the assignment
                    ok = true;
                }
            }
            break;
        }
        case QTS_TYPE: {
            qore_type_t t = n.getType();
            if (n.isEnum()) {
                // a union with an enum type of the value's enum keeps the value as an enum value: a base type, or a
                // type that converts any value (softlist), does not accept it, and the enum type does (union<int, E>
                // takes an E value as an E value, wherever E is in the union, also in an optional type, and for
                // hash keys and members too; qore#6277); checked only for an enum value, so that the acceptance of
                // other values does not change
                if ((u.t != NT_ALL || accept.map) && typeInfo.getAcceptSpecs().size() > 1
                        && qore_type_info_has_enum_spec_for(typeInfo, n)) {
                    break;
                }
                // an enum value accepted by a plain type (e.g. "string s = EnumVal") is converted to its base value,
                // whatever stores it: a variable, a parameter, a member, or a hash key or list element of a container
                // whose value type is the plain type, as an assignment to the container's element converts it; only
                // an enum type or auto keeps the enum value (qore#6274)
                if (u.t != NT_ALL) {
                    QoreValue base_val = n.getEnumMember()->getValue();
                    base_val.ref();
                    n.discard(xsink);
                    n = base_val;
                }
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
            if (n.isEnum() && acceptsEnumDecl(n.getEnumMember()->getEnumDecl())) {
                ok = true;
            }
            break;
        }

        case QTS_TYPEPARAM:
        case QTS_WILDCARD:
            // a type parameter or a wildcard is replaced by its type argument before a value is accepted at runtime,
            // so its own spec accepts no value
            break;
    }

    if (ok) {
        assert(!priv_error);
        if (accept.map) {
            // a union with a member of the value's own type keeps the value as it is (qore#6407)
            if (typeInfo.unionAcceptsExactly(n, typespec == QTS_TYPE && u.t != NT_ALL ? u.t : n.getType())) {
                return true;
            }
            policy.map(accept, n, xsink);
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

// A parameterized class type is built once for each class and type arguments, so that the copy of a generic class in
// another Program (an imported class) has types of its own: they are the same type if the classes are the same class
// and the type arguments are the same types (qore#6575)
static __attribute__((noinline)) bool qore_parameterized_class_types_equal(const QoreParameterizedClassTypeInfo* a,
        const QoreParameterizedClassTypeInfo* b) {
    if (a->isOrNothing() != b->isOrNothing() || a->getArgCount() != b->getArgCount()) {
        return false;
    }
    if (!qore_class_private::get(*a->getBaseClass())->equal(*qore_class_private::get(*b->getBaseClass()))) {
        return false;
    }
    const std::vector<const QoreTypeInfo*>& aargs = a->getTypeArgs();
    const std::vector<const QoreTypeInfo*>& bargs = b->getTypeArgs();
    for (size_t i = 0, e = aargs.size(); i < e; ++i) {
        if (!QoreTypeInfo::equal(aargs[i], bargs[i])) {
            return false;
        }
    }
    return true;
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
            return qore_class_private::get(*uQc())->equal(*qore_class_private::get(*other.uQc()));
        case QTS_HASHDECL:
            return typed_hash_decl_private::get(*uHd())->equal(*typed_hash_decl_private::get(*other.uHd()));
        case QTS_COMPLEXHASH:
        case QTS_COMPLEXLIST:
        case QTS_COMPLEXSOFTLIST:
        case QTS_COMPLEXHARDREF:
        case QTS_COMPLEXREF:
            return QoreTypeInfo::equal(uTi(), other.uTi());
        case QTS_COMPLEXBUFFER:
            return uTi() == other.uTi();
        case QTS_PARAMCLASS:
            return u.ti == other.u.ti || qore_parameterized_class_types_equal(
                static_cast<const QoreParameterizedClassTypeInfo*>(u.ti),
                static_cast<const QoreParameterizedClassTypeInfo*>(other.u.ti));
        case QTS_TYPEPARAM:
        case QTS_WILDCARD:
        case QTS_INITIALIZER_FILTER:
            return uTi() == other.uTi();
        case QTS_HARDREF:
             return true;
        case QTS_ENUM:
            return uEd() == other.uEd();
    }
    return false;
}

bool QoreTypeSpec::operator!=(const QoreTypeSpec& other) const {
   return !(*this == other);
}


//! a reference to the list or hash of a weak reference that the type match reads, released when it is taken only
//! (qore#7375): the match of any other value costs no exception sink
template <typename T>
struct qore_weak_target_ref {
    T* target = nullptr;

    DLLLOCAL qore_weak_target_ref() = default;
    qore_weak_target_ref(const qore_weak_target_ref&) = delete;
    qore_weak_target_ref& operator=(const qore_weak_target_ref&) = delete;

    DLLLOCAL ~qore_weak_target_ref() {
        if (target) [[unlikely]] {
            // the release of a last reference raises its destructors' exceptions here, as
            // WeakListReferenceNode::getAsBoolImpl() does
            ExceptionSink xsink;
            target->deref(&xsink);
        }
    }
};

qore_type_result_e QoreTypeSpec::runtimeAcceptsValue(const QoreValue& n, bool exact, bool fold) const {
    return runtimeAcceptsValueImpl(n, exact, fold, DefaultTypeMatchPolicy{});
}

qore_type_result_e QoreTypeSpec::runtimeAcceptsValue(const QoreValue& n, bool exact, bool fold,
        LexicalSemanticPolicy policy) const {
    return runtimeAcceptsValueImpl(n, exact, fold, LexicalTypeMatchPolicy{policy});
}

template <typename MatchPolicy>
qore_type_result_e QoreTypeSpec::runtimeAcceptsValueImpl(const QoreValue& n, bool exact, bool fold,
        MatchPolicy policy) const {
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
                const qore_class_private* target = qore_class_private::get(*uQc());
                if (obj->getInstantiatedTypeInfo() && target->hasTypeParams()
                        && !target->rawAcceptsParameterized()) {
                    return QTI_NOT_EQUAL;
                }
                qore_type_result_e rv = qore_class_private::runtimeCheckCompatibleClass(*uQc(), *obj->getClass());
                if (rv == QTI_NOT_EQUAL) {
                    return rv;
                }
                // issue #3272: do not return a match for deleted objects, except to select a variant (issue #6159)
                if constexpr (!MatchPolicy::deleted_object_keeps_class) {
                    if (!obj->isValid()) {
                        return QTI_NOT_EQUAL;
                    }
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
            if (!obj) {
                return QTI_NOT_EQUAL;
            }
            // issue #3272: a deleted object does not match, except to select a variant (issue #6159)
            if constexpr (!MatchPolicy::deleted_object_keeps_class) {
                if (!obj->isValid()) {
                    return QTI_NOT_EQUAL;
                }
            }
            if (obj->getInstantiatedTypeInfo() == uTi()) {
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
                    if (QoreTypeInfo::equal(mapped_type, uTi())
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
                // a hash keeps its hashdecl when it is freed
                hd = n.get<const WeakHashReferenceNode>()->borrow()->getHashDecl();
            }
            if (hd) {
                const typed_hash_decl_private* target = typed_hash_decl_private::get(*uHd());
                const typed_hash_decl_private* source = typed_hash_decl_private::get(*hd);
                if (source->equal(*target)) {
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
                bool type_may_not_match = false;
                bool type_may_need_filter = false;
                qore_type_result_e rv = qore_parameterized_hashdecl_accepts(uHd(), hd, type_may_not_match,
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
            // the hash of a weak reference is read with a reference of its own, as another thread can free it
            // meanwhile (qore#7375)
            qore_weak_target_ref<QoreHashNode> weak_target;
            if (ot == NT_HASH) {
                h = n.get<const QoreHashNode>();
                ti = h->getValueTypeInfo();
            } else if (ot == NT_WEAKREF_HASH) [[unlikely]] {
                // its type and its size, which reads as empty once it is freed (see WeakHashReferenceNode::lock())
                weak_target.target = n.get<const WeakHashReferenceNode>()->lock();
                h = weak_target.target;
                ti = h->getValueTypeInfo();
            }
            if (!h) {
                return QTI_NOT_EQUAL;
            }
            if (is_auto_vti(uTi())) {
                return QTI_NEAR;
            }
            // CRITICAL: A hasddecl-typed hash (with a specific structure) cannot match a complex hash type
            // with a SPECIFIC value type (a flexible map with string keys). They are mutually exclusive types.
            // This prevents incorrect variant selection when a hasddecl is passed where a complex hash
            // with specific value type is expected. However, hash<auto> should accept any hash including hasddecls.
            if (h->getHashDecl() && uTi() != autoTypeInfo) {
                return QTI_NOT_EQUAL;
            }
            if (ti && QoreTypeInfo::hasType(ti) && QoreTypeInfo::parseAccepts(uTi(), ti)) {
                return exact ? QTI_IDENT : QTI_AMBIGUOUS;
            }
            // acceptInputComplexHash() folds a value whose element type the fold path can convert, so a value this
            // returned no match for would still be accepted once it was bound; when asked for that verdict, report
            // it as an inexact match, so that variant selection and binding cannot disagree; the value does not have
            // this type, though, so it does not match otherwise (instanceof, and code that skips the conversion of a
            // value that has a slot's type)
            if (fold && ti && QoreTypeInfo::hasType(ti) && qore_container_value_may_convert_to(uTi(), ti)) {
                return QTI_AMBIGUOUS;
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
            // the list of a weak reference is read with a reference of its own, as another thread can free it
            // meanwhile (qore#7375)
            qore_weak_target_ref<QoreListNode> weak_target;
            if (ot == NT_LIST) {
                l = n.get<const QoreListNode>();
                ti = l->getValueTypeInfo();
            } else if (ot == NT_WEAKREF_LIST) [[unlikely]] {
                // its type and its size, which reads as empty once it is freed (see WeakListReferenceNode::lock())
                weak_target.target = n.get<const WeakListReferenceNode>()->lock();
                l = weak_target.target;
                ti = l->getValueTypeInfo();
            }
            if (l) {
                if (is_auto_vti(uTi())) {
                    return QTI_NEAR;
                }
                if (ti && QoreTypeInfo::hasType(ti) && QoreTypeInfo::parseAccepts(uTi(), ti)) {
                    return exact ? QTI_IDENT : QTI_AMBIGUOUS;
                }
                // as in the complex hash case above: acceptInputComplexList() folds a value whose element type
                // the fold path can convert, so reporting no match here would reject a value that binding
                // accepts
                if (fold && ti && QoreTypeInfo::hasType(ti) && qore_container_value_may_convert_to(uTi(), ti)) {
                    return QTI_AMBIGUOUS;
                }
                // issue #2647: allow an empty list with no specific type to be passed to any complex list type
                // it will get folded at runtime into the desired type in any case
                // NOTE: if the list has a specific type (ti with hasType), it must be compatible - checked above
                if (l->empty() && (!ti || !QoreTypeInfo::hasType(ti))) {
                    return QTI_NEAR;
                }
            }
            if (typespec == QTS_COMPLEXSOFTLIST) {
                qore_type_result_e rv = policy.value(uTi(), n, fold);
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
            if (!qore_complex_buffer_accepts_runtime(uTi(), *b, needs_nullable_copy)) {
                return QTI_NOT_EQUAL;
            }
            return needs_nullable_copy ? QTI_NEAR : (exact ? QTI_IDENT : QTI_AMBIGUOUS);
        }

        case QTS_COMPLEXREF:
            if (ot == NT_REFERENCE) {
                const QoreTypeInfo* ti = n.get<const ReferenceNode>()->getLValueTypeInfo();
                //printd(5, "QoreTypeSpec::runtimeAcceptsValue() cr ti: '%s' typeInfo: '%s' eq: %d ss: %d\n",
                //    QoreTypeInfo::getName(ti), QoreTypeInfo::getName(uTi()), QoreTypeInfo::equal(uTi(), ti),
                //    QoreTypeInfo::outputSuperSetOf(ti, uTi()));
                if (QoreTypeInfo::equal(uTi(), ti))
                    return QTI_IDENT;
                if (QoreTypeInfo::outputSuperSetOf(ti, uTi()))
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
                    // its type, which does not change when it is freed
                    h = qore_hash_private::get(*n.get<const WeakHashReferenceNode>()->borrow());
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
                    // its type, which does not change when it is freed
                    l = qore_list_private::get(*n.get<const WeakListReferenceNode>()->borrow());
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
                if (member->getEnumDecl() == uEd()
                    || qore_enum_decl_private::get(*member->getEnumDecl())->parseEqual(
                        *qore_enum_decl_private::get(*uEd()))) {
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
            return complexCodeAcceptsCallable(n) ? rv : QTI_NOT_EQUAL;
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeInfo::runtimeAcceptsValueOrFold(const QoreValue& n) const {
    for (auto& t : accept_vec) {
        qore_type_result_e rv = t.spec.runtimeAcceptsValue(n, t.exact, true);
        if (rv != QTI_NOT_EQUAL) {
            return complexCodeAcceptsCallable(n) ? rv : QTI_NOT_EQUAL;
        }
    }
    return QTI_NOT_EQUAL;
}

qore_type_result_e QoreTypeInfo::runtimeAcceptsValue(const QoreTypeInfo* ti, const QoreValue& n,
        LexicalSemanticPolicy policy, bool fold) {
    if (!hasType(ti)) {
        return QTI_WILDCARD;
    }
    for (auto& t : ti->accept_vec) {
        qore_type_result_e rv = t.spec.runtimeAcceptsValue(n, t.exact, fold, policy);
        if (rv != QTI_NOT_EQUAL) {
            return ti->complexCodeAcceptsCallable(n) ? rv : QTI_NOT_EQUAL;
        }
    }
    return ti->acceptsBrokenSoftNull(n, policy) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
}

// returns the list or hash that a call argument is or refers to, if any, and whether its element type is untyped,
// as the element type of a container built at runtime without a declared element type is; a hashdecl hash is a
// declared structure, not generic data, and is left to the hashdecl rules
namespace {
//! holds the list or hash of a weak reference with a reference of its own while a call argument is checked (qore#7198)
/** The release, which can run destructors, is out of the path of the other values, as is the ExceptionSink that it
    needs.
*/
struct QoreCallArgWeakTarget {
    AbstractQoreNode* target = nullptr;

    DLLLOCAL ~QoreCallArgWeakTarget() {
        if (target) [[unlikely]] {
            release(target);
        }
    }

    DLLLOCAL static QORE_INTERN_NOINLINE void release(AbstractQoreNode* target) {
        ExceptionSink xsink;
        target->deref(&xsink);
    }
};
}

// the list or hash of a weak reference is held by \a weak_target with a reference of its own, or is a new empty one if
// it was freed (qore#7198)
static QORE_INTERN_ALWAYS_INLINE void qore_get_call_arg_container(const QoreValue& n, const QoreListNode*& l, const QoreHashNode*& h,
        bool& untyped, QoreCallArgWeakTarget& weak_target) {
    l = nullptr;
    h = nullptr;
    untyped = false;
    switch (n.getType()) {
        case NT_LIST:
            l = n.get<const QoreListNode>();
            break;

        case NT_WEAKREF_LIST: {
            QoreListNode* target = n.get<const WeakListReferenceNode>()->lock();
            weak_target.target = target;
            l = target;
            break;
        }

        case NT_HASH:
            h = n.get<const QoreHashNode>();
            break;

        case NT_WEAKREF_HASH: {
            QoreHashNode* target = n.get<const WeakHashReferenceNode>()->lock();
            weak_target.target = target;
            h = target;
            break;
        }

        default:
            return;
    }
    if (h && h->getHashDecl()) {
        h = nullptr;
        return;
    }
    untyped = !QoreTypeInfo::hasType(l ? l->getValueTypeInfo() : h->getValueTypeInfo());
}

// checks every element of an untyped container against the element type of a typed container parameter type, as
// QoreTypeSpec::acceptInputComplexList()/acceptInputComplexHash() will when the argument is bound; returns false
// at the first element that is not accepted, otherwise lowers quality to the worst element match
template <typename MatchPolicy>
static bool qore_call_arg_elements_accepted(const QoreTypeInfo* elem_ti, const QoreListNode* l,
        const QoreHashNode* h, int& quality, bool& content_dependent, MatchPolicy policy) {
    // a hash element is converted to a hashdecl element type by QoreTypeInfo::retypeValue(), which checks its keys
    // as it builds the typed hash; as for a typed container whose elements may be converted, the match is decided by
    // the type here and the conversion reports a key that does not belong to the hashdecl
    bool elem_hashdecl = QoreTypeInfo::getTypedHash(elem_ti);
    // likewise, an object is materialized into a list<T> element type if it is an iterator, which binding finds out
    bool elem_list = QoreTypeInfo::getComplexListValueType(elem_ti);
    auto check = [elem_ti, elem_hashdecl, elem_list, &quality, &content_dependent, policy] (const QoreValue& v) -> bool {
        int elem_quality;
        if (QoreTypeInfo::runtimeAcceptsCallArgImpl(elem_ti, v, elem_quality, content_dependent, policy) == QTI_NOT_EQUAL) {
            if (!(elem_hashdecl && v.getType() == NT_HASH) && !(elem_list && v.getType() == NT_OBJECT)) {
                return false;
            }
            elem_quality = QTI_AMBIGUOUS;
        }
        if (elem_quality < quality) {
            quality = elem_quality;
        }
        return true;
    };
    if (l) {
        for (size_t i = 0, e = l->size(); i < e; ++i) {
            if (!check(l->retrieveEntry(i))) {
                return false;
            }
        }
        return true;
    }
    assert(h);
    ConstHashIterator i(h);
    while (i.next()) {
        if (!check(i.get())) {
            return false;
        }
    }
    return true;
}

// returns the match quality of a typed container argument for a container parameter type: the type match scores a
// container whose element type the parameter's element type accepts only by a conversion or as a subclass (ex:
// list<int> for list<float>) as one of the parameter's own element type, so the quality, which ranks variants with
// equal scores, ranks the latter first (issue #5541); other arguments have the quality of their match
static int qore_typed_container_quality(const QoreTypeInfo* ti, const QoreListNode* l, const QoreHashNode* h,
        bool untyped, qore_type_result_e rv) {
    if ((!l && !h) || untyped || rv <= QTI_AMBIGUOUS) {
        return rv;
    }
    const QoreTypeInfo* elem_ti = l ? QoreTypeInfo::getComplexListValueType(ti)
        : QoreTypeInfo::getComplexHashValueType(ti);
    if (!elem_ti) {
        return rv;
    }
    const QoreTypeInfo* vti = l ? l->getValueTypeInfo() : h->getValueTypeInfo();
    return QoreTypeInfo::equal(elem_ti, vti) ? rv : QTI_AMBIGUOUS;
}

qore_type_result_e QoreTypeInfo::runtimeAcceptsCallArg(const QoreTypeInfo* ti, const QoreValue& n, int& quality,
        bool& content_dependent) {
    return runtimeAcceptsCallArgImpl(ti, n, quality, content_dependent,
        CallArgTypeMatchPolicy<DefaultTypeMatchPolicy>{});
}

qore_type_result_e QoreTypeInfo::runtimeAcceptsCallArg(const QoreTypeInfo* ti, const QoreValue& n, int& quality,
        bool& content_dependent, LexicalSemanticPolicy policy) {
    return runtimeAcceptsCallArgImpl(ti, n, quality, content_dependent,
        CallArgTypeMatchPolicy<LexicalTypeMatchPolicy>{{policy}});
}

template <typename MatchPolicy>
qore_type_result_e QoreTypeInfo::runtimeAcceptsCallArgImpl(const QoreTypeInfo* ti, const QoreValue& n, int& quality,
        bool& content_dependent, MatchPolicy policy) {
    const QoreListNode* l;
    const QoreHashNode* h;
    bool untyped;
    QoreCallArgWeakTarget weak_target;
    qore_get_call_arg_container(n, l, h, untyped, weak_target);
    // empty untyped containers keep the rule of issue #2647, and empty typed containers have no elements to check
    if ((!l && !h) || (l ? l->empty() : h->empty()) || !hasType(ti)) {
        qore_type_result_e rv = policy.value(ti, n, true);
        quality = qore_typed_container_quality(ti, l, h, untyped, rv);
        return rv;
    }

    // the element type of each typed container in the type that matches the kind of the value
    auto get_elem_ti = [l] (const QoreAcceptSpec& t) -> const QoreTypeInfo* {
        const QoreTypeInfo* elem_ti = l ? t.spec.getComplexList() : t.spec.getComplexHash();
        return elem_ti && !is_auto_vti(elem_ti) ? elem_ti : nullptr;
    };

    // the element type of the typed list in the type, if any; a softlist<T> type also converts any list with a mapped
    // plain list type, whose conversion fails for the same elements
    const QoreTypeInfo* list_elem_ti = nullptr;
    if (l) {
        for (auto& t : ti->accept_vec) {
            if ((list_elem_ti = get_elem_ti(t))) {
                break;
            }
        }
    }
    auto is_softlist_catch_all = [l, list_elem_ti] (const QoreAcceptSpec& t) -> bool {
        return l && list_elem_ti && t.map && t.spec.isRuntimeBaseType(NT_LIST);
    };

    // as in runtimeAcceptsValue(), the first type in the type that accepts the value gives the result
    for (auto& t : ti->accept_vec) {
        const QoreTypeInfo* elem_ti = get_elem_ti(t);
        if (untyped) {
            // every element of an untyped container is checked against a typed container type
            if (elem_ti) {
                content_dependent = true;
                int elem_quality = QTI_IDENT;
                if (qore_call_arg_elements_accepted(elem_ti, l, h, elem_quality, content_dependent, policy)) {
                    quality = elem_quality;
                    return QTI_AMBIGUOUS;
                }
                continue;
            }
            if (is_softlist_catch_all(t)) {
                continue;
            }
            qore_type_result_e rv = policy.spec(t.spec, n, t.exact, true);
            if (rv != QTI_NOT_EQUAL) {
                quality = rv;
                return rv;
            }
            continue;
        }

        // a typed container: the type decides, except where the verdict depends on the elements, which are then
        // checked as binding will convert them; the result and its quality are those of the type, so a call that
        // binding accepts selects the variant it selected without the check
        qore_type_result_e rv = policy.spec(t.spec, n, t.exact, true);
        if (rv == QTI_NOT_EQUAL) {
            continue;
        }
        const QoreTypeInfo* check_elem_ti = nullptr;
        if (is_softlist_catch_all(t)) {
            check_elem_ti = list_elem_ti;
        } else if (elem_ti) {
            // a value whose element type is the element type needs no check; another one does if the match comes
            // from the fold rule, which accepts nested containers by their shape, or if its element type may not
            // match (ex: list<list<auto>> for list<list<int>>, which the type accepts as an exact match)
            const QoreTypeInfo* vti = l ? l->getValueTypeInfo() : h->getValueTypeInfo();
            // an element type that the element type still validates (date for date<absolute>) needs the check
            // (issue #5782)
            if (vti != elem_ti && !QoreTypeInfo::acceptsInputUnchecked(elem_ti, vti)) {
                bool may_not_match = false;
                if ((rv == QTI_AMBIGUOUS && policy.spec(t.spec, n, t.exact, false) == QTI_NOT_EQUAL)
                        || (QoreTypeInfo::parseAccepts(elem_ti, vti, may_not_match) && may_not_match)) {
                    check_elem_ti = elem_ti;
                }
            }
        }
        if (check_elem_ti) {
            content_dependent = true;
            int elem_quality = QTI_IDENT;
            if (!qore_call_arg_elements_accepted(check_elem_ti, l, h, elem_quality, content_dependent, policy)) {
                continue;
            }
        }
        quality = qore_typed_container_quality(ti, l, h, untyped, rv);
        return rv;
    }
    quality = QTI_NOT_EQUAL;
    return QTI_NOT_EQUAL;
}

bool QoreTypeInfo::acceptsBrokenSoftNull(const QoreValue& n, LexicalSemanticPolicy policy) const {
    return policy.brokenSoftTypes() && n.getType() == NT_NULL && brokenSoftNullImpl();
}

void QoreTypeInfo::concatNameConverted(QoreString& str) const {
    qore_string_private* p = qore_string_private::get(str);
    const qore_string_private* np = qore_string_private::get(tname);
    if (!np->len) {
        return;
    }
    // a name in ASCII is the same bytes in an encoding in which every ASCII character is itself
    bool ascii_identical = qore_encoding_private::get(*p->getEncoding())->isAsciiIdentical();
    if (ascii_identical && np->isDataAscii()) {
        p->concat_intern(np->effective_buf(), np->len);
        return;
    }
    ExceptionSink xsink;
    if (!p->concat(&tname, &xsink)) {
        return;
    }
    xsink.clear();
    if (!ascii_identical) {
        return;
    }
    // the name has a character that the encoding of the string does not have: its ASCII characters are appended,
    // with '?' in place of each other character
    bool utf8 = np->getEncoding() == QCS_UTF8;
    const char* b = np->effective_buf();
    for (size_t i = 0; i < np->len; ++i) {
        unsigned char c = (unsigned char)b[i];
        if (c < 0x80) {
            p->concat((char)c);
        } else if (!utf8 || (c & 0xc0) != 0x80) {
            // a UTF-8 continuation byte is part of the character whose lead byte was replaced
            p->concat('?');
        }
    }
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
    // ti is not itself a reference type, but its specs match any base type, including a reference (e.g. a
    // symbolic type parameter).  There is no hard-reference form to substitute for such a type, so return it
    // unchanged rather than dereferencing a null cast.
    if (!type) {
        return ti;
    }
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

    // a bound that names its own generic type (class Self<T: Self<int>>) is checked again while it is resolved here;
    // the check in progress decides it with the bound resolved, so a nested check of the same bound is not made,
    // which would never end (qore#5774)
    thread_local std::vector<const char*> bounds_in_check;
    if (std::find(bounds_in_check.begin(), bounds_in_check.end(), bound_type) != bounds_in_check.end()) {
        return 0;
    }
    bounds_in_check.push_back(bound_type);
    ON_BLOCK_EXIT([] () { bounds_in_check.pop_back(); });

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

    // as in qore_check_parse_type_arg_bound(): a bound that names its own generic type is decided by the check in
    // progress, not checked again while it is resolved (qore#5774)
    thread_local std::vector<const char*> bounds_in_check;
    if (std::find(bounds_in_check.begin(), bounds_in_check.end(), bound_type) != bounds_in_check.end()) {
        return true;
    }
    bounds_in_check.push_back(bound_type);
    ON_BLOCK_EXIT([] () { bounds_in_check.pop_back(); });

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
        // the default resolved where the class is declared, once it is initialized (qore#6285)
        if (const QoreTypeInfo* declared = qore_class_private::get(*qc)->getTypeParamDefaultTypeInfo(i)) {
            args.push_back(declared);
            continue;
        }
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
        // the default resolved where the class is declared (qore#6285)
        const QoreTypeInfo* arg = qore_class_private::get(*qc)->getTypeParamDefaultTypeInfo(i);
        if (!arg) {
            arg = qore_resolve_runtime_default_type_arg(qc->getTypeParameterDefaultType(i));
        }
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
    "int8, int16, int32, int64, float32, float64, bool, string, decimal128";

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

        // split the signature into the return type and the parameter types (see qore_split_code_signature())
        std::string return_type_str;
        std::vector<std::string> param_strs;
        bool has_varargs;
        {
            std::string error;
            if (!qore_split_code_signature(sig_str, return_type_str, param_strs, has_varargs, error)) {
                return nullptr;
            }
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
            // reject parameter names and any other trailing text after the type; the parse-time path
            // (resolveSubtype()) raises a PARSE-TYPE-ERROR for the same input
            std::string trailing;
            if (code_sig_type_trailing_text(return_type_str, trailing)) {
                return nullptr;
            }
            returnType = qore_get_type_from_string_intern(return_type_str.c_str());
            if (!returnType) {
                return nullptr;
            }
        }
        // If empty or "nothing", returnType stays nullptr (means nothing return)

        // Resolve the parameter types
        type_vec_t param_types;
        for (const std::string& current_param : param_strs) {
            // reject parameter names and any other trailing text after the type; the parse-time path
            // (resolveSubtype()) raises a PARSE-TYPE-ERROR for the same input
            std::string trailing;
            if (code_sig_type_trailing_text(current_param, trailing)) {
                return nullptr;
            }
            const QoreTypeInfo* param_type = qore_get_type_from_string_intern(current_param.c_str());
            if (!param_type) {
                return nullptr;
            }
            param_types.push_back(param_type);
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
            if (qore_aot_should_defer_source_symbol(loc, subtypes[0]->cscope->ostr,
                    QoreAOTSourceSymbolKind::HashDecl)) {
                return qore_get_aot_deferred_type_info(loc, subtypes[0]->cscope->ostr, or_nothing, true);
            }
            const TypedHashDecl* hd = qore_resolve_parse_hashdecl_type(*subtypes[0], loc, err);
            if (hd) {
                return hd->getTypeInfo(or_nothing);
            }
            if (qore_aot_source_parse_active()) {
                return qore_get_aot_deferred_type_info(loc, subtypes[0]->cscope->ostr, or_nothing, true);
            }
            // Unknown hashdecl - raise parse error with a near-match suggestion if available
            {
                QoreSuggestionList sl(subtypes[0]->cscope->ostr);
                qore_root_ns_private::addHashDeclSuggestions(sl);
                std::string shint = sl.getHint();
                if (!shint.empty()) {
                    parseException(*loc, "PARSE-EXCEPTION", "illegal access to unknown member in undefined "
                        "hashdecl '%s'; %s", subtypes[0]->cscope->ostr, shint.c_str());
                } else {
                    parseException(*loc, "PARSE-EXCEPTION", "illegal access to unknown member in undefined "
                        "hashdecl '%s'", subtypes[0]->cscope->ostr);
                }
            }
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

        // split the signature into the return type and the parameter types (see qore_split_code_signature(), as
        // resolveRuntimeSubtype() does): the return type can be a callable type with a parameter list of its own
        // (ex: code<*hash<auto>()> in code<code<*hash<auto>()>()>), and a nested callable's parameters and "..." stay
        // with it (#6022)
        std::string return_type_str;
        std::vector<std::string> param_strs;
        bool has_varargs;
        {
            std::string error;
            if (!qore_split_code_signature(sig_str, return_type_str, param_strs, has_varargs, error)) {
                parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve '%s'; invalid callable signature '%s'; %s",
                    getName(), sig_str, error.c_str());
                err = -1;
                return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
            }
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

        // Resolve the parameter types
        type_vec_t param_types;
        for (const std::string& current_param : param_strs) {
            // Use the helper to properly parse and resolve complex types
            const QoreTypeInfo* param_type = parse_and_resolve_type_string(current_param.c_str(), loc, err);
            if (err) {
                return or_nothing ? codeOrNothingTypeInfo : codeTypeInfo;
            }
            param_types.push_back(param_type);
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

    if (qore_aot_should_defer_source_symbol(loc, cscope.ostr, QoreAOTSourceSymbolKind::Class)) {
        return qore_get_aot_deferred_type_info(loc, cscope.ostr, or_nothing, false);
    }
    if (qore_aot_should_defer_source_symbol(loc, cscope.ostr, QoreAOTSourceSymbolKind::HashDecl)) {
        return qore_get_aot_deferred_type_info(loc, cscope.ostr, or_nothing, true);
    }

    // check for typedef first (both simple names and scoped names)
    TypedefEntry* td = qore_root_ns_private::parseFindTypedef(cscope);
    if (td) {
        // A typedef is expanded where it is used, so the code that uses it is compiled against the type it names:
        // unlike a class, which a type annotation names and the program resolves when it is loaded, a typedef cannot
        // be deferred, and a standalone compile of the consumer must preload the source that declares it.  It is
        // therefore a dependency of the consumer, as a folded constant is (narrowed to the provider's declaration
        // contract, which names the type of each typedef).
        // (A typedef imported from a module is covered by the dependency on the module.)
        if (!td->mod_imported) {
            qore_aot_note_referenced_decl(td->loc, loc);
        }
        // the typedef's underlying type, resolved in the namespace that declares the typedef
        const QoreTypeInfo* rv = td->parseResolve(cscope.ostr, err);

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
    const TypedHashDecl* hd = qore_root_ns_private::get(*getRootNS())->parseTryFindHashDecl(cscope);
    if (hd) {
        if (typed_hash_decl_private::get(*hd)->hasTypeParams()) {
            parseException(*loc, "PARSE-TYPE-ERROR", "cannot resolve generic hashdecl '%s' without explicit type "
                "arguments", cscope.ostr);
            err = -1;
            return autoTypeInfo;
        }
        return hd->getTypeInfo(or_nothing);
    }

    if (qore_aot_source_parse_active()) {
        return qore_get_aot_deferred_type_info(loc, cscope.ostr, or_nothing, false);
    }

    // type not found - raise error with a near-match suggestion from known type names
    {
        QoreSuggestionList sl(cscope.ostr);
        qore_root_ns_private::addClassSuggestions(sl);
        qore_root_ns_private::addHashDeclSuggestions(sl);
        std::string hint = sl.getHint();
        if (!hint.empty()) {
            parse_error(*loc, "reference to undefined type '%s'; %s", cscope.ostr, hint.c_str());
        } else {
            parse_error(*loc, "reference to undefined type '%s'", cscope.ostr);
        }
    }
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
    concatName(str);
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
    concatName(getFirstAcceptSpec().spec.getComplexBuffer(), str);
    str.concat(" or no value (NOTHING)");
}

// the conversions read the element type from the cell, which is orphaned with the specs (issue #6419)
QoreComplexSoftListTypeInfo::QoreComplexSoftListTypeInfo(qore_orphanable_type_cell_t cell)
        : QoreComplexListTypeInfo(q_accept_vec_t {
            {
                QoreComplexSoftListTypeSpec(cell->get()),
                [cell] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreTypeInfo* vti = cell->get();
                    if (n.getType() != NT_LIST || n.get<const QoreListNode>()->getValueTypeInfo() != vti) {
                        QoreValue val{};
                        n.swap(val);
                        n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val,
                            xsink));
                    }
                },
                true,
                [cell] (QoreValue& n, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
                    const QoreTypeInfo* vti = cell->get();
                    if (n.getType() != NT_LIST || n.get<const QoreListNode>()->getValueTypeInfo() != vti) {
                        QoreValue val{};
                        n.swap(val);
                        n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val,
                            xsink, policy));
                    }
                }
            },
            {
                NT_LIST,
                [cell] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreTypeInfo* vti = cell->get();
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink));
                }, false,
                [cell] (QoreValue& n, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
                    const QoreTypeInfo* vti = cell->get();
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink,
                        policy));
                }
            },
            {
                NT_NOTHING,
                [cell] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreTypeInfo* vti = cell->get();
                    QoreListNode* l = new QoreListNode(vti);
                    n.assign(l);
                }
            },
        }, q_return_vec_t {
            { QoreComplexSoftListTypeSpec(cell->get()), true }
        }, QoreStringMaker("softlist<%s>", QoreTypeInfo::getName(cell->get()))), vti_cell(cell) {
    const QoreTypeInfo* vti = cell->get();
    assert(vti);
    pname = QoreStringMaker("softlist<%s>", QoreTypeInfo::getPath(vti));
}

QoreComplexSoftListOrNothingTypeInfo::QoreComplexSoftListOrNothingTypeInfo(qore_orphanable_type_cell_t cell)
    : QoreComplexListOrNothingTypeInfo(q_accept_vec_t {
            {
                QoreComplexSoftListTypeSpec(cell->get()),
                [cell] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreTypeInfo* vti = cell->get();
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
                true,
                [cell] (QoreValue& n, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
                    const QoreTypeInfo* vti = cell->get();
                    if (!n.isNothing()
                            && (n.getType() != NT_LIST || n.get<const QoreListNode>()->getValueTypeInfo() != vti)) {
                        QoreValue val{};
                        n.swap(val);
                        n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val,
                            xsink, policy));
                    }
                }
            },
            {
                NT_LIST,
                [cell] (QoreValue& n, ExceptionSink* xsink) {
                    const QoreTypeInfo* vti = cell->get();
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink));
                }, false,
                [cell] (QoreValue& n, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
                    const QoreTypeInfo* vti = cell->get();
                    QoreValue val{};
                    n.swap(val);
                    n.assign(qore_list_private::newComplexListFromValue(qore_get_complex_list_type(vti), val, xsink,
                        policy));
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
        }, q_return_vec_t {{QoreComplexSoftListTypeSpec(cell->get())}, {NT_NOTHING}
        }, QoreStringMaker("*softlist<%s>", QoreTypeInfo::getName(cell->get()))), vti_cell(cell) {
    const QoreTypeInfo* vti = cell->get();
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

// Normalize a container stored under a no-narrow lvalue type: hash<auto!> and
// list<auto!> accept narrowed containers but must store them as plain auto
// containers so later heterogeneous key/element writes remain valid and the
// value's own hashdecl/complex type stops being enforced.  Retags in place
// when the container is unique, copies when it is shared.  Same semantics as
// the strip in LValueHelper::assign(), which keeps its own inline version
// because it runs under the lvalue lock and must defer derefs via saveTemp().
void q_apply_no_narrow_hash_type(QoreValue& val, ExceptionSink* xsink) {
    if (val.getType() != NT_HASH) {
        return;
    }
    QoreHashNode* h = val.get<QoreHashNode>();
    qore_hash_private* hp = qore_hash_private::get(*h);
    if (!hp->getHashDecl() && hp->complexTypeInfo == autoHashTypeInfo) {
        return;
    }
    if (!h->is_unique()) {
        QoreHashNode* copy = h->copy();
        qore_hash_private* cp = qore_hash_private::get(*copy);
        if (cp->getHashDecl()) {
            cp->setHashDecl(nullptr);
        }
        cp->complexTypeInfo = autoHashTypeInfo;
        AbstractQoreNode* old = val.assign(copy);
        discard(old, xsink);
    } else {
        if (hp->getHashDecl()) {
            hp->setHashDecl(nullptr);
        }
        hp->complexTypeInfo = autoHashTypeInfo;
    }
}

void q_apply_no_narrow_list_type(QoreValue& val, ExceptionSink* xsink) {
    if (val.getType() != NT_LIST) {
        return;
    }
    QoreListNode* l = val.get<QoreListNode>();
    qore_list_private* lp = qore_list_private::get(*l);
    if (lp->complexTypeInfo == autoListTypeInfo) {
        return;
    }
    if (!l->is_unique()) {
        QoreListNode* copy = l->copy();
        qore_list_private::get(*copy)->complexTypeInfo = autoListTypeInfo;
        AbstractQoreNode* old = val.assign(copy);
        discard(old, xsink);
    } else {
        lp->complexTypeInfo = autoListTypeInfo;
    }
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
        const QoreValue& n, ExceptionSink* xsink, q_lvalue_vts_e vts) const {
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
            doTypeException(arg_type, param_num + 1, param_name, n, xsink, vts);
        }
    }
    return -1;
}

int QoreTypeInfo::doTypeException(const char* arg_type, int param_num, const char* param_name, const QoreValue& n,
        ExceptionSink* xsink, q_lvalue_vts_e vts) const {
    // xsink may be null in case parse exceptions have been disabled in the QoreProgram object
    // for example if there was a "requires" error
    if (!xsink)
        return -1;

    SimpleRefHolder<QoreStringNode> desc(new QoreStringNode);
    QoreTypeInfo::ptext(**desc, arg_type, param_num, param_name);
    desc->sprintf("expects type '%s', but got ", tname.c_str());
    QoreTypeInfo::getNodeType(**desc, n);
    desc->concat(" instead");
    // the structured diagnostic of the error, for tools and coding assistants; see QoreRuntimeDiagnostic.h
    const char* container = nullptr;
    // At runtime we can prove that the rejected target type came from a container, but not whether
    // that container was declared with an inferred or explicit element type.  Keep this message
    // accurate for both cases; the parse-time diagnostic provides auto! guidance when it has the
    // variable declaration and narrowing location needed to prove that recommendation applies.
    if (vts != QLVTS_None && isSimpleTypeNarrowed()) {
        switch (vts) {
            case QLVTS_Hash:
                container = "hash";
                break;
            case QLVTS_List:
                container = "list";
                break;
            case QLVTS_None:
                break;
        }
        if (container) {
            desc->sprintf("; this type is the declared value type of the %s holding the target; "
                "review the container declaration and the value being assigned", container);
        }
    }
    ReferenceHolder<QoreHashNode> diag(makeRuntimeDiagnostic(n, container), nullptr);
    xsink->raiseExceptionArg("RUNTIME-TYPE-ERROR", diag.release(), desc.release());
    return -1;
}

QoreHashNode* QoreTypeInfo::makeRuntimeDiagnostic(const QoreValue& n, const char* container) const {
    std::vector<std::pair<std::string, std::string>> facts = {{"expectedType", tname.c_str()}};
    if (n.isNothing()) {
        return qore_make_runtime_diagnostic(QORE_RTD_NO_VALUE_FOR_TYPE,
            "a value of this type is required, but there is no value (NOTHING), as for a variable that was declared "
            "but not assigned, a missing hash key, or the result of map over NOTHING", "pitfalls_declaration", facts,
            {"assign the variable before it is read, as in the declaration (int i = 0;)",
             "coalesce a value that can be missing, as in (h.count ?? 0) or ((map ..., l) ?? ())",
             "declare an optional type, such as *int, where no value is expected"});
    } else {
        QoreString actual;
        facts.push_back({"actualType", n.getFullTypeName(false, actual)});
        if (container) {
            facts.push_back({"container", container});
            return qore_make_runtime_diagnostic(QORE_RTD_CONTAINER_VALUE_TYPE,
                "the value type of the container holding the target does not accept this value; if the container "
                "was created from values of one type, as with a literal, its value type was narrowed to that type",
                "pitfalls_narrowing", facts,
                {"create a container that holds values of different types as hash<auto!> or list<auto!>",
                 "build a merged or widened result in a new hash<auto!> instead of writing it into the given one"});
        } else {
            return qore_make_runtime_diagnostic(QORE_RTD_INCOMPATIBLE_RUNTIME_TYPE,
                "the value does not have the type that the target requires", nullptr, facts,
                {"convert the value to the required type, or change the declared type of the target"});
        }
    }
}

int QoreTypeInfo::doObjectHashDeclTypeException(const char* arg_type, const char* param_name, const QoreValue& n,
        ExceptionSink* xsink) const {
    assert(xsink);
    SimpleRefHolder<QoreStringNode> desc(new QoreStringNode);
    desc->sprintf("%s '%s' expects type '%s', but got ", arg_type, param_name, tname.c_str());
    QoreTypeInfo::getNodeType(**desc, n);
    desc->concat(" instead");
    ReferenceHolder<QoreHashNode> diag(makeRuntimeDiagnostic(n, nullptr), nullptr);
    xsink->raiseExceptionArg("RUNTIME-TYPE-ERROR", diag.release(), desc.release());
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

// Verifies that one container element satisfies the element type that retypeValue() is about to
// stamp on the container, converting it first where the element type allows it.
//
// QoreTypeInfo::retypeValue() only reshapes container metadata; for a leaf value (a scalar element
// type, an object, code, etc.) it has nothing to reshape and returns success without looking at the
// value at all.  Its callers then stamp the container with the target element type, so an element
// that does not satisfy that type would be silently mislabeled -- producing e.g. a
// hash<string, string> that holds a bool.  Such a value reads back unharmed under AST execution but
// makes the typed read emitted by the IR/JIT/AOT tiers throw RUNTIME-TYPE-ERROR far from the
// assignment that created it, and it defeats the fold check in acceptInputComplexHash(), which
// short-circuits as soon as the value type matches.
//
// The element type's own acceptance machinery is used here so that legal conversions (soft types,
// int -> float, ...) still take place; an element that cannot be accepted fails the retype, and the
// caller reports the type error.  An element that is accepted only through such a conversion is converted
// too: cast<list<list<softint>>>() of a list<list<string>> gave a list<list<softint>> holding strings.
template <typename FilterPolicy>
static bool qore_retype_element_accepted(QoreValue& v, const QoreTypeInfo* elem_ti, const char* key,
        ExceptionSink* xsink, FilterPolicy policy) {
    // auto and auto! element types accept any value without narrowing it
    if (!elem_ti || elem_ti == autoTypeInfo || elem_ti == autoNoNarrowTypeInfo) {
        return true;
    }
    switch (v.getType()) {
        // Not values yet, or not values at all: judging these here would reject a container that
        // is in fact well typed.
        //
        // A deferred constant reference stands in for a constant that has not been resolved -- the
        // AOT deserializer restores a sibling class constant declared later in the class as one and
        // resolves it afterwards, and the resolved value has its own type applied then.
        // qore_runtime_accepts_call_arg() in Function.cpp makes the same allowance for call
        // arguments.  Weak references are unwrapped by dedicated branches in QoreTypeSpec::acceptInput(); the runtime
        // match below does not see through them.
        case NT_RTCONSTREF:
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            return true;

        default:
            break;
    }
    // an element that is accepted only through a conversion (a soft type given a string, a float given an int, ...)
    // is converted as an assignment converts it; only an element that already has the type is left as it is, or
    // the container would be stamped with an element type that its element does not have; an enum value is
    // converted to its base value by an element type of its base type, although that type is a superset of the
    // enum's type (qore#6274)
    qore_type_result_e rv = policy.value(elem_ti, v, false);
    // an lvalue reference whose declared type cannot match the element type must not label the container, or the typed
    // reads of the compiled modes read it as a value of that type: it goes to the error below, which takes no lock.  A
    // reference that can match, or that a soft list type would wrap, is kept as it is and not bound or wrapped here:
    // that can read the referenced variable, and a retype can run while the assignment holds the lock of that variable
    // (qore#2889, qore#7377; the cases kept this way are qore#7380)
    if (v.getType() == NT_REFERENCE) {
        if (rv != QTI_NOT_EQUAL) {
            return true;
        }
    } else if (!v.isEnum()
            && (rv == QTI_IDENT || (rv != QTI_NOT_EQUAL && QoreTypeInfo::superSetOf(elem_ti, v.getTypeInfo())))) {
        return true;
    }
    if (key) {
        policy.input(*elem_ti, xsink, "key", false, -1, key, v, nullptr);
    } else {
        policy.assignment(elem_ti, "<list element>", v, xsink);
    }
    if (xsink && *xsink) {
        return false;
    }
    // with parse exceptions disabled there is no xsink to report through; the caller turns a false
    // return into a type error of its own
    return policy.value(elem_ti, v, false) != QTI_NOT_EQUAL;
}

bool QoreTypeInfo::retypeValue(QoreValue& v, const QoreTypeInfo* target_ti,
        ExceptionSink* xsink) {
    return retypeValueImpl(v, target_ti, xsink, DefaultTypeFilterPolicy{});
}

bool QoreTypeInfo::retypeValue(QoreValue& v, const QoreTypeInfo* target_ti,
        ExceptionSink* xsink, LexicalSemanticPolicy policy) {
    return retypeValueImpl(v, target_ti, xsink, LexicalTypeFilterPolicy{policy});
}

template <typename FilterPolicy>
bool QoreTypeInfo::retypeValueImpl(QoreValue& v, const QoreTypeInfo* target_ti,
        ExceptionSink* xsink, FilterPolicy policy) {
    // retyping only mutates a value when the declared type can actually reshape the runtime value.  A soft
    // complex-auto container (hash<auto>/list<auto> and their "*..." or-nothing variants) accepts any element
    // type and preserves the runtime value type — it never mutates — so treat it like plain "auto" and return
    // early.  This is what distinguishes soft hash<auto> (which preserves e.g. a runtime hash<string, string>)
    // from hard hash<auto!> (a distinct hash<string, auto> type that widens the value type to auto).  Without
    // this, a "const hash<auto>" initializer would be widened to hash<auto> under AST execution while IR/JIT
    // (which re-evaluate the initializer) correctly preserved the narrow runtime type.
    if (!v.hasNode() || !target_ti || target_ti == autoTypeInfo
            || target_ti == autoHashTypeInfo || target_ti == autoHashOrNothingTypeInfo
            || target_ti == autoListTypeInfo || target_ti == autoListOrNothingTypeInfo) {
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
        // the element type can be a hashdecl of another Program (a member type of a hashdecl from a deleted Program)
        QoreHashNode* coerced = policy.hash(hd, src, xsink);
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
    if (inner_h_vt) {
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
        if (inner_h_vt != autoTypeInfo) {
            HashIterator hi(h);
            size_t i = 0;
            while (hi.next()) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "hash value retyping")) {
                    return false;
                }
                hash_assignment_priv ha(*qore_hash_private::get(*h), *qhi_priv::get(hi)->i);
                QoreValue cur(ha.swap(QoreValue()));
                if (!policy.retype(cur, inner_h_vt, xsink)
                        || !qore_retype_element_accepted(cur, inner_h_vt, hi.getKey(), xsink, policy)) {
                    ha.swap(cur);
                    return false;
                }
                ha.swap(cur);
                ++i;
            }
        }
        qore_hash_private::get(*h)->complexTypeInfo
            = qore_get_complex_hash_type(inner_h_vt);
        return true;
    }

    const QoreTypeInfo* inner_l_vt = QoreTypeInfo::getComplexListValueType(target_ti);
    if (inner_l_vt) {
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
        if (inner_l_vt != autoTypeInfo) {
            size_t n = l->size();
            for (size_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "list value retyping")) {
                    return false;
                }
                QoreValue cur = lp->swap(i, QoreValue());
                if (!policy.retype(cur, inner_l_vt, xsink)
                        || !qore_retype_element_accepted(cur, inner_l_vt, nullptr, xsink, policy)) {
                    lp->swap(i, cur);
                    return false;
                }
                lp->swap(i, cur);
            }
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
    // a value of a type parameter (in generic code) has its bound, or any type if it has none (a may-match checked at
    // run time), unless the target is a type parameter, which matches it by identity (qore#5774)
    if (second && first && !first->return_vec.empty() && !first->return_vec[0].spec.getTypeParameterTypeInfo()) {
        if (const QoreTypeInfo* src = getTypeParameterSourceType(second)) {
            second = src;
        }
    }
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
    bool aot_deferred_result = false;
    if (qore_type_info_aot_deferred_compare(first, second, aot_deferred_result) && aot_deferred_result) {
        may_not_match = false;
        may_need_filter = false;
        max_result = QTI_IDENT;
        return QTI_IDENT;
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
    // date<absolute> and date<relative> have no value in common: both accept and return dates, so only their
    // subtypes tell them apart, and a value of one is never accepted by the other (issue #5722)
    if (isDateSubtypeMismatch(first, second)) {
        max_result = QTI_NOT_EQUAL;
        return QTI_NOT_EQUAL;
    }
    // a typed callable type accepts the typed callable types whose signatures are compatible with its own; the specs
    // do not hold the signature (qore#6660)
    if (__builtin_expect(first->complex_code && second->complex_code, 0)
            && !checkComplexCodeCompatibility(first, second)) {
        max_result = QTI_NOT_EQUAL;
        return QTI_NOT_EQUAL;
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
    bool aot_deferred_result = false;
    if (qore_type_info_aot_deferred_compare(this, typeInfo, aot_deferred_result) && aot_deferred_result) {
        return QTI_IDENT;
    }

    if (typeInfo->return_vec.size() != return_vec.size() || typeInfo->getAcceptVecSize() != getAcceptVecSize()) {
        return QTI_NOT_EQUAL;
    }

    // two typed callable types match if the signature of the second is compatible with the first's; the specs do not
    // hold the signature (qore#6660)
    if (__builtin_expect(complex_code && typeInfo->complex_code, 0)) {
        if (complexCodeTypesEqual(this, typeInfo)) {
            return QTI_IDENT;
        }
        return checkComplexCodeCompatibility(this, typeInfo) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
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
    if (QoreTypeInfo::isType(ntype, NT_NOTHING)) {
        if (!QoreTypeInfo::parseAcceptsReturns(ctype, NT_NOTHING)) {
            ctype = get_or_nothing_type(ctype);
        }
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
    // The existing common type can already contain the new type, for example
    // *hash<Info> followed by hash<Info>. Preserve it before rejecting types
    // with multiple return alternatives; folding must work in either order.
    if (ctype->superSetOf(ntype)) {
        return true;
    }

    // two typed callables of different signatures have the type code in common (qore#6660)
    if (__builtin_expect(ctype->isComplexCode() && ntype->isComplexCode(), 0)) {
        ctype = static_cast<const QoreComplexCodeTypeInfo*>(ctype)->isOrNothing()
                || static_cast<const QoreComplexCodeTypeInfo*>(ntype)->isOrNothing()
            ? codeOrNothingTypeInfo : codeTypeInfo;
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
        // an enum value is not a value of its base type or of another enum with the same base type: an enum type and
        // a different type have no common type, so that a container whose value type is taken from its values keeps
        // its enum values, and a container of a declared type converts them when they are stored in it (qore#6274)
        if (ctype->return_vec[0].spec.getEnum() != ntype->return_vec[0].spec.getEnum()) {
            ctype = autoTypeInfo;
            return false;
        }
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

//! returns true if the container spec \a target keeps the values of the container spec \a source as they are: a
//! list or hash of typed callables for a list or hash of code (qore#6660)
/** Before the signature of a typed callable was part of its type, list<code> and list<code<...>> were the same type,
    and a container of closures kept the type of its closures where a container of code was expected; called only
    for specs that are not equal
*/
static __attribute__((noinline)) bool qore_spec_keeps_typed_callables(const QoreTypeSpec& target,
        const QoreTypeSpec& source) {
    q_typespec_t ts = target.getTypeSpec();
    if (ts != source.getTypeSpec() || (ts != QTS_COMPLEXLIST && ts != QTS_COMPLEXHASH && ts != QTS_COMPLEXSOFTLIST)) {
        return false;
    }
    const QoreTypeInfo* tv = target.getElementType();
    if (tv != codeTypeInfo && tv != codeOrNothingTypeInfo) {
        return false;
    }
    const QoreTypeInfo* sv = source.getElementType();
    if (!sv || !sv->isComplexCode()) {
        return false;
    }
    return tv == codeOrNothingTypeInfo || !static_cast<const QoreComplexCodeTypeInfo*>(sv)->isOrNothing();
}

bool QoreTypeInfo::superSetOf(const QoreTypeInfo* t) const {
    if (this == t) {
        return true;
    }
    if (getAcceptVecSize() < t->getAcceptVecSize() || return_vec.size() < t->return_vec.size())
        return false;

    // the specs of a typed callable type do not hold its signature (qore#6660); plain code is a superset of every
    // typed callable type, as its specs give
    if (__builtin_expect(complex_code, 0)) {
        return qore_complex_code_super_set_of(this, t);
    }

    for (unsigned i = 0; i < t->getAcceptVecSize(); ++i) {
        if (t->getAcceptSpec(i).spec != getAcceptSpec(i).spec
                && !qore_spec_keeps_typed_callables(getAcceptSpec(i).spec, t->getAcceptSpec(i).spec))
            return false;
    }

    for (unsigned i = 0; i < t->return_vec.size(); ++i) {
        if (t->return_vec[i].spec != return_vec[i].spec
                && !qore_spec_keeps_typed_callables(return_vec[i].spec, t->return_vec[i].spec))
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

bool QoreTypeInfo::outputAccepts(const QoreTypeInfo* t) const {
    for (auto& ti : t->return_vec) {
        bool found = false;
        for (auto& i : return_vec) {
            bool may_not_match = false;
            bool may_need_filter = false;
            if (i.spec.match(ti.spec, may_not_match, may_need_filter) != QTI_NOT_EQUAL && !may_not_match
                && !may_need_filter) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

void QoreTypeInfo::acceptInputIntern(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
        const char* param_name, QoreValue& n, LValueHelper* lvhelper, q_lvalue_vts_e vts) const {
    acceptInputInternImpl(xsink, arg_type, obj, param_num, param_name, n, lvhelper, vts, DefaultTypeFilterPolicy{});
}

void QoreTypeInfo::acceptInputIntern(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
        const char* param_name, QoreValue& n, LexicalSemanticPolicy policy, LValueHelper* lvhelper,
        q_lvalue_vts_e vts) const {
    acceptInputInternImpl(xsink, arg_type, obj, param_num, param_name, n, lvhelper, vts,
        LexicalTypeFilterPolicy{policy});
}

void QoreTypeInfo::acceptInputParam(const QoreTypeInfo* ti, int param_num, const char* param_name, QoreValue& n,
        ExceptionSink* xsink, LexicalSemanticPolicy policy) {
    if (hasType(ti)) {
        if (ti->acceptsRuntimeValueWithoutFilter(n)) {
            return;
        }
        ti->acceptInputIntern(xsink, "parameter", false, param_num, param_name, n, policy);
    } else if (ti != autoTypeInfo) {
        stripTypeInfo(n, xsink);
    }
}

#ifdef DEBUG
//! Asserts that a runtime type check of a derived type that names a user declaration is made in a thread that
//! publishes an epoch of type retirement (issue #6419)
/** Otherwise the declaration can be freed while the check reads it: code that checks values in a thread outside Qore
    code (a module's own thread) must enter a Program first.  A declaration's own type is not checked: it is read from
    an owner that keeps the declaration.  Not checked at library cleanup, when nothing is freed anymore.
*/
static void qore_type_check_epoch_assert(const QoreTypeInfo* ti) {
    if (type_owner_cleanup.load(std::memory_order_relaxed) || !qore_thread_has_no_type_epoch()
            || dynamic_cast<const QoreClassTypeInfo*>(ti) || dynamic_cast<const QoreHashDeclTypeInfo*>(ti)
            || dynamic_cast<const QoreEnumTypeInfo*>(ti)) {
        return;
    }
    for (const QoreAcceptSpec& as : ti->getAcceptSpecs()) {
        const QoreTypeSpec& spec = as.spec;
        bool user = qore_user_class_owner(spec.getClass()) || qore_user_hashdecl_owner(spec.getHashDecl())
            || qore_user_enum_owner(spec.getEnum());
        if (!user) {
            switch (spec.getTypeSpec()) {
                case QTS_COMPLEXHASH:
                case QTS_COMPLEXLIST:
                case QTS_COMPLEXSOFTLIST:
                case QTS_COMPLEXHARDREF:
                case QTS_COMPLEXREF:
                    user = qore_user_decl_type_owner(spec.getOrphanableTypeInfo());
                    break;
                default:
                    break;
            }
        }
        if (user) {
            fprintf(stderr, "type check of '%s' in a thread that publishes no epoch of type retirement (issue #6419)\n",
                QoreTypeInfo::getName(ti));
            assert(false);
        }
    }
}
#endif

template <typename FilterPolicy>
void QoreTypeInfo::acceptInputInternImpl(ExceptionSink* xsink, const char* arg_type, bool obj, int param_num,
        const char* param_name, QoreValue& n, LValueHelper* lvhelper, q_lvalue_vts_e vts, FilterPolicy policy) const {
#ifdef DEBUG
    qore_type_check_epoch_assert(this);
#endif
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
                    if (t.spec.acceptInputImpl(xsink, *this, t, arg_type, obj, param_num, param_name, n, lvhelper, policy)) {
                        return;
                    }
                }
            }
            // If complex handlers didn't work, try hashdecl/other handlers
            for (auto& t : getAcceptSpecs()) {
                q_typespec_t spec_type = t.spec.getTypeSpec();
                if (spec_type != QTS_COMPLEXHASH && spec_type != QTS_COMPLEXLIST &&
                    spec_type != QTS_COMPLEXSOFTLIST) {
                    if (t.spec.acceptInputImpl(xsink, *this, t, arg_type, obj, param_num, param_name, n, lvhelper, policy)) {
                        return;
                    }
                }
            }
            doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink, vts);
            return;
        }
    }

    // Normal routing for non-optional or non-complex types
    for (auto& t : getAcceptSpecs()) {
        if (t.spec.acceptInputImpl(xsink, *this, t, arg_type, obj, param_num, param_name, n, lvhelper, policy)) {
            return;
        }
    }
    // with %broken-soft-types, a non-optional soft scalar type converts SQL NULL to its zero or empty value
    if (policy.brokenSoftTypes() && n.getType() == NT_NULL && brokenSoftNullImpl()) {
        discard(n.assign(getDefaultQoreValueImpl()), xsink);
        return;
    }
    doAcceptError(false, arg_type, obj, param_num, param_name, n, xsink, vts);
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

namespace {
// a result of an arithmetic operator that has no parse type here (a buffer, or SQL NULL)
constexpr qore_type_t QORE_ARITH_UNTYPED = -1;

// a value type that can be the value of an operand, with the type of the member of the operand type that gives it
struct qore_arith_value_t {
    qore_type_t type;
    //! the type of the member: a type of values of this value type only (ex: hash<X>), or nullptr if it is not known
    //! more precisely than the base type
    const QoreTypeInfo* member;
};

// the member type of an operand type for its values of one value type, if a value of the member type has that type
// itself: a base type, a hash type, a hashdecl, a class or a list type, but not a soft list type (ex: softlist<int>),
// whose values are lists of another type
const QoreTypeInfo* qore_arith_member_type(const QoreTypeSpec& spec) {
    switch (spec.getTypeSpec()) {
        case QTS_TYPE:
        case QTS_CLASS:
        case QTS_HASHDECL:
        case QTS_COMPLEXHASH:
        case QTS_COMPLEXLIST:
            return spec.getTypeInfo();
        default:
            return nullptr;
    }
}

// the value types that can be the value of an operand of the given type, as their runtime type codes, with the member
// types that give them
// returns false if the type does not restrict them: then every type is possible
bool qore_arith_value_types(const QoreTypeInfo* ti, std::vector<qore_arith_value_t>& types) {
    if (!QoreTypeInfo::hasType(ti) || ti == autoTypeInfo || ti == autoNoNarrowTypeInfo || ti == anyTypeInfo) {
        return false;
    }
    for (const QoreReturnSpec& r : ti->return_vec) {
        qore_type_t t = r.spec.getType();
        if (t == NT_ALL || t < 0) {
            return false;
        }
        const QoreTypeInfo* member = t == NT_NOTHING ? nothingTypeInfo : qore_arith_member_type(r.spec);
        bool found = false;
        for (qore_arith_value_t& v : types) {
            if (v.type == t) {
                found = true;
                // two members of one value type: the value type is not known more precisely
                if (v.member != member) {
                    v.member = nullptr;
                }
                break;
            }
        }
        if (!found) {
            types.push_back({t, member});
        }
    }
    return !types.empty();
}

// every value type, for an operand whose type does not restrict its values
const std::vector<qore_arith_value_t>& qore_arith_all_value_types() {
    static const std::vector<qore_arith_value_t> all = {
        {NT_NOTHING, nothingTypeInfo}, {NT_INT, nullptr}, {NT_FLOAT, nullptr}, {NT_NUMBER, nullptr},
        {NT_STRING, nullptr}, {NT_DATE, nullptr}, {NT_BOOLEAN, nullptr}, {NT_BINARY, nullptr}, {NT_LIST, nullptr},
        {NT_HASH, nullptr}, {NT_OBJECT, nullptr}, {NT_NULL, nullptr}, {NT_BUFFER, nullptr},
    };
    return all;
}

bool qore_arith_is_int(qore_type_t t) {
    return t == NT_INT || t == NT_CHAR;
}

// the runtime result type of the operator for operand values of the given types, mirroring the evalImpl() of each
// operator (QorePlusOperatorNode, QoreMinusOperatorNode, QoreMultiplicationOperatorNode)
qore_type_t qore_arith_runtime_result(QoreArithParseOp op, qore_type_t lt, qore_type_t rt) {
    if (lt == NT_BUFFER || rt == NT_BUFFER) {
        return QORE_ARITH_UNTYPED;
    }
    switch (op) {
        case QoreArithParseOp::Plus:
            if (lt == NT_LIST || rt == NT_LIST) {
                return NT_LIST;
            }
            // a char is added as a one-character string
            if (lt == NT_STRING || rt == NT_STRING || lt == NT_CHAR || rt == NT_CHAR) {
                return NT_STRING;
            }
            if (lt == NT_DATE || rt == NT_DATE) {
                return NT_DATE;
            }
            if (lt == NT_NUMBER || rt == NT_NUMBER) {
                return NT_NUMBER;
            }
            if (lt == NT_FLOAT || rt == NT_FLOAT) {
                return NT_FLOAT;
            }
            if (lt == NT_INT || rt == NT_INT) {
                return NT_INT;
            }
            if (lt == NT_HASH) {
                return NT_HASH;
            }
            if (lt == NT_OBJECT) {
                return rt == NT_HASH ? NT_HASH : NT_OBJECT;
            }
            if (rt == NT_HASH || rt == NT_OBJECT) {
                return rt;
            }
            if (lt == NT_BINARY || rt == NT_BINARY) {
                return NT_BINARY;
            }
            return NT_NOTHING;

        case QoreArithParseOp::Minus:
            if (rt == NT_NOTHING) {
                // the left value itself
                if (lt == NT_NULL) {
                    return QORE_ARITH_UNTYPED;
                }
                return qore_arith_is_int(lt) ? NT_INT : lt;
            }
            if (lt == NT_DATE || rt == NT_DATE) {
                return NT_DATE;
            }
            if (lt == NT_NUMBER || rt == NT_NUMBER) {
                return NT_NUMBER;
            }
            if (lt == NT_FLOAT || rt == NT_FLOAT) {
                return NT_FLOAT;
            }
            if (qore_arith_is_int(lt) || qore_arith_is_int(rt)) {
                return NT_INT;
            }
            if (lt == NT_HASH && (rt == NT_LIST || rt == NT_STRING)) {
                return NT_HASH;
            }
            return NT_NOTHING;

        case QoreArithParseOp::Divide:
            // QoreDivisionOperatorNode::doDivision()
            if (lt == NT_NUMBER || rt == NT_NUMBER) {
                return NT_NUMBER;
            }
            if (qore_arith_is_int(lt) && qore_arith_is_int(rt)) {
                return NT_INT;
            }
            return NT_FLOAT;

        case QoreArithParseOp::Multiply:
            if (lt == NT_DATE || rt == NT_DATE) {
                return NT_DATE;
            }
            if (lt == NT_NUMBER || rt == NT_NUMBER) {
                return NT_NUMBER;
            }
            if (lt == NT_FLOAT || rt == NT_FLOAT) {
                return NT_FLOAT;
            }
            if (qore_arith_is_int(lt) || qore_arith_is_int(rt)) {
                return NT_INT;
            }
            return NT_NOTHING;
    }
    assert(false);
    return QORE_ARITH_UNTYPED;
}

// the parse type of a runtime result type, or nullptr if it has none here
const QoreTypeInfo* qore_arith_result_type(qore_type_t t) {
    switch (t) {
        case NT_INT: return bigIntTypeInfo;
        case NT_FLOAT: return floatTypeInfo;
        case NT_NUMBER: return numberTypeInfo;
        case NT_STRING: return stringTypeInfo;
        case NT_DATE: return dateTypeInfo;
        case NT_BOOLEAN: return boolTypeInfo;
        case NT_BINARY: return binaryTypeInfo;
        case NT_LIST: return autoListTypeInfo;
        case NT_HASH: return autoHashTypeInfo;
        case NT_OBJECT: return objectTypeInfo;
        default: return nullptr;
    }
}

bool qore_arith_is_container(qore_type_t t) {
    return t == NT_LIST || t == NT_HASH || t == NT_OBJECT;
}
}

QoreArithParseResult qore_arith_parse_result_type(QoreArithParseOp op, const QoreTypeInfo* lti,
        const QoreTypeInfo* rti, const QoreTypeInfo*& result, bool& single,
        qore_arith_container_rule_t container_rule) {
    result = nullptr;
    std::vector<qore_arith_value_t> lv, rv;
    bool lk = qore_arith_value_types(lti, lv);
    bool rk = qore_arith_value_types(rti, rv);
    const std::vector<qore_arith_value_t>& lvals = lk ? lv : qore_arith_all_value_types();
    const std::vector<qore_arith_value_t>& rvals = rk ? rv : qore_arith_all_value_types();
    single = lk && rk && lv.size() == 1 && rv.size() == 1;

    // the result types with the value types that they are types of
    std::vector<std::pair<qore_type_t, const QoreTypeInfo*>> types;
    bool has_nothing = false;
    bool container = false;
    for (const qore_arith_value_t& l : lvals) {
        for (const qore_arith_value_t& r : rvals) {
            qore_type_t t = qore_arith_runtime_result(op, l.type, r.type);
            if (t == NT_NOTHING) {
                has_nothing = true;
                continue;
            }
            const QoreTypeInfo* ti = qore_arith_result_type(t);
            if (!ti) {
                return QoreArithParseResult::Untyped;
            }
            if (qore_arith_is_container(t)) {
                container = true;
                // the container that the operator gives for the members, if it is known more precisely (ex: the
                // left hash of hash<X> - string has the type of the left operand)
                if (container_rule) {
                    const QoreTypeInfo* member_ti = container_rule(l.type, l.member, r.type, r.member);
                    if (member_ti && QoreTypeInfo::isType(member_ti, t)) {
                        ti = member_ti;
                    }
                }
            }
            std::pair<qore_type_t, const QoreTypeInfo*> entry(t, ti);
            if (std::find(types.begin(), types.end(), entry) == types.end()) {
                types.push_back(entry);
            }
        }
    }
    // a result of a value type with the base type of the value type takes every value of the value type, so the other
    // result types of the value type are not needed
    std::vector<qore_type_t> base_types;
    for (const auto& e : types) {
        if (e.second == qore_arith_result_type(e.first)) {
            base_types.push_back(e.first);
        }
    }
    types.erase(std::remove_if(types.begin(), types.end(), [&base_types](const std::pair<qore_type_t,
            const QoreTypeInfo*>& e) {
        return e.second != qore_arith_result_type(e.first)
            && std::find(base_types.begin(), base_types.end(), e.first) != base_types.end();
    }), types.end());
    // the members are ordered so that a value of each result type is taken by its own member: a union takes a value
    // with its first member that accepts it, and a float accepts an int by converting it, so an int result type comes
    // before a float result type (ex: union<int, float> for *float + int, not union<float, int>, which made 2.0 of 2)
    static const std::vector<qore_type_t> order = {
        NT_INT, NT_FLOAT, NT_NUMBER, NT_STRING, NT_DATE, NT_BOOLEAN, NT_BINARY, NT_LIST, NT_HASH, NT_OBJECT,
    };
    std::stable_sort(types.begin(), types.end(), [](const std::pair<qore_type_t, const QoreTypeInfo*>& a,
            const std::pair<qore_type_t, const QoreTypeInfo*>& b) {
        return std::find(order.begin(), order.end(), a.first) < std::find(order.begin(), order.end(), b.first);
    });
    type_vec_t result_types;
    result_types.reserve(types.size());
    for (const auto& e : types) {
        result_types.push_back(e.second);
    }
    result = result_types.empty() ? nothingTypeInfo : qore_get_union_type(result_types, has_nothing);
    return container ? QoreArithParseResult::Container : QoreArithParseResult::Scalar;
}

bool qore_arith_container_rule_matches(const QoreTypeInfo* rule_ti, const QoreTypeInfo* arith_ti) {
    for (qore_type_t t : {NT_LIST, NT_HASH, NT_OBJECT}) {
        if (QoreTypeInfo::isType(arith_ti, t)) {
            return QoreTypeInfo::isType(rule_ti, t);
        }
    }
    return false;
}
