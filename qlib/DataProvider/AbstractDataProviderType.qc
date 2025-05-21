# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderType class definition

/** AbstractDataProviderType.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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
#! maps type codes to type names
public const TypeCodeMap = {
    NT_INT: "int",
    NT_STRING: "string",
    NT_BOOLEAN: "bool",
    NT_FLOAT: "float",
    NT_NUMBER: "number",
    NT_BINARY: "binary",
    NT_LIST: "list",
    NT_HASH: "hash",
    NT_OBJECT: "object",
    NT_ALL: "any",
    NT_DATE: "date",
    NT_NULL: "null",
    NT_NOTHING: "nothing",
};

#! maps Qore type name constant values from the @ref Qore::Type "Type" namespace to optimal Qore types names
/** additionally, \c "all" is mapped to \c "any"
*/
public const OptimalQoreDataTypeMap = {
    "int": "int",
    Type::Int: "int",
    Type::String: "string",
    "boolean": "bool",
    Type::Boolean: "bool",
    "double": "float",
    Type::Float: "float",
    Type::Number: "number",
    Type::Binary: "binary",
    Type::List: "list<auto>",
    Type::Hash: "hash<auto>",
    Type::Object: "object",
    Type::Date: "date",
    Type::NullType: "null",
    Type::NothingType: "nothing",
    "all": "any",
};

#! maps Qore type name constant values from the @ref Qore::Type "Type" namespace to optimal Qore types names
/** additionally, \c "all" is mapped to \c "any"
*/
public const OptimalQoreSoftDataTypeMap = OptimalQoreDataTypeMap + {
    "int": "softint",
    Type::Int: "softint",
    Type::String: "softstring",
    "boolean": "softbool",
    Type::Boolean: "softbool",
    "double": "softfloat",
    Type::Float: "softfloat",
    Type::Number: "softnumber",
    Type::List: "softlist<auto>",
    Type::Date: "softdate",
};

#! Maps Qore type name constant values to data type objects
public const DataTypeMap = {
    "int": IntType,
    Type::Int: IntType,
    Type::String: StringType,
    "boolean": BoolType,
    Type::Boolean: BoolType,
    "double": FloatType,
    Type::Float: FloatType,
    Type::Number: NumberType,
    Type::Binary: BinaryType,
    Type::List: AutoListType,
    Type::Hash: AutoHashType,
    Type::Object: ObjectType,
    Type::Date: DateType,
    Type::NullType: Reflection::NullType,
    Type::NothingType: AbstractDataProviderType::nothingType,
    "base64binary": Base64BinaryType,
    "base64urlbinary": Base64UrlBinaryType,
    "hexbinary": HexBinaryType,
    "data": DataType,

    "softint": SoftIntType,
    "softstring": SoftStringType,
    "softbool": SoftBoolType,
    "softfloat": SoftFloatType,
    "softnumber": SoftNumberType,
    "softdate": SoftDateType,
    "softlist": SoftListType,

    "*softint": SoftIntOrNothingType,
    "*softstring": SoftStringOrNothingType,
    "*softbool": SoftBoolOrNothingType,
    "*softfloat": SoftFloatOrNothingType,
    "*softnumber": SoftNumberOrNothingType,
    "*softdate": SoftDateOrNothingType,
    "*softlist": SoftListOrNothingType,

    "all": AbstractDataProviderType::anyType,
    "any": AbstractDataProviderType::anyType,
    "auto": AbstractDataProviderType::anyType,

    "*int": IntOrNothingType,
    "*integer": IntOrNothingType,
    "*string": StringOrNothingType,
    "*boolean": BoolOrNothingType,
    "*bool": BoolOrNothingType,
    "*double": FloatOrNothingType,
    "*float": FloatOrNothingType,
    "*number": NumberOrNothingType,
    "*binary": BinaryOrNothingType,
    "*list": AutoListOrNothingType,
    "*hash": AutoHashOrNothingType,
    "*object": ObjectOrNothingType,
    "*date": DateOrNothingType,
    "*base64binary": Base64BinaryOrNothingType,
    "*base64urlbinary": Base64UrlBinaryOrNothingType,
    "*hexbinary": HexBinaryOrNothingType,
    "*data": DataOrNothingType,
};

#! Maps Qore type name constant values to AbstractDataProviderType values
public const AbstractDataProviderTypeMap = (map {$1.key: AbstractDataProviderType::get($1.value)},
    DataTypeMap.pairIterator()) + {
    "byte": new QoreSignedByteDataType(),
    "*byte": new QoreSignedByteOrNothingDataType(),
    "softbyte": new QoreSoftSignedByteDataType(),
    "*softbyte": new QoreSoftSignedByteOrNothingDataType(),

    "file": FileDataType,
    "*file": FileOrNothingDataType,

    "unixts": UnixTimestampType,
    "*unixts": UnixTimestampOrNothingType,

    "unixtsms": UnixTimestampMsType,
    "*unixtsms": UnixTimestampMsOrNothingType,

    "epoch": EpochIntToDateType,
    "*epoch": EpochIntToDateOrNothingType,

    "supersoftstring": SuperSoftStringType,
    "*supersoftstring": SuperSoftStringOrNothingType,

    "ubyte": QoreUnsignedByteDataType,
    "*ubyte": new QoreUnsignedByteOrNothingDataType(),
    "softubyte": new QoreSoftUnsignedByteDataType(),
    "*softubyte": new QoreSoftUnsignedByteOrNothingDataType(),

    "rgbcolor": RgbColorDataType,
    "*rgbcolor": RgbColorOrNothingDataType,
};

#! describes type options
public hashdecl DataProviderTypeOptionInfo {
    #! the option value type
    string type;

    #! the description of the option
    string desc;

    #! Flag to publish the option as a `type_option`
    bool publish;
}

#! describes a data type
public hashdecl DataTypeInfo {
    #! The name of the type
    string name;

    #! The display name for the type
    string display_name;

    #! A short description for the type, plain text formatting
    string short_desc;

    #! The description of the type
    string desc;

    #! The type path in the type cache, if known
    *string path;

    #! Output: transformation options supported by the type
    *hash<string, hash<DataProviderTypeOptionInfo>> supported_options;

    #! Output: current transformation option values
    *hash<auto> options;

    #! Output: base type
    string base_type;

    #! Output: can be null / missing?
    bool mandatory;

    #! Output: list of types accepted
    list<string> types_accepted;

    #! Input: list of types returned
    list<string> types_returned;

    #! Any fields supported by the type; will be NOTHING when \a recursive is @ref True
    *hash<string, hash<DataFieldInfo>> fields;

    #! A type description of any element type
    *hash<DataTypeInfo> element_type;

    #! The default value for the type
    auto default_value;

    #! If fields can be added dynamically to the type and if the type will accept any field at runtime
    bool can_manage_fields = False;

    #! Default type for fields not listed in \a fields
    *hash<DataTypeInfo> default_field_type_info;

    #! Unique type identifier
    string unique_key;

    #! Recursive flag
    bool recursive;

    #! Any tags set on the type
    *hash<auto> tags;
}

/** @defgroup data_type_tags Data Type Tag Constants
    These constants provide predefined data type tags
*/
#/@{
#! Tag indicating that a string value can be taken from a file
public const DTT_FromFile = "from_file";

#! Tag indicates that a string value can indicate a location
public const DTT_FromLocation = "from_location";

#! Tag indicates that the value is only applicable in client contexts
public const DTT_ClientOnly = "client_only";
#/@}

#! describes a data type
public class AbstractDataProviderType inherits Qore::Serializable {
    public {
        static Type nothingType = Reflection::NothingType;
        static Type anyType = Reflection::AutoType;
        # not initialized here to workaround issue #4048 (circular initializtion error)
        static AbstractDataProviderType anyDataType;

        #! Default description
        const DefaultDesc = "no description available";
    }

    private {
        #! type options
        hash<auto> options;

        #! type tags
        *hash<auto> tags;

        #! The type path, if any
        *string path;
    }

    #! creates the type
    constructor() {
    }

    #! creates the type
    constructor(string path) {
        self.path = path;
    }

    #! creates the type and sets options
    /** @param options a hash of options, if any options match supported options for this type, they are set
        @param tags any user-defined tags to set on the type
        @param path the registered type path, if any

        @throw TYPE-OPTION-ERROR option value has an invalid type
    */
    constructor(hash<auto> options, *hash<auto> tags, *string path) {
        setOptions(options);
        if (tags) {
            self.tags = tags;
        }
        if (path.val()) {
            self.path = path;
        }
    }

    #! creates the type and sets options
    /** @param options a hash of options, if any options match supported options for this type, they are set
        @param tags any user-defined tags to set on the type
        @param path the registered type path, if any

        @throw TYPE-OPTION-ERROR option value has an invalid type
    */
    constructor(*hash<auto> options, hash<auto> tags, *string path) {
        if (options) {
            setOptions(options);
        }
        if (tags) {
            self.tags = tags;
        }
        if (path.val()) {
            self.path = path;
        }
    }

    #! Returns the description
    string getDesc() {
        return DefaultDesc;
    }

    #! Returns the type path in the type cache, if any
    /** @since DataProvider 3.0
    */
    *string getPath() {
        return path;
    }

    #! Sets the type path for the type
    /** @since DataProvider 3.0
    */
    setPath(string path) {
        self.path = path;
    }

    #! Clears the type path for the type
    /** @since DataProvider 3.0
    */
    clearPath() {
        remove path;
    }

    #! Returns supported options
    *hash<string, hash<DataProviderTypeOptionInfo>> getSupportedOptions() {
        # this method intentionally left blank
    }

    #! Returns a description of the type as an input type
    /** @return a description of the type as an input type; only the following keys are returned
        - \c name
        - \c types_returned
        - \c fields
        - \c base_type
    */
    hash<DataTypeInfo> getInputInfo(*bool simple) {
        # to catch recursive fields
        string uh = self.uniqueHash();
        bool recursive = rimap{uh} ?? False;
        if (!recursive) {
            rimap{uh} = True;
        }
        on_exit if (!recursive) {
            remove rimap{uh};
        }
        return <DataTypeInfo>{
            "name": getName(),
            "types_returned": keys getReturnTypeHash(simple),
            "fields": !recursive ? getFieldInfo() : NOTHING,
            "recursive": recursive,
            "unique_key": uh,
            "base_type": getBaseTypeName(),
        };
    }

    #! Returns a description of the type as a hash
    hash<DataTypeInfo> getInfo(*bool simple) {
        # to catch recursive fields
        string uh = self.uniqueHash();
        bool recursive = rimap{uh} ?? False;
        if (!recursive) {
            rimap{uh} = True;
        }
        on_exit if (!recursive) {
            remove rimap{uh};
        }
        on_error rethrow $1.err, sprintf("%s (while reading type %s %s recursive: %y rimap: %y)", $1.desc, getName(),
            uh, recursive, rimap.size());
        hash<DataTypeInfo> rv = <DataTypeInfo>{
            "name": getName(),
            "display_name": getDisplayName(),
            "short_desc": getShortDescription(),
            "desc": getDesc(),
            "supported_options": getSupportedOptions(),
            "options": options
                ? (map {
                    $1.key: ($1.value.typeCode() == NT_OBJECT || $1.value.callp())
                        ? "<" + $1.value.fullType() + ">"
                        : $1.value,
                }, options.pairIterator())
                : NOTHING,
            "base_type": getBaseTypeName(),
            "mandatory": getDirectTypeHash(){NT_NOTHING} ? False : True,
            "types_accepted": keys getAcceptTypeHash(simple),
            "types_returned": keys getReturnTypeHash(simple),
            "fields": !recursive ? getFieldInfo() : NOTHING,
            "recursive": recursive,
            "unique_key": uh,
            "tags": tags,
        };
        if (*AbstractDataProviderType etype = getElementType()) {
            rv.element_type = etype.getInfo(simple);
        }
        if (exists (auto v = getDefaultValue())) {
            rv.default_value = v;
        }
        if (path.val()) {
            rv.path = path;
        }
        return rv;
    }

    #! Returns information on fields supported
    *hash<string, hash<DataFieldInfo>> getFieldInfo() {
        return cast<hash<string, hash<DataFieldInfo>>>(map {$1.key: $1.value.getInfo()}, getFields().pairIterator());
    }

    #! Returns True if this type can be assigned from values of the argument type
    bool isAssignableFrom(AbstractDataProviderType t) {
        *Type othertype = t.getValueType();
        if (!othertype) {
            return False;
        }
        return isAssignableFrom(othertype);
    }

    #! Returns True if this type can be assigned from values of the argument type
    bool isAssignableFrom(Type t) {
        *Type thistype = getValueType();
        if (!thistype) {
            return False;
        }
        return thistype.isAssignableFrom(t);
    }

    #/! Returns @ref True if this type has a default value
    /** @par Example:
        @code{.py}
    bool b = t.hasDefaultValue();
        @endcode

        @return @ref True if this type has a default value

        @note The base class method always returns @ref False

        @since DataProvider 3.0
    */
    bool hasDefaultValue() {
        return False;
    }

    #//! Returns the default value for the type or @ref nothing if the type has no default value
    /** @par Example:
        @code{.py}
    auto v = t.getDefaultValue();
        @endcode

        @return the default value for the type or @ref nothing if the type has no default value

        @note The base class method always returns @ref NOTHING

        @since DataProvider 3.0
    */
    auto getDefaultValue() {
    }

    #! Returns True if this type is a list
    bool isList() {
        *Type thistype = getValueType();
        return thistype ? AutoListOrNothingType.isOutputCompatible(thistype) : False;
    }

    #! Returns True if the type must have a value
    bool isMandatory() {
        *Type thistype = getValueType();
        return thistype ? !thistype.isOrNothingType() : True;
    }

    #! Returns the given field, if present, or @ref nothing if not
    *AbstractDataField getField(string field_name) {
        return getFields().field_name;
    }

    #! Returns True if the type is not a wildcard type
    bool hasType() {
        *Type thistype = getValueType();
        return !thistype || thistype.hasType();
    }

    #! Returns the base type name for the type; must be a standard Qore base type name
    string getBaseTypeName() {
        *Type thistype = getValueType();
        if (thistype) {
            string name = thistype.getName();
            name =~ s/\*?(soft)?//;
            # remove any complex type information
            name =~ s/<.*>$//;
            return name;
        }
        return "all";
    }

    #! Returns the base type code for the type
    int getBaseTypeCode() {
        *Type thistype = getValueType();
        return thistype
            ? thistype.getBaseTypeCode()
            : NT_ALL;
    }

    #! Returns a hash of native base type code keys where no translations are performed; keys are type codes, not names
    *hash<string, bool> getDirectTypeHash() {
        *Type thistype = getValueType();
        if (thistype) {
            return {
                thistype.getBaseTypeCode(): True,
            };
        }
    }

    #! Returns True if the type also accepts @ref nothing
    bool isOrNothingType() {
        *Type thistype = getValueType();
        return thistype
            ? thistype.isOrNothingType()
            : False;
    }

    #! get the given field type if it exists, otherwise return @ref nothing
    *AbstractDataProviderType getFieldType(string field_name) {
        *AbstractDataField field = getField(field_name);
        if (field) {
            return field.getType();
        }
    }

    #! Returns the value of the given option
    auto getOptionValue(string opt) {
        return options{opt};
    }

    #! Returns options set on the type
    *hash<auto> getOptions() {
        return options;
    }

    #! Returns options with the publish flag set on the type
    /** @since DataProvider 3.0
    */
    *hash<auto> getPublicOptions() {
        return map {$1.key: $1.value}, options.pairIterator(), $1.value.publish;
    }

    #! sets the given option on the type
    /** @param opt the option to set
        @param value the value to set

        @throw TYPE-OPTION-ERROR invalid option or invalid option type
    */
    setOption(string opt, auto value) {
        if (!(*hash<string, hash<DataProviderTypeOptionInfo>> options = getSupportedOptions()){opt}) {
            throw "TYPE-OPTION-ERROR", sprintf("option %y is not supported by type %y; supported options: %y",
                opt, getName(), keys options);
        }
        string value_type = value.type();
        if (value_type == "object") {
            value_type = value.fullType();
        }
        if (options{opt}.type != value_type) {
            throw "TYPE-OPTION-ERROR", sprintf("type %y expects type %y for option %y; got type %y instead "
                "(value: %y)", getName(), options{opt}.type, opt, value.type(), value);
        }

        setOptionIntern(opt, value);
    }

    #! sets options on the type
    /** @param options the options to set; unsupported options are ignored

        @throw TYPE-OPTION-ERROR invalid option type
    */
    setOptions(hash<auto> options) {
        foreach hash<auto> i in (getSupportedOptions().pairIterator()) {
            if (exists (auto value = options{i.key})) {
                string value_type = value.type();
                if (value_type == "object") {
                    value_type = value.fullType();
                }
                if (i.value.type == "list" && value_type != "list") {
                    value = (value,);
                } else if (i.value.type != value_type) {
                    throw "TYPE-OPTION-ERROR", sprintf("type %y expects type %y for option %y; got type %y instead "
                        "(value: %y)", getName(), i.value.type, i.key, value.type());
                }
                setOptionIntern(i.key, value);
            }
        }
    }

    #! Returns the value of the given tag
    auto getTag(string tag) {
        return tags{tag};
    }

    #! Returns tags set on the type
    *hash<auto> getTags() {
        return tags;
    }

    #! Sets the dynamic tag on the type
    setDynamicTag() {
        tags.dynamic = True;
    }

    #! Returns a "soft" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "soft" type

        @return a "soft" type equivalent to the current type
    */
    AbstractDataProviderType getSoftType() {
        return self;
    }

    #! Returns an "or nothing" type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable "or nothing" type

        @return an "or nothing" type equivalent to the current type

        @note recursively updates fields to "or nothing" types as well
    */
    AbstractDataProviderType getOrNothingType() {
        return self;
    }

    #! Returns a mandatory (i.e. not "or nothing") type equivalent to the current type
    /** The base class method returns the same type; this method must be overridden in child classes to return a
        usable mandatory type

        @return a mandatory (i.e. not "or nothing") type equivalent to the current type

        @since DataProvider 3.0
    */
    AbstractDataProviderType getMandatoryType() {
        return self;
    }

    #! Returns any example value for type, if any
    /** @param emap a hash to ensure that examples are only produced once
        @param fname any field context for the value

        @since DataProvider 3.0
    */
    auto getExampleValue(*hash<string, bool> emap, *string fname) {
        string u = self.uniqueHash();
        if (emap{u}) {
            return;
        }
        emap{u} = True;

        if (*hash<string, AbstractDataField> fields = getFields()) {
            # do not return example values for complex "or nothing" types
            if (isOrNothingType()) {
                return;
            }
            return map {$1.key: $1.value.getExampleValue(emap, $1.key)}, fields.pairIterator();
        }
        int t = getBaseTypeCode();
        switch (getBaseTypeCode()) {
            case NT_INT: return 100;
            case NT_STRING: return fname.val() ? sprintf("Example %s", fname) : "example string";
            case NT_BOOLEAN: return True;
            case NT_FLOAT: return 3.1415927;
            case NT_NUMBER: return 3.1415927n;
            case NT_DATE: return now_us();
            case NT_BINARY: return <abcd>;
            case NT_LIST: {
                # do not return example values for complex "or nothing" types
                if (isOrNothingType()) {
                    return;
                }
                if (*AbstractDataProviderType etype = getElementType()) {
                    return map etype.getExampleValue(emap, fname), xrange(1);
                }
                return (1, 2);
            }
            case NT_HASH: {
                # do not return example values for complex "or nothing" types
                if (isOrNothingType()) {
                    return;
                }
                *AbstractDataProviderType etype = getElementType();
                if (!etype && self.hasCallableMethod("getDefaultOtherElementType")) {
                    etype = cast<object>(self).getDefaultOtherElementType();
                }
                if (etype) {
                    return map {$1: etype.getExampleValue(emap, fname ? fname + "." + $1 : $1)}, ("a", "b");
                }
                return {"a": 1, "b": "two"};
            }
            case NT_ALL: return "any value";
            case NT_NOTHING: return;
        }
        throw "DATA-ERROR", sprintf("do not have example data for type %y (base type %y)", getName(),
            getBaseTypeName());
    }

    #! Returns the type's display name
    /** @return the type's display name

        @since DataProvider 3.0
    */
    string getDisplayName() {
        return path ?? AbstractDataField::getDisplayName(getName());
    }

    #! Returns the type's short description
    /** @return the plain-text short description for the type

        @since DataProvider 3.0
    */
    string getShortDescription() {
        return AbstractDataField::getShortDescription(getDesc());
    }

    #! Returns the type name
    abstract string getName();

    #! Returns the base type for the type, if any
    abstract *Type getValueType();

    #! Returns the subtype (for lists or hashes) if there is only one
    abstract *AbstractDataProviderType getElementType();

    #! Returns the fields of the data structure; if any
    abstract *hash<string, AbstractDataField> getFields();

    #! Returns a hash of types accepted by this type; keys are type names
    abstract hash<string, bool> getAcceptTypeHash(*bool simple);

    #! Returns a hash of types returned by this type; keys are type names
    abstract hash<string, bool> getReturnTypeHash(*bool simple);

    #! Returns the value if the value can be assigned to the type
    /** @param value the value to assign to the type

        @return the value to be assigned; can be converted by the type
    */
    abstract auto acceptsValue(auto value);

    #! Returns an appropriate object for the given type
    static AbstractDataProviderType get(Type type, *hash<auto> options, *hash<auto> tags,
            auto default_value, *hash<NameDescInfo> attr) {
        if (type.isTypedHash()) {
            return new HashDeclDataType(type, options, tags, default_value, attr);
        } else if (type.getBaseTypeCode() == NT_HASH) {
            return new QoreHashDataType(type, options, tags, default_value, attr);
        } else if (type.getBaseTypeCode() == NT_STRING && !type.isOutputCompatible(BinaryType)) {
            return new QoreStringDataTypeBase(type, options, tags, default_value, attr);
        }
        return new QoreDataType(type, options, tags, default_value, attr);
    }

    #! Returns an appropriate object for the given type
    /** @param typename the name of the type; should be a valid Qore type string
        @param options type options for the data type object
    */
    static AbstractDataProviderType get(string typename, *hash<auto> options) {
        return AbstractDataProviderType::get(DataTypeMap{typename} ?? new Type(typename), options);
    }

    #! Returns an appropriate object for the given type description
    /** @param info the description of the type
    */
    static AbstractDataProviderType get(hash<DataTypeInfo> info) {
        if (info.path) {
            #return DataProvider::getTypeCache().lookupTypeFromPathEx(info.path);
            # FIXME: the call above causes a recursive constant reference error
            return Class::forName("DataProvider").getStaticMethod("getTypeCache").call()
                .lookupTypeFromPathEx(info.path);
        }
        if (*Type t = DataTypeMap{info.name} ?? DataTypeMap{info.base_type}) {
            hash<auto> attr = info{"display_name", "short_desc", "desc"};
            AbstractDataProviderType rv = AbstractDataProviderType::get(t, info.options, info.tags,
                info.default_value, attr ? cast<hash<NameDescInfo>>(attr) : NOTHING);
            if (!info.mandatory && !rv.isOrNothingType()) {
                return rv.getOrNothingType();
            }
            if (info.default_field_type_info && (rv instanceof HashDataType)) {
                HashDataType hdt = cast<HashDataType>(rv);
                if (!hdt.getDefaultOtherFieldType()) {
                    hdt.setDefaultOtherFieldType(AbstractDataProviderType::get(info.default_field_type_info));
                }
            }
            return rv;
        }

        throw "TYPE-ERROR", sprintf("No match for type: %y", info);
    }

    #! sets the given option without any validation of the option
    private setOptionIntern(string opt, auto value) {
        options{opt} = value;
    }
}
}

namespace Priv {
#! Recursive info map: unique key -> True
thread_local hash<string, bool> rimap;
}