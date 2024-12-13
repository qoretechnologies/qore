# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDataField class definition

/** QoreDataField.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! describes a data type based on a hashdecl
public class QoreDataField inherits AbstractDataField {
    private {
        #! Name of the field
        string name;

        #! Display name of the field
        string display_name;

        #! Short description of the field
        *string short_desc;

        #! description of the field
        *string desc;

        #! field type
        AbstractDataProviderType type;

        #! Example value for the field
        auto example_value;
    }

    #! creates the field from the argument
    constructor(hash<auto> h) {
        if (!h.name && h.display_name) {
            h.name = AbstractDataField::getNameFromDisplayName(h.display_name);
        }
        checkString(False, h, "name", "desc");
        checkString(False, h, "display_name", "short_desc");
        checkHash(False, h, "attr");
        if (h.type instanceof Qore::Reflection::Type) {
            type = AbstractDataProviderType::get(h.type);
        } else if (h.type instanceof AbstractDataProviderType) {
            type = h.type;
        } else if (h.type instanceof hash<DataTypeInfo>) {
            type = AbstractDataProviderType::get(h.type);
        } else {
            throw "QORE-DATA-FIELD-ERROR", sprintf("missing or invalid key \"type\"; expecting type \"Type\" or "
                "\"AbstractDataProviderType\"; got %y instead (hash: %y)", h.type.fullType(), h);
        }
        name = h.name;
        desc = h.desc;
        display_name = h.display_name ?? AbstractDataField::getDisplayName(name);
        short_desc = h.short_desc ?? AbstractDataField::getShortDescription(desc);
        if (exists h.default_value) {
            setDefaultValue(h.default_value);
        }
        if (h.multiselect) {
            multiselect = True;
        }
        if (exists h.allowed_values) {
            setAllowedValues(h.allowed_values, h.allowed_values_creatable);
        }
        if (exists h.example_value) {
            example_value = type.acceptsValue(h.example_value);
        }
        if (h.allowed_values_creatable) {
            allowed_values_creatable = True;
            if (allowed_values_map) {
                remove allowed_values_map;
            }
        }
        if (h.attr) {
            attr += h.attr;
        }
    }

    #! creates the field from the argument
    constructor(Qore::Reflection::TypedHashMember mem) {
        name = mem.getName();
        if (exists (auto dv = mem.getDefaultValue())) {
            setDefaultValue(dv);
        }
        type = AbstractDataProviderType::get(mem.getType());
    }

    #! creates the field from the arguments
    constructor(string name, *string desc, Type type, auto default_value, *softlist<auto> allowed_values,
            *bool multiselect, *bool allowed_values_creatable) {
        self.name = name;
        display_name = AbstractDataField::getDisplayName(name);
        if (desc) {
            self.desc = desc;
            short_desc = AbstractDataField::getShortDescription(desc);
        }
        self.type = AbstractDataProviderType::get(type);
        if (exists default_value) {
            setDefaultValue(default_value);
        }
        if (multiselect) {
            self.multiselect = True;
        }
        if (exists allowed_values) {
            setAllowedValues(allowed_values, allowed_values_creatable);
        }
        if (allowed_values_creatable) {
            self.allowed_values_creatable = True;
        }
    }

    #! creates the field from the arguments
    constructor(string name, *string desc, AbstractDataProviderType type, auto default_value,
            *softlist<auto> allowed_values, *bool multiselect, *bool allowed_values_creatable) {
        self.name = display_name = name;
        self.desc = short_desc = desc;
        self.type = type;
        if (exists default_value) {
            setDefaultValue(default_value);
        }
        if (multiselect) {
            self.multiselect = True;
        }
        if (exists allowed_values) {
            setAllowedValues(allowed_values, allowed_values_creatable);
        }
        if (allowed_values_creatable) {
            self.allowed_values_creatable = True;
        }
    }

    #! returns the name of the field
    string getName() {
        return name;
    }

    #! Returns the display name of the field
    string getDisplayName() {
        # display_name can be NOTHING if it's been deserialized from a previous version of the class
        return display_name ?? name;
    }

    #! Returns the short description, if any
    *string getShortDescription() {
        return short_desc;
    }

    #! returns the description, if any
    *string getDescription() {
        return desc;
    }

    #! returns the type of the field
    AbstractDataProviderType getType() {
        return type;
    }

    #! adds a string to the description
    /** @param separator string to add first if there is already a description
        @param new_desc the new string to add to the description
    */
    appendDescription(string separator, string new_desc) {
        if (desc) {
            desc += separator;
        }
        desc += new_desc;
    }

    #! returns a field with a "soft" type equivalent to the current type
    /** @return a field with a "soft" type equivalent to the current type
    */
    AbstractDataField getSoftType() {
        return getNewFieldType(type.getSoftType());
    }

    #! returns a field with an "or nothing" type equivalent to the current type
    /** @return a field with an "or nothing" type equivalent to the current type
    */
    AbstractDataField getOrNothingType() {
        return getNewFieldType(type.getOrNothingType());
    }

    #! Returns a mandatory (i.e. not "or nothing") field equivalent to the current field
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory field

        @return a mandatory (i.e. not "or nothing") field equivalent to the current field

        @since DataProvider 3.0
    */
    AbstractDataField getMandatoryType() {
        return getNewFieldType(type.getMandatoryType());
    }

    #! Updates the field type
    AbstractDataField updateFieldType(AbstractDataProviderType type) {
        self.type = type;
        return self;
    }

    #! Returns any example value for the field, if present
    /** @param emap a hash to ensure that examples are only produced once

        @return any example value for the field, otherwise the default value for the field, otherwise the example
        value for the type

        @since DataProvider 3.0
    */
    auto getExampleValue(*hash<string, bool> emap) {
        return example_value ?? default_value ?? AbstractDataField::getExampleValue(emap);
    }

    #! returns information about the field
    hash<DataFieldInfo> getConfigInfo() {
        return <DataFieldInfo>{
            "name": name,
            "display_name": display_name,
            "short_desc": short_desc,
            "desc": desc,
            "default_value": default_value,
            "example_value": example_value,
            "allowed_values": allowed_values,
            "allowed_values_creatable": allowed_values_creatable ?? False,
            "type": getType().getInfo(){"name", "path", "base_type", "options", "default_value", "mandatory", "tags",
                "default_field_type_info"},
            "attr": attr,
        };
    }

    #! returns a field with the new type
    /** @return a field with the new type
    */
    private AbstractDataField getNewFieldType(AbstractDataProviderType new_type) {
        AbstractDataProviderType type = getType();
        if (new_type == type) {
            return self;
        }
        QoreDataField rv = copy();
        rv.type = new_type;
        return rv;
    }

    #! Checks for string values in a hash
    private static checkString(bool required, hash<auto> h, ...) {
        foreach string key in (argv) {
            if (h{key}.typeCode() != NT_STRING && (required || exists h{key})) {
                throw "QORE-DATA-FIELD-ERROR", sprintf("missing or invalid key %y; expecting type \"string\"; got %y "
                    "instead (value: %y)", key, h{key}.fullType(), h{key});
            }
        }
    }

    #! Checks for hash values in a hash
    private static checkHash(bool required, hash<auto> h, ...) {
        foreach string key in (argv) {
            if (h{key}.typeCode() != NT_HASH && (required || exists h{key})) {
                throw "QORE-DATA-FIELD-ERROR", sprintf("missing or invalid key %y; expecting type \"hash\"; got %y "
                    "instead (value: %y)", key, h{key}.fullType(), h{key});
            }
        }
    }
}
}
