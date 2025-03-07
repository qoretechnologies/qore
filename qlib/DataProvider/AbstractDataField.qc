# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataField class definition

/** AbstractDataField.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

%requires reflection

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! describes a data type
public hashdecl DataFieldInfo {
    #! the name of the field
    string name;

    #! The display name of the field
    string display_name;

    #! The short description of the field in plain text
    *string short_desc;

    #! the description of the field, if available, with markdown formatting
    *string desc;

    #! the default value for the field
    auto default_value;

    #! Any example value for the field
    auto example_value;

    #! Are allowed values elements of the list's element type that indicate what values the list can have?
    bool multiselect;

    #! for enum fields; allowed values
    *list<hash<AllowedValueInfo>> allowed_values;

    #! Does the field accept values not defined in allowed_values too?
    *bool allowed_values_creatable;

    #! for enum fields; allowed values for elements for list types
    *list<hash<AllowedValueInfo>> element_allowed_values;

    #! Do elements of the field for list types accept values not defined in element_allowed_values too?
    *bool element_allowed_values_creatable;

    #! Type info
    hash<DataTypeInfo> type;

    #! Unique field identifier
    string unique_key;

    #! User-defined attributes set on the field
    *hash<auto> attr;
}

#! describes a data type based on a hashdecl
public class AbstractDataField inherits Qore::Serializable {
    private {
        #! the default value for the field, if any
        auto default_value;

        #! Are allowed values elements of the list's element type that indicate what values the list can have?
        bool multiselect;

        #! for enum fields; raw allowed values
        list<hash<AllowedValueInfo>> allowed_values;

        #! Does the field accept values not defined in allowed_values too?
        *bool allowed_values_creatable;

        #! for enum fields; allowed values for elements for list types
        *list<hash<AllowedValueInfo>> element_allowed_values;

        #! Do elements of the field for list types accept values not defined in element_allowed_values too?
        *bool element_allowed_values_creatable;

        #! for enum fields; allowed values
        /** only supported for types that convert to a string
        */
        hash<string, bool> allowed_values_map;

        #! for enum fields; allowed values for elements for list types
        /** only supported for types that convert to a string
        */
        hash<string, bool> element_allowed_values_map;

        #! User-defined custom attributes set on the field
        hash<auto> attr;
    }

    #! returns True if this field's type can be assigned from values of the argument type
    bool isAssignableFrom(AbstractDataProviderType t) {
        return getType().isAssignableFrom(t);
    }

    #! returns True if this field's type can be assigned from values of the argument type
    bool isAssignableFrom(Type t) {
        return getType().isAssignableFrom(t);
    }

    #! returns True if this field's type is a list
    bool isList() {
        return getType().isList();
    }

    #! returns True if the field's type must have a value
    bool isMandatory() {
        return getType().isMandatory();
    }

    #! Returns True if the type is a list and the allowed values are allowed values of the list's type
    bool isMultiselect() {
        return multiselect ?? False;
    }

    #! returns the value of the given option on the field's type
    auto getOptionValue(string opt) {
        return getType().getOptionValue(opt);
    }

    #! returns options set on the field's type
    *hash<auto> getOptions() {
        return getType().getOptions();
    }

    #! returns supported options on the field's type
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return getType().getSupportedOptions();
    }

    #! sets the given option on the field's type
    /** @param opt the option to set
        @param value the value to set

        @throw TYPE-OPTION-ERROR invalid option or invalid option type
    */
    AbstractDataField setOption(string opt, auto value) {
        getType().setOption(opt, value);
        return self;
    }

    #! sets the given options on the field's type
    /** @param options a hash of options, if any options match supported options for this type, they are set

        @throw TYPE-OPTION-ERROR option value has an invalid type
    */
    AbstractDataField setOptions(hash<auto> options) {
        getType().setOptions(options);
        return self;
    }

    #! returns the type name
    string getTypeName() {
        return getType().getName();
    }

    #! Sets allowed values for enum types
    AbstractDataField setAllowedValues(list<auto> allowed_values, *bool allowed_values_creatable) {
        AbstractDataProviderType type = getType();
        if (allowed_values[0] instanceof hash<AllowedValueInfo>) {
            self.allowed_values = allowed_values;
        } else if (allowed_values[0].typeCode() == NT_HASH && allowed_values[0].hasKey("value")
            && type.getBaseTypeCode() != NT_HASH) {
            self.allowed_values = map cast<hash<AllowedValueInfo>>($1), allowed_values;
        } else {
            self.allowed_values = map <AllowedValueInfo>{
                "display_name": $1.toString(),
                "value": $1,
            }, allowed_values;
        }
        on_error rethrow $1.err, sprintf("%s (allowed_values: %y)", $1.desc, allowed_values);
        # check that the value can be assigned to the type
        map self.allowed_values[$#].value = type.acceptsValue($1.value), self.allowed_values.iterator();
        if (allowed_values_creatable) {
            self.allowed_values_creatable = True;
        } else {
            # NOTE: requires that values can be converted to a string hash key to simulate a set
            allowed_values_map = map {$1.value: True}, self.allowed_values;
        }
        return self;
    }

    #! Sets allowed values for enum types
    AbstractDataField setElementAllowedValues(list<auto> element_allowed_values,
            *bool element_allowed_values_creatable) {
        AbstractDataProviderType type = getType();
        if (element_allowed_values[0] instanceof hash<AllowedValueInfo>) {
            self.element_allowed_values = element_allowed_values;
        } else if (element_allowed_values[0].typeCode() == NT_HASH && element_allowed_values[0].hasKey("value")
            && type.getBaseTypeCode() != NT_HASH) {
            self.element_allowed_values = map cast<hash<AllowedValueInfo>>($1), element_allowed_values;
        } else {
            self.element_allowed_values = map <AllowedValueInfo>{
                "display_name": $1.toString(),
                "value": $1,
            }, element_allowed_values;
        }
        on_error rethrow $1.err, sprintf("%s (element_allowed_values: %y)", $1.desc, element_allowed_values);
        if (*AbstractDataProviderType et = type.getElementType()) {
            # check that the value can be assigned to the type
            map self.element_allowed_values[$#].value = et.acceptsValue($1.value),
                self.element_allowed_values.iterator();
        }
        if (element_allowed_values_creatable) {
            self.element_allowed_values_creatable = True;
        } else {
            # NOTE: requires that values can be converted to a string hash key to simulate a set
            element_allowed_values_map = map {$1.value: True}, self.element_allowed_values;
        }
        return self;
    }

    #! sets the default value for the field
    AbstractDataField setDefaultValue(auto default_value) {
        # check that the value can be assigned to the type
        self.default_value = getType().acceptsValue(default_value);
        return self;
    }

    #! get default value, if any
    auto getDefaultValue() {
        return default_value ?? getType().getDefaultValue();
    }

    #! Get allowed values
    *list<hash<AllowedValueInfo>> getAllowedValues() {
        return allowed_values;
    }

    #! Get allowed values
    *hash<string, bool> getAllowedValueMap() {
        return allowed_values_map;
    }

    bool getAllowedValuesCreatable() {
        return allowed_values_creatable ?? False;
    }

    #! Get element allowed values
    *list<hash<AllowedValueInfo>> getElementAllowedValues() {
        return element_allowed_values;
    }

    #! Get allowed values
    *hash<string, bool> getElementAllowedValueMap() {
        return element_allowed_values_map;
    }

    bool getElementAllowedValuesCreatable() {
        return element_allowed_values_creatable ?? False;
    }

    #! returns True if the field's type is not a wildcard type
    bool hasType() {
        return getType().hasType();
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type
    */
    auto acceptsValue(auto value) {
        # do not reference default_value directly here in case it's provided with code
        if (!exists value && exists (auto dv = getDefaultValue())) {
            return dv;
        }
        try {
            # first do any value / type filtering / conversions
            value = getType().acceptsValue(value);
            if (allowed_values && !allowed_values_creatable) {
                if (!allowed_values_map{value}) {
                    throw "FIELD-VALUE-ERROR", sprintf("field %y does not accept value %y; allowed values: %y",
                        getName(), value, (map $1.value, allowed_values));
                }
            }
            if (element_allowed_values) {
                foreach auto v in (value) {
                    if (!element_allowed_values_map{v}) {
                        throw "FIELD-VALUE-ERROR", sprintf("field %y does not accept value %y; allowed "
                            "values for elements: %y", getName(), v, (map $1.value, element_allowed_values));
                    }
                }
            }
            return value;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, sprintf("%s (while setting field %y)", ex.desc, getName()), ex.arg;
        }
    }

    #! returns information about the field as an input field
    /** the \c default_value and \example_value keys are not returned, and the \c type key returns only input
        information
    */
    hash<DataFieldInfo> getInputInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return <DataFieldInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDescription(),
            "type": getType().getInfo(),
            "unique_key": self.uniqueHash(),
            "attr": attr,
        };
    }

    #! returns information about the field
    hash<DataFieldInfo> getInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return <DataFieldInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDescription(),
            "default_value": getDefaultValue(),
            "example_value": getExampleValue(),
            "multiselect": multiselect ?? False,
            "allowed_values": allowed_values,
            "allowed_values_creatable": allowed_values_creatable ?? False,
            "element_allowed_values": element_allowed_values,
            "element_allowed_values_creatable": element_allowed_values_creatable ?? False,
            "type": getType().getInfo(),
            "unique_key": self.uniqueHash(),
            "attr": attr,
        };
    }

    #! returns information about the field
    hash<DataFieldInfo> getConfigInfo() {
        on_error rethrow $1.err, sprintf("%s (while reading field %s %s)", $1.desc, getType().getName(), getName());
        return <DataFieldInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDescription(),
            "default_value": getDefaultValue(),
            "multiselect": multiselect ?? False,
            "allowed_values": allowed_values,
            "allowed_values_creatable": allowed_values_creatable ?? False,
            "element_allowed_values": element_allowed_values,
            "element_allowed_values_creatable": element_allowed_values_creatable ?? False,
            "type": getType().getInfo(){"name", "path", "base_type", "options", "default_value", "mandatory", "tags",
                "default_field_type_info"},
            "unique_key": self.uniqueHash(),
            "attr": attr,
        };
    }

    #! returns a field with a "soft" type equivalent to the current type
    /** The base class method returns the same field; this method must be overridden in child classes to return a
        field with a usable "soft" type

        @return a field with a "soft" type equivalent to the current type
    */
    AbstractDataField getSoftType() {
        return self;
    }

    #! returns a field with an "or nothing" type equivalent to the current type
    /** The base class method returns the same field; this method must be overridden in child classes to return a
        field with a usable "or nothing" type

        @return a field with an "or nothing" type equivalent to the current type
    */
    AbstractDataField getOrNothingType() {
        return self;
    }

    #! Returns a mandatory (i.e. not "or nothing") field equivalent to the current field
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory field

        @return a mandatory (i.e. not "or nothing") field equivalent to the current field

        @since DataProvider 3.0
    */
    AbstractDataField getMandatoryType() {
        return self;
    }

    #! Returns the display name of the field
    string getDisplayName() {
        return AbstractDataField::getDisplayName(getName());
    }

    #! Returns the short description, if any
    *string getShortDescription() {
        return AbstractDataField::getShortDescription(getDescription());
    }

    #! Returns custom attributes set on the field
    *hash<auto> getAttributes() {
        return attr;
    }

    #! Sets custom attributes
    AbstractDataField setAttributes(hash<auto> attr) {
        self.attr += attr;
        return self;
    }

    #! Replaces all custom attributes
    replaceAttributes(*hash<auto> attr) {
        remove self.attr;
        if (attr) {
            self.attr += attr;
        }
    }

    #! Returns any example value for the field, otherwise return example data from the type
    /** @param emap a hash to ensure that examples are only produced once
        @param fname any field context for the value

        @return if a default value exists, it is returned first, then the example value for the type, if any

        @since DataProvider 3.0
    */
    auto getExampleValue(*hash<string, bool> emap, *string fname) {
        return getDefaultValue() ?? getType().getExampleValue(emap, getDisplayName());
    }

    #! Converts a long description into a short description
    static *string getShortDescription(*string desc, *int maxlen) {
        if (!exists desc) {
            return;
        }
        # convert backquotes to single quotes
        desc =~ s/`/'/g;

        # get single-quoted strings
        *list<string> sq = (desc =~ x/'([^\'\n]*)'/g);
        # replace them with markers
        if (sq) {
            map desc = replace(desc, $1, "{{" + $# + "}}"), sq;
        }
        # get double-quoted strings
        *list<string> dq = (desc =~ x/"([^\"\n]*)"/g);
        # replace them with markers
        if (dq) {
            map desc = replace(desc, $1, "{{" + $# + "}}"), dq;
        }

        # end after the first sentence or newline, if any
        desc =~ s/[\.;\n].*//s;
        trim desc;

        # re-add quotes to what's left
        map desc = replace(desc, "{{" + $# + "}}", $1), sq;
        map desc = replace(desc, "{{" + $# + "}}", $1), dq;

        if (maxlen > 0) {
            splice desc, maxlen;
        }

        return desc;
    }

    #! Converts a technical name into a display name
    static string getDisplayName(string name) {
        # convert dashes and underscores to spaces
        name =~ s/[-_]/ /g;
        # word break (space) before
        bool wb;
        string display_name;
        for (int i = 0, int e = name.length(); i < e; ++i) {
            string c = name[i];
            if (!i || wb) {
                display_name += c.upr();
                wb = False;
                continue;
            }
            if (c == "." || c == " ") {
                display_name += " ";
                wb = True;
                continue;
            }
            if (wb) {
                wb = False;
            }
            if (c =~ /\p{Lu}/ && (display_name =~ /\p{Ll}$/) && i > 1) {
                display_name += " ";
            } else if (c =~ /[a-z]/ && display_name =~ /(^| )[A-Z]{2,}[a-z]$/ && i > 1) {
                splice display_name, -2, 0, " ";
            }
            display_name += c;
        }
        return display_name;
    }

    #! Converts a display name into a technical name
    static string getNameFromDisplayName(string display_name) {
        string name = display_name;
        # convert the name to all lower case
        name =~ tr/A-Z/a-z/;
        # convert spaces to dashes
        name =~ s/\s+/-/g;
        # remove all punctuation and special characters
        name =~ s/[!@#\$%\^&*\(\)_=\+\{\}\[\]|\\;':",\.\/<>\?`~]//g;
        return name;
    }

    #! returns the name of the field
    abstract string getName();

    #! returns the description, if any
    abstract *string getDescription();

    #! returns the type of the field
    abstract AbstractDataProviderType getType();
}
}
