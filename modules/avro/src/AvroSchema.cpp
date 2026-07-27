/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroSchema.cpp Avro schema node graph and JSON schema parser */
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

#include "AvroSchema.h"
#include "AvroDecimal.h"

#include <qore/QoreJson.h>
#include <qore/ReferenceHolder.h>

#include <cstring>
#include <set>

static const char* avro_type_names[] = {
    "null", "boolean", "int", "long", "float", "double", "bytes", "string",
    "record", "enum", "array", "map", "union", "fixed",
};

const char* avro_type_name(AvroType t) {
    assert((size_t)t < sizeof(avro_type_names) / sizeof(*avro_type_names));
    return avro_type_names[(size_t)t];
}

const char* avro_logical_type_name(AvroLogicalType lt) {
    switch (lt) {
        case ALT_NONE: return nullptr;
        case ALT_DECIMAL: return "decimal";
        case ALT_UUID: return "uuid";
        case ALT_DATE: return "date";
        case ALT_TIME_MILLIS: return "time-millis";
        case ALT_TIME_MICROS: return "time-micros";
        case ALT_TIMESTAMP_MILLIS: return "timestamp-millis";
        case ALT_TIMESTAMP_MICROS: return "timestamp-micros";
        case ALT_LOCAL_TIMESTAMP_MILLIS: return "local-timestamp-millis";
        case ALT_LOCAL_TIMESTAMP_MICROS: return "local-timestamp-micros";
        case ALT_DURATION: return "duration";
    }
    return nullptr;
}

bool AvroNode::matchesName(const std::string& name) const {
    if (fullname == name) {
        return true;
    }
    for (const std::string& a : aliases) {
        if (a == name) {
            return true;
        }
    }
    return false;
}

//! returns the base type for a primitive type name, or -1 if \a name is not a primitive type name
static int avro_primitive_type(const char* name) {
    for (int i = AT_NULL; i <= AT_STRING; ++i) {
        if (!strcmp(name, avro_type_names[i])) {
            return i;
        }
    }
    return -1;
}

//! returns true if \a name is a valid unqualified Avro name: [A-Za-z_][A-Za-z0-9_]*
static bool avro_valid_name(const char* name) {
    if (!name || !*name) {
        return false;
    }
    if (!isalpha((unsigned char)*name) && *name != '_') {
        return false;
    }
    for (const char* p = name + 1; *p; ++p) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            return false;
        }
    }
    return true;
}

//! returns true if \a name is a valid Avro fullname: one or more dot-separated valid names
static bool avro_valid_fullname(const std::string& name) {
    size_t pos = 0;
    while (true) {
        size_t dot = name.find('.', pos);
        std::string part = name.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
        if (!avro_valid_name(part.c_str())) {
            return false;
        }
        if (dot == std::string::npos) {
            return true;
        }
        pos = dot + 1;
    }
}

//! parses an Avro schema in Qore data form into an AvroSchemaData arena
class AvroSchemaParser {
public:
    DLLLOCAL AvroSchemaParser(AvroSchemaData& data, ExceptionSink* xsink) : data(data), xsink(xsink) {
    }

    //! parses \a v as a schema; returns nullptr if an exception was raised
    DLLLOCAL const AvroNode* parse(QoreValue v, const std::string& enc_ns, unsigned depth);

private:
    AvroSchemaData& data;
    ExceptionSink* xsink;
    //! shared nodes for plain primitives (no logical type); indexed by AvroType
    AvroNode* primitive_cache[AT_STRING + 1] = {};

    DLLLOCAL AvroNode* newNode(AvroType t) {
        data.arena.push_back(std::unique_ptr<AvroNode>(new AvroNode(t)));
        return data.arena.back().get();
    }

    DLLLOCAL const AvroNode* getPrimitive(AvroType t) {
        assert(t <= AT_STRING);
        if (!primitive_cache[t]) {
            primitive_cache[t] = newNode(t);
        }
        return primitive_cache[t];
    }

    DLLLOCAL const AvroNode* parseTypeName(const char* name, const std::string& enc_ns);
    DLLLOCAL const AvroNode* parseUnion(const QoreListNode* l, const std::string& enc_ns, unsigned depth);
    DLLLOCAL const AvroNode* parseObject(const QoreHashNode* h, const std::string& enc_ns, unsigned depth);
    DLLLOCAL const AvroNode* parseRecord(const QoreHashNode* h, const std::string& enc_ns, unsigned depth);
    DLLLOCAL const AvroNode* parseEnum(const QoreHashNode* h, const std::string& enc_ns);
    DLLLOCAL const AvroNode* parseFixed(const QoreHashNode* h, const std::string& enc_ns);

    //! computes the fullname of a named type declaration and registers it; returns -1 on error
    DLLLOCAL int registerName(AvroNode* node, const QoreHashNode* h, const std::string& enc_ns,
            const char* kind);

    //! applies a "logicalType" attribute to \a node if it is valid for the node's base type
    DLLLOCAL void applyLogicalType(AvroNode* node, const QoreHashNode* h);

    //! converts an Avro JSON default value to its Qore representation; returns -1 on error
    DLLLOCAL int convertDefault(QoreValue& out, const AvroNode* node, QoreValue json,
            const char* field_name, unsigned depth);

    DLLLOCAL const char* getStringKey(const QoreHashNode* h, const char* key) {
        QoreValue v = h->getKeyValue(key);
        return v.getType() == NT_STRING ? v.get<const QoreStringNode>()->c_str() : nullptr;
    }
};

const AvroNode* AvroSchemaParser::parse(QoreValue v, const std::string& enc_ns, unsigned depth) {
    if (depth > AVRO_MAX_NESTING_DEPTH) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "schema nesting exceeds the maximum depth of %d",
            AVRO_MAX_NESTING_DEPTH);
        return nullptr;
    }
    switch (v.getType()) {
        case NT_STRING:
            return parseTypeName(v.get<const QoreStringNode>()->c_str(), enc_ns);
        case NT_LIST:
            return parseUnion(v.get<const QoreListNode>(), enc_ns, depth);
        case NT_HASH:
            return parseObject(v.get<const QoreHashNode>(), enc_ns, depth);
        default:
            break;
    }
    xsink->raiseException("AVRO-SCHEMA-ERROR", "a schema must be a JSON string, array or object; "
        "got type '%s'", v.getTypeName());
    return nullptr;
}

const AvroNode* AvroSchemaParser::parseTypeName(const char* name, const std::string& enc_ns) {
    int prim = avro_primitive_type(name);
    if (prim >= 0) {
        return getPrimitive((AvroType)prim);
    }

    // a reference to a previously-defined named type; unqualified names resolve against the
    // enclosing namespace first, then against the null namespace
    std::string sname(name);
    if (sname.find('.') == std::string::npos && !enc_ns.empty()) {
        const AvroNode* n = data.findNamedType(enc_ns + "." + sname);
        if (n) {
            return n;
        }
    }
    const AvroNode* n = data.findNamedType(sname);
    if (n) {
        return n;
    }
    xsink->raiseException("AVRO-SCHEMA-ERROR", "unknown Avro type or undefined named type '%s'", name);
    return nullptr;
}

const AvroNode* AvroSchemaParser::parseUnion(const QoreListNode* l, const std::string& enc_ns,
        unsigned depth) {
    AvroNode* node = newNode(AT_UNION);
    // a set of the branch keys already seen: the type name for unnamed types, the fullname for
    // named types; the spec allows repeated named types but no other repeats
    std::set<std::string> seen;
    ConstListIterator li(l);
    while (li.next()) {
        const AvroNode* b = parse(li.getValue(), enc_ns, depth + 1);
        if (!b) {
            return nullptr;
        }
        if (b->type == AT_UNION) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "a union may not immediately contain "
                "another union");
            return nullptr;
        }
        std::string key = b->fullname.empty() ? std::string(avro_type_name(b->type)) : b->fullname;
        if (!seen.insert(key).second) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "union contains more than one branch of "
                "type '%s'", key.c_str());
            return nullptr;
        }
        node->branches.push_back(b);
    }
    if (node->branches.empty()) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "a union must have at least one branch");
        return nullptr;
    }
    return node;
}

const AvroNode* AvroSchemaParser::parseObject(const QoreHashNode* h, const std::string& enc_ns,
        unsigned depth) {
    QoreValue tv = h->getKeyValue("type");
    if (tv.isNothing()) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "schema object is missing the required "
            "'type' attribute");
        return nullptr;
    }
    // {"type": [...]} and {"type": {...}} are both legal: the attribute holds a nested schema
    if (tv.getType() != NT_STRING) {
        return parse(tv, enc_ns, depth + 1);
    }

    const char* tname = tv.get<const QoreStringNode>()->c_str();
    if (!strcmp(tname, "record") || !strcmp(tname, "error")) {
        return parseRecord(h, enc_ns, depth);
    }
    if (!strcmp(tname, "enum")) {
        return parseEnum(h, enc_ns);
    }
    if (!strcmp(tname, "fixed")) {
        return parseFixed(h, enc_ns);
    }
    if (!strcmp(tname, "array")) {
        QoreValue iv = h->getKeyValue("items");
        if (iv.isNothing()) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "array schema is missing the required "
                "'items' attribute");
            return nullptr;
        }
        AvroNode* node = newNode(AT_ARRAY);
        if (!(node->items = parse(iv, enc_ns, depth + 1))) {
            return nullptr;
        }
        return node;
    }
    if (!strcmp(tname, "map")) {
        QoreValue vv = h->getKeyValue("values");
        if (vv.isNothing()) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "map schema is missing the required "
                "'values' attribute");
            return nullptr;
        }
        AvroNode* node = newNode(AT_MAP);
        if (!(node->items = parse(vv, enc_ns, depth + 1))) {
            return nullptr;
        }
        return node;
    }

    int prim = avro_primitive_type(tname);
    if (prim >= 0) {
        // a primitive in object form; it may carry a logical type
        AvroNode* node = newNode((AvroType)prim);
        applyLogicalType(node, h);
        if (node->logical == ALT_NONE) {
            // no distinguishing attributes: share the cached primitive node instead
            data.arena.pop_back();
            return getPrimitive((AvroType)prim);
        }
        return node;
    }

    return parseTypeName(tname, enc_ns);
}

int AvroSchemaParser::registerName(AvroNode* node, const QoreHashNode* h, const std::string& enc_ns,
        const char* kind) {
    const char* name = getStringKey(h, "name");
    if (!name) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "%s schema is missing the required 'name' "
            "attribute", kind);
        return -1;
    }
    std::string fullname(name);
    if (fullname.find('.') == std::string::npos) {
        const char* ns = getStringKey(h, "namespace");
        std::string use_ns = ns ? std::string(ns) : enc_ns;
        if (!avro_valid_name(name)) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "'%s' is not a valid Avro name for a %s "
                "type", name, kind);
            return -1;
        }
        if (!use_ns.empty()) {
            if (!avro_valid_fullname(use_ns)) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "'%s' is not a valid Avro namespace",
                    use_ns.c_str());
                return -1;
            }
            fullname = use_ns + "." + fullname;
        }
    } else if (!avro_valid_fullname(fullname)) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "'%s' is not a valid Avro fullname for a %s "
            "type", name, kind);
        return -1;
    }

    if (data.registry.find(fullname) != data.registry.end()) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "named type '%s' is defined more than once in "
            "the schema", fullname.c_str());
        return -1;
    }

    node->fullname = fullname;
    std::string node_ns = node->getNamespace();

    QoreValue av = h->getKeyValue("aliases");
    if (!av.isNothing()) {
        if (av.getType() != NT_LIST) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'aliases' attribute of named type "
                "'%s' must be an array; got type '%s'", fullname.c_str(), av.getTypeName());
            return -1;
        }
        ConstListIterator li(av.get<const QoreListNode>());
        while (li.next()) {
            if (li.getValue().getType() != NT_STRING) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'aliases' attribute of named type "
                    "'%s' must contain only strings; got type '%s'", fullname.c_str(),
                    li.getValue().getTypeName());
                return -1;
            }
            std::string alias(li.getValue().get<const QoreStringNode>()->c_str());
            if (alias.find('.') == std::string::npos && !node_ns.empty()) {
                alias = node_ns + "." + alias;
            }
            if (!avro_valid_fullname(alias)) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "'%s' is not a valid Avro alias for "
                    "named type '%s'", alias.c_str(), fullname.c_str());
                return -1;
            }
            node->aliases.push_back(alias);
        }
    }

    node->source_value = const_cast<QoreHashNode*>(h)->refSelf();
    data.registry[fullname] = node;
    data.named_order.push_back(fullname);
    return 0;
}

const AvroNode* AvroSchemaParser::parseRecord(const QoreHashNode* h, const std::string& enc_ns,
        unsigned depth) {
    AvroNode* node = newNode(AT_RECORD);
    if (registerName(node, h, enc_ns, "record")) {
        return nullptr;
    }
    // fields are parsed with the record's own namespace in scope, and after registration, so
    // that a record may refer to itself
    std::string rec_ns = node->getNamespace();

    QoreValue fv = h->getKeyValue("fields");
    if (fv.getType() != NT_LIST) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "record '%s' is missing the required 'fields' "
            "array", node->fullname.c_str());
        return nullptr;
    }

    ConstListIterator li(fv.get<const QoreListNode>());
    while (li.next()) {
        if (!(li.index() % AVRO_INTERRUPT_CHECK_INTERVAL)
            && qore_check_cancel(xsink, "parsing an Avro schema")) {
            return nullptr;
        }
        if (li.getValue().getType() != NT_HASH) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "field %d of record '%s' must be an "
                "object; got type '%s'", (int)li.index(), node->fullname.c_str(),
                li.getValue().getTypeName());
            return nullptr;
        }
        const QoreHashNode* fh = li.getValue().get<const QoreHashNode>();
        const char* fname = getStringKey(fh, "name");
        if (!fname || !avro_valid_name(fname)) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "field %d of record '%s' has a missing or "
                "invalid 'name' attribute", (int)li.index(), node->fullname.c_str());
            return nullptr;
        }
        QoreValue ftv = fh->getKeyValue("type");
        if (ftv.isNothing()) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "field '%s' of record '%s' is missing the "
                "required 'type' attribute", fname, node->fullname.c_str());
            return nullptr;
        }

        node->fields.emplace_back();
        AvroField& field = node->fields.back();
        field.name = fname;
        if (!(field.type = parse(ftv, rec_ns, depth + 1))) {
            return nullptr;
        }

        QoreValue fav = fh->getKeyValue("aliases");
        if (!fav.isNothing()) {
            if (fav.getType() != NT_LIST) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'aliases' attribute of field '%s' "
                    "of record '%s' must be an array; got type '%s'", fname,
                    node->fullname.c_str(), fav.getTypeName());
                return nullptr;
            }
            ConstListIterator ai(fav.get<const QoreListNode>());
            while (ai.next()) {
                if (ai.getValue().getType() != NT_STRING) {
                    xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'aliases' attribute of field "
                        "'%s' of record '%s' must contain only strings; got type '%s'", fname,
                        node->fullname.c_str(), ai.getValue().getTypeName());
                    return nullptr;
                }
                field.aliases.push_back(ai.getValue().get<const QoreStringNode>()->c_str());
            }
        }

        if (fh->existsKey("default")) {
            if (convertDefault(field.default_value, field.type, fh->getKeyValue("default"), fname,
                    depth + 1)) {
                return nullptr;
            }
            field.has_default = true;
        }

        size_t idx = node->fields.size() - 1;
        if (!node->field_index.insert(std::make_pair(field.name, idx)).second) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "record '%s' declares field '%s' more than "
                "once", node->fullname.c_str(), fname);
            return nullptr;
        }
        // aliases resolve to the same field, but never shadow a real field name
        for (const std::string& a : field.aliases) {
            node->field_index.insert(std::make_pair(a, idx));
        }
    }

    return node;
}

const AvroNode* AvroSchemaParser::parseEnum(const QoreHashNode* h, const std::string& enc_ns) {
    AvroNode* node = newNode(AT_ENUM);
    if (registerName(node, h, enc_ns, "enum")) {
        return nullptr;
    }

    QoreValue sv = h->getKeyValue("symbols");
    if (sv.getType() != NT_LIST) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "enum '%s' is missing the required 'symbols' "
            "array", node->fullname.c_str());
        return nullptr;
    }
    ConstListIterator li(sv.get<const QoreListNode>());
    while (li.next()) {
        if (li.getValue().getType() != NT_STRING) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "symbol %d of enum '%s' must be a string; "
                "got type '%s'", (int)li.index(), node->fullname.c_str(),
                li.getValue().getTypeName());
            return nullptr;
        }
        const char* sym = li.getValue().get<const QoreStringNode>()->c_str();
        if (!avro_valid_name(sym)) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "'%s' is not a valid symbol name in enum "
                "'%s'", sym, node->fullname.c_str());
            return nullptr;
        }
        if (!node->symbol_index.insert(std::make_pair(std::string(sym),
                (int)node->symbols.size())).second) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "enum '%s' declares symbol '%s' more than "
                "once", node->fullname.c_str(), sym);
            return nullptr;
        }
        node->symbols.push_back(sym);
    }
    if (node->symbols.empty()) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "enum '%s' must declare at least one symbol",
            node->fullname.c_str());
        return nullptr;
    }

    if (h->existsKey("default")) {
        QoreValue dv = h->getKeyValue("default");
        if (dv.getType() != NT_STRING) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'default' attribute of enum '%s' must "
                "be a string; got type '%s'", node->fullname.c_str(), dv.getTypeName());
            return nullptr;
        }
        node->enum_default = node->findSymbol(dv.get<const QoreStringNode>()->c_str());
        if (node->enum_default < 0) {
            xsink->raiseException("AVRO-SCHEMA-ERROR", "the 'default' attribute of enum '%s' is "
                "'%s', which is not one of its symbols", node->fullname.c_str(),
                dv.get<const QoreStringNode>()->c_str());
            return nullptr;
        }
    }

    return node;
}

const AvroNode* AvroSchemaParser::parseFixed(const QoreHashNode* h, const std::string& enc_ns) {
    AvroNode* node = newNode(AT_FIXED);
    if (registerName(node, h, enc_ns, "fixed")) {
        return nullptr;
    }
    QoreValue szv = h->getKeyValue("size");
    if (szv.getType() != NT_INT) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "fixed type '%s' is missing the required "
            "integer 'size' attribute", node->fullname.c_str());
        return nullptr;
    }
    int64 sz = szv.getAsBigInt();
    if (sz < 0 || sz > 0x7fffffff) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "fixed type '%s' declares an invalid size "
            QLLD, node->fullname.c_str(), sz);
        return nullptr;
    }
    node->fixed_size = (unsigned)sz;
    applyLogicalType(node, h);
    return node;
}

void AvroSchemaParser::applyLogicalType(AvroNode* node, const QoreHashNode* h) {
    QoreValue lv = h->getKeyValue("logicalType");
    if (lv.getType() != NT_STRING) {
        return;
    }
    const char* lt = lv.get<const QoreStringNode>()->c_str();

    // The specification requires implementations to ignore a logical type they do not recognise,
    // or one that is invalid for its base type, and fall back to the base type.  Every early
    // return below is that fallback.
    if (!strcmp(lt, "decimal")) {
        if (node->type != AT_BYTES && node->type != AT_FIXED) {
            return;
        }
        QoreValue pv = h->getKeyValue("precision");
        if (pv.getType() != NT_INT) {
            return;
        }
        int64 precision = pv.getAsBigInt();
        int64 scale = 0;
        QoreValue sv = h->getKeyValue("scale");
        if (!sv.isNothing()) {
            if (sv.getType() != NT_INT) {
                return;
            }
            scale = sv.getAsBigInt();
        }
        if (precision <= 0 || scale < 0 || scale > precision || precision > AVRO_MAX_PRECISION) {
            return;
        }
        if (node->type == AT_FIXED
            && precision > avro_decimal_max_precision(node->fixed_size)) {
            return;
        }
        node->precision = (int)precision;
        node->scale = (int)scale;
        node->logical = ALT_DECIMAL;
        return;
    }
    if (!strcmp(lt, "uuid")) {
        // Avro 1.12 allows uuid over fixed[16] as well as over string
        if (node->type == AT_STRING || (node->type == AT_FIXED && node->fixed_size == 16)) {
            node->logical = ALT_UUID;
        }
        return;
    }
    if (!strcmp(lt, "date")) {
        if (node->type == AT_INT) {
            node->logical = ALT_DATE;
        }
        return;
    }
    if (!strcmp(lt, "time-millis")) {
        if (node->type == AT_INT) {
            node->logical = ALT_TIME_MILLIS;
        }
        return;
    }
    if (!strcmp(lt, "time-micros")) {
        if (node->type == AT_LONG) {
            node->logical = ALT_TIME_MICROS;
        }
        return;
    }
    if (!strcmp(lt, "timestamp-millis")) {
        if (node->type == AT_LONG) {
            node->logical = ALT_TIMESTAMP_MILLIS;
        }
        return;
    }
    if (!strcmp(lt, "timestamp-micros")) {
        if (node->type == AT_LONG) {
            node->logical = ALT_TIMESTAMP_MICROS;
        }
        return;
    }
    if (!strcmp(lt, "local-timestamp-millis")) {
        if (node->type == AT_LONG) {
            node->logical = ALT_LOCAL_TIMESTAMP_MILLIS;
        }
        return;
    }
    if (!strcmp(lt, "local-timestamp-micros")) {
        if (node->type == AT_LONG) {
            node->logical = ALT_LOCAL_TIMESTAMP_MICROS;
        }
        return;
    }
    if (!strcmp(lt, "duration")) {
        if (node->type == AT_FIXED && node->fixed_size == 12) {
            node->logical = ALT_DURATION;
        }
        return;
    }
    // unknown logical type: ignored, per the specification
}

int AvroSchemaParser::convertDefault(QoreValue& out, const AvroNode* node, QoreValue json,
        const char* field_name, unsigned depth) {
    if (depth > AVRO_MAX_NESTING_DEPTH) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' exceeds the "
            "maximum nesting depth of %d", field_name, AVRO_MAX_NESTING_DEPTH);
        return -1;
    }

    switch (node->type) {
        case AT_NULL:
            if (!json.isNullOrNothing()) {
                break;
            }
            out = QoreValue();
            return 0;

        case AT_BOOLEAN:
            if (json.getType() != NT_BOOLEAN) {
                break;
            }
            out = json.getAsBool();
            return 0;

        case AT_INT:
        case AT_LONG:
            if (json.getType() != NT_INT) {
                break;
            }
            out = json.getAsBigInt();
            return 0;

        case AT_FLOAT:
        case AT_DOUBLE:
            if (json.getType() != NT_INT && json.getType() != NT_FLOAT) {
                break;
            }
            out = json.getAsFloat();
            return 0;

        case AT_BYTES:
        case AT_FIXED: {
            // the JSON encoding of bytes and fixed defaults is a string in which each character's
            // code point (0-255) is one byte
            if (json.getType() != NT_STRING) {
                break;
            }
            const QoreStringNode* str = json.get<const QoreStringNode>();
            TempEncodingHelper utf8(str, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            SimpleRefHolder<BinaryNode> b(new BinaryNode);
            const char* p = utf8->c_str();
            const char* end = p + utf8->size();
            while (p < end) {
                unsigned clen = 0;
                int cp = QCS_UTF8->getUnicode(p, end, clen, xsink);
                if (*xsink) {
                    return -1;
                }
                if (cp > 0xff) {
                    xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' "
                        "contains code point U+%04X, which is out of the 0-255 range required for "
                        "a %s default", field_name, (unsigned)cp, avro_type_name(node->type));
                    return -1;
                }
                unsigned char byte = (unsigned char)cp;
                b->append(&byte, 1);
                p += clen;
            }
            if (node->type == AT_FIXED && b->size() != node->fixed_size) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' is %d "
                    "bytes long, but fixed type '%s' requires exactly %u", field_name,
                    (int)b->size(), node->fullname.c_str(), node->fixed_size);
                return -1;
            }
            out = b.release();
            return 0;
        }

        case AT_STRING:
        case AT_ENUM: {
            if (json.getType() != NT_STRING) {
                break;
            }
            const QoreStringNode* str = json.get<const QoreStringNode>();
            if (node->type == AT_ENUM && node->findSymbol(str->c_str()) < 0) {
                xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' is "
                    "'%s', which is not a symbol of enum '%s'", field_name, str->c_str(),
                    node->fullname.c_str());
                return -1;
            }
            out = str->stringRefSelf();
            return 0;
        }

        case AT_ARRAY: {
            if (json.getType() != NT_LIST) {
                break;
            }
            ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
            ConstListIterator li(json.get<const QoreListNode>());
            while (li.next()) {
                QoreValue elem;
                if (convertDefault(elem, node->items, li.getValue(), field_name, depth + 1)) {
                    return -1;
                }
                l->push(elem, xsink);
                if (*xsink) {
                    return -1;
                }
            }
            out = l.release();
            return 0;
        }

        case AT_MAP: {
            if (json.getType() != NT_HASH) {
                break;
            }
            ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
            ConstHashIterator hi(json.get<const QoreHashNode>());
            while (hi.next()) {
                QoreValue elem;
                if (convertDefault(elem, node->items, hi.get(), field_name, depth + 1)) {
                    return -1;
                }
                rv->setKeyValue(hi.getKey(), elem, xsink);
                if (*xsink) {
                    return -1;
                }
            }
            out = rv.release();
            return 0;
        }

        case AT_RECORD: {
            if (json.getType() != NT_HASH) {
                break;
            }
            const QoreHashNode* src = json.get<const QoreHashNode>();
            ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
            for (const AvroField& f : node->fields) {
                QoreValue elem;
                if (src->existsKey(f.name.c_str())) {
                    if (convertDefault(elem, f.type, src->getKeyValue(f.name.c_str()), field_name,
                            depth + 1)) {
                        return -1;
                    }
                } else if (f.has_default) {
                    elem = f.default_value.refSelf();
                } else {
                    xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' "
                        "does not supply a value for field '%s' of record '%s', which has no "
                        "default of its own", field_name, f.name.c_str(), node->fullname.c_str());
                    return -1;
                }
                rv->setKeyValue(f.name.c_str(), elem, xsink);
                if (*xsink) {
                    return -1;
                }
            }
            out = rv.release();
            return 0;
        }

        case AT_UNION:
            // per the specification, the default value of a union corresponds to its first branch
            assert(!node->branches.empty());
            return convertDefault(out, node->branches[0], json, field_name, depth + 1);
    }

    xsink->raiseException("AVRO-SCHEMA-ERROR", "the default value of field '%s' has JSON type "
        "'%s', which cannot be a default for Avro type '%s'", field_name, json.getTypeName(),
        avro_type_name(node->type));
    return -1;
}

//! computes AvroNode::zero_width for \a node and everything it reaches
/** \a in_progress holds the nodes on the current path: a type that can only be encoded by
    encoding itself needs infinite bytes, so treating a cycle as non-zero-width is correct.
*/
static bool compute_zero_width(const AvroNode* node, std::set<const AvroNode*>& in_progress) {
    AvroNode* n = const_cast<AvroNode*>(node);
    if (n->computed_zero_width) {
        return n->zero_width;
    }
    if (!in_progress.insert(n).second) {
        return false;
    }

    bool zw = false;
    switch (n->type) {
        case AT_NULL:
            zw = true;
            break;
        case AT_FIXED:
            zw = !n->fixed_size;
            break;
        case AT_RECORD: {
            zw = true;
            for (const AvroField& f : n->fields) {
                if (!compute_zero_width(f.type, in_progress)) {
                    zw = false;
                    break;
                }
            }
            break;
        }
        case AT_ARRAY:
        case AT_MAP:
            // an empty array or map still costs the terminating zero block count
            compute_zero_width(n->items, in_progress);
            break;
        case AT_UNION:
            // the branch index always costs at least one byte
            for (const AvroNode* b : n->branches) {
                compute_zero_width(b, in_progress);
            }
            break;
        default:
            break;
    }

    in_progress.erase(n);
    n->zero_width = zw;
    n->computed_zero_width = true;
    return zw;
}

AvroSchemaData* AvroSchemaData::parseValue(QoreValue schema, ExceptionSink* xsink) {
    ReferenceHolder<AvroSchemaData> data(new AvroSchemaData, xsink);
    AvroSchemaParser parser(**data, xsink);
    const AvroNode* root = parser.parse(schema, std::string(), 0);
    if (!root) {
        assert(*xsink);
        return nullptr;
    }
    {
        std::set<const AvroNode*> in_progress;
        for (const std::unique_ptr<AvroNode>& n : data->arena) {
            compute_zero_width(n.get(), in_progress);
        }
    }
    data->root = root;
    data->schema_value = schema.refSelf();
    return data.release();
}

AvroSchemaData* AvroSchemaData::parseJson(const QoreString& schema_json, ExceptionSink* xsink) {
    const QoreJsonApi* json = avro_get_json_api(xsink);
    if (!json) {
        return nullptr;
    }
    ValueHolder v(json->parse(schema_json, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return parseValue(*v, xsink);
}

QoreValue QoreAvroSchema::getHash() const {
    if (root == data->getRoot()) {
        return data->getSchemaValue();
    }
    // a handle rooted at an interior node is always a named type, and every named type keeps a
    // reference to the JSON object that declared it
    assert(!root->source_value.isNothing());
    return root->source_value.refSelf();
}

QoreStringNode* QoreAvroSchema::getJson(ExceptionSink* xsink) const {
    const QoreJsonApi* json = avro_get_json_api(xsink);
    if (!json) {
        return nullptr;
    }
    ValueHolder v(getHash(), xsink);
    return json->generate(*v, JGF_NONE, QCS_UTF8, xsink);
}

QoreStringNode* QoreAvroSchema::getCanonicalForm() const {
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(QCS_UTF8));
    avro_canonical_form(**str, root);
    return str.release();
}

int64 QoreAvroSchema::getFingerprint() const {
    SimpleRefHolder<QoreStringNode> cf(getCanonicalForm());
    return avro_fingerprint64(cf->c_str(), cf->size());
}

//! appends \a s to \a str as a JSON string literal
static void canonical_string(QoreString& str, const std::string& s) {
    str.concat('"');
    for (char c : s) {
        switch (c) {
            case '"': str.concat("\\\""); break;
            case '\\': str.concat("\\\\"); break;
            case '\b': str.concat("\\b"); break;
            case '\f': str.concat("\\f"); break;
            case '\n': str.concat("\\n"); break;
            case '\r': str.concat("\\r"); break;
            case '\t': str.concat("\\t"); break;
            default:
                if ((unsigned char)c < 0x20) {
                    str.sprintf("\\u%04x", (unsigned char)c);
                } else {
                    str.concat(c);
                }
        }
    }
    str.concat('"');
}

//! writes the Parsing Canonical Form of \a node, expanding each named type at its first occurrence
static void canonical_form_intern(QoreString& str, const AvroNode* node,
        std::set<const AvroNode*>& seen) {
    switch (node->type) {
        case AT_NULL:
        case AT_BOOLEAN:
        case AT_INT:
        case AT_LONG:
        case AT_FLOAT:
        case AT_DOUBLE:
        case AT_BYTES:
        case AT_STRING:
            // [PRIMITIVES] and [STRIP]: logical types are not part of the canonical form
            str.sprintf("\"%s\"", avro_type_name(node->type));
            return;

        case AT_UNION: {
            str.concat('[');
            bool first = true;
            for (const AvroNode* b : node->branches) {
                if (!first) {
                    str.concat(',');
                }
                first = false;
                canonical_form_intern(str, b, seen);
            }
            str.concat(']');
            return;
        }

        case AT_ARRAY:
            str.concat("{\"type\":\"array\",\"items\":");
            canonical_form_intern(str, node->items, seen);
            str.concat('}');
            return;

        case AT_MAP:
            str.concat("{\"type\":\"map\",\"values\":");
            canonical_form_intern(str, node->items, seen);
            str.concat('}');
            return;

        case AT_RECORD:
        case AT_ENUM:
        case AT_FIXED:
            break;
    }

    // a named type: expanded once, then referred to by fullname
    if (!seen.insert(node).second) {
        canonical_string(str, node->fullname);
        return;
    }

    // [ORDER]: name, type, fields, symbols, items, values, size
    str.concat("{\"name\":");
    canonical_string(str, node->fullname);
    str.sprintf(",\"type\":\"%s\"", avro_type_name(node->type));

    switch (node->type) {
        case AT_RECORD: {
            str.concat(",\"fields\":[");
            bool first = true;
            for (const AvroField& f : node->fields) {
                if (!first) {
                    str.concat(',');
                }
                first = false;
                str.concat("{\"name\":");
                canonical_string(str, f.name);
                str.concat(",\"type\":");
                canonical_form_intern(str, f.type, seen);
                str.concat('}');
            }
            str.concat(']');
            break;
        }

        case AT_ENUM: {
            str.concat(",\"symbols\":[");
            bool first = true;
            for (const std::string& s : node->symbols) {
                if (!first) {
                    str.concat(',');
                }
                first = false;
                canonical_string(str, s);
            }
            str.concat(']');
            break;
        }

        case AT_FIXED:
            str.sprintf(",\"size\":%u", node->fixed_size);
            break;

        default:
            assert(false);
            break;
    }
    str.concat('}');
}

void avro_canonical_form(QoreString& str, const AvroNode* node) {
    std::set<const AvroNode*> seen;
    canonical_form_intern(str, node, seen);
}

//! CRC-64-AVRO: the 64-bit Rabin fingerprint defined by the Avro specification
class AvroFingerprintTable {
public:
    static constexpr uint64_t empty = 0xc15d213aa4d7a795ULL;

    DLLLOCAL AvroFingerprintTable() {
        for (int i = 0; i < 256; ++i) {
            uint64_t fp = (uint64_t)i;
            for (int j = 0; j < 8; ++j) {
                fp = (fp >> 1) ^ (empty & ~((fp & 1) - 1));
            }
            table[i] = fp;
        }
    }

    DLLLOCAL uint64_t operator[](size_t i) const {
        return table[i];
    }

private:
    uint64_t table[256];
};

int64 avro_fingerprint64(const char* buf, size_t len) {
    // C++11 guarantees this is initialized exactly once, even with concurrent callers
    static const AvroFingerprintTable table;

    uint64_t fp = AvroFingerprintTable::empty;
    for (size_t i = 0; i < len; ++i) {
        fp = (fp >> 8) ^ table[(fp ^ (unsigned char)buf[i]) & 0xff];
    }
    return (int64)fp;
}
