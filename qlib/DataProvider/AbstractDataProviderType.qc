# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProviderType class definition

/** AbstractDataProviderType.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
#! maps type codes to type names
public const hash<string, string> TypeCodeMap = {
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
public const hash<auto> OptimalQoreDataTypeMap = {
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
public const hash<auto> DataTypeMap = {
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

    "timeout": TimeoutType,
    "*timeout": TimeoutOrNothingType,

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

#! Maps type names to class-backed AbstractDataProviderType values not present in @ref DataTypeMap
/** These types (e.g. @c "file", @c "rgbcolor", @c "email") are registered here rather than in
    @ref DataTypeMap because they are not backed by a plain Qore reflection type. The map is kept separate
    from @ref AbstractDataProviderTypeMap so that @ref AbstractDataProviderType::get() can resolve these
    names by string without a recursive constant reference to @ref AbstractDataProviderTypeMap.

    @note The class-backed types listed here must not read @ref AbstractDataProviderTypeMap during their own
    construction (e.g. in field-type definitions): @ref AbstractDataProviderTypeMap merges in this map, so any
    such read closes a constant-initialization cycle (@ref AbstractDataProviderTypeMap -> this map -> the type ->
    @ref AbstractDataProviderTypeMap) that Qore rejects nondeterministically depending on constant commit order.
    Reference the leaf Qore reflection @ref Qore::Type "Type" constants (e.g. \c StringType) directly instead.

    @since %DataProvider 3.6
*/
public const NamedClassBackedTypeMap = {
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

    "supersoftstring": SuperSoftStringDataType,
    "*supersoftstring": SuperSoftStringOrNothingDataType,

    "ubyte": QoreUnsignedByteDataType,
    "*ubyte": new QoreUnsignedByteOrNothingDataType(),
    "softubyte": new QoreSoftUnsignedByteDataType(),
    "*softubyte": new QoreSoftUnsignedByteOrNothingDataType(),

    "rgbcolor": RgbColorDataType,
    "*rgbcolor": RgbColorOrNothingDataType,

    "email": EmailType,
    "*email": EmailOrNothingType,
    "uri": UriType,
    "*uri": UriOrNothingType,
    "uuid": UuidType,
    "*uuid": UuidOrNothingType,
    "hostname": HostnameType,
    "*hostname": HostnameOrNothingType,
    "ipv4": Ipv4Type,
    "*ipv4": Ipv4OrNothingType,
    "ipv6": Ipv6Type,
    "*ipv6": Ipv6OrNothingType,
    "phone": PhoneType,
    "*phone": PhoneOrNothingType,
};

#! Maps Qore type name constant values to AbstractDataProviderType values
public const AbstractDataProviderTypeMap = (map {$1.key: AbstractDataProviderType::get($1.value)},
    DataTypeMap.pairIterator()) + NamedClassBackedTypeMap;

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

    #! Alternative type descriptions for union types
    /** @since DataProvider 3.5.1
    */
    *list<hash<DataTypeInfo>> union_types;

    #! The default value for the type
    auto default_value;

    #! for enum types; allowed values
    *list<hash<AllowedValueInfo>> allowed_values;

    #! Does the type accept values not defined in allowed_values too?
    bool allowed_values_creatable = False;

    #! for enum types; allowed values for elements for list types
    *list<hash<AllowedValueInfo>> element_allowed_values;

    #! Do elements of the list accept values not defined in element_allowed_values too?
    bool element_allowed_values_creatable = False;

    #! If fields can be added dynamically to the type and if the type will accept any field at runtime
    bool can_manage_fields = False;

    #! Default type for fields not listed in \a fields
    *hash<DataTypeInfo> default_field_type_info;

    #! Required field groups - at least one field in each group must have a value
    /** @since DataProvider 3.1
    */
    *list<list<string>> required_groups;

    #! Unique type identifier
    string unique_key;

    #! Recursive flag
    bool recursive;

    #! Any tags set on the type
    *hash<auto> tags;

    #! Validation constraints of the type, if any
    /** @since DataProvider 3.8
    */
    *hash<DataTypeConstraintInfo> constraints;
}

#! Validation constraints of a data type
/** These are the validation keywords of JSON Schema, which describe constraints on values beyond their type.  Each
    constraint applies only to values of the kind it describes (for example, \c min_length only to strings), so a
    constraint on a type that accepts values of several kinds applies to the values it describes.  The constraints of
    a type are enforced by
    @ref AbstractDataProviderType::checkConstraints() "AbstractDataProviderType::checkConstraints()" for the types
    provided by this module, and are part of the type's description, so that a type restored from its description
    with @ref AbstractDataProviderType::get() "AbstractDataProviderType::get()" enforces them too.

    @since DataProvider 3.8
*/
public hashdecl DataTypeConstraintInfo {
    #! The smallest number accepted
    *softnumber minimum;

    #! The largest number accepted
    *softnumber maximum;

    #! Numbers must be greater than this value
    *softnumber exclusive_minimum;

    #! Numbers must be less than this value
    *softnumber exclusive_maximum;

    #! Numbers must be a multiple of this value
    *softnumber multiple_of;

    #! The minimum number of characters in a string
    *int min_length;

    #! The maximum number of characters in a string
    *int max_length;

    #! A regular expression that strings must match somewhere; it is not implicitly anchored
    *string pattern;

    #! The minimum number of elements in a list
    *int min_items;

    #! The maximum number of elements in a list
    *int max_items;

    #! Whether the elements of a list must all be different
    *bool unique_items;

    #! The minimum number of keys in a hash
    *int min_properties;

    #! The maximum number of keys in a hash
    *int max_properties;

    #! The values accepted, compared as strings
    *list<string> enum_values;

    #! For union types: whether values must match exactly one alternative instead of at least one
    *bool exclusive_union;

    #! For union types: the name of the hash key whose value selects the alternative
    *string discriminator;

    #! For union types with a discriminator: maps values of the discriminator key to alternative indexes
    *hash<string, int> discriminator_mapping;
}

/** @defgroup data_type_tags Data Type Tag Constants
    These constants provide predefined data type tags
*/
#/@{
#! Tag indicating that a string value can be taken from a file
public const string DTT_FromFile = "from_file";

#! Tag indicates that a string value can indicate a location
public const string DTT_FromLocation = "from_location";

#! Tag indicates that the value is only applicable in client contexts
public const string DTT_ClientOnly = "client_only";
#/@}

#! describes a data type
public class AbstractDataProviderType inherits Qore::Serializable {
    public {
        static Type nothingType = Reflection::NothingType;
        static Type anyType = Reflection::AutoType;
        #! The "any" data type (accepts any value); lazily constructed on first access
        /** A static class variable rather than a constant because it is an instance of a subclass
            (QoreDataType) of the enclosing class: Qore initializes static variable values lazily on first
            access (not at class-init time), so this does not form a class-initialization cycle even though
            AbstractDataProviderType's own static initializer references a subclass. */
        static AbstractDataProviderType anyDataType = new QoreDataType(AutoType);

        #! Default description
        const string DefaultDesc = "no description available";
    }

    private {
        #! type options
        hash<auto> options;

        #! type tags
        *hash<auto> tags;

        #! The type path, if any
        *string path;

        #! Validation constraints, if any
        *hash<DataTypeConstraintInfo> constraints;
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

    #! Returns the validation constraints of the type, if any
    /** @return the validation constraints of the type, if any

        @since DataProvider 3.8
    */
    *hash<DataTypeConstraintInfo> getConstraints() {
        return constraints;
    }

    #! Returns a copy of the type with the given validation constraints
    /** Types are commonly shared, so constraints are only ever set on a copy

        @param constraints the validation constraints of the copy; if empty, the copy has no constraints

        @return a copy of the type with the given validation constraints, or the type itself if neither it nor the
        copy has any constraints

        @since DataProvider 3.8
    */
    AbstractDataProviderType copyWithConstraints(*hash<DataTypeConstraintInfo> constraints) {
        if (!constraints && !self.constraints) {
            return self;
        }
        AbstractDataProviderType rv = self.copy();
        rv.constraints = constraints;
        return rv;
    }

    #! Checks the given value against the validation constraints of the type
    /** @param value the value to check; values of kinds that no constraint describes are not checked

        @throw RUNTIME-TYPE-ERROR the value violates a constraint

        @since DataProvider 3.8
    */
    checkConstraints(auto value) {
        if (!constraints) {
            return;
        }
        if (constraints.enum_values && exists value && value.typeCode() != NT_LIST && value.typeCode() != NT_HASH
                && !inlist(value.toString(), constraints.enum_values)) {
            throw "TYPE-VALUE-ERROR", sprintf("type %y does not accept value %y; accepted values: %y", getName(),
                value, constraints.enum_values);
        }
        switch (value.typeCode()) {
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                checkNumericConstraints(value);
                break;
            case NT_STRING:
                if (exists constraints.min_length && value.length() < constraints.min_length) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires at least %d character%s; got %d",
                        getName(), constraints.min_length, constraints.min_length == 1 ? "" : "s", value.length());
                }
                if (exists constraints.max_length && value.length() > constraints.max_length) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y accepts at most %d character%s; got %d",
                        getName(), constraints.max_length, constraints.max_length == 1 ? "" : "s", value.length());
                }
                if (constraints.pattern.val() && !regex(value, constraints.pattern)) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("value %y does not match the pattern %y of type %y", value,
                        constraints.pattern, getName());
                }
                break;
            case NT_LIST:
                if (exists constraints.min_items && value.size() < constraints.min_items) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires at least %d element%s; got %d", getName(),
                        constraints.min_items, constraints.min_items == 1 ? "" : "s", value.size());
                }
                if (exists constraints.max_items && value.size() > constraints.max_items) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y accepts at most %d element%s; got %d", getName(),
                        constraints.max_items, constraints.max_items == 1 ? "" : "s", value.size());
                }
                if (constraints.unique_items) {
                    hash<string, bool> seen;
                    foreach auto element in (value) {
                        string key = sprintf("%s:%y", element.type(), element);
                        if (seen{key}) {
                            throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires unique elements; %y is repeated",
                                getName(), element);
                        }
                        seen{key} = True;
                    }
                }
                break;
            case NT_HASH:
                if (exists constraints.min_properties && value.size() < constraints.min_properties) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires at least %d key%s; got %d", getName(),
                        constraints.min_properties, constraints.min_properties == 1 ? "" : "s", value.size());
                }
                if (exists constraints.max_properties && value.size() > constraints.max_properties) {
                    throw "RUNTIME-TYPE-ERROR", sprintf("type %y accepts at most %d key%s; got %d", getName(),
                        constraints.max_properties, constraints.max_properties == 1 ? "" : "s", value.size());
                }
                break;
        }
    }

    #! Checks a numeric value against the numeric constraints of the type
    private checkNumericConstraints(auto value) {
        # compare decimal values: a float converted directly to a number carries its binary rounding error
        number n = value.typeCode() == NT_FLOAT ? number(value.toString()) : number(value);
        if (exists constraints.minimum && n < constraints.minimum) {
            throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires a value of at least %s; got %s", getName(),
                constraints.minimum.toString(), n.toString());
        }
        if (exists constraints.maximum && n > constraints.maximum) {
            throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires a value of at most %s; got %s", getName(),
                constraints.maximum.toString(), n.toString());
        }
        if (exists constraints.exclusive_minimum && n <= constraints.exclusive_minimum) {
            throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires a value greater than %s; got %s", getName(),
                constraints.exclusive_minimum.toString(), n.toString());
        }
        if (exists constraints.exclusive_maximum && n >= constraints.exclusive_maximum) {
            throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires a value less than %s; got %s", getName(),
                constraints.exclusive_maximum.toString(), n.toString());
        }
        if (constraints.multiple_of) {
            # division of decimal values is not exact, so the quotient is compared with its nearest integer
            number q = n / constraints.multiple_of;
            number nearest = number((q + (q < 0 ? -0.5n : 0.5n)).toInt());
            if (abs(q - nearest) > 0.000000001n * (abs(q) > 1n ? abs(q) : 1n)) {
                throw "RUNTIME-TYPE-ERROR", sprintf("type %y requires a multiple of %s; got %s", getName(),
                    constraints.multiple_of.toString(), n.toString());
            }
        }
    }

    #! Copy and set new tags to the copy
    AbstractDataProviderType copyWithTags(*hash<auto> new_tags) {
        AbstractDataProviderType t = self.copy();
        t.tags = new_tags;
        return t;
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

        # "rimap" only tracks types on the current path, so a type object reachable by more than one path (i.e. any
        # shared subtype in a type DAG) would be re-expanded at every occurrence, which is exponential in the depth
        # of the graph.  Memoize each type's info for the duration of the traversal instead; results are shared by
        # reference, so this bounds both the time and the memory by the number of distinct type objects
        string ck = uh + (simple ? "-s" : "-f");
        bool toplevel = !rimap;
        on_exit if (toplevel) {
            remove imap;
            remove itrunc;
        }
        if (*hash<DataTypeInfo> cached = imap{ck}) {
            return cached;
        }

        bool recursive = rimap{uh} ?? False;
        if (!recursive) {
            rimap{uh} = True;
        }
        on_exit if (!recursive) {
            remove rimap{uh};
        }
        on_error rethrow $1.err, sprintf("%s (while reading type %s %s recursive: %y rimap: %y)", $1.desc, getName(),
            uh, recursive, rimap.size());

        # info for a type truncated by a cycle depends on the path taken to reach it, so it cannot be memoized;
        # record truncations so that every type on the cycle's path is excluded from the cache as well
        int trunc0 = itrunc ?? 0;
        if (recursive) {
            itrunc = trunc0 + 1;
        }
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
            # NOTE: asked through isMandatory() rather than by looking for NT_NOTHING in the direct type
            # hash; that hash reports the base type code of the type's value, so an "or nothing" type
            # contributes only its value's code and would be reported as mandatory here
            "mandatory": isMandatory(),
            "types_accepted": keys getAcceptTypeHash(simple),
            "types_returned": keys getReturnTypeHash(simple),
            "fields": !recursive ? getFieldInfo() : NOTHING,
            "recursive": recursive,
            "unique_key": uh,
            "tags": tags,
            "constraints": constraints,
        };
        # element types are traversed as live objects, so a cyclic element-type graph would recurse without
        # bound here; suppress it on the same condition as "fields", with "recursive" marking the truncation
        if (!recursive) {
            if (*AbstractDataProviderType etype = getElementType()) {
                rv.element_type = etype.getInfo(simple);
            }
        }
        if (exists (auto v = getDefaultValue())) {
            rv.default_value = v;
        }
        if (path.val()) {
            rv.path = path;
        }
        # only memoize path-independent results: neither this type nor anything below it was truncated by a cycle
        if (!recursive && (itrunc ?? 0) == trunc0) {
            imap{ck} = rv;
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

    #! Get allowed values
    *list<hash<AllowedValueInfo>> getAllowedValues() {
    }

    #! Get allowed values
    *hash<string, bool> getAllowedValueMap() {
    }

    #! Get the "allowed values creatable" flag
    bool getAllowedValuesCreatable() {
        return False;
    }

    #! Get element allowed values
    *list<hash<AllowedValueInfo>> getElementAllowedValues() {
    }

    #! Get allowed values
    *hash<string, bool> getElementAllowedValueMap() {
    }

    #! Get the "element allowed values creatable" flag
    bool getElementAllowedValuesCreatable() {
        return False;
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

    #! Returns the soft type for \a type, reusing the one already made for it in this conversion
    /** Preserves the identity of a type shared by more than one parent: without this, each path to a shared type
        converts it again, which is exponential in the depth of the graph, and a recursive type never terminates.

        The memo lives only for the duration of the outermost conversion, so no state is shared between calls or
        between threads, and a later call sees any change made to the source graph in the meantime.

        @param type the source type to convert

        @return the soft type equivalent to \a type

        @since DataProvider 3.8
    */
    static AbstractDataProviderType getSoftTypeShared(AbstractDataProviderType type) {
        string uh = type.uniqueHash();
        if (*hash<SoftTypeMemoInfo> cached = smap{uh}) {
            return cached.soft;
        }
        AbstractDataProviderType::softTypeScopeEnter();
        on_exit AbstractDataProviderType::softTypeScopeExit();
        AbstractDataProviderType rv = type.getSoftType();
        # a composite type publishes its own placeholder before converting its fields; keep that object
        if (*hash<SoftTypeMemoInfo> published = smap{uh}) {
            return published.soft;
        }
        smap{uh} = <SoftTypeMemoInfo>{
            "src": type,
            "soft": rv,
        };
        return rv;
    }

    #! Returns the soft types for \a types, converting each type shared between them only once
    /** @param types the source types to convert

        @return the soft types equivalent to \a types, in the same order

        @since DataProvider 3.8
    */
    static list<AbstractDataProviderType> getSoftTypesShared(list<AbstractDataProviderType> types) {
        AbstractDataProviderType::softTypeScopeEnter();
        on_exit AbstractDataProviderType::softTypeScopeExit();
        return map AbstractDataProviderType::getSoftTypeShared($1), types;
    }

    #! Publishes the soft type of a composite type before its members are converted
    /** A recursive edge back to \a src then resolves to \a soft instead of converting \a src again.

        @param src the source type being converted
        @param soft the soft type being built for \a src

        @since DataProvider 3.8
    */
    static publishSoftType(AbstractDataProviderType src, AbstractDataProviderType soft) {
        if (sdepth) {
            smap{src.uniqueHash()} = <SoftTypeMemoInfo>{
                "src": src,
                "soft": soft,
            };
        }
    }

    #! Starts a soft conversion scope; a composite type holds one while converting its members
    /** A composite type must hold a scope for the whole of its member conversion; the memo is released by the
        outermost scope, so without one each member would be converted with a memo of its own, and types shared
        between the members would be converted once per member.

        @since DataProvider 3.8
    */
    static softTypeScopeEnter() {
        ++sdepth;
    }

    #! Ends a soft conversion scope, releasing the memo with the outermost one
    /** Call this from an @ref on_exit "on_exit" block, so that the memo is also released when a conversion fails.

        @since DataProvider 3.8
    */
    static softTypeScopeExit() {
        if (!--sdepth) {
            remove smap;
        }
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

        # "emap" is passed by value, so it only tracks types on the current path; a type object shared by more than
        # one parent (i.e. any shared subtype in a type DAG) therefore has its example value rebuilt at every
        # occurrence, which is exponential in the depth of the graph.  Memoize per traversal instead: values are
        # shared by reference, so this bounds the time and the memory by the number of distinct (type, field) pairs
        bool toplevel = !exists emap;
        on_exit if (toplevel) {
            remove exmap;
            remove extrunc;
        }
        # scalar examples depend on the field name (see NT_STRING below), so it forms part of the key
        string ck = u + "-" + (fname ?? "");
        if (*hash<auto!> cached = exmap{ck}) {
            return cached.value;
        }

        if (emap{u}) {
            # the value of a type truncated by a cycle depends on the path taken to reach it, so it cannot be
            # memoized; count truncations so that every type on the cycle's path is excluded from the cache too
            extrunc = (extrunc ?? 0) + 1;
            return;
        }
        emap{u} = True;

        int trunc0 = extrunc ?? 0;
        auto rv = getExampleValueImpl(emap, fname);
        if ((extrunc ?? 0) == trunc0) {
            exmap{ck} = {"set": True, "value": rv};
        }
        return rv;
    }

    #! Returns any example value for the type; cycle detection and memoization are handled by the caller
    /** @param emap a hash to ensure that examples are only produced once
        @param fname any field context for the value

        @since DataProvider 3.6
    */
    private auto getExampleValueImpl(*hash<string, bool> emap, *string fname) {
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
            # Examples describe the type, not the time metadata was requested. A fixed UTC
            # sample also makes nested examples stable across processes and time zones.
            case NT_DATE: return 2000-01-01T00:00:00Z;
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
        } else if (type.getBaseTypeCode() == NT_BOOLEAN) {
            # like strings, booleans are converted from strings with validation, so that e.g. "x" is not true
            return new QoreBoolDataTypeBase(type, options, tags, default_value, attr);
        }
        return new QoreDataType(type, options, tags, default_value, attr);
    }

    #! Returns an appropriate object for the given type
    /** @param typename the name of the type; should be a valid Qore type string
        @param options type options for the data type object
    */
    static AbstractDataProviderType get(string typename, *hash<auto> options) {
        if (*list<string> union_types = AbstractDataProviderType::splitTypeUnion(typename)) {
            return new QoreUnionDataType(union_types);
        }
        if (*Type t = DataTypeMap{typename}) {
            return AbstractDataProviderType::get(t, options);
        }
        # class-backed named types (e.g. "file", "rgbcolor", "email", "uuid") live only in
        # NamedClassBackedTypeMap, not DataTypeMap
        if (*AbstractDataProviderType adt = NamedClassBackedTypeMap{typename}) {
            return adt;
        }
        return AbstractDataProviderType::get(new Type(typename), options);
    }

    #! Split a type string into top-level union alternatives
    /** @param typename the type string to split
        @return a list of top-level union alternatives, or @ref NOTHING if the type is not a union

        @throw UNKNOWN-TYPE a top-level union delimiter has an empty type alternative

        @since DataProvider 3.5.1
    */
    static *list<string> splitTypeUnion(string typename) {
        int angle_depth = 0;
        int square_depth = 0;
        int paren_depth = 0;
        int start = 0;
        list<string> rv = ();

        for (int i = 0; i < typename.length(); ++i) {
            string ch = typename[i];
            switch (ch) {
                case "<": ++angle_depth; break;
                case ">": if (angle_depth) --angle_depth; break;
                case "[": ++square_depth; break;
                case "]": if (square_depth) --square_depth; break;
                case "(": ++paren_depth; break;
                case ")": if (paren_depth) --paren_depth; break;
                case "|":
                    if (!angle_depth && !square_depth && !paren_depth) {
                        string alternative = trim(typename.substr(start, i - start));
                        if (!alternative.val()) {
                            throw "UNKNOWN-TYPE", sprintf("invalid union type %y: empty alternative", typename);
                        }
                        rv += alternative;
                        start = i + 1;
                    }
                    break;
            }
        }

        if (!rv) {
            return;
        }

        string alternative = trim(typename.substr(start));
        if (!alternative.val()) {
            throw "UNKNOWN-TYPE", sprintf("invalid union type %y: empty alternative", typename);
        }
        rv += alternative;
        return rv;
    }

    #! Returns an appropriate object for the given type description
    /** @param info the description of the type

        @return the type registered with the description's \c path in this process, if any, otherwise a type
        restored from the structure in the description

        @throw TYPE-ARG-ERROR the description has a \c path that is not registered in this process, and no name or
        base type to restore the type from

        @note A list type is restored as a @ref SoftListDataType when its name is a soft list type name such as
        \c "softlist<string>" or \c "*softlist<string>", so that it converts a single value to a list like the
        described type does; a list type with any other name is restored as a @ref ListDataType

        @note A type path, such as one registered when a provider app is loaded, identifies the registered type
        only in a process where that registration has been made.  Elsewhere the type is restored from its
        description, like a type without a path: it has the described structure and constraints but not any
        custom behavior of the original type's class, and it has no path
    */
    static AbstractDataProviderType get(hash<DataTypeInfo> info) {
        AbstractDataProviderType rv = AbstractDataProviderType::getFromInfo(info);
        # restored types may be shared, so constraints are set on a copy
        return info.constraints ? rv.copyWithConstraints(info.constraints) : rv;
    }

    #! Returns an appropriate object for the given type description, without its validation constraints
    /** @param info the description of the type

        @return a type for the description, which may be shared

        @since DataProvider 3.8
    */
    private static AbstractDataProviderType getFromInfo(hash<DataTypeInfo> info) {
        if (info.path) {
            # the path identifies the type registered in this process, which keeps the custom behavior of the
            # original type's class; a path registered only where its app is loaded is not known here, and the type
            # is then restored from the structure the description also holds
            *AbstractDataProviderType rv = DataProvider::getTypeCache().lookupTypeFromPath(info.path);
            if (rv) {
                return rv;
            }
            if (!info.name && !info.base_type) {
                throw "TYPE-ARG-ERROR", sprintf("Cannot find any type with path %y, and the description has no "
                    "structure to restore it from", info.path);
            }
        }
        # class-backed named types (e.g. "file", "email") are identified by their name, even when their description
        # also lists fields
        if (*AbstractDataProviderType adt = NamedClassBackedTypeMap{info.name}) {
            return adt;
        }
        hash<auto> attr = info{"display_name", "short_desc", "desc"};
        if (info.union_types) {
            # each alternative's description restores its own nullability, from which the union's is derived
            return new QoreUnionDataType((map AbstractDataProviderType::get($1), info.union_types),
                attr ? cast<hash<NameDescInfo>>(attr) : NOTHING);
        }
        # a hash type is restored by its name and description when the name is not a Qore type, even if it has no
        # fields
        if (info.fields || (info.base_type == "hash" && (!AbstractDataProviderType::isQoreTypeName(info.name)
                || info.default_field_type_info))) {
            HashDataType rv(info.mandatory ? AutoHashType : AutoHashOrNothingType, info.name,
                AbstractDataProviderType::getDataFields(info.fields ?? {}), info.options, info.tags, info.default_value,
                attr ? cast<hash<NameDescInfo>>(attr) : NOTHING);
            AbstractDataProviderType::setHashDataTypeInfo(rv, info);
            return rv;
        }
        if ((info.base_type == "list" || info.name =~ /^.?list</) && info.element_type) {
            AbstractDataProviderType element_type = AbstractDataProviderType::get(info.element_type);
            *hash<NameDescInfo> name_desc = attr ? cast<hash<NameDescInfo>>(attr) : NOTHING;
            # like a soft scalar type, a soft list is identified by its type name; it converts a single element to a
            # list, so restoring it as a plain list would reject values the original type accepts
            if (info.name =~ /^\*?softlist</) {
                return new SoftListDataType(info.name, element_type, !info.mandatory, info.options, info.tags,
                    info.default_value, name_desc);
            }
            return new ListDataType(info.name, element_type, !info.mandatory, info.options, info.tags,
                info.default_value, name_desc);
        }
        *Type t = DataTypeMap{info.name};
        # the map has aliases (e.g. "any" for "auto"); the type named by the description is restored under that name
        if (t && t.getName() != info.name) {
            try {
                Type named(info.name);
                if (named.getName() == info.name) {
                    t = named;
                }
            } catch (hash<ExceptionInfo> ex) {
            }
        }
        if (!t && info.name) {
            # class-backed named types (e.g. "file", "email") are not Qore reflection types; the name
            # ("email" vs "*email") already encodes nullability
            if (*AbstractDataProviderType adt = NamedClassBackedTypeMap{info.name}) {
                return adt;
            }
            try {
                t = new Type(info.name);
            } catch (hash<ExceptionInfo> ex) {
                # Fall back to the base type below; named schema types are not necessarily Qore type names.
            }
        }
        if (!t) {
            t = DataTypeMap{info.base_type};
        }
        if (t) {
            # options apply to the mandatory type (an "or nothing" type supports none), so a type with options and
            # its "or nothing" equivalent are created from the mandatory type, like the original
            if (info.options && !info.mandatory && info.name =~ /^\*/
                    && (*Type mandatory_type = DataTypeMap{info.name.substr(1)})) {
                AbstractDataProviderType rv = AbstractDataProviderType::get(mandatory_type, info.options, info.tags,
                    info.default_value, attr ? cast<hash<NameDescInfo>>(attr) : NOTHING);
                return AbstractDataProviderType::setDataTypeInfo(rv.getOrNothingType(), info);
            }
            AbstractDataProviderType rv = AbstractDataProviderType::get(t, info.options, info.tags, info.default_value,
                attr ? cast<hash<NameDescInfo>>(attr) : NOTHING);
            return AbstractDataProviderType::setDataTypeInfo(rv, info);
        }

        # a type whose name and base type are known only in the program defining it, such as a type based on a custom
        # Qore type, is restored from the value type it returns and the values it accepts
        if (*Type value_type = AbstractDataProviderType::getValueTypeFromInfo(info)) {
            # the type keeps the name it is described by, which is not the name of the Qore value type
            hash<NameDescInfo> named = <NameDescInfo>{
                "name": info.name ?? value_type.getName(),
                "display_name": info.display_name ?? info.name ?? value_type.getName(),
                "short_desc": info.short_desc,
                "desc": info.desc ?? info.short_desc ?? info.name ?? value_type.getName(),
            };
            AbstractDataProviderType rv = AbstractDataProviderType::get(value_type, info.options, info.tags,
                info.default_value, named);
            return AbstractDataProviderType::setDataTypeInfo(rv, info);
        }

        throw "TYPE-ERROR", sprintf("No match for type: %y", info);
    }

    #! Returns the Qore type for a description from the value type it returns and the values it accepts
    /** @param info the type description

        @return the Qore type returning the single value type the description returns, as a soft type if it also
        accepts values of other types; @ref nothing if the description returns more than one value type or one that
        is not a Qore type

        @since DataProvider 3.8
    */
    private static *Type getValueTypeFromInfo(hash<DataTypeInfo> info) {
        list<string> returned = select info.types_returned, $1 != "nothing" && $1 != "null";
        if (returned.size() != 1 || !DataTypeMap{returned[0]}) {
            return;
        }
        string value_type = returned[0];
        bool converts = (select info.types_accepted,
            $1 != value_type && $1 != "nothing" && $1 != "null").size() > 0;
        if (converts && (*string soft_name = OptimalQoreSoftDataTypeMap{value_type})) {
            return DataTypeMap{soft_name} ?? new Type(soft_name);
        }
        return DataTypeMap{value_type};
    }

    #! Returns True if the given name is a Qore type name
    /** @param name the name to check

        @return @ref True if @a name is a Qore type name

        @since DataProvider 3.8
    */
    private static bool isQoreTypeName(string name) {
        if (DataTypeMap{name}) {
            return True;
        }
        try {
            Type t(name);
            return True;
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
    }

    #! Restores data fields from serialized field descriptions
    /** @param fields serialized field descriptions

        @return restored data fields keyed by field name

        @since DataProvider 3.5.1
    */
    private static hash<string, AbstractDataField> getDataFields(hash<string, hash<DataFieldInfo>> fields) {
        return map {$1.key: AbstractDataProviderType::getDataField($1.value + {"name": $1.value.name ?? $1.key})},
            fields.pairIterator();
    }

    #! Restores a data field from its serialized description
    /** An example value is presentation metadata; one that the restored field does not accept, such as a
        generated example in a description written by an earlier version, is dropped rather than failing the
        restoration of the whole type

        @param field the serialized field description

        @return the restored data field

        @since DataProvider 3.8
    */
    private static AbstractDataField getDataField(hash<auto> field) {
        try {
            return new QoreDataField(field);
        } catch (hash<ExceptionInfo> ex) {
            if (!exists field.example_value) {
                rethrow;
            }
        }
        remove field.example_value;
        return new QoreDataField(field);
    }

    #! Applies common serialized data type metadata to a restored type
    /** @param rv the restored data provider type
        @param info the serialized type description

        @return the restored type with common metadata applied

        @since DataProvider 3.5.1
    */
    private static AbstractDataProviderType setDataTypeInfo(AbstractDataProviderType rv, hash<DataTypeInfo> info) {
        if (!info.mandatory && !rv.isOrNothingType()) {
            rv = rv.getOrNothingType();
        }
        if (info.default_field_type_info && (rv instanceof HashDataType)) {
            HashDataType hdt = cast<HashDataType>(rv);
            if (!hdt.getDefaultOtherFieldType()) {
                hdt.setDefaultOtherFieldType(AbstractDataProviderType::get(info.default_field_type_info));
            }
        }
        if (info.required_groups && (rv instanceof HashDataType)) {
            cast<HashDataType>(rv).setRequiredGroups(info.required_groups);
        }
        return rv;
    }

    #! Applies hash-specific serialized type metadata to a restored hash type
    /** @param rv the restored hash data provider type
        @param info the serialized type description

        @since DataProvider 3.5.1
    */
    private static setHashDataTypeInfo(HashDataType rv, hash<DataTypeInfo> info) {
        # a hash type without fields accepts any other fields by default, so an absent default is restored as none
        rv.setDefaultOtherFieldType(info.default_field_type_info
            ? AbstractDataProviderType::get(info.default_field_type_info)
            : NOTHING);
        if (info.required_groups) {
            rv.setRequiredGroups(info.required_groups);
        }
    }

    #! sets the given option without any validation of the option
    private setOptionIntern(string opt, auto value) {
        options{opt} = value;
    }

    #! Returns a list of allowed value entries for all field names in the given type
    /** Recursively walks the type's fields, building dot-notation paths for nested hash fields.

        @param type the data provider type to enumerate fields from
        @param prefix the current dot-notation prefix (empty for top-level)
        @param visited a set of type unique IDs already visited (to guard against circular types)

        @return a list of AllowedValueInfo hashes with field names as values

        @since %DataProvider 3.4
    */
    static list<hash<AllowedValueInfo>> getFieldNamesAsAllowedValues(AbstractDataProviderType type,
            string prefix = "", *hash<string, bool> visited) {
        list<hash<AllowedValueInfo>> result = ();
        *hash<string, AbstractDataField> fields = type.getFields();
        if (!fields) {
            return result;
        }
        string uid = type.uniqueHash();
        if (visited{uid}) {
            return result;
        }
        visited{uid} = True;
        foreach hash<auto> i in (fields.pairIterator()) {
            string full_name = prefix.val() ? prefix + "." + i.key : i.key;
            AbstractDataProviderType field_type = i.value.getType();
            string type_name = field_type.getName();
            push result, <AllowedValueInfo>{
                "value": full_name,
                "display_name": full_name,
                "desc": sprintf("field '%s' (%s)", full_name, type_name),
            };
            # recurse into hash-type fields for dot-notation access
            if (field_type.getBaseTypeCode() == NT_HASH) {
                result += AbstractDataProviderType::getFieldNamesAsAllowedValues(field_type, full_name, visited);
            }
        }
        return result;
    }
}
}

namespace Priv {
#! Recursive info map: unique key -> True
thread_local hash<string, bool> rimap;

#! Memoizes type info for the duration of a single getInfo() traversal, keyed by type object identity
/** Without this, a type object shared by multiple parents in a type DAG is re-expanded once per path, which is
    exponential in the depth of the graph
*/
thread_local hash<string, hash<DataTypeInfo>> imap;

#! Counts cycle truncations during a getInfo() traversal, so that path-dependent results are not memoized
thread_local int itrunc;

#! Memoizes example values for the duration of a single getExampleValue() traversal
/** Keyed by type object identity and field name; without this, a type object shared by multiple parents in a type
    DAG has its example value rebuilt once per path, which is exponential in the depth of the graph
*/
thread_local hash<string, hash<auto!>> exmap;

#! Counts cycle truncations during a getExampleValue() traversal, so that path-dependent values are not memoized
thread_local int extrunc;

#! An entry in the soft type conversion memo
hashdecl SoftTypeMemoInfo {
    #! The source type, retained so that no other type can take its identity while the memo is live
    /** The \c uniqueHash() object pseudo-method identifies an object by its address, so a source type released
        during the conversion could otherwise be replaced at the same address by an unrelated type, which would
        then be given the first type's soft type.  Holding a reference here keeps the address unavailable.
    */
    AbstractDataProviderType src;

    #! The soft type created for \a src
    AbstractDataProviderType soft;
}

#! Memoizes soft types for the duration of a single getSoftType() conversion, keyed by source type object identity
/** Without this, a type object shared by more than one parent in a type DAG is converted once per path, which is
    exponential in the depth of the graph.  A composite type publishes its soft type here before converting its
    fields, so that a recursive edge back to it resolves to the same object instead of recursing forever.
*/
thread_local hash<string, hash<SoftTypeMemoInfo>> smap;

#! Tracks the nesting depth of getSoftType() conversions, so that the memo is released by the outermost call
thread_local int sdepth;
}
