# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SuperSoftStringDataType.qc Copyright (C) 2025 Qore Technologies s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! A type that converts almost any type to a string
public const SuperSoftStringDataType = new SuperSoftStringDataType();

#! A type that converts to UNIX timestamps in seconds to dates
public class SuperSoftStringDataType inherits AbstractDataProviderType {
    #! creates the field from the argument
    constructor(*hash<auto> options, *hash<auto> tags) : AbstractDataProviderType(options, tags) {
    }

    #! Returns the type name
    string getName() {
        return "supersoftstring";
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return "Super Soft String";
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return "Accepts almost any type and converts it to a string";
    }

    #! Returns the description
    string getDesc() {
        return "Accepts almost any type and converts it to a string";
    }

    #! Returns the base type for the type, if any
    *Type getValueType() {
        return StringType;
    }

    #! Returns the subtype (for lists or hashes) if there is only one
    *AbstractDataProviderType getElementType() {
        # intentionally empty
    }

    #! Returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        # intentionally empty
    }

    #! Returns a hash of types accepted by this type; keys are type names
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        return {
            Type::String: True,
            Type::Int: True,
            Type::Float: True,
            Type::Number: True,
            Type::Date: True,
            Type::Boolean: True,
            Type::Binary: True,
            Type::Hash: True,
            Type::List: True,
        };
    }

    #! Returns a hash of types returned by this type; keys are type names
    hash<string, bool> getReturnTypeHash(*bool simple) {
        return {
            Type::String: True,
        };
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        return getStringValue(value);
    }

    #! Returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    static string getStringValue(auto value) {
        switch (value.typeCode()) {
            case NT_STRING:
                return value;

            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return value.toString();

            case NT_BOOLEAN:
                return value ? "true" : "false";

            case NT_DATE:
                return value.format("IF");

%ifndef NoYaml
            case NT_HASH:
            case NT_LIST:
                # strip YAML header (e.g., "%YAML 1.2\n--- " or "%YAML 1.2\n---\n") for inline values
                return regex_subst(trim(make_yaml(value)), "^%YAML [0-9.]+\\n---[ \\n]", "");
%endif

            default:
                throw "RUNTIME-TYPE-ERROR", sprintf("This data type does not accept values of type %y (%y)",
                    value.fullType(), value);
        }
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "or nothing" type

        @return an "or nothing" type equivalent to the current type
    */
    AbstractDataProviderType getOrNothingType() {
        return SuperSoftStringOrNothingDataType;
    }
}
}
