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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include <qore/intern/ParseReferenceNode.h>
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include <qore/QoreEnumDecl.h>

#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreAOTInstRegistry.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"

#include <cassert>
#include <cstring>
#include <deque>
#include <numeric>
#include <unordered_set>
#include <zlib.h>

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

// Forward-reference class lookup map used by readValue VT_NEW_OBJECT during
// AOT class deserialization. The deserializer populates this as each class is
// added to its namespace so that instance-member init expressions of the form
// `OtherClass m()` can resolve the target class even before it has been
// committed into the root namespace's clmap (which happens after the full
// classes pass). The pointer is installed in an RAII scope from
// deserializeClasses() and cleared on exit.
static thread_local const std::unordered_map<std::string, QoreClass*>*
    g_aot_pending_class_map = nullptr;

// Read an instance-member / static-member default value.
//
// If the value is a VT_NEW_OBJECT whose target class is not yet registered in
// the program or in the in-progress class map, defer the class lookup: read
// the class path + arg values into `pending_class_path` + `pending_args` so
// the second pass (after all classes are committed) can resolve the class
// and construct the ScopedObjectCallNode. Otherwise, read normally and
// return the QoreValue via `default_val`.
//
// Returns true on success, false on malformed data.
template <class Pending>
static bool readDeferredMemberDefault(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        std::string& error,
        QoreValue& default_val,
        Pending& pim) {
    if (ptr >= end) {
        error = "unexpected end of data reading member default tag";
        return false;
    }
    const uint8_t* save = ptr;
    uint8_t tag_byte = *ptr;
    if (tag_byte != static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_OBJECT)) {
        // Normal case: use the standard reader
        default_val = reader.readValue(ptr, end, error);
        return error.empty();
    }
    // VT_NEW_OBJECT: try resolving the class now, deferring to a pending
    // entry on the PendingInstanceMember / PendingStaticMember if not yet
    // registered.
    ++ptr;  // consume tag
    if (ptr + 8 > end) {
        error = "unexpected end of data reading new_object class path";
        return false;
    }
    (void)QoreAOTBinaryReader::readU32(ptr);  // path_len (unused — using string pool)
    uint32_t path_offset = QoreAOTBinaryReader::readU32(ptr);
    const char* class_path = reader.getString(path_offset);
    if (!class_path) {
        error = "invalid string offset for new_object class path";
        return false;
    }
    if (ptr + 4 > end) {
        error = "unexpected end of data reading new_object arg count";
        return false;
    }
    uint32_t nargs = QoreAOTBinaryReader::readU32(ptr);

    // Read constructor arguments regardless — args don't forward-reference
    // classes in practice (they're usually simple constant values).
    std::vector<QoreValue> args;
    args.reserve(nargs);
    for (uint32_t i = 0; i < nargs; ++i) {
        QoreValue arg = reader.readValue(ptr, end, error);
        if (!error.empty()) {
            for (auto& v : args) v.discard(nullptr);
            return false;
        }
        args.push_back(arg);
    }

    // Try to resolve class: first in committed program, then in the
    // in-progress class map populated during deserializeClasses.
    const QoreClass* qc = nullptr;
    {
        ExceptionSink xs;
        qc = getProgram()->findClass(class_path, &xs);
        if (xs.isException()) {
            xs.clear();
        }
    }
    if (!qc && g_aot_pending_class_map) {
        auto it = g_aot_pending_class_map->find(class_path);
        if (it != g_aot_pending_class_map->end()) {
            qc = it->second;
        }
    }
    if (qc) {
        // Resolvable now: construct ScopedObjectCallNode immediately.
        QoreParseListNode* parse_args = nullptr;
        if (nargs > 0) {
            parse_args = new QoreParseListNode(&loc_builtin);
            for (auto& v : args) {
                parse_args->add(v, &loc_builtin);
            }
            args.clear();
        }
        ScopedObjectCallNode* socn = new ScopedObjectCallNode(
            &loc_builtin, qc, parse_args);
        if (parse_args) {
            socn->resolveParseArgs();
        }
        default_val = QoreValue(socn);
    } else {
        // Defer: store class path + args; second pass resolves.
        pim.pending_new_class_path = class_path;
        pim.pending_new_args = std::move(args);
        default_val = QoreValue();
    }
    (void)save;  // save no longer needed — ptr correctly advanced
    return true;
}

// Resolve a constant FQN (e.g. "Reflection::AutoHashType" or
// "Some::Class::MEMBER") to its ConstantEntry via the given program, falling
// back to a class-constant lookup when the name isn't a namespace constant.
// Returns nullptr on failure. Used by AOT load paths that rebuild AST nodes
// referencing program/class constants.
static ConstantEntry* aot_resolve_constant_by_fqn(QoreProgram* pgm, const char* fqn) {
    if (!pgm || !fqn || !*fqn) {
        return nullptr;
    }
    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* cns = nullptr;
    ConstantEntry* ce = const_cast<ConstantEntry*>(
        qore_root_ns_private::runtimeFindNamespaceConstant(*pp->RootNS, fqn, cns));
    if (ce) {
        return ce;
    }
    // Try class constant lookup: path format "ClassName::ConstName"
    std::string path(fqn);
    size_t sep = path.rfind("::");
    if (sep == std::string::npos || sep == 0) {
        return nullptr;
    }
    std::string class_path = path.substr(0, sep);
    std::string const_name = path.substr(sep + 2);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path.c_str(), found_ns);
    if (!qc) {
        return nullptr;
    }
    return const_cast<ConstantEntry*>(
        qore_class_private::get(*qc)->constlist.findEntry(const_name.c_str()));
}

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
    // For otherwise-unserializable pointer-backed values (e.g. a QoreObject
    // produced by parse-time constant folding of `{"int": IntType}`), check
    // whether the node pointer is registered in the program reverse map.
    // If so, write as VT_CONST_REF so the reference can be resolved at load
    // time. This is skipped for container types (list/hash) and scope refs
    // which have their own dedicated encodings below; checking only the
    // types that would otherwise fall into the default-NOTHING case avoids
    // accidentally rewriting a constant's own top-level value as a
    // self-reference (only leaf objects make it here).
    if (const_reverse_map && v.hasNode()) {
        qore_type_t nt = v.getType();
        if (nt == NT_OBJECT) {
            const AbstractQoreNode* node = v.getInternalNode();
            auto it = const_reverse_map->find(node);
            if (it != const_reverse_map->end()) {
                writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_CONST_REF));
                writeU32(static_cast<uint32_t>(it->second.size()));
                writeStringRef(it->second.c_str(), it->second.size());
                return true;
            }
        }
    }
    // Must check isEnum() before getType() because getType() on TAG_ENUM
    // returns the base type (e.g., NT_INT), which would serialize the wrong thing
    if (v.isEnum()) {
        writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_ENUM));
        const QoreEnumMember* member = v.getEnumMember();
        std::string path = member->getEnumDecl()->getNamespacePath();
        writeU32(static_cast<uint32_t>(path.size()));
        writeStringRef(path.c_str(), path.size());
        const char* name = member->getName();
        uint32_t name_len = static_cast<uint32_t>(strlen(name));
        writeU32(name_len);
        writeStringRef(name, name_len);
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
        case NT_SCOPE_REF: {
            // NT_SCOPE_REF is shared by ScopedObjectCallNode, NewHashDeclNode,
            // NewComplexListNode, NewComplexHashNode — must use dynamic_cast
            const AbstractQoreNode* node = v.getInternalNode();
            const ScopedObjectCallNode* socn = dynamic_cast<const ScopedObjectCallNode*>(node);
            if (socn && socn->oc) {
                std::string class_path = socn->oc->getNamespacePath();
                const QoreListNode* args = socn->getArgs();
                uint32_t nargs = args ? static_cast<uint32_t>(args->size()) : 0;

                // Only serialize if all args are serializable concrete values
                bool args_ok = true;
                for (uint32_t i = 0; i < nargs; ++i) {
                    QoreValue arg = args->retrieveEntry(i);
                    // Check that args are concrete value types we can serialize
                    if (arg.getType() == NT_SCOPE_REF || arg.getType() == NT_FUNCTION_CALL
                            || arg.getType() == NT_SELF_VARREF || arg.getType() == NT_VARREF) {
                        args_ok = false;
                        break;
                    }
                }

                if (args_ok) {
                    writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NEW_OBJECT));
                    writeU32(static_cast<uint32_t>(class_path.size()));
                    writeStringRef(class_path.c_str(), class_path.size());
                    writeU32(nargs);
                    for (uint32_t i = 0; i < nargs; ++i) {
                        writeValue(args->retrieveEntry(i));
                    }
                    return true;
                }
            }
            // Fall through to default if not serializable
            writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_NOTHING));
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
    // Bytes 34: compression, byte 35: reserved (separate writes to preserve layout)
    writeU8LE(header.compression);
    writeU8LE(header.reserved);
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
    header.compression = readU8(ptr);
    header.reserved = readU8(ptr);
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

    // Handle decompression if needed (before reading section directory and string pool)
    // When compressed, the entire post-header region is compressed
    if (header.compression == 1) {
        const uint8_t* compressed_start = data + header_size;
        size_t compressed_len = in_size - header_size;
        std::string decomp_error;
        if (!decompressMetadata(compressed_start, compressed_len, decompressed_body, decomp_error)) {
            error = "failed to decompress metadata: " + decomp_error;
            return false;
        }
        // Use decompressed data as our working buffer
        // IMPORTANT: After decompression, ptr and data must be consistent!
        // ptr was pointing into section directory, so reset it relative to new buffer
        data = decompressed_body.data();
        total_size = static_cast<uint32_t>(decompressed_body.size());
        ptr = data;  // Reset ptr to start of decompressed data (start of section directory)
    }

    // Read section directory
    uint32_t section_dir_size = header.section_count * sizeof(QoreAOTSectionHeader);
    uint32_t needed = header_size + section_dir_size;
    if (total_size < needed) {
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

        case QoreAOTValueTag::VT_ENUM: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading enum path";
                return QoreValue();
            }
            uint32_t path_len = readU32(ptr);
            uint32_t path_offset = readU32(ptr);
            const char* path = getString(path_offset);
            if (!path) {
                error = "invalid string offset for enum path";
                return QoreValue();
            }
            if (ptr + 8 > end) {
                error = "unexpected end of data reading enum member name";
                return QoreValue();
            }
            uint32_t name_len = readU32(ptr);
            uint32_t name_offset = readU32(ptr);
            const char* member_name = getString(name_offset);
            if (!member_name) {
                error = "invalid string offset for enum member name";
                return QoreValue();
            }
            const QoreNamespace* pns = nullptr;
            const QoreEnumDecl* ed = getProgram()->findEnum(path, pns);
            if (!ed) {
                error = std::string("enum not found: ") + path;
                return QoreValue();
            }
            const QoreEnumMember* member = ed->findMember(member_name);
            if (!member) {
                error = std::string("enum member not found: ") + std::string(path) + "::" + member_name;
                return QoreValue();
            }
            return QoreValue::makeEnum(member);
        }

        case QoreAOTValueTag::VT_NEW_OBJECT: {
            if (ptr + 8 > end) {
                error = "unexpected end of data reading new_object class path";
                return QoreValue();
            }
            uint32_t path_len = readU32(ptr);
            uint32_t path_offset = readU32(ptr);
            const char* class_path = getString(path_offset);
            if (!class_path) {
                error = "invalid string offset for new_object class path";
                return QoreValue();
            }
            if (ptr + 4 > end) {
                error = "unexpected end of data reading new_object arg count";
                return QoreValue();
            }
            uint32_t nargs = readU32(ptr);

            // Read constructor arguments
            QoreParseListNode* parse_args = nullptr;
            if (nargs > 0) {
                parse_args = new QoreParseListNode(&loc_builtin);
                for (uint32_t i = 0; i < nargs; ++i) {
                    QoreValue arg = readValue(ptr, end, error);
                    if (!error.empty()) {
                        delete parse_args;
                        return QoreValue();
                    }
                    parse_args->add(arg, &loc_builtin);
                }
            }

            // Resolve the class. Try the program's committed-class map
            // first; fall back to the deserializer's in-progress class map
            // for forward references (e.g. `OtherClass m();` in a class that
            // is deserialized before `OtherClass`). The in-progress map is
            // installed by deserializeClasses() via g_aot_pending_class_map.
            ExceptionSink xsink;
            const QoreClass* qc = getProgram()->findClass(class_path, &xsink);
            if (xsink.isException()) {
                xsink.clear();
            }
            if (!qc && g_aot_pending_class_map) {
                auto it = g_aot_pending_class_map->find(class_path);
                if (it != g_aot_pending_class_map->end()) {
                    qc = it->second;
                }
            }
            if (!qc) {
                printd(0, "AOT readValue VT_NEW_OBJECT: cannot resolve class '%s'\n",
                    class_path ? class_path : "(null)");
                delete parse_args;
                return QoreValue();
            }
            ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, parse_args);
            // Convert parse_args to args so evalImpl() doesn't hit the assertion
            if (parse_args) {
                socn->resolveParseArgs();
            }
            return QoreValue(socn);
        }

        case QoreAOTValueTag::VT_CONST_REF: {
            // Written as: FQN string (length + string pool offset).
            // At load time, resolve the referenced constant from the current
            // program's namespace tree and return its referenced value. Used
            // for objects and other unserializable values that live inside
            // parse-time-folded hash/list literals — the parser inlines the
            // constant's value into the literal, and the writer detects the
            // shared node pointer via the program reverse map.
            if (ptr + 8 > end) {
                error = "unexpected end of data reading const_ref name";
                return QoreValue();
            }
            uint32_t name_len = readU32(ptr);
            uint32_t name_offset = readU32(ptr);
            const char* fqn = getString(name_offset);
            if (!fqn) {
                error = "invalid string offset for const_ref name";
                return QoreValue();
            }
            QoreProgram* pgm = getProgram();
            if (!pgm) {
                error = "no current program for const_ref resolution";
                return QoreValue();
            }
            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* cns = nullptr;
            const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(
                *pp->RootNS, fqn, cns);
            if (!ce) {
                // Try class constant lookup: path format "ClassName::ConstName"
                std::string path(fqn);
                size_t sep = path.rfind("::");
                if (sep != std::string::npos && sep > 0) {
                    std::string class_path = path.substr(0, sep);
                    std::string const_name = path.substr(sep + 2);
                    const qore_ns_private* found_ns = nullptr;
                    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                        *pp->RootNS, class_path.c_str(), found_ns);
                    if (qc) {
                        ce = qore_class_private::get(*qc)->constlist.findEntry(
                            const_name.c_str());
                    }
                }
            }
            if (!ce) {
                printd(0, "AOT readValue: cannot resolve const_ref '%s'\n", fqn);
                return QoreValue();
            }
            return ce->getReferencedValue();
        }

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

//! Normalise a complex type path string for runtime parsing.
//!
//! QoreTypeInfo::getPath() for user hashdecls returns fully-qualified names
//! with a leading "::", e.g. "hash<::DataProvider::DataProviderExpressionInfo>".
//! When the parser-based runtime resolver walks this into a NamedScope, the
//! leading "::" becomes an empty first component, and
//! qore_root_ns_private::runtimeFindHashDeclIntern(NamedScope) iterates
//! nsmap looking for a namespace whose name is "" — which never matches, so
//! the hashdecl is not found and the outer cast fails with IR-CAST-ERROR.
//!
//! This normalizer strips any "::" prefix from identifier components inside
//! complex type paths (after "<" or ", "), which makes the nested hashdecl
//! lookup succeed. It is a string-level fix contained to the AOT resolver
//! path so shared runtime name-lookup code remains unchanged.
static std::string normalize_aot_type_path(const char* path) {
    std::string out;
    if (!path) {
        return out;
    }
    out.reserve(strlen(path));
    size_t i = 0;
    size_t n = strlen(path);
    // Handle a possible leading "*::" / "::" on the outer type path.
    if (n >= 2 && path[0] == ':' && path[1] == ':') {
        i += 2;
    } else if (n >= 3 && path[0] == '*' && path[1] == ':' && path[2] == ':') {
        out.push_back('*');
        i += 3;
    }
    while (i < n) {
        char c = path[i];
        out.push_back(c);
        ++i;
        // After "<" or "," (possibly followed by space), strip a "::" prefix.
        bool at_arg_start = (c == '<');
        if (c == ',' && i < n && path[i] == ' ') {
            out.push_back(' ');
            ++i;
            at_arg_start = true;
        } else if (c == ',') {
            at_arg_start = true;
        }
        if (at_arg_start && i + 1 < n && path[i] == ':' && path[i + 1] == ':') {
            i += 2;
        }
    }
    return out;
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
    // Normalize the path to strip leading "::" on nested hashdecl/enum refs
    // (see the normaliser comment for the rationale).
    std::string norm_path = normalize_aot_type_path(path);
    const char* use_path = norm_path.c_str();
    if (pgm) {
        ExceptionSink xsink;
        ProgramRuntimeParseAccessHelper pah(&xsink, pgm);
        if (!xsink) {
            return qore_get_type_from_string_intern(use_path);
        }
    }
    return qore_get_type_from_string_intern(use_path);
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
    qore_ns_private* root_ns = nullptr;  // program root namespace (for program-wide CRM)

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
                    || dt == NT_HASH || dt == NT_OBJECT) {
                // Note: NT_OBJECT is routed through writeValue so the writer's
                // CRM lookup can emit VT_CONST_REF for parse-folded Type-class
                // constants (e.g. `*Type t = IntType` default args).
                writer.writeU8(1);
                writer.writeValue(dv);
            } else if (dv.hasNode()) {
                const AbstractQoreNode* node = dv.getInternalNode();

                // Helper: resolve the FQN of a constant referenced by node
                // chain (either a directly-registered CRM pointer, or a
                // RuntimeConstantRefNode wrapping a ConstantEntry whose
                // val/saved_val node is in the CRM).
                auto resolveConstFqn = [&writer](const AbstractQoreNode* n) -> std::string {
                    if (!writer.const_reverse_map || !n) {
                        return std::string();
                    }
                    auto it = writer.const_reverse_map->find(n);
                    if (it != writer.const_reverse_map->end()) {
                        return it->second;
                    }
                    if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(n)) {
                        ConstantEntry* ce = rcr->getConstantEntry();
                        if (ce) {
                            if (ce->val.hasNode()) {
                                auto it2 = writer.const_reverse_map->find(ce->val.getInternalNode());
                                if (it2 != writer.const_reverse_map->end()) {
                                    return it2->second;
                                }
                            }
                            QoreValue sv = ce->getReferencedValue();
                            std::string rv;
                            if (sv.hasNode()) {
                                auto it2 = writer.const_reverse_map->find(sv.getInternalNode());
                                if (it2 != writer.const_reverse_map->end()) {
                                    rv = it2->second;
                                }
                            }
                            sv.discard(nullptr);
                            return rv;
                        }
                    }
                    return std::string();
                };

                // Case 1: no-arg function call default (e.g., getcwd(), now())
                auto* fcn = dynamic_cast<const FunctionCallNode*>(node);
                if (fcn && fcn->getName() && (!fcn->getArgs() || fcn->getArgs()->empty())) {
                    writer.writeU8(2);  // expression default: function call
                    writer.writeStringRef(fcn->getName());
                    continue;
                }

                // Case 2: RuntimeConstantRefNode — a plain constant reference.
                //   e.g. `hash<auto> options = DefaultsMap`. At call time we want
                //   to return the constant's current value, so serialize the
                //   constant's FQN and rebuild a RuntimeConstantRefNode at load.
                if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
                    (void)rcr;  // silence unused-if-no-CRM warning
                    std::string fqn = resolveConstFqn(node);
                    if (!fqn.empty()) {
                        writer.writeU8(3);  // expression default: constant ref
                        writer.writeStringRef(fqn.c_str());
                        continue;
                    }
                }

                // Case 3: QoreDotEvalOperatorNode — method call on a constant,
                //   e.g. `AutoHashType.getName()`. We only support the no-arg
                //   form with a constant-ref left-hand side. The reader builds
                //   a fresh QoreDotEvalOperatorNode whose `left` is a new
                //   RuntimeConstantRefNode — the method is then resolved by
                //   dynamic dispatch at each call.
                if (auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
                    MethodCallNode* mc = de->getMethodCall();
                    if (mc && mc->getName() && (!mc->getArgs() || mc->getArgs()->empty())) {
                        QoreValue left = de->getExpression();
                        std::string fqn;
                        if (left.hasNode()) {
                            fqn = resolveConstFqn(left.getInternalNode());
                        }
                        if (!fqn.empty()) {
                            writer.writeU8(4);  // expression default: const.method()
                            writer.writeStringRef(fqn.c_str());
                            writer.writeStringRef(mc->getName());
                            continue;
                        }
                    }
                }

                // Fallback: unclassifiable complex expression — write opaque
                writer.writeU8(1);
                writer.writeU8(static_cast<uint8_t>(QoreAOTValueTag::VT_OPAQUE_DEFAULT));
            } else {
                // Unknown type — write opaque placeholder
                writer.writeU8(1);
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
            // Serialize the initial value. If the parser folded the init
            // expression to a concrete value (e.g. `static Type t = IntType`
            // where IntType is a reflection constant), we need to persist
            // that value so it survives AOT load. For unserializable values
            // (objects, closures), writeValue falls back to VT_CONST_REF via
            // the program reverse map when possible; otherwise NOTHING is
            // written and the static var will need an init function (which
            // is generated separately if the expression `needs_eval()`).
            if (vi.second->exp) {
                writer.writeU8(1);
                writer.writeValue(vi.second->exp);
            } else {
                writer.writeU8(0);
            }
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
                    if (ce->hasInitExpr()) {
                        // Class constants with init expressions get NOTHING placeholder
                        writer.writeValue(QoreValue());
                    } else {
                        // Use getReferencedValue() for the actual evaluated value
                        QoreValue actual_val = ce->getReferencedValue();
                        writer.writeValue(actual_val);
                        actual_val.discard(nullptr);
                    }
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
        if (ce->hasInitExpr()) {
            // Constants with init expressions will be initialized at runtime
            // by their lowered init function — serialize NOTHING as placeholder
            writer.writeValue(QoreValue());
        } else {
            // Use getReferencedValue() to get the actual evaluated value.
            // ce->val may hold a RuntimeConstantRefNode (NT_RTCONSTREF) which is
            // just a reference to the constant's evaluated saved_val.
            QoreValue actual_val = ce->getReferencedValue();
            writer.writeValue(actual_val);
            actual_val.discard(nullptr);
        }
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

//! Recursively build a reverse map from constant value node pointers to FQNs for all namespaces
//! Used for BCA serialization to resolve constants from the entire program, not just ancestor namespaces
static void buildProgramConstantReverseMapImpl(qore_ns_private* ns,
        AOTConstantReverseMap& crm) {
    if (!ns) {
        return;
    }

    // Add namespace constants
    ConstConstantListIterator nsi(ns->constant);
    while (nsi.next()) {
        QoreValue v = nsi.getValue();
        if (!v.hasNode()) {
            continue;
        }
        const AbstractQoreNode* node = v.getInternalNode();
        if (node && crm.find(node) == crm.end()) {  // Only add if not already present
            std::string ns_path = ns->path;
            if (ns_path.size() >= 2) {
                ns_path = ns_path.substr(2);  // strip leading "::"
            }
            std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
            crm.emplace(node, std::move(fqn));
        }
    }

    // Add class constants within this namespace
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        std::string class_prefix = std::string(qc->getPath() + 2) + "::";  // strip leading "::"
        ConstConstantListIterator cci(qore_class_private::get(*qc)->constlist);
        while (cci.next()) {
            QoreValue v = cci.getValue();
            if (!v.hasNode()) {
                continue;
            }
            const AbstractQoreNode* node = v.getInternalNode();
            if (node && crm.find(node) == crm.end()) {  // Only add if not already present
                std::string fqn = class_prefix + cci.getName();
                crm.emplace(node, std::move(fqn));
            }
        }
    }

    // Recurse into child namespaces
    for (auto& ni : ns->nsl.nsmap) {
        if (ni.second) {
            buildProgramConstantReverseMapImpl(qore_ns_private::get(*ni.second), crm);
        }
    }
}

//! Build a reverse map from constant value node pointers to names for a specific class
//! Includes the class's own constants and parent namespace constants
static AOTConstantReverseMap buildClassConstantReverseMap(const QoreClass* qc) {
    AOTConstantReverseMap crm;
    if (!qc) {
        return crm;
    }

    const qore_class_private* cls_priv = qore_class_private::get(*qc);
    std::string class_prefix = std::string(qc->getPath() + 2) + "::";  // strip leading "::"

    // Add class constants
    ConstConstantListIterator cci(cls_priv->constlist);
    while (cci.next()) {
        QoreValue v = cci.getValue();
        if (!v.hasNode()) {
            continue;
        }
        const AbstractQoreNode* node = v.getInternalNode();
        if (node) {
            std::string fqn = class_prefix + cci.getName();
            crm.emplace(node, std::move(fqn));
        }
    }

    // Add namespace constants from the class's enclosing namespace hierarchy
    if (cls_priv->ns) {
        const qore_ns_private* ns_priv = cls_priv->ns;
        while (ns_priv) {
            // Iterate namespace constants using the ConstantList directly
            ConstConstantListIterator nsi(ns_priv->constant);
            while (nsi.next()) {
                QoreValue v = nsi.getValue();
                if (!v.hasNode()) {
                    continue;
                }
                const AbstractQoreNode* node = v.getInternalNode();
                if (node && crm.find(node) == crm.end()) {  // Only add if not already present
                    std::string ns_path = ns_priv->path;
                    if (ns_path.size() >= 2) {
                        ns_path = ns_path.substr(2);  // strip leading "::"
                    }
                    std::string fqn = ns_path.empty() ? nsi.getName() : ns_path + "::" + nsi.getName();
                    crm.emplace(node, std::move(fqn));
                }
            }
            // Walk up to parent namespace
            ns_priv = ns_priv->parent;
        }
    }

    return crm;
}

//! Write METHODS section
static void writeMethodsSection(QoreAOTBinaryWriter& writer, const AOTSerializeState& state) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::METHODS);

    // Build program-wide constant reverse map for BCA arg serialization
    // This includes constants from ALL namespaces, not just the class's ancestors
    AOTConstantReverseMap program_crm;
    if (state.root_ns) {
        buildProgramConstantReverseMapImpl(state.root_ns, program_crm);
    }

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
            bool is_constructor = strcmp(method->getName(), "constructor") == 0;
            bool debug = getenv("QORE_AOT_DEBUG") != nullptr;
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

                    // Serialize BCA (Base Class Constructor Arguments) for constructors
                    if (is_constructor) {
                        const ConstructorMethodVariant* cmv = CONMV_const(mvb);
                        const BCAList* bcal = cmv->getBaseClassArgumentList();
                        if (bcal && !bcal->empty()) {
                            writer.writeU8(1);  // has_bca = true
                            writer.writeU16(static_cast<uint16_t>(bcal->size()));

                            // Build slot map from constructor's signature params
                            // Must use dynamic_cast due to multiple inheritance:
                            // UserConstructorVariant inherits both ConstructorMethodVariant
                            // (via MethodVariantBase -> AbstractQoreFunctionVariant) and
                            // UserVariantBase. reinterpret_cast gives wrong pointer offset.
                            const UserConstructorVariant* ucv =
                                dynamic_cast<const UserConstructorVariant*>(cmv);
                            const UserSignature* sig = ucv
                                ? const_cast<UserConstructorVariant*>(ucv)->getUserSignature()
                                : nullptr;
                            AOTSlotMap bca_slots;
                            if (sig) {
                                for (unsigned i = 0; i < sig->numParams(); ++i) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->lv[i])] = i;
                                }
                                // Also map selfid if present
                                if (sig->selfid) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->selfid)] =
                                        static_cast<int32_t>(sig->numParams());
                                }
                                // Also map argvid if present
                                if (sig->argvid) {
                                    bca_slots.local_slots[reinterpret_cast<const void*>(sig->argvid)] =
                                        static_cast<int32_t>(sig->numParams() + (sig->selfid ? 1 : 0));
                                }
                            }

                            for (const BCANode* bca : *bcal) {
                                // Write base class path for runtime resolution
                                const QoreClass* base_cls = nullptr;
                                if (bca->classid) {
                                    const qore_class_private* cls_priv =
                                        qore_class_private::get(*mi.method->getClass());
                                    if (cls_priv->scl) {
                                        ClassAccess access;
                                        base_cls = cls_priv->scl->getClass(
                                            bca->classid, access, true);
                                    }
                                }
                                writer.writeStringRef(base_cls ? base_cls->getPath() : "");

                                // Serialize args as individual EXPR_TREE blobs
                                const QoreListNode* args = bca->getArgs();
                                uint16_t num_args = args ? static_cast<uint16_t>(args->size()) : 0;
                                writer.writeU16(num_args);

                                for (uint16_t ai = 0; ai < num_args; ++ai) {
                                    QoreValue arg_val = args->retrieveEntry(ai);
                                    std::vector<uint8_t> blob;
                                    if (serializeExprTreeToBlob(arg_val, bca_slots, blob, debug, &program_crm)) {
                                        writer.writeU32(static_cast<uint32_t>(blob.size()));
                                        if (!blob.empty()) {
                                            writer.writeBytes(blob.data(),
                                                static_cast<uint32_t>(blob.size()));
                                        }
                                    } else {
                                        printd(0, "AOT: failed to serialize BCA arg %d for %s::%s\n",
                                            ai, mi.method->getClass()->getName(), method->getName());
                                        // Write empty blob to keep format consistent
                                        writer.writeU32(0);
                                    }
                                }
                            }
                        } else {
                            writer.writeU8(0);  // has_bca = false
                        }
                    }
                }
            }
        }
    }

    writer.endSection(sec_idx);
}

} // anonymous namespace

//! Lower a closure variant to IR for serialization
/** Follows the same pattern as buildContextForVariant() in QoreAOTRuntime.cpp.
    @param variant the closure variant to lower
    @return heap-allocated IR function, or nullptr on failure (caller owns)
*/
QoreIRFunction* lowerClosureForSerialization(const UserClosureVariant* variant) {
    StatementBlock* sb = const_cast<UserClosureVariant*>(variant)->getStatementBlock();
    if (!sb) {
        return nullptr;
    }

    auto* ir = new QoreIRFunction("<closure>");

    // Record pre-instantiated locals from signature
    const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();
    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->lv[i]));
        }
        if (sig->argvid) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->argvid));
        }
        if (sig->selfid) {
            ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->selfid));
        }
    }

    QoreIRBuilder builder(ir);
    auto* entry = ir->createBlock("entry");
    builder.setBlock(entry);

    QoreProgram* pgm = const_cast<UserClosureVariant*>(variant)->pgm;
    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string error;
    if (!lowering.lowerStatementBlock(sb, error)) {
        printd(2, "AOT: closure IR lowering failed: %s\n", error.c_str());
        delete ir;
        return nullptr;
    }

    // Ensure all blocks have terminators. Complex closures (switch statements
    // with break/fall-through) can leave merge blocks empty or unterminated.
    // Add ReturnNothing to any block that needs it.
    for (auto& block : ir->blocks) {
        if (block->instructions.empty()) {
            builder.setBlock(block.get());
            builder.createReturnNothing();
        } else if (!isTerminator(block->instructions.back()->opcode)) {
            builder.setBlock(block.get());
            builder.createReturnNothing();
        }
    }

    std::string verify_error;
    if (!QoreIRVerifier::verify(*ir, verify_error)) {
        printd(2, "AOT: closure IR verification failed: %s\n", verify_error.c_str());
        delete ir;
        return nullptr;
    }

    // Compile handler IRs for try/catch blocks inside the closure
    std::string handler_error;
    if (lowering.compileAllHandlerIRs(handler_error) < 0) {
        printd(2, "AOT: closure handler IR compilation failed: %s\n", handler_error.c_str());
    }

    // Collect body locals
    collectAllStatementLocals(sb, ir->all_body_locals);

    ir->computeSlotIdsAndEmbed();
    return ir;
}


//! Classify and write a QoreValue expression in AOTExprKind format
/** Used by handler IR serialization to classify expression nodes inline.
    Handles function calls, method calls, variable refs, constants, and enums.
    Returns true on success, false if the expression cannot be classified.
*/
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map) {
    if (!expr.hasNode()) {
        if (expr.isEnum()) {
            const QoreEnumMember* member = expr.getEnumMember();
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_ENUM));
            std::string ns_path = member->getEnumDecl()->getNamespacePath();
            writer.writeStringRef(ns_path.c_str());
            writer.writeStringRef(member->getName());
            return true;
        }
        // Handle inline primitive values
        switch (expr.getType()) {
            case NT_INT: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
                writer.writeI64(expr.getAsBigInt());
                return true;
            }
            case NT_FLOAT: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
                writer.writeF64(expr.getAsFloat());
                return true;
            }
            case NT_BOOLEAN: {
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BOOL));
                writer.writeU8(expr.getAsBool() ? 1 : 0);
                return true;
            }
            case NT_NOTHING:
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
                return true;
            case NT_NULL:
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NULL));
                return true;
            default:
                break;
        }
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
        return true;
    }

    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
        return true;
    }

    // FunctionCallNode: regular function call
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL));
        writer.writeStringRef(call->getName());
        return true;
    }

    // SelfFunctionCallNode: method call on self
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            writer.writeStringRef(qc ? qc->getPath() : "");
        } else {
            writer.writeStringRef("");
        }
        // Strip class prefix from method name if present (e.g., "LoggerWrapper::debug" → "debug")
        const char* mname = call->getName();
        const char* last_sep = strrchr(mname, ':');
        writer.writeStringRef((last_sep && last_sep > mname && *(last_sep - 1) == ':') ? last_sep + 1 : mname);
        return true;
    }

    // StaticMethodCallNode: static method call
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            writer.writeStringRef(qc ? qc->getPath() : "");
        } else {
            writer.writeStringRef("");
        }
        writer.writeStringRef(call->getName());
        // Serialize method args (must match read_expr_static_method_call format)
        const QoreListNode* args = call->getArgs();
        if (args && args->size() > 0) {
            writer.writeU8(static_cast<uint8_t>(args->size()));
            for (size_t j = 0; j < args->size(); ++j) {
                classifyAndWriteExpr(writer, args->retrieveEntry(j),
                    parent_locals, parent_globals, const_reverse_map);
            }
        } else {
            writer.writeU8(0);
        }
        return true;
    }

    // VarRefNewObjectNode: variable declaration with object constructor call
    // (e.g., "Foo f("arg1", var)") — MUST check before VarRefNode since it
    // inherits from VarRefNode and would be matched by the VarRefNode handler
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
            writer.writeStringRef(qc->getPath());
            const QoreListNode* args = vrn->getArgs();
            if (args && args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(args->size()));
                for (size_t j = 0; j < args->size(); ++j) {
                    classifyAndWriteExpr(writer, args->retrieveEntry(j),
                        parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
        // Non-class VarRefNewObjectNode (hashdecl, complex hash/list) — fall through
    }

    // VarRefNode: local and global variable references
    // Note: VarRefNewObjectNode inherits from VarRefNode, so must come AFTER above check
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_LOCAL || varref->getType() == VT_LOCAL_TS ||
                varref->getType() == VT_CLOSURE) {
            // Look up the local slot index using pointer identity first (handles
            // same-named variables in different scopes), then fall back to name
            const void* var_ptr = varref->ref.id;
            bool found = false;
            // First pass: match by pointer identity (exact match)
            if (var_ptr) {
                for (size_t i = 0; i < parent_locals.size(); ++i) {
                    if (parent_locals[i].local_var_ptr == var_ptr) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        found = true;
                        break;
                    }
                }
            }
            // Second pass: fall back to name match (for cases where pointer isn't available)
            if (!found) {
                for (size_t i = 0; i < parent_locals.size(); ++i) {
                    if (varref->getName() && parent_locals[i].name == varref->getName()) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                return true;
            }
        } else if (varref->getType() == VT_GLOBAL || varref->getType() == VT_THREAD_LOCAL) {
            Var* global_var = varref->ref.var;
            if (global_var) {
                // Find the global slot index by name
                for (size_t i = 0; i < parent_globals.size(); ++i) {
                    if (parent_globals[i].name == global_var->getName()) {
                        writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                        writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
            }
        }
    }

    // SelfVarrefNode: self variable reference
    if (auto* svn = dynamic_cast<const SelfVarrefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_VARREF));
        writer.writeStringRef(svn->str ? svn->str : "");
        return true;
    }

    // StaticClassVarRefNode: static class variable reference
    if (auto* sv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_VARREF));
        writer.writeStringRef(sv->qc.getName());
        writer.writeStringRef(sv->str.c_str());
        return true;
    }

    // QoreStringNode: string literal constant (e.g., "" as constructor arg)
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        writer.writeStringRef(str->c_str());
        return true;
    }

    // QoreNullNode: NULL constant
    if (dynamic_cast<const QoreNullNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NULL));
        return true;
    }

    // ScopedObjectCallNode: namespace-scoped constructor call (e.g., "new Ns::Foo(args)")
    // Used in inline IR context where the QoreIRNewObjectInstruction::expr holds this node.
    // Must come BEFORE NewObjectCallNode since they have different class hierarchies.
    if (auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        if (socn->oc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::SCOPED_NEW_OBJECT));
            writer.writeStringRef(socn->oc->getPath());
            // Try evaluated args first, fall back to parse args
            const QoreListNode* args = socn->getArgs();
            if (args && args->size() > 0 && args->size() <= 255) {
                writer.writeU8(static_cast<uint8_t>(args->size()));
                for (size_t j = 0; j < args->size(); ++j) {
                    classifyAndWriteExpr(writer, args->retrieveEntry(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else if (const QoreParseListNode* pargs = socn->getParseArgs()) {
                // Args not yet evaluated; serialize from parse-time expressions
                uint8_t num_args = pargs->size() <= 255 ? static_cast<uint8_t>(pargs->size()) : 0;
                writer.writeU8(num_args);
                for (uint8_t j = 0; j < num_args; ++j) {
                    classifyAndWriteExpr(writer, pargs->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewObjectCallNode: bare constructor call (e.g., "new Foo()")
    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
        const QoreClass* qc = no->getClass();
        writer.writeStringRef(qc ? qc->getPath() : "");
        // Serialize constructor args if available
        const QoreListNode* args = no->getArgs();
        if (args && args->size() > 0 && args->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(args->size()));
            for (size_t j = 0; j < args->size(); ++j) {
                classifyAndWriteExpr(writer, args->retrieveEntry(j), parent_locals, parent_globals, const_reverse_map);
            }
        } else if (const QoreParseListNode* pargs = no->getParseArgs()) {
            uint8_t num_args = pargs->size() <= 255 ? static_cast<uint8_t>(pargs->size()) : 0;
            writer.writeU8(num_args);
            for (uint8_t j = 0; j < num_args; ++j) {
                classifyAndWriteExpr(writer, pargs->get(j), parent_locals, parent_globals, const_reverse_map);
            }
        } else {
            writer.writeU8(0);
        }
        return true;
    }

    // QoreNumberNode: number literal constant
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NUMBER));
        QoreString str;
        num->toString(str);
        writer.writeStringRef(str.c_str());
        return true;
    }

    // BinaryNode: binary literal constant
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BINARY));
        std::string hex;
        const unsigned char* data = static_cast<const unsigned char*>(bin->getPtr());
        size_t len = bin->size();
        hex.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", data[i]);
            hex.append(buf);
        }
        writer.writeStringRef(hex.c_str());
        return true;
    }

    // QoreHashNode: already-evaluated hash (e.g., {}, {"key": "val"}) — serialize as HASH_LITERAL
    // Only handles string-keyed hashes with simple values for safety; empty hash is the common case.
    if (auto* qhn = dynamic_cast<const QoreHashNode*>(node)) {
        // If this is a hashdecl-typed hash, serialize as HASHDECL_NEW to preserve the type.
        // The reader creates a NewHashDeclNode which evaluates to a properly typed hash.
        const TypedHashDecl* qhd = qhn->getHashDecl();
        if (qhd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(qhd->getNamespacePath().c_str());
            if (qhn->empty()) {
                writer.writeU8(0);  // no args
            } else {
                // Non-empty hashdecl hash: serialize the hash contents as a single HASH_LITERAL arg
                writer.writeU8(1);  // 1 arg (the hash contents)
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
                writer.writeU8(static_cast<uint8_t>(qhn->size()));
                ConstHashIterator it(qhn);
                while (it.next()) {
                    writer.writeStringRef(it.getKey());
                    classifyAndWriteExpr(writer, it.get(), parent_locals, parent_globals, const_reverse_map);
                }
            }
            return true;
        }
        if (qhn->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            writer.writeU8(static_cast<uint8_t>(qhn->size()));
            ConstHashIterator it(qhn);
            while (it.next()) {
                writer.writeStringRef(it.getKey());
                classifyAndWriteExpr(writer, it.get(), parent_locals, parent_globals, const_reverse_map);
            }
            return true;
        }
    }

    // QoreListNode: already-evaluated list constant — serialize as LIST_LITERAL
    if (auto* qln = dynamic_cast<const QoreListNode*>(node)) {
        if (qln->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            writer.writeU8(static_cast<uint8_t>(qln->size()));
            for (size_t i = 0; i < qln->size(); ++i) {
                classifyAndWriteExpr(writer, qln->retrieveEntry(i), parent_locals, parent_globals, const_reverse_map);
            }
            return true;
        }
    }

    // QoreParseListNode: parse-time list literal with unevaluated elements
    // (e.g., list of hashdecl init expressions in constructor args)
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
        if (pln->size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            writer.writeU8(static_cast<uint8_t>(pln->size()));
            for (size_t i = 0; i < pln->size(); ++i) {
                classifyAndWriteExpr(writer, pln->get(i), parent_locals, parent_globals, const_reverse_map);
            }
            return true;
        }
    }

    // QoreParseHashNode: hash literal with runtime values (e.g., ("key": local_var))
    if (auto* phn = dynamic_cast<const QoreParseHashNode*>(node)) {
        const QoreParseHashNode::nvec_t& keys = phn->getKeys();
        const QoreParseHashNode::nvec_t& vals = phn->getValues();
        assert(keys.size() == vals.size());
        if (keys.size() <= 255) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            writer.writeU8(static_cast<uint8_t>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                // Keys are typically string constants
                QoreStringValueHelper key(keys[i]);
                writer.writeStringRef(key->c_str());
                classifyAndWriteExpr(writer, vals[i], parent_locals, parent_globals, const_reverse_map);
            }
            return true;
        }
        // Hash too large for u8 count — fall through to GENERIC_EVAL
    }

    // QoreHashObjectDereferenceOperatorNode: hash.key or hash{key} dereference chains
    // (e.g., oh.paths."/create".post — left=base, right=key, both recursively classified)
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_DEREF));
        classifyAndWriteExpr(writer, hd->getLeft(), parent_locals, parent_globals, const_reverse_map);
        classifyAndWriteExpr(writer, hd->getRight(), parent_locals, parent_globals, const_reverse_map);
        return true;
    }

    // ParseReferenceNode: \var lvalue reference — serialize inner lvalue expression
    if (auto* prn = dynamic_cast<const ParseReferenceNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_REF));
        classifyAndWriteExpr(writer, prn->getLVExp(), parent_locals, parent_globals, const_reverse_map);
        return true;
    }

    // NewHashDeclNode: hashdecl construction (e.g., <StatInfo>{"size": 1})
    if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
        if (nhd->hd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            writer.writeStringRef(nhd->hd->getNamespacePath().c_str());
            // Serialize constructor args (typically a single hash initializer)
            if (nhd->args && nhd->args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(nhd->args->size()));
                for (size_t j = 0; j < nhd->args->size(); ++j) {
                    classifyAndWriteExpr(writer, nhd->args->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewComplexHashNode: complex typed hash construction
    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (nch->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            writer.writeStringRef(QoreTypeInfo::getPath(nch->typeInfo));
            // Serialize constructor args
            if (nch->args && nch->args->size() > 0) {
                writer.writeU8(static_cast<uint8_t>(nch->args->size()));
                for (size_t j = 0; j < nch->args->size(); ++j) {
                    classifyAndWriteExpr(writer, nch->args->get(j), parent_locals, parent_globals, const_reverse_map);
                }
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // NewComplexListNode: complex typed list construction
    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (ncl->typeInfo) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            writer.writeStringRef(QoreTypeInfo::getPath(ncl->typeInfo));
            // Serialize constructor arg (single QoreValue)
            if (ncl->args.hasNode()) {
                writer.writeU8(1);
                classifyAndWriteExpr(writer, ncl->args, parent_locals, parent_globals, const_reverse_map);
            } else {
                writer.writeU8(0);
            }
            return true;
        }
    }

    // QoreHashDeclCastOperatorNode: cast<StatInfo>(hash)
    if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hdc->getCastTypeInfo());
        if (hd) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_HASHDECL));
            writer.writeStringRef(hd->getNamespacePath().c_str());
            writer.writeU8(hdc->isOrNothing() ? 1 : 0);
            // Serialize the inner expression being cast
            QoreValue inner = hdc->getExp();
            if (inner.hasNode()) {
                writer.writeU8(1);  // has inner expression
                classifyAndWriteExpr(writer, inner, parent_locals, parent_globals, const_reverse_map);
            } else {
                writer.writeU8(0);  // no inner expression (cast from nothing)
            }
            return true;
        }
    }

    // QoreComplexHashCastOperatorNode: cast<hash<string, int>>(hash)
    if (auto* chc = dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_HASH));
        writer.writeStringRef(QoreTypeInfo::getPath(chc->getCastTypeInfo()));
        writer.writeU8(chc->isOrNothing() ? 1 : 0);
        return true;
    }

    // QoreComplexListCastOperatorNode: cast<list<int>>(list)
    if (auto* clc = dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_LIST));
        writer.writeStringRef(QoreTypeInfo::getPath(clc->getCastTypeInfo()));
        writer.writeU8(clc->isOrNothing() ? 1 : 0);
        return true;
    }

    // QoreClassCastOperatorNode: cast<ClassName>(obj)
    if (auto* cc = dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(cc->getCastTypeInfo());
        if (qc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_CLASS));
            writer.writeStringRef(qc->getPath());
            writer.writeU8(cc->isOrNothing() ? 1 : 0);
            return true;
        }
    }

    // QoreEnumCastOperatorNode: cast<EnumType>(val)
    if (auto* ec = dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
        writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_ENUM));
        writer.writeStringRef(QoreTypeInfo::getPath(ec->getCastTypeInfo()));
        writer.writeU8(ec->isOrNothing() ? 1 : 0);
        return true;
    }

    // QoreClosureParseNode: closure/lambda in expression context (e.g., hash literal values)
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        UserClosureFunction* ucf = closure->getFunction();
        if (ucf) {
            auto* variant = static_cast<const UserClosureVariant*>(ucf->first());
            if (variant) {
                const UserSignature* sig = const_cast<UserClosureVariant*>(variant)->getUserSignature();
                writer.writeU8(static_cast<uint8_t>(AOTExprKind::CLOSURE_CREATE));

                // Write flags: "lambda,in_method" (matches write_slot_CLOSURE_CREATE ref1 format)
                std::string flags = std::string(closure->isLambda() ? "1" : "0") + ","
                    + (closure->isInMethod() ? "1" : "0");
                writer.writeStringRef(flags.c_str());

                // Write class type path (ref2)
                const QoreTypeInfo* cti = ucf->getClassType();
                writer.writeStringRef(cti ? QoreTypeInfo::getPath(cti) : "");

                // Write return type
                const char* ret_path = sig->getReturnTypeInfo()
                    ? QoreTypeInfo::getPath(sig->getReturnTypeInfo()) : "";
                writer.writeStringRef(ret_path);

                // Write params: count, then (name, type_path, default) per param
                unsigned num_params = sig->numParams();
                writer.writeU16(static_cast<uint16_t>(num_params));
                for (unsigned p = 0; p < num_params; ++p) {
                    const char* pname = sig->getName(p);
                    writer.writeStringRef(pname ? pname : "");
                    const char* ptype = sig->getParamTypeInfo(p)
                        ? QoreTypeInfo::getPath(sig->getParamTypeInfo(p)) : "";
                    writer.writeStringRef(ptype);
                    bool has_default = sig->hasDefaultArg(p);
                    writer.writeU8(has_default ? 1 : 0);
                    if (has_default) {
                        writer.writeValue(sig->getDefaultArgList()[p]);
                    }
                }
                writer.writeU8(sig->hasVarargs() ? 1 : 0);

                // Write captured variable names and parent slot indices
                const LVarSet* vlist = const_cast<UserClosureFunction*>(ucf)->getVList();
                writer.writeU16(vlist ? static_cast<uint16_t>(vlist->size()) : 0);
                if (vlist) {
                    for (LocalVar* lv : *vlist) {
                        writer.writeStringRef(lv->getName());
                        // Write parent slot index for disambiguation
                        int32_t parent_slot = -1;
                        for (size_t i = 0; i < parent_locals.size(); ++i) {
                            if (parent_locals[i].local_var_ptr == lv) {
                                parent_slot = static_cast<int32_t>(i);
                                break;
                            }
                        }
                        writer.writeU32(static_cast<uint32_t>(parent_slot));
                    }
                }

                // Lower closure to IR and serialize
                const QoreIRFunction* closure_ir = const_cast<UserClosureVariant*>(variant)->getCachedIR();
                QoreIRFunction* owned_ir = nullptr;
                if (!closure_ir) {
                    owned_ir = ::lowerClosureForSerialization(variant);
                    closure_ir = owned_ir;
                }

                if (closure_ir) {
                    writer.writeU8(1);  // has_ir
                    uint32_t size_pos = writer.position();
                    writer.writeU32(0);  // placeholder

                    // Build extended locals list including the closure's own parameters
                    // so VarRefNodes to closure params can be serialized (not GENERIC_EVAL)
                    std::vector<AOTLocalSlotId> closure_locals = parent_locals;
                    if (sig) {
                        for (unsigned p = 0; p < sig->numParams(); ++p) {
                            if (sig->lv[p]) {
                                AOTLocalSlotId slot;
                                slot.local_var_ptr = reinterpret_cast<const void*>(sig->lv[p]);
                                slot.name = sig->lv[p]->getName();
                                closure_locals.push_back(slot);
                            }
                        }
                        if (sig->argvid) {
                            AOTLocalSlotId slot;
                            slot.local_var_ptr = reinterpret_cast<const void*>(sig->argvid);
                            slot.name = "argv";
                            closure_locals.push_back(slot);
                        }
                    }

                    auto writeExpr = [&closure_locals, &parent_globals, const_reverse_map](
                            QoreAOTBinaryWriter& w, const QoreValue& e) -> bool {
                        return classifyAndWriteExpr(w, e, closure_locals, parent_globals,
                            const_reverse_map);
                    };

                    ::serializeIRFunction(writer, *closure_ir, writeExpr);
                    uint32_t end_pos = writer.position();
                    writer.patchU32(size_pos, end_pos - size_pos - 4);
                } else {
                    writer.writeU8(0);  // no IR
                }
                delete owned_ir;
                return true;
            }
        }
    }

    // RuntimeConstantRefNode: reference to a compile-time constant
    // Look up the constant's evaluated value node in the reverse map
    if (auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(node)) {
        ConstantEntry* ce = rcr->getConstantEntry();
        if (ce && const_reverse_map) {
            // ce->val may be:
            // 1. The actual value (QoreHashNode etc.) — check if it's in the reverse map
            // 2. Another RuntimeConstantRefNode — follow the chain to saved_val
            const AbstractQoreNode* val_node = nullptr;
            if (ce->val.hasNode()) {
                val_node = ce->val.getInternalNode();
                auto it = const_reverse_map->find(val_node);
                if (it != const_reverse_map->end()) {
                    writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
                    writer.writeStringRef(it->second.c_str());
                    return true;
                }
            }
            // Try saved_val (the resolved constant value after all indirection)
            QoreValue sv = ce->getReferencedValue();
            if (sv.hasNode() && sv.getInternalNode() != val_node) {
                auto it = const_reverse_map->find(sv.getInternalNode());
                if (it != const_reverse_map->end()) {
                    writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
                    writer.writeStringRef(it->second.c_str());
                    sv.discard(nullptr);
                    return true;
                }
            }
            sv.discard(nullptr);
        }
    }

    // QoreDotEvalOperatorNode: obj.method(args) — serialize as DOT_EVAL_TARGET.
    // The inline form (used when this node appears as an argument or sub-expression
    // of another expression — e.g. STATIC_METHOD_CALL arg) must carry the full
    // information needed to rebuild the AST at load time: class_path, method_name,
    // is_pseudo, the target expression (`left`), and the argument list.  The slot
    // map form (see write_slot_DOT_EVAL_TARGET) writes only the method identity
    // because there the target and args are separate slots.
    if (auto* de = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        MethodCallNode* mc = de->getMethodCall();
        if (mc) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::DOT_EVAL_TARGET));
            const QoreClass* qc = mc->getClass();
            writer.writeStringRef(qc ? qc->getPath() : "");
            writer.writeStringRef(mc->getName() ? mc->getName() : "");
            writer.writeU8(mc->isPseudo() ? 1 : 0);
            // Target expression (left-hand side of the dot)
            classifyAndWriteExpr(writer, de->getExpression(),
                parent_locals, parent_globals, const_reverse_map);
            // Method args: prefer the evaluated args list, fall back to parse_args
            const QoreListNode* call_args = mc->getArgs();
            const QoreParseListNode* parse_args = mc->getParseArgs();
            size_t num_args = 0;
            if (call_args) {
                num_args = call_args->size();
            } else if (parse_args) {
                num_args = parse_args->size();
            }
            if (num_args > 255) {
                num_args = 0;  // too many to encode — fall through to arg-less form
            }
            writer.writeU8(static_cast<uint8_t>(num_args));
            for (size_t j = 0; j < num_args; ++j) {
                QoreValue arg = call_args
                    ? call_args->retrieveEntry(j)
                    : parse_args->get(j);
                classifyAndWriteExpr(writer, arg,
                    parent_locals, parent_globals, const_reverse_map);
            }
            return true;
        }
    }

    // Try reverse constant lookup for unsupported node types (e.g., QoreObject)
    if (const_reverse_map) {
        auto it = const_reverse_map->find(node);
        if (it != const_reverse_map->end()) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            writer.writeStringRef(it->second.c_str());
            return true;
        }
    }

    // Try EXPR_TREE serialization for operator/complex expressions
    {
        AOTSlotMap temp_slots;
        for (size_t j = 0; j < parent_locals.size(); ++j) {
            if (parent_locals[j].local_var_ptr) {
                temp_slots.local_slots[parent_locals[j].local_var_ptr] = j;
            }
        }
        std::vector<uint8_t> blob;
        if (serializeExprTreeToBlob(expr, temp_slots, blob, false, const_reverse_map)) {
            writer.writeU8(static_cast<uint8_t>(AOTExprKind::EXPR_TREE));
            writer.writeU32(static_cast<uint32_t>(blob.size()));
            writer.writeBytes(blob.data(), static_cast<uint32_t>(blob.size()));
            return true;
        }
    }

    // Unsupported — write GENERIC_EVAL placeholder
    printd(3, "AOT: handler IR unsupported expr type '%s' for serialization\n",
        node->getTypeName());
    writer.writeU8(static_cast<uint8_t>(AOTExprKind::GENERIC_EVAL));
    return true;
}

bool serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const AOTConstantReverseMap* const_reverse_map, std::string& error) {
    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::SLOT_MAPS);

    // Number of function entries
    writer.writeU32(static_cast<uint32_t>(funcs.size()));

    for (auto& func : funcs) {
        // Save entry start position and write size placeholder
        uint32_t entry_size_pos = writer.position();
        writer.writeU32(0);  // placeholder for entry size (patched below)

        // Function header
        writer.writeStringRef(func.name.c_str());
        writer.writeU16(static_cast<uint16_t>(func.num_locals));
        writer.writeU16(static_cast<uint16_t>(func.num_globals));
        writer.writeU16(static_cast<uint16_t>(func.num_exprs));
        writer.writeU16(static_cast<uint16_t>(func.num_stmts));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.regex_cases.size()));
        writer.writeU16(static_cast<uint16_t>(func.slot_ids.body_locals.size()));
        writer.writeU8(func.slot_ids.has_unsupported_exprs ? 1 : 0);
        writer.writeU8(static_cast<uint8_t>(func.num_lv_path_insts)); // was: padding byte

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

            // Use registry dispatch for expression slot metadata serialization
            const auto* kinfo = getAOTExprSlotKindInfo(static_cast<uint8_t>(expr.kind));
            if (!kinfo || !kinfo->is_supported || !kinfo->write_fn) {
                error = "unsupported expression slot kind " + std::to_string(static_cast<uint8_t>(expr.kind));
                return false;
            }
            AOTExprSlotWriteCtx wctx{writer, expr, func.slot_ids.locals, func.slot_ids.globals, const_reverse_map};
            if (!kinfo->write_fn(wctx)) {
                if (error.empty()) {
                    error = "failed to serialize expression slot kind "
                        + std::to_string(static_cast<uint8_t>(expr.kind))
                        + " (" + (kinfo->name ? kinfo->name : "?")
                        + ") in function '" + func.name + "'";
                }
                return false;
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

        // LValuePath instruction entries (in slot-index order)
        for (auto& lvid : func.slot_ids.lv_path_insts) {
            writer.writeU16(lvid.opcode);
            writer.writeU8(lvid.weak);
            writer.writeU8(lvid.compound_op);
            writer.writeU8(lvid.unary_op);
            writer.writeU8(lvid.binary_mut_op);
            writer.writeU8(lvid.ternary_op);
            writer.writeU8(lvid.ref_rv);
            // Pattern info for RegexSubst / Transliterate binary_mut ops.
            // Emitted unconditionally as (present_flag u8) so readers can skip.
            writer.writeU8(lvid.pattern_empty ? 0 : 1);
            if (!lvid.pattern_empty) {
                writer.writeStringRef(lvid.pattern.c_str());
                writer.writeStringRef(lvid.pattern_newstr.c_str());
                writer.writeI64(lvid.pattern_options);
                writer.writeU8(lvid.pattern_global);
            }
            writer.writeU8(static_cast<uint8_t>(lvid.steps.size()));
            for (auto& step : lvid.steps) {
                writer.writeU8(step.kind);
                writer.writeU32(step.slot_id);
                writer.writeStringRef(step.name.c_str());
                writer.writeU32(step.operand_idx);
            }
        }

        // Handler IR entries for statement slots
        // For each stmt slot, write u8 flag (1 = handler IR follows, 0 = no handler IR)
        // If handler IR is present, serialize the IR function inline
        for (int i = 0; i < func.num_stmts; ++i) {
            const QoreIRFunction* handler_ir = (i < static_cast<int>(func.handler_irs.size()))
                ? func.handler_irs[i] : nullptr;
            if (handler_ir) {
                writer.writeU8(1);
                // Write a size placeholder — we'll patch it after serializing the IR
                uint32_t size_pos = writer.position();
                writer.writeU32(0);  // placeholder

                // Use a writeExpr callback that classifies and writes expressions
                // using the parent function's slot info for variable resolution
                const auto& parent_locals = func.slot_ids.locals;
                const auto& parent_globals = func.slot_ids.globals;
                auto writeExpr = [&parent_locals, &parent_globals, const_reverse_map](
                        QoreAOTBinaryWriter& w, const QoreValue& expr) -> bool {
                    return classifyAndWriteExpr(w, expr, parent_locals, parent_globals,
                        const_reverse_map);
                };
                if (!serializeIRFunction(writer, *handler_ir, writeExpr)) {
                    // Handler IR serialization failed — mark as unavailable
                    printd(0, "AOT: handler IR serialization failed for stmt slot %d\n", i);
                }
                // Patch the size field
                uint32_t end_pos = writer.position();
                writer.patchU32(size_pos, end_pos - size_pos - 4);
            } else {
                writer.writeU8(0);
            }
        }

        // Location table entries (AOT runtime_loc tracking)
        writer.writeU16(static_cast<uint16_t>(func.aot_locs.size()));
        for (auto& loc : func.aot_locs) {
            writer.writeU16(static_cast<uint16_t>(loc.start_line));
            writer.writeU16(static_cast<uint16_t>(loc.end_line));
            writer.writeStringRef(loc.file.c_str());
        }

        // Patch the entry size field
        uint32_t entry_end_pos = writer.position();
        writer.patchU32(entry_size_pos, entry_end_pos - entry_size_pos - 4);
    }

    writer.endSection(sec_idx);
    return true;
}

// ---- Init Functions Section ----

//! Topologically sort init functions by their dependency graph.
//! Returns indices in execution order. Detects and reports circular dependencies.
static std::vector<size_t> topologicalSortInitFuncs(
        const std::vector<AOTCompiledInitFunc>& init_funcs) {
    size_t n = init_funcs.size();

    // Build name → index map
    std::unordered_map<std::string, size_t> name_to_idx;
    for (size_t i = 0; i < n; ++i) {
        name_to_idx[init_funcs[i].name] = i;
    }

    // Build adjacency list and in-degree count
    std::vector<std::vector<size_t>> dependents(n);  // dependents[i] = list of funcs that depend on i
    std::vector<int> in_degree(n, 0);

    for (size_t i = 0; i < n; ++i) {
        for (auto& dep_name : init_funcs[i].deps) {
            auto it = name_to_idx.find(dep_name);
            if (it != name_to_idx.end()) {
                dependents[it->second].push_back(i);
                ++in_degree[i];
            }
        }
    }

    // Kahn's algorithm
    std::vector<size_t> order;
    order.reserve(n);
    std::deque<size_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            queue.push_back(i);
        }
    }

    while (!queue.empty()) {
        size_t idx = queue.front();
        queue.pop_front();
        order.push_back(idx);
        for (size_t dep_idx : dependents[idx]) {
            if (--in_degree[dep_idx] == 0) {
                queue.push_back(dep_idx);
            }
        }
    }

    if (order.size() < n) {
        // Circular dependency detected — report and include remaining in original order
        printd(0, "AOT WARNING: circular dependency detected among %d init functions\n",
            (int)(n - order.size()));
        std::unordered_set<size_t> added(order.begin(), order.end());
        for (size_t i = 0; i < n; ++i) {
            if (added.find(i) == added.end()) {
                printd(0, "AOT WARNING: circular dependency involves '%s'\n",
                    init_funcs[i].name.c_str());
                order.push_back(i);
            }
        }
    }

    return order;
}

void serializeInitFuncs(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledInitFunc>& init_funcs) {
    // Topologically sort to ensure dependencies are initialized first
    std::vector<size_t> order = topologicalSortInitFuncs(init_funcs);

    uint32_t sec_idx = writer.beginSection(QoreAOTSectionType::INIT_FUNCS);

    writer.writeU32(static_cast<uint32_t>(init_funcs.size()));
    for (size_t idx : order) {
        auto& cif = init_funcs[idx];
        writer.writeStringRef(cif.name.c_str());
        writer.writeU8(static_cast<uint8_t>(cif.target_type));
        writer.writeStringRef(cif.ns_path.c_str());
        writer.writeStringRef(cif.item_name.c_str());
    }

    writer.endSection(sec_idx);
}

bool readInitFuncs(const uint8_t* data, uint32_t size,
        std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error) {
    QoreAOTBinaryReader reader;
    if (!reader.open(data, size, error)) {
        return false;
    }

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::INIT_FUNCS);
    if (!sec) {
        // No init funcs section — this is OK
        return true;
    }

    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        error = "invalid INIT_FUNCS section data";
        return false;
    }

    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    init_funcs.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        AOTInitFuncDescriptor desc;
        const char* name = reader.readStringRef(ptr);
        if (!name) {
            error = "invalid init func name at index " + std::to_string(i);
            return false;
        }
        desc.name = name;
        desc.target_type = static_cast<AOTCompiledInitFunc::TargetType>(
            QoreAOTBinaryReader::readU8(ptr));
        const char* ns_path = reader.readStringRef(ptr);
        if (!ns_path) {
            error = "invalid init func ns_path at index " + std::to_string(i);
            return false;
        }
        desc.ns_path = ns_path;
        const char* item_name = reader.readStringRef(ptr);
        if (!item_name) {
            error = "invalid init func item_name at index " + std::to_string(i);
            return false;
        }
        desc.item_name = item_name;
        init_funcs.push_back(std::move(desc));
    }

    return true;
}

// ---- Per-Function Source Fallback (Phase 6) ----

void serializeFallbackSources(QoreAOTBinaryWriter& writer,
        const std::vector<AOTCompiledFuncWithSlots>& funcs,
        const char* source_text, int source_len) {
    // Collect functions that need source fallback:
    // - functions with unsupported expressions (GENERIC_EVAL)
    // - functions with stmt_slots that lack handler IR (need AST execution)
    // - constructor/destructor/copy methods (need BCAList from source parsing)
    // Note: CLOSURE_CREATE no longer triggers fallback — closures are fully serialized
    std::vector<const AOTCompiledFuncWithSlots*> fallback_funcs;
    for (auto& func : funcs) {
        if (func.slot_ids.has_unsupported_exprs) {
            fallback_funcs.push_back(&func);
            continue;
        }
        if (func.num_stmts > 0) {
            bool all_have_ir = static_cast<int>(func.handler_irs.size()) == func.num_stmts
                && std::all_of(func.handler_irs.begin(), func.handler_irs.end(),
                    [](const QoreIRFunction* hir) { return hir != nullptr; });
            if (!all_have_ir) {
                fallback_funcs.push_back(&func);
                continue;
            }
        }
        // NOTE: constructor/destructor/copy methods no longer need blanket source
        // fallback — BCA data is serialized in the METHODS section (v2 format)
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

// ---- IR Function Serialization (Phase 5) ----

#include "qore/intern/Variable.h"

//! Determine the instruction group for serialization using dynamic_cast
static QoreIRInstGroup classifyInstruction(const QoreIRInstruction* inst) {
    // Check specific subclasses from most to least derived to avoid false matches.
    // Order matters because some subclasses derive from others (e.g. InvokeSimError from Throw).
    if (dynamic_cast<const QoreIRConstInstruction*>(inst)) {
        return QoreIRInstGroup::Const;
    }
    if (dynamic_cast<const QoreIRBranchIfInstruction*>(inst)) {
        return QoreIRInstGroup::BranchIf;
    }
    if (dynamic_cast<const QoreIRBranchInstruction*>(inst)) {
        return QoreIRInstGroup::Branch;
    }
    if (dynamic_cast<const QoreIRSwitchIntInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchInt;
    }
    if (dynamic_cast<const QoreIRSwitchStringInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchString;
    }
    if (dynamic_cast<const QoreIRPhiInstruction*>(inst)) {
        return QoreIRInstGroup::Phi;
    }
    if (dynamic_cast<const QoreIRGuardInstruction*>(inst)) {
        return QoreIRInstGroup::Guard;
    }
    if (dynamic_cast<const QoreIRReturnInstruction*>(inst)) {
        return QoreIRInstGroup::Return;
    }
    if (dynamic_cast<const QoreIRThrowInstruction*>(inst)) {
        return QoreIRInstGroup::Throw;
    }
    if (dynamic_cast<const QoreIRAddAssignLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedAddLocal;
    }
    if (dynamic_cast<const QoreIRIncrementLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedIncLocal;
    }
    if (dynamic_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst)) {
        return QoreIRInstGroup::FusedBrLtLocal;
    }
    if (dynamic_cast<const QoreIRLocalInstruction*>(inst)) {
        return QoreIRInstGroup::Local;
    }
    if (dynamic_cast<const QoreIRVarInstruction*>(inst)) {
        return QoreIRInstGroup::Var;
    }
    if (dynamic_cast<const QoreIRImplicitArgInstruction*>(inst)) {
        return QoreIRInstGroup::ImplicitArg;
    }
    if (dynamic_cast<const QoreIRHashKeyStoreInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyStore;
    }
    if (dynamic_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyStoreDynamic;
    }
    if (dynamic_cast<const QoreIRLValuePathInstruction*>(inst)) {
        return QoreIRInstGroup::LValuePath;
    }
    if (dynamic_cast<const QoreIRHashKeyAccessInstruction*>(inst)) {
        return QoreIRInstGroup::HashKeyAccess;
    }
    if (dynamic_cast<const QoreIRListIndexStoreInstruction*>(inst)) {
        return QoreIRInstGroup::ListIndexStore;
    }
    if (dynamic_cast<const QoreIRMapHashKeyInstruction*>(inst)) {
        return QoreIRInstGroup::MapHashKey;
    }
    if (dynamic_cast<const QoreIRSelfMemberInstruction*>(inst)) {
        return QoreIRInstGroup::SelfMember;
    }
    if (dynamic_cast<const QoreIRStaticVarInstruction*>(inst)) {
        return QoreIRInstGroup::StaticVar;
    }
    if (dynamic_cast<const QoreIRNewObjectInstruction*>(inst)) {
        return QoreIRInstGroup::NewObject;
    }
    if (dynamic_cast<const QoreIRLoadConstantInstruction*>(inst)) {
        return QoreIRInstGroup::LoadConst;
    }
    if (dynamic_cast<const QoreIRCreateClosureInstruction*>(inst)) {
        return QoreIRInstGroup::CreateClosure;
    }
    if (dynamic_cast<const QoreIRCreateCallRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateCallRef;
    }
    if (dynamic_cast<const QoreIRCreateMethodRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateMethodRef;
    }
    if (dynamic_cast<const QoreIRCreateParseRefInstruction*>(inst)) {
        return QoreIRInstGroup::CreateParseRef;
    }
    if (dynamic_cast<const QoreIRNewHashDeclInstruction*>(inst)) {
        return QoreIRInstGroup::NewHashDecl;
    }
    if (dynamic_cast<const QoreIRNewComplexHashInstruction*>(inst)) {
        return QoreIRInstGroup::NewComplexHash;
    }
    if (dynamic_cast<const QoreIRNewComplexListInstruction*>(inst)) {
        return QoreIRInstGroup::NewComplexList;
    }
    if (dynamic_cast<const QoreIRVrnConstructInstruction*>(inst)) {
        return QoreIRInstGroup::VrnConstruct;
    }
    if (dynamic_cast<const QoreIRNewHashDeclFromHashInstruction*>(inst)) {
        return QoreIRInstGroup::NewHashDeclFromHash;
    }
    if (dynamic_cast<const QoreIRLValueInstruction*>(inst)) {
        return QoreIRInstGroup::LValue;
    }
    // Check specific call instruction types before generic expr
    if (dynamic_cast<const QoreIRCallDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallDirect;
    }
    if (dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::InvokeMethodDirect;
    }
    if (dynamic_cast<const QoreIRCallMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallMethodDirect;
    }
    if (dynamic_cast<const QoreIRCallStaticDirectInstruction*>(inst)) {
        return QoreIRInstGroup::CallStaticDirect;
    }
    if (dynamic_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::InvokeDotEvalMethodDirect;
    }
    if (dynamic_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst)) {
        return QoreIRInstGroup::DotEvalMethodDirect;
    }
    if (dynamic_cast<const QoreIRInvokeInstruction*>(inst)) {
        return QoreIRInstGroup::Invoke;
    }
    if (dynamic_cast<const QoreIRIteratorCreateInstruction*>(inst)) {
        return QoreIRInstGroup::IteratorCreate;
    }
    if (dynamic_cast<const QoreIRIteratorNextInstruction*>(inst)) {
        return QoreIRInstGroup::IteratorNext;
    }
    if (dynamic_cast<const QoreIROnBlockExitInstruction*>(inst)) {
        return QoreIRInstGroup::OnBlockExit;
    }
    if (dynamic_cast<const QoreIRScopeEnterInstruction*>(inst)) {
        return QoreIRInstGroup::ScopeEnter;
    }
    if (dynamic_cast<const QoreIRScopeExitInstruction*>(inst)) {
        return QoreIRInstGroup::ScopeExit;
    }
    if (dynamic_cast<const QoreIRLandingPadInstruction*>(inst)) {
        return QoreIRInstGroup::LandingPad;
    }
    if (dynamic_cast<const QoreIRSwitchRegexMatchInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchRegexMatch;
    }
    if (dynamic_cast<const QoreIRRefForeachInitInstruction*>(inst)) {
        return QoreIRInstGroup::RefForeachInit;
    }
    if (dynamic_cast<const QoreIRMakeHashConstKeysInstruction*>(inst)) {
        return QoreIRInstGroup::MakeHashConstKeys;
    }
    if (dynamic_cast<const QoreIRSwitchCaseMatchInstruction*>(inst)) {
        return QoreIRInstGroup::SwitchCaseMatch;
    }
    if (dynamic_cast<const QoreIRContextInstruction*>(inst)) {
        return QoreIRInstGroup::Context;
    }
    if (dynamic_cast<const QoreIRSummarizeInstruction*>(inst)) {
        return QoreIRInstGroup::Summarize;
    }
    if (dynamic_cast<const QoreIRListIndexAccessInstruction*>(inst)) {
        return QoreIRInstGroup::ListIndexAccess;
    }
    if (dynamic_cast<const QoreIRExprInstruction*>(inst)) {
        return QoreIRInstGroup::Expr;
    }
    // Default: base instruction (no extra fields)
    return QoreIRInstGroup::Base;
}

//! Get type path string for a LocalVar, handling nullptr typeInfo
const char* getLocalTypePath(const LocalVar* lv) {
    const QoreTypeInfo* ti = lv->getTypeInfo();
    return ti ? QoreTypeInfo::getPath(ti) : "";
}

//! Serialize a single IR instruction
static bool serializeIRInstruction(QoreAOTBinaryWriter& writer, const QoreIRInstruction* inst,
        const std::unordered_map<const QoreIRBasicBlock*, uint16_t>& block_idx,
        const AOTExprWriteFunc& writeExpr) {
    // Write opcode
    writer.writeU16(static_cast<uint16_t>(inst->opcode));

    // Classify and write group tag
    QoreIRInstGroup group = classifyInstruction(inst);
    writer.writeU8(static_cast<uint8_t>(group));

    // Write base fields: result, operands, exception_target
    writer.writeU32(inst->result.id);
    writer.writeU8(static_cast<uint8_t>(inst->operands.size()));
    for (auto& op : inst->operands) {
        writer.writeU32(op.id);
    }
    // Exception target block index (0xFFFF = none)
    // Write base exception target
    if (inst->exception_target) {
        auto it = block_idx.find(inst->exception_target);
        writer.writeU16(it != block_idx.end() ? it->second : 0xFFFF);
    } else {
        writer.writeU16(0xFFFF);
    }

    // Write group-specific fields via registry dispatch
    const auto* ginfo = getAOTInstGroupInfo(static_cast<uint8_t>(group));
    if (!ginfo || !ginfo->is_serializable) {
        return false;
    }
    if (ginfo->write_fn) {
        AOTInstWriteCtx wctx{writer, inst, block_idx, writeExpr};
        if (!ginfo->write_fn(wctx)) {
            return false;
        }
    }

    // Write source location for runtime exception stack traces (AOT location table)
    if (inst->loc && inst->loc->start_line > 0) {
        writer.writeU16(static_cast<uint16_t>(inst->loc->start_line));
        writer.writeU16(static_cast<uint16_t>(inst->loc->end_line));
        writer.writeStringRef(inst->loc->getFile() ? inst->loc->getFile() : "");
    } else {
        writer.writeU16(0);  // start_line=0 signals "no location"
        writer.writeU16(0);
        writer.writeStringRef("");
    }

    return true;
}

bool serializeIRFunction(QoreAOTBinaryWriter& writer, const QoreIRFunction& func,
        const AOTExprWriteFunc& writeExpr) {
    // 1. Function header
    writer.writeStringRef(func.name.c_str());
    writer.writeU32(func.max_value_id);
    writer.writeU32(func.max_local_slot_id);
    writer.writeU32(func.num_guards);
    writer.writeStringRef(func.return_type_info ? QoreTypeInfo::getPath(func.return_type_info) : "");
    // Phase C: Serialize parent_slot_count for handler IR functions
    writer.writeU32(func.parent_slot_count);
    writer.writeU16(static_cast<uint16_t>(func.blocks.size()));
    writer.writeU16(static_cast<uint16_t>(func.local_var_slots.size()));
    writer.writeU16(static_cast<uint16_t>(func.all_body_locals.size()));

    // 2. Local variable slot table
    // Sort by slot_id for deterministic serialization
    std::vector<std::pair<const LocalVar*, uint32_t>> sorted_slots(
        func.local_var_slots.begin(), func.local_var_slots.end());
    std::sort(sorted_slots.begin(), sorted_slots.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    for (auto& [lv, slot_id] : sorted_slots) {
        writer.writeStringRef(lv->getName());
        writer.writeStringRef(getLocalTypePath(lv));
        writer.writeU32(slot_id);
    }

    // 3. Body locals
    for (auto* lv : func.all_body_locals) {
        writer.writeStringRef(lv->getName());
        writer.writeStringRef(getLocalTypePath(lv));
    }

    // 4. Build block index map for block reference serialization
    std::unordered_map<const QoreIRBasicBlock*, uint16_t> block_idx;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_idx[func.blocks[i].get()] = static_cast<uint16_t>(i);
    }

    // 5. Serialize blocks
    for (auto& block : func.blocks) {
        writer.writeStringRef(block->name.c_str());
        writer.writeU8(block->is_loop_header ? 1 : 0);
        writer.writeU16(static_cast<uint16_t>(block->instructions.size()));

        for (auto& inst_ptr : block->instructions) {
            if (!serializeIRInstruction(writer, inst_ptr.get(), block_idx, writeExpr)) {
                return false;
            }
        }
    }

    return true;
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

    // In-progress class path → QoreClass* map used as a fallback lookup in
    // readValue VT_NEW_OBJECT for forward references inside member init
    // expressions (e.g. `OtherClass m()`). This is needed because classes
    // are not committed to the root namespace's clmap until after the full
    // classes pass, so getProgram()->findClass() won't find them mid-pass.
    std::unordered_map<std::string, QoreClass*> pending_class_map;
    struct ClassMapRAII {
        ClassMapRAII(const std::unordered_map<std::string, QoreClass*>* p) {
            g_aot_pending_class_map = p;
        }
        ~ClassMapRAII() {
            g_aot_pending_class_map = nullptr;
        }
    };
    ClassMapRAII raii(&pending_class_map);

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

        // Register into the forward-ref map for member-init resolution.
        // Build both the bare path ("DataProvider::Foo") and a fully-scoped
        // form with leading "::" so either lookup shape works.
        if (path && *path) {
            pending_class_map[path] = qc;
            if (strncmp(path, "::", 2) == 0) {
                pending_class_map[std::string(path + 2)] = qc;
            } else {
                pending_class_map[std::string("::") + path] = qc;
            }
        }

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
            PendingInstanceMember pim;
            pim.name = mname ? mname : "";
            pim.type_path = mtype_path ? mtype_path : "";
            pim.access = maccess;
            if (has_default) {
                if (!readDeferredMemberDefault(reader, ptr, end, error,
                        default_val, pim)) {
                    error = "instance member '" + pim.name + "' default: " + error;
                    return false;
                }
            }
            pim.default_val = default_val;

            if (!class_already_existed && mname && *mname) {
                instance_members.push_back(std::move(pim));
            } else {
                if (default_val.hasNode()) {
                    default_val.discard(nullptr);
                }
                for (auto& v : pim.pending_new_args) {
                    v.discard(nullptr);
                }
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
            // Read the serialized initial value if present. Matches the
            // write-side layout: u8 has_value + optional value.
            QoreValue default_val;
            PendingStaticMember psm;
            psm.name = sm_name ? sm_name : "";
            psm.type_path = sm_type_path ? sm_type_path : "";
            psm.access = sm_access;
            uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
            if (has_default) {
                if (!readDeferredMemberDefault(reader, ptr, end, error,
                        default_val, psm)) {
                    error = "static member '" + psm.name + "': " + error;
                    return false;
                }
            }
            psm.default_val = default_val;
            if (!class_already_existed && sm_name && *sm_name) {
                static_members.push_back(std::move(psm));
            } else {
                if (default_val.hasNode()) {
                    default_val.discard(nullptr);
                }
                for (auto& v : psm.pending_new_args) {
                    v.discard(nullptr);
                }
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
            } else if (cval.hasNode()) {
                cval.discard(nullptr);
            }
        }
        pending_class_constants.push_back(std::move(class_constants));
    }

    return true;
}

bool QoreAOTBinaryDeserializer::resolveClassBases(std::string& error) {
    uint32_t count = std::min(static_cast<uint32_t>(class_list.size()),
        static_cast<uint32_t>(pending_bases.size()));

    // Build a map from class path to class_list index for newly deserialized classes
    std::unordered_map<std::string, uint32_t> path_to_idx;
    for (uint32_t i = 0; i < count; ++i) {
        if (!class_list[i] || preexisting_classes.count(i)) {
            continue;
        }
        path_to_idx[class_list[i]->getPath()] = i;
    }

    // Compute topological order (bases before derived) using Kahn's algorithm.
    // This ensures addBaseClass() can propagate grandparent classes correctly,
    // since the base class's hierarchy is fully resolved before the derived class.
    {
        // Build adjacency graph: edge from base_idx -> derived_idx
        std::vector<std::vector<uint32_t>> dependents(count);
        std::vector<uint32_t> in_degree(count, 0);

        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                continue;
            }
            for (auto& pbc : pending_bases[i]) {
                auto it = path_to_idx.find(pbc.base_path);
                if (it != path_to_idx.end() && it->second != i) {
                    // Base class is also newly deserialized — must be processed first
                    dependents[it->second].push_back(i);
                    ++in_degree[i];
                }
            }
        }

        // Kahn's algorithm: start with classes that have no in-module base dependencies
        std::deque<uint32_t> queue;
        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                continue;
            }
            if (in_degree[i] == 0) {
                queue.push_back(i);
            }
        }

        topo_order.clear();
        topo_order.reserve(count);
        while (!queue.empty()) {
            uint32_t idx = queue.front();
            queue.pop_front();
            topo_order.push_back(idx);
            for (uint32_t dep : dependents[idx]) {
                if (--in_degree[dep] == 0) {
                    queue.push_back(dep);
                }
            }
        }

        // Add any classes not in the topological order (preexisting or null)
        // These are processed last but typically skipped anyway
        for (uint32_t i = 0; i < count; ++i) {
            if (!class_list[i] || preexisting_classes.count(i)) {
                topo_order.push_back(i);
            }
        }

        printd(5, "AOT deser: topological order for %d classes computed (%d in topo sort)\n",
            (int)count, (int)topo_order.size());
    }

    // Resolve base classes in topological order (bases before derived)
    for (uint32_t idx : topo_order) {
        if (idx >= count || !class_list[idx] || preexisting_classes.count(idx)) {
            continue;
        }
        QoreClass* qc = class_list[idx];

        for (auto& pbc : pending_bases[idx]) {
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
                printd(5, "AOT deser: resolved base class '%s' (id: %d) for class '%s' (id: %d)\n",
                    pbc.base_path.c_str(), base->getID(),
                    qc->getName(), qc->getID());
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
    // Build a path→QoreClass* map across all deserialized classes so pending
    // forward-reference `NewObject` init expressions can be resolved here,
    // after every class has been registered.
    std::unordered_map<std::string, QoreClass*> all_class_map;
    for (uint32_t i = 0; i < class_list.size(); ++i) {
        if (!class_list[i]) {
            continue;
        }
        const char* cpath = class_list[i]->getPath();
        if (cpath && *cpath) {
            all_class_map[cpath] = class_list[i];
            if (strncmp(cpath, "::", 2) == 0) {
                all_class_map[std::string(cpath + 2)] = class_list[i];
            } else {
                all_class_map[std::string("::") + cpath] = class_list[i];
            }
        }
    }

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

            // Resolve a pending forward-referenced NewObject default if any.
            if (!pim.pending_new_class_path.empty()) {
                const QoreClass* target = nullptr;
                {
                    ExceptionSink xs;
                    target = getProgram()->findClass(pim.pending_new_class_path.c_str(), &xs);
                    if (xs.isException()) {
                        xs.clear();
                    }
                }
                if (!target) {
                    auto it = all_class_map.find(pim.pending_new_class_path);
                    if (it != all_class_map.end()) {
                        target = it->second;
                    }
                }
                if (target) {
                    QoreParseListNode* parse_args = nullptr;
                    if (!pim.pending_new_args.empty()) {
                        parse_args = new QoreParseListNode(&loc_builtin);
                        for (auto& v : pim.pending_new_args) {
                            parse_args->add(v, &loc_builtin);
                        }
                        pim.pending_new_args.clear();
                    }
                    ScopedObjectCallNode* socn = new ScopedObjectCallNode(
                        &loc_builtin, target, parse_args);
                    if (parse_args) {
                        socn->resolveParseArgs();
                    }
                    pim.default_val = QoreValue(socn);
                } else {
                    printd(0, "AOT deser: cannot resolve pending NewObject class '%s' for "
                        "instance member '%s' in class '%s'\n",
                        pim.pending_new_class_path.c_str(), pim.name.c_str(), qc->getName());
                    for (auto& v : pim.pending_new_args) {
                        v.discard(nullptr);
                    }
                    pim.pending_new_args.clear();
                }
                pim.pending_new_class_path.clear();
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
    // Build an all-classes path map for pending forward-ref NewObject init
    // resolution (mirrors resolveInstanceMembers).
    std::unordered_map<std::string, QoreClass*> all_class_map;
    for (uint32_t i = 0; i < class_list.size(); ++i) {
        if (!class_list[i]) {
            continue;
        }
        const char* cpath = class_list[i]->getPath();
        if (cpath && *cpath) {
            all_class_map[cpath] = class_list[i];
            if (strncmp(cpath, "::", 2) == 0) {
                all_class_map[std::string(cpath + 2)] = class_list[i];
            } else {
                all_class_map[std::string("::") + cpath] = class_list[i];
            }
        }
    }

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

            // Resolve a pending forward-referenced NewObject default if any
            if (!psm.pending_new_class_path.empty()) {
                const QoreClass* target = nullptr;
                {
                    ExceptionSink xs;
                    target = getProgram()->findClass(psm.pending_new_class_path.c_str(), &xs);
                    if (xs.isException()) {
                        xs.clear();
                    }
                }
                if (!target) {
                    auto it = all_class_map.find(psm.pending_new_class_path);
                    if (it != all_class_map.end()) {
                        target = it->second;
                    }
                }
                if (target) {
                    QoreParseListNode* parse_args = nullptr;
                    if (!psm.pending_new_args.empty()) {
                        parse_args = new QoreParseListNode(&loc_builtin);
                        for (auto& v : psm.pending_new_args) {
                            parse_args->add(v, &loc_builtin);
                        }
                        psm.pending_new_args.clear();
                    }
                    ScopedObjectCallNode* socn = new ScopedObjectCallNode(
                        &loc_builtin, target, parse_args);
                    if (parse_args) {
                        socn->resolveParseArgs();
                    }
                    psm.default_val = QoreValue(socn);
                } else {
                    printd(0, "AOT deser: cannot resolve pending NewObject class '%s' for "
                        "static member '%s' in class '%s'\n",
                        psm.pending_new_class_path.c_str(), psm.name.c_str(), qc->getName());
                    for (auto& v : psm.pending_new_args) {
                        v.discard(nullptr);
                    }
                    psm.pending_new_args.clear();
                }
                psm.pending_new_class_path.clear();
            }

            // Create the static variable info. The default value is
            // installed via assignInit() below (after construction) so the
            // serialized initial value survives AOT load. For static vars
            // whose init expression `needs_eval()`, an svar init function
            // was generated at compile time and will overwrite this value
            // at load time (see compileInitExpr / executeInitFunctions).
            QoreVarInfo* vi = new QoreVarInfo(&loc_builtin, ti, nullptr, QoreValue(),
                static_cast<ClassAccess>(psm.access));

            // Run parseInit() first so the QoreMemberInfoBaseAccess::init
            // flag is set and future parseInit() calls (e.g. from
            // qore_class_private::copy() during class merge) become no-ops.
            // Without this guard, the copy path would re-enter parseInit()
            // and call val.set(typeInfo) → reset() on our already-installed
            // value, silently losing it.
            vi->parseInit(psm.name.c_str());

            // Install the serialized initial value (if any). Takes
            // ownership of the ref held in psm.default_val.
            if (psm.default_val.hasNode() || psm.default_val.getType() != NT_NOTHING) {
                QoreValue v = psm.default_val;
                psm.default_val = QoreValue();  // transfer ownership
                vi->assignInit(v);
                vi->eval_init = true;
            }

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
                printd(0, "AOT: cannot resolve type '%s' for member '%s' "
                    "in hashdecl '%s': %s (falling back to auto)\n",
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
            for (auto& mi : members) {
                mi.default_val.discard(nullptr);
            }
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
            for (auto& mi : members) {
                mi.default_val.discard(nullptr);
            }
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
            printd(0, "AOT: failed to resolve type '%s' for const '%s': %s\n",
                type_path ? type_path : "(null)", name ? name : "(null)", error.c_str());
            error = "cannot resolve type '" + std::string(type_path ? type_path : "(null)") +
                "' for constant '" + std::string(name) + "': " + error;
            return false;
        }
        const QoreTypeInfo* final_ti = ti ? ti : val.getTypeInfo();
        // Create as user constant (not builtin) with proper pub flag.
        // Using add() would mark the constant as builtin, which causes
        // scanMergeCommittedNamespace to skip it (isUserPublic() returns false).
        // Create the ConstantEntry directly with: pub = is_pub, init = true (value
        // already resolved), builtin = false (user constant from AOT module).
        ConstantEntry* ce = new ConstantEntry(&loc_builtin, name, val,
            final_ti, is_pub != 0, true, false,
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
            printd(0, "AOT: cannot resolve type '%s' for parameter '%s': %s "
                "(falling back to auto)\n",
                ptype_path ? ptype_path : "(null)", param_names[j].c_str(), error.c_str());
            error.clear();
            pti = autoTypeInfo;
        }
        param_types[j] = pti;

        if (has_default == 1) {
            // Constant default value
            param_defaults[j] = reader.readValue(ptr, end, error);
            if (!error.empty()) {
                // Clean up already-read defaults
                for (uint32_t k = 0; k < j; ++k) {
                    param_defaults[k].discard(nullptr);
                }
                return false;
            }
        } else if (has_default == 2) {
            // Expression default: no-arg function call (e.g., getcwd())
            const char* fname = reader.readStringRef(ptr);
            if (fname && *fname) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                    *pp->RootNS, fname);
                if (fe) {
                    FunctionCallNode* fcn = new FunctionCallNode(
                        &loc_builtin, fe, (QoreListNode*)nullptr, pgm);
                    param_defaults[j] = QoreValue(fcn);
                } else {
                    printd(0, "AOT deser: cannot resolve default expression function '%s'\n",
                        fname);
                    param_defaults[j] = QoreValue(true);
                }
            } else {
                param_defaults[j] = QoreValue(true);
            }
        } else if (has_default == 3) {
            // Expression default: plain constant reference.
            // Build a RuntimeConstantRefNode pointing at the resolved entry
            // so later evaluation returns the current value of the constant.
            const char* cfqn = reader.readStringRef(ptr);
            ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, cfqn);
            if (ce) {
                param_defaults[j] = QoreValue(new RuntimeConstantRefNode(&loc_builtin, ce));
            } else {
                printd(0, "AOT deser: cannot resolve default const ref '%s'\n",
                    cfqn ? cfqn : "(null)");
                param_defaults[j] = QoreValue(true);
            }
        } else if (has_default == 4) {
            // Expression default: no-arg method call on a constant
            //   e.g. `AutoHashType.getName()`.
            // Rebuild the AST as a QoreDotEvalOperatorNode whose `left` is a
            // freshly-constructed RuntimeConstantRefNode wrapping the
            // referenced ConstantEntry, and whose method call has a dynamic
            // lookup by name (qc/method left null) — the runtime dispatch
            // path in AbstractMethodCallNode::exec handles this case.
            const char* cfqn = reader.readStringRef(ptr);
            const char* mname = reader.readStringRef(ptr);
            ConstantEntry* ce = aot_resolve_constant_by_fqn(pgm, cfqn);
            if (ce && mname && *mname) {
                auto* rcr = new RuntimeConstantRefNode(&loc_builtin, ce);
                auto* mc = new MethodCallNode(&loc_builtin, strdup(mname),
                    (QoreParseListNode*)nullptr);
                auto* de = new QoreDotEvalOperatorNode(&loc_builtin, QoreValue(rcr), mc);
                param_defaults[j] = QoreValue(de);
            } else {
                printd(0, "AOT deser: cannot resolve default dot-eval const '%s'.%s()\n",
                    cfqn ? cfqn : "(null)", mname ? mname : "(null)");
                param_defaults[j] = QoreValue(true);
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
                    if (has_default == 1) {
                        // Constant default: skip value
                        QoreValue default_val = reader.readValue(ptr, end, error);
                        if (!error.empty()) {
                            return false;
                        }
                        default_val.discard(nullptr);
                    } else if (has_default == 2) {
                        // Expression default (no-arg function call): skip function name ref
                        reader.readStringRef(ptr);
                    } else if (has_default == 3) {
                        // Expression default (const ref): skip FQN
                        reader.readStringRef(ptr);
                    } else if (has_default == 4) {
                        // Expression default (method call on const): skip FQN + method name
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
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

            // Sync QCF_USES_EXTRA_ARGS flag with signature varargs state; the constructor
            // couldn't set this because the signature was populated after construction
            if (has_varargs) {
                ufv->setFlag(QCF_USES_EXTRA_ARGS);
            }

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

            // Sync QCF_USES_EXTRA_ARGS flag with signature varargs state
            if (has_varargs) {
                mvb->setFlag(QCF_USES_EXTRA_ARGS);
            }

            // Deserialize BCA (Base Class Constructor Arguments) for constructors
            if (is_constructor && ptr < end) {
                uint8_t has_bca = QoreAOTBinaryReader::readU8(ptr);
                if (has_bca) {
                    uint16_t num_bca = QoreAOTBinaryReader::readU16(ptr);
                    if (num_bca > 0) {
                        BCAList* bcal = new BCAList();

                        // Build local var array from constructor's signature params
                        UserSignature* sig = umv->getUserSignature();
                        std::vector<LocalVar*> local_vars;
                        if (sig) {
                            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                                local_vars.push_back(sig->lv[pi]);
                            }
                            if (sig->selfid) {
                                local_vars.push_back(sig->selfid);
                            }
                            if (sig->argvid) {
                                local_vars.push_back(sig->argvid);
                            }
                        }

                        for (uint16_t bi = 0; bi < num_bca; ++bi) {
                            // Read base class path
                            const char* base_path = reader.readStringRef(ptr);

                            // Resolve base class by path
                            qore_classid_t base_classid = 0;
                            if (base_path && base_path[0]) {
                                ExceptionSink xsink;
                                const QoreClass* base_cls = pgm->findClass(base_path, &xsink);
                                if (xsink.isException()) {
                                    xsink.clear();
                                }
                                if (base_cls) {
                                    base_classid = base_cls->getID();
                                } else {
                                    printd(0, "AOT deser: cannot resolve BCA base class '%s' "
                                        "for %s::constructor\n", base_path, qc->getName());
                                }
                            }

                            // Read and deserialize args
                            uint16_t num_args = QoreAOTBinaryReader::readU16(ptr);
                            QoreListNode* arg_list = nullptr;
                            if (num_args > 0) {
                                // Must use newList(true) so the list has value=false and
                                // needs_eval=true; otherwise evalList() returns the list as-is
                                // without evaluating expression nodes
                                arg_list = qore_list_private::newList(true);
                                qore_list_private::get(*arg_list)->complexTypeInfo =
                                    qore_get_complex_list_type(autoTypeInfo);
                                for (uint16_t ai = 0; ai < num_args; ++ai) {
                                    uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                                    if (blob_size > 0 && ptr + blob_size <= end) {
                                        QoreValue arg_val = deserializeExprTreeFromBlob(
                                            ptr, blob_size, pgm,
                                            local_vars.empty() ? nullptr : local_vars.data(),
                                            static_cast<int>(local_vars.size()));
                                        ptr += blob_size;
                                        arg_list->push(arg_val, nullptr);
                                    } else {
                                        ptr += blob_size;
                                        arg_list->push(QoreValue(), nullptr);
                                    }
                                }
                            }

                            // Create BCANode with pre-resolved data
                            BCANode* bca_node = new BCANode(base_classid, arg_list);
                            bcal->push_back(bca_node);

                            printd(5, "AOT deser: BCA entry %d/%d: base='%s' classid=%d "
                                "num_args=%d for %s::constructor\n",
                                bi + 1, num_bca, base_path ? base_path : "",
                                base_classid, num_args, qc->getName());
                        }

                        // Set BCA on the constructor variant
                        UserConstructorVariant* ucv = dynamic_cast<UserConstructorVariant*>(mvb);
                        if (ucv) {
                            ucv->setBCAList(bcal);
                        } else {
                            delete bcal;
                        }
                    }
                }
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
    // Use topological order (bases before derived) computed in resolveClassBases().
    // If topo_order is empty (no classes or resolveClassBases not called), fall back
    // to sequential order for backward compatibility.
    auto& order = topo_order;
    std::vector<uint32_t> fallback_order;
    if (order.empty() && !class_list.empty()) {
        fallback_order.resize(class_list.size());
        std::iota(fallback_order.begin(), fallback_order.end(), 0);
        order = fallback_order;
    }

    // First pass: commit all newly deserialized classes (moves pending variants to vlist)
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
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

    // Second pass: import abstract methods from parent classes and resolve them.
    // This must happen AFTER parseCommit() because concrete variants are in the
    // pending list until committed — parseHasVariantWithSignature() only searches
    // the committed vlist. This mirrors parseInitPartialIntern() (QoreClass.cpp:4477).
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl) {
            continue;
        }
        for (auto bi = priv->scl->begin(), be = priv->scl->end(); bi != be; ++bi) {
            const QoreClass* parent = (*bi)->sclass;
            if (!parent) {
                continue;
            }
            const AbstractMethodMap& parent_ahm = qore_class_private::get(*parent)->ahm;
            for (auto ai = parent_ahm.begin(), ae = parent_ahm.end(); ai != ae; ++ai) {
                if (priv->ahm.find(ai->first) != priv->ahm.end()) {
                    continue;
                }
                // Check if we have a local concrete override (now in committed vlist)
                auto mi = priv->hm.find(ai->first);
                MethodFunctionBase* f = (mi != priv->hm.end())
                    ? qore_method_private::get(*mi->second)->getFunction() : nullptr;
                if (f && f->parseHasVariantWithSignature(
                        ai->second->vlist.begin()->second, priv->ahm.relaxed_match)) {
                    // Resolved — concrete override matches abstract signature
                    continue;
                }
                // Unresolved — import abstract method
                std::unique_ptr<AbstractMethod> m(new AbstractMethod(priv->ahm.relaxed_match));
                m->parseMergeBase(*(ai->second), f);
                if (!m->empty()) {
                    priv->ahm.insert(amap_t::value_type(ai->first, m.release()));
                }
            }
        }
    }

    // Validation pass: verify every base class is reachable via getClass().
    // Catches hierarchy bugs at load time instead of deep in object construction.
    for (uint32_t i : order) {
        if (i >= class_list.size() || preexisting_classes.count(i)) {
            continue;
        }
        QoreClass* qc = class_list[i];
        if (!qc) {
            continue;
        }
        qore_class_private* priv = qore_class_private::get(*qc);
        if (!priv->scl) {
            continue;
        }
        for (auto bi = priv->scl->begin(), be = priv->scl->end(); bi != be; ++bi) {
            const QoreClass* base = (*bi)->sclass;
            if (!base) {
                continue;
            }
            if (!qc->getClass(base->getID())) {
                error = "class hierarchy broken: '" + std::string(qc->getName()) +
                    "' cannot reach base class '" + std::string(base->getName()) +
                    "' (id: " + std::to_string(base->getID()) + ")";
                return false;
            }
        }
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
    state.root_ns = root_ns;  // Store root namespace for program-wide CRM building
    collectItems(state, root_ns, UINT32_MAX, module_name, keep_modules);

    // Build a program-wide constant reverse map once and make it available to
    // the writer so writeValue() can encode node-pointer references (e.g. an
    // object inside a parse-time-folded hash literal) as VT_CONST_REF entries
    // that resolve at load time. The writer takes a non-owning pointer; the
    // map is cleared after all sections that call writeValue are done.
    AOTConstantReverseMap program_crm;
    if (root_ns) {
        buildProgramConstantReverseMapImpl(root_ns, program_crm);
    }
    writer.const_reverse_map = &program_crm;

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

    // Drop the non-owning CRM pointer — program_crm goes out of scope next.
    writer.const_reverse_map = nullptr;

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

bool compressMetadata(const std::vector<uint8_t>& input,
        std::vector<uint8_t>& output,
        std::string& error) {
    if (input.empty()) {
        // Empty input, still compress it
        output.resize(4);
        // Store original size (0) as 4-byte little-endian
        output[0] = 0;
        output[1] = 0;
        output[2] = 0;
        output[3] = 0;
        return true;
    }

    // Reserve space for original size (4 bytes) + compressed data
    uLongf compressed_size = compressBound(input.size());
    output.resize(4 + compressed_size);

    // Store original size as 4-byte little-endian prefix
    uint32_t orig_size = static_cast<uint32_t>(input.size());
    output[0] = static_cast<uint8_t>(orig_size & 0xFF);
    output[1] = static_cast<uint8_t>((orig_size >> 8) & 0xFF);
    output[2] = static_cast<uint8_t>((orig_size >> 16) & 0xFF);
    output[3] = static_cast<uint8_t>((orig_size >> 24) & 0xFF);

    // Compress into buffer after the size prefix
    int ret = compress2(output.data() + 4, &compressed_size,
                        input.data(), input.size(), 9);

    if (ret != Z_OK) {
        error = "zlib compression failed (error code " + std::to_string(ret) + ")";
        output.clear();
        return false;
    }

    // Trim output to actual compressed size + 4 byte prefix
    output.resize(4 + compressed_size);
    return true;
}

bool decompressMetadata(const uint8_t* input, size_t input_len,
        std::vector<uint8_t>& output,
        std::string& error) {
    if (input_len < 4) {
        error = "compressed metadata too short (need at least 4 bytes for size prefix)";
        return false;
    }

    // Read original size from first 4 bytes (little-endian)
    uint32_t orig_size = static_cast<uint32_t>(input[0]) |
                         (static_cast<uint32_t>(input[1]) << 8) |
                         (static_cast<uint32_t>(input[2]) << 16) |
                         (static_cast<uint32_t>(input[3]) << 24);

    if (orig_size == 0) {
        // Empty metadata
        output.clear();
        return true;
    }

    // Sanity check: decompressed size shouldn't be larger than a reasonable limit
    const size_t MAX_DECOMPRESSED = 100 * 1024 * 1024;  // 100 MB limit
    if (orig_size > MAX_DECOMPRESSED) {
        error = "decompressed metadata size " + std::to_string(orig_size) +
                " exceeds maximum allowed (" + std::to_string(MAX_DECOMPRESSED) + " bytes)";
        return false;
    }

    output.resize(orig_size);
    uLongf dest_len = orig_size;

    // Decompress
    int ret = uncompress(output.data(), &dest_len,
                         input + 4, input_len - 4);

    if (ret != Z_OK) {
        error = "zlib decompression failed (error code " + std::to_string(ret) + ")";
        output.clear();
        return false;
    }

    if (dest_len != orig_size) {
        error = "decompressed size " + std::to_string(dest_len) +
                " does not match expected size " + std::to_string(orig_size);
        output.clear();
        return false;
    }

    return true;
}
