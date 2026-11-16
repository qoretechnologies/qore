/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_column.cpp

    DataFrame column utilities implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_column.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <utility>

namespace QoreDataFrameNS {

namespace {

static std::vector<QoreValue> refAutoData(const std::vector<QoreValue>& old) {
    std::vector<QoreValue> rv;
    rv.reserve(old.size());
    for (const QoreValue& v : old) {
        rv.push_back(v.refSelf());
    }
    return rv;
}

static void discardAutoData(std::vector<QoreValue>& values, ExceptionSink* xsink) {
    for (QoreValue& v : values) {
        v.discard(xsink);
    }
    values.clear();
}

static bool getBitmapBit(const uint8_t* bytes, size_t bit_offset, int64_t index) {
    if (!bytes) {
        return true;
    }
    size_t bit = bit_offset + static_cast<size_t>(index);
    return bytes[bit / 8] & (uint8_t(1) << (bit % 8));
}

static int checkRawBufferData(const void* data, int64_t n, QoreBufferElementType type, ExceptionSink* xsink) {
    if (n && !data) {
        xsink->raiseException("DATAFRAME-ERROR",
            "buffer<%s> has no host data available for DataFrame conversion",
            qore_buffer_element_type_name(type));
        return -1;
    }
    return 0;
}

static void splitMicros(int64_t micros, int64_t& seconds, int& microseconds) {
    seconds = micros / 1000000;
    microseconds = static_cast<int>(micros % 1000000);
    if (microseconds < 0) {
        --seconds;
        microseconds += 1000000;
    }
}

static bool isDurationColumn(const ColumnData& col) {
    return col.isDuration();
}

static DateTimeNode* makeDateFromMicros(int64_t micros, bool duration) {
    int64_t seconds = 0;
    int microseconds = 0;
    splitMicros(micros, seconds, microseconds);
    if (duration) {
        return dataFrameMakeDuration(seconds, microseconds);
    }
    return DateTimeNode::makeAbsolute(currentTZ(), seconds, microseconds);
}

}

ColumnData::ColumnData(const ColumnData& old)
        : type(old.type), n_rows(old.n_rows), dense_buffer(old.dense_buffer),
        dense_buffer_type(old.dense_buffer_type), external_column_kind(old.external_column_kind),
        external_column_owner(old.external_column_owner), float_data(old.float_data), int_data(old.int_data),
        str_data(old.str_data), bool_data(old.bool_data), date_data(old.date_data),
        auto_data(refAutoData(old.auto_data)), columnar_schema(old.columnar_schema),
        has_columnar_schema(old.has_columnar_schema), null_mask(old.null_mask) {
    if (dense_buffer) {
        dense_buffer->ref();
    }
}

ColumnData::ColumnData(ColumnData&& old) noexcept
        : type(old.type), n_rows(old.n_rows), dense_buffer(old.dense_buffer),
        dense_buffer_type(old.dense_buffer_type), external_column_kind(old.external_column_kind),
        external_column_owner(std::move(old.external_column_owner)), float_data(std::move(old.float_data)),
        int_data(std::move(old.int_data)), str_data(std::move(old.str_data)),
        bool_data(std::move(old.bool_data)), date_data(std::move(old.date_data)),
        auto_data(std::move(old.auto_data)), columnar_schema(std::move(old.columnar_schema)),
        has_columnar_schema(old.has_columnar_schema), null_mask(std::move(old.null_mask)) {
    old.dense_buffer = nullptr;
    old.dense_buffer_type = QoreBufferElementType::Invalid;
    old.external_column_kind = ExternalColumnKind::NONE;
    old.has_columnar_schema = false;
}

ColumnData& ColumnData::operator=(const ColumnData& old) {
    if (this != &old) {
        // the values of a column that a holder reports to the garbage collector do not change
        assert(!scan_claimed.load(std::memory_order_relaxed));
        scan_state.store(-1, std::memory_order_relaxed);
        std::vector<QoreValue> new_auto_data = refAutoData(old.auto_data);
        if (old.dense_buffer) {
            old.dense_buffer->ref();
        }
        ExceptionSink xsink;
        if (dense_buffer) {
            dense_buffer->deref(&xsink);
        }
        discardAutoData(auto_data, &xsink);

        type = old.type;
        n_rows = old.n_rows;
        dense_buffer = old.dense_buffer;
        dense_buffer_type = old.dense_buffer_type;
        external_column_kind = old.external_column_kind;
        external_column_owner = old.external_column_owner;
        float_data = old.float_data;
        int_data = old.int_data;
        str_data = old.str_data;
        bool_data = old.bool_data;
        date_data = old.date_data;
        auto_data = std::move(new_auto_data);
        columnar_schema = old.columnar_schema;
        has_columnar_schema = old.has_columnar_schema;
        null_mask = old.null_mask;
    }
    return *this;
}

ColumnData& ColumnData::operator=(ColumnData&& old) noexcept {
    if (this != &old) {
        // the values of a column that a holder reports to the garbage collector do not change
        assert(!scan_claimed.load(std::memory_order_relaxed));
        scan_state.store(-1, std::memory_order_relaxed);
        ExceptionSink xsink;
        if (dense_buffer) {
            dense_buffer->deref(&xsink);
        }
        discardAutoData(auto_data, &xsink);

        type = old.type;
        n_rows = old.n_rows;
        dense_buffer = old.dense_buffer;
        dense_buffer_type = old.dense_buffer_type;
        external_column_kind = old.external_column_kind;
        external_column_owner = std::move(old.external_column_owner);
        float_data = std::move(old.float_data);
        int_data = std::move(old.int_data);
        str_data = std::move(old.str_data);
        bool_data = std::move(old.bool_data);
        date_data = std::move(old.date_data);
        auto_data = std::move(old.auto_data);
        columnar_schema = std::move(old.columnar_schema);
        has_columnar_schema = old.has_columnar_schema;
        null_mask = std::move(old.null_mask);

        old.dense_buffer = nullptr;
        old.dense_buffer_type = QoreBufferElementType::Invalid;
        old.external_column_kind = ExternalColumnKind::NONE;
        old.has_columnar_schema = false;
    }
    return *this;
}

ColumnData::~ColumnData() {
    ExceptionSink xsink;
    if (dense_buffer) {
        dense_buffer->deref(&xsink);
    }
    discardAutoData(auto_data, &xsink);
}

void ColumnData::setDenseBufferRef(const QoreBufferNode* buffer) {
    clearExternalColumnRef();
    if (dense_buffer) {
        ExceptionSink xsink;
        dense_buffer->deref(&xsink);
        dense_buffer = nullptr;
        dense_buffer_type = QoreBufferElementType::Invalid;
    }
    if (buffer) {
        dense_buffer = const_cast<QoreBufferNode*>(buffer);
        dense_buffer->ref();
        dense_buffer_type = buffer->getElementType();
    }
}

void ColumnData::setExternalColumnRef(ExternalColumnKind kind, std::shared_ptr<void> owner) {
    external_column_kind = owner ? kind : ExternalColumnKind::NONE;
    external_column_owner = std::move(owner);
}

void ColumnData::clearExternalColumnRef() {
    external_column_kind = ExternalColumnKind::NONE;
    external_column_owner.reset();
}

std::shared_ptr<ColumnData> ColumnData::copyForUpdate() const {
    auto rv = std::make_shared<ColumnData>(*this);
    // the preserved storage holds the values of this column, not the changed values of the copy
    rv->setDenseBufferRef(nullptr);
    rv->clearExternalColumnRef();
    return rv;
}

QoreBufferNode* ColumnData::refDenseBuffer() const {
    if (!dense_buffer) {
        return nullptr;
    }
    dense_buffer->ref();
    return dense_buffer;
}

void ColumnData::setAutoValue(int64_t i, QoreValue value, ExceptionSink* xsink) {
    assert(i >= 0 && i < static_cast<int64_t>(auto_data.size()));
    // the values do not change once needsScan() has been computed
    assert(scan_state.load(std::memory_order_relaxed) < 0);
    auto_data[i].discard(xsink);
    auto_data[i] = value.refSelf();
}

void ColumnData::appendAutoValue(QoreValue value) {
    // the values do not change once needsScan() has been computed
    assert(scan_state.load(std::memory_order_relaxed) < 0);
    auto_data.push_back(value.refSelf());
}

bool dataFrameMergeColumnSchema(bool& has_schema, QoreColumnarTypeDescriptor& schema,
        const ColumnData& first, const ColumnData& src) {
    if (src.type != first.type) {
        return false;
    }
    if (src.type == ColumnType::DATE) {
        if (src.isDuration() != first.isDuration()) {
            return false;
        }
    } else if (has_schema != src.has_columnar_schema) {
        return false;
    }
    if (!src.has_columnar_schema) {
        if (has_schema) {
            // a plain date column: absolute dates in microseconds with no time zone and no native type, so a native
            // type of the schema describes the cells of the schema's columns only
            if (schema.time_unit != "us") {
                schema.time_unit = "us";
            }
            if (!schema.timezone.empty()) {
                schema.timezone = "UTC";
            }
            schema.native_type.clear();
        }
        return true;
    }
    const QoreColumnarTypeDescriptor& other = src.columnar_schema;
    if (!has_schema) {
        // a temporal schema merged into a plain date column, which has no native type
        schema = other;
        schema.name.clear();
        has_schema = true;
        if (schema.time_unit != "us") {
            schema.time_unit = "us";
        }
        if (!schema.timezone.empty()) {
            schema.timezone = "UTC";
        }
        schema.native_type.clear();
        return true;
    }
    if (schema.kind != other.kind || schema.buffer_type != other.buffer_type || schema.fixed_size != other.fixed_size
            || !schema.children.empty() || !other.children.empty()) {
        return false;
    }
    if (schema.kind == QoreColumnarTypeKind::Decimal128
            && (schema.precision != other.precision || schema.scale != other.scale)) {
        int32_t int_digits = std::max(schema.precision - schema.scale, other.precision - other.scale);
        int32_t scale = std::max(schema.scale, other.scale);
        if (int_digits + scale > 38) {
            return false;
        }
        schema.precision = int_digits + scale;
        schema.scale = scale;
        schema.native_type.clear();
    } else if (schema.precision != other.precision || schema.scale != other.scale) {
        return false;
    }
    if (schema.time_unit != other.time_unit) {
        schema.time_unit = "us";
        schema.native_type.clear();
    }
    if (schema.timezone != other.timezone) {
        schema.timezone = "UTC";
        schema.native_type.clear();
    }
    if (other.nullable) {
        schema.nullable = true;
    }
    // a native type that is not the same for all columns describes none of them
    if (schema.native_type != other.native_type) {
        schema.native_type.clear();
    }
    return true;
}

bool dataFrameIntDecimalSchema(const std::vector<const ColumnData*>& cols, bool& has_schema,
        QoreColumnarTypeDescriptor& schema) {
    // the types first, with no scan of the cells
    if (!dataFrameIntDecimalTypes(cols)) {
        return false;
    }
    const ColumnData* first_decimal = nullptr;
    bool nullable = false;
    // an int64 holds at most 19 digits
    int32_t int_digits = 19;
    int32_t scale = 0;
    for (const ColumnData* cd : cols) {
        if (cd->type == ColumnType::INT64) {
            nullable = nullable || cd->countNull() > 0;
        } else {
            if (!first_decimal) {
                first_decimal = cd;
            }
            const QoreColumnarTypeDescriptor& d = cd->columnar_schema;
            int_digits = std::max(int_digits, d.precision - d.scale);
            scale = std::max(scale, d.scale);
            nullable = nullable || d.nullable;
        }
    }
    assert(first_decimal);
    if (int_digits + scale > 38) {
        has_schema = false;
        return true;
    }
    schema = first_decimal->columnar_schema;
    schema.name.clear();
    schema.native_type.clear();
    schema.precision = int_digits + scale;
    schema.scale = scale;
    schema.nullable = nullable;
    has_schema = true;
    return true;
}

bool dataFrameIntDecimalTypes(const std::vector<const ColumnData*>& cols) {
    bool any_int = false;
    bool any_decimal = false;
    for (const ColumnData* cd : cols) {
        if (cd->type == ColumnType::INT64) {
            any_int = true;
        } else if (dataFrameIsDecimalColumn(*cd)) {
            any_decimal = true;
        } else {
            return false;
        }
    }
    return any_int && any_decimal;
}

QoreValue dataFrameDecimalValue(const ColumnData& cd, int64_t row, ExceptionSink* xsink) {
    if (cd.isNull(row)) {
        return QoreValue();
    }
    if (cd.type == ColumnType::INT64) {
        return new QoreNumberNode(static_cast<int64>(cd.int_data[row]));
    }
    return cd.getValueAt(row, xsink);
}

void dataFrameSetDurationSchema(ColumnData& col) {
    col.columnar_schema = QoreColumnarTypeDescriptor();
    col.columnar_schema.kind = QoreColumnarTypeKind::Duration;
    col.columnar_schema.column_type = QoreColumnarColumnType::Date;
    col.columnar_schema.time_unit = "us";
    col.columnar_schema.nullable = true;
    col.has_columnar_schema = true;
}

namespace {
//! appends the length of a part of a key and the part
void appendKeyBytes(std::string& key, const char* data, size_t len) {
    uint64_t n = len;
    key.append(reinterpret_cast<const char*>(&n), sizeof(n));
    key.append(data, len);
}

//! returns the value that a value of a column that holds any value stands for, and its type (qore#6107)
/** An enum member stands for its value, as with the core operators, and an opaque or weak reference for the value
    that it refers to (a weak reference to an object is keyed as the object; one whose object is deleted as no
    value, which makes a key of its own, apart from the null cells of the column).  The tags of the value are
    checked inline, and the reference is resolved only for a reference.
*/
static inline QoreValue resolveKeyValue(QoreValue v, qore_type_t& t) {
    // a float, held in the value, is told with no call into the library
    if (v.isFloat()) {
        t = NT_FLOAT;
        return v;
    }
    if (v.isEnum()) {
        v = v.getEnumBaseValue();
    }
    if (v.isOpaque()) {
        v = v.resolveIndirect();
    }
    t = v.getType();
    if (t == NT_WEAKREF || t == NT_WEAKREF_HASH || t == NT_WEAKREF_LIST) {
        v = v.resolveIndirect();
        t = v.getType();
    }
    return v;
}

//! appends the key of a date: its kind and microseconds, so that two dates make one key if they are equal
/** An absolute date is keyed by its microseconds since the epoch in UTC, so that two dates at the same instant in
    different time zones make one key, as they are equal with the == operator; a relative date (a duration) by its
    length in microseconds, as a column of durations orders them, so that 24h and 1D make one key (a month is 31
    days and a year 365 days); a date whose microseconds do not fit in an int64 by its seconds and microseconds, or
    by its parts (qore#6107).
*/
static void appendDateKey(std::string& key, const DateTimeNode* dt) {
    int64 micros;
    switch (dt->getEpochMicrosecondsUTCCheckedKind(micros)) {
        case 1:
            key += 'd';
            key.append(reinterpret_cast<const char*>(&micros), sizeof(micros));
            return;
        case 2:
            key += 'r';
            key.append(reinterpret_cast<const char*>(&micros), sizeof(micros));
            return;
        default:
            break;
    }
    if (dt->isRelative()) {
        qore_tm parts;
        dt->getInfo(parts);
        int fields[] = {parts.year, parts.month, parts.day, parts.hour, parts.minute, parts.second, parts.us};
        key += 'R';
        key.append(reinterpret_cast<const char*>(fields), sizeof(fields));
        return;
    }
    int64 seconds = dt->getEpochSecondsUTC();
    int us = dt->getMicrosecond();
    key += 'D';
    key.append(reinterpret_cast<const char*>(&seconds), sizeof(seconds));
    key.append(reinterpret_cast<const char*>(&us), sizeof(us));
}

//! appends the key of an arbitrary-precision number: its exact value, whatever its precision, so that two numbers
//! make one key if and only if they are equal; NaN numbers make one key, and -0 and 0 one key (qore#6107)
/** The key of QoreNumberNode::appendExactKey() does not delimit itself, so it is appended with its length.
*/
static void appendNumberKey(std::string& key, const QoreNumberNode* n) {
    key += 'n';
    size_t len_pos = key.size();
    key.append(sizeof(uint64_t), '\0');
    n->appendExactKey(key);
    uint64_t len = key.size() - len_pos - sizeof(uint64_t);
    memcpy(&key[len_pos], &len, sizeof(len));
}

//! appends the key of a value that is not a list or a hash, whose type is given (see appendAutoValueKey())
void appendScalarValueKey(std::string& key, QoreValue v, qore_type_t t) {
    switch (t) {
        case NT_NOTHING:
            key += 'N';
            break;
        // NULL != NOTHING with the == operator (a null cell of a column has a key of its own at the top level)
        case NT_NULL:
            key += 'U';
            break;
        case NT_FLOAT:
            appendDataFrameFloatKey(key, 'g', v.isFloat() ? v.getDouble() : v.getAsFloat());
            break;
        case NT_DATE:
            appendDateKey(key, v.get<const DateTimeNode>());
            break;
        case NT_NUMBER:
            appendNumberKey(key, v.get<const QoreNumberNode>());
            break;
        case NT_INT: {
            int64 iv = v.getAsBigInt();
            key += 'i';
            key.append(reinterpret_cast<const char*>(&iv), sizeof(iv));
            break;
        }
        case NT_BOOLEAN:
            key += v.getAsBool() ? 'T' : 'F';
            break;
        case NT_STRING: {
            // a string is compared in UTF-8, the encoding of the string cells, so that equal strings in different
            // encodings make the same key; a UTF-8 string (a short string too) is appended with no copy
            key += 's';
            QoreStringDataHelper str(v);
            if (str.getEncoding() == QCS_UTF8) {
                appendKeyBytes(key, str.c_str(), str.size());
            } else {
                std::string utf8 = dataFrameUtf8Key(v);
                appendKeyBytes(key, utf8.data(), utf8.size());
            }
            break;
        }
        case NT_BINARY: {
            const BinaryNode* b = v.get<const BinaryNode>();
            key += 'x';
            appendKeyBytes(key, static_cast<const char*>(b->getPtr()), b->size());
            break;
        }
        case NT_OBJECT:
        case NT_RUNTIME_CLOSURE:
        case NT_FUNCREF: {
            // keyed by identity
            const void* p = v.getInternalNode();
            key += 'O';
            key.append(reinterpret_cast<const char*>(&p), sizeof(p));
            break;
        }
        default: {
            // another value (a char, for example): its type and its string form
            key += 'a';
            key.append(reinterpret_cast<const char*>(&t), sizeof(t));
            std::string str = dataFrameUtf8Key(v);
            appendKeyBytes(key, str.data(), str.size());
            break;
        }
    }
}

//! appends the key of a value of a column that holds any value (qore#6107)
/** A float is keyed by its bits, as in a float64 column, as its string form can be the same for different floats; a
    date by its kind and microseconds (see appendDateKey()); an arbitrary-precision number by its exact value; a
    binary value by its bytes; a list by its elements and a hash by its keys and values (in the order of its sorted
    keys, so that equal hashes with their keys in another order make the same key), so that two lists or hashes have
    the same key only if they are equal; an object, a closure or a call reference by its identity; an enum member,
    an opaque or a weak reference as the value it stands for (see resolveKeyValue()); an int, a bool or a string by
    its value; any other value by its type and string form.  A string is compared in UTF-8, the encoding of the
    string cells, so that equal strings in different encodings make the same key.  Values of different types never
    make the same key: 1, 1.0, "1" and True make four keys.  Each part has its tag and a part of any length its
    length, so that two different values never make the same key.  The elements are walked with a stack of their
    own, so that a deeply nested value cannot exhaust the thread's stack.
*/
void appendAutoValueKey(std::string& key, QoreValue top) {
    // a value that is not a list or a hash needs no stack
    qore_type_t top_type;
    top = resolveKeyValue(top, top_type);
    if (top_type != NT_LIST && top_type != NT_HASH) {
        appendScalarValueKey(key, top, top_type);
        return;
    }
    // a value to key, after the bytes of the hash key of the value, if any
    struct Item {
        QoreValue v;
        const char* hash_key;
    };
    std::vector<Item> pending;
    pending.push_back({top, nullptr});
    while (!pending.empty()) {
        Item item = pending[pending.size() - 1];
        pending.pop_back();
        if (item.hash_key) {
            appendKeyBytes(key, item.hash_key, strlen(item.hash_key));
        }
        qore_type_t t;
        QoreValue v = resolveKeyValue(item.v, t);
        switch (t) {
            case NT_LIST: {
                const QoreListNode* l = v.get<const QoreListNode>();
                key += 'L';
                uint64_t n = l->size();
                key.append(reinterpret_cast<const char*>(&n), sizeof(n));
                // the elements are keyed in order: they are pushed in reverse
                for (size_t i = l->size(); i > 0; --i) {
                    pending.push_back({l->retrieveEntry(i - 1), nullptr});
                }
                break;
            }
            case NT_HASH: {
                const QoreHashNode* h = v.get<const QoreHashNode>();
                key += 'H';
                uint64_t n = h->size();
                key.append(reinterpret_cast<const char*>(&n), sizeof(n));
                // a hash key is in the default encoding, as in every hash; the members are keyed in the order of
                // their sorted keys
                std::vector<std::pair<const char*, QoreValue>> members;
                members.reserve(h->size());
                ConstHashIterator hi(h);
                while (hi.next()) {
                    members.emplace_back(hi.getKey(), hi.get());
                }
                std::sort(members.begin(), members.end(), [] (const std::pair<const char*, QoreValue>& a,
                        const std::pair<const char*, QoreValue>& b) {
                    return strcmp(a.first, b.first) < 0;
                });
                for (size_t i = members.size(); i > 0; --i) {
                    pending.push_back({members[i - 1].second, members[i - 1].first});
                }
                break;
            }
            default:
                appendScalarValueKey(key, v, t);
                break;
        }
    }
}
}

void dataFrameAppendAutoValueKey(std::string& key, QoreValue v) {
    appendAutoValueKey(key, v);
}

void dataFrameAppendNumberKey(std::string& key, const QoreNumberNode* n) {
    appendNumberKey(key, n);
}

std::string dataFrameValueKey(const ColumnData& cd, int64_t row) {
    if (cd.isNull(row)) {
        return std::string("\xffnull", 5);
    }

    std::string key;
    switch (cd.type) {
        case ColumnType::FLOAT64:
            appendDataFrameFloatKey(key, 'f', cd.float_data(row));
            return key;
        case ColumnType::INT64: {
            int64_t value = cd.int_data[row];
            key.assign("i", 1);
            key.append(reinterpret_cast<const char*>(&value), sizeof(value));
            return key;
        }
        case ColumnType::STRING:
            key.assign("s", 1);
            key.append(cd.str_data[row]);
            return key;
        case ColumnType::BOOL:
            return cd.bool_data[row] ? std::string("b1", 2) : std::string("b0", 2);
        case ColumnType::DATE: {
            int64_t value = cd.date_data[row];
            key.assign("d", 1);
            key.append(reinterpret_cast<const char*>(&value), sizeof(value));
            return key;
        }
        case ColumnType::AUTO:
            appendAutoValueKey(key, cd.auto_data[row]);
            return key;
        default:
            return {};
    }
}

bool ColumnData::needsScan() const {
    int state = scan_state.load(std::memory_order_acquire);
    if (state < 0) {
        // threads computing it at the same time compute the same answer
        state = 0;
        for (const QoreValue& v : auto_data) {
            if (AbstractScannablePrivateData::needsScan(v)) {
                state = 1;
                break;
            }
        }
        scan_state.store(state, std::memory_order_release);
    }
    return state != 0;
}

size_t claimScanColumns(std::vector<Column>& columns, std::vector<std::shared_ptr<ColumnData>>& replaced) {
    size_t count = 0;
    try {
        for (Column& col : columns) {
            if (!col.data->needsScan()) {
                continue;
            }
            if (!col.data->claimScan()) {
                // the values are reported by another holder, or by this one for an earlier column: this one gets
                // its own references to them
                std::shared_ptr<ColumnData> copy = std::make_shared<ColumnData>(*col.data);
                bool claimed = copy->claimScan();
                assert(claimed);
                static_cast<void>(claimed);
                replaced.reserve(replaced.size() + 1);
                replaced.push_back(std::move(col.data));
                col.data = std::move(copy);
            }
            ++count;
        }
    } catch (...) {
        // the claims made are released, so that the holder claims no column unless it reports them all
        for (Column& col : columns) {
            if (!count) {
                break;
            }
            if (col.data->needsScan()) {
                col.data->releaseScan();
                --count;
            }
        }
        throw;
    }
    return count;
}

void releaseScanColumns(const std::vector<Column>& columns) {
    for (const Column& col : columns) {
        if (col.data->needsScan()) {
            col.data->releaseScan();
        }
    }
}

void markScanColumnValuesStored(QoreObject& self, const std::vector<Column>& columns) {
    for (const Column& col : columns) {
        if (!col.data->needsScan()) {
            continue;
        }
        for (const QoreValue& v : col.data->auto_data) {
            if (AbstractScannablePrivateData::needsScan(v)) {
                self.privateDataValueStored(v);
            }
        }
    }
}

bool scanColumnValues(const std::vector<Column>& columns, QorePrivateDataScanner& scanner) {
    for (const Column& col : columns) {
        if (!col.data->needsScan()) {
            continue;
        }
        for (const QoreValue& v : col.data->auto_data) {
            if (scanner.scan(v)) {
                return true;
            }
        }
    }
    return false;
}

const char* columnTypeName(ColumnType type) {
    switch (type) {
        case ColumnType::FLOAT64: return "float64";
        case ColumnType::INT64:   return "int64";
        case ColumnType::STRING:  return "string";
        case ColumnType::BOOL:    return "bool";
        case ColumnType::DATE:    return "date";
        case ColumnType::AUTO:    return "auto";
        default:                  return "unknown";
    }
}

int dataFrameConvertUtf8String(QoreValue v, std::string& out, ExceptionSink* xsink) {
    ExceptionSink conv_xsink;
    QoreStringValueHelper str(v, QCS_UTF8, &conv_xsink);
    if (conv_xsink) {
        conv_xsink.clear();
        QoreStringDataHelper src(v);
        xsink->raiseException("DATAFRAME-ENCODING-ERROR", "a string in encoding '%s' cannot be converted to UTF-8, "
            "the encoding of the string cells of a DataFrame", src ? src.getEncoding()->getCode() : "unknown");
        return -1;
    }
    out.assign(str->c_str(), str->size());
    return 0;
}

int dataFrameConvertUtf8Bytes(const char* data, size_t len, const QoreEncoding* enc, std::string& out,
        ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(data, len, enc));
    return dataFrameConvertUtf8String(QoreValue(*str), out, xsink);
}

int dataFrameConvertFromUtf8(const std::string& utf8, const QoreEncoding* enc, std::string& out,
        ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(utf8, QCS_UTF8));
    ExceptionSink conv_xsink;
    QoreStringValueHelper converted(QoreValue(*str), enc, &conv_xsink);
    if (conv_xsink) {
        conv_xsink.clear();
        xsink->raiseException("DATAFRAME-ENCODING-ERROR", "a DataFrame string cannot be converted from UTF-8 to "
            "encoding '%s'", enc->getCode());
        return -1;
    }
    out.assign(converted->c_str(), converted->size());
    return 0;
}

std::string dataFrameUtf8Key(QoreValue v) {
    std::string out;
    ExceptionSink xsink;
    if (dataFrameUtf8String(v, out, &xsink)) {
        xsink.clear();
        QoreStringValueHelper str(v);
        out.assign(str->c_str(), str->size());
    }
    return out;
}

void dataFrameRaiseDateRangeError(const char* desc, ExceptionSink* xsink) {
    xsink->raiseException("DATAFRAME-DATE-RANGE-ERROR", "%s is outside the range of a DataFrame date, which is "
        "stored in microseconds since the epoch in an int64 (from -290308-12-21T19:59:05.224192Z to "
        "294247-01-10T04:00:54.775807Z)", desc);
}

int dataFrameDateOutOfRange(const DateTimeNode* dt, ExceptionSink* xsink) {
    QoreString desc;
    if (dt->isRelative()) {
        // the parts are read with getInfo(), as DateTime::getYear() returns a short, which cannot hold the years of
        // a long time value
        qore_tm parts;
        dt->getInfo(parts);
        desc.sprintf("the time value of %d years, %d months, %d days, %d hours, %d minutes, %d seconds and %d "
            "microseconds", parts.year, parts.month, parts.day, parts.hour, parts.minute, parts.second, parts.us);
        // a time value is stored as its microseconds (qore#6107)
        xsink->raiseException("DATAFRAME-DATE-RANGE-ERROR", "%s is outside the range of a DataFrame time value, "
            "which is stored in microseconds in an int64 (about 292471 years either way)", desc.c_str());
        return -1;
    } else {
        desc.sprintf("the date " QLLD " seconds after the epoch", (int64)dt->getEpochSecondsUTC());
    }
    dataFrameRaiseDateRangeError(desc.c_str(), xsink);
    return -1;
}

int64_t ColumnData::countNonNull() const {
    int64_t count = 0;
    for (int64_t i = 0; i < n_rows; ++i) {
        if (!isNull(i)) {
            ++count;
        }
    }
    return count;
}

int64_t ColumnData::countNull() const {
    return n_rows - countNonNull();
}

QoreValue ColumnData::getValueAt(int64_t i, ExceptionSink* xsink) const {
    if (i < 0 || i >= n_rows) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "row index " QLLD " out of range (0.." QLLD ")", (int64)i, (int64)(n_rows - 1));
        return QoreValue();
    }
    if (isNull(i)) {
        return QoreValue();  // NOTHING
    }

    switch (type) {
        case ColumnType::FLOAT64:
            return QoreValue(float_data(i));
        case ColumnType::INT64:
            return QoreValue(int_data[i]);
        case ColumnType::STRING:
            return QoreValue::makeStringValue(str_data[i], QCS_UTF8);
        case ColumnType::BOOL:
            return QoreValue((bool)bool_data[i]);
        case ColumnType::DATE:
            return QoreValue(makeDateFromMicros(date_data[i], isDurationColumn(*this)));
        case ColumnType::AUTO:
            assert(i < static_cast<int64_t>(auto_data.size()));
            return auto_data[i].refSelf();
        default:
            return QoreValue();
    }
}

std::shared_ptr<ColumnData> buildColumnData(const QoreListNode* values,
        ColumnType type, ExceptionSink* xsink) {
    auto col = std::make_shared<ColumnData>();
    col->type = type;
    int64_t n = values ? (int64_t)values->size() : 0;
    col->n_rows = n;
    col->null_mask.resize(n, 0);

    switch (type) {
        case ColumnType::FLOAT64: {
            col->float_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
                } else {
                    col->float_data(i) = v.getAsFloat();
                }
            }
            break;
        }
        case ColumnType::INT64: {
            col->int_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->int_data[i] = 0;
                } else {
                    col->int_data[i] = v.getAsBigInt();
                }
            }
            break;
        }
        case ColumnType::STRING: {
            col->str_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                } else if (dataFrameUtf8String(v, col->str_data[i], xsink)) {
                    return nullptr;
                }
            }
            break;
        }
        case ColumnType::BOOL: {
            col->bool_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->bool_data[i] = 0;
                } else {
                    col->bool_data[i] = v.getAsBool() ? 1 : 0;
                }
            }
            break;
        }
        case ColumnType::DATE: {
            col->date_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                    col->date_data[i] = 0;
                } else if (v.getType() == NT_DATE) {
                    if (dataFrameDateMicros(v.get<const DateTimeNode>(), col->date_data[i], xsink)) {
                        return nullptr;
                    }
                } else {
                    // Non-date value in a date column → treat as null
                    // (type inference should prevent this; if it happens,
                    // the column would have been inferred as STRING instead)
                    col->null_mask[i] = 1;
                    col->date_data[i] = 0;
                }
            }
            break;
        }
        case ColumnType::AUTO: {
            col->auto_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame auto column")) {
                    return nullptr;
                }
                QoreValue v = values->retrieveEntry(i);
                if (v.isNullOrNothing()) {
                    col->null_mask[i] = 1;
                } else {
                    col->setAutoValue(i, v, xsink);
                }
            }
            break;
        }
        default:
            break;
    }

    return col;
}

//! Builds a column of values of two scalar types, found at the given value: a string column, which stores a scalar
//! value as its string form, unless a later value is not a scalar, which makes a column that holds any value
//! (qore#6097)
static std::shared_ptr<ColumnData> buildMixedColumnData(const QoreListNode* values, int64_t start,
        ExceptionSink* xsink) {
    for (size_t i = static_cast<size_t>(start); i < values->size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
            return nullptr;
        }
        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            continue;
        }
        switch (v.getType()) {
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
            case NT_STRING:
            case NT_BOOLEAN:
            case NT_DATE:
                break;
            default:
                return buildColumnData(values, ColumnType::AUTO, xsink);
        }
    }
    return buildColumnData(values, ColumnType::STRING, xsink);
}

std::shared_ptr<ColumnData> buildColumnDataAuto(const QoreListNode* values,
        ExceptionSink* xsink) {
    int64_t n = values ? (int64_t)values->size() : 0;
    if (!n) {
        return buildColumnData(values, ColumnType::STRING, xsink);
    }

    auto col = std::make_shared<ColumnData>();
    col->type = ColumnType::AUTO;
    col->n_rows = n;
    col->null_mask.resize(n, 0);

    auto set_float_nulls = [&](int64_t count) -> bool {
        for (int64_t j = 0; j < count; ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
                return true;
            }
            if (col->null_mask[j]) {
                col->float_data(j) = std::numeric_limits<double>::quiet_NaN();
            }
        }
        return false;
    };

    auto initialize_type = [&](ColumnType type, int64_t initialized_rows) -> bool {
        col->type = type;
        switch (type) {
            case ColumnType::FLOAT64:
                col->float_data.resize(n);
                return set_float_nulls(initialized_rows);
            case ColumnType::INT64:
                col->int_data.resize(n);
                break;
            case ColumnType::STRING:
                col->str_data.resize(n);
                break;
            case ColumnType::BOOL:
                col->bool_data.resize(n);
                break;
            case ColumnType::DATE:
                col->date_data.resize(n);
                break;
            case ColumnType::AUTO:
                col->auto_data.resize(n);
                break;
            default:
                break;
        }
        return false;
    };

    auto widen_int_to_float = [&](int64_t initialized_rows) -> bool {
        Eigen::VectorXd float_data(n);
        for (int64_t j = 0; j < initialized_rows; ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
                return true;
            }
            float_data(j) = col->null_mask[j]
                ? std::numeric_limits<double>::quiet_NaN() : (double)col->int_data[j];
        }
        col->int_data.clear();
        col->int_data.shrink_to_fit();
        col->float_data = std::move(float_data);
        col->type = ColumnType::FLOAT64;
        return false;
    };

    // the error of a string or date that cannot be stored in a string or date cell is raised only if the column
    // keeps its type to the end: a later value can make it a string column, which stores a date as a string, or a
    // column that holds any value, which stores any string and date
    struct DeferredError {
        ExceptionSink xsink;
        //! true if the sink holds an error, which is not checked with the sink itself, a call into the library
        bool raised = false;

        ~DeferredError() {
            xsink.clear();
        }
    } deferred;

    // the kind of the dates of a date column (a DataFrameDateKind value, -1 before the first date, never 0, the
    // value of a date that cannot be stored, so that such a date is never taken for one of that kind): a column of
    // absolute and relative dates (durations) holds any value, as a concatenation of the two does, and a column of
    // relative dates is a column of durations (qore#6107)
    int date_kind = -1;

    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column")) {
            return nullptr;
        }

        QoreValue v = values->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            col->null_mask[i] = 1;
            if (col->type == ColumnType::FLOAT64) {
                col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
            }
            continue;
        }

        switch (v.getType()) {
            case NT_INT:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::INT64, i)) {
                        return nullptr;
                    }
                } else if (col->type == ColumnType::FLOAT64) {
                    col->float_data(i) = v.getAsFloat();
                    continue;
                } else if (col->type != ColumnType::INT64) {
                    return buildMixedColumnData(values, i, xsink);
                }
                col->int_data[i] = v.getAsBigInt();
                break;

            case NT_FLOAT:
            case NT_NUMBER:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::FLOAT64, i)) {
                        return nullptr;
                    }
                } else if (col->type == ColumnType::INT64) {
                    if (widen_int_to_float(i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::FLOAT64) {
                    return buildMixedColumnData(values, i, xsink);
                }
                col->float_data(i) = v.getAsFloat();
                break;

            case NT_STRING: {
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::STRING, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::STRING) {
                    return buildMixedColumnData(values, i, xsink);
                }
                if (!deferred.raised && dataFrameUtf8String(v, col->str_data[i], &deferred.xsink)) {
                    deferred.raised = true;
                }
                break;
            }

            case NT_BOOLEAN:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::BOOL, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::BOOL) {
                    return buildMixedColumnData(values, i, xsink);
                }
                col->bool_data[i] = v.getAsBool() ? 1 : 0;
                break;

            case NT_DATE:
                if (col->type == ColumnType::AUTO) {
                    if (initialize_type(ColumnType::DATE, i)) {
                        return nullptr;
                    }
                } else if (col->type != ColumnType::DATE) {
                    return buildMixedColumnData(values, i, xsink);
                }
                {
                    const DateTimeNode* dt = v.get<const DateTimeNode>();
                    int64 micros;
                    // a date of the kind of the others is stored with one comparison
                    int kind = deferred.raised ? 0 : dt->getEpochMicrosecondsUTCCheckedKind(micros);
                    if (kind == date_kind) {
                        col->date_data[i] = micros;
                        break;
                    }
                    if (!kind) {
                        // a date that cannot be stored in a date cell; its kind is needed to tell whether the column
                        // holds any value, which stores any date
                        if (!deferred.raised) {
                            dataFrameDateOutOfRange(dt, &deferred.xsink);
                            deferred.raised = true;
                        }
                        kind = dt->isRelative() ? DFDK_RELATIVE : DFDK_ABSOLUTE;
                    } else {
                        col->date_data[i] = micros;
                    }
                    if (date_kind < 0) {
                        date_kind = kind;
                    } else if (kind != date_kind) {
                        return buildColumnData(values, ColumnType::AUTO, xsink);
                    }
                }
                break;

            default:
                return buildColumnData(values, ColumnType::AUTO, xsink);
        }
    }

    if (deferred.raised) {
        xsink->assimilate(deferred.xsink);
        return nullptr;
    }

    if (date_kind == DFDK_RELATIVE) {
        dataFrameSetDurationSchema(*col);
    }

    if (col->type == ColumnType::AUTO) {
        if (initialize_type(ColumnType::FLOAT64, n)) {
            return nullptr;
        }
    }

    return col;
}

std::shared_ptr<ColumnData> buildColumnDataFromBuffer(const QoreBufferNode* values,
        ExceptionSink* xsink) {
    if (!values) {
        return std::make_shared<ColumnData>();
    }
    if (values->ensureHostStorage(xsink)) {
        return nullptr;
    }

    auto col = std::make_shared<ColumnData>();
    int64_t n = values ? (int64_t)values->size() : 0;
    col->n_rows = n;
    col->null_mask.resize(n, 0);
    col->setDenseBufferRef(values);
    QoreBufferElementType element_type = values->getElementType();
    bool all_valid = true;
    const uint8_t* validity = nullptr;
    size_t validity_bit_offset = 0;
    if (values->hasNullableElements()) {
        all_valid = values->countValid(xsink) == values->size();
        if (*xsink) {
            return nullptr;
        }
        if (!all_valid) {
            validity = values->getRawValidityData();
            validity_bit_offset = values->getRawValidityBitOffset();
        }
    }

    switch (element_type) {
        case QoreBufferElementType::Int8:
        case QoreBufferElementType::Int16:
        case QoreBufferElementType::Int32:
        case QoreBufferElementType::Int64: {
            col->type = ColumnType::INT64;
            col->int_data.resize(n);
            const void* raw_data = values->getRawData();
            if (checkRawBufferData(raw_data, n, element_type, xsink)) {
                return nullptr;
            }
            if (all_valid) {
                switch (element_type) {
                    case QoreBufferElementType::Int8: {
                        const int8_t* src = static_cast<const int8_t*>(raw_data);
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int16: {
                        const int16_t* src = static_cast<const int16_t*>(raw_data);
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int32: {
                        const int32_t* src = static_cast<const int32_t*>(raw_data);
                        for (int64_t i = 0; i < n; ++i) {
                            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                                return nullptr;
                            }
                            col->int_data[i] = src[i];
                        }
                        break;
                    }
                    case QoreBufferElementType::Int64: {
                        const int64_t* src = static_cast<const int64_t*>(raw_data);
                        if (n) {
                            std::memcpy(col->int_data.data(), src, n * sizeof(int64_t));
                        }
                        break;
                    }
                    default:
                        break;
                }
            } else if (element_type == QoreBufferElementType::Int64) {
                const int64_t* src = static_cast<const int64_t*>(raw_data);
                if (n) {
                    std::memcpy(col->int_data.data(), src, n * sizeof(int64_t));
                }
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (!getBitmapBit(validity, validity_bit_offset, i)) {
                        col->null_mask[i] = 1;
                        col->int_data[i] = 0;
                    }
                }
            } else {
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (!getBitmapBit(validity, validity_bit_offset, i)) {
                        col->null_mask[i] = 1;
                        col->int_data[i] = 0;
                    } else {
                        switch (element_type) {
                            case QoreBufferElementType::Int8:
                                col->int_data[i] = static_cast<const int8_t*>(raw_data)[i];
                                break;
                            case QoreBufferElementType::Int16:
                                col->int_data[i] = static_cast<const int16_t*>(raw_data)[i];
                                break;
                            case QoreBufferElementType::Int32:
                                col->int_data[i] = static_cast<const int32_t*>(raw_data)[i];
                                break;
                            case QoreBufferElementType::Int64:
                                col->int_data[i] = static_cast<const int64_t*>(raw_data)[i];
                                break;
                            default:
                                assert(false);
                        }
                    }
                }
            }
            break;
        }
        case QoreBufferElementType::Float32:
        case QoreBufferElementType::Float64: {
            col->type = ColumnType::FLOAT64;
            col->float_data.resize(n);
            const void* raw_data = values->getRawData();
            if (checkRawBufferData(raw_data, n, element_type, xsink)) {
                return nullptr;
            }
            if (all_valid) {
                if (element_type == QoreBufferElementType::Float64) {
                    const double* src = static_cast<const double*>(raw_data);
                    if (n) {
                        std::memcpy(col->float_data.data(), src, n * sizeof(double));
                    }
                } else {
                    const float* src = static_cast<const float*>(raw_data);
                    for (int64_t i = 0; i < n; ++i) {
                        if (i && !(i % 100)
                                && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                            return nullptr;
                        }
                        col->float_data(i) = src[i];
                    }
                }
            } else if (element_type == QoreBufferElementType::Float64) {
                const double* src = static_cast<const double*>(raw_data);
                if (n) {
                    std::memcpy(col->float_data.data(), src, n * sizeof(double));
                }
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (!getBitmapBit(validity, validity_bit_offset, i)) {
                        col->null_mask[i] = 1;
                        col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            } else {
                const float* src = static_cast<const float*>(raw_data);
                for (int64_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                        return nullptr;
                    }
                    if (!getBitmapBit(validity, validity_bit_offset, i)) {
                        col->null_mask[i] = 1;
                        col->float_data(i) = std::numeric_limits<double>::quiet_NaN();
                    } else {
                        col->float_data(i) = src[i];
                    }
                }
            }
            break;
        }
        case QoreBufferElementType::Bool: {
            col->type = ColumnType::BOOL;
            col->bool_data.resize(n);
            const uint8_t* data = static_cast<const uint8_t*>(values->getRawData());
            if (checkRawBufferData(data, n, element_type, xsink)) {
                return nullptr;
            }
            size_t data_bit_offset = values->getRawDataBitOffset();
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                    return nullptr;
                }
                if (!all_valid && !getBitmapBit(validity, validity_bit_offset, i)) {
                    col->null_mask[i] = 1;
                    col->bool_data[i] = 0;
                } else {
                    col->bool_data[i] = getBitmapBit(data, data_bit_offset, i) ? 1 : 0;
                }
            }
            break;
        }
        case QoreBufferElementType::String: {
            col->type = ColumnType::STRING;
            col->str_data.resize(n);
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame column from buffer")) {
                    return nullptr;
                }
                if (values->isElementNull(i)) {
                    col->null_mask[i] = 1;
                } else {
                    ValueHolder value(values->getReferencedEntry(i, xsink), xsink);
                    if (value->getType() != NT_STRING) {
                        xsink->raiseException("DATAFRAME-ERROR",
                            "buffer<string> element " QLLD " could not be converted to a DataFrame string", (int64)i);
                        return nullptr;
                    }
                    if (dataFrameUtf8String(*value, col->str_data[i], xsink)) {
                        return nullptr;
                    }
                }
            }
            break;
        }
        case QoreBufferElementType::Decimal128: {
            col->type = ColumnType::AUTO;
            col->auto_data.resize(n);
            col->columnar_schema.kind = QoreColumnarTypeKind::Decimal128;
            col->columnar_schema.column_type = QoreColumnarColumnType::Number;
            col->columnar_schema.buffer_type = QoreBufferElementType::Decimal128;
            col->columnar_schema.nullable = values->hasNullableElements();
            col->columnar_schema.precision = values->getDecimalPrecision();
            col->columnar_schema.scale = values->getDecimalScale();
            col->has_columnar_schema = true;
            for (int64_t i = 0; i < n; ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame decimal column from buffer")) {
                    return nullptr;
                }
                if (values->isElementNull(i)) {
                    col->null_mask[i] = 1;
                } else {
                    ValueHolder value(values->getReferencedEntry(i, xsink), xsink);
                    col->setAutoValue(i, *value, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }
            }
            break;
        }
        default:
            xsink->raiseException("DATAFRAME-ERROR",
                "unsupported buffer element type '%s' for DataFrame column",
                qore_buffer_element_type_name(values->getElementType()));
            return nullptr;
    }

    return col;
}

std::shared_ptr<ColumnData> buildColumnDataFromDenseRows(const ColumnData& src, const QoreBufferNode* values,
        ExceptionSink* xsink) {
    return src.type == ColumnType::DATE
        ? buildColumnDataFromTemporalBuffer(values, xsink)
        : buildColumnDataFromBuffer(values, xsink);
}

std::shared_ptr<ColumnData> buildColumnDataFromTemporalBuffer(const QoreBufferNode* values,
        ExceptionSink* xsink) {
    if (!values) {
        return std::make_shared<ColumnData>();
    }
    if (values->getElementType() != QoreBufferElementType::Int64) {
        xsink->raiseException("DATAFRAME-ERROR",
            "dense temporal ColumnarResult columns require buffer<int64> storage; got buffer<%s>",
            qore_buffer_element_type_name(values->getElementType()));
        return nullptr;
    }
    if (values->ensureHostStorage(xsink)) {
        return nullptr;
    }

    auto col = std::make_shared<ColumnData>();
    int64_t n = static_cast<int64_t>(values->size());
    col->type = ColumnType::DATE;
    col->n_rows = n;
    col->date_data.resize(n);
    col->null_mask.resize(n, 0);
    col->setDenseBufferRef(values);

    const int64_t* raw = static_cast<const int64_t*>(values->getRawData());
    if (checkRawBufferData(raw, n, values->getElementType(), xsink)) {
        return nullptr;
    }
    if (n) {
        std::memcpy(col->date_data.data(), raw, n * sizeof(int64_t));
    }
    if (values->hasNullableElements()) {
        for (int64_t i = 0; i < n; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "building DataFrame temporal column from buffer")) {
                return nullptr;
            }
            if (values->isElementNull(i)) {
                col->null_mask[i] = 1;
                col->date_data[i] = 0;
            }
        }
    }
    return col;
}

QoreListNode* columnToQoreList(const ColumnData& col, ExceptionSink* xsink) {
    if (col.hasDenseBuffer() && col.type != ColumnType::DATE) {
        return col.dense_buffer->toList(xsink);
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int64_t i = 0; i < col.n_rows; ++i) {
        if (col.isNull(i)) {
            list->push(QoreValue(), xsink);
        } else {
            switch (col.type) {
                case ColumnType::FLOAT64:
                    list->push(col.float_data(i), xsink);
                    break;
                case ColumnType::INT64:
                    list->push(col.int_data[i], xsink);
                    break;
                case ColumnType::STRING:
                    list->push(new QoreStringNode(col.str_data[i], QCS_UTF8), xsink);
                    break;
                case ColumnType::BOOL:
                    list->push((bool)col.bool_data[i], xsink);
                    break;
                case ColumnType::DATE:
                    list->push(makeDateFromMicros(col.date_data[i], isDurationColumn(col)), xsink);
                    break;
                case ColumnType::AUTO:
                    assert(i < static_cast<int64_t>(col.auto_data.size()));
                    list->push(col.auto_data[i].refSelf(), xsink);
                    break;
                default:
                    list->push(QoreValue(), xsink);
                    break;
            }
        }
    }

    return list.release();
}

QoreListNode* columnToQoreListRange(const ColumnData& col, int64_t start,
        int64_t count, ExceptionSink* xsink) {
    if (start < 0 || count < 0 || start + count > col.n_rows) {
        xsink->raiseException("DATAFRAME-INDEX-ERROR",
            "column slice start " QLLD " count " QLLD " out of range for " QLLD " rows",
            (int64)start, (int64)count, (int64)col.n_rows);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    if (col.hasDenseBuffer() && col.type != ColumnType::DATE) {
        ReferenceHolder<QoreBufferNode> buffer(col.dense_buffer->slice(
            static_cast<size_t>(start), static_cast<size_t>(count), xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        return buffer->toList(xsink);
    }

    for (int64_t i = start; i < start + count; ++i) {
        if (i != start && !((i - start) % 100)
                && qore_check_cancel(xsink, "converting DataFrame column range to list")) {
            return nullptr;
        }
        if (col.isNull(i)) {
            list->push(QoreValue(), xsink);
        } else {
            switch (col.type) {
                case ColumnType::FLOAT64:
                    list->push(col.float_data(i), xsink);
                    break;
                case ColumnType::INT64:
                    list->push(col.int_data[i], xsink);
                    break;
                case ColumnType::STRING:
                    list->push(new QoreStringNode(col.str_data[i], QCS_UTF8), xsink);
                    break;
                case ColumnType::BOOL:
                    list->push((bool)col.bool_data[i], xsink);
                    break;
                case ColumnType::DATE:
                    list->push(makeDateFromMicros(col.date_data[i], isDurationColumn(col)), xsink);
                    break;
                case ColumnType::AUTO:
                    assert(i < static_cast<int64_t>(col.auto_data.size()));
                    list->push(col.auto_data[i].refSelf(), xsink);
                    break;
                default:
                    list->push(QoreValue(), xsink);
                    break;
            }
        }
    }

    return list.release();
}

QoreBufferNode* columnToQoreBuffer(const ColumnData& col, ExceptionSink* xsink) {
    if (col.hasDenseBuffer()) {
        return col.refDenseBuffer();
    }

    QoreBufferElementType element_type = QoreBufferElementType::Invalid;
    switch (col.type) {
        case ColumnType::FLOAT64:
            element_type = QoreBufferElementType::Float64;
            break;
        case ColumnType::INT64:
            element_type = QoreBufferElementType::Int64;
            break;
        case ColumnType::BOOL:
            element_type = QoreBufferElementType::Bool;
            break;
        case ColumnType::STRING:
            element_type = QoreBufferElementType::String;
            break;
        case ColumnType::DATE:
            element_type = QoreBufferElementType::Int64;
            break;
        default:
            xsink->raiseException("DATAFRAME-COLUMN-ERROR",
                "column type '%s' cannot be represented as a dense buffer",
                columnTypeName(col.type));
            return nullptr;
    }

    if (element_type == QoreBufferElementType::String) {
        ReferenceHolder<QoreListNode> values(columnToQoreList(col, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        return new QoreBufferNode(element_type, col.countNull() > 0, *values, xsink);
    }

    ReferenceHolder<QoreBufferNode> buffer(
        new QoreBufferNode(element_type, col.countNull() > 0, static_cast<size_t>(col.n_rows)), xsink);

    for (int64_t i = 0; i < col.n_rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "building dense DataFrame column buffer")) {
            return nullptr;
        }
        if (col.isNull(i)) {
            if (buffer->setEntry(static_cast<size_t>(i), QoreValue(), xsink)) {
                return nullptr;
            }
            continue;
        }

        QoreValue value;
        switch (col.type) {
            case ColumnType::FLOAT64:
                value = QoreValue(col.float_data(i));
                break;
            case ColumnType::INT64:
                value = QoreValue(col.int_data[i]);
                break;
            case ColumnType::DATE:
                value = QoreValue(col.date_data[i]);
                break;
            case ColumnType::BOOL:
                value = QoreValue(static_cast<bool>(col.bool_data[i]));
                break;
            case ColumnType::STRING:
                value = QoreValue::makeStringValue(col.str_data[i], QCS_UTF8);
                break;
            default:
                assert(false);
                break;
        }
        ValueHolder value_holder(value, xsink);
        if (buffer->setEntry(static_cast<size_t>(i), *value_holder, xsink)) {
            return nullptr;
        }
    }

    return buffer.release();
}

} // namespace QoreDataFrameNS
