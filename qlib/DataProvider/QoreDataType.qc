# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreDataType class definition

/** QoreDataType.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! Name and description info
public hashdecl NameDescInfo {
    #! The name of the type
    string name;

    #! The display name for the type
    string display_name;

    #! The short plain-text description for the type
    string short_desc;

    #! The markdown description for the type
    string desc;
}

#! describes a data type based on a %Qore data type
/** @note %Qore "soft" types that do not accept @ref nothing also accept @ref null; to ensure that soft types
    inheriting this class do not accept @ref null, use the \c qore.no_null option in the constructor
*/
public class QoreDataType inherits AbstractDataProviderType {
    private {
        #! the Qore type
        Type type;

        #! The name of the type; if not set then the name of "type" is used instead
        string name;

        #! The display name for the type
        string display_name;

        #! The short plain-text description for the type
        string short_desc;

        #! The long description for the type in markdown
        string desc;

        #! flag for soft types
        bool soft_type;

        #! flag for "or nothing" types
        bool or_nothing_type;

        #! Default value for type
        auto default_value;

        #! hash of base types where no translations are performed; keys are type codes, not names
        /** @note
            - this hash wil not contain the primary type code for the type if such values might undergo translation in
              acceptsValue()
            - keys are %Qore type codes converted to integers, not type names
        */
        hash<string, bool> base_type_hash;

        #! hash of base types accepted by this type; keys are type names
        *hash<string, bool> accept_type_hash;

        #! hash of base types returned by this type; keys are type names
        *hash<string, bool> return_type_hash;

        #! hash of base types accepted by this type; keys are simple type names
        *hash<string, bool> accept_type_hash_simple;

        #! hash of base types returned by this type; keys are simple type names
        *hash<string, bool> return_type_hash_simple;

        #! hash of type codes accepted by the base type but not accepted by this type: code -> True
        hash<string, bool> blacklist_type_hash;

        #! supported options
        const SupportedOptions = {
            "qore.no_null": <DataProviderTypeOptionInfo>{
                "type": Type::Boolean,
                "desc": "if True then NULL is not supported on input if NOTHING is also not accepted",
            },
            "qore.external_name": <DataProviderTypeOptionInfo>{
                "type": Type::String,
                "desc": "A note giving the external name for the type",
            },
        };

        #! map from normal types to soft types
        const SoftTypeMap = {
            "int": SoftIntType,
            "bool": SoftBoolType,
            "float": SoftFloatType,
            "number": SoftNumberType,
            "string": SoftStringType,
            "date": SoftDateType,
            "list": SoftAutoListType,
            "*int": SoftIntOrNothingType,
            "*bool": SoftBoolOrNothingType,
            "*float": SoftFloatOrNothingType,
            "*number": SoftNumberOrNothingType,
            "*string": SoftStringOrNothingType,
            "*date": SoftDateOrNothingType,
            "*list": SoftAutoListOrNothingType,
        };
    }

    #! creates the type
    constructor(Type type, *hash<auto> options, *hash<auto> tags, auto default_value, *hash<NameDescInfo> attr,
            *string path) : AbstractDataProviderType(path) {
        self.type = type;
        base_type_hash{type.getBaseTypeCode()} = True;
        if (type.isOrNothingType()) {
            base_type_hash{NT_NOTHING} = True;
        }
        accept_type_hash = type.getAcceptTypeHash();
        return_type_hash = type.getReturnTypeHash();
        accept_type_hash_simple = type.getAcceptTypeHash(True);
        return_type_hash_simple = type.getReturnTypeHash(True);

        soft_type = (type.getName() =~ /^.?soft/);
        or_nothing_type = type.isOrNothingType();

        # set options after setting base type info
        if (options) {
            setOptions(options);
        }
        if (tags) {
            self.tags = tags;
        }
        if (self.options."qore.no_null") {
            accept_type_hash -= "null";
            accept_type_hash_simple -= "null";
            blacklist_type_hash{NT_NULL} = True;
        }

        self.default_value = default_value;

        if (attr) {
            self += attr;
        }
    }

    #! returns the type name
    string getName() {
        return name ?? type.getName();
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return display_name ?? path ?? AbstractDataField::getDisplayName(getName());
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return short_desc ?? AbstractDataField::getShortDescription(getDesc());
    }

    #! Returns the description
    string getDesc() {
        return desc ?? sprintf("Qore data type %y", type.getName());
    }

    #! returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        return or_nothing_type ? NOTHING : SupportedOptions;
    }

    #! returns the base type for the type, if any
    *Type getValueType() {
        return type;
    }

    #! returns a hash of native base type code where no translations are performed; keys are type codes, not names
    *hash<string, bool> getDirectTypeHash() {
        return base_type_hash;
    }

    #/! Returns @ref True if this type has a default value
    /** @par Example:
        @code{.py}
    bool b = t.hasDefaultValue();
        @endcode

        @return @ref True if this type has a default value

        @since DataProvider 3.0
    */
    bool hasDefaultValue() {
        return exists default_value;
    }

    #//! Returns the default value for the type or @ref nothing if the type has no default value
    /** @par Example:
        @code{.py}
    auto v = t.getDefaultValue();
        @endcode

        @return the default value for the type or @ref nothing if the type has no default value

        @since DataProvider 3.0
    */
    auto getDefaultValue() {
        return default_value;
    }

    #! returns the subtype (for lists or hashes) if there is only one
    *AbstractDataProviderType getElementType() {
        if (*Type elementType = type.getElementType()) {
            return AbstractDataProviderType::get(elementType);
        }
    }

    #! returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type

        @throw RUNTIME-TYPE-ERROR value cannot be assigned to type
    */
    auto acceptsValue(auto value) {
        if (blacklist_type_hash && blacklist_type_hash{value.typeCode()}) {
            throw "RUNTIME-TYPE-ERROR", sprintf("type %y does not accept values of type %y", getName(),
                value.type());
        }
        if ((!exists value || value == NULL) && exists default_value) {
            return default_value;
        }
        return type.acceptsValue(value);
    }

    #! returns the fields of the data structure; if any
    *hash<string, AbstractDataField> getFields() {
        # this method intentionally left blank
    }

    #! returns a hash of base types accepted by this type; keys are type names
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        return simple ? accept_type_hash_simple : accept_type_hash;
    }

    #! returns a hash of base types returned by this type; keys are type names
    hash<string, bool> getReturnTypeHash(*bool simple) {
        return simple ? return_type_hash_simple : return_type_hash;
    }

    #! returns a description of the type as a hash
    hash<DataTypeInfo> getInfo(*bool simple) {
        hash<DataTypeInfo> rv = AbstractDataProviderType::getInfo(simple);
        if ((base_type_hash{NT_HASH} || base_type_hash{NT_ALL}) && !rv.fields) {
            rv.can_manage_fields = True;
        }
        return rv;
    }

    #! returns a "soft" type equivalent to the current type
    /** @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        if (!soft_type) {
            # check if a soft type exists
            *Type soft_type = SoftTypeMap{type.getName()};
            if (soft_type) {
                return new QoreDataType(soft_type, options, tags, default_value);
            }
        }
        return self;
    }

    #! returns an "or nothing" type equivalent to the current type
    /** @return an "or nothing" type equivalent to the current type
    */
    AbstractDataProviderType getOrNothingType() {
        if (!or_nothing_type) {
            QoreDataType rv = self.copy();
            if (rv.name && rv.name !~ /^\*/) {
                rv.name = "*" + rv.name;
            }
            if (rv.display_name && rv.display_name !~ /^\*/) {
                rv.display_name = "*" + display_name;
            }
            rv.or_nothing_type = True;
            rv.accept_type_hash_simple."nothing" = rv.accept_type_hash."nothing" = True;
            rv.return_type_hash_simple."nothing" = rv.return_type_hash."nothing" = True;
            if (!options."qore.no_null") {
                rv.accept_type_hash_simple."null" = rv.accept_type_hash."null" = True;
            }
            rv.type = type.getOrNothingType();
            return rv;
        }
        return self;
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory type

        @return a mandatory (i.e. not "or nothing") type equivalent to the current type
    */
    AbstractDataProviderType getMandatoryType() {
        if (or_nothing_type) {
            QoreDataType rv = self.copy();
            if (rv.name && rv.name =~ /^\*/) {
                rv.name = rv.name[1..];
            }
            if (rv.display_name && rv.display_name =~ /^\*/) {
                rv.display_name = rv.display_name[1..];
            }
            rv.or_nothing_type = False;
            remove rv.accept_type_hash_simple."nothing";
            remove rv.accept_type_hash."nothing";
            remove rv.accept_type_hash_simple."null";
            remove rv.accept_type_hash."null";
            remove rv.return_type_hash_simple."nothing";
            remove rv.return_type_hash."nothing";
            remove rv.return_type_hash_simple."null";
            remove rv.return_type_hash."null";
            remove rv.base_type_hash{NT_NOTHING};
            remove rv.base_type_hash{NT_NULL};
            rv.type = type.getMandatoryType();
            return rv;
        }
        return self;
    }

    # assigs the object's members after deserialization
    private:internal deserializeMembers(hash<auto> members) {
        self += members;
        # for type objects serialized before the accept_type_hash_simple attribute was added
        if (!accept_type_hash_simple) {
            accept_type_hash_simple = type.getAcceptTypeHash(True);
        }
        # for type objects serialized before the return_type_hash_simple attribute was added
        if (!return_type_hash_simple) {
            return_type_hash_simple = type.getReturnTypeHash(True);
        }
    }
}
}
