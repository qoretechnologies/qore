/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBinary.cpp

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

#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreIR.h"

#include <qore/Qore.h>
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_enum_decl_private.h"

#include <cassert>
#include <cstring>

// ---- QoreAOTBinaryWriter ----

bool QoreAOTBinaryWriter::writeValue(const QoreValue& v) {
    if (v.isNothing()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
        return true;
    }
    if (v.isNull()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NULL));
        return true;
    }

    qore_type_t t = v.getType();
    switch (t) {
        case NT_BOOLEAN: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_BOOL));
            writeU8(v.getAsBool() ? 1 : 0);
            return true;
        }
        case NT_INT: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_INT64));
            writeI64(v.getAsBigInt());
            return true;
        }
        case NT_FLOAT: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_FLOAT64));
            writeF64(v.getAsFloat());
            return true;
        }
        case NT_STRING: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_STRING));
            const QoreStringNode* str = v.get<const QoreStringNode>();
            if (str) {
                writeU32(static_cast<uint32_t>(str->size()));
                writeStringRef(str->c_str(), str->size());
            } else {
                writeU32(0);
                writeStringRef("", 0);
            }
            return true;
        }
        case NT_DATE: {
            const DateTimeNode* dt = v.get<const DateTimeNode>();
            if (!dt) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
                return true;
            }
            if (dt->isRelative()) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_REL_DATE));
                // For relative dates, store individual components
                writeI64(static_cast<int64_t>(dt->getYear()));
                writeI64(static_cast<int64_t>(dt->getMonth()));
                writeI64(static_cast<int64_t>(dt->getDay()));
                writeI64(static_cast<int64_t>(dt->getHour()));
                writeI64(static_cast<int64_t>(dt->getMinute()));
                writeI64(static_cast<int64_t>(dt->getSecond()));
                writeI64(static_cast<int64_t>(dt->getMicrosecond()));
            } else {
                const AbstractQoreZoneInfo* zone = dt->getZone();
                const char* region = zone ? zone->getRegionName() : nullptr;
                // Use region name for DST-aware zones (e.g., "Europe/Paris")
                // Offset zones have names like "+01:00", "-06:00" — use fixed offset for those
                if (region && region[0] != '+' && region[0] != '-' && strcmp(region, "UTC") != 0) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ABS_DATE_REGION));
                    writeI64(dt->getEpochMicrosecondsUTC());
                    // Write region name as length-prefixed string in string pool
                    uint32_t len = static_cast<uint32_t>(strlen(region));
                    writeU32(len);
                    writeStringRef(region, len);
                } else {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ABS_DATE));
                    // For absolute dates, store epoch microseconds UTC + zone offset
                    writeI64(dt->getEpochMicrosecondsUTC());
                    // Store UTC offset in seconds for zone reconstruction
                    int utc_offset = 0;
                    if (zone) {
                        utc_offset = AbstractQoreZoneInfo::getUTCOffset(zone);
                    }
                    writeI64(static_cast<int64_t>(utc_offset));
                }
            }
            return true;
        }
        case NT_NUMBER: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NUMBER));
            const QoreNumberNode* num = v.get<const QoreNumberNode>();
            if (num) {
                // Serialize as string representation for portability
                QoreString str;
                num->toString(str, QORE_NF_RAW);
                writeU32(static_cast<uint32_t>(str.size()));
                writeStringRef(str.c_str(), str.size());
            } else {
                writeU32(0);
                writeStringRef("0", 1);
            }
            return true;
        }
        case NT_BINARY: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_BINARY));
            const BinaryNode* bin = v.get<const BinaryNode>();
            if (bin && bin->size() > 0) {
                writeU32(static_cast<uint32_t>(bin->size()));
                writeBytes(bin->getPtr(), static_cast<uint32_t>(bin->size()));
            } else {
                writeU32(0);
            }
            return true;
        }
        case NT_LIST: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_LIST));
            const QoreListNode* list = v.get<const QoreListNode>();
            if (list) {
                uint32_t count = static_cast<uint32_t>(list->size());
                writeU32(count);
                for (uint32_t i = 0; i < count; ++i) {
                    // Must not return false here - would leave partial data
                    // Unsupported element types become NOTHING
                    writeValue(list->retrieveEntry(i));
                }
            } else {
                writeU32(0);
            }
            return true;
        }
        case NT_HASH: {
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_HASH));
            const QoreHashNode* hash = v.get<const QoreHashNode>();
            if (hash) {
                uint32_t count = static_cast<uint32_t>(hash->size());
                writeU32(count);
                ConstHashIterator hi(*hash);
                while (hi.next()) {
                    const char* key = hi.getKey();
                    writeStringRef(key);
                    // Must not return false here - would leave partial data
                    // Unsupported value types become NOTHING
                    writeValue(hi.get());
                }
            } else {
                writeU32(0);
            }
            return true;
        }
        default:
            // Unsupported value type - write NOTHING instead of failing
            // This preserves binary structure integrity for container types
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
            return true;
    }
}

bool QoreAOTBinaryWriter::finalize(const QoreAOTBinaryHeader& in_header, std::vector<uint8_t>& output) {
    // Make a mutable copy so we can fill in section count
    QoreAOTBinaryHeader header = in_header;
    header.section_count = static_cast<uint32_t>(sections.size());

    // Fixed header size (60 bytes)
    uint32_t header_size = QORE_AOT_HEADER_SIZE;
    uint32_t section_dir_size = static_cast<uint32_t>(sections.size() * sizeof(QoreAOTSectionHeader));
    uint32_t string_pool_size = strings.size();
    uint32_t data_size = static_cast<uint32_t>(buffer.size());
    uint32_t total = header_size + section_dir_size + 4 /* string pool size */ + string_pool_size + data_size;

    output.clear();
    output.reserve(total);

    // Write header (60 bytes, single flat format)
    auto writeU8LE = [&](uint8_t v) {
        output.push_back(v);
    };
    auto writeU16LE = [&](uint16_t v) {
        output.push_back(static_cast<uint8_t>(v & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto writeU32LE = [&](uint32_t v) {
        output.push_back(static_cast<uint8_t>(v & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto writeI64LE = [&](int64_t v) {
        uint64_t uv;
        memcpy(&uv, &v, sizeof(uv));
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFF));
        }
    };
    auto writeU64LE = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    };

    // Bytes 0-3: magic
    writeU32LE(header.magic);
    // Bytes 4-5: version
    writeU16LE(header.version);
    // Bytes 6-7: flags
    writeU16LE(header.flags);
    // Bytes 8-15: parse_options_lo
    writeI64LE(header.parse_options_lo);
    // Bytes 16-19: section_count
    writeU32LE(header.section_count);
    // Bytes 20-23: label_offset
    writeU32LE(header.label_offset);
    // Bytes 24-27: label_length
    writeU32LE(header.label_length);
    // Bytes 28-29: max_opcode_id
    writeU16LE(header.max_opcode_id);
    // Bytes 30: qore_version_major
    writeU8LE(header.qore_version_major);
    // Bytes 31: qore_version_minor
    writeU8LE(header.qore_version_minor);
    // Bytes 32-33: qore_version_patch
    writeU16LE(header.qore_version_patch);
    // Bytes 34-35: reserved
    writeU16LE(header.reserved);
    // Bytes 36-43: parse_options_hi
    writeI64LE(header.parse_options_hi);
    // Bytes 44-51: source_hash
    writeU64LE(header.source_hash);
    // Bytes 52-59: feature_flags
    writeU64LE(header.feature_flags);

    // Write section directory
    for (auto& sec : sections) {
        writeU16LE(sec.type);
        writeU16LE(sec.reserved);
        writeU32LE(sec.offset);
        writeU32LE(sec.size);
    }

    // Write string pool (preceded by its size)
    writeU32LE(string_pool_size);
    const auto& pool_data = strings.getData();
    output.insert(output.end(), pool_data.begin(), pool_data.end());

    // Write data area
    output.insert(output.end(), buffer.begin(), buffer.end());

    return true;
}

// ---- QoreAOTBinaryReader ----

bool QoreAOTBinaryReader::open(const uint8_t* in_data, uint32_t in_size, std::string& error) {
    data = in_data;
    total_size = in_size;

    // Fixed header size (60 bytes)
    const uint32_t header_size = QORE_AOT_HEADER_SIZE;

    // Check full header fits
    if (in_size < header_size) {
        error = "binary too small for header (" + std::to_string(in_size) + " < " + std::to_string(header_size) + ")";
        return false;
    }

    // Read header (60 bytes, single flat format)
    const uint8_t* ptr = data;
    header.magic = readU32(ptr);
    header.version = readU16(ptr);
    header.flags = readU16(ptr);
    header.parse_options_lo = readI64(ptr);
    header.section_count = readU32(ptr);
    header.label_offset = readU32(ptr);
    header.label_length = readU32(ptr);
    header.max_opcode_id = readU16(ptr);
    header.qore_version_major = readU8(ptr);
    header.qore_version_minor = readU8(ptr);
    header.qore_version_patch = readU16(ptr);
    header.reserved = readU16(ptr);
    header.parse_options_hi = readI64(ptr);

    // Read new v1 fields: source_hash (8) and feature_flags (8)
    uint64_t source_hash = 0;
    uint64_t feature_flags = 0;
    {
        uint64_t temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        source_hash = temp;
        ptr += 8;
        temp = 0;
        for (int i = 0; i < 8; ++i) {
            temp |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        feature_flags = temp;
        ptr += 8;
    }
    header.source_hash = source_hash;
    header.feature_flags = feature_flags;

    // Validate magic
    if (header.magic != QORE_AOT_BINARY_MAGIC) {
        error = "invalid magic number (expected QORD)";
        return false;
    }

    // Validate version (must be exactly 1)
    if (header.version != QORE_AOT_BINARY_VERSION) {
        error = "unsupported binary format version " + std::to_string(header.version)
              + " (expected " + std::to_string(QORE_AOT_BINARY_VERSION) + ")"
              + "; please update your Qore installation";
        return false;
    }

    // Validate opcode compatibility
    if (header.max_opcode_id > QORE_IR_MAX_OPCODE) {
        error = "binary was compiled with Qore "
              + std::to_string(header.qore_version_major) + "."
              + std::to_string(header.qore_version_minor) + "."
              + std::to_string(header.qore_version_patch)
              + " (max opcode ID " + std::to_string(header.max_opcode_id) + ")"
              + " but this runtime only supports up to opcode ID "
              + std::to_string(QORE_IR_MAX_OPCODE)
              + "; please update your Qore installation";
        return false;
    }

    // Read section directory
    uint32_t section_dir_size = header.section_count * sizeof(QoreAOTSectionHeader);
    uint32_t needed = header_size + section_dir_size;
    if (in_size < needed) {
        error = "binary too small for section directory";
        return false;
    }

    sections.resize(header.section_count);
    for (uint32_t i = 0; i < header.section_count; ++i) {
        sections[i].type = readU16(ptr);
        sections[i].reserved = readU16(ptr);
        sections[i].offset = readU32(ptr);
        sections[i].size = readU32(ptr);
    }

    // Read string pool size
    if (ptr + 4 > data + total_size) {
        error = "binary too small for string pool size";
        return false;
    }
    string_pool_size = readU32(ptr);

    // Validate string pool
    if (ptr + string_pool_size > data + total_size) {
        error = "binary too small for string pool data";
        return false;
    }
    string_pool = reinterpret_cast<const char*>(ptr);
    ptr += string_pool_size;

    // Remaining data is the data area
    data_area = ptr;
    data_area_size = static_cast<uint32_t>((data + total_size) - ptr);

    // Validate section offsets
    for (auto& sec : sections) {
        if (sec.offset + sec.size > data_area_size) {
            error = "section offset/size exceeds data area";
            return false;
        }
    }

    return true;
}

QoreValue QoreAOTBinaryReader::readValue(const uint8_t*& ptr, const uint8_t* end,
        std::string& error) const {
    if (ptr >= end) {
        error = "unexpected end of data reading value tag";
        return QoreValue();
    }

    QoreAOTValueTag tag = static_cast<QoreAOTValueTag>(readU8(ptr));
    switch (tag) {
        case QoreAOTValueTag::VT_NOTHING:
            return QoreValue();

        case QoreAOTValueTag::VT_NULL:
            return QoreValue(null());

        case QoreAOTValueTag::VT_BOOL: {
            if (ptr >= end) {
                error = "unexpected end of data reading bool value";
                return QoreValue();
            }
            uint8_t b = readU8(ptr);
            return QoreValue(b != 0);
        }

        case QoreAOTValueTag::VT_INT64: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading int64 value";
                return QoreValue();
            }
            int64_t val = readI64(ptr);
            return QoreValue(val);
        }

        case QoreAOTValueTag::VT_FLOAT64: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading float64 value";
                return QoreValue();
            }
            double val = readF64(ptr);
            return QoreValue(val);
        }

        case QoreAOTValueTag::VT_STRING: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading string value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            uint32_t str_offset = readU32(ptr);
            const char* str = getString(str_offset);
            if (!str) {
                error = "invalid string offset in value";
                return QoreValue();
            }
            return QoreValue(new QoreStringNode(str, len, QCS_UTF8));
        }

        case QoreAOTValueTag::VT_ABS_DATE: {
            if (ptr + 16 > end) {
                error = "unexpected end of data reading abs_date value";
                return QoreValue();
            }
            int64_t epoch_us = readI64(ptr);
            int64_t utc_offset = readI64(ptr);
            // Reconstruct zone from UTC offset
            const AbstractQoreZoneInfo* zone = nullptr;
            if (utc_offset != 0) {
                zone = findCreateOffsetZone(static_cast<int>(utc_offset));
            }
            // Convert epoch_us to seconds + microseconds
            int64_t epoch_s = epoch_us / 1000000;
            int us = static_cast<int>(epoch_us % 1000000);
            if (us < 0) {
                // Handle negative microseconds (dates before epoch)
                epoch_s -= 1;
                us += 1000000;
            }
            return QoreValue(DateTimeNode::makeAbsolute(zone, epoch_s, us));
        }

        case QoreAOTValueTag::VT_ABS_DATE_REGION: {
            if (ptr + 12 > end) {
                error = "unexpected end of data reading abs_date_region value";
                return QoreValue();
            }
            int64_t epoch_us = readI64(ptr);
            uint32_t name_len = readU32(ptr);
            if (ptr + 4 > end) {
                error = "unexpected end of data reading region name offset";
                return QoreValue();
            }
            // Read string pool offset (writeStringRef writes a pool offset)
            uint32_t str_offset = readU32(ptr);
            const char* region_name = getString(str_offset);
            if (!region_name) {
                error = "invalid string offset for region name";
                return QoreValue();
            }
            // Look up region zone
            ExceptionSink xsink;
            const AbstractQoreZoneInfo* zone = QTZM.findLoadRegion(region_name, &xsink);
            if (!zone || xsink) {
                // Fallback to UTC if region not found
                xsink.clear();
                zone = nullptr;
            }
            // Convert epoch_us to seconds + microseconds
            int64_t epoch_s = epoch_us / 1000000;
            int us = static_cast<int>(epoch_us % 1000000);
            if (us < 0) {
                epoch_s -= 1;
                us += 1000000;
            }
            return QoreValue(DateTimeNode::makeAbsolute(zone, epoch_s, us));
        }

        case QoreAOTValueTag::VT_REL_DATE: {
            if (ptr + 56 > end) {
                error = "unexpected end of data reading rel_date value";
                return QoreValue();
            }
            int year = static_cast<int>(readI64(ptr));
            int month = static_cast<int>(readI64(ptr));
            int day = static_cast<int>(readI64(ptr));
            int hour = static_cast<int>(readI64(ptr));
            int minute = static_cast<int>(readI64(ptr));
            int second = static_cast<int>(readI64(ptr));
            int us = static_cast<int>(readI64(ptr));
            return QoreValue(DateTimeNode::makeRelative(year, month, day, hour, minute, second, us));
        }

        case QoreAOTValueTag::VT_NUMBER: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading number value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            uint32_t str_offset = readU32(ptr);
            const char* str = getString(str_offset);
            if (!str) {
                error = "invalid string offset in number value";
                return QoreValue();
            }
            return QoreValue(new QoreNumberNode(str));
        }

        case QoreAOTValueTag::VT_BINARY: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading binary value";
                return QoreValue();
            }
            uint32_t len = readU32(ptr);
            if (len == 0) {
                return QoreValue(new BinaryNode());
            }
            if (ptr + len > end) {
                error = "unexpected end of data reading binary payload";
                return QoreValue();
            }
            void* buf = malloc(len);
            if (!buf) {
                error = "out of memory allocating binary value";
                return QoreValue();
            }
            memcpy(buf, ptr, len);
            ptr += len;
            return QoreValue(new BinaryNode(buf, len));
        }

        case QoreAOTValueTag::VT_LIST: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading list count";
                return QoreValue();
            }
            uint32_t count = readU32(ptr);
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
            for (uint32_t i = 0; i < count; ++i) {
                QoreValue elem = readValue(ptr, end, error);
                if (!error.empty()) {
                    return QoreValue();
                }
                list->push(elem, nullptr);
            }
            return QoreValue(list.release());
        }

        case QoreAOTValueTag::VT_HASH: {
            if (ptr + 4 > end) {
                error = "unexpected end of data reading hash count";
                return QoreValue();
            }
            uint32_t count = readU32(ptr);
            ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), nullptr);
            for (uint32_t i = 0; i < count; ++i) {
                if (ptr + 4 > end) {
                    error = "unexpected end of data reading hash key";
                    return QoreValue();
                }
                uint32_t key_offset = readU32(ptr);
                const char* key = getString(key_offset);
                if (!key) {
                    error = "invalid string offset for hash key";
                    return QoreValue();
                }
                QoreValue val = readValue(ptr, end, error);
                if (!error.empty()) {
                    return QoreValue();
                }
                hash->setKeyValue(key, val, nullptr);
            }
            return QoreValue(hash.release());
        }

        case QoreAOTValueTag::VT_OPAQUE_DEFAULT:
            // Complex expression default (e.g. function call) that couldn't be
            // serialized. Return boolean True as a placeholder to mark the parameter
            // as optional in the function signature. The actual default is evaluated
            // by the compiled function code at runtime.
            return QoreValue(true);

        default:
            error = "unknown value tag: " + std::to_string(static_cast<int>(tag))
                + " at offset " + std::to_string(ptr - 1 - end);
            return QoreValue();
    }
}

// ---- QoreAOTTypeResolver ----

//! Static lookup table for builtin type path strings → QoreTypeInfo*
/** This provides a fast path for the most common type resolutions.
    The map keys are the strings returned by QoreTypeInfo::getPath() for builtin types.
*/
struct BuiltinTypeEntry {
    const char* name;
    const QoreTypeInfo** type_ptr;
};

// NOTE: this table must stay in sync with QoreTypeInfo type path strings
static const BuiltinTypeEntry builtin_types[] = {
    {"int",             &bigIntTypeInfo},
    {"float",           &floatTypeInfo},
    {"number",          &numberTypeInfo},
    {"string",          &stringTypeInfo},
    {"bool",            &boolTypeInfo},
    {"date",            &dateTypeInfo},
    {"binary",          &binaryTypeInfo},
    {"hash",            &hashTypeInfo},
    {"list",            &listTypeInfo},
    {"object",          &objectTypeInfo},
    {"nothing",         &nothingTypeInfo},
    {"null",            &nullTypeInfo},
    {"auto",            &autoTypeInfo},
    {"any",             &anyTypeInfo},
    {"data",            &dataTypeInfo},
    {"code",            &codeTypeInfo},
    {"reference",       &referenceTypeInfo},
    {"timeout",         &timeoutTypeInfo},
    {"softint",         &softBigIntTypeInfo},
    {"softfloat",       &softFloatTypeInfo},
    {"softnumber",      &softNumberTypeInfo},
    {"softstring",      &softStringTypeInfo},
    {"softbool",        &softBoolTypeInfo},
    {"softdate",        &softDateTypeInfo},
    {"softlist",        &softListTypeInfo},
    {"*int",            &bigIntOrNothingTypeInfo},
    {"*float",          &floatOrNothingTypeInfo},
    {"*number",         &numberOrNothingTypeInfo},
    {"*string",         &stringOrNothingTypeInfo},
    {"*bool",           &boolOrNothingTypeInfo},
    {"*date",           &dateOrNothingTypeInfo},
    {"*binary",         &binaryOrNothingTypeInfo},
    {"*hash",           &hashOrNothingTypeInfo},
    {"*list",           &listOrNothingTypeInfo},
    {"*object",         &objectOrNothingTypeInfo},
    {"*data",           &dataOrNothingTypeInfo},
    {"*code",           &codeOrNothingTypeInfo},
    {"*reference",      &referenceOrNothingTypeInfo},
    {"*timeout",        &timeoutOrNothingTypeInfo},
    {"*softint",        &softBigIntOrNothingTypeInfo},
    {"*softfloat",      &softFloatOrNothingTypeInfo},
    {"*softnumber",     &softNumberOrNothingTypeInfo},
    {"*softstring",     &softStringOrNothingTypeInfo},
    {"*softbool",       &softBoolOrNothingTypeInfo},
    {"*softdate",       &softDateOrNothingTypeInfo},
    {"*softlist",       &softListOrNothingTypeInfo},
    {"auto list",       &autoListTypeInfo},
    {"auto hash",       &autoHashTypeInfo},
    {"*auto list",      &autoListOrNothingTypeInfo},
    {"*auto hash",      &autoHashOrNothingTypeInfo},
    {"softauto list",   &softAutoListTypeInfo},
    {"*softauto list",  &softAutoListOrNothingTypeInfo},
    {nullptr, nullptr}
};

const QoreTypeInfo* QoreAOTTypeResolver::resolveBuiltin(const char* path) {
    for (const BuiltinTypeEntry* entry = builtin_types; entry->name; ++entry) {
        if (strcmp(path, entry->name) == 0) {
            return *entry->type_ptr;
        }
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveClassType(const char* path) {
    if (!pgm) {
        return nullptr;
    }
    ExceptionSink xsink;
    const QoreClass* qc = pgm->findClass(path, &xsink);
    if (xsink.isException()) {
        xsink.clear();
    }
    if (qc) {
        return qc->getTypeInfo();
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveHashDeclType(const char* path) {
    if (!pgm) {
        return nullptr;
    }
    // path format: "hash<DeclName>" — extract the name
    const char* start = strchr(path, '<');
    if (!start) {
        return nullptr;
    }
    ++start;
    const char* end = strchr(start, '>');
    if (!end) {
        return nullptr;
    }
    std::string decl_name(start, end - start);

    const QoreNamespace* pns = nullptr;
    const TypedHashDecl* thd = pgm->findHashDecl(decl_name.c_str(), pns);
    if (thd) {
        return thd->getTypeInfo();
    }
    return nullptr;
}

const QoreTypeInfo* QoreAOTTypeResolver::resolveComplexType(const char* path) {
    // Handle object<ClassName> patterns directly by looking up the class
    // in the namespace tree. This works even before rebuildAllIndexes() is called.
    if (strncmp(path, "object<", 7) == 0) {
        const char* start = path + 7;
        const char* end = strrchr(start, '>');
        if (end && end > start) {
            std::string class_path(start, end - start);
            // Look up the class in the program's namespace tree
            qore_program_private* pp = qore_program_private::get(*pgm);
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            // runtimeFindClass searches the namespace tree directly
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str());
            if (qc) {
                return qc->getOrNothingTypeInfo();
            }
        }
    }

    // Handle *object<ClassName> patterns (or-nothing class types)
    if (strncmp(path, "*object<", 8) == 0) {
        const char* start = path + 8;
        const char* end = strrchr(start, '>');
        if (end && end > start) {
            std::string class_path(start, end - start);
            qore_program_private* pp = qore_program_private::get(*pgm);
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str());
            if (qc) {
                return qc->getOrNothingTypeInfo();
            }
        }
    }

    // Use the existing parser infrastructure to resolve complex type strings
    // qore_get_type_from_string_intern() handles: list<T>, hash<T>, *T, reference<T>, etc.
    // We need to set up the program context so that class lookups like object<ClassName>
    // can find classes defined in the program's namespace tree.
    if (pgm) {
        ExceptionSink xsink;
        ProgramRuntimeParseAccessHelper pah(&xsink, pgm);
        if (!xsink) {
            return qore_get_type_from_string_intern(path);
        }
    }
    return qore_get_type_from_string_intern(path);
}

const QoreTypeInfo* QoreAOTTypeResolver::resolve(const char* path, std::string& error) {
    if (!path || !*path) {
        return nullptr;  // null/empty = no type constraint (auto)
    }

    // Check cache first
    auto it = cache.find(path);
    if (it != cache.end()) {
        return it->second;
    }

    // Try builtin types (fast path)
    const QoreTypeInfo* result = resolveBuiltin(path);

    // Try the parser-based resolver for complex types (handles everything)
    if (!result) {
        result = resolveComplexType(path);
    }

    if (result) {
        cache[path] = result;
        return result;
    }

    error = "cannot resolve type path: " + std::string(path);
    return nullptr;
}

// ---- Namespace Serialization (Phase 3) ----

namespace {

//! Get type path string from QoreTypeInfo, handling null
static const char* getTypePath(const QoreTypeInfo* ti) {
    return ti ? QoreTypeInfo::getPath(ti) : "";
}

//! Internal state for collecting namespace items during serialization
struct AOTSerializeState {
    struct NSInfo {
        qore_ns_private* ns;
        uint32_t parent_idx;
    };
    std::vector<NSInfo> namespaces;

    struct ClassInfo {
        QoreClass* cls;
        qore_class_private* priv;
        uint32_t ns_idx;
    };
    std::vector<ClassInfo> classes;

    struct HashDeclInfo {
        const TypedHashDecl* hd;
        uint32_t ns_idx;
    };
    std::vector<HashDeclInfo> hashdecls;

    struct EnumInfo {
        const QoreEnumDecl* ed;
        uint32_t ns_idx;
    };
    std::vector<EnumInfo> enums;

    struct TypedefInfo {
        std::string name;
        const QoreTypeInfo* typeInfo;
        bool pub;
        uint32_t ns_idx;
    };
    std::vector<TypedefInfo> typedefs;

    struct ConstInfo {
        const ConstantEntry* entry;
        uint32_t ns_idx;
    };
    std::vector<ConstInfo> constants;

    struct GlobalInfo {
        Var* var;
        uint32_t ns_idx;
    };
    std::vector<GlobalInfo> globals;

    struct FuncInfo {
        FunctionEntry* entry;
        QoreFunction* func;
        uint32_t ns_idx;
    };
    std::vector<FuncInfo> functions;

    struct MethodInfo {
        const QoreMethod* method;
        uint32_t class_idx;
        bool is_static;
    };
    std::vector<MethodInfo> methods;
};

//! Helper to check if an item should be skipped (from a different module than the one being compiled)
/** @param item_module the module name of the item (from getModuleName())
    @param current_module the module being compiled (nullptr means include all items)
    @return true if the item should be skipped, false otherwise
*/
static inline bool shouldSkipReexportedItem(const char* item_module, const char* current_module,
        const std::unordered_set<std::string>* keep_modules = nullptr) {
    // If no current module specified, include all items (non-strip-source mode)
    if (!current_module) {
        return false;
    }
    // If item has no module name, it matches the current module (or is script-local)
    if (!item_module) {
        return false;
    }
    // If item's module is in the keep set, don't skip it (e.g., local modules that
    // can't be loaded by name at runtime)
    if (keep_modules && keep_modules->count(item_module)) {
        return false;
    }
    // If item has a module and it differs from current module, skip it
    return strcmp(item_module, current_module) != 0;
}

//! Recursively collect all user-defined items from the namespace tree
/** @param state the state object to collect items into
    @param ns the namespace to collect from
    @param parent_idx the parent namespace index
    @param current_module optional module name to filter items; when provided, only items from this
           module are collected (items from reexported dependencies are filtered out)
*/
static void collectItems(AOTSerializeState& state, qore_ns_private* ns, uint32_t parent_idx,
        const char* current_module, const std::unordered_set<std::string>* keep_modules = nullptr) {
    uint32_t ns_idx = static_cast<uint32_t>(state.namespaces.size());
    state.namespaces.push_back({ns, parent_idx});

    // Collect user classes
    {
        ClassListIterator cli(ns->classList);
        while (cli.next()) {
            QoreClass* cls = cli.get();
            qore_class_private* priv = qore_class_private::get(*cls);
            if (!priv->sys) {
                // Filter out classes from reexported dependencies
                const char* class_module = priv->getModuleName();
                printd(5, "AOT serialize class '%s': module='%s' current_module='%s' skip=%d\n",
                    cls->getName(), class_module ? class_module : "n/a",
                    current_module ? current_module : "n/a",
                    shouldSkipReexportedItem(class_module, current_module, keep_modules));
                if (shouldSkipReexportedItem(class_module, current_module, keep_modules)) {
                    continue;
                }

                uint32_t class_idx = static_cast<uint32_t>(state.classes.size());
                state.classes.push_back({cls, priv, ns_idx});

                // Collect user methods for this class
                for (auto& mi : priv->hm) {
                    if (mi.second->isUser()) {
                        state.methods.push_back({mi.second, class_idx, false});
                    }
                }
                for (auto& mi : priv->shm) {
                    if (mi.second->isUser()) {
                        state.methods.push_back({mi.second, class_idx, true});
                    }
                }
            }
        }
    }

    // Collect user hashdecls
    {
        HashDeclListIterator hdi(ns->hashDeclList);
        while (hdi.next()) {
            TypedHashDecl* hd = hdi.get();
            if (!hd->isSystem()) {
                // Filter out hashdecls from reexported dependencies
                const char* hd_module = typed_hash_decl_private::get(*hd)->getModuleName();
                if (shouldSkipReexportedItem(hd_module, current_module, keep_modules)) {
                    continue;
                }
                state.hashdecls.push_back({hd, ns_idx});
            }
        }
    }

    // Collect user enums
    {
        EnumListIterator eli(ns->enumList);
        while (eli.next()) {
            QoreEnumDecl* ed = eli.get();
            if (!ed->isSystem()) {
                // Filter out enums from reexported dependencies
                const char* ed_module = qore_enum_decl_private::get(*ed)->getModuleName();
                if (shouldSkipReexportedItem(ed_module, current_module, keep_modules)) {
                    continue;
                }
                state.enums.push_back({ed, ns_idx});
            }
        }
    }

    // Collect user typedefs (only resolved ones)
    for (auto& ti : ns->typedefMap) {
        if (ti.second->typeInfo) {
            // Filter out typedefs from reexported dependencies
            const char* td_module = ti.second->getModuleName();
            if (shouldSkipReexportedItem(td_module, current_module, keep_modules)) {
                continue;
            }
            state.typedefs.push_back({ti.first, ti.second->typeInfo, ti.second->pub, ns_idx});
        }
    }

    // Collect user constants
    {
        ConstantListIterator cli(ns->constant);
        while (cli.next()) {
            ConstantEntry* ce = cli.getEntry();
            if (!ce->isSystem()) {
                // Filter out constants from reexported dependencies
                const char* const_module = ce->getModuleName();
                if (shouldSkipReexportedItem(const_module, current_module, keep_modules)) {
                    continue;
                }
                state.constants.push_back({ce, ns_idx});
            }
        }
    }

    // Collect user global variables
    for (auto& vi : ns->var_list.vmap) {
        Var* var = vi.second;
        if (!var->isBuiltin()) {
            // Filter out globals from reexported dependencies
            const char* var_module = var->getModuleName();
            if (shouldSkipReexportedItem(var_module, current_module, keep_modules)) {
                continue;
            }
            state.globals.push_back({var, ns_idx});
        }
    }

    // Collect user functions
    for (auto fi = ns->func_list.begin(), fe = ns->func_list.end(); fi != fe; ++fi) {
        FunctionEntry* entry = fi->second;
        QoreFunction* func = entry->getFunction();
        if (func && !entry->hasBuiltin()) {
            // Filter out functions from reexported dependencies
            const char* func_module = func->getModuleName();
            if (shouldSkipReexportedItem(func_module, current_module, keep_modules)) {
                continue;
            }
            state.functions.push_back({entry, func, ns_idx});
        }
    }

    // Recurse into child namespaces (filter out namespaces from reexported dependencies)
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            qore_ns_private* child_priv = qore_ns_private::get(*child_ns);
            // Filter out namespaces from reexported dependencies
            const char* ns_module = child_priv->getModuleName();
            printd(5, "AOT serialize: checking namespace '%s' from module '%s' (current_module='%s') skip=%d\n",
                child_ns->getName(), ns_module ? ns_module : "n/a",
                current_module ? current_module : "n/a",
                shouldSkipReexportedItem(ns_module, current_module, keep_modules));
            if (shouldSkipReexportedItem(ns_module, current_module, keep_modules)) {
                continue;
            }
            collectItems(state, child_priv, ns_idx, current_module, keep_modules);
        }
    }
}

//! Write a function/method variant signature
static void writeVariantSignature(QoreAOTBinaryWriter& writer, const AbstractQoreFunctionVariant* v) {
    const AbstractFunctionSignature* sig = const_cast<AbstractQoreFunctionVariant*>(v)->getSignature();
    assert(sig);

    // return type path
    writer.writeStringRef(getTypePath(sig->getReturnTypeInfo()));

    // num params
    uint32_t np = sig->numParams();
    writer.writeU32(np);

    // flags: bit 0 = varargs
    uint16_t flags = 0;
    if (sig->hasVarargs()) {
        flags |= 0x0001;
    }
    if (v->isUser()) {
        flags |= 0x0002;
    }
    writer.writeU16(flags);

    // params
    const arg_vec_t& defaults = sig->getDefaultArgList();
    for (uint32_t i = 0; i < np; ++i) {
        // param name
        const char* pname = sig->getName(i);
        writer.writeStringRef(pname ? pname : "");

        // param type path
        writer.writeStringRef(getTypePath(sig->getParamTypeInfo(i)));

        // default argument
        bool has_default = sig->hasDefaultArg(i);
        if (has_default && i < static_cast<uint32_t>(defaults.size())) {
            writer.writeU8(1);
            QoreValue dv = defaults[i];
            // Check if the default value is a serializable constant type.
            // AST expression nodes (function calls, variable refs, etc.) have types
            // not in the switch list and would be serialized as VT_NOTHING, which
            // would make the parameter appear required. Use VT_OPAQUE_DEFAULT instead
            // to preserve the "has default" semantics.
            qore_type_t dt = dv.getType();
            if (dv.isNothing() || dv.isNull() || dt == NT_BOOLEAN || dt == NT_INT
                    || dt == NT_FLOAT || dt == NT_STRING || dt == NT_DATE
                    || dt == NT_NUMBER || dt == NT_BINARY || dt == NT_LIST
                    || dt == NT_HASH) {
                writer.writeValue(dv);
            } else {
                // Complex expression default (e.g. function call like getcwd())
                // Write opaque marker so deserialization knows parameter is optional
                writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_OPAQUE_DEFAULT));
            }
        } else {
            writer.writeU8(0);
        }
    }
}

//! Write NAMESPACES section
static void writeNamespacesSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::NAMESPACES);

    uint32_t count = static_cast<uint32_t>(state.namespaces.size());
    writer.writeU32(count);

    for (auto& nsi : state.namespaces) {
        const qore_ns_private* ns = nsi.ns;
        writer.writeStringRef(ns->name.c_str());
        writer.writeStringRef(ns->path.c_str());
        writer.writeU32(nsi.parent_idx);
        writer.writeU32(ns->depth);
        uint16_t flags = 0;
        if (ns->pub) {
            flags |= 0x0001;
        }
        if (ns->builtin) {
            flags |= 0x0002;
        }
        if (ns->root) {
            flags |= 0x0004;
        }
        writer.writeU16(flags);
    }

    writer.endSection(sec_idx);
}

//! Write CLASSES section
static void writeClassesSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CLASSES);

    uint32_t count = static_cast<uint32_t>(state.classes.size());
    writer.writeU32(count);

    for (auto& ci : state.classes) {
        const qore_class_private* priv = ci.priv;

        // name and path
        writer.writeStringRef(priv->name.c_str());
        writer.writeStringRef(priv->path.c_str());
        writer.writeU32(ci.ns_idx);

        // flags: bit 0 = pub, bit 1 = final
        uint16_t flags = 0;
        if (priv->pub) {
            flags |= 0x0001;
        }
        if (priv->final) {
            flags |= 0x0002;
        }
        writer.writeU16(flags);

        // domain
        writer.writeI64(priv->domain);

        // base classes
        if (priv->scl) {
            uint32_t num_bases = static_cast<uint32_t>(priv->scl->size());
            writer.writeU32(num_bases);
            for (auto* bcn : *priv->scl) {
                // base class path
                if (bcn->sclass) {
                    const qore_class_private* bp = qore_class_private::get(*bcn->sclass);
                    writer.writeStringRef(bp->path.c_str());
                } else {
                    writer.writeStringRef("");
                }
                writer.writeU8(static_cast<uint8_t>(bcn->access));
                writer.writeU8(bcn->is_virtual ? 1 : 0);
            }
        } else {
            writer.writeU32(0);
        }

        // members - only serialize local (non-inherited) members
        uint32_t num_members = 0;
        for (auto& mi : priv->members.member_list) {
            if (mi.second->local()) {
                ++num_members;
            }
        }
        writer.writeU32(num_members);
        for (auto& mi : priv->members.member_list) {
            if (!mi.second->local()) {
                continue;
            }
            writer.writeStringRef(mi.first);
            writer.writeStringRef(getTypePath(mi.second->getTypeInfo()));
            writer.writeU8(static_cast<uint8_t>(mi.second->access));
            // default initialization value
            if (mi.second->exp) {
                writer.writeU8(1);
                // writeValue handles unsupported types by writing NOTHING
                writer.writeValue(mi.second->exp);
            } else {
                writer.writeU8(0);
            }
        }

        // static members
        uint32_t num_static = static_cast<uint32_t>(priv->vars.size());
        writer.writeU32(num_static);
        for (auto& vi : priv->vars.member_list) {
            writer.writeStringRef(vi.first);
            writer.writeStringRef(getTypePath(vi.second->getTypeInfo()));
            writer.writeU8(static_cast<uint8_t>(vi.second->access));
        }

        // class constants
        uint32_t num_consts = 0;
        {
            // count user constants
            ConstConstantListIterator ccli(priv->constlist);
            while (ccli.next()) {
                if (!ccli.getEntry()->isSystem()) {
                    ++num_consts;
                }
            }
        }
        writer.writeU32(num_consts);
        {
            ConstConstantListIterator ccli(priv->constlist);
            while (ccli.next()) {
                const ConstantEntry* ce = ccli.getEntry();
                if (!ce->isSystem()) {
                    writer.writeStringRef(ce->getName());
                    writer.writeStringRef(getTypePath(ce->typeInfo));
                    writer.writeU8(static_cast<uint8_t>(ce->getAccess()));
                    // Use getReferencedValue() for the actual evaluated value
                    QoreValue actual_val = ce->getReferencedValue();
                    writer.writeValue(actual_val);
                    actual_val.discard(nullptr);
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write HASHDECLS section
static void writeHashDeclsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::HASHDECLS);

    uint32_t count = static_cast<uint32_t>(state.hashdecls.size());
    writer.writeU32(count);

    for (auto& hdi : state.hashdecls) {
        const TypedHashDecl* hd = hdi.hd;

        writer.writeStringRef(hd->getName());
        std::string nspath = hd->getNamespacePath();
        writer.writeStringRef(nspath.c_str());
        writer.writeU32(hdi.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (hd->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // parent hashdecl path (empty if no parent)
        const TypedHashDecl* parent = hd->getParentHashDecl();
        if (parent) {
            std::string parent_path = parent->getNamespacePath();
            writer.writeStringRef(parent_path.c_str());
        } else {
            writer.writeStringRef("");
        }

        // members - count by iterating first
        uint32_t num_members = 0;
        {
            TypedHashDeclMemberIterator tmi(*hd);
            while (tmi.next()) {
                ++num_members;
            }
        }
        writer.writeU32(num_members);
        {
            TypedHashDeclMemberIterator tmi(*hd);
            while (tmi.next()) {
                writer.writeStringRef(tmi.getName());
                writer.writeStringRef(getTypePath(tmi.getMember().getTypeInfo()));
                // default values not serialized at this phase - marked as no default
                writer.writeU8(0);
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write ENUMS section
static void writeEnumsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::ENUMS);

    uint32_t count = static_cast<uint32_t>(state.enums.size());
    writer.writeU32(count);

    for (auto& ei : state.enums) {
        const QoreEnumDecl* ed = ei.ed;

        writer.writeStringRef(ed->getName());
        std::string nspath = ed->getNamespacePath();
        writer.writeStringRef(nspath.c_str());
        writer.writeU32(ei.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (ed->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // base type path
        writer.writeStringRef(getTypePath(ed->getBaseTypeInfo()));

        // members
        uint32_t num_members = static_cast<uint32_t>(ed->getMemberCount());
        writer.writeU32(num_members);
        {
            QoreEnumMemberIterator emi(*ed);
            while (emi.next()) {
                writer.writeStringRef(emi.getName());
                writer.writeValue(emi.getValue());
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write TYPEDEFS section
static void writeTypedefsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::TYPEDEFS);

    uint32_t count = static_cast<uint32_t>(state.typedefs.size());
    writer.writeU32(count);

    for (auto& ti : state.typedefs) {
        writer.writeStringRef(ti.name.c_str());
        writer.writeStringRef(getTypePath(ti.typeInfo));
        writer.writeU32(ti.ns_idx);
        writer.writeU8(ti.pub ? 1 : 0);
    }

    writer.endSection(sec_idx);
}

//! Write CONSTANTS section (namespace-level constants only; class constants are in CLASSES)
static void writeConstantsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::CONSTANTS);

    uint32_t count = static_cast<uint32_t>(state.constants.size());
    writer.writeU32(count);

    for (auto& ci : state.constants) {
        const ConstantEntry* ce = ci.entry;
        writer.writeStringRef(ce->getName());
        writer.writeStringRef(getTypePath(ce->typeInfo));
        writer.writeU32(ci.ns_idx);
        writer.writeU8(static_cast<uint8_t>(ce->getAccess()));
        writer.writeU8(ce->isPublic() ? 1 : 0);
        // Use getReferencedValue() to get the actual evaluated value.
        // ce->val may hold a RuntimeConstantRefNode (NT_RTCONSTREF) which is
        // just a reference to the constant's evaluated saved_val.
        QoreValue actual_val = ce->getReferencedValue();
        writer.writeValue(actual_val);
        actual_val.discard(nullptr);
    }

    writer.endSection(sec_idx);
}

//! Write GLOBALS section
static void writeGlobalsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::GLOBALS);

    uint32_t count = static_cast<uint32_t>(state.globals.size());
    writer.writeU32(count);

    for (auto& gi : state.globals) {
        Var* var = gi.var;
        writer.writeStringRef(var->getName());
        writer.writeStringRef(getTypePath(var->getTypeInfo()));
        writer.writeU32(gi.ns_idx);
        writer.writeU8(var->isThreadLocal() ? 1 : 0);
        writer.writeU8(var->isPublic() ? 1 : 0);
    }

    writer.endSection(sec_idx);
}

//! Write FUNCTIONS section
static void writeFunctionsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::FUNCTIONS);

    uint32_t count = static_cast<uint32_t>(state.functions.size());
    writer.writeU32(count);

    for (auto& fi : state.functions) {
        writer.writeStringRef(fi.entry->getName());
        writer.writeU32(fi.ns_idx);

        // flags: bit 0 = pub
        uint16_t flags = 0;
        if (fi.entry->isPublic()) {
            flags |= 0x0001;
        }
        writer.writeU16(flags);

        // count user variants
        uint32_t num_variants = 0;
        {
            QoreFunctionIterator qfi(*fi.func);
            while (qfi.next()) {
                if (qfi.getVariant()->isUser()) {
                    ++num_variants;
                }
            }
        }
        writer.writeU32(num_variants);

        // write user variant signatures
        {
            QoreFunctionIterator qfi(*fi.func);
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (v->isUser()) {
                    writeVariantSignature(writer, v);
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

//! Write METHODS section
static void writeMethodsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::METHODS);

    uint32_t count = static_cast<uint32_t>(state.methods.size());
    writer.writeU32(count);

    for (auto& mi : state.methods) {
        const QoreMethod* method = mi.method;
        writer.writeU32(mi.class_idx);
        writer.writeStringRef(method->getName());
        writer.writeU8(mi.is_static ? 1 : 0);

        // get the method's underlying function
        const qore_method_private* mp = qore_method_private::get(*method);
        const MethodFunctionBase* mfb = mp->func;

        // count user variants
        uint32_t num_variants = 0;
        {
            QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (v->isUser()) {
                    ++num_variants;
                }
            }
        }
        writer.writeU32(num_variants);

        // write user variant signatures
        {
            QoreFunctionIterator qfi(*static_cast<const QoreFunction*>(mfb));
            while (qfi.next()) {
                const AbstractQoreFunctionVariant* v = qfi.getVariant();
                if (v->isUser()) {
                    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
                    // write access + flags before the signature
                    writer.writeU8(static_cast<uint8_t>(mvb->getAccess()));
                    uint8_t mflags = 0;
                    if (mvb->isFinal()) {
                        mflags |= 0x01;
                    }
                    if (mvb->isAbstract()) {
                        mflags |= 0x02;
                    }
                    writer.writeU8(mflags);
                    writeVariantSignature(writer, v);
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

} // anonymous namespace

void serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::SLOT_MAPS);

    // Number of function entries
    writer.writeU32(static_cast<uint32_t>(funcs.size()));

    for (auto& func : funcs) {
        // Function header
        writer.writeStringRef(func.name.c_str());
        writer.writeU16(static_cast<uint16_t>(func.num_locals));
        writer.writeU16(static_cast<uint16_t>(func.num_globals));
        writer.writeU16(static_cast<uint16_t>(func.num_exprs));
        writer.writeU16(static_cast<uint16_t>(func.num_stmts));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.regex_cases.size()));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.body_locals.size()));
        writer.writeU8(func.slot_ids.has_unsupported_exprs ? 1 : 0);
        writer.writeU8(0); // padding for alignment

        // Local slot entries (in slot order)
        for (auto& local : func.slot_ids.locals) {
            writer.writeStringRef(local.name.c_str());
            writer.writeStringRef(local.type_path.c_str());
            writer.writeU8(local.flags);
            writer.writeU16(local.param_index);
        }

        // Global slot entries (in slot order)
        for (auto& global : func.slot_ids.globals) {
            writer.writeStringRef(global.name.c_str());
            writer.writeStringRef(global.type_path.c_str());
            writer.writeU8(global.is_thread_local ? 1 : 0);
        }

        // Expression slot entries (in slot order)
        for (auto& expr : func.slot_ids.exprs) {
            writer.writeU8(static_cast<uint8_t>(expr.kind));
            switch (expr.kind) {
                case AOTExprKind::FUNC_CALL:
                case AOTExprKind::NEW_OBJECT:
                case AOTExprKind::SCOPED_NEW_OBJECT:
                case AOTExprKind::RUNTIME_CONST_REF:
                case AOTExprKind::LOCAL_VARREF:
                case AOTExprKind::GLOBAL_VARREF:
                case AOTExprKind::CONST_NUMBER:
                case AOTExprKind::CONST_BINARY:
                case AOTExprKind::HASHDECL_NEW:
                case AOTExprKind::COMPLEX_HASH_NEW:
                case AOTExprKind::COMPLEX_LIST_NEW:
                    // ref1 = name/value/index/path/slot
                    writer.writeStringRef(expr.ref1.c_str());
                    break;
                case AOTExprKind::SELF_METHOD_CALL:
                case AOTExprKind::STATIC_METHOD_CALL:
                case AOTExprKind::STATIC_VARREF:
                    // ref1 = class path, ref2 = method/var name
                    writer.writeStringRef(expr.ref1.c_str());
                    writer.writeStringRef(expr.ref2.c_str());
                    break;
                case AOTExprKind::SELF_VARREF:
                    // ref1 = member name
                    writer.writeStringRef(expr.ref1.c_str());
                    break;
                case AOTExprKind::CLOSURE_CREATE:
                case AOTExprKind::CALL_REF:
                case AOTExprKind::OBJ_METHOD_REF:
                    // no additional data
                    break;
                case AOTExprKind::EXPR_TREE: {
                    // ref1 contains binary tree blob — write inline with length prefix
                    uint32_t blob_size = static_cast<uint32_t>(expr.ref1.size());
                    writer.writeU32(blob_size);
                    writer.writeBytes(expr.ref1.data(), blob_size);
                    break;
                }
                case AOTExprKind::GENERIC_EVAL:
                default:
                    // no additional data — function needs source fallback
                    break;
            }
        }

        // Body local entries (in order)
        for (auto& bl : func.slot_ids.body_locals) {
            writer.writeStringRef(bl.name.c_str());
            writer.writeStringRef(bl.type_path.c_str());
            writer.writeU8(bl.is_closure ? 1 : 0);
        }

        // Regex case entries (in slot-index order)
        // Format per case: pattern_ref(u32) options(i64) is_negated(u8)
        for (auto& rc : func.slot_ids.regex_cases) {
            writer.writeStringRef(rc.pattern.c_str());
            writer.writeI64(rc.options);
            writer.writeU8(rc.is_negated ? 1 : 0);
        }
    }

    writer.endSection(sec_idx);
}

// ---- Per-Function Source Fallback (Phase 6) ----

void serializeFallbackSources(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const char* source_text, int source_len) {
    // Collect functions that need source fallback:
    // - functions with unsupported expressions (need AST evaluation)
    // - functions with stmt_slots (on_exit/on_success/on_error handlers need AST execution)
    std::vector<const AOTCompiledFuncWithSlots*> fallback_funcs;
    for (auto& func : funcs) {
        if (func.slot_ids.has_unsupported_exprs || func.num_stmts > 0) {
            fallback_funcs.push_back(&func);
        }
    }

    // Always write the FUNC_SOURCES section when called (caller controls whether
    // to include source via --include-source flag)
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::FUNC_SOURCES);

    // Store the full source text for re-parsing fallback functions
    writer.writeStringRef(source_text, static_cast<size_t>(source_len));

    // Write list of function names that need source fallback
    writer.writeU32(static_cast<uint32_t>(fallback_funcs.size()));
    for (auto* func : fallback_funcs) {
        writer.writeStringRef(func->name.c_str());
    }

    writer.endSection(sec_idx);
}

// ---- Namespace Deserialization (Phase 4) ----

#include "qore/intern/Function.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/Variable.h"

bool QoreAOTBinaryDeserializer::deserializeIntoProgram(QoreProgram* in_pgm, const uint8_t* data,
        uint32_t size, std::string& error) {
    pgm = in_pgm;

    // Open and validate the binary blob
    if (!reader.open(data, size, error)) {
        return false;
    }

    // Create type resolver for this program
    type_resolver = new QoreAOTTypeResolver(pgm);

    // Deserialize in dependency order
    if (!deserializeNamespaces(error)) {
        return false;
    }
    if (!deserializeClasses(error)) {
        return false;
    }
    // Resolve class base classes after all classes are created (two-pass)
    if (!resolveClassBases(error)) {
        return false;
    }
    // Deserialize hashdecls and enums before static members so type references resolve
    if (!deserializeHashDecls(error)) {
        return false;
    }
    if (!deserializeEnums(error)) {
        return false;
    }
    if (!deserializeTypedefs(error)) {
        return false;
    }
    // Resolve typedefs first (multi-pass for forward refs), then enum base types and hashdecl members
    // Order matters: enum base types and hashdecl members may reference typedefs
    if (!resolveTypedefs(error)) {
        return false;
    }
    if (!resolveEnumBaseTypes(error)) {
        return false;
    }
    if (!resolveHashdeclMembers(error)) {
        return false;
    }
    // Resolve class members and constants after all types are available
    if (!resolveInstanceMembers(error)) {
        return false;
    }
    // Import inherited members from base classes (must be after resolveInstanceMembers)
    if (!importInheritedMembers(error)) {
        return false;
    }
    if (!resolveStaticMembers(error)) {
        return false;
    }
    if (!resolveClassConstants(error)) {
        return false;
    }
    if (!deserializeConstants(error)) {
        return false;
    }
    if (!deserializeGlobals(error)) {
        return false;
    }
    if (!deserializeFunctions(error)) {
        return false;
    }
    if (!deserializeMethods(error)) {
        return false;
    }
    // Commit all newly deserialized classes (set initialized + commit pending method variants)
    if (!commitDeserializedClasses(error)) {
        return false;
    }
    if (!deserializeFallbackSources(error)) {
        return false;
    }

    // Rebuild root namespace indexes (fmap, varmap, clmap, etc.) so that
    // runtime lookups like runtimeFindFunctionEntry() can find the
    // deserialized functions, classes, etc.
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
        qore_ns_private::get(*pp->RootNS));
    rpriv->rebuildAllIndexes();

    printd(2, "AOT: deserialized namespace tree: %d namespaces, %d classes%s\n",
        static_cast<int>(ns_list.size()), static_cast<int>(class_list.size()),
        hasFallbackSource() ? " (with source fallback)" : "");

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeNamespaces(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::NAMESPACES);
    if (!sec) {
        return true;  // no namespaces section is OK
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid NAMESPACES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    ns_list.resize(count);

    // Get program's root namespace
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* path = reader.readStringRef(ptr);
        uint32_t parent_idx = QoreAOTBinaryReader::readU32(ptr);
        uint32_t depth = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        (void)depth;

        if (parent_idx == UINT32_MAX) {
            // Root namespace - use existing
            ns_list[i] = root_ns;
        } else {
            // Create child namespace and add to parent
            if (parent_idx >= ns_list.size() || !ns_list[parent_idx]) {
                error = "invalid parent namespace index " + std::to_string(parent_idx);
                return false;
            }

            // Check if this namespace already exists in the parent (e.g. "Qore" system NS)
            QoreNamespace* existing = nullptr;
            auto it = ns_list[parent_idx]->nsl.nsmap.find(name);
            if (it != ns_list[parent_idx]->nsl.nsmap.end()) {
                existing = it->second;
            }

            if (existing) {
                ns_list[i] = qore_ns_private::get(*existing);
            } else {
                QoreNamespace* ns = new QoreNamespace(name);
                qore_ns_private* nsp = qore_ns_private::get(*ns);
                nsp->pub = (flags & 0x0001) != 0;
                // Mark as non-builtin so it's treated as user-defined and can be merged
                nsp->builtin = false;
                ns_list[parent_idx]->ns->addNamespace(ns);
                ns_list[i] = nsp;
            }
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeClasses(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CLASSES);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid CLASSES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    class_list.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        int64_t domain = QoreAOTBinaryReader::readI64(ptr);

        // Validate namespace index before creating the class
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index for class '" + std::string(name) + "'";
            return false;
        }

        // Create the class and add to namespace immediately so it's owned
        // by the namespace (QoreClass destructor is protected)
        QoreClass* qc = new QoreClass(name, path, domain);
        qore_class_private* priv = qore_class_private::get(*qc);
        priv->pub = (flags & 0x0001) != 0;
        if (flags & 0x0002) {
            priv->final = true;
        }
        bool class_already_existed = false;
        int add_rv = ns_list[ns_idx]->classList.add(qc);
        if (add_rv != 0) {
            printd(2, "AOT deser: class '%s' already exists in namespace, using existing\n", name);
            // Class already exists - use the existing one and delete the new one
            QoreClass* existing = ns_list[ns_idx]->classList.find(name);
            qore_class_private::get(*qc)->deref(true, true);
            qc = existing;
            class_already_existed = true;
            preexisting_classes.insert(i);
        }
        class_list[i] = qc;

        // Read base classes (store paths for later resolution)
        uint32_t num_bases = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingBaseClass> bases;
        bases.reserve(num_bases);
        for (uint32_t j = 0; j < num_bases; ++j) {
            const char* base_path = reader.readStringRef(ptr);
            uint8_t access = QoreAOTBinaryReader::readU8(ptr);
            uint8_t is_virtual = QoreAOTBinaryReader::readU8(ptr);
            if (base_path && *base_path) {
                PendingBaseClass pbc;
                pbc.base_path = base_path;
                pbc.access = access;
                pbc.is_virtual = (is_virtual != 0);
                bases.push_back(std::move(pbc));
            }
        }
        // Skip pending data for classes that already existed (from loaded modules)
        // — they already have their bases, members, etc. set up
        if (class_already_existed) {
            bases.clear();
        }
        pending_bases.push_back(std::move(bases));

        // Read instance members (store for later resolution after hashdecls/enums)
        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingInstanceMember> instance_members;
        if (!class_already_existed) {
            instance_members.reserve(num_members);
        }
        for (uint32_t j = 0; j < num_members; ++j) {
            const char* mname = reader.readStringRef(ptr);
            const char* mtype_path = reader.readStringRef(ptr);
            uint8_t maccess = QoreAOTBinaryReader::readU8(ptr);
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            QoreValue default_val;
            if (has_default) {
                default_val = reader.readValue(ptr, end, error);
                if (!error.empty()) {
                    error = "instance member '" + std::string(mname ? mname : "(null)") + "' default: " + error;
                    return false;
                }
            }

            if (!class_already_existed && mname && *mname) {
                PendingInstanceMember pim;
                pim.name = mname;
                pim.type_path = mtype_path ? mtype_path : "";
                pim.access = maccess;
                pim.default_val = default_val;
                instance_members.push_back(std::move(pim));
            }
        }
        pending_instance_members.push_back(std::move(instance_members));

        // Read static members (store for later resolution)
        uint32_t num_static = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingStaticMember> static_members;
        if (!class_already_existed) {
            static_members.reserve(num_static);
        }
        for (uint32_t j = 0; j < num_static; ++j) {
            const char* sm_name = reader.readStringRef(ptr);
            const char* sm_type_path = reader.readStringRef(ptr);
            uint8_t sm_access = QoreAOTBinaryReader::readU8(ptr);
            if (!class_already_existed && sm_name && *sm_name) {
                PendingStaticMember psm;
                psm.name = sm_name;
                psm.type_path = sm_type_path ? sm_type_path : "";
                psm.access = sm_access;
                static_members.push_back(std::move(psm));
            }
        }
        pending_static_members.push_back(std::move(static_members));

        // Read class constants (store for later resolution after hashdecls/enums)
        uint32_t num_consts = QoreAOTBinaryReader::readU32(ptr);
        std::vector<PendingClassConstant> class_constants;
        if (!class_already_existed) {
            class_constants.reserve(num_consts);
        }
        for (uint32_t j = 0; j < num_consts; ++j) {
            const char* cname = reader.readStringRef(ptr);
            const char* ctype_path = reader.readStringRef(ptr);
            uint8_t caccess = QoreAOTBinaryReader::readU8(ptr);
            QoreValue cval = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                error = "class constant '" + std::string(cname ? cname : "(null)") + "': " + error;
                return false;
            }

            if (!class_already_existed && cname && *cname) {
                PendingClassConstant pcc;
                pcc.name = cname;
                pcc.type_path = ctype_path ? ctype_path : "";
                pcc.access = caccess;
                pcc.value = cval;
                class_constants.push_back(std::move(pcc));
            }
        }
        pending_class_constants.push_back(std::move(class_constants));
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveClassBases(std::string& error) {
    // Second pass: resolve base class references now that all classes exist
    for (uint32_t i = 0; i < class_list.size() && i < pending_bases.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        for (auto& pbc : pending_bases[i]) {
            // Look up base class by path in the program
            ExceptionSink xsink;
            const QoreClass* base = pgm->findClass(pbc.base_path.c_str(), &xsink);
            if (xsink.isException()) {
                xsink.clear();
            }
            if (base) {
                // Add base class to this class with proper access level
                qc->addBaseClass(const_cast<QoreClass*>(base),
                    static_cast<ClassAccess>(pbc.access), pbc.is_virtual);
                printd(5, "AOT deser: resolved base class '%s' for class '%s' (access: %d)\n",
                    pbc.base_path.c_str(), qc->getName(), pbc.access);
            } else {
                error = "cannot resolve base class '" + pbc.base_path + "' for class '" +
                    std::string(qc->getName()) + "'";
                pending_bases.clear();
                return false;
            }
        }
    }

    // Clear pending data
    pending_bases.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveInstanceMembers(std::string& error) {
    // Second pass: create instance members now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    for (uint32_t i = 0; i < class_list.size() && i < pending_instance_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& pim : pending_instance_members[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!pim.type_path.empty()) {
                ti = type_resolver->resolve(pim.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for instance member '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        pim.type_path.c_str(), pim.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            // Transfer ownership of the default value to the class member
            QoreValue default_val = pim.default_val;
            pim.default_val = QoreValue();  // Clear to prevent double-deref
            priv->addMember(pim.name.c_str(), static_cast<ClassAccess>(pim.access), ti,
                default_val);

            printd(5, "AOT deser: added instance member '%s' to class '%s'\n",
                pim.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_instance_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveStaticMembers(std::string& error) {
    // Second pass: create static members now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    for (uint32_t i = 0; i < class_list.size() && i < pending_static_members.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& psm : pending_static_members[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!psm.type_path.empty()) {
                ti = type_resolver->resolve(psm.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for static member '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        psm.type_path.c_str(), psm.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            // Create the static variable info
            QoreVarInfo* vi = new QoreVarInfo(&loc_builtin, ti, nullptr, QoreValue(),
                static_cast<ClassAccess>(psm.access));

            // Add to class's vars list
            priv->vars.addNoCheck(strdup(psm.name.c_str()), vi);

            printd(5, "AOT deser: added static member '%s' to class '%s'\n",
                psm.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_static_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveClassConstants(std::string& error) {
    // Second pass: add class constants now that types are resolved
    // NOTE: hashdecls and enums must be deserialized before calling this method
    // so that type references to them can be resolved
    for (uint32_t i = 0; i < class_list.size() && i < pending_class_constants.size(); ++i) {
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }

        qore_class_private* priv = qore_class_private::get(*qc);
        for (auto& pcc : pending_class_constants[i]) {
            const QoreTypeInfo* ti = nullptr;
            if (!pcc.type_path.empty()) {
                ti = type_resolver->resolve(pcc.type_path.c_str(), error);
                if (!error.empty()) {
                    printd(2, "AOT deser: cannot resolve type '%s' for constant '%s' "
                        "in class '%s': %s (falling back to auto)\n",
                        pcc.type_path.c_str(), pcc.name.c_str(), qc->getName(), error.c_str());
                    error.clear();
                    ti = autoTypeInfo;
                }
            }

            // Use addUserConstant to avoid setting sys=true on user classes
            priv->addUserConstant(pcc.name.c_str(), pcc.value,
                static_cast<ClassAccess>(pcc.access), ti);

            printd(5, "AOT deser: added constant '%s' to class '%s'\n",
                pcc.name.c_str(), qc->getName());
        }
    }

    // Clear pending data
    pending_class_constants.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveHashdeclMembers(std::string& error) {
    // Second pass: add hashdecl members now that all types exist
    for (auto& entry : pending_hashdecl_members) {
        TypedHashDecl* hd = entry.first;
        typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);

        for (auto& phm : entry.second) {
            const QoreTypeInfo* mti = type_resolver->resolve(phm.type_path.c_str(), error);
            if (!error.empty()) {
                // Fall back to auto type when the type can't be resolved
                printd(2, "AOT deser: cannot resolve type '%s' for member '%s' in hashdecl '%s': %s "
                    "(falling back to auto)\n",
                    phm.type_path.c_str(), phm.name.c_str(), hd->getName(), error.c_str());
                error.clear();
                mti = autoTypeInfo;
            }
            hdp->addMember(phm.name.c_str(), mti, phm.default_val);

            printd(5, "AOT deser: added member '%s' to hashdecl '%s'\n",
                phm.name.c_str(), hd->getName());
        }
    }

    // Clear pending data
    pending_hashdecl_members.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::resolveTypedefs(std::string& error) {
    // Multi-pass resolution to handle forward references between typedefs
    // Keep iterating until all are resolved or no progress is made
    while (!pending_typedefs.empty()) {
        size_t resolved_count = 0;
        std::vector<PendingTypedef> unresolved;

        for (auto& pt : pending_typedefs) {
            std::string temp_error;
            const QoreTypeInfo* ti = type_resolver->resolve(pt.type_path.c_str(), temp_error);
            if (temp_error.empty() && ti) {
                ns_list[pt.ns_idx]->typedefMap[pt.name.c_str()] =
                    new TypedefEntry(nullptr, ti, nullptr, pt.is_pub);
                ++resolved_count;
                printd(5, "AOT deser: created typedef '%s'\n", pt.name.c_str());
            } else {
                unresolved.push_back(std::move(pt));
            }
        }

        if (resolved_count == 0 && !unresolved.empty()) {
            // No progress - circular reference or genuinely missing type
            error = "cannot resolve type '" + unresolved[0].type_path +
                "' for typedef '" + unresolved[0].name + "'";
            pending_typedefs.clear();
            return false;
        }

        pending_typedefs = std::move(unresolved);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveEnumBaseTypes(std::string& error) {
    // Resolve enum base types now that typedefs are available
    for (auto& pebt : pending_enum_base_types) {
        const QoreTypeInfo* base_ti = type_resolver->resolve(pebt.base_type_path.c_str(), error);
        if (!error.empty()) {
            error = "cannot resolve base type '" + pebt.base_type_path +
                "' for enum '" + std::string(pebt.ed->getName()) + "': " + error;
            pending_enum_base_types.clear();
            return false;
        }
        if (base_ti) {
            qore_enum_decl_private::get(*pebt.ed)->setBaseTypeInfo(base_ti);
            printd(5, "AOT deser: set base type for enum '%s'\n", pebt.ed->getName());
        }
    }

    // Clear pending data
    pending_enum_base_types.clear();
    return true;
}

bool QoreAOTBinaryDeserializer::deserializeHashDecls(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::HASHDECLS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid HASHDECLS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    // Two-pass approach: first create all hashdecls, then resolve parent pointers
    struct HashdeclInfo {
        TypedHashDecl* hd;
        std::string parent_path;
    };
    std::vector<HashdeclInfo> hashdecl_list;
    hashdecl_list.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* nspath = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        const char* parent_path = reader.readStringRef(ptr);

        // Read members first to collect info
        struct MemberInfo {
            std::string name;
            std::string type_path;
            QoreValue default_val;
        };
        std::vector<MemberInfo> members;

        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        members.reserve(num_members);
        for (uint32_t j = 0; j < num_members; ++j) {
            MemberInfo mi;
            mi.name = reader.readStringRef(ptr);
            mi.type_path = reader.readStringRef(ptr);
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            if (has_default) {
                mi.default_val = reader.readValue(ptr, end, error);
                if (!error.empty()) {
                    error = "hashdecl '" + std::string(name ? name : "(null)") + "' member '" + mi.name + "' default: " + error;
                    return false;
                }
            }
            members.push_back(std::move(mi));
        }

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            printd(2, "AOT: skipping hashdecl '%s' - invalid namespace index %u\n", name, ns_idx);
            continue;
        }

        // Create the TypedHashDecl
        TypedHashDecl* hd = new TypedHashDecl(name, nspath);
        typed_hash_decl_private* hdp = typed_hash_decl_private::get(*hd);

        // Set visibility
        if (flags & 0x0001) {
            hdp->setPublic();
        }

        // Set namespace
        hdp->setNamespace(ns_list[ns_idx]);

        // Add to namespace's hashDeclList FIRST (before storing in pending list)
        if (ns_list[ns_idx]->hashDeclList.add(hd) != 0) {
            printd(2, "AOT: hashdecl '%s' already exists in namespace\n", name);
            hdp->deref();
            continue;
        }

        // Store members for later resolution (after all hashdecls/enums/typedefs exist)
        std::vector<PendingHashdeclMember> pending_members;
        pending_members.reserve(members.size());
        for (auto& mi : members) {
            PendingHashdeclMember phm;
            phm.name = std::move(mi.name);
            phm.type_path = std::move(mi.type_path);
            phm.default_val = mi.default_val;
            pending_members.push_back(std::move(phm));
        }
        pending_hashdecl_members.push_back({hd, std::move(pending_members)});

        // Store for parent resolution pass
        hashdecl_list.push_back({hd, parent_path ? parent_path : ""});
    }

    // Second pass: resolve parent hashdecl pointers
    for (auto& hdi : hashdecl_list) {
        if (!hdi.parent_path.empty()) {
            // Look up parent by path in the program
            qore_program_private* pp = qore_program_private::get(*pgm);
            qore_root_ns_private* rpriv = static_cast<qore_root_ns_private*>(
                qore_ns_private::get(*pp->RootNS));
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* parent = qore_root_ns_private::runtimeFindHashDecl(
                *rpriv->rns, hdi.parent_path.c_str(), found_ns);
            if (parent) {
                typed_hash_decl_private::get(*hdi.hd)->setParentHashDecl(parent);
            }
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeEnums(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::ENUMS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid ENUMS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* nspath = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        const char* base_type_path = reader.readStringRef(ptr);

        // Read members first to collect info
        struct EnumMemberInfo {
            std::string name;
            QoreValue val;
        };
        std::vector<EnumMemberInfo> members;

        uint32_t num_members = QoreAOTBinaryReader::readU32(ptr);
        members.reserve(num_members);
        for (uint32_t j = 0; j < num_members; ++j) {
            EnumMemberInfo emi;
            emi.name = reader.readStringRef(ptr);
            emi.val = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                error = "enum '" + std::string(name ? name : "(null)") + "' member '" + emi.name + "': " + error;
                return false;
            }
            members.push_back(std::move(emi));
        }

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            printd(2, "AOT: skipping enum '%s' - invalid namespace index %u\n", name, ns_idx);
            continue;
        }

        // Create the QoreEnumDecl with default base type (will be resolved later if needed)
        QoreEnumDecl* ed = new QoreEnumDecl(name, nspath, bigIntTypeInfo);

        // Store base type path for later resolution if it's not the default
        if (base_type_path && *base_type_path) {
            PendingEnumBaseType pebt;
            pebt.ed = ed;
            pebt.base_type_path = base_type_path;
            pending_enum_base_types.push_back(std::move(pebt));
        }
        qore_enum_decl_private* edp = qore_enum_decl_private::get(*ed);

        // Set visibility
        if (flags & 0x0001) {
            edp->setPublic();
        }

        // Set namespace
        edp->setNamespace(ns_list[ns_idx]);

        // Add members
        for (auto& emi : members) {
            edp->addMember(emi.name.c_str(), emi.val);
        }

        // Add to namespace's enumList
        if (ns_list[ns_idx]->enumList.add(ed) != 0) {
            printd(2, "AOT: enum '%s' already exists in namespace\n", name);
            // Remove any pending base type entry that points to the deleted enum
            if (base_type_path && *base_type_path) {
                pending_enum_base_types.pop_back();
            }
            edp->deref();
            continue;
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeTypedefs(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::TYPEDEFS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid TYPEDEFS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    // Store typedefs for later resolution (after all hashdecls/enums exist)
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);

        // Validate namespace index
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for typedef '" + std::string(name ? name : "(null)") + "'";
            return false;
        }

        if (name && *name) {
            PendingTypedef pt;
            pt.name = name;
            pt.type_path = type_path ? type_path : "";
            pt.ns_idx = ns_idx;
            pt.is_pub = (is_pub != 0);
            pending_typedefs.push_back(std::move(pt));
        }
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeConstants(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::CONSTANTS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid CONSTANTS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t access = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);
        QoreValue val = reader.readValue(ptr, end, error);
        if (!error.empty()) {
            error = "namespace constant '" + std::string(name ? name : "(null)") + "': " + error;
            return false;
        }

        // Add constant to namespace
        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            val.discard(nullptr);
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for constant '" + std::string(name ? name : "(null)") + "'";
            return false;
        }
        if (!name || !*name) {
            val.discard(nullptr);
            error = "invalid empty name for namespace constant";
            return false;
        }

        // Skip if constant already exists (from dependency module)
        {
            const QoreTypeInfo* existing_ti = nullptr;
            bool found = false;
            ns_list[ns_idx]->constant.find(name, existing_ti, found);
            if (found) {
                printd(2, "AOT: skipping constant '%s' - already exists (from dependency)\n", name);
                val.discard(nullptr);
                continue;
            }
        }

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            val.discard(nullptr);
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for constant '" + std::string(name) + "': " + error;
            return false;
        }
        // Create as user constant (not builtin) with proper pub flag.
        // Using add() would mark the constant as builtin, which causes
        // scanMergeCommittedNamespace to skip it (isUserPublic() returns false).
        // Create the ConstantEntry directly with: pub = is_pub, init = true (value
        // already resolved), builtin = false (user constant from AOT module).
        ConstantEntry* ce = new ConstantEntry(&loc_builtin, name, val,
            ti ? ti : val.getTypeInfo(), is_pub != 0, true, false,
            static_cast<ClassAccess>(access));
        ns_list[ns_idx]->constant.addEntry(name, ce);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeGlobals(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::GLOBALS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid GLOBALS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        const char* type_path = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint8_t is_thread_local = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_pub = QoreAOTBinaryReader::readU8(ptr);

        if (ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid namespace index " + std::to_string(ns_idx) +
                " for global variable '" + std::string(name ? name : "(null)") + "'";
            return false;
        }
        if (!name || !*name) {
            error = "invalid empty name for global variable";
            return false;
        }

        const QoreTypeInfo* ti = type_resolver->resolve(type_path, error);
        if (!error.empty()) {
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for global variable '" + std::string(name) + "': " + error;
            return false;
        }

        // Create the global variable directly
        Var* var = new Var(get_runtime_location(), name, ti, false,
            is_thread_local != 0);
        if (is_pub) {
            var->setPublic();
        }
        ns_list[ns_idx]->var_list.vmap[var->getName()] = var;

        printd(5, "AOT deser: created global var '%s' (type=%s, thread_local=%d)\n",
            name, type_path, is_thread_local);
    }

    return true;
}

//! Helper: read a variant signature and set up the UserSignature from AOT metadata
static bool readAndSetupVariantSignature(
        const QoreAOTBinaryReader& reader,
        QoreAOTTypeResolver* type_resolver,
        QoreProgram* pgm,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb,
        bool& has_varargs,
        std::string& error,
        const QoreClass* classTypeInfo = nullptr) {
    // return type path
    const char* ret_type_path = reader.readStringRef(ptr);

    // num params
    uint32_t np = QoreAOTBinaryReader::readU32(ptr);

    // flags: bit 0 = varargs, bit 1 = is_user
    uint16_t sig_flags = QoreAOTBinaryReader::readU16(ptr);
    has_varargs = (sig_flags & 0x0001) != 0;

    // Read params
    std::vector<std::string> param_names;
    std::vector<const QoreTypeInfo*> param_types;
    std::vector<QoreValue> param_defaults;
    param_names.resize(np);
    param_types.resize(np);
    param_defaults.resize(np);

    for (uint32_t j = 0; j < np; ++j) {
        const char* pname = reader.readStringRef(ptr);
        const char* ptype_path = reader.readStringRef(ptr);
        uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);

        param_names[j] = pname ? pname : "";

        const QoreTypeInfo* pti = type_resolver->resolve(ptype_path, error);
        if (!error.empty()) {
            // Fall back to auto type when the type can't be resolved (e.g., module-private
            // types that were filtered from the metadata). The compiled code already has
            // the type checks baked in, so this only affects variant matching.
            printd(2, "AOT deser: cannot resolve type '%s' for parameter '%s': %s "
                "(falling back to auto)\n",
                ptype_path ? ptype_path : "(null)", param_names[j].c_str(), error.c_str());
            error.clear();
            pti = autoTypeInfo;
        }
        param_types[j] = pti;

        if (has_default) {
            param_defaults[j] = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                // Clean up already-read defaults
                for (uint32_t k = 0; k < j; ++k) {
                    param_defaults[k].discard(nullptr);
                }
                return false;
            }
        }
    }

    // Resolve return type
    const QoreTypeInfo* ret_ti = type_resolver->resolve(ret_type_path, error);
    if (!error.empty()) {
        // Fall back to auto type when the return type can't be resolved
        printd(2, "AOT deser: cannot resolve return type '%s': %s (falling back to auto)\n",
            ret_type_path ? ret_type_path : "(null)", error.c_str());
        error.clear();
        ret_ti = autoTypeInfo;
    }

    // Set up the variant's signature from metadata
    UserSignature* sig = uvb->getUserSignature();
    sig->setupFromAOTMetadata(pgm, ret_ti, param_names, param_types, param_defaults, has_varargs, classTypeInfo);

    // Clean up default values (they were ref'd by setupFromAOTMetadata)
    for (auto& dv : param_defaults) {
        dv.discard(nullptr);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeFunctions(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNCTIONS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNCTIONS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        uint32_t ns_idx = QoreAOTBinaryReader::readU32(ptr);
        uint16_t flags = QoreAOTBinaryReader::readU16(ptr);
        uint32_t num_variants = QoreAOTBinaryReader::readU32(ptr);

        if (!name || !*name || ns_idx >= ns_list.size() || !ns_list[ns_idx]) {
            error = "invalid function entry";
            return false;
        }

        // Skip if function already exists (from dependency module)
        if (ns_list[ns_idx]->func_list.findNode(name)) {
            printd(2, "AOT: skipping function '%s' - already exists (from dependency)\n", name);
            // Skip reading variants (must match exact format of readAndSetupVariantSignature)
            for (uint32_t v = 0; v < num_variants; ++v) {
                // Read variant data matching the format in readAndSetupVariantSignature:
                // 1. ret_type_path (StringRef)
                reader.readStringRef(ptr);
                // 2. num_params (U32)
                uint32_t num_params = QoreAOTBinaryReader::readU32(ptr);
                // 3. sig_flags (U16)
                QoreAOTBinaryReader::readU16(ptr);
                // 4. For each param: name, type_path, has_default, and optionally value
                for (uint32_t p = 0; p < num_params; ++p) {
                    reader.readStringRef(ptr);  // param name
                    reader.readStringRef(ptr);  // param type path
                    uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (has_default) {
                        QoreValue default_val = reader.readValue(ptr, end, error);
                        if (!error.empty()) {
                            return false;
                        }
                        default_val.discard(nullptr);
                    }
                }
            }
            continue;
        }

        // Create the QoreFunction
        QoreFunction* func = new QoreFunction(name);

        for (uint32_t v = 0; v < num_variants; ++v) {
            // Create an empty UserFunctionVariant (no body, no params)
            UserFunctionVariant* ufv = new UserFunctionVariant(
                nullptr, 0, 0, QoreValue(), nullptr, false);

            bool has_varargs = false;
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    ufv, has_varargs, error)) {
                // variant ownership transfers to addPendingVariant or cleanup
                ufv->deref();
                // function can't be deleted directly; add it to namespace empty
                ns_list[ns_idx]->func_list.add(func, ns_list[ns_idx]);
                return false;
            }

            // Note: QCF_USES_EXTRA_ARGS flag is handled by the overridden hasVarargs()
            // method which checks signature.hasVarargs() directly

            if (flags & 0x0001) {
                ufv->setModulePublic();
            }

            // Add variant to function via the parse-time API, then commit
            func->addPendingVariant(ufv);
        }

        // Commit all pending variants to the committed list
        func->parseCommit();

        // Add function to namespace
        ns_list[ns_idx]->func_list.add(func, ns_list[ns_idx]);

        printd(5, "AOT deser: created function '%s' with %d variant(s)\n", name, num_variants);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::deserializeMethods(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::METHODS);
    if (!sec) {
        return true;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid METHODS section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t class_idx = QoreAOTBinaryReader::readU32(ptr);
        const char* method_name = reader.readStringRef(ptr);
        uint8_t is_static = QoreAOTBinaryReader::readU8(ptr);
        uint32_t num_variants = QoreAOTBinaryReader::readU32(ptr);

        if (class_idx >= class_list.size() || !class_list[class_idx]) {
            error = "invalid class index for method '" + std::string(method_name ? method_name : "") + "'";
            return false;
        }

        QoreClass* qc = class_list[class_idx];
        bool skip_class = preexisting_classes.count(class_idx) > 0;

        for (uint32_t v = 0; v < num_variants; ++v) {
            // Read method-specific fields: access + flags
            uint8_t access = QoreAOTBinaryReader::readU8(ptr);
            uint8_t mflags = QoreAOTBinaryReader::readU8(ptr);
            bool is_final = (mflags & 0x01) != 0;
            bool is_abstract = (mflags & 0x02) != 0;

            // Create the correct variant type for special methods:
            // constructor → UserConstructorVariant, destructor → UserDestructorVariant,
            // copy → UserCopyVariant, everything else → UserMethodVariant.
            // This is critical because the runtime dispatches through type-specific
            // virtual methods (evalConstructor, evalDestructor, evalCopy) via
            // reinterpret_cast from the base MethodVariant pointer.
            bool is_constructor = method_name && strcmp(method_name, "constructor") == 0;
            bool is_destructor = method_name && strcmp(method_name, "destructor") == 0;
            bool is_copy = method_name && strcmp(method_name, "copy") == 0;

            MethodVariantBase* mvb;
            if (is_constructor) {
                mvb = new UserConstructorVariant(
                    static_cast<ClassAccess>(access),
                    nullptr, 0, 0, QoreValue(), nullptr, QCF_NO_FLAGS);
            } else if (is_destructor) {
                mvb = new UserDestructorVariant(nullptr, 0, 0);
            } else if (is_copy) {
                mvb = new UserCopyVariant(
                    static_cast<ClassAccess>(access),
                    nullptr, 0, 0, QoreValue(), nullptr, false);
            } else {
                mvb = new UserMethodVariant(
                    static_cast<ClassAccess>(access), is_final,
                    nullptr, 0, 0, QoreValue(), nullptr, false,
                    QCF_NO_FLAGS, is_abstract);
            }

            bool has_varargs = false;
            UserVariantBase* umv = dynamic_cast<UserVariantBase*>(mvb);
            assert(umv);
            if (!readAndSetupVariantSignature(reader, type_resolver, pgm, ptr, end,
                    umv, has_varargs, error, qc)) {
                delete mvb;
                return false;
            }

            // Skip methods for classes that already existed from module loading
            // — they're already committed with all their methods
            if (skip_class) {
                delete mvb;
                continue;
            }

            // Note: QCF_USES_EXTRA_ARGS flag is handled by the overridden hasVarargs()
            // method which checks signature.hasVarargs() directly

            // Add method to class
            qore_class_private::addUserMethod(*qc, method_name, mvb, is_static != 0);
        }

        printd(5, "AOT deser: %s method '%s::%s' (%s) with %d variant(s)\n",
            skip_class ? "skipped preexisting" : "created",
            qc->getName(), method_name, is_static ? "static" : "instance", num_variants);
    }

    return true;
}

bool QoreAOTBinaryDeserializer::importInheritedMembers(std::string& error) {
    // Import inherited members from base classes into newly deserialized classes.
    // During normal parsing, BCNode::initializeMembers() calls parseImportMembers()
    // to copy base class members into the derived class's member map. AOT deserialization
    // skips this step, so derived classes can't access inherited members at runtime.
    for (size_t i = 0; i < class_list.size(); ++i) {
        if (preexisting_classes.count(i)) {
            continue;  // already fully initialized from module loading
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        // initializeMembers() checks parse_resolve_class_members flag to avoid re-initialization,
        // iterates base class list, and calls parseImportMembers() for each base class
        priv->initializeMembers();
        printd(5, "AOT deser: imported inherited members for class '%s'\n", qc->getName());
    }
    return true;
}

bool QoreAOTBinaryDeserializer::commitDeserializedClasses(std::string& error) {
    // Commit all newly deserialized classes (set initialized + commit pending method variants)
    for (size_t i = 0; i < class_list.size(); ++i) {
        if (preexisting_classes.count(i)) {
            continue;  // already initialized and committed
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        // Signatures already resolved by readAndSetupVariantSignature — just set initialized
        priv->initialized = true;
        // Commits all pending method variants (hm, shm maps); handles base-class recursion
        priv->parseCommit();
        printd(5, "AOT deser: committed class '%s' constructor=%p hm.size=%d\n",
            qc->getName(), (void*)priv->constructor, (int)priv->hm.size());
    }
    return true;
}

bool QoreAOTBinaryDeserializer::deserializeFallbackSources(std::string& error) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNC_SOURCES);
    if (!sec) {
        return true;  // no fallback sources needed — all functions fully serialized
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNC_SOURCES section data";
        return false;
    }
    const uint8_t* end = ptr + sec->size;

    // Read the full source text reference
    const char* src = reader.readStringRef(ptr);
    if (src && *src) {
        fallback_source = src;
        fallback_source_len = strlen(src);
    }

    // Read fallback function names
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    fallback_func_names.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const char* name = reader.readStringRef(ptr);
        if (name) {
            fallback_func_names.emplace_back(name);
        }
    }

    printd(2, "AOT: loaded fallback source (%d bytes) for %d function(s)\n",
        static_cast<int>(fallback_source_len), static_cast<int>(fallback_func_names.size()));

    return true;
}

bool serializeNamespaceTree(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
        const char* module_name, const std::unordered_set<std::string>* keep_modules) {
    // Phase 1: Collect all user-defined items into indexed vectors
    // When module_name is provided, filter out items from reexported dependencies
    // When keep_modules is provided, items from those modules are always included
    printd(5, "serializeNamespaceTree: module_name='%s' root_ns='%s'\n",
        module_name ? module_name : "n/a", root_ns->ns->getName());
    AOTSerializeState state;
    collectItems(state, root_ns, UINT32_MAX, module_name, keep_modules);

    // Phase 2: Write each section
    writeNamespacesSection(writer, state);
    writeClassesSection(writer, state);
    writeHashDeclsSection(writer, state);
    writeEnumsSection(writer, state);
    writeTypedefsSection(writer, state);
    writeConstantsSection(writer, state);
    writeGlobalsSection(writer, state);
    writeFunctionsSection(writer, state);
    writeMethodsSection(writer, state);

    return true;
}

void serializeDependencies(QoreAOTBinaryWriter& writer, const std::vector<std::string>& dependencies) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::DEPENDENCIES);

    uint32_t count = static_cast<uint32_t>(dependencies.size());
    writer.writeU32(count);

    for (const auto& dep : dependencies) {
        writer.writeStringRef(dep.c_str());
    }

    writer.endSection(sec_idx);
}

bool readDependencies(const uint8_t* data, uint32_t size, std::vector<std::string>& dependencies, std::string& error) {
    // Open the binary to read just the dependencies section
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    // Find DEPENDENCIES section
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::DEPENDENCIES);
    if (!sec) {
        // No dependencies section - this is OK, just means no deps
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid DEPENDENCIES section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    dependencies.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* dep_name = reader.readStringRef(ptr);
        if (!dep_name) {
            error = "invalid dependency name at index " + std::to_string(i);
            return false;
        }
        dependencies.push_back(dep_name);
    }

    return true;
}

void serializeReexportModules(QoreAOTBinaryWriter& writer, const std::vector<std::string>& reexport_modules) {
    if (reexport_modules.empty()) {
        return;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::REEXPORT_MODULES);

    uint32_t count = static_cast<uint32_t>(reexport_modules.size());
    writer.writeU32(count);

    for (const auto& mod : reexport_modules) {
        writer.writeStringRef(mod.c_str());
    }

    writer.endSection(sec_idx);
}

bool readReexportModules(const uint8_t* data, uint32_t size, std::vector<std::string>& reexport_modules,
        std::string& error) {
    // Open the binary to read the reexport modules section
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    // Find REEXPORT_MODULES section
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::REEXPORT_MODULES);
    if (!sec) {
        // No reexport modules section - this is OK, just means no reexports
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid REEXPORT_MODULES section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    // Sanity check: each entry needs at least 4 bytes (string ref), so count can't exceed section size
    uint32_t max_entries = sec->size / 4;
    if (count > max_entries) {
        error = "reexport module count " + std::to_string(count) + " exceeds section capacity";
        return false;
    }
    reexport_modules.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        const char* mod_name = reader.readStringRef(ptr);
        if (!mod_name) {
            error = "invalid reexport module name at index " + std::to_string(i);
            return false;
        }
        reexport_modules.push_back(mod_name);
    }

    return true;
}

void serializeProgramMetadata(QoreAOTBinaryWriter& writer, const char* exec_class_name) {
    // Only create the section if there's metadata to write
    if (!exec_class_name || !*exec_class_name) {
        return;
    }

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::PROGRAM_METADATA);

    // Write exec-class flag (u8) and name (string ref)
    writer.writeU8(1);  // has exec-class
    writer.writeStringRef(exec_class_name);

    writer.endSection(sec_idx);
}

bool readProgramMetadata(const uint8_t* data, uint32_t size, std::string& exec_class_name,
        std::string& error) {
    exec_class_name.clear();

    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::PROGRAM_METADATA);
    if (!sec) {
        // No program metadata section — this is OK (older binaries won't have it)
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid PROGRAM_METADATA section data";
        return false;
    }

    uint8_t has_exec_class = QoreAOTBinaryReader::readU8(ptr);
    if (has_exec_class) {
        const char* name = reader.readStringRef(ptr);
        if (name && *name) {
            exec_class_name = name;
        }
    }

    return true;
}

bool readFallbackSource(const uint8_t* data, uint32_t size, const char*& source, size_t& source_len,
        std::string& error) {
    source = nullptr;
    source_len = 0;

    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::FUNC_SOURCES);
    if (!sec) {
        // No fallback sources section - this is OK
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid FUNC_SOURCES section data";
        return false;
    }

    // Read the full source text reference (first item in FUNC_SOURCES section)
    const char* src = reader.readStringRef(ptr);
    if (src && *src) {
        source = src;
        source_len = strlen(src);
    }

    return true;
}
