/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file ProtobufHelper.cpp protobuf <-> Qore hash conversion implementation */
/*
    Qore protobuf module

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
*/

#include "ProtobufHelper.h"

using google::protobuf::Message;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Reflection;

namespace {
// the number of message levels being converted in this thread: the conversion recurses with the nesting of messages
thread_local int nesting_depth = 0;

//! counts a message level of a conversion, and checks the nesting depth and the thread's stack
class NestingGuard {
public:
    DLLLOCAL NestingGuard(const Descriptor* desc, ExceptionSink* xsink) {
        // the top-level message is level 0
        if (++nesting_depth > ProtobufHelper::MaxNesting + 1) {
            xsink->raiseException("PROTOBUF-DEPTH-ERROR", "message type '%s' is nested more than %d levels deep; "
                "deeper messages cannot be converted", std::string(desc->full_name()).c_str(),
                ProtobufHelper::MaxNesting);
            return;
        }
        if (q_check_stack(xsink)) {
            return;
        }
        ok = true;
    }

    DLLLOCAL ~NestingGuard() {
        --nesting_depth;
    }

    DLLLOCAL explicit operator bool() const {
        return ok;
    }

private:
    bool ok = false;
};
}

// returns the value of a string field as a UTF-8 string; raises INVALID-ENCODING if it is not valid UTF-8
/* A protobuf string is UTF-8 text: proto3 refuses a message with one that is not valid UTF-8, but proto2 does not
   check it (qore#6604); the text is checked where it enters the program
*/
static QoreStringNode* protobuf_string(const std::string& val, const FieldDescriptor* field, const char* what,
        const char* where, ExceptionSink* xsink) {
    if (q_get_invalid_text_offset(QCS_UTF8, val.data(), val.size()) >= 0) {
        QoreString desc;
        desc.sprintf("%s '%s'", what, std::string(field->full_name()).c_str());
        q_check_text_encoding(QCS_UTF8, val.data(), val.size(), where, desc.c_str(), xsink);
        return nullptr;
    }
    return new QoreStringNode(val.data(), val.size(), QCS_UTF8);
}

QoreHashNode* ProtobufHelper::messageToHash(const Message& msg, const char* where, ExceptionSink* xsink) {
    const Descriptor* desc = msg.GetDescriptor();
    const Reflection* ref = msg.GetReflection();

    NestingGuard guard(desc, xsink);
    if (!guard) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);

    for (int i = 0; i < desc->field_count(); ++i) {
        const FieldDescriptor* field = desc->field(i);

        // the holder releases a value returned with an exception
        ValueHolder val(xsink);
        if (field->is_map()) {
            val = mapFieldToHash(msg, field, ref, where, xsink);
        } else if (field->is_repeated()) {
            val = repeatedFieldToList(msg, field, ref, where, xsink);
        } else {
            val = fieldToQore(msg, field, ref, where, xsink);
        }
        if (*xsink || hash->setKeyValue(std::string(field->name()), val.release(), xsink)) {
            return nullptr;
        }
    }

    // the extension fields that are set in the message, which are not fields of its descriptor; they are given with
    // their full names in square brackets, as in protobuf's JSON and text formats
    if (desc->extension_range_count()) {
        std::vector<const FieldDescriptor*> fields;
        ref->ListFields(msg, &fields);
        for (const FieldDescriptor* field : fields) {
            if (!field->is_extension()) {
                continue;
            }
            ValueHolder val(xsink);
            if (field->is_repeated()) {
                val = repeatedFieldToList(msg, field, ref, where, xsink);
            } else {
                val = fieldToQore(msg, field, ref, where, xsink);
            }
            if (*xsink || hash->setKeyValue(extensionKey(field), val.release(), xsink)) {
                return nullptr;
            }
        }
    }

    return hash.release();
}

const FieldDescriptor* ProtobufHelper::findSetExtension(const Message& msg) {
    const Reflection* ref = msg.GetReflection();
    std::vector<const FieldDescriptor*> fields;
    ref->ListFields(msg, &fields);
    for (const FieldDescriptor* field : fields) {
        if (field->is_extension()) {
            return field;
        }
        if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) {
            continue;
        }
        // the nesting of the message was bounded when it was made from a hash
        if (field->is_repeated()) {
            for (int i = 0, e = ref->FieldSize(msg, field); i < e; ++i) {
                if (const FieldDescriptor* rv = findSetExtension(ref->GetRepeatedMessage(msg, field, i))) {
                    return rv;
                }
            }
        } else if (const FieldDescriptor* rv = findSetExtension(ref->GetMessage(msg, field))) {
            return rv;
        }
    }
    return nullptr;
}

std::string ProtobufHelper::extensionKey(const FieldDescriptor* field) {
    return std::string("[") + std::string(field->full_name()) + "]";
}

const FieldDescriptor* ProtobufHelper::findExtension(const Descriptor* desc, const char* key) {
    if (!desc->extension_range_count()) {
        return nullptr;
    }
    // "[full.name]", or the full name alone
    std::string name(key);
    if (name.size() > 2 && name.front() == '[' && name.back() == ']') {
        name = name.substr(1, name.size() - 2);
    }
    const FieldDescriptor* field = desc->file()->pool()->FindExtensionByName(name);
    return field && field->containing_type() == desc ? field : nullptr;
}

bool ProtobufHelper::hashToMessage(const QoreHashNode* hash, Message* msg, ExceptionSink* xsink) {
    if (!hash) {
        return true;
    }

    const Descriptor* desc = msg->GetDescriptor();
    const Reflection* ref = msg->GetReflection();

    NestingGuard guard(desc, xsink);
    if (!guard) {
        return false;
    }

    ConstHashIterator hi(hash);
    while (hi.next()) {
        const char* key = hi.getKey();
        const FieldDescriptor* field = desc->FindFieldByName(key);

        if (!field) {
            // Try camelCase -> snake_case conversion
            field = desc->FindFieldByCamelcaseName(key);
        }

        if (!field) {
            // an extension field of the message type
            field = findExtension(desc, key);
        }

        if (!field) {
            xsink->raiseException("PROTOBUF-FIELD-ERROR",
                "unknown field '%s' in message type '%s'", key, std::string(desc->full_name()).c_str());
            return false;
        }

        QoreValue val = hi.get();

        if (field->is_map()) {
            const QoreHashNode* map_hash = val.get<QoreHashNode>();
            if (!map_hash) {
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "expected hash for map field '%s', got %s", key,
                    val.getFullTypeName());
                return false;
            }
            if (!setMapFieldFromHash(msg, field, ref, map_hash, xsink)) {
                return false;
            }
        } else if (field->is_repeated()) {
            const QoreListNode* list = val.get<QoreListNode>();
            if (!list) {
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "expected list for repeated field '%s', got %s", key,
                    val.getFullTypeName());
                return false;
            }
            if (!setRepeatedFieldFromList(msg, field, ref, list, xsink)) {
                return false;
            }
        } else {
            if (!setFieldFromQore(msg, field, ref, val, xsink)) {
                return false;
            }
        }
    }

    return true;
}

QoreValue ProtobufHelper::fieldToQore(const Message& msg, const FieldDescriptor* field,
        const Reflection* ref, const char* where, ExceptionSink* xsink) {
    switch (field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
            return (int64)ref->GetInt32(msg, field);
        case FieldDescriptor::CPPTYPE_INT64:
            return ref->GetInt64(msg, field);
        case FieldDescriptor::CPPTYPE_UINT32:
            return (int64)ref->GetUInt32(msg, field);
        case FieldDescriptor::CPPTYPE_UINT64:
            return (int64)ref->GetUInt64(msg, field);
        case FieldDescriptor::CPPTYPE_DOUBLE:
            return ref->GetDouble(msg, field);
        case FieldDescriptor::CPPTYPE_FLOAT:
            return (double)ref->GetFloat(msg, field);
        case FieldDescriptor::CPPTYPE_BOOL:
            return ref->GetBool(msg, field);
        case FieldDescriptor::CPPTYPE_STRING: {
            std::string scratch;
            const std::string& val = ref->GetStringReference(msg, field, &scratch);
            if (field->type() == FieldDescriptor::TYPE_BYTES) {
                {
                    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                    bin->append(val.data(), val.size());
                    return bin.release();
                }
            }
            return protobuf_string(val, field, "string field", where, xsink);
        }
        case FieldDescriptor::CPPTYPE_ENUM:
            return (int64)ref->GetEnumValue(msg, field);
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            // an unset message field has no value; its default instance is not converted, which for a message type
            // with a field of its own type would never end
            if (!ref->HasField(msg, field)) {
                return QoreValue();
            }
            const Message& sub = ref->GetMessage(msg, field);
            return messageToHash(sub, where, xsink);
        }
        default:
            xsink->raiseException("PROTOBUF-TYPE-ERROR",
                "unsupported field type for field '%s'", std::string(field->name()).c_str());
            return QoreValue();
    }
}

QoreListNode* ProtobufHelper::repeatedFieldToList(const Message& msg,
        const FieldDescriptor* field, const Reflection* ref, const char* where, ExceptionSink* xsink) {
    int count = ref->FieldSize(msg, field);
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int i = 0; i < count; ++i) {
        ValueHolder val(repeatedElementToQore(msg, field, ref, i, where, xsink), xsink);
        if (*xsink || list->push(val.release(), xsink)) {
            return nullptr;
        }
    }

    return list.release();
}

QoreValue ProtobufHelper::repeatedElementToQore(const Message& msg,
        const FieldDescriptor* field, const Reflection* ref, int index, const char* where, ExceptionSink* xsink) {
    switch (field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
            return (int64)ref->GetRepeatedInt32(msg, field, index);
        case FieldDescriptor::CPPTYPE_INT64:
            return ref->GetRepeatedInt64(msg, field, index);
        case FieldDescriptor::CPPTYPE_UINT32:
            return (int64)ref->GetRepeatedUInt32(msg, field, index);
        case FieldDescriptor::CPPTYPE_UINT64:
            return (int64)ref->GetRepeatedUInt64(msg, field, index);
        case FieldDescriptor::CPPTYPE_DOUBLE:
            return ref->GetRepeatedDouble(msg, field, index);
        case FieldDescriptor::CPPTYPE_FLOAT:
            return (double)ref->GetRepeatedFloat(msg, field, index);
        case FieldDescriptor::CPPTYPE_BOOL:
            return ref->GetRepeatedBool(msg, field, index);
        case FieldDescriptor::CPPTYPE_STRING: {
            std::string scratch;
            const std::string& val = ref->GetRepeatedStringReference(msg, field, index, &scratch);
            if (field->type() == FieldDescriptor::TYPE_BYTES) {
                {
                    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                    bin->append(val.data(), val.size());
                    return bin.release();
                }
            }
            return protobuf_string(val, field, "string field", where, xsink);
        }
        case FieldDescriptor::CPPTYPE_ENUM:
            return (int64)ref->GetRepeatedEnumValue(msg, field, index);
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            const Message& sub = ref->GetRepeatedMessage(msg, field, index);
            return messageToHash(sub, where, xsink);
        }
        default:
            xsink->raiseException("PROTOBUF-TYPE-ERROR",
                "unsupported repeated field type for field '%s'", std::string(field->name()).c_str());
            return QoreValue();
    }
}

bool ProtobufHelper::setFieldFromQore(Message* msg, const FieldDescriptor* field,
        const Reflection* ref, QoreValue val, ExceptionSink* xsink) {
    switch (field->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
            ref->SetInt32(msg, field, (int32_t)val.getAsBigInt());
            return true;
        case FieldDescriptor::CPPTYPE_INT64:
            ref->SetInt64(msg, field, val.getAsBigInt());
            return true;
        case FieldDescriptor::CPPTYPE_UINT32:
            ref->SetUInt32(msg, field, (uint32_t)val.getAsBigInt());
            return true;
        case FieldDescriptor::CPPTYPE_UINT64:
            ref->SetUInt64(msg, field, (uint64_t)val.getAsBigInt());
            return true;
        case FieldDescriptor::CPPTYPE_DOUBLE:
            ref->SetDouble(msg, field, val.getAsFloat());
            return true;
        case FieldDescriptor::CPPTYPE_FLOAT:
            ref->SetFloat(msg, field, (float)val.getAsFloat());
            return true;
        case FieldDescriptor::CPPTYPE_BOOL:
            ref->SetBool(msg, field, val.getAsBool());
            return true;
        case FieldDescriptor::CPPTYPE_STRING: {
            if (field->type() == FieldDescriptor::TYPE_BYTES) {
                const BinaryNode* bin = val.get<BinaryNode>();
                if (bin) {
                    ref->SetString(msg, field,
                        std::string(static_cast<const char*>(bin->getPtr()), bin->size()));
                } else {
                    ref->SetString(msg, field, std::string());
                }
            } else {
                // a protobuf string is UTF-8 text
                QoreStringValueHelper str(val, QCS_UTF8, xsink);
                if (*xsink) {
                    return false;
                }
                ref->SetString(msg, field, std::string(str->c_str(), str->size()));
            }
            return true;
        }
        case FieldDescriptor::CPPTYPE_ENUM:
            ref->SetEnumValue(msg, field, (int)val.getAsBigInt());
            return true;
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            const QoreHashNode* sub_hash = val.get<QoreHashNode>();
            if (!sub_hash) {
                if (val.isNothing() || val.isNull()) {
                    return true;
                }
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "expected hash for message field '%s', got %s",
                    std::string(field->name()).c_str(), val.getFullTypeName());
                return false;
            }
            Message* sub = ref->MutableMessage(msg, field);
            return hashToMessage(sub_hash, sub, xsink);
        }
        default:
            xsink->raiseException("PROTOBUF-TYPE-ERROR",
                "unsupported field type for field '%s'", std::string(field->name()).c_str());
            return false;
    }
}

bool ProtobufHelper::setRepeatedFieldFromList(Message* msg, const FieldDescriptor* field,
        const Reflection* ref, const QoreListNode* list, ExceptionSink* xsink) {
    for (size_t i = 0; i < list->size(); ++i) {
        QoreValue val = list->retrieveEntry(i);

        switch (field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_INT32:
                ref->AddInt32(msg, field, (int32_t)val.getAsBigInt());
                break;
            case FieldDescriptor::CPPTYPE_INT64:
                ref->AddInt64(msg, field, val.getAsBigInt());
                break;
            case FieldDescriptor::CPPTYPE_UINT32:
                ref->AddUInt32(msg, field, (uint32_t)val.getAsBigInt());
                break;
            case FieldDescriptor::CPPTYPE_UINT64:
                ref->AddUInt64(msg, field, (uint64_t)val.getAsBigInt());
                break;
            case FieldDescriptor::CPPTYPE_DOUBLE:
                ref->AddDouble(msg, field, val.getAsFloat());
                break;
            case FieldDescriptor::CPPTYPE_FLOAT:
                ref->AddFloat(msg, field, (float)val.getAsFloat());
                break;
            case FieldDescriptor::CPPTYPE_BOOL:
                ref->AddBool(msg, field, val.getAsBool());
                break;
            case FieldDescriptor::CPPTYPE_STRING: {
                if (field->type() == FieldDescriptor::TYPE_BYTES) {
                    const BinaryNode* bin = val.get<BinaryNode>();
                    if (bin) {
                        ref->AddString(msg, field,
                            std::string(static_cast<const char*>(bin->getPtr()), bin->size()));
                    } else {
                        ref->AddString(msg, field, std::string());
                    }
                } else {
                    // a protobuf string is UTF-8 text
                    QoreStringValueHelper str(val, QCS_UTF8, xsink);
                    if (*xsink) {
                        return false;
                    }
                    ref->AddString(msg, field, std::string(str->c_str(), str->size()));
                }
                break;
            }
            case FieldDescriptor::CPPTYPE_ENUM:
                ref->AddEnumValue(msg, field, (int)val.getAsBigInt());
                break;
            case FieldDescriptor::CPPTYPE_MESSAGE: {
                const QoreHashNode* sub_hash = val.get<QoreHashNode>();
                if (!sub_hash) {
                    xsink->raiseException("PROTOBUF-TYPE-ERROR",
                        "expected hash for repeated message field '%s' element %d, got %s",
                        std::string(field->name()).c_str(), (int)i, val.getFullTypeName());
                    return false;
                }
                Message* sub = ref->AddMessage(msg, field);
                if (!hashToMessage(sub_hash, sub, xsink)) {
                    return false;
                }
                break;
            }
            default:
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "unsupported repeated field type for field '%s'", std::string(field->name()).c_str());
                return false;
        }
    }

    return true;
}

QoreHashNode* ProtobufHelper::mapFieldToHash(const Message& msg, const FieldDescriptor* field,
        const Reflection* ref, const char* where, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);

    int count = ref->FieldSize(msg, field);
    if (!count) {
        return hash.release();
    }
    const Descriptor* entry_desc = field->message_type();
    // a map entry is a message level, as in protobuf's parsers
    NestingGuard guard(entry_desc, xsink);
    if (!guard) {
        return nullptr;
    }
    const FieldDescriptor* key_field = entry_desc->map_key();
    const FieldDescriptor* value_field = entry_desc->map_value();

    for (int i = 0; i < count; ++i) {
        const Message& entry = ref->GetRepeatedMessage(msg, field, i);
        const Reflection* entry_ref = entry.GetReflection();

        // Get key as string
        std::string key_str;
        switch (key_field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING:
                key_str = entry_ref->GetString(entry, key_field);
                // a string key is UTF-8 text, as a string field is
                if (q_get_invalid_text_offset(QCS_UTF8, key_str.data(), key_str.size()) >= 0) {
                    QoreString desc;
                    desc.sprintf("map key of field '%s'", std::string(field->full_name()).c_str());
                    q_check_text_encoding(QCS_UTF8, key_str.data(), key_str.size(), where, desc.c_str(), xsink);
                    return nullptr;
                }
                break;
            case FieldDescriptor::CPPTYPE_INT32:
                key_str = std::to_string(entry_ref->GetInt32(entry, key_field));
                break;
            case FieldDescriptor::CPPTYPE_INT64:
                key_str = std::to_string(entry_ref->GetInt64(entry, key_field));
                break;
            case FieldDescriptor::CPPTYPE_UINT32:
                key_str = std::to_string(entry_ref->GetUInt32(entry, key_field));
                break;
            case FieldDescriptor::CPPTYPE_UINT64:
                key_str = std::to_string(entry_ref->GetUInt64(entry, key_field));
                break;
            case FieldDescriptor::CPPTYPE_BOOL:
                key_str = entry_ref->GetBool(entry, key_field) ? "true" : "false";
                break;
            default:
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "unsupported map key type for field '%s'", std::string(field->name()).c_str());
                return nullptr;
        }

        // Get value
        ValueHolder val(fieldToQore(entry, value_field, entry_ref, where, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        // a string key with a NUL character cannot be a hash key, and cut at the NUL character, it would merge
        // entries, so the message cannot be decoded to a hash
        if (QoreHashNode::checkKey(key_str.data(), key_str.size(), "PROTOBUF-DECODE-ERROR", "protobuf map key",
                xsink)) {
            return nullptr;
        }
        // a hash key is in the default encoding: the UTF-8 key is converted to it, as parse_json() converts its keys;
        // with a UTF-8 default encoding, the key is used as it is
        if (QCS_DEFAULT == QCS_UTF8) {
            if (hash->setKeyValue(key_str.c_str(), val.release(), xsink) || *xsink) {
                return nullptr;
            }
            continue;
        }
        QoreString key(key_str.data(), key_str.size(), QCS_UTF8);
        if (hash->setKeyValue(key, val.release(), xsink) || *xsink) {
            return nullptr;
        }
    }

    return hash.release();
}

bool ProtobufHelper::setMapFieldFromHash(Message* msg, const FieldDescriptor* field,
        const Reflection* ref, const QoreHashNode* hash, ExceptionSink* xsink) {
    const Descriptor* entry_desc = field->message_type();
    const FieldDescriptor* key_field = entry_desc->map_key();
    const FieldDescriptor* value_field = entry_desc->map_value();
    if (hash->empty()) {
        return true;
    }
    // a map entry is a message level, as in protobuf's parsers
    NestingGuard guard(entry_desc, xsink);
    if (!guard) {
        return false;
    }

    ConstHashIterator hi(hash);
    while (hi.next()) {
        Message* entry = ref->AddMessage(msg, field);
        const Reflection* entry_ref = entry->GetReflection();

        // Set key
        const char* key = hi.getKey();
        switch (key_field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING: {
                // a hash key is in the default encoding, a protobuf string key is UTF-8 text; with a UTF-8 default
                // encoding, the key is used as it is
                if (QCS_DEFAULT == QCS_UTF8) {
                    entry_ref->SetString(entry, key_field, std::string(key));
                    break;
                }
                QoreString key_str(key, QCS_DEFAULT);
                TempEncodingHelper utf8_key(key_str, QCS_UTF8, xsink);
                if (!utf8_key) {
                    return false;
                }
                entry_ref->SetString(entry, key_field, std::string(utf8_key->c_str(), utf8_key->size()));
                break;
            }
            case FieldDescriptor::CPPTYPE_INT32:
                entry_ref->SetInt32(entry, key_field, atoi(key));
                break;
            case FieldDescriptor::CPPTYPE_INT64:
                entry_ref->SetInt64(entry, key_field, atoll(key));
                break;
            case FieldDescriptor::CPPTYPE_UINT32:
                entry_ref->SetUInt32(entry, key_field, (uint32_t)atoll(key));
                break;
            case FieldDescriptor::CPPTYPE_UINT64:
                entry_ref->SetUInt64(entry, key_field, (uint64_t)atoll(key));
                break;
            case FieldDescriptor::CPPTYPE_BOOL:
                entry_ref->SetBool(entry, key_field, strcmp(key, "true") == 0);
                break;
            default:
                xsink->raiseException("PROTOBUF-TYPE-ERROR",
                    "unsupported map key type for field '%s'", std::string(field->name()).c_str());
                return false;
        }

        // Set value
        if (!setFieldFromQore(entry, value_field, entry_ref, hi.get(), xsink)) {
            return false;
        }
    }

    return true;
}
