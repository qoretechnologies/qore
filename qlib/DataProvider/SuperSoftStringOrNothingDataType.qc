# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SuperSoftStringOrNothingDataType.qc Copyright (C) 2025 Qore Technologies s.r.o.

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
public const SuperSoftStringOrNothingDataType = new SuperSoftStringOrNothingDataType();

#! A type that converts almost any type to a string
public class SuperSoftStringOrNothingDataType inherits SuperSoftStringDataType {
    #! creates the field from the argument
    constructor(*hash<auto> options, *hash<auto> tags) : SuperSoftStringDataType(options, tags) {
    }

    #! Returns the type name
    string getName() {
        return "*supersoftstring";
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return "Super Soft String Or Nothing";
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return "Accepts almost any type and converts it to a string; accepts and returns no value as well";
    }

    #! Returns the description
    string getDesc() {
        return "Accepts almost any type and converts it to a string; accepts and returns no value as well";
    }

    #! Returns the base type for the type, if any
    *Type getValueType() {
        return StringOrNothingType;
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
            Type::NothingType: True,
            Type::NullType: True,
        };
    }

    #! Returns a hash of types returned by this type; keys are type names
    hash<string, bool> getReturnTypeHash(*bool simple) {
        return {
            Type::String: True,
            Type::NothingType: True,
        };
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        if (!exists value || value === NULL) {
            return;
        }
        return SuperSoftStringDataType::acceptsValue(value);
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory type

        @return a mandatory (i.e. not "or nothing") type equivalent to the current type
    */
    AbstractDataProviderType getMandatoryType() {
        return SuperSoftStringDataType;
    }
}
}
