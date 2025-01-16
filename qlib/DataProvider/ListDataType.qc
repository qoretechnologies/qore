# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ListDataType class definition

/** ListDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on a hash
public class ListDataType inherits QoreDataType {
    private {
        #! element type
        AbstractDataProviderType element_type;
    }

    #! creates the object
    private constructor(string name, Type qelement_type, AbstractDataProviderType element_type, bool or_nothing,
            *hash<auto> options, *hash<auto> tags, *list<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(qelement_type, options, tags, default_value, attr) {
        self.name = name;
        self.element_type = element_type;
    }

    #! creates the object
    constructor(AbstractDataProviderType element_type, bool or_nothing = False, *hash<auto> options,
            *hash<auto> tags, *list<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(ListDataType::getQoreType(element_type.getValueType(), or_nothing), options, tags,
                default_value, attr) {
        if (!name) {
            name = sprintf("%slist<%s>", or_nothing ? "*" : "", element_type.getName());
        }
        self.element_type = element_type;
        if (element_type.getTags().dynamic) {
            setDynamicTag();
        }
    }

    #! creates the object
    constructor(Type element_type, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *list<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(ListDataType::getQoreType(element_type, or_nothing), options, tags, default_value, attr) {
        if (!name) {
            name = sprintf("%slist<%s>", or_nothing ? "*" : "", element_type.getName());
        }
        self.element_type = AbstractDataProviderType::get(element_type);
    }

    #! creates the object
    constructor(string name, AbstractDataProviderType element_type, bool or_nothing = False, *hash<auto> options,
            *hash<auto> tags, *list<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(ListDataType::getQoreType(element_type.getValueType(), or_nothing), options, tags,
                default_value, attr) {
        self.name = name;
        self.element_type = element_type;
        if (element_type.getTags().dynamic) {
            setDynamicTag();
        }
    }

    #! creates the object
    constructor(string name, Type element_type, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *list<auto> default_value, *hash<NameDescInfo> attr)
            : QoreDataType(ListDataType::getQoreType(element_type, or_nothing), options, tags) {
        self.name = name;
        self.element_type = AbstractDataProviderType::get(element_type);
    }

    #! Copies the object and assigns a new element type to the copy
    ListDataType copyWithElementType(AbstractDataProviderType element_type) {
        ListDataType rv = copy();
        rv.element_type = element_type;
        if (rv.element_type.getTags().dynamic) {
            rv.setDynamicTag();
        }
        return rv;
    }

    #! Returns the description
    string getDesc() {
        return desc ?? sprintf("Qore type %slist<%s>", accept_type_hash."nothing" ? "*" : "",
            element_type ? element_type.getName() : "auto");
    }

    #! returns the base type for the type, if any
    *Type getValueType() {
        return type;
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
        value = QoreDataType::acceptsValue(value);
        if (element_type.hasType()) {
            value = map element_type.acceptsValue($1), value;
        }
        return value;
    }

    #! Updates the element type
    updateElementType(AbstractDataProviderType element_type) {
        self.element_type = element_type;
    }

    #! Returns the underlying %Qore type for this type
    private Type getQoreType(Type element_type, bool or_nothing) {
        return new Type(sprintf("%slist<%s>", or_nothing ? "*" : "", element_type.getName()));
    }
}
}
