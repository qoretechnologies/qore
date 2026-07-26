/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_JsonSchema.h JSON Schema validation class definition */
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

#ifndef _QORE_QC_JSONSCHEMA_H
#define _QORE_QC_JSONSCHEMA_H

#include "qore-json-module.h"

#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <memory>
#include <string>
#include <vector>

DLLLOCAL QoreClass* initJsonSchemaClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_JsonSchemaValidationError(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_JsonSchemaValidationResult(QoreNamespace& ns);
DLLLOCAL extern const TypedHashDecl* hashdeclJsonSchemaValidationError;
DLLLOCAL extern const TypedHashDecl* hashdeclJsonSchemaValidationResult;

//! JSON Schema validation error structure
struct JsonSchemaError {
    std::string path;       //!< JSON Pointer to error location
    std::string keyword;    //!< Failed validation keyword
    std::string message;    //!< Human-readable error message
    std::string schema_path; //!< Path in schema that caused failure
};

//! Type alias for JSON schema
using JsonSchemaType = jsoncons::jsonschema::json_schema<jsoncons::json>;

//! JSON Schema validator class
class JsonSchemaValidator : public AbstractPrivateData {
public:
    //! Constructor from JSON schema string
    /** @param schema_json the JSON schema as a string
        @param xsink exception sink
    */
    DLLLOCAL JsonSchemaValidator(const QoreStringNode* schema_json, ExceptionSink* xsink);

    //! Constructor from Qore hash schema
    /** @param schema the schema as a Qore hash
        @param xsink exception sink
    */
    DLLLOCAL JsonSchemaValidator(const QoreHashNode* schema, ExceptionSink* xsink);

    DLLLOCAL virtual ~JsonSchemaValidator();

    //! Validate data against the schema
    /** @param data the data to validate
        @param xsink exception sink
        @return true if valid, false otherwise
    */
    DLLLOCAL bool validate(QoreValue data, ExceptionSink* xsink) const;

    //! Validate data and return detailed errors
    /** @param data the data to validate
        @param xsink exception sink
        @return a hash with 'valid' (bool) and 'errors' (list) keys
    */
    DLLLOCAL QoreHashNode* validateWithErrors(QoreValue data, ExceptionSink* xsink) const;

    //! Check if the schema was compiled successfully
    DLLLOCAL bool isValid() const { return valid; }

private:
    //! Convert Qore value to jsoncons JSON
    DLLLOCAL jsoncons::json qoreToJson(QoreValue val, ExceptionSink* xsink) const;

    //! Convert jsoncons JSON to Qore value
    DLLLOCAL QoreValue jsonToQore(const jsoncons::json& j, ExceptionSink* xsink) const;

    //! Initialize schema from JSON string
    DLLLOCAL void initFromJsonString(const std::string& json_str, ExceptionSink* xsink);

    //! Check for circular $ref references in the schema
    /** @param schema_json the parsed JSON schema
        @param xsink exception sink
        @return true if circular refs detected (exception raised), false if OK
    */
    DLLLOCAL static bool checkCircularRefs(const jsoncons::json& schema_json, ExceptionSink* xsink);

    //! The compiled JSON schema validator (heap allocated for easier management)
    std::shared_ptr<JsonSchemaType> schema;

    //! Whether the schema is valid
    bool valid;
};

#endif // _QORE_QC_JSONSCHEMA_H
