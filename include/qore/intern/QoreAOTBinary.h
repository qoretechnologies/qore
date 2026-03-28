/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBinary.h

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

#ifndef _QORE_INTERN_QOREAOTBINARY_H
#define _QORE_INTERN_QOREAOTBINARY_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <qore/QoreValue.h>
#include <qore/QoreEnumDecl.h>
#include <qore/TypedHashDecl.h>

class AbstractQoreNode;
class QoreValue;
class QoreProgram;
class QoreTypeInfo;
class qore_ns_private;
class QoreIRFunction;
class LocalVar;
class Var;

//! Reverse map from constant value node pointer to fully-qualified constant name
typedef std::unordered_map<const AbstractQoreNode*, std::string> AOTConstantReverseMap;

//! Magic number: "QORD" in little-endian (0x44524F51)
constexpr uint32_t QORE_AOT_BINARY_MAGIC = 0x44524F51;

//! Current binary format version
//! v1: initial format with full 128-bit parse options + source hash
//! v2: added BCA (Base Class Constructor Arguments) serialization for constructors
constexpr uint16_t QORE_AOT_BINARY_VERSION = 2;

//! On-disk header size (60 bytes)
constexpr uint32_t QORE_AOT_HEADER_SIZE = 60;

//! Binary header flags
constexpr uint16_t QORE_AOT_FLAG_HAS_TOPLEVEL = 0x0001;
constexpr uint16_t QORE_AOT_FLAG_IS_MODULE    = 0x0002;

//! Feature flags for binary compatibility (64-bit bitmask in header.feature_flags)
constexpr uint64_t QORE_AOT_FEAT_FOREACH_REF    = 1ULL << 0;  //!< RefForeach* opcodes (325-330)
constexpr uint64_t QORE_AOT_FEAT_NATIVE_CAST     = 1ULL << 1;  //!< Cast* opcodes (252-256)
constexpr uint64_t QORE_AOT_FEAT_BLOCK_EXIT      = 1ULL << 2;  //!< OnBlockExit opcode
constexpr uint64_t QORE_AOT_FEAT_DIRECT_INDEX    = 1ULL << 3;  //!< ListGet* opcodes (13-15)
constexpr uint64_t QORE_AOT_FEAT_HASH_KEY_ACCESS = 1ULL << 4;  //!< HashKeyAccess opcodes
constexpr uint64_t QORE_AOT_FEAT_FAST_CALL       = 1ULL << 5;  //!< CallMethodDirect/CallStaticDirect
constexpr uint64_t QORE_AOT_FEAT_COMPLEX_RETURN  = 1ULL << 6;  //!< reserved, set to 0 for now
constexpr uint64_t QORE_AOT_FEAT_HASH_KEY_STORE  = 1ULL << 7;  //!< HashKeyStore opcode (335)
constexpr uint64_t QORE_AOT_FEAT_LIST_INDEX_STORE = 1ULL << 8;  //!< ListIndexStore opcode (337)
//! Mask of all currently supported features
constexpr uint64_t QORE_AOT_SUPPORTED_FEATURES   = 0x1FFULL;

//! Section type IDs
enum class QoreAOTSectionType : uint16_t {
    STRINGS       = 1,
    NAMESPACES    = 2,
    CLASSES       = 3,
    HASHDECLS     = 4,
    ENUMS         = 5,
    TYPEDEFS      = 6,
    CONSTANTS     = 7,
    GLOBALS       = 8,
    FUNCTIONS     = 9,
    METHODS       = 10,
    SLOT_MAPS     = 11,
    TOPLEVEL      = 12,
    FUNC_SOURCES  = 13,
    DEPENDENCIES  = 14,  //!< Module dependencies (for strip-source modules)
    REEXPORT_MODULES = 15,  //!< Modules that should be reexported (for strip-source modules)
    PROGRAM_METADATA = 16,  //!< Program-level metadata (exec-class name, etc.)
    INIT_FUNCS       = 17,  //!< Init functions for constants/static vars with lowered init expressions
};

//! Value type tags for serialized constant values
enum class QoreAOTValueTag : uint8_t {
    VT_NOTHING    = 0,
    VT_NULL       = 1,
    VT_BOOL       = 2,
    VT_INT64      = 3,
    VT_FLOAT64    = 4,
    VT_STRING     = 5,
    VT_ABS_DATE   = 6,
    VT_REL_DATE   = 7,
    VT_LIST       = 8,
    VT_HASH       = 9,
    VT_NUMBER     = 10,
    VT_BINARY     = 11,
    //! Marks a default parameter value that is a complex expression (e.g. function call)
    //! and cannot be serialized as a constant value. Deserialized as boolean True
    //! to mark the parameter as optional in the function signature.
    VT_OPAQUE_DEFAULT = 12,
    //! Absolute date with region name for DST-aware timezone reconstruction
    VT_ABS_DATE_REGION = 13,
    //! Enum value: namespace path + member name
    VT_ENUM = 14,
};

//! Section header in the binary format
struct QoreAOTSectionHeader {
    uint16_t type;      //!< QoreAOTSectionType
    uint16_t reserved;  //!< reserved for future use
    uint32_t offset;    //!< byte offset from start of data area
    uint32_t size;      //!< size in bytes
};

//! Binary file header (60 bytes total, no version dispatch needed)
struct QoreAOTBinaryHeader {
    uint32_t magic;              //!< QORE_AOT_BINARY_MAGIC
    uint16_t version;            //!< QORE_AOT_BINARY_VERSION (always 1)
    uint16_t flags;              //!< QORE_AOT_FLAG_*
    int64_t parse_options_lo;    //!< low 64 bits of parse options (0-63)
    uint32_t section_count;      //!< number of sections
    uint32_t label_offset;       //!< offset into string pool for source label
    uint32_t label_length;       //!< length of source label
    uint16_t max_opcode_id;      //!< maximum IR opcode ID that this binary may use
    uint8_t qore_version_major;  //!< Qore version major that compiled this binary
    uint8_t qore_version_minor;  //!< Qore version minor
    uint16_t qore_version_patch; //!< Qore version patch
    uint8_t compression;         //!< compression method (0=none, 1=zlib)
    uint8_t reserved;            //!< reserved for future use (must be 0)
    int64_t parse_options_hi;    //!< high 64 bits of parse options (64-127)
    uint64_t source_hash;        //!< xxHash64 of source file bytes (0 = not set)
    uint64_t feature_flags;      //!< QORE_AOT_FEAT_* bitset of required IR features
};

//! String pool with deduplication for efficient string storage
class QoreAOTStringPool {
    std::vector<char> data;
    std::unordered_map<std::string, uint32_t> dedup;

public:
    QoreAOTStringPool() {
        // Reserve offset 0 for empty string
        data.push_back('\0');
    }

    //! Add a string to the pool and return its offset
    /** @param str the string to add (null-terminated)
        @return offset into pool data
    */
    uint32_t add(const char* str) {
        if (!str || !*str) {
            return 0;  // empty string at offset 0
        }
        std::string key(str);
        auto it = dedup.find(key);
        if (it != dedup.end()) {
            return it->second;
        }
        uint32_t offset = static_cast<uint32_t>(data.size());
        size_t len = key.size();
        data.insert(data.end(), str, str + len + 1);  // include null terminator
        dedup[std::move(key)] = offset;
        return offset;
    }

    //! Add a string with explicit length to the pool
    /** @param str pointer to string data
        @param len length of string (not including null terminator)
        @return offset into pool data
    */
    uint32_t add(const char* str, size_t len) {
        if (!str || len == 0) {
            return 0;
        }
        std::string key(str, len);
        auto it = dedup.find(key);
        if (it != dedup.end()) {
            return it->second;
        }
        uint32_t offset = static_cast<uint32_t>(data.size());
        data.insert(data.end(), str, str + len);
        data.push_back('\0');
        dedup[std::move(key)] = offset;
        return offset;
    }

    //! Get a string from the pool by offset
    /** @param offset byte offset into pool data
        @return null-terminated string, or nullptr if offset is out of range
    */
    const char* get(uint32_t offset) const {
        if (offset >= data.size()) {
            return nullptr;
        }
        return &data[offset];
    }

    //! Get the raw pool data
    const std::vector<char>& getData() const { return data; }

    //! Get the total size of the pool
    uint32_t size() const { return static_cast<uint32_t>(data.size()); }
};

//! Binary format writer for AOT metadata
class QoreAOTBinaryWriter {
public:
    QoreAOTStringPool strings;

private:
    std::vector<uint8_t> buffer;
    std::vector<QoreAOTSectionHeader> sections;

public:
    //! Write an unsigned 8-bit integer
    void writeU8(uint8_t v) {
        buffer.push_back(v);
    }

    //! Write an unsigned 16-bit integer (little-endian)
    void writeU16(uint16_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    //! Write an unsigned 32-bit integer (little-endian)
    void writeU32(uint32_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    //! Write a signed 64-bit integer (little-endian)
    void writeI64(int64_t v) {
        uint64_t uv;
        memcpy(&uv, &v, sizeof(uv));
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<uint8_t>((uv >> (i * 8)) & 0xFF));
        }
    }

    //! Write a 64-bit float (little-endian, IEEE 754)
    void writeF64(double v) {
        uint64_t bits;
        memcpy(&bits, &v, sizeof(bits));
        for (int i = 0; i < 8; ++i) {
            buffer.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
        }
    }

    //! Write raw bytes
    void writeBytes(const void* data, uint32_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), p, p + len);
    }

    //! Write a string reference (adds to pool, writes offset)
    void writeStringRef(const char* str) {
        uint32_t offset = strings.add(str);
        writeU32(offset);
    }

    //! Write a string reference with explicit length
    void writeStringRef(const char* str, size_t len) {
        uint32_t offset = strings.add(str, len);
        writeU32(offset);
    }

    //! Write a QoreValue (serializes constant value)
    /** Supports: nothing, null, bool, int64, float64, string, abs_date,
        rel_date, list, hash, number, binary.
        @param v the value to serialize
        @return true on success, false if value type is unsupported
    */
    bool writeValue(const QoreValue& v);

    //! Get current write position in buffer
    uint32_t position() const { return static_cast<uint32_t>(buffer.size()); }

    //! Overwrite a U32 at a previously recorded position (for patching size fields)
    void patchU32(uint32_t pos, uint32_t v) {
        assert(pos + 4 <= buffer.size());
        buffer[pos]     = static_cast<uint8_t>(v & 0xFF);
        buffer[pos + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        buffer[pos + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        buffer[pos + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }

    //! Begin a new section
    /** @param type the section type
        @return section index
    */
    uint32_t beginSection(QoreAOTSectionType type) {
        uint32_t idx = static_cast<uint32_t>(sections.size());
        QoreAOTSectionHeader hdr;
        hdr.type = static_cast<uint16_t>(type);
        hdr.reserved = 0;
        hdr.offset = position();
        hdr.size = 0;
        sections.push_back(hdr);
        return idx;
    }

    //! End a section (records its size)
    void endSection(uint32_t idx) {
        assert(idx < sections.size());
        sections[idx].size = position() - sections[idx].offset;
    }

    //! Finalize the binary and produce the complete output
    /** @param header the binary header to write
        @param output receives the complete binary blob
        @return true on success
    */
    bool finalize(const QoreAOTBinaryHeader& header, std::vector<uint8_t>& output);
};

//! Binary format reader for AOT metadata
class QoreAOTBinaryReader {
    const uint8_t* data = nullptr;
    uint32_t total_size = 0;

    // Parsed header
    QoreAOTBinaryHeader header;

    // Section directory
    std::vector<QoreAOTSectionHeader> sections;

    // String pool location
    const char* string_pool = nullptr;
    uint32_t string_pool_size = 0;

    // Data area start (after header + section directory)
    const uint8_t* data_area = nullptr;
    uint32_t data_area_size = 0;

    // Holds decompressed data if compression was used
    std::vector<uint8_t> decompressed_body;

public:
    //! Open and validate a binary blob
    /** @param data pointer to the binary data
        @param size size of the binary data
        @param error receives error message on failure
        @return true on success, false on failure
    */
    bool open(const uint8_t* data, uint32_t size, std::string& error);

    //! Get the parsed binary header
    const QoreAOTBinaryHeader& getHeader() const { return header; }

    //! Get the number of sections
    uint32_t getSectionCount() const { return static_cast<uint32_t>(sections.size()); }

    //! Get a section header by index
    const QoreAOTSectionHeader* getSection(uint32_t idx) const {
        if (idx >= sections.size()) {
            return nullptr;
        }
        return &sections[idx];
    }

    //! Find a section by type
    /** @param type the section type to find
        @return pointer to the section header, or nullptr if not found
    */
    const QoreAOTSectionHeader* findSection(QoreAOTSectionType type) const {
        for (auto& s : sections) {
            if (s.type == static_cast<uint16_t>(type)) {
                return &s;
            }
        }
        return nullptr;
    }

    //! Get a pointer to section data
    /** @param section the section header
        @return pointer to the section data, or nullptr if invalid
    */
    const uint8_t* getSectionData(const QoreAOTSectionHeader& section) const {
        if (!data_area || section.offset + section.size > data_area_size) {
            return nullptr;
        }
        return data_area + section.offset;
    }

    //! Get a string from the string pool
    /** @param offset byte offset into the string pool
        @return null-terminated string, or nullptr if offset is out of range
    */
    const char* getString(uint32_t offset) const {
        if (!string_pool || offset >= string_pool_size) {
            return nullptr;
        }
        return string_pool + offset;
    }

    //! Get the source label from the header
    const char* getLabel() const {
        return getString(header.label_offset);
    }

    //! Read an unsigned 8-bit integer from a data pointer
    static uint8_t readU8(const uint8_t*& ptr) {
        return *ptr++;
    }

    //! Read an unsigned 16-bit integer (little-endian) from a data pointer
    static uint16_t readU16(const uint8_t*& ptr) {
        uint16_t v = static_cast<uint16_t>(ptr[0])
                   | (static_cast<uint16_t>(ptr[1]) << 8);
        ptr += 2;
        return v;
    }

    //! Read an unsigned 32-bit integer (little-endian) from a data pointer
    static uint32_t readU32(const uint8_t*& ptr) {
        uint32_t v = static_cast<uint32_t>(ptr[0])
                   | (static_cast<uint32_t>(ptr[1]) << 8)
                   | (static_cast<uint32_t>(ptr[2]) << 16)
                   | (static_cast<uint32_t>(ptr[3]) << 24);
        ptr += 4;
        return v;
    }

    //! Read a signed 64-bit integer (little-endian) from a data pointer
    static int64_t readI64(const uint8_t*& ptr) {
        uint64_t uv = 0;
        for (int i = 0; i < 8; ++i) {
            uv |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        int64_t v;
        memcpy(&v, &uv, sizeof(v));
        return v;
    }

    //! Read a 64-bit float (little-endian, IEEE 754) from a data pointer
    static double readF64(const uint8_t*& ptr) {
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) {
            bits |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        double v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    }

    //! Read a string reference (offset into string pool) from a data pointer
    const char* readStringRef(const uint8_t*& ptr) const {
        uint32_t offset = readU32(ptr);
        return getString(offset);
    }

    //! Read a serialized QoreValue from a data pointer
    /** @param ptr data pointer (advanced past the value)
        @param end pointer past the end of valid data
        @param error receives error message on failure
        @return the deserialized value, or NOTHING on failure
    */
    QoreValue readValue(const uint8_t*& ptr, const uint8_t* end, std::string& error) const;

    //! Check if a data pointer range is valid
    bool checkRange(const uint8_t* ptr, uint32_t needed) const {
        return ptr && data_area && ptr >= data_area
            && (ptr + needed) <= (data_area + data_area_size);
    }
};

//! Type resolver: maps type path strings back to const QoreTypeInfo* pointers at runtime
class QoreAOTTypeResolver {
    QoreProgram* pgm;
    std::unordered_map<std::string, const QoreTypeInfo*> cache;

public:
    explicit QoreAOTTypeResolver(QoreProgram* pgm) : pgm(pgm) {}

    //! Resolve a type path string to a QoreTypeInfo pointer
    /** @param path the type path (from QoreTypeInfo::getPath())
        @param error receives error message on failure
        @return the resolved type, or nullptr on failure
    */
    const QoreTypeInfo* resolve(const char* path, std::string& error);

private:
    const QoreTypeInfo* resolveBuiltin(const char* path);
    const QoreTypeInfo* resolveClassType(const char* path);
    const QoreTypeInfo* resolveHashDeclType(const char* path);
    const QoreTypeInfo* resolveComplexType(const char* path);
};

//! Serialize the namespace tree metadata into binary sections
/** Writes all namespace structure sections (NAMESPACES, CLASSES, HASHDECLS,
    ENUMS, TYPEDEFS, CONSTANTS, GLOBALS, FUNCTIONS, METHODS) into the
    binary writer. Only user-defined (non-builtin) items are serialized.

    @param writer the binary writer to write to
    @param root_ns pointer to the root namespace private data
    @param module_name optional module name; when provided, only items belonging to this module are
           serialized (items from reexported dependencies are filtered out)
    @return true on success, false if serialization failed
*/
bool serializeNamespaceTree(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns,
    const char* module_name = nullptr,
    const std::unordered_set<std::string>* keep_modules = nullptr);

//! Serialize module dependencies into the DEPENDENCIES binary section
/** Writes all module dependencies (including reexport) so they can be loaded
    before deserializing the namespace tree in strip-source mode.

    @param writer the binary writer to write to
    @param dependencies vector of dependency module names
*/
void serializeDependencies(QoreAOTBinaryWriter& writer, const std::vector<std::string>& dependencies);

//! Read module dependencies from binary metadata
/** Reads the DEPENDENCIES section from serialized binary metadata.
    This should be called before deserializeIntoProgram() to load dependencies
    that are needed for namespace tree deserialization (e.g., base classes).

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param dependencies receives the list of dependency module names
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readDependencies(const uint8_t* data, uint32_t size, std::vector<std::string>& dependencies, std::string& error);

//! Serialize reexported module names into the REEXPORT_MODULES binary section
/** Writes the list of modules that should be reexported when this module is imported.
    When a compiled module is loaded as a binary module, the reexport mechanism from
    \%requires(reexport) must be preserved so that dependency namespaces (especially
    system classes from binary modules) are made available to importing programs.

    @param writer the binary writer to write to
    @param reexport_modules vector of module names to reexport
*/
void serializeReexportModules(QoreAOTBinaryWriter& writer, const std::vector<std::string>& reexport_modules);

//! Read reexported module names from binary metadata
/** Reads the REEXPORT_MODULES section from serialized binary metadata.

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param reexport_modules receives the list of reexported module names
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readReexportModules(const uint8_t* data, uint32_t size, std::vector<std::string>& reexport_modules, std::string& error);

//! Serialize program-level metadata into the PROGRAM_METADATA binary section
/** Writes exec-class name and other program-level settings.
    @param writer the binary writer to write to
    @param exec_class_name the exec-class name (empty string if not set)
*/
void serializeProgramMetadata(QoreAOTBinaryWriter& writer, const char* exec_class_name);

//! Read program-level metadata from binary metadata
/** Reads the PROGRAM_METADATA section from serialized binary metadata.
    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param exec_class_name receives the exec-class name (empty if not set)
    @param error receives error message on failure
    @return true on success, false on failure (missing section is not an error)
*/
bool readProgramMetadata(const uint8_t* data, uint32_t size, std::string& exec_class_name, std::string& error);

/** Read fallback source from a v2 AOT binary metadata blob without full deserialization.
    @param data pointer to the metadata blob
    @param size size of the metadata blob
    @param source receives the fallback source text (empty if not present)
    @param source_len receives the fallback source length
    @param error receives error message on failure
    @return true on success, false on failure
*/
bool readFallbackSource(const uint8_t* data, uint32_t size, const char*& source, size_t& source_len, std::string& error);

// ---- Slot Map Serialization (Phase 5) ----

//! Expression identity kinds for serialized slot maps
enum class AOTExprKind : uint8_t {
    FUNC_CALL          = 1,   //!< Regular function call: ref1=function_name
    SELF_METHOD_CALL   = 2,   //!< Self method call: ref1=class_path, ref2=method_name
    STATIC_METHOD_CALL = 3,   //!< Static method call: ref1=class_path, ref2=method_name
    NEW_OBJECT         = 4,   //!< New object constructor: ref1=class_name
    RUNTIME_CONST_REF  = 5,   //!< Runtime constant reference: ref1=const_name
    SELF_VARREF        = 6,   //!< Self variable reference (self keyword)
    LOCAL_VARREF       = 7,   //!< Local variable reference: ref1=local_slot_index (as string)
    GLOBAL_VARREF      = 8,   //!< Global variable reference: ref1=global_slot_index (as string)
    CONST_NUMBER       = 9,   //!< Number constant: ref1=string representation
    CONST_BINARY       = 10,  //!< Binary constant: ref1=hex-encoded bytes
    CLOSURE_CREATE     = 11,  //!< Closure/lambda: ref1=enclosing class name (empty if none)
    CALL_REF           = 12,  //!< Call reference call: ref1=function_name (if function ref)
    OBJ_METHOD_REF     = 13,  //!< Object method reference: ref1=method_name
    STATIC_VARREF      = 14,  //!< Static class variable: ref1=class_name, ref2=var_name
    SCOPED_NEW_OBJECT  = 15,  //!< Scoped new object: ref1=class_name
    HASHDECL_NEW       = 16,  //!< Hashdecl construction: ref1=hashdecl_path
    COMPLEX_HASH_NEW   = 17,  //!< Complex hash construction: ref1=type_path
    COMPLEX_LIST_NEW   = 18,  //!< Complex list construction: ref1=type_path
    CONST_ENUM         = 19,  //!< Enum constant: ref1=enum_path, ref2=member_name
    CONST_STRING       = 20,  //!< String constant: ref1=string content
    HASH_LITERAL       = 21,  //!< Hash literal: num_pairs(u8) + [key_str(stringref) + value(AOTExprKind)] * N
    HASH_DEREF         = 22,  //!< Hash/object dereference: left(AOTExprKind) + right(AOTExprKind)
    PARSE_REF          = 23,  //!< Parse reference (\var): inner_lvalue(AOTExprKind)
    CAST_HASHDECL      = 24,  //!< Hashdecl cast: ref1=hashdecl_path, u8 or_nothing
    CAST_COMPLEX_HASH  = 25,  //!< Complex hash cast: ref1=type_path, u8 or_nothing
    CAST_COMPLEX_LIST  = 26,  //!< Complex list cast: ref1=type_path, u8 or_nothing
    CAST_CLASS         = 27,  //!< Class cast: ref1=class_path, u8 or_nothing
    CAST_ENUM          = 28,  //!< Enum cast: ref1=enum_path, u8 or_nothing
    CONST_INT          = 29,  //!< Integer constant: i64 value (8 bytes LE)
    CONST_FLOAT        = 30,  //!< Float constant: f64 value (8 bytes LE, IEEE 754)
    CONST_BOOL         = 31,  //!< Boolean constant: u8 value (0 or 1)
    CONST_NOTHING      = 32,  //!< Nothing constant: no data
    LIST_LITERAL       = 33,  //!< List literal: count(u8) + [value(AOTExprKind)] * N
    CONST_NULL         = 34,  //!< NULL constant: no data
    EXPR_TREE          = 0xFE, //!< Recursive expression tree: binary blob (inline bytes)
    GENERIC_EVAL       = 0xFF //!< Unsupported expression — function needs source fallback
};

//! Node kinds for recursive expression tree serialization (EXPR_TREE blobs)
/** Each node in the tree blob has: kind (u8), metadata (variable), num_children (u16), children (recursive).
    Metadata format varies by kind. Strings are u16-length-prefixed.
*/
enum class AOTExprNodeKind : uint8_t {
    // Leaf constants (0 children)
    EN_NOTHING       = 0,   //!< QoreValue nothing
    EN_NULL          = 1,   //!< QoreValue null
    EN_INT           = 2,   //!< i64 value
    EN_FLOAT         = 3,   //!< f64 value
    EN_STRING        = 4,   //!< u16 len + bytes
    EN_BOOL          = 5,   //!< u8 (0/1)
    EN_NUMBER        = 6,   //!< u16 len + bytes (string repr)
    EN_BINARY        = 7,   //!< u32 len + bytes

    // Leaf references (0 children)
    EN_LOCAL_VAR     = 10,  //!< u16 slot_index
    EN_GLOBAL_VAR    = 11,  //!< u16 name_len + bytes
    EN_SELF_REF      = 12,  //!< u16 name_len + bytes (member name)
    EN_STATIC_VAR    = 13,  //!< u16 class_len + bytes + u16 var_len + bytes
    EN_CONST_REF     = 14,  //!< u16 name_len + bytes (fully qualified)

    // Call nodes (children = args)
    EN_FUNC_CALL     = 20,  //!< u16 name_len + bytes; children = args
    EN_SELF_CALL     = 21,  //!< u16 class_len + bytes + u16 method_len + bytes; children = args
    EN_STATIC_CALL   = 22,  //!< u16 class_len + bytes + u16 method_len + bytes; children = args
    EN_DOT_EVAL      = 23,  //!< u16 method_len + bytes; children[0] = target, [1..] = args
    EN_NEW           = 24,  //!< u16 class_len + bytes; children = args
    EN_CALLREF_CALL  = 25,  //!< no metadata; children[0] = callref, [1..] = args
    EN_SCOPED_NEW    = 26,  //!< u16 class_len + bytes; children = args

    // Access operators (children = operands)
    EN_HASH_DEREF    = 30,  //!< children = [target, key_expr]
    EN_SQUARE_BRKT   = 31,  //!< children = [target, index_expr]

    // Unary operators (1 child = operand)
    EN_KEYS          = 40,  //!< no metadata
    EN_ELEMENTS      = 41,  //!< no metadata
    EN_EXISTS        = 42,  //!< no metadata
    EN_DELETE        = 43,  //!< no metadata
    EN_REMOVE        = 44,  //!< no metadata
    EN_BACKGROUND    = 45,  //!< no metadata
    EN_RESERVED_46   = 46,  //!< reserved for binary compatibility (previously EN_TYPEOF, no Qore operator)
    EN_TRIM          = 47,  //!< no metadata
    EN_CHOMP         = 48,  //!< no metadata
    EN_POP           = 49,  //!< no metadata
    EN_INSTANCEOF    = 50,  //!< u16 type_path_len + bytes
    EN_UNARY_MINUS   = 51,  //!< no metadata
    EN_UNARY_PLUS    = 52,  //!< no metadata
    EN_LOG_NOT       = 53,  //!< no metadata
    EN_BIT_NOT       = 54,  //!< no metadata
    EN_SHIFT         = 55,  //!< no metadata (shift list)

    // Binary operators (2 children = [left, right])
    EN_PUSH          = 60,  //!< no metadata
    EN_UNSHIFT       = 61,  //!< no metadata
    EN_LIST_ASSIGN   = 62,  //!< no metadata
    EN_PLUS          = 63,  //!< no metadata
    EN_MINUS         = 64,  //!< no metadata
    EN_MULTIPLY      = 65,  //!< no metadata
    EN_DIVIDE        = 66,  //!< no metadata
    EN_MODULO        = 67,  //!< no metadata
    EN_SHIFT_LEFT    = 68,  //!< no metadata
    EN_SHIFT_RIGHT   = 69,  //!< no metadata
    EN_BIT_AND       = 70,  //!< no metadata
    EN_BIT_OR        = 71,  //!< no metadata
    EN_BIT_XOR       = 72,  //!< no metadata
    EN_LOG_CMP       = 73,  //!< no metadata (<=>)
    EN_LOG_AND       = 74,  //!< no metadata (&&)
    EN_LOG_OR        = 75,  //!< no metadata (||)
    EN_LOG_EQ        = 76,  //!< no metadata (==)
    EN_LOG_NE        = 77,  //!< no metadata (!=)
    EN_LOG_AEQ       = 78,  //!< no metadata (===)
    EN_LOG_ANE       = 79,  //!< no metadata (!==)
    EN_LOG_LT        = 80,  //!< no metadata (<)
    EN_LOG_GT        = 81,  //!< no metadata (>)
    EN_LOG_LE        = 82,  //!< no metadata (<=)
    EN_LOG_GE        = 83,  //!< no metadata (>=)
    EN_NULL_COAL     = 84,  //!< no metadata (??)
    EN_VAL_COAL      = 85,  //!< no metadata (?*)
    EN_QUESTION      = 86,  //!< 3 children: [cond, true_expr, false_expr]
    EN_RANGE         = 87,  //!< 3 children: [start, stop, step]

    // Regex operators (1 child = operand)
    EN_REGEX_MATCH   = 90,  //!< u16 pattern_len + bytes + i64 options
    EN_REGEX_NMATCH  = 91,  //!< u16 pattern_len + bytes + i64 options
    EN_REGEX_EXTRACT = 92,  //!< u16 pattern_len + bytes + i64 options
    EN_REGEX_SUBST   = 93,  //!< u16 pat_len + bytes + u16 repl_len + bytes + i64 options + u8 global
    EN_TRANSLIT      = 94,  //!< u16 src_len + bytes + u16 tgt_len + bytes

    // Special nodes
    EN_OBJ_METH_REF  = 100, //!< u16 method_len + bytes; 1 child = target expr
    EN_SELF_METH_REF = 101, //!< u16 method_len + bytes; 0 children
    EN_CLOSURE       = 102, //!< u32 expr_slot_index; 0 children — references CLOSURE_CREATE expr slot
    EN_FUNC_REF      = 103, //!< u16 name_len + bytes; 0 children (function reference)
    EN_STATIC_METH_REF = 104, //!< u16 class_len + bytes + u16 method_len + bytes; 0 children
    EN_BOUND_METH_REF  = 105, //!< u16 class_len + bytes + u16 method_len + bytes; 0 children (bound to self)

    // Assignment (2 children = [lvalue, rvalue])
    EN_ASSIGN        = 110, //!< no metadata
    EN_PLUS_EQ       = 111, //!< no metadata
    EN_MINUS_EQ      = 112, //!< no metadata
    EN_MULTIPLY_EQ   = 113, //!< no metadata
    EN_DIVIDE_EQ     = 114, //!< no metadata
    EN_MODULO_EQ     = 115, //!< no metadata
    EN_AND_EQ        = 116, //!< no metadata
    EN_OR_EQ         = 117, //!< no metadata
    EN_XOR_EQ        = 118, //!< no metadata
    EN_SHL_EQ        = 119, //!< no metadata
    EN_SHR_EQ        = 120, //!< no metadata
    EN_PRE_INC       = 121, //!< 1 child (lvalue)
    EN_PRE_DEC       = 122, //!< 1 child (lvalue)
    EN_POST_INC      = 123, //!< 1 child (lvalue)
    EN_POST_DEC      = 124, //!< 1 child (lvalue)

    // Multi-child
    EN_EXTRACT       = 130, //!< 2-4 children: [lvalue, offset, [length, [new_val]]]
    EN_SPLICE        = 131, //!< 2-4 children: same as extract
    EN_PARSE_LIST    = 132, //!< N children (internal arg list)

    // Cast
    EN_CAST          = 140, //!< u16 type_path_len + bytes + u8 or_nothing; 1 child

    // Literal collections
    EN_LIST          = 150, //!< N children (list elements)
    EN_HASH          = 151, //!< u16 num_keys; for each: u16 key_len + bytes, then 1 child (value)

    // Implicit arguments ($1, $2, $#)
    EN_IMPLICIT_ARG  = 152, //!< i16 offset (-1 = $argv, 0 = $1, 1 = $2, ...); 0 children
    EN_IMPLICIT_ELEM = 153, //!< $# (implicit element index); 0 children

    // Reference to lvalue (\var)
    EN_REF_TO_LVALUE = 154, //!< 1 child (lvalue expression)

    // Parse-time hash literal (key expressions + value expressions)
    EN_PARSE_HASH    = 156, //!< u16 num_entries; for each: 1 child (key expr) + 1 child (value expr)

    // Square brackets range (x[m..n])
    EN_SQ_BRKT_RANGE = 155, //!< 3 children: [target, start, end]

    // List processing operators
    EN_MAP           = 160, //!< 2 children: [map_expr, source]
    EN_MAP_SELECT    = 161, //!< 3 children: [map_expr, source, where_expr]
    EN_HASH_MAP      = 162, //!< 3 children: [key_expr, val_expr, source]
    EN_HASH_MAP_SELECT = 163, //!< 4 children: [key_expr, val_expr, source, where_expr]
    EN_DATE          = 8,   //!< u8 is_relative + date data; 0 children
    EN_ENUM          = 9,   //!< u16 enum_path_len + bytes + u16 member_name_len + bytes; 0 children

    EN_FOLDL         = 164, //!< 2 children: [accumulator_expr, source]
    EN_FOLDR         = 165, //!< 2 children: [accumulator_expr, source]
};

//! Identity for a local variable slot
struct AOTLocalSlotId {
    std::string name;        //!< variable name
    std::string type_path;   //!< type path from QoreTypeInfo::getPath()
    uint8_t flags = 0;       //!< bit 0: is_param, bit 1: is_closure, bit 2: is_self, bit 3: is_argv
    uint16_t param_index = 0;//!< parameter index (valid only if is_param flag set)
};

//! Identity for a global variable slot
struct AOTGlobalSlotId {
    std::string name;        //!< qualified variable name
    std::string type_path;   //!< type path
    bool is_thread_local = false; //!< true if thread-local variable
};

//! Identity for an expression slot
class UserClosureFunction;

struct AOTExprSlotId {
    AOTExprKind kind = AOTExprKind::GENERIC_EVAL; //!< expression kind
    std::string ref1;        //!< kind-specific: function name or class path
    std::string ref2;        //!< kind-specific: method name (for method calls)
    const QoreListNode* call_args = nullptr; //!< call args for NEW_OBJECT/SCOPED_NEW_OBJECT/STATIC_METHOD_CALL
    const UserClosureFunction* closure_func = nullptr; //!< For CLOSURE_CREATE: source closure function
};

//! Identity for a body local variable (needed for instantiation management)
struct AOTBodyLocalId {
    std::string name;        //!< variable name
    std::string type_path;   //!< type path
    bool is_closure = false; //!< true if closure variable
};

//! Identity for a regex case slot
struct AOTRegexCaseSlotId {
    std::string pattern;     //!< regex pattern string (from re->getPatternCStr())
    int64_t options = 0;     //!< PCRE2 options (from re->getOptions())
    bool is_negated = false; //!< true for CaseNodeNegRegex (~! match)
};

//! Complete slot identity set for a single compiled function
struct AOTSlotIdentities {
    std::vector<AOTLocalSlotId> locals;   //!< indexed by local slot index
    std::vector<AOTGlobalSlotId> globals; //!< indexed by global slot index
    std::vector<AOTExprSlotId> exprs;     //!< indexed by expression slot index
    std::vector<AOTBodyLocalId> body_locals; //!< body locals in order
    std::vector<AOTRegexCaseSlotId> regex_cases; //!< indexed by regex case slot index
    bool has_unsupported_exprs = false;   //!< true if any expression is GENERIC_EVAL
    bool has_closure_exprs = false;       //!< true if any expression is CLOSURE_CREATE
};

//! Descriptor for a compiled function with slot identities
struct AOTCompiledFuncWithSlots {
    std::string name;                //!< AOT function name (e.g. "myFunc", "MyClass::method")
    int num_locals = 0;              //!< number of local variable slots
    int num_globals = 0;             //!< number of global variable slots
    int num_exprs = 0;               //!< number of expression slots
    int num_stmts = 0;               //!< number of statement slots (OnBlockExit)
    int num_regex_cases = 0;         //!< number of regex case slots (SwitchRegexMatch)
    AOTSlotIdentities slot_ids;      //!< extracted slot identities
    //! Handler IR functions for each statement slot (indexed by stmt slot index).
    //! Non-null entries have serializable handler IR; null entries need AST fallback.
    std::vector<const QoreIRFunction*> handler_irs;
};

//! Descriptor for a compiled constant/static-var init function
struct AOTCompiledInitFunc {
    std::string name;               //!< init function name (e.g. "__const_init::Ns::ConstName")
    std::string llvm_symbol;        //!< LLVM symbol name in the module
    int num_locals = 0;
    int num_globals = 0;
    int num_exprs = 0;
    int num_stmts = 0;
    int num_regex_cases = 0;
    AOTSlotIdentities slot_ids;
    uint64_t feature_flags = 0;

    //! Target type for the init function result
    enum TargetType : uint8_t {
        NS_CONSTANT = 0,      //!< namespace-level constant
        CLASS_CONSTANT = 1,   //!< class-level constant
        STATIC_VAR = 2,       //!< static class variable
    };
    TargetType target_type = NS_CONSTANT;
    std::string ns_path;            //!< namespace path or class path
    std::string item_name;          //!< constant or variable name

    //! Names of other init functions this one depends on (for topological sort)
    std::vector<std::string> deps;
};

//! Serialize slot maps for compiled functions into the SLOT_MAPS binary section
/** @param writer the binary writer to write to
    @param funcs vector of compiled function descriptors with slot identities
    @param const_reverse_map optional reverse map for constant node → FQN resolution
*/
bool serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs,
    const AOTConstantReverseMap* const_reverse_map, std::string& error);

//! Serialize per-function source fallback into the FUNC_SOURCES binary section
/** Functions with unsupported expression types need the full source text
    for fallback context building at runtime (re-parse + IR re-lowering).
    If no functions need fallback, this section is omitted entirely.

    @param writer the binary writer to write to
    @param funcs vector of compiled function descriptors with slot identities
    @param source_text the full source text for fallback re-parsing
    @param source_len the length of the source text
*/
void serializeFallbackSources(QoreAOTBinaryWriter& writer,
    const std::vector<AOTCompiledFuncWithSlots>& funcs,
    const char* source_text, int source_len);

//! Serialize init function descriptors into the INIT_FUNCS binary section
/** Each entry maps an init function name to its target (namespace constant,
    class constant, or static variable). The init functions themselves are
    registered via the SLOT_MAPS section like regular AOT functions.
*/
void serializeInitFuncs(QoreAOTBinaryWriter& writer,
    const std::vector<AOTCompiledInitFunc>& init_funcs);

//! Descriptor for a deserialized init function (read from INIT_FUNCS section)
struct AOTInitFuncDescriptor {
    std::string name;                               //!< init function name (matches QoreAOTFunc::name)
    AOTCompiledInitFunc::TargetType target_type;    //!< what the init function initializes
    std::string ns_path;                            //!< namespace path or class path
    std::string item_name;                          //!< constant or variable name
};

//! Read init function descriptors from binary metadata
/** Reads the INIT_FUNCS section from serialized binary metadata.
    Returns the list of init function descriptors that map init function names
    to their target constants/static vars.

    @param data pointer to the binary metadata blob
    @param size size of the binary metadata blob
    @param init_funcs receives the list of init function descriptors
    @param error receives error message on failure
    @return true on success (even if section is absent), false on failure
*/
bool readInitFuncs(const uint8_t* data, uint32_t size,
    std::vector<AOTInitFuncDescriptor>& init_funcs, std::string& error);

// ---- Namespace Deserialization (Phase 4) ----

class QoreClass;

//! Deserializer: reconstructs namespace tree from binary metadata (replaces parse())
/** Reads the binary metadata sections and creates namespace tree elements
    within an existing QoreProgram, including:
    - Namespace hierarchy
    - Classes with members, base classes, and constants
    - Hashdecls, enums, typedefs
    - Constants and global variables
    - Functions with proper UserSignature (no bodies)
    - Methods on classes with proper UserSignature (no bodies)
*/
class QoreAOTBinaryDeserializer {
    QoreAOTBinaryReader reader;
    QoreAOTTypeResolver* type_resolver = nullptr;
    QoreProgram* pgm = nullptr;

    // Index maps: serialized index → created object
    std::vector<qore_ns_private*> ns_list;
    std::vector<QoreClass*> class_list;

    // Source fallback data (from FUNC_SOURCES section)
    const char* fallback_source = nullptr;       //!< full source text for fallback parsing
    size_t fallback_source_len = 0;              //!< length of fallback source text
    std::vector<std::string> fallback_func_names; //!< names of functions needing source fallback

    // Classes that already existed in the program (from module loading)
    // — skip methods/members for these since they're already committed
    std::unordered_set<uint32_t> preexisting_classes;

    // Pending base class info for two-pass class resolution
    struct PendingBaseClass {
        std::string base_path;
        uint8_t access;  //!< ClassAccess value for the base class inheritance
        bool is_virtual;
    };
    std::vector<std::vector<PendingBaseClass>> pending_bases;

    // Pending instance member info for two-pass class resolution
    struct PendingInstanceMember {
        std::string name;
        std::string type_path;
        uint8_t access;
        QoreValue default_val;
    };
    std::vector<std::vector<PendingInstanceMember>> pending_instance_members;

    // Pending static member info for two-pass class resolution
    struct PendingStaticMember {
        std::string name;
        std::string type_path;
        uint8_t access;
    };
    std::vector<std::vector<PendingStaticMember>> pending_static_members;

    // Pending class constant info for two-pass class resolution
    struct PendingClassConstant {
        std::string name;
        std::string type_path;
        uint8_t access;
        QoreValue value;
    };
    std::vector<std::vector<PendingClassConstant>> pending_class_constants;

    // Pending hashdecl member info for two-pass resolution
    struct PendingHashdeclMember {
        std::string name;
        std::string type_path;
        QoreValue default_val;
    };
    // Map from hashdecl pointer to pending members
    std::vector<std::pair<TypedHashDecl*, std::vector<PendingHashdeclMember>>> pending_hashdecl_members;

    // Pending typedef info for two-pass resolution
    struct PendingTypedef {
        std::string name;
        std::string type_path;
        uint32_t ns_idx;
        bool is_pub;
    };
    std::vector<PendingTypedef> pending_typedefs;

    // Pending enum base type info for two-pass resolution
    struct PendingEnumBaseType {
        QoreEnumDecl* ed;
        std::string base_type_path;
    };
    std::vector<PendingEnumBaseType> pending_enum_base_types;

    bool deserializeNamespaces(std::string& error);
    bool deserializeClasses(std::string& error);
    bool resolveClassBases(std::string& error);
    bool resolveInstanceMembers(std::string& error);
    bool importInheritedMembers(std::string& error);
    bool resolveStaticMembers(std::string& error);
    bool resolveClassConstants(std::string& error);
    bool resolveHashdeclMembers(std::string& error);
    bool resolveTypedefs(std::string& error);
    bool resolveEnumBaseTypes(std::string& error);
    bool deserializeHashDecls(std::string& error);
    bool deserializeEnums(std::string& error);
    bool deserializeTypedefs(std::string& error);
    bool deserializeConstants(std::string& error);
    bool deserializeGlobals(std::string& error);
    bool deserializeFunctions(std::string& error);
    bool deserializeMethods(std::string& error);
    bool deserializeFallbackSources(std::string& error);
    bool commitDeserializedClasses(std::string& error);

public:
    ~QoreAOTBinaryDeserializer() {
        delete type_resolver;
        // Clean up any pending QoreValues that weren't transferred to the namespace tree
        // (only fires on early-return error paths; normal path transfers ownership)
        for (auto& class_members : pending_instance_members) {
            for (auto& pim : class_members) {
                pim.default_val.discard(nullptr);
            }
        }
        for (auto& class_consts : pending_class_constants) {
            for (auto& pcc : class_consts) {
                pcc.value.discard(nullptr);
            }
        }
        for (auto& hd_pair : pending_hashdecl_members) {
            for (auto& phm : hd_pair.second) {
                phm.default_val.discard(nullptr);
            }
        }
    }

    //! Deserialize binary metadata into a QoreProgram's namespace tree
    /** @param pgm the target QoreProgram (must be set up with parse options)
        @param data pointer to the binary metadata blob
        @param size size of the binary metadata blob
        @param error receives error message on failure
        @return true on success, false on failure
    */
    bool deserializeIntoProgram(QoreProgram* pgm, const uint8_t* data, uint32_t size, std::string& error);

    //! Get the reader for access to header info after deserialization
    const QoreAOTBinaryReader& getReader() const { return reader; }

    //! Check if any functions need source fallback
    bool hasFallbackSource() const { return fallback_source != nullptr; }

    //! Get the fallback source text (for re-parsing fallback functions)
    const char* getFallbackSource() const { return fallback_source; }

    //! Get the fallback source text length
    size_t getFallbackSourceLen() const { return fallback_source_len; }

    //! Get the list of function names that need source fallback
    const std::vector<std::string>& getFallbackFuncNames() const { return fallback_func_names; }

    //! Check if a specific function needs source fallback
    bool needsFallback(const char* func_name) const {
        for (auto& name : fallback_func_names) {
            if (name == func_name) {
                return true;
            }
        }
        return false;
    }
};

// ---- IR Function Serialization (Phase 5) ----

//! Instruction group tag for IR function serialization
/** Identifies the instruction subclass for correct serialization/deserialization
    of subclass-specific fields. Stored as a u8 in the binary format.
*/
enum class QoreIRInstGroup : uint8_t {
    Base = 0,               //!< QoreIRInstruction — no extra fields
    Const = 1,              //!< QoreIRConstInstruction
    Branch = 2,             //!< QoreIRBranchInstruction
    BranchIf = 3,           //!< QoreIRBranchIfInstruction
    Return = 4,             //!< QoreIRReturnInstruction
    Throw = 5,              //!< QoreIRThrowInstruction
    Local = 6,              //!< QoreIRLocalInstruction
    Var = 7,                //!< QoreIRVarInstruction
    LValue = 8,             //!< QoreIRLValueInstruction
    Expr = 9,               //!< QoreIRExprInstruction
    CallDirect = 10,        //!< QoreIRCallDirectInstruction
    CallMethodDirect = 11,  //!< QoreIRCallMethodDirectInstruction
    InvokeMethodDirect = 12,//!< QoreIRInvokeMethodDirectInstruction
    CallStaticDirect = 13,  //!< QoreIRCallStaticDirectInstruction
    DotEvalMethodDirect = 14, //!< QoreIRDotEvalMethodDirectInstruction
    InvokeDotEvalMethodDirect = 15, //!< QoreIRInvokeDotEvalMethodDirectInstruction
    Invoke = 16,            //!< QoreIRInvokeInstruction
    ScopeEnter = 17,        //!< QoreIRScopeEnterInstruction
    ScopeExit = 18,         //!< QoreIRScopeExitInstruction
    LandingPad = 19,        //!< QoreIRLandingPadInstruction
    SwitchInt = 20,         //!< QoreIRSwitchIntInstruction
    SwitchString = 21,      //!< QoreIRSwitchStringInstruction
    Phi = 22,               //!< QoreIRPhiInstruction
    Guard = 23,             //!< QoreIRGuardInstruction
    ImplicitArg = 24,       //!< QoreIRImplicitArgInstruction
    HashKeyAccess = 25,     //!< QoreIRHashKeyAccessInstruction
    SelfMember = 26,        //!< QoreIRSelfMemberInstruction
    StaticVar = 27,         //!< QoreIRStaticVarInstruction
    NewObject = 28,         //!< QoreIRNewObjectInstruction
    LoadConst = 29,         //!< QoreIRLoadConstantInstruction
    CreateClosure = 30,     //!< QoreIRCreateClosureInstruction
    CreateCallRef = 31,     //!< QoreIRCreateCallRefInstruction
    CreateMethodRef = 32,   //!< QoreIRCreateMethodRefInstruction
    CreateParseRef = 33,    //!< QoreIRCreateParseRefInstruction
    NewHashDecl = 34,       //!< QoreIRNewHashDeclInstruction
    NewComplexHash = 35,    //!< QoreIRNewComplexHashInstruction
    NewComplexList = 36,    //!< QoreIRNewComplexListInstruction
    VrnConstruct = 37,      //!< QoreIRVrnConstructInstruction
    HashKeyStore = 38,      //!< QoreIRHashKeyStoreInstruction
    ListIndexStore = 39,    //!< QoreIRListIndexStoreInstruction
    FusedAddLocal = 40,     //!< QoreIRAddAssignLocalIntInstruction
    FusedIncLocal = 41,     //!< QoreIRIncrementLocalIntInstruction
    FusedBrLtLocal = 42,    //!< QoreIRBranchIfLtLocalIntInstruction
    MapHashKey = 43,        //!< QoreIRMapHashKeyInstruction
    // Foreach = 44,        // REMOVED: fully lowered to iterators
    OnBlockExit = 45,       //!< QoreIROnBlockExitInstruction
    IteratorCreate = 46,    //!< QoreIRIteratorCreateInstruction
    IteratorNext = 47,      //!< QoreIRIteratorNextInstruction
    SwitchRegexMatch = 48,  //!< QoreIRSwitchRegexMatchInstruction
    RefForeachInit = 49,    //!< QoreIRRefForeachInitInstruction
    // Debug = 50,          // REMOVED: fully lowered inline
    // Assert = 51,         // REMOVED: fully lowered inline
    MakeHashConstKeys = 52, //!< QoreIRMakeHashConstKeysInstruction
    SwitchCaseMatch = 53,   //!< QoreIRSwitchCaseMatchInstruction
    Context = 54,           //!< QoreIRContextInstruction
    Summarize = 55,         //!< QoreIRSummarizeInstruction
    ListIndexAccess = 56,   //!< QoreIRListIndexAccessInstruction
    Unsupported = 0xFF,     //!< Instruction cannot be serialized
};

//! Callback for serializing AST expressions embedded in IR instructions
/** Called for each QoreValue expr/lvalue field that needs serialization.
    Must write the expression data in AOTExprKind format (u8 kind + kind-specific refs).
    @param writer binary writer
    @param expr the AST expression to serialize
    @return true on success, false if expression cannot be serialized
*/
typedef std::function<bool(QoreAOTBinaryWriter& writer, const QoreValue& expr)> AOTExprWriteFunc;

//! Callback for deserializing AST expressions embedded in IR instructions
/** Reads expression data in AOTExprKind format and reconstructs the AST expression.
    @param reader binary reader (for string pool access)
    @param ptr data pointer (advanced past the expression data)
    @param end pointer past end of valid data
    @param error receives error message on failure
    @return the reconstructed expression, or NOTHING on failure
*/
typedef std::function<QoreValue(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
    const uint8_t* end, std::string& error)> AOTExprReadFunc;

//! Serialize a QoreIRFunction to binary format
/** Writes a compact binary representation of the IR function that can be
    deserialized at runtime to reconstruct the function for IR interpreter execution.

    Binary format:
    - Function header: name, max_value_id, max_local_slot_id, num_guards, return_type, block/local counts
    - Local variable slot table: name, type_path, slot_id for each local
    - Body locals list: name, type_path for each body local
    - Blocks: for each block, name, is_loop_header, instructions
    - Instructions: opcode, group_tag, result, operands, exception_target, group-specific fields

    @param writer binary writer (uses string pool and buffer)
    @param func the IR function to serialize
    @param writeExpr callback for serializing AST expression fields
    @return true on success, false if any instruction cannot be serialized
*/
bool serializeIRFunction(QoreAOTBinaryWriter& writer, const QoreIRFunction& func,
    const AOTExprWriteFunc& writeExpr);

//! Deserialize a QoreIRFunction from binary data
/** Reads binary data written by serializeIRFunction() and reconstructs a
    QoreIRFunction suitable for IR interpreter execution.

    @param reader binary reader (for string pool access)
    @param ptr data pointer (advanced past the function data on success)
    @param end pointer past end of valid data
    @param pgm the QoreProgram for namespace/type resolution
    @param readExpr callback for deserializing AST expression fields
    @param enclosing_locals optional name→LocalVar* map from the enclosing function's scope
    @param error receives error message on failure
    @return reconstructed function, or nullptr on failure
*/
std::unique_ptr<QoreIRFunction> deserializeIRFunction(
    const QoreAOTBinaryReader& reader,
    const uint8_t*& ptr,
    const uint8_t* end,
    QoreProgram* pgm,
    const AOTExprReadFunc& readExpr,
    const std::unordered_map<std::string, LocalVar*>* enclosing_locals,
    std::string& error);

//! Compress metadata blob using zlib
/** Compresses the serialized metadata blob to reduce size and LLVM compilation overhead.
    The compressed data includes the original size as a 4-byte little-endian prefix.

    @param input the uncompressed metadata blob
    @param output receives the compressed data
    @param error receives error message on failure
    @return true on success, false on compression failure
*/
bool compressMetadata(const std::vector<uint8_t>& input,
    std::vector<uint8_t>& output,
    std::string& error);

//! Decompress metadata blob using zlib
/** Decompresses metadata previously compressed by compressMetadata().

    @param input pointer to compressed data
    @param input_len length of compressed data
    @param output receives the decompressed metadata
    @param error receives error message on failure
    @return true on success, false on decompression failure
*/
bool decompressMetadata(const uint8_t* input, size_t input_len,
    std::vector<uint8_t>& output,
    std::string& error);

// ============================================================================
// Internal Expression Handler Helpers (Phase 3.2+)
// ============================================================================
// These functions are used by expression registry handlers for recursive
// serialization/deserialization of nested expressions.

//! Classify and write a QoreValue expression in AOTExprKind format
//! @param writer binary writer to write to
//! @param expr the expression to serialize
//! @param parent_locals parent function's local slot metadata
//! @param parent_globals parent function's global slot metadata
//! @param const_reverse_map reverse map for constant lookup (optional)
//! @return true if expression was successfully serialized, false otherwise
bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map = nullptr);

//! Read one expression from inline closure/handler IR binary data
//! @param rdr binary reader for reading data
//! @param p current read pointer (advanced by reading)
//! @param e end of valid data
//! @param err set to error message on failure
//! @param pgm the Qore program for symbol resolution
//! @param locals LocalVar* array for LOCAL_VARREF resolution (may be null)
//! @param num_locals number of entries in locals
//! @param globals Var* array for GLOBAL_VARREF resolution (may be null)
//! @param num_globals number of entries in globals
//! @return reconstructed expression, or NOTHING on failure
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals);

#endif
