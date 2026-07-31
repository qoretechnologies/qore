/*
    Datasource.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

    NOTE that 2 copies of connection values are kept in case
    the values are changed while a connection is in use

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
#include <qore/QoreBufferNode.h>
#include "qore/intern/qore_dbi_private.h"
#include "qore/intern/qore_ds_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreSQLStatement.h"
#include "qore/intern/QC_SQLStatement.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/xxhash.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
enum class DbiTypedStorage {
    List,
    Buffer,
};

struct DbiTypedShape {
    DbiTypedStorage storage = DbiTypedStorage::List;
    QoreBufferElementType buffer_type = QoreBufferElementType::Invalid;
    const QoreTypeInfo* element_type = autoTypeInfo;
    bool nullable = false;

    const QoreTypeInfo* getElementType() const {
        return nullable ? qore_get_or_nothing_type(element_type) : element_type;
    }

    const QoreTypeInfo* getColumnType() const {
        if (storage == DbiTypedStorage::Buffer) {
            return qore_get_complex_buffer_type(buffer_type, nullable);
        }
        return qore_get_complex_list_type(getElementType());
    }

    const QoreTypeInfo* getRowType() const {
        if (storage == DbiTypedStorage::Buffer) {
            return qore_buffer_element_scalar_type_info(buffer_type, nullable);
        }
        return getElementType();
    }
};

static std::string dbi_lower_type(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    value = value.substr(begin, end - begin);
    size_t paren = value.find('(');
    if (paren != std::string::npos) {
        value.resize(paren);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
    }
    return value;
}

static bool dbi_type_is_one_of(const std::string& value, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (value == name) {
            return true;
        }
    }
    return false;
}

static std::string dbi_get_string_value(QoreValue value) {
    if (value.getType() != NT_STRING) {
        return std::string();
    }

    ExceptionSink xsink;
    QoreString str;
    value.getAsString(str, 0, &xsink);
    return xsink ? std::string() : std::string(str.c_str());
}

static std::string dbi_get_desc_string(const QoreHashNode* desc, const char* key) {
    return desc ? dbi_get_string_value(desc->getKeyValue(key)) : std::string();
}

static int64 dbi_get_desc_int(const QoreHashNode* desc, const char* key, int64 def = 0) {
    if (!desc) {
        return def;
    }

    QoreValue value = desc->getKeyValue(key);
    return value.getType() == NT_INT ? value.getAsBigInt() : def;
}

static bool dbi_get_desc_nullable(const QoreHashNode* desc) {
    if (!desc) {
        return false;
    }

    QoreValue value = desc->getKeyValue("nullable");
    return value.getType() == NT_BOOLEAN ? value.getAsBool() : false;
}

static const QoreHashNode* dbi_get_desc_column(const QoreHashNode* desc, const char* key) {
    if (!desc) {
        return nullptr;
    }

    QoreValue value = desc->getKeyValue(key);
    return value.getType() == NT_HASH ? value.get<const QoreHashNode>() : nullptr;
}

static bool dbi_shape_from_native_type(const std::string& native_type, int64 maxsize, DbiTypedShape& shape) {
    std::string type = dbi_lower_type(native_type);
    if (type.empty()) {
        return false;
    }

    if (type.size() > 2 && type.compare(type.size() - 2, 2, "[]") == 0) {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, autoTypeInfo, true};
        return true;
    }

    if (!type.empty() && type[0] == '_') {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, autoTypeInfo, true};
        return true;
    }

    if (dbi_type_is_one_of(type, {"bool", "boolean", "bit", "sql_bit"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Bool, boolTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"tinyint", "int1", "sql_tinyint"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Int8, bigIntTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"smallint", "int2", "sql_smallint"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Int16, bigIntTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"integer", "int", "int4", "serial", "mediumint", "sql_integer"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Int32, bigIntTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"bigint", "int8", "bigserial", "longlong", "sql_bigint"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Int64, bigIntTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"real", "float4", "sql_real", "binary_float"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Float32, floatTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"double", "double precision", "float8", "sql_double", "binary_double"})) {
        shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Float64, floatTypeInfo, false};
        return true;
    }

    if (type == "float") {
        shape = {DbiTypedStorage::Buffer,
            maxsize == 4 ? QoreBufferElementType::Float32 : QoreBufferElementType::Float64, floatTypeInfo, false};
        return true;
    }

    if (dbi_type_is_one_of(type, {"numeric", "decimal", "number", "sql_numeric", "sql_decimal"})) {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, numberTypeInfo, false};
        return true;
    }

    if (type.find("char") != std::string::npos || type.find("text") != std::string::npos
            || type.find("clob") != std::string::npos || type == "string" || type == "enum"
            || type == "set" || type == "uuid" || type == "xml") {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, stringTypeInfo, true};
        return true;
    }

    if (type.find("date") != std::string::npos || type.find("time") != std::string::npos) {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, dateTypeInfo, false};
        return true;
    }

    if (type.find("binary") != std::string::npos || type.find("blob") != std::string::npos
            || type == "bytea" || type == "raw" || type == "varbinary") {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, binaryTypeInfo, false};
        return true;
    }

    if (type == "json" || type == "jsonb" || type == "geometry" || type == "geography") {
        shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, autoTypeInfo, true};
        return true;
    }

    return false;
}

static bool dbi_shape_from_desc(const QoreHashNode* desc, DbiTypedShape& shape) {
    if (!desc) {
        return false;
    }

    if (dbi_shape_from_native_type(dbi_get_desc_string(desc, "native_type"), dbi_get_desc_int(desc, "maxsize"), shape)) {
        shape.nullable = shape.nullable || dbi_get_desc_nullable(desc);
        return true;
    }

    switch (dbi_get_desc_int(desc, "type", NT_NOTHING)) {
        case NT_BOOLEAN:
            shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Bool, boolTypeInfo, dbi_get_desc_nullable(desc)};
            return true;
        case NT_INT:
            shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Int64, bigIntTypeInfo, dbi_get_desc_nullable(desc)};
            return true;
        case NT_FLOAT:
            shape = {DbiTypedStorage::Buffer, QoreBufferElementType::Float64, floatTypeInfo, dbi_get_desc_nullable(desc)};
            return true;
        case NT_NUMBER:
            shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, numberTypeInfo, dbi_get_desc_nullable(desc)};
            return true;
        case NT_STRING:
            shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, stringTypeInfo, true};
            return true;
        case NT_DATE:
            shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, dateTypeInfo, dbi_get_desc_nullable(desc)};
            return true;
        case NT_BINARY:
            shape = {DbiTypedStorage::List, QoreBufferElementType::Invalid, binaryTypeInfo,
                dbi_get_desc_nullable(desc)};
            return true;
        default:
            break;
    }

    return false;
}

enum class DbiInferredKind {
    Unknown,
    Bool,
    Int,
    Float,
    Number,
    String,
    Date,
    Binary,
    Auto,
};

static DbiInferredKind dbi_value_kind(QoreValue value) {
    switch (value.getType()) {
        case NT_BOOLEAN:
            return DbiInferredKind::Bool;
        case NT_INT:
            return DbiInferredKind::Int;
        case NT_FLOAT:
            return DbiInferredKind::Float;
        case NT_NUMBER:
            return DbiInferredKind::Number;
        case NT_STRING:
            return DbiInferredKind::String;
        case NT_DATE:
            return DbiInferredKind::Date;
        case NT_BINARY:
            return DbiInferredKind::Binary;
        default:
            return DbiInferredKind::Auto;
    }
}

static void dbi_merge_kind(DbiInferredKind& current, DbiInferredKind value) {
    if (current == DbiInferredKind::Unknown) {
        current = value;
        return;
    }
    if (current == value) {
        return;
    }
    if ((current == DbiInferredKind::Int && value == DbiInferredKind::Float)
            || (current == DbiInferredKind::Float && value == DbiInferredKind::Int)) {
        current = DbiInferredKind::Float;
        return;
    }
    if ((current == DbiInferredKind::Int || current == DbiInferredKind::Float
            || current == DbiInferredKind::Number)
            && (value == DbiInferredKind::Int || value == DbiInferredKind::Float
                || value == DbiInferredKind::Number)) {
        current = DbiInferredKind::Number;
        return;
    }
    current = DbiInferredKind::Auto;
}

static DbiTypedShape dbi_shape_from_kind(DbiInferredKind kind, bool nullable) {
    switch (kind) {
        case DbiInferredKind::Bool:
            return {DbiTypedStorage::Buffer, QoreBufferElementType::Bool, boolTypeInfo, nullable};
        case DbiInferredKind::Int:
            return {DbiTypedStorage::Buffer, QoreBufferElementType::Int64, bigIntTypeInfo, nullable};
        case DbiInferredKind::Float:
            return {DbiTypedStorage::Buffer, QoreBufferElementType::Float64, floatTypeInfo, nullable};
        case DbiInferredKind::Number:
            return {DbiTypedStorage::List, QoreBufferElementType::Invalid, numberTypeInfo, nullable};
        case DbiInferredKind::String:
            return {DbiTypedStorage::List, QoreBufferElementType::Invalid, stringTypeInfo, nullable};
        case DbiInferredKind::Date:
            return {DbiTypedStorage::List, QoreBufferElementType::Invalid, dateTypeInfo, nullable};
        case DbiInferredKind::Binary:
            return {DbiTypedStorage::List, QoreBufferElementType::Invalid, binaryTypeInfo, nullable};
        default:
            return {DbiTypedStorage::List, QoreBufferElementType::Invalid, autoTypeInfo, true};
    }
}

static DbiTypedShape dbi_infer_list_shape(const QoreListNode* list, ExceptionSink* xsink) {
    DbiInferredKind kind = DbiInferredKind::Unknown;
    bool nullable = false;

    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "typed DBI result inference")) {
            return dbi_shape_from_kind(DbiInferredKind::Auto, true);
        }

        QoreValue value = i.getValue();
        if (value.getType() == NT_NOTHING || value.getType() == NT_NULL) {
            nullable = true;
            continue;
        }
        dbi_merge_kind(kind, dbi_value_kind(value));
    }

    return dbi_shape_from_kind(kind, nullable);
}

static bool dbi_list_has_nulls(const QoreListNode* list, ExceptionSink* xsink) {
    ConstListIterator i(list);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "typed DBI result null scan")) {
            return true;
        }
        qore_type_t value_type = i.getValue().getType();
        if (value_type == NT_NOTHING || value_type == NT_NULL) {
            return true;
        }
    }
    return false;
}

static QoreListNode* dbi_make_typed_list(const QoreListNode* source, const QoreTypeInfo* element_type,
        ExceptionSink* xsink) {
    if (element_type == autoTypeInfo) {
        return source->listRefSelf();
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(element_type), xsink);
    ConstListIterator i(source);
    while (i.next()) {
        if (i.index() && !(i.index() % 100) && qore_check_cancel(xsink, "typed DBI list conversion")) {
            return nullptr;
        }

        rv->push(i.getReferencedValue(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return rv.release();
}

static QoreValue dbi_make_column_value(const QoreListNode* source, const DbiTypedShape& shape,
        ExceptionSink* xsink) {
    if (shape.storage == DbiTypedStorage::Buffer) {
        return new QoreBufferNode(shape.buffer_type, shape.nullable, source, xsink);
    }

    return dbi_make_typed_list(source, shape.getElementType(), xsink);
}

static bool dbi_append_signature(std::string& signature, const qore_dbi_typed_result_members_t& members,
        ExceptionSink* xsink) {
    signature = "typed-dbi-result-v1|";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "typed DBI result signature")) {
            return true;
        }
        signature += std::to_string(i);
        signature += ':';
        signature += members[i].first;
        signature += ':';
        signature += QoreTypeInfo::getPath(members[i].second);
        signature += ';';
    }
    return false;
}

struct DbiTypedColumnInfo {
    std::string name;
    const QoreListNode* source = nullptr;
    DbiTypedShape shape;
};
}

void qore_ds_private::statementExecuted(int rc) {
    // we always assume we are in a transaction after executing a transaction-relevant statement
    // unless the connection was aborted
    if (!in_transaction) {
        if (!connection_aborted) {
            assert(!active_transaction);
            assert(isopen);
            in_transaction = true;
            active_transaction = true;
            return;
        }
    }
    else if (!rc && !active_transaction)
        active_transaction = true;
}

void qore_ds_private::clearTypedResultHashDeclCache() {
    AutoLocker al(m);
    size_t count = 0;
    for (auto& i : typed_result_hashdecl_cache) {
        if (count && !(count % 100)) {
            qore_check_cancel(nullptr, "typed DBI result hashdecl cache cleanup");
        }
        typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(i.second))->deref();
        ++count;
    }
    typed_result_hashdecl_cache.clear();
}

const TypedHashDecl* qore_ds_private::getTypedResultHashDecl(const char* driver_name, const std::string& signature,
        const qore_dbi_typed_result_members_t& members, ExceptionSink* xsink) {
    AutoLocker al(m);

    auto i = typed_result_hashdecl_cache.find(signature);
    if (i != typed_result_hashdecl_cache.end()) {
        return i->second;
    }

    std::string safe_driver;
    if (driver_name) {
        for (const char* p = driver_name; *p; ++p) {
            unsigned char c = static_cast<unsigned char>(*p);
            safe_driver += std::isalnum(c) ? static_cast<char>(c) : '_';
        }
    }
    if (safe_driver.empty()) {
        safe_driver = "dbi";
    }

    unsigned hash = XXH32(signature.data(), signature.size(), 0);
    char hash_str[9];
    snprintf(hash_str, sizeof(hash_str), "%08x", hash);

    std::string name = "__minted_" + safe_driver + "_" + hash_str;
    TypedHashDeclHolder hd_holder(new TypedHashDecl(name.c_str(), name.c_str()));
    TypedHashDecl* hd = *hd_holder;
    for (size_t n = 0; n < members.size(); ++n) {
        if (n && !(n % 100) && qore_check_cancel(xsink, "typed DBI result hashdecl creation")) {
            return nullptr;
        }
        const auto& member = members[n];
        hd->addMember(member.first.c_str(), member.second, QoreValue());
    }

    typed_result_hashdecl_cache[signature] = hd_holder.release();
    return hd;
}

QoreHashNode* qore_ds_private::getCurrentOptionHash(bool ensure_hash) const {
    QoreHashNode* options = nullptr;

    ReferenceHolder<QoreHashNode> opts(getOptionHash(), nullptr);
    ConstHashIterator hi(*opts);
    while (hi.next()) {
        QoreValue v = hi.get();
        // if we have private data, then we are dealing with runtime data
        if (private_data) {
            const QoreHashNode* ov = hi.get().get<const QoreHashNode>();
            v = ov->getKeyValue("value");
        }
        // otherwise for pending data, we already have the value in "v"
        if (v.isNothing() || (v.getType() == NT_BOOLEAN && !v.getAsBool())) {
            continue;
        }

        if (!options) {
            options = new QoreHashNode(autoTypeInfo);
        }

        qore_hash_private::get(*options)->setKeyValueIntern(hi.getKey(), v.refSelf());
    }

    if (ensure_hash && !options) {
        options = new QoreHashNode(autoTypeInfo);
    }

    return options;
}

QoreHashNode* qore_ds_private::getConfigHash() const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

    h->setKeyValue("type", new QoreStringNode(dsl->getName()), nullptr);
    if (private_data) {
        if (!username.empty()) {
            h->setKeyValue("user", new QoreStringNode(username), nullptr);
        }
        if (!password.empty()) {
            h->setKeyValue("pass", new QoreStringNode(password), nullptr);
        }
        if (!dbname.empty()) {
            h->setKeyValue("db", new QoreStringNode(dbname), nullptr);
        }
        if (!db_encoding.empty()) {
            h->setKeyValue("charset", new QoreStringNode(db_encoding), nullptr);
        }
        if (!hostname.empty()) {
            h->setKeyValue("host", new QoreStringNode(hostname), nullptr);
        }
        if (port) {
            h->setKeyValue("port", port, nullptr);
        }
    } else {
        if (!p_username.empty()) {
            h->setKeyValue("user", new QoreStringNode(p_username), nullptr);
        }
        if (!p_password.empty()) {
            h->setKeyValue("pass", new QoreStringNode(p_password), nullptr);
        }
        if (!p_dbname.empty()) {
            h->setKeyValue("db", new QoreStringNode(p_dbname), nullptr);
        }
        if (!p_db_encoding.empty()) {
            h->setKeyValue("charset", new QoreStringNode(p_db_encoding), nullptr);
        }
        if (!p_hostname.empty()) {
            h->setKeyValue("host", new QoreStringNode(p_hostname), nullptr);
        }
        if (p_port) {
            h->setKeyValue("port", p_port, nullptr);
        }
    }

    QoreHashNode* options = getCurrentOptionHash();
    if (options) {
        h->setKeyValue("options", options, nullptr);
    }

    return h.release();
}

QoreStringNode* qore_ds_private::getConfigString() const {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(dsl->getName()));
    str->concat(':');

    if (private_data) {
        if (!username.empty()) {
            str->concat(username);
        }
        if (!password.empty()) {
            str->sprintf("/%s", password.c_str());
        }
        // the '@' symbol must be present even if the database name is empty
        str->concat('@');
        if (!dbname.empty()) {
            str->concat(dbname);
        }
        if (!db_encoding.empty()) {
            str->sprintf("(%s)", db_encoding.c_str());
        }
        if (!hostname.empty()) {
            str->sprintf("%%%s", hostname.c_str());
        }
        if (port) {
            str->sprintf(":%d", port);
        }
    } else {
        if (!p_username.empty()) {
            str->concat(p_username);
        }
        if (!p_password.empty()) {
            str->sprintf("/%s", p_password.c_str());
        }
        if (!p_dbname.empty()) {
            str->sprintf("@%s", p_dbname.c_str());
        }
        if (!p_db_encoding.empty()) {
            str->sprintf("(%s)", p_db_encoding.c_str());
        }
        if (!p_hostname.empty()) {
            str->sprintf("%%%s", p_hostname.c_str());
        }
        if (p_port) {
            str->sprintf(":%d", p_port);
        }
    }

    bool first = false;
    ReferenceHolder<QoreHashNode> opts(getOptionHash(), nullptr);
    ConstHashIterator hi(*opts);
    while (hi.next()) {
        QoreValue v = hi.get();
        // if we have private data, then we are dealing with runtime data
        if (private_data) {
            const QoreHashNode* ov = hi.get().get<const QoreHashNode>();
            v = ov->getKeyValue("value");
        }
        // otherwise for pending data, we already have the value in "v"
        if (v.isNothing() || (v.getType() == NT_BOOLEAN && !v.getAsBool()))
            continue;

        if (first) {
            str->concat(',');
        } else {
            str->concat('{');
            first = true;
        }
        str->concat(hi.getKey());
        if (v.getType() == NT_BOOLEAN && v.getAsBool()) {
            continue;
        }

        QoreStringValueHelper sv(v);
        str->sprintf("=%s", sv->getBuffer());
    }
    if (first) {
        str->concat('}');
    }

    return str.release();
}

Datasource::Datasource(DBIDriver* ndsl, DatasourceStatementHelper* dsh) : priv(new qore_ds_private(this, ndsl, dsh)) {
}

Datasource::Datasource(const Datasource& old, DatasourceStatementHelper* dsh) : priv(new qore_ds_private(*old.priv, this, dsh)) {
}

Datasource::Datasource(DBIDriver* ndsl) : priv(new qore_ds_private(this, ndsl, nullptr)) {
}

Datasource::Datasource(const Datasource& old) : priv(new qore_ds_private(*old.priv, this, nullptr)) {
}

Datasource::~Datasource() {
    if (priv->isopen)
        close();

    delete priv;
}

void Datasource::setPendingConnectionValues(const Datasource* other) {
    priv->setPendingConnectionValues(other->priv);
}

void Datasource::setTransactionStatus(bool t) {
    //printd(5, "Datasource::setTS(%d) this=%p\n", t, this);
    priv->in_transaction = t;
}

QoreListNode* Datasource::getCapabilityList() const {
    return qore_dbi_private::get(*priv->dsl)->getCapList();
}

int Datasource::getCapabilities() const {
    return qore_dbi_private::get(*priv->dsl)->getCaps();
}

bool Datasource::isInTransaction() const {
    return priv->in_transaction;
}

bool Datasource::activeTransaction() const {
    return priv->active_transaction;
}

bool Datasource::getAutoCommit() const {
    return priv->autocommit;
}

bool Datasource::isOpen() const {
    return priv->isopen;
}

Datasource* Datasource::copy() const {
    return new Datasource(*this);
}

void Datasource::setConnectionValues() {
    priv->setConnectionValues();
}

void Datasource::setAutoCommit(bool ac) {
    priv->autocommit = ac;
}

QoreHashNode* qore_dbi_make_typed_select_result(Datasource* ds, const QoreHashNode* columns,
        const QoreHashNode* desc, ExceptionSink* xsink) {
    assert(xsink);
    if (!ds || !columns) {
        xsink->raiseException("DBI-TYPED-SELECT-ERROR", "typed select result creation requires a Datasource and "
            "a column result hash");
        return nullptr;
    }

    std::vector<DbiTypedColumnInfo> column_info;
    qore_dbi_typed_result_members_t members;
    column_info.reserve(columns->size());
    members.reserve(columns->size());

    ConstHashIterator i(columns);
    while (i.next()) {
        if (column_info.size() && !(column_info.size() % 100)
                && qore_check_cancel(xsink, "typed DBI column result inference")) {
            return nullptr;
        }

        QoreValue value = i.get();
        if (value.getType() != NT_LIST) {
            return columns->hashRefSelf();
        }

        const QoreListNode* source = value.get<const QoreListNode>();
        DbiTypedShape shape;
        const QoreHashNode* column_desc = dbi_get_desc_column(desc, i.getKey());
        if (dbi_shape_from_desc(column_desc, shape)) {
            shape.nullable = shape.nullable || dbi_list_has_nulls(source, xsink);
        } else {
            shape = dbi_infer_list_shape(source, xsink);
        }
        if (*xsink) {
            return nullptr;
        }

        column_info.push_back({i.getKey(), source, shape});
        members.push_back({i.getKey(), shape.getColumnType()});
    }

    std::string signature;
    if (dbi_append_signature(signature, members, xsink)) {
        return nullptr;
    }
    const TypedHashDecl* hd = qore_ds_private::get(*ds)->getTypedResultHashDecl(ds->getDriverName(), signature,
        members, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hd, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    for (size_t n = 0; n < column_info.size(); ++n) {
        if (n && !(n % 100) && qore_check_cancel(xsink, "typed DBI column result conversion")) {
            return nullptr;
        }

        ValueHolder column_value(dbi_make_column_value(column_info[n].source, column_info[n].shape, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->setKeyValue(column_info[n].name.c_str(), column_value.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return rv.release();
}

QoreListNode* qore_dbi_make_typed_select_rows_result(Datasource* ds, const QoreListNode* rows,
        const QoreHashNode* desc, ExceptionSink* xsink) {
    assert(xsink);
    if (!ds || !rows) {
        xsink->raiseException("DBI-TYPED-SELECT-ERROR", "typed select row result creation requires a Datasource and "
            "a row result list");
        return nullptr;
    }

    if (rows->empty() && (!desc || desc->empty())) {
        return rows->listRefSelf();
    }

    std::vector<std::string> names;
    if (desc && !desc->empty()) {
        ConstHashIterator hi(desc);
        while (hi.next()) {
            if (names.size() && !(names.size() % 100)
                    && qore_check_cancel(xsink, "typed DBI row result column collection")) {
                return nullptr;
            }
            names.push_back(hi.getKey());
        }
    } else {
        QoreValue first = rows->retrieveEntry(0);
        if (first.getType() != NT_HASH) {
            return rows->listRefSelf();
        }

        ConstHashIterator hi(first.get<const QoreHashNode>());
        while (hi.next()) {
            if (names.size() && !(names.size() % 100)
                    && qore_check_cancel(xsink, "typed DBI row result column collection")) {
                return nullptr;
            }
            names.push_back(hi.getKey());
        }
    }

    qore_dbi_typed_result_members_t members;
    members.reserve(names.size());

    for (size_t n = 0; n < names.size(); ++n) {
        if (n && !(n % 100) && qore_check_cancel(xsink, "typed DBI row result inference")) {
            return nullptr;
        }

        DbiTypedShape shape;
        bool has_desc_shape = dbi_shape_from_desc(dbi_get_desc_column(desc, names[n].c_str()), shape);
        DbiInferredKind kind = DbiInferredKind::Unknown;
        bool nullable = has_desc_shape ? shape.nullable : false;

        ConstListIterator li(rows);
        while (li.next()) {
            if (li.index() && !(li.index() % 100) && qore_check_cancel(xsink, "typed DBI row result scan")) {
                return nullptr;
            }

            QoreValue row_value = li.getValue();
            if (row_value.getType() != NT_HASH) {
                return rows->listRefSelf();
            }

            bool exists = false;
            QoreValue value = row_value.get<const QoreHashNode>()->getKeyValueExistence(names[n].c_str(), exists);
            if (!exists || value.getType() == NT_NOTHING || value.getType() == NT_NULL) {
                nullable = true;
                continue;
            }

            if (!has_desc_shape) {
                dbi_merge_kind(kind, dbi_value_kind(value));
            }
        }

        if (has_desc_shape) {
            shape.nullable = nullable;
        } else {
            shape = dbi_shape_from_kind(kind, nullable);
        }
        members.push_back({names[n], shape.getRowType()});
    }

    std::string signature;
    if (dbi_append_signature(signature, members, xsink)) {
        return nullptr;
    }
    const TypedHashDecl* hd = qore_ds_private::get(*ds)->getTypedResultHashDecl(ds->getDriverName(), signature,
        members, xsink);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hd->getTypeInfo()), xsink);
    ConstListIterator li(rows);
    while (li.next()) {
        if (li.index() && !(li.index() % 100) && qore_check_cancel(xsink, "typed DBI row result conversion")) {
            return nullptr;
        }

        QoreValue row_value = li.getValue();
        if (row_value.getType() != NT_HASH) {
            return rows->listRefSelf();
        }

        const QoreHashNode* source = row_value.get<const QoreHashNode>();
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(hd, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        for (size_t n = 0; n < names.size(); ++n) {
            if (n && !(n % 100) && qore_check_cancel(xsink, "typed DBI row result member conversion")) {
                return nullptr;
            }
            const std::string& name = names[n];
            row->setKeyValue(name.c_str(), source->getKeyValue(name.c_str()).refSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        rv->push(row.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return rv.release();
}

//! mutation observer admission point for a read operation
/** Reads are identified structurally by the core API used; no SQL text is inspected.

    @return 0 to continue; -1 if the observer rejected the operation, in which case an exception has
    been raised in \a xsink and the operation must not be executed
*/
static int ds_mutation_read_pre(qore_ds_private* priv, ExceptionSink* xsink) {
    if (!priv->observes(SQL_MUTATION_MASK_READ)) {
        return 0;
    }
    SqlMutationEvent ev(SQL_MUTATION_EVENT_PRE_EXEC, SQL_STMT_CLASS_READ);
    return priv->dispatchMutationEvent(ev, xsink);
}

//! mutation observer result boundary for a read operation
static void ds_mutation_read_post(qore_ds_private* priv, ExceptionSink* xsink) {
    if (!priv->getMutationCtx()) {
        return;
    }
    if (priv->observes(SQL_MUTATION_MASK_READ)) {
        SqlMutationEvent ev(SQL_MUTATION_EVENT_POST_EXEC, SQL_STMT_CLASS_READ);
        if (priv->connection_aborted) {
            ev.outcome = SQL_MUTATION_OUTCOME_LOST_CONNECTION;
            ev.replay_safe = 1;
        } else {
            ev.outcome = *xsink ? SQL_MUTATION_OUTCOME_ERROR : SQL_MUTATION_OUTCOME_OK;
        }
        ev.driver_xsink = xsink;
        priv->dispatchMutationEvent(ev, xsink);
    }
    // a connection lost during a read still ends any open transaction
    priv->flushMutationConnectionLost(xsink);

    // in autocommit mode the read is its own implicit transaction, which Datasource::autoCommit()
    // is about to close; no commit outcome is reported for a read, but the transaction identity
    // must not leak into the next operation
    if (priv->autocommit) {
        priv->tx_id.clear();
        priv->tx_seq = 0;
    }
}

QoreValue Datasource::select(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return QoreValue();
    }
    QoreValue rv = qore_dbi_private::get(*priv->dsl)->select(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreValue Datasource::selectTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return QoreValue();
    }
    QoreValue rv = qore_dbi_private::get(*priv->dsl)->selectTyped(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreColumnarResult* Datasource::selectColumnar(const QoreString* query_str, const QoreListNode* args,
        ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return nullptr;
    }
    QoreColumnarResult* rv = qore_dbi_private::get(*priv->dsl)->selectColumnar(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreValue Datasource::selectRows(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return QoreValue();
    }
    QoreValue rv = qore_dbi_private::get(*priv->dsl)->selectRows(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreValue Datasource::selectRowsTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return QoreValue();
    }
    QoreValue rv = qore_dbi_private::get(*priv->dsl)->selectRowsTyped(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreHashNode* Datasource::selectRow(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return nullptr;
    }
    QoreHashNode* rv = qore_dbi_private::get(*priv->dsl)->selectRow(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

QoreValue Datasource::exec_internal(bool doBind, const QoreString* query_str, const QoreListNode* args,
        ExceptionSink* xsink) {
    assert(xsink);

    // mutation observer admission point; delivered before any implicit transaction is started so
    // that a rejected operation can never leave an open transaction behind
    if (priv->observes(SQL_MUTATION_MASK_EXEC)) {
        SqlMutationEvent ev(SQL_MUTATION_EVENT_PRE_EXEC, SQL_STMT_CLASS_EXEC);
        if (priv->dispatchMutationEvent(ev, xsink)) {
            return QoreValue();
        }
    }

    const bool tx_begin = !priv->autocommit && !priv->in_transaction;
    if (tx_begin && beginImplicitTransaction(xsink))
        return QoreValue();

    if (tx_begin && priv->getMutationCtx()) {
        priv->dispatchMutationTxBegin(xsink);
    }

    assert(priv->isopen && priv->private_data);

    QoreValue rv = doBind ? qore_dbi_private::get(*priv->dsl)->execSQL(this, query_str, args, xsink)
        : qore_dbi_private::get(*priv->dsl)->execRawSQL(this, query_str, xsink);;
    //printd(5, "Datasource::exec_internal() this=%p, autocommit=%d, in_transaction=%d, xsink=%d\n", this,
    //    priv->autocommit, priv->in_transaction, xsink->isException());

    if (priv->connection_aborted) {
        assert(*xsink);
        assert(!rv);
        if (priv->getMutationCtx()) {
            if (priv->observes(SQL_MUTATION_MASK_EXEC)) {
                SqlMutationEvent ev(SQL_MUTATION_EVENT_POST_EXEC, SQL_STMT_CLASS_EXEC);
                ev.outcome = SQL_MUTATION_OUTCOME_LOST_CONNECTION;
                // no commit was in flight, so the operation can be replayed
                ev.replay_safe = 1;
                ev.driver_xsink = xsink;
                priv->dispatchMutationEvent(ev, xsink);
            }
            priv->flushMutationConnectionLost(xsink);
        }
        return QoreValue();
    }

    const bool stmt_ok = !*xsink;

    if (priv->observes(SQL_MUTATION_MASK_EXEC)) {
        SqlMutationEvent ev(SQL_MUTATION_EVENT_POST_EXEC, SQL_STMT_CLASS_EXEC);
        ev.outcome = stmt_ok ? SQL_MUTATION_OUTCOME_OK : SQL_MUTATION_OUTCOME_ERROR;
        ev.driver_xsink = xsink;
        priv->dispatchMutationEvent(ev, xsink);
    }

    if (priv->autocommit) {
        if (priv->getMutationCtx()) {
            priv->commit_in_progress = true;
        }
        qore_dbi_private::get(*priv->dsl)->autoCommit(this, xsink);
        if (priv->getMutationCtx()) {
            priv->commit_in_progress = false;
            int outcome;
            int replay_safe;
            if (!stmt_ok) {
                // the statement itself failed, so in autocommit mode nothing can have been
                // committed; this is structural, not derived from the error
                outcome = SQL_MUTATION_OUTCOME_ROLLBACK;
                replay_safe = 1;
            } else if (*xsink || priv->connection_aborted) {
                // the commit did not demonstrably succeed; the server may still have applied it
                outcome = SQL_MUTATION_OUTCOME_COMMIT_AMBIGUOUS;
                replay_safe = 0;
            } else {
                outcome = SQL_MUTATION_OUTCOME_COMMIT;
                replay_safe = 0;
            }
            priv->dispatchMutationOutcome(SQL_STMT_CLASS_COMMIT, outcome, replay_safe, xsink, xsink);
        }
    } else {
        priv->statementExecuted(*xsink);
    }

    return rv;
}

int Datasource::autoCommit(ExceptionSink* xsink) {
    if (priv->autocommit && !priv->connection_aborted)
        return qore_dbi_private::get(*priv->dsl)->autoCommit(this, xsink);
    return 0;
}

QoreValue Datasource::exec(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    return exec_internal(true, query_str, args, xsink);
}

// deprecated: remove due to extraneous ignored "args" argument
QoreValue Datasource::execRaw(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(!args);
    return exec_internal(false, query_str, nullptr, xsink);
}

QoreValue Datasource::execRaw(const QoreString* query_str, ExceptionSink* xsink) {
    return exec_internal(false, query_str, nullptr, xsink);
}

QoreHashNode* Datasource::describe(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    assert(xsink);
    if (ds_mutation_read_pre(priv, xsink)) {
        return nullptr;
    }
    QoreHashNode* rv = qore_dbi_private::get(*priv->dsl)->describe(this, query_str, args, xsink);
    ds_mutation_read_post(priv, xsink);
    autoCommit(xsink);

    // set active_transaction flag if in a transaction and the active_transaction flag
    // has not yet been set and no exception was raised
    if (priv->in_transaction && !priv->active_transaction && !*xsink)
        priv->active_transaction = true;

    return rv;
}

int Datasource::beginImplicitTransaction(ExceptionSink* xsink) {
    //printd(5, "Datasource::beginImplicitTransaction() autocommit=%s\n", autocommit ? "true" : "false");
    if (priv->autocommit) {
        xsink->raiseException("AUTOCOMMIT-ERROR", "%s:%s@%s: transaction management is not available because "
            "autocommit is enabled for this Datasource", getDriverName(), priv->username.c_str(), priv->dbname.c_str());
        return -1;
    }
    return qore_dbi_private::get(*priv->dsl)->beginTransaction(this, xsink);
}

int Datasource::beginTransaction(ExceptionSink* xsink) {
    int rc = beginImplicitTransaction(xsink);
    if (!rc && !priv->in_transaction) {
        priv->in_transaction = true;
        assert(!priv->active_transaction);
        if (priv->getMutationCtx()) {
            priv->dispatchMutationTxBegin(xsink);
        }
    }
    return rc;
}

int Datasource::commit(ExceptionSink* xsink) {
    if (!priv->in_transaction && beginImplicitTransaction(xsink))
        return -1;

    return priv->commit(xsink);
}

int Datasource::rollback(ExceptionSink* xsink) {
    if (!priv->in_transaction && beginImplicitTransaction(xsink))
        return -1;

    return priv->rollback(xsink);
}

int Datasource::open(ExceptionSink* xsink) {
    assert(xsink);
    int rc;

    if (!priv->isopen) {
        // copy pending connection values to connection values
        setConnectionValues();

        priv->connection_aborted = false;

        rc = qore_dbi_private::get(*priv->dsl)->init(this, xsink);
        if (!*xsink) {
            assert(priv->qorecharset);
            priv->isopen = true;
        }
    } else {
        rc = 0;
    }

    return rc;
}

int Datasource::close() {
    return priv->close();
}

void Datasource::connectionAborted() {
    ExceptionSink xsink;
    priv->connectionAborted(&xsink);
    xsink.clear();
}

void Datasource::connectionAborted(ExceptionSink* xsink) {
    priv->connectionAborted(xsink);
}

void Datasource::connectionLost(ExceptionSink* xsink) {
    priv->connectionLost(xsink);
}

void Datasource::connectionRecovered(ExceptionSink* xsink) {
    priv->connectionRecovered(xsink);
}

bool Datasource::wasConnectionAborted() const {
    return priv->connection_aborted;
}

// forces a close and open to reset a database connection
void Datasource::reset(ExceptionSink* xsink) {
    if (priv->isopen) {
        // close the Datasource
        qore_dbi_private::get(*priv->dsl)->close(this);
        priv->isopen = false;

        // open the connection
        open(xsink);

        //printd(5, "Datasource::reset() this: %p priv: %p in_transaction: %d active_transaction: %d\n", this, priv, priv->in_transaction, priv->active_transaction);

        // close any open transaction(s)
        priv->in_transaction = false;
        priv->active_transaction = false;
        priv->commit_in_progress = false;
        priv->tx_id.clear();
        priv->tx_seq = 0;
    }
}

void* Datasource::getPrivateData() const {
    return priv->private_data;
}

void Datasource::setPrivateData(void* data) {
    priv->private_data = data;
}

void Datasource::setPendingUsername(const char* u) {
    priv->p_username = u;
}

void Datasource::setPendingPassword(const char* p) {
    priv->p_password = p;
}

void Datasource::setPendingDBName(const char* d) {
    priv->p_dbname = d;
}

void Datasource::setPendingDBEncoding(const char* c) {
    priv->p_db_encoding = c;
}

void Datasource::setPendingHostName(const char* h) {
    priv->p_hostname = h;
}

void Datasource::setPendingPort(int port) {
    priv->p_port = port;
}

const std::string &Datasource::getUsernameStr() const {
    return priv->username;
}

const std::string &Datasource::getPasswordStr() const {
    return priv->password;
}

const std::string &Datasource::getDBNameStr() const {
    return priv->dbname;
}

const std::string &Datasource::getDBEncodingStr() const {
    return priv->db_encoding;
}

const std::string &Datasource::getHostNameStr() const {
    return priv->hostname;
}

const char* Datasource::getUsername() const {
    return priv->username.empty() ? nullptr : priv->username.c_str();
}

const char* Datasource::getPassword() const {
    return priv->password.empty() ? nullptr : priv->password.c_str();
}

const char* Datasource::getDBName() const {
    return priv->dbname.empty() ? nullptr : priv->dbname.c_str();
}

const char* Datasource::getDBEncoding() const {
    return priv->db_encoding.empty() ? nullptr : priv->db_encoding.c_str();
}

const char* Datasource::getOSEncoding() const {
    return priv->qorecharset ? priv->qorecharset->getCode() : nullptr;
}

const char* Datasource::getHostName() const {
    return priv->hostname.empty() ? nullptr : priv->hostname.c_str();
}

int Datasource::getPort() const {
    return priv->port;
}

const QoreEncoding* Datasource::getQoreEncoding() const {
    return priv->qorecharset;
}

void Datasource::setDBEncoding(const char* name) {
    priv->db_encoding = name;
}

void Datasource::setQoreEncoding(const char* name) {
    priv->qorecharset = QEM.findCreate(name);
}

void Datasource::setQoreEncoding(const QoreEncoding* enc) {
    priv->qorecharset = enc;
}

QoreStringNode* Datasource::getPendingUsername() const {
    return priv->p_username.empty() ? nullptr : new QoreStringNode(priv->p_username.c_str());
}

QoreStringNode* Datasource::getPendingPassword() const {
    return priv->p_password.empty() ? nullptr : new QoreStringNode(priv->p_password.c_str());
}

QoreStringNode* Datasource::getPendingDBName() const {
    return priv->p_dbname.empty() ? nullptr : new QoreStringNode(priv->p_dbname.c_str());
}

QoreStringNode* Datasource::getPendingDBEncoding() const {
    return priv->p_db_encoding.empty() ? nullptr : new QoreStringNode(priv->p_db_encoding.c_str());
}

QoreStringNode* Datasource::getPendingHostName() const {
    return priv->p_hostname.empty() ? nullptr : new QoreStringNode(priv->p_hostname.c_str());
}

int Datasource::getPendingPort() const {
    return priv->p_port;
}

const char* Datasource::getDriverName() const {
    return priv->dsl->getName();
}

const DBIDriver* Datasource::getDriver() const {
    return priv->dsl;
}

QoreValue Datasource::getServerVersion(ExceptionSink* xsink) {
    return qore_dbi_private::get(*priv->dsl)->getServerVersion(this, xsink);
}

QoreValue Datasource::getClientVersion(ExceptionSink* xsink) const {
    return qore_dbi_private::get(*priv->dsl)->getClientVersion(this, xsink);
}

QoreStringNode* Datasource::getDriverRealName(ExceptionSink* xsink) {
    return qore_dbi_private::get(*priv->dsl)->getDriverRealName(this, xsink);
}

QoreHashNode* Datasource::getOptionHash() const {
    return priv->getOptionHash();
}

int Datasource::setOption(const char* opt, const QoreValue val, ExceptionSink* xsink) {
    // maintain a copy of the option internally
    priv->setOption(opt, val, xsink);
    // only set options in private data if private data is already set
    if (priv->private_data) {
        return qore_dbi_private::get(*priv->dsl)->opt_set(this, opt, val, xsink);
    }

    // issue #3243: validate options before sending them to the driver
    OptInputHelper opt_helper(xsink, *qore_dbi_private::get(*priv->dsl), opt, true, val);
    return *xsink ? -1 : 0;
}

const QoreHashNode* Datasource::getConnectOptions() const {
    return priv->opt;
}

QoreValue Datasource::getOption(const char* opt, ExceptionSink* xsink) {
    if (!isOpen()) {
        xsink->raiseException("DATASOURCE-ERROR", "cannot retrieve the value for option '%s' when the datasource is " \
            "closed; use getOptionHash() to retrieve raw configuration option when the datasource is closed", opt);
        return QoreValue();
    }
    return qore_dbi_private::get(*priv->dsl)->opt_get(this, opt, xsink);
}

QoreHashNode* Datasource::getConfigHash() const {
    return priv->getConfigHash();
}

QoreHashNode* Datasource::getCurrentOptionHash() const {
    return priv->getCurrentOptionHash();
}

QoreStringNode* Datasource::getConfigString() const {
    return priv->getConfigString();
}

void Datasource::setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
    priv->setEventQueue(q, arg, xsink);
}

QoreHashNode* Datasource::getEventQueueHash(Queue*& q, int event_code) const {
    return priv->getEventQueueHash(q, event_code);
}

QoreObject* Datasource::getSQLStatementObjectForResultSet(void* stmt_private_data) {
    return new QoreObject(QC_SQLSTATEMENT, getProgram(), new QoreSQLStatement(this, stmt_private_data, priv->dsh, STMT_EXECED));
}

void Datasource::setMutationObserver(ResolvedCallReferenceNode* observer, int64 event_mask, QoreValue arg,
        ExceptionSink* xsink) {
    assert(observer);
    priv->getOrCreateMutationContext()->setObserver(observer, event_mask, arg, xsink);
}

void Datasource::clearMutationObserver(ExceptionSink* xsink) {
    if (SqlMutationContext* ctx = priv->getMutationCtx()) {
        ctx->clearObserver(xsink);
    }
}

bool Datasource::hasMutationObserver() const {
    SqlMutationContext* ctx = priv->getMutationCtx();
    return ctx && ctx->hasObserver();
}

int Datasource::pushMutationDeclaration(const QoreHashNode* info, ExceptionSink* xsink) {
    return priv->getOrCreateMutationContext()->pushDeclaration(info, xsink);
}

int Datasource::popMutationDeclaration(ExceptionSink* xsink) {
    SqlMutationContext* ctx = priv->getMutationCtx();
    if (!ctx) {
        xsink->raiseException(SQL_MUTATION_DECLARATION_ERR, "there is no active mutation declaration in this thread "
            "to remove");
        return -1;
    }
    return ctx->popDeclaration(xsink);
}

QoreHashNode* Datasource::getMutationDeclaration() const {
    SqlMutationContext* ctx = priv->getMutationCtx();
    return ctx ? ctx->getDeclaration() : nullptr;
}

bool Datasource::sqlMutationObserverActive() const {
    return priv->observes(SQL_MUTATION_MASK_STREAM);
}

int Datasource::reportMutationStreamBegin(int64 declared_bytes, ExceptionSink* xsink) {
    assert(xsink);
    if (declared_bytes <= 0) {
        // fall back to the declared growth bound, if the producer declared one
        declared_bytes = -1;
        if (SqlMutationContext* ctx = priv->getMutationCtx()) {
            ReferenceHolder<QoreHashNode> decl(ctx->getDeclaration(), xsink);
            if (decl) {
                int64 mgb = decl->getKeyValue("max_growth_bytes").getAsBigInt();
                if (mgb > 0) {
                    declared_bytes = mgb;
                }
            }
        }
    }
    priv->stream_declared_bytes = declared_bytes;

    if (!priv->observes(SQL_MUTATION_MASK_STREAM)) {
        return 0;
    }
    SqlMutationEvent ev(SQL_MUTATION_EVENT_STREAM_BEGIN, SQL_STMT_CLASS_STREAM);
    ev.declared_bytes = declared_bytes;
    ev.consumed_bytes = 0;
    return priv->dispatchMutationEvent(ev, xsink);
}

int Datasource::reportMutationStreamProgress(int64 consumed_bytes, ExceptionSink* xsink) {
    assert(xsink);
    if (!priv->observes(SQL_MUTATION_MASK_STREAM)) {
        return 0;
    }
    SqlMutationEvent ev(SQL_MUTATION_EVENT_STREAM_PROGRESS, SQL_STMT_CLASS_STREAM);
    ev.declared_bytes = priv->stream_declared_bytes;
    ev.consumed_bytes = consumed_bytes < 0 ? 0 : consumed_bytes;
    return priv->dispatchMutationEvent(ev, xsink);
}

int Datasource::reportMutationStreamEnd(int64 consumed_bytes, bool ok, ExceptionSink* xsink) {
    assert(xsink);
    const int64 declared_bytes = priv->stream_declared_bytes;
    priv->stream_declared_bytes = -1;

    if (!priv->observes(SQL_MUTATION_MASK_STREAM)) {
        return 0;
    }
    SqlMutationEvent ev(SQL_MUTATION_EVENT_STREAM_END, SQL_STMT_CLASS_STREAM);
    ev.declared_bytes = declared_bytes;
    ev.consumed_bytes = consumed_bytes < 0 ? 0 : consumed_bytes;
    ev.outcome = ok ? SQL_MUTATION_OUTCOME_OK : SQL_MUTATION_OUTCOME_ERROR;
    ev.driver_xsink = xsink;
    // stream end is a notification: it cannot reject anything that has already been streamed
    priv->dispatchMutationEvent(ev, xsink);
    return 0;
}

SqlMutationContext* Datasource::getMutationContext() const {
    return priv->getMutationCtx();
}

void Datasource::setMutationContext(SqlMutationContext* ctx, ExceptionSink* xsink) {
    priv->setMutationContext(ctx, xsink);
}

SqlMutationContext* Datasource::getOrCreateMutationContext() {
    return priv->getOrCreateMutationContext();
}
