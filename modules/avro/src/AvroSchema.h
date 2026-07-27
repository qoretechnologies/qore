/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroSchema.h Avro schema node graph and JSON schema parser */
/*
    Qore avro module

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

#ifndef _QORE_AVRO_AVROSCHEMA_H
#define _QORE_AVRO_AVROSCHEMA_H

#include "avro-module.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

//! Avro base types, in the order used by the type-name table
enum AvroType : unsigned char {
    AT_NULL = 0,
    AT_BOOLEAN,
    AT_INT,
    AT_LONG,
    AT_FLOAT,
    AT_DOUBLE,
    AT_BYTES,
    AT_STRING,
    AT_RECORD,
    AT_ENUM,
    AT_ARRAY,
    AT_MAP,
    AT_UNION,
    AT_FIXED,
};

//! Avro logical types layered over the base types
enum AvroLogicalType : unsigned char {
    ALT_NONE = 0,
    ALT_DECIMAL,
    ALT_UUID,
    ALT_DATE,
    ALT_TIME_MILLIS,
    ALT_TIME_MICROS,
    ALT_TIMESTAMP_MILLIS,
    ALT_TIMESTAMP_MICROS,
    ALT_LOCAL_TIMESTAMP_MILLIS,
    ALT_LOCAL_TIMESTAMP_MICROS,
    ALT_DURATION,
};

//! returns the Avro type name for a base type (ex: "record")
DLLLOCAL const char* avro_type_name(AvroType t);

//! returns the Avro logical type name, or nullptr for ALT_NONE
DLLLOCAL const char* avro_logical_type_name(AvroLogicalType lt);

class AvroNode;

//! a field of an Avro record
class AvroField {
public:
    std::string name;
    std::vector<std::string> aliases;
    const AvroNode* type = nullptr;

    //! the field's default value, already converted to its Qore representation
    QoreValue default_value;
    bool has_default = false;

    DLLLOCAL AvroField() {
    }

    DLLLOCAL AvroField(AvroField&& other) noexcept : name(std::move(other.name)),
            aliases(std::move(other.aliases)), type(other.type), default_value(other.default_value),
            has_default(other.has_default) {
        other.default_value = QoreValue();
        other.has_default = false;
    }

    DLLLOCAL ~AvroField() {
        // schema defaults are never objects or closures, so no destructor can run and no
        // exception can be raised here
        default_value.discard(nullptr);
    }

    AvroField(const AvroField&) = delete;
    AvroField& operator=(const AvroField&) = delete;
    AvroField& operator=(AvroField&&) = delete;
};

//! a node in the resolved schema graph
/** Nodes are owned by an AvroSchemaData arena and refer to each other with raw pointers, so
    recursive named types cost nothing and nothing is owned twice.
*/
class AvroNode {
public:
    AvroType type;
    AvroLogicalType logical = ALT_NONE;

    // named types: record, enum, fixed
    std::string fullname;
    std::vector<std::string> aliases;

    // record
    std::vector<AvroField> fields;
    //! field name (and alias) -> index into fields
    std::map<std::string, size_t> field_index;

    // enum
    std::vector<std::string> symbols;
    std::map<std::string, int> symbol_index;
    //! index into symbols of the enum's "default" symbol, or -1 if none was given
    int enum_default = -1;

    //! array item type / map value type
    const AvroNode* items = nullptr;

    //! union branches
    std::vector<const AvroNode*> branches;

    //! fixed size in bytes
    unsigned fixed_size = 0;

    // decimal logical type
    int precision = 0;
    int scale = 0;

    //! for named types: the schema JSON object that declared this type, as Qore data
    /** Lets a handle rooted at an interior named type render an exact JSON form of just that
        type; see QoreAvroSchema::getHash().
    */
    QoreValue source_value;

    //! true if a value of this type can encode to zero bytes; computed once after parsing
    /** Only `null`, `fixed` of size 0, and records all of whose fields are zero-width qualify.
        The decoder uses this to bound array and map block counts against the remaining input:
        for any other element type a block of N elements needs at least N bytes, so a count
        larger than the remaining input is provably corrupt.
    */
    bool zero_width = false;
    //! true once zero_width has been computed
    bool computed_zero_width = false;

    DLLLOCAL AvroNode(AvroType t) : type(t) {
    }

    DLLLOCAL ~AvroNode() {
        // schema JSON values are never objects or closures, so no destructor can run and no
        // exception can be raised here
        source_value.discard(nullptr);
    }

    AvroNode(const AvroNode&) = delete;
    AvroNode& operator=(const AvroNode&) = delete;

    //! returns the simple (unqualified) name of a named type
    DLLLOCAL std::string getSimpleName() const {
        size_t i = fullname.rfind('.');
        return i == std::string::npos ? fullname : fullname.substr(i + 1);
    }

    //! returns the namespace of a named type, or an empty string if it is in the null namespace
    DLLLOCAL std::string getNamespace() const {
        size_t i = fullname.rfind('.');
        return i == std::string::npos ? std::string() : fullname.substr(0, i);
    }

    //! returns the index of the given field name, checking aliases as well, or -1 if not present
    DLLLOCAL int findField(const std::string& name) const {
        std::map<std::string, size_t>::const_iterator i = field_index.find(name);
        return i == field_index.end() ? -1 : (int)i->second;
    }

    //! returns the index of the given enum symbol, or -1 if the symbol is not in this enum
    DLLLOCAL int findSymbol(const std::string& sym) const {
        std::map<std::string, int>::const_iterator i = symbol_index.find(sym);
        return i == symbol_index.end() ? -1 : i->second;
    }

    //! returns true if this node's fullname or any of its aliases matches \a name
    DLLLOCAL bool matchesName(const std::string& name) const;
};

//! refcounted owner of a parsed schema: the node arena, the named-type registry and the source text
class AvroSchemaData : public AbstractPrivateData {
public:
    DLLLOCAL AvroSchemaData() {
    }

    //! parses \a schema_json and returns the parsed data, or nullptr if an exception was raised
    DLLLOCAL static AvroSchemaData* parseJson(const QoreString& schema_json, ExceptionSink* xsink);

    //! parses an Avro schema already represented as Qore data
    DLLLOCAL static AvroSchemaData* parseValue(QoreValue schema, ExceptionSink* xsink);

    DLLLOCAL const AvroNode* getRoot() const {
        return root;
    }

    //! returns the named type with the given fullname, or nullptr if there is no such type
    DLLLOCAL const AvroNode* findNamedType(const std::string& fullname) const {
        std::map<std::string, AvroNode*>::const_iterator i = registry.find(fullname);
        return i == registry.end() ? nullptr : i->second;
    }

    //! returns the fullnames of all named types in the schema, in definition order
    DLLLOCAL const std::vector<std::string>& getNamedTypeNames() const {
        return named_order;
    }

    //! returns the schema as Qore data; the caller owns the reference returned
    DLLLOCAL QoreValue getSchemaValue() const {
        return schema_value.refSelf();
    }

protected:
    DLLLOCAL virtual ~AvroSchemaData() {
        schema_value.discard(nullptr);
    }

private:
    std::vector<std::unique_ptr<AvroNode>> arena;
    std::map<std::string, AvroNode*> registry;
    std::vector<std::string> named_order;
    const AvroNode* root = nullptr;

    //! the schema as Qore data, as parsed; the source of getSchemaValue() and getSchemaJson()
    QoreValue schema_value;

    friend class AvroSchemaParser;
};

//! private data for the Qore AvroSchema class: a reference to shared schema data plus a root node
/** Rooting a handle at an interior node is what makes AvroSchema::getNamedType() safe: the
    returned handle shares the arena and holds it alive.
*/
class QoreAvroSchema : public AbstractPrivateData {
public:
    DLLLOCAL QoreAvroSchema(AvroSchemaData* d, const AvroNode* r) : data(d), root(r) {
        assert(d);
        assert(r);
    }

    DLLLOCAL const AvroNode* getRoot() const {
        return root;
    }

    DLLLOCAL AvroSchemaData* getData() const {
        return data;
    }

    //! returns the schema as JSON text; the caller owns the reference returned
    DLLLOCAL QoreStringNode* getJson(ExceptionSink* xsink) const;

    //! returns the schema as Qore data; the caller owns the reference returned
    DLLLOCAL QoreValue getHash() const;

    //! returns the Avro Parsing Canonical Form of the schema rooted at this handle
    DLLLOCAL QoreStringNode* getCanonicalForm() const;

    //! returns the CRC-64-AVRO fingerprint of the Parsing Canonical Form
    DLLLOCAL int64 getFingerprint() const;

protected:
    DLLLOCAL virtual ~QoreAvroSchema() {
        data->deref();
    }

private:
    //! the arena owner; this object holds a reference for its lifetime
    AvroSchemaData* data;
    //! the node in data's arena that this handle is rooted at
    const AvroNode* root;
};

//! writes the Avro Parsing Canonical Form of \a node to \a str
DLLLOCAL void avro_canonical_form(QoreString& str, const AvroNode* node);

//! returns the CRC-64-AVRO (64-bit Rabin) fingerprint of \a buf
DLLLOCAL int64 avro_fingerprint64(const char* buf, size_t len);

#endif // _QORE_AVRO_AVROSCHEMA_H
