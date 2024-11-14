# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SoftListDataType class definition

/** SoftListDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%new-style
# require type definitions everywhere
%require-types
#! strict argument handling
%strict-args
# enable all warnings
%enable-all-warnings

%requires reflection

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type based on a hash
public class SoftListDataType inherits ListDataType {
    #! creates the object
    constructor(AbstractDataProviderType element_type, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *list<auto> default_value, *hash<NameDescInfo> attr)
            : ListDataType(sprintf("%ssoftlist<%s>", or_nothing ? "*" : "", element_type.getName()),
            SoftListDataType::getQoreType(element_type.getValueType(), or_nothing), element_type,
            or_nothing, options, tags, default_value, attr) {
    }

    #! creates the object
    constructor(Type element_type, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *list<auto> default_value, *hash<NameDescInfo> attr)
            : ListDataType(sprintf("%ssoftlist<%s>", or_nothing ? "*" : "", element_type.getName()),
            SoftListDataType::getQoreType(element_type, or_nothing), AbstractDataProviderType::get(element_type),
            or_nothing, options, tags, default_value, attr) {
    }

    #! creates the object
    constructor(string name, AbstractDataProviderType element_type, bool or_nothing = False, *hash<auto> options,
            *hash<auto> tags, *list<auto> default_value, *hash<NameDescInfo> attr)
            : ListDataType(name, SoftListDataType::getQoreType(element_type.getValueType(), or_nothing),
            element_type, or_nothing, options, tags, default_value, attr) {
    }

    #! creates the object
    constructor(string name, Type element_type, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *list<auto> default_value, *hash<NameDescInfo> attr)
            : ListDataType(name,
            SoftListDataType::getQoreType(element_type, or_nothing), AbstractDataProviderType::get(element_type),
            or_nothing, options, tags. default_value, attr) {
    }

    #! returns the subtype (for lists or hashes) if there is only one
    *AbstractDataProviderType getElementType() {
        return element_type;
    }

    #! returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        # this method intentionally left blank
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        string typename = value.type();
        on_error rethrow $1.err, sprintf("%s (while accepting a value of type %y for type %y (%s)", $1.desc,
            value.fullType(), name, type.getName());

        if (typename == Type::List) {
            return map element_type.acceptsValue($1), value;
        } else {
            return (element_type.acceptsValue(value),);
        }
    }

    #! common constructor initialization
    private init(AbstractDataProviderType element_type, bool or_nothing) {
        string element_type_name = element_type.getName();
        name = sprintf("%ssoftlist<%s>", or_nothing ? "*" : "", element_type_name);
        self.element_type = element_type;
        accept_type_hash_simple = accept_type_hash = return_type_hash = return_type_hash_simple = {
            Type::List: True,
        };
        if (or_nothing) {
            accept_type_hash_simple."nothing" = accept_type_hash."nothing" = True;
            return_type_hash_simple."nothing" = return_type_hash."nothing" = True;
        }
        *Type element_base_type = element_type.getValueType();
        if (element_base_type) {
            accept_type_hash += element_base_type.getAcceptTypeHash();
            accept_type_hash_simple += element_base_type.getAcceptTypeHash();
        }
        if (accept_type_hash."any") {
            accept_type_hash_simple = accept_type_hash = {"any": True};
        }
    }

    #! Returns the underlying %Qore type for this type
    private Type getQoreType(Type element_type, bool or_nothing) {
        return new Type(sprintf("%ssoftlist<%s>", or_nothing ? "*" : "", element_type.getName()));
    }
}
}
