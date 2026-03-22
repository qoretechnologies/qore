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

QoreHashNode* ProtobufHelper::messageToHash(const Message& msg, ExceptionSink* xsink) {
    const Descriptor* desc = msg.GetDescriptor();
    const Reflection* ref = msg.GetReflection();

    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);

    for (int i = 0; i < desc->field_count(); ++i) {
        const FieldDescriptor* field = desc->field(i);

        if (field->is_map()) {
            QoreHashNode* map_hash = mapFieldToHash(msg, field, ref, xsink);
            if (*xsink) {
                return nullptr;
            }
            hash->setKeyValue(std::string(field->name()), map_hash, xsink);
        } else if (field->is_repeated()) {
            QoreListNode* list = repeatedFieldToList(msg, field, ref, xsink);
            if (*xsink) {
                return nullptr;
            }
            hash->setKeyValue(std::string(field->name()), list, xsink);
        } else {
            QoreValue val = fieldToQore(msg, field, ref, xsink);
            if (*xsink) {
                return nullptr;
            }
            hash->setKeyValue(std::string(field->name()), val, xsink);
        }
    }

    return hash.release();
}

bool ProtobufHelper::hashToMessage(const QoreHashNode* hash, Message* msg, ExceptionSink* xsink) {
    if (!hash) {
        return true;
    }

    const Descriptor* desc = msg->GetDescriptor();
    const Reflection* ref = msg->GetReflection();

    ConstHashIterator hi(hash);
    while (hi.next()) {
        const char* key = hi.getKey();
        const FieldDescriptor* field = desc->FindFieldByName(key);

        if (!field) {
            // Try camelCase -> snake_case conversion
            field = desc->FindFieldByCamelcaseName(key);
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
        const Reflection* ref, ExceptionSink* xsink) {
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
            return new QoreStringNode(val);
        }
        case FieldDescriptor::CPPTYPE_ENUM:
            return (int64)ref->GetEnumValue(msg, field);
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            const Message& sub = ref->GetMessage(msg, field);
            return messageToHash(sub, xsink);
        }
        default:
            xsink->raiseException("PROTOBUF-TYPE-ERROR",
                "unsupported field type for field '%s'", std::string(field->name()).c_str());
            return QoreValue();
    }
}

QoreListNode* ProtobufHelper::repeatedFieldToList(const Message& msg,
        const FieldDescriptor* field, const Reflection* ref, ExceptionSink* xsink) {
    int count = ref->FieldSize(msg, field);
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    for (int i = 0; i < count; ++i) {
        QoreValue val = repeatedElementToQore(msg, field, ref, i, xsink);
        if (*xsink) {
            return nullptr;
        }
        list->push(val, xsink);
    }

    return list.release();
}

QoreValue ProtobufHelper::repeatedElementToQore(const Message& msg,
        const FieldDescriptor* field, const Reflection* ref, int index, ExceptionSink* xsink) {
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
            return new QoreStringNode(val);
        }
        case FieldDescriptor::CPPTYPE_ENUM:
            return (int64)ref->GetRepeatedEnumValue(msg, field, index);
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            const Message& sub = ref->GetRepeatedMessage(msg, field, index);
            return messageToHash(sub, xsink);
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
                QoreStringValueHelper str(val);
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
                    QoreStringValueHelper str(val);
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
        const Reflection* ref, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);

    int count = ref->FieldSize(msg, field);
    const Descriptor* entry_desc = field->message_type();
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
        QoreValue val = fieldToQore(entry, value_field, entry_ref, xsink);
        if (*xsink) {
            return nullptr;
        }

        hash->setKeyValue(key_str, val, xsink);
    }

    return hash.release();
}

bool ProtobufHelper::setMapFieldFromHash(Message* msg, const FieldDescriptor* field,
        const Reflection* ref, const QoreHashNode* hash, ExceptionSink* xsink) {
    const Descriptor* entry_desc = field->message_type();
    const FieldDescriptor* key_field = entry_desc->map_key();
    const FieldDescriptor* value_field = entry_desc->map_value();

    ConstHashIterator hi(hash);
    while (hi.next()) {
        Message* entry = ref->AddMessage(msg, field);
        const Reflection* entry_ref = entry->GetReflection();

        // Set key
        const char* key = hi.getKey();
        switch (key_field->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING:
                entry_ref->SetString(entry, key_field, std::string(key));
                break;
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
