/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file JsonSchemaImpl.cpp JSON Schema validation implementation */
/*
    Qore Programming Language - JSON Module

    Copyright (C) 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "QC_JsonSchema.h"

#include <sstream>
#include <cassert>
#include <set>
#include <functional>
#include <vector>

const TypedHashDecl* hashdeclJsonSchemaValidationError = nullptr;
const TypedHashDecl* hashdeclJsonSchemaValidationResult = nullptr;

namespace {
// the exception sink of the schema operation in progress in this thread
thread_local ExceptionSink* schema_recursion_xsink = nullptr;
// the number of recursion checks in this thread
thread_local unsigned schema_recursion_checks = 0;

//! Ends a schema operation after a stack or cancellation check raised a Qore exception
class JsonSchemaAbort : public std::exception {
public:
    const char* what() const noexcept override {
        return "the schema operation was ended by a Qore exception";
    }
};

//! Sets the exception sink for the stack and cancellation checks of a schema operation
class SchemaRecursionHelper {
public:
    DLLLOCAL explicit SchemaRecursionHelper(ExceptionSink* xsink) : old_xsink(schema_recursion_xsink) {
        schema_recursion_xsink = xsink;
    }

    DLLLOCAL ~SchemaRecursionHelper() {
        schema_recursion_xsink = old_xsink;
    }

    SchemaRecursionHelper(const SchemaRecursionHelper&) = delete;
    SchemaRecursionHelper& operator=(const SchemaRecursionHelper&) = delete;

private:
    ExceptionSink* old_xsink;
};
}

void qore_json_schema_check_recursion() {
    if (!schema_recursion_xsink) {
        return;
    }
    // a validation can follow a number of paths through a schema's references that is exponential in the size of
    // the schema, so it has a cancellation point
    if (q_check_stack(schema_recursion_xsink)
        || (!(++schema_recursion_checks % 100)
            && qore_check_cancel(schema_recursion_xsink, "processing a JSON Schema"))) {
        throw JsonSchemaAbort();
    }
}

JsonSchemaValidator::JsonSchemaValidator(const QoreStringNode* schema_json, ExceptionSink* xsink)
    : valid(false) {
    if (!schema_json) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Schema string cannot be null");
        return;
    }
    initFromJsonString(schema_json->c_str(), xsink);
}

JsonSchemaValidator::JsonSchemaValidator(const QoreHashNode* schema_hash, ExceptionSink* xsink)
    : valid(false) {
    if (!schema_hash) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Schema hash cannot be null");
        return;
    }

    // Convert Qore hash to JSON
    SchemaRecursionHelper srh(xsink);
    try {
        jsoncons::json json_schema = qoreToJson(schema_hash, xsink);
        if (*xsink) {
            return;
        }

        // Check for circular $ref before compiling
        if (checkCircularRefs(json_schema, xsink)) {
            return;
        }

        // Compile the schema; the document is moved, as copying it recurses for each level
        schema = std::make_shared<JsonSchemaType>(jsoncons::jsonschema::make_json_schema(std::move(json_schema)));
        valid = true;
    } catch (const JsonSchemaAbort&) {
        // the exception has been raised in xsink
    } catch (const jsoncons::jsonschema::schema_error& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Invalid JSON Schema: %s", e.what());
    } catch (const std::exception& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Error compiling schema: %s", e.what());
    }
}

JsonSchemaValidator::~JsonSchemaValidator() {
}

void JsonSchemaValidator::initFromJsonString(const std::string& json_str, ExceptionSink* xsink) {
    SchemaRecursionHelper srh(xsink);
    try {
        // Parse the JSON string
        jsoncons::json json_schema = jsoncons::json::parse(json_str);

        // Check for circular $ref before compiling
        if (checkCircularRefs(json_schema, xsink)) {
            return;
        }

        // Compile the schema; the document is moved, as copying it recurses for each level
        schema = std::make_shared<JsonSchemaType>(jsoncons::jsonschema::make_json_schema(std::move(json_schema)));
        valid = true;
    } catch (const JsonSchemaAbort&) {
        // the exception has been raised in xsink
    } catch (const jsoncons::ser_error& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Invalid JSON in schema: %s", e.what());
    } catch (const jsoncons::jsonschema::schema_error& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Invalid JSON Schema: %s", e.what());
    } catch (const std::exception& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Error compiling schema: %s", e.what());
    }
}

jsoncons::json JsonSchemaValidator::qoreToJson(QoreValue val, ExceptionSink* xsink, int depth) const {
    if (val.isNullOrNothing()) {
        return jsoncons::json::null();
    }

    qore_type_t type = val.getType();
    if (type == NT_LIST || type == NT_HASH) {
        // jsoncons compiles, validates and compares documents recursively, so a converted value is limited to the
        // nesting depth of a JSON document that jsoncons parses
        static const int max_depth = jsoncons::json_options().max_nesting_depth();
        if (depth >= max_depth) {
            xsink->raiseException("JSON-SCHEMA-ERROR", "the value is nested more than %d levels deep", max_depth);
            return jsoncons::json::null();
        }
        if (q_check_stack(xsink)) {
            return jsoncons::json::null();
        }
    }

    switch (type) {
        case NT_INT:
            return jsoncons::json(val.getAsBigInt());

        case NT_FLOAT:
            return jsoncons::json(val.getAsFloat());

        case NT_BOOLEAN:
            return jsoncons::json(val.getAsBool());

        case NT_STRING: {
            QoreStringValueHelper str(val);
            return jsoncons::json(str->c_str());
        }

        case NT_LIST: {
            const QoreListNode* list = val.get<const QoreListNode>();
            jsoncons::json arr = jsoncons::json::array();
            ConstListIterator it(list);
            while (it.next()) {
                arr.push_back(qoreToJson(it.getValue(), xsink, depth + 1));
                if (*xsink) {
                    return jsoncons::json::null();
                }
            }
            return arr;
        }

        case NT_HASH: {
            const QoreHashNode* hash = val.get<const QoreHashNode>();
            jsoncons::json obj = jsoncons::json::object();
            ConstHashIterator it(hash);
            while (it.next()) {
                obj[it.getKey()] = qoreToJson(it.get(), xsink, depth + 1);
                if (*xsink) {
                    return jsoncons::json::null();
                }
            }
            return obj;
        }

        case NT_NUMBER: {
            const QoreNumberNode* num = val.get<const QoreNumberNode>();
            QoreString str;
            num->toString(str);
            // Try to parse as a decimal number
            try {
                return jsoncons::json::parse(str.c_str());
            } catch (...) {
                return jsoncons::json(num->getAsFloat());
            }
        }

        case NT_DATE: {
            const DateTimeNode* dt = val.get<const DateTimeNode>();
            QoreString str;
            dt->format(str, "YYYY-MM-DDTHH:mm:SS.xxZ");
            return jsoncons::json(str.c_str());
        }

        case NT_BINARY: {
            const BinaryNode* bin = val.get<const BinaryNode>();
            // Encode as base64 using Qore's built-in function
            SimpleRefHolder<QoreStringNode> b64(new QoreStringNode());
            b64->concatBase64(bin);
            return jsoncons::json(b64->c_str());
        }

        default:
            // For unknown types, try to get a string representation
            return jsoncons::json::null();
    }
}

QoreValue JsonSchemaValidator::jsonToQore(const jsoncons::json& j, ExceptionSink* xsink) const {
    switch (j.type()) {
        case jsoncons::json_type::null_value:
            return QoreValue();

        case jsoncons::json_type::bool_value:
            return QoreValue(j.as_bool());

        case jsoncons::json_type::int64_value:
            return QoreValue(j.as<int64_t>());

        case jsoncons::json_type::uint64_value:
            return QoreValue(static_cast<int64_t>(j.as<uint64_t>()));

        case jsoncons::json_type::half_value:
        case jsoncons::json_type::double_value:
            return QoreValue(j.as_double());

        case jsoncons::json_type::string_value:
            return new QoreStringNode(j.as_string().c_str());

        case jsoncons::json_type::array_value: {
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
            for (const auto& item : j.array_range()) {
                list->push(jsonToQore(item, xsink), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            return list.release();
        }

        case jsoncons::json_type::object_value: {
            ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
            for (const auto& kv : j.object_range()) {
                hash->setKeyValue(kv.key().c_str(), jsonToQore(kv.value(), xsink), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            return hash.release();
        }

        default:
            return QoreValue();
    }
}

bool JsonSchemaValidator::validate(QoreValue data, ExceptionSink* xsink) const {
    if (!valid || !schema) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Schema is not valid");
        return false;
    }

    SchemaRecursionHelper srh(xsink);
    try {
        jsoncons::json json_data = qoreToJson(data, xsink);
        if (*xsink) {
            return false;
        }

        // Create an error handler that tracks errors
        std::vector<JsonSchemaError> errors;
        auto reporter = [&errors](const jsoncons::jsonschema::validation_message& msg) {
            JsonSchemaError err;
            err.path = msg.instance_location().string();
            err.schema_path = msg.schema_location().string();
            err.keyword = msg.keyword();
            err.message = msg.message();
            errors.push_back(err);
            return jsoncons::jsonschema::walk_result::advance;
        };

        schema->validate(json_data, reporter);
        return errors.empty();
    } catch (const JsonSchemaAbort&) {
        // the exception has been raised in xsink
        return false;
    } catch (const std::exception& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Validation error: %s", e.what());
        return false;
    }
}

QoreHashNode* JsonSchemaValidator::validateWithErrors(QoreValue data, ExceptionSink* xsink) const {
    assert(hashdeclJsonSchemaValidationError);
    assert(hashdeclJsonSchemaValidationResult);

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(hashdeclJsonSchemaValidationResult, xsink), xsink);

    if (!valid || !schema) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Schema is not valid");
        return nullptr;
    }

    SchemaRecursionHelper srh(xsink);
    try {
        jsoncons::json json_data = qoreToJson(data, xsink);
        if (*xsink) {
            return nullptr;
        }

        // Create an error handler that collects all errors
        ReferenceHolder<QoreListNode> error_list(
            new QoreListNode(hashdeclJsonSchemaValidationError->getTypeInfo()), xsink
        );
        auto reporter = [&error_list, xsink](const jsoncons::jsonschema::validation_message& msg) {
            ReferenceHolder<QoreHashNode> err(new QoreHashNode(hashdeclJsonSchemaValidationError, xsink), xsink);
            err->setKeyValue("path", new QoreStringNode(msg.instance_location().string()), xsink);
            err->setKeyValue("schema_path", new QoreStringNode(msg.schema_location().string()), xsink);
            err->setKeyValue("keyword", new QoreStringNode(msg.keyword()), xsink);
            err->setKeyValue("message", new QoreStringNode(msg.message()), xsink);
            error_list->push(err.release(), xsink);
            return jsoncons::jsonschema::walk_result::advance;
        };

        schema->validate(json_data, reporter);

        result->setKeyValue("valid", error_list->size() == 0, xsink);
        result->setKeyValue("errors", error_list.release(), xsink);

        return result.release();
    } catch (const JsonSchemaAbort&) {
        // the exception has been raised in xsink
        return nullptr;
    } catch (const std::exception& e) {
        xsink->raiseException("JSON-SCHEMA-ERROR", "Validation error: %s", e.what());
        return nullptr;
    }
}

// Resolve a JSON Pointer (e.g. "#/$defs/foo") against a root JSON document
static const jsoncons::json* resolveJsonPointer(const jsoncons::json& root, const std::string& ref) {
    // Must start with '#'
    if (ref.empty() || ref[0] != '#') {
        return nullptr;
    }
    // "#" alone refers to root
    if (ref.size() == 1) {
        return &root;
    }
    // Must be "#/" followed by path
    if (ref.size() < 2 || ref[1] != '/') {
        return nullptr;
    }

    const jsoncons::json* current = &root;
    std::string path = ref.substr(2); // skip "#/"

    size_t pos = 0;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        std::string segment = (next == std::string::npos) ? path.substr(pos) : path.substr(pos, next - pos);

        // JSON Pointer unescaping: ~1 -> /, ~0 -> ~
        std::string unescaped;
        for (size_t i = 0; i < segment.size(); ++i) {
            if (segment[i] == '~' && i + 1 < segment.size()) {
                if (segment[i + 1] == '1') {
                    unescaped += '/';
                    ++i;
                    continue;
                } else if (segment[i + 1] == '0') {
                    unescaped += '~';
                    ++i;
                    continue;
                }
            }
            unescaped += segment[i];
        }

        if (current->is_object()) {
            auto it = current->find(unescaped);
            if (it == current->object_range().end()) {
                return nullptr;
            }
            current = &(it->value());
        } else if (current->is_array()) {
            try {
                size_t idx = std::stoul(unescaped);
                if (idx >= current->size()) {
                    return nullptr;
                }
                current = &((*current)[idx]);
            } catch (...) {
                return nullptr;
            }
        } else {
            return nullptr;
        }

        pos = (next == std::string::npos) ? path.size() : next + 1;
    }

    return current;
}

// A schema object whose pure $ref chains are being followed
struct RefCycleFrame {
    const jsoncons::json* node;
    // the $ref target that this frame explores, or nullptr
    const jsoncons::json* target;
    // the sub-schemas to follow and their $ref targets, filled when the frame is entered
    std::vector<std::pair<const jsoncons::json*, const jsoncons::json*>> next;
    size_t pos = 0;
    bool entered = false;

    RefCycleFrame(const jsoncons::json* node, const jsoncons::json* target) : node(node), target(target) {
    }
};

// Follow pure $ref chains to detect unconditional cycles.
// A "pure $ref" node is one where the schema object's only meaningful keyword is "$ref"
// (i.e., it resolves to another schema purely by reference with no other constraints).
// This detects cycles like: A -> B -> A, or self -> self.
// It does NOT flag recursive schemas where $ref appears inside "properties", "items", etc.,
// since those are data-driven and terminate naturally.
// The search is a depth-first search with an explicit stack, as a $ref chain can be longer than the call stack
// allows; a $ref target whose chains have been followed without finding a cycle is not followed again.
// Returns 1 if a cycle is found, 0 if not, and -1 if an exception has been raised.
static int detectPureRefCycle(const jsoncons::json& root, ExceptionSink* xsink) {
    static const char* array_keywords[] = {"allOf", "anyOf", "oneOf"};
    // the $ref targets on the current path, and the $ref targets whose chains have no cycle
    std::set<const jsoncons::json*> active = {&root};
    std::set<const jsoncons::json*> done;
    std::vector<RefCycleFrame> stack;
    stack.emplace_back(&root, &root);
    unsigned iteration = 0;
    while (!stack.empty()) {
        if (!(++iteration % 100) && qore_check_cancel(xsink, "checking JSON Schema references")) {
            return -1;
        }
        RefCycleFrame& frame = stack.back();
        if (!frame.entered) {
            frame.entered = true;
            const jsoncons::json& node = *frame.node;
            if (node.is_object()) {
                auto ref_it = node.find("$ref");
                if (ref_it == node.object_range().end() || !ref_it->value().is_string()) {
                    // No $ref at this level; follow the sub-schema keywords to find nested pure-ref chains
                    for (const char* kw : array_keywords) {
                        auto it = node.find(kw);
                        if (it != node.object_range().end() && it->value().is_array()) {
                            for (const auto& item : it->value().array_range()) {
                                frame.next.emplace_back(&item, nullptr);
                            }
                        }
                    }
                } else {
                    std::string ref = ref_it->value().as<std::string>();
                    // Only handle local refs
                    const jsoncons::json* target = !ref.empty() && ref[0] == '#'
                        ? resolveJsonPointer(root, ref)
                        : nullptr;
                    if (target) {
                        if (active.count(target)) {
                            return 1; // cycle detected
                        }
                        if (!done.count(target)) {
                            active.insert(target);
                            frame.next.emplace_back(target, target);
                        }
                    }
                }
            }
        }
        if (frame.pos < frame.next.size()) {
            // the frame reference is invalid after the next frame is added
            std::pair<const jsoncons::json*, const jsoncons::json*> next = frame.next[frame.pos++];
            stack.emplace_back(next.first, next.second);
            continue;
        }
        if (frame.target) {
            active.erase(frame.target);
            done.insert(frame.target);
        }
        stack.pop_back();
    }
    return 0;
}

bool JsonSchemaValidator::checkCircularRefs(const jsoncons::json& schema_json, ExceptionSink* xsink) {
    int rc = detectPureRefCycle(schema_json, xsink);
    if (rc > 0) {
        xsink->raiseException("JSON-SCHEMA-ERROR",
            "Schema contains circular $ref references that would cause infinite recursion during validation");
    }
    return rc != 0;
}
