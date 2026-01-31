# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeneratorFieldDefinitionDataType class definition

/** GeneratorFieldDefinitionDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeneratorDataProvider module
public namespace GeneratorDataProvider {

#! Data type for a single field definition in the generator schema
/** Each field definition has a required name and type, and an optional description.
*/
public class GeneratorFieldDefinitionDataType inherits DataProvider::HashDataType {
    public {
        #! Field definition fields
        const Fields = {
            "name": {
                "display_name": "Field Name",
                "short_desc": "The name of the field",
                "desc": "The name of the field in the generated record",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "type": {
                "display_name": "Field Type",
                "short_desc": "The data type of the field",
                "desc": "The data type of the field; determines the kind of values generated",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": GeneratorFieldType::String,
                        "desc": "String values",
                    },
                    <AllowedValueInfo>{
                        "value": GeneratorFieldType::Int,
                        "desc": "Integer values",
                    },
                    <AllowedValueInfo>{
                        "value": GeneratorFieldType::Float,
                        "desc": "Floating-point values",
                    },
                    <AllowedValueInfo>{
                        "value": GeneratorFieldType::Bool,
                        "desc": "Boolean values",
                    },
                    <AllowedValueInfo>{
                        "value": GeneratorFieldType::Date,
                        "desc": "Date/time values",
                    },
                ),
            },
            "desc": {
                "display_name": "Description",
                "short_desc": "An optional description of the field",
                "desc": "An optional human-readable description of the field",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("GeneratorFieldDefinition") {
        addQoreFields(Fields);
    }
}
}
