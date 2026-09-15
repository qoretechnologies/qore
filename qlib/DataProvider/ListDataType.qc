# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ListDataType class definition

/** ListDataType.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
    constructor(hash<QoreTypeInfo> info, Type element_type = AutoType) : QoreDataType(ListDataType::checkInfo(info)) {
        self.element_type = AbstractDataProviderType::get(element_type);
    }

    #! creates the object
    constructor(hash<QoreTypeInfo> info, AbstractDataProviderType element_type)
            : QoreDataType(ListDataType::checkInfo(info)) {
        self.element_type = element_type;
        if (element_type.getTags().dynamic) {
            setDynamicTag();
        }
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
            : QoreDataType(ListDataType::getQoreType(element_type, or_nothing), options, tags,
                default_value, attr) {
        if (!name || !element_type.getValueType()) {
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
            : QoreDataType(ListDataType::getQoreType(element_type, or_nothing), options, tags,
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
        rv.setElementTypeIntern(element_type);
        return rv;
    }

    #! Returns a "soft" type equivalent to the current type
    /** The element type is converted as well; without this, a soft conversion would leave every type reachable
        through a list unconverted.

        @return a list of the same kind whose element type is the soft equivalent of this type's element type

        @since DataProvider 3.8
    */
    AbstractDataProviderType getSoftType() {
        # an unparameterized list type has a soft equivalent of its own in the base class, which replaces the list
        # entirely; the element type carries no information in that case
        AbstractDataProviderType base_soft = QoreDataType::getSoftType();
        # the element type is not guaranteed to be set; getDesc() allows for it as well
        if (!element_type || base_soft.uniqueHash() != self.uniqueHash()) {
            return base_soft;
        }
        # hold a conversion scope and publish this type before converting the element type, so that a type shared
        # with another part of the graph is converted once and a recursive edge resolves to this object; see
        # AbstractDataProviderType::getSoftTypeShared()
        AbstractDataProviderType::softTypeScopeEnter();
        on_exit AbstractDataProviderType::softTypeScopeExit();
        ListDataType rv = copy();
        AbstractDataProviderType::publishSoftType(self, rv);
        rv.setElementTypeIntern(AbstractDataProviderType::getSoftTypeShared(element_type));
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
        setElementTypeIntern(element_type);
    }

    #! Assigns the element type and updates the type information derived from it
    /** The %Qore type, the accepted and returned type hashes and the generated name all describe the element type,
        so they must be regenerated with it; otherwise the list would go on describing - and accepting - values of
        the element type it no longer has.

        A name or display name that was not generated from the element type was given explicitly, so it is kept.

        @param element_type the new element type

        @since DataProvider 3.8
    */
    private setElementTypeIntern(AbstractDataProviderType element_type) {
        # the element type is not guaranteed to be set; getDesc() allows for it as well
        if (self.element_type && self.element_type.uniqueHash() == element_type.uniqueHash()) {
            return;
        }
        # the name was generated either from the %Qore type or from the element type's name; regenerate it the
        # same way, so that a list named after the old element type is not left describing it
        bool qore_type_name = (name == type.getName());
        bool generated_name = qore_type_name
            || (self.element_type && name == getListTypeName(self.element_type));
        bool generated_display_name = (display_name == AbstractDataField::getDisplayName(name));
        self.element_type = element_type;
        if (element_type.getTags().dynamic) {
            setDynamicTag();
        }
        type = getQoreType(element_type, or_nothing_type);
        soft_type = (type.getName() =~ /^.?soft/);
        accept_type_hash = type.getAcceptTypeHash();
        return_type_hash = type.getReturnTypeHash();
        accept_type_hash_simple = type.getAcceptTypeHash(True);
        return_type_hash_simple = type.getReturnTypeHash(True);
        if (options."qore.no_null") {
            accept_type_hash -= "null";
            accept_type_hash_simple -= "null";
        }
        if (generated_name) {
            name = qore_type_name ? type.getName() : getListTypeName(element_type);
            if (generated_display_name) {
                display_name = AbstractDataField::getDisplayName(name);
            }
        }
    }

    #! Returns the type name generated for a list of the given element type
    /** @param element_type the element type

        @return the type name generated for a list of \a element_type

        @since DataProvider 3.8
    */
    private string getListTypeName(AbstractDataProviderType element_type) {
        return sprintf("%slist<%s>", or_nothing_type ? "*" : "", element_type.getName());
    }

    #! Checks the info hash for required fields
    private static hash<QoreTypeInfo> checkInfo(hash<QoreTypeInfo> info) {
        if (!info.type) {
            info.type = AutoListType;
        } else if (info.type.getBaseTypeCode() != NT_LIST) {
            throw "DATA-PROVIDER-ERROR", sprintf("ListDataType cannot be created from type %y not compatible with "
                "'list'", info.type.getName());
        }
        if (!info.name) {
            info.name = info.type.getName();
        }
        return info;
    }

    #! Returns the underlying %Qore type for this type
    private Type getQoreType(Type element_type, bool or_nothing) {
        return new Type(sprintf("%slist<%s>", or_nothing ? "*" : "", element_type.getName()));
    }

    #! Returns the underlying %Qore type for this type
    private Type getQoreType(AbstractDataProviderType element_type, bool or_nothing) {
        *Type qtype = element_type.getValueType();
        return qtype
            ? ListDataType::getQoreType(qtype, or_nothing)
            : (or_nothing ? AutoListOrNothingType : AutoListType);
    }
}
}
