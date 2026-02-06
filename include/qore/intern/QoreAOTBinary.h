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
#include <unordered_map>
#include <vector>

class QoreValue;
class QoreProgram;
class QoreTypeInfo;
class qore_ns_private;

//! Magic number: "QORD" in big-endian (0x514F5244)
constexpr uint32_t QORE_AOT_BINARY_MAGIC = 0x514F5244;

//! Current binary format version
constexpr uint16_t QORE_AOT_BINARY_VERSION = 1;

//! Binary header flags
constexpr uint16_t QORE_AOT_FLAG_HAS_TOPLEVEL = 0x0001;
constexpr uint16_t QORE_AOT_FLAG_IS_MODULE    = 0x0002;

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
};

//! Section header in the binary format
struct QoreAOTSectionHeader {
    uint16_t type;      //!< QoreAOTSectionType
    uint16_t reserved;  //!< reserved for future use
    uint32_t offset;    //!< byte offset from start of data area
    uint32_t size;      //!< size in bytes
};

//! Binary file header
struct QoreAOTBinaryHeader {
    uint32_t magic;           //!< QORE_AOT_BINARY_MAGIC
    uint16_t version;         //!< QORE_AOT_BINARY_VERSION
    uint16_t flags;           //!< QORE_AOT_FLAG_*
    int64_t parse_options;    //!< parse options used during compilation
    uint32_t section_count;   //!< number of sections
    uint32_t label_offset;    //!< offset into string pool for source label
    uint32_t label_length;    //!< length of source label
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
    @return true on success, false if serialization failed
*/
bool serializeNamespaceTree(QoreAOTBinaryWriter& writer, qore_ns_private* root_ns);

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
    GENERIC_EVAL       = 0xFF //!< Unsupported expression — function needs source fallback
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
struct AOTExprSlotId {
    AOTExprKind kind = AOTExprKind::GENERIC_EVAL; //!< expression kind
    std::string ref1;        //!< kind-specific: function name or class path
    std::string ref2;        //!< kind-specific: method name (for method calls)
};

//! Identity for a body local variable (needed for instantiation management)
struct AOTBodyLocalId {
    std::string name;        //!< variable name
    std::string type_path;   //!< type path
    bool is_closure = false; //!< true if closure variable
};

//! Complete slot identity set for a single compiled function
struct AOTSlotIdentities {
    std::vector<AOTLocalSlotId> locals;   //!< indexed by local slot index
    std::vector<AOTGlobalSlotId> globals; //!< indexed by global slot index
    std::vector<AOTExprSlotId> exprs;     //!< indexed by expression slot index
    std::vector<AOTBodyLocalId> body_locals; //!< body locals in order
    bool has_unsupported_exprs = false;   //!< true if any expression is GENERIC_EVAL
};

//! Descriptor for a compiled function with slot identities
struct AOTCompiledFuncWithSlots {
    std::string name;                //!< AOT function name (e.g. "myFunc", "MyClass::method")
    int num_locals = 0;              //!< number of local variable slots
    int num_globals = 0;             //!< number of global variable slots
    int num_exprs = 0;               //!< number of expression slots
    int num_stmts = 0;               //!< number of statement slots (OnBlockExit)
    AOTSlotIdentities slot_ids;      //!< extracted slot identities
};

//! Serialize slot maps for compiled functions into the SLOT_MAPS binary section
/** @param writer the binary writer to write to
    @param funcs vector of compiled function descriptors with slot identities
*/
void serializeSlotMaps(QoreAOTBinaryWriter& writer, const std::vector<AOTCompiledFuncWithSlots>& funcs);

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

public:
    ~QoreAOTBinaryDeserializer() {
        delete type_resolver;
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

#endif
