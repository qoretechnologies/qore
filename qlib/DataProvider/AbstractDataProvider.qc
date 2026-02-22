# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractDataProvider class definition

/** AbstractDataProvider.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
/** @defgroup db_provider_upsert_results DB Provider Upsert Result Codes
    These are the results codes returned with single-record upsert actions
*/
#/@{
#! Indicates that the record was inserted
public const UpsertResultInserted = "inserted";

#! Indicates that the record was updated
public const UpsertResultUpdated = "updated";

#! Indicates that the record was verified as already in the target state
public const UpsertResultVerified = "verified";

#! Indicates that the record was left unchanged
public const UpsertResultUnchanged = "unchanged";

#! Indicates that the record was deleted
public const UpsertResultDeleted = "deleted";
#/@}

#! Base option info with common fields shared by DataProviderOptionInfo, ActionOptionInfo, and ConnectionOptionInfo
/** @since %DataProvider 3.1
*/
public hashdecl BaseOptionInfo inherits DisplayDescInfo {
    #! Is the option value sensitive?
    bool sensitive = False;

    #! List of allowed values (enum)
    *softlist<hash<Util::AllowedValueInfo>> allowed_values;

    #! Does the option accept values not defined in allowed_values too?
    bool allowed_values_creatable = False;

    #! List of allowed values for elements of list types (enum)
    *softlist<hash<Util::AllowedValueInfo>> element_allowed_values;

    #! Does the option accept values for elements not defined in element_allowed_values too?
    bool element_allowed_values_creatable = False;

    #! Any default value
    auto default_value;

    #! Any optional example value for the option
    auto example_value;

    #! Free-form metadata on options
    *hash<auto> metadata;

    #! Messages related to the option
    *list<hash<UiMessageInfo>> messages;

    #! Any icon info for the option
    *string icon;

    #! Any image info for the option
    *string image;

    #! Optional UI type override for frontend rendering
    /** If not set, the UI should derive ui_type from the base type (e.g., @c "string" → @c "richtext").
        Set this to override the default mapping (e.g., @c "processor", @c "password", @c "file-picker").

        @since %DataProvider 3.3
    */
    *string ui_type;

    #! Required flag
    bool required = False;

    #! Is this option read-only?
    bool readonly = False;

    #! Is this option disabled?
    bool disabled = False;

    #! Should the option be preselected in UIs?
    bool preselected = False;

    #! Does any other option depend on this one?
    bool has_dependents = False;

    #! A list of other options that must be set before this option can be chosen
    /** Each element is either:
        - a string: a single option name that must have a value (e.g. @c "driver")
        - a string with @c "=": option must equal a specific value (e.g. @c "driver=jdbc")
        - a list of strings: a group where at least one must have a value

        @since %DataProvider 3.3
    */
    *softlist<softstring> depends_on;

    #! How to react to changes (e.g. @c "refetch" to reload options from the server)
    /** @since %DataProvider 3.3
    */
    *softlist<softstring> on_change;
}

#! Data provider option info
public hashdecl DataProviderOptionInfo inherits BaseOptionInfo {
    #! The option value type or types
    softlist<AbstractDataProviderType> type;

    #! Is the option part of a required group?
    /** A required group is a group of options of which at least one must be set; each group name is an arbitrary
        string that is used to mark membership in the group by different options
    */
    *softlist<string> required_groups;

    #! Is the option exclusive with other options?
    *softlist<string> exclusive_with;

    #! A tag allowing allowed values to be returned dynamically as reference data
    /** The value is the name of the reference data; if allowed_values are not present, calls will be made to
        @ref DataProvider::AbstractDataProvider::getReferenceData() "AbstractDataProvider::getReferenceData()"
        in the available DataProvider when resolving application actions if this attribute is set
    */
    *string ref_data;

    #! A tag allowing element allowed values to be returned dynamically as reference data
    /** The value is the name of the reference data; if allowed_values are not present, calls will be made to
        @ref DataProvider::AbstractDataProvider::getElementReferenceData() "AbstractDataProvider::getElementReferenceData()"
        in the available DataProvider when resolving application actions if this attribute is set
    */
    *string element_ref_data;

    #! A tag allowing the default value for the option to be returned dynamically
    *string default_ref_data;

}

#! Data provider summary info
public hashdecl DataProviderSummaryInfo inherits NameDescInfo {
    #! The name of the provider type
    string type;

    #! Does the data provider support reading
    bool supports_read = False;

    #! Does the data provider support record creation?
    bool supports_create = False;

    #! Does the data provider support record updates?
    bool supports_update = False;

    #! Does the data provider support record upserts (create or update)?
    bool supports_upsert = False;

    #! Does the data provider support record deletion?
    bool supports_delete = False;

    #! Does the data provider support native record searching?
    bool supports_native_search = False;

    #! Does the data provider support native / optimized bulk reads?
    /** if this is @ref False and \a supports_read is @ref True, the data provider will still support bulk read
        APIs but without any performance improvements for bulk data (simulated bulk reads)
    */
    bool supports_bulk_read = False;

    #! Does the data provider support native / optimized bulk creation?
    /** if this is @ref False and \a supports_creation is @ref True, the data provider will still support bulk write
        APIs but without any performance improvements for bulk data (simulated bulk creates)
    */
    bool supports_bulk_create = False;

    #! Does the data provider support native / optimized bulk upserts?
    /** if this is @ref False and \a supports_upsert is @ref True, the data provider will still support bulk upsert
        APIs but without any performance improvements for bulk data (simulated bulk upserts)
    */
    bool supports_bulk_upsert = False;

    #! Does the data provider support the request API?
    bool supports_request = False;

    #! Does the data provider support children?
    bool supports_children = False;

    #! Does the data provider require transaction management?
    bool transaction_management = False;

    #! Does the data provider provide a record?
    bool has_record = False;

    #! Does this data provide support advanced search expressions?
    bool supports_search_expressions = False;

    #! Does this data provider support the event API / observer pattern?
    /**
        @since DataProvider 2.5
    */
    bool supports_observable = False;

    #! Message support
    /** This describes the data provider's support for output messages, if any, and the relationship to observed
        events, if any.

        @see dataprovider_message_support

        @since DataProvider 2.5
    */
    string supports_messages = MSG_None;

    #! Does the data provider support connections and connection status reporting?
    /** @since DataProvider 3.0
    */
    bool supports_connections = False;

    #! Does the data provider support automatic reconnection?
    /** @since DataProvider 3.0
    */
    bool supports_auto_reconnect = False;

    #! Can any child data providers offer API services (request - response data providers)?
    /** @since DataProvider 2.7
    */
    bool children_can_support_apis = False;

    #! Can any child data providers offer record-based providers?
    /** @since DataProvider 2.7
    */
    bool children_can_support_records = False;

    #! Can any child data providers support the observer pattern / event API?
    /** @since DataProvider 2.7
    */
    bool children_can_support_observers = False;

    #! Can any child data providers support messages?
    /** @since DataProvider 2.7
    */
    bool children_can_support_messages = False;

    #! Can any child data providers support transaction management?
    /** @since DataProvider 2.8
    */
    bool children_can_support_transactions = False;

    #! Are all children always of the same data provider class that support the same options?
    /** @since DataProvider 3.0
    */
    bool children_identical = False;

    #! Does the data provider support being used as a pipeline processor?
    /** @since %DataProvider 3.3
    */
    bool supports_pipeline_processor = False;

    #! Processor capability bitfield (DPC_* constants)
    /** @since %DataProvider 3.3
    */
    int processor_capabilities = 0;

    #! Can any child data providers support being used as pipeline processors?
    /** @since %DataProvider 3.3
    */
    bool children_can_support_pipeline_processors = False;
}

#! Data provider message and event info
public hashdecl DataProviderMessageInfo {
    #! The description of the message or event
    /** Markdown is supported
    */
    string desc;

    #! The data type of the message or event
    AbstractDataProviderType type;
}

/** @defgroup dataprovider_message_support Output Message Support

    These constants describe the data provider's support for output messages, if any, and the relationship to observed
    events, if any.

    @since DataProvider 2.5
*/
#/@{
#! No output type
/** The data provider does not support output messages
*/
public const MSG_None = "NONE";

#! Synchronous output type, an output message is expected after each observed event
/** The data provider supports output messages, and an output message is expected after each input message.

    This can be used to support the server-side request-response API integration pattern
*/
public const MSG_Sync = "SYNC";

#! Asynchronous output type, output can happen any time
/** Input (i.e. observed events) may not be related or supported
*/
public const MSG_Async = "ASYNC";
#/@}

/** @defgroup signature_type_codes Signature Type Codes

    @since DataProvider 2.3
*/
#/@{
#! Indicates any valid expression that returns the given type (default)
public const ST_Any = 1;

#! Indicates any literal value of the given type; i.e. not an operator or other expression
public const ST_Value = 2;

#! The signature type indicates a string field name when used as an argument, and the type is the field type
/** In this case the value must always be a string (referring to the field name), and the type is always "any",
    referring to the fact that the final type reflects the field's type, which can be of any supported type
*/
public const ST_Field = 3;
#/@}

#! Maps signature type codes to descriptions
public const SignatureTypeCodeMap = {
    ST_Any: "any",
    ST_Value: "value",
    ST_Field: "field reference",
};

#! Maps signature description values to integer codes
public const SignatureTypeDescMap = map {$1.value: $1.key.toInt()}, SignatureTypeCodeMap.pairIterator();

/** @defgroup expression_role_codes Expression Role Codes

    @since DataProvider 2.3
*/
#/@{
#! Indicates an expression that can be used in a search operation
public const ER_Search = (1 << 0);

#! Indicates an expression that can be used in a field
public const ER_Field = (1 << 1);

#! Indicates an expression that can be used in all contexts
public const ER_All = ER_Search | ER_Field;
#/@}

#! Role map from codes to descriptions
public const RoleCodeMap = {
    ER_Search: "search",
    ER_Field: "field",
    ER_All: "all",
};

#! Role map from descriptions to codes
public const RoleDescMap = map {$1.value: $1.key.toInt()}, RoleCodeMap.pairIterator();

/** @defgroup logic_capability_codes Logic Capability Codes

    @since DataProvider 2.3
*/
#/@{
#! Indicates that boolean expressions can be combined with AND logic
public const LC_And = (1 << 0);

#! Indicates that boolean expressions can be combined with OR logic
public const LC_Or = (1 << 1);

#! Indicates that all logic capabilities are supported
public const LC_All = (LC_And | LC_Or);
#/@}

#! Logic map from codes to descriptions
public const LogicCodeMap = {
    LC_And: "AND",
    LC_Or: "OR",
};

#! Logic map from descriptions to integer codes
public const LogicDescMap = map {$1.value: $1.key.toInt()}, LogicCodeMap.pairIterator();

#! Data provider signature argument info
public hashdecl DataProviderSignatureTypeInfo {
    #! Argument types
    AbstractDataProviderType type;

    #! The signature type code
    int type_code = ST_Any;
}

#! Data provider signature field type
public const DataProviderSignatureFieldType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."any",
    "type_code": ST_Field,
};

#! Data provider signature string value type
public const DataProviderSignatureStringValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."string",
    "type_code": ST_Value,
};

#! Data provider signature optional string value type
public const DataProviderSignatureOptionalStringValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*string",
    "type_code": ST_Value,
};

#! Data provider signature int value type
public const DataProviderSignatureIntValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."int",
    "type_code": ST_Value,
};

#! Data provider signature *int value type
public const DataProviderSignatureOptionalIntValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*int",
    "type_code": ST_Value,
};

#! Data provider signature bool value type
public const DataProviderSignatureBoolValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."bool",
    "type_code": ST_Value,
};

#! Data provider signature *int value type
public const DataProviderSignatureOptionalBoolValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*bool",
    "type_code": ST_Value,
};

#! Data provider signature list value type
public const DataProviderSignatureListValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."list",
    "type_code": ST_Value,
};

#! Data provider signature optional list value type
public const DataProviderSignatureOptionalListValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*list",
    "type_code": ST_Value,
};

#! Data provider signature optional string list value type
public const DataProviderSignatureOptionalStringListValueType = <DataProviderSignatureTypeInfo>{
    "type": new ListDataType("*list<string>", StringType, True),
    "type_code": ST_Value,
};

#! Data provider signature hash value type
public const DataProviderSignatureHashValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."hash",
    "type_code": ST_Value,
};

#! Data provider signature optional hash value type
public const DataProviderSignatureOptionalHashValueType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*hash",
    "type_code": ST_Value,
};

#! Data provider signature any type
public const DataProviderSignatureAnyType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."any",
    "type_code": ST_Any,
};

#! Data provider signature list type
public const DataProviderSignatureListType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."list",
    "type_code": ST_Any,
};

#! Data provider signature bool type
public const DataProviderSignatureBoolType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."bool",
    "type_code": ST_Any,
};

#! Data provider signature optional bool type
public const DataProviderSignatureOptionalBoolType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*bool",
    "type_code": ST_Any,
};

#! Data provider signature int type
public const DataProviderSignatureIntType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."int",
    "type_code": ST_Any,
};

#! Data provider signature optional int type
public const DataProviderSignatureOptionalIntType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*int",
    "type_code": ST_Any,
};

#! Data provider signature string type
public const DataProviderSignatureStringType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."string",
    "type_code": ST_Any,
};

#! Data provider signature optional string type
public const DataProviderSignatureOptionalStringType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*string",
    "type_code": ST_Any,
};

#! Data provider signature date type
public const DataProviderSignatureDateType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."date",
    "type_code": ST_Any,
};

#! Data provider signature optional date type
public const DataProviderSignatureOptionalDateType = <DataProviderSignatureTypeInfo>{
    "type": AbstractDataProviderTypeMap."*date",
    "type_code": ST_Any,
};

/** @defgroup dataprovider_expression_type_codes DataProvider Expression Type Codes

    @since DataProvider 2.3
*/
#/@{
#! Indicates an operator expression
public const DET_Operator = 1;

#! Indicates a function expression
public const DET_Function = 2;
#/@}

#! Maps DataProvider expression type codes to descriptions
public const ExpressionTypeCodeMap = {
    DET_Operator: "operator",
    DET_Function: "function",
};

#! Maps DataProvider expression type descriptions to integer codes
public const ExpressionTypeDescMap = map {$1.value: $1.key.toInt()}, ExpressionTypeCodeMap.pairIterator();

/** @defgroup dataprovider_expression_subtype_codes DataProvider Expression Subtype Codes

    @since DataProvider 3.0
*/
#/@{
#! Indicates a generic subtyp
public const DES_Generic = 1;

#! Indicates a logical operator type
public const DES_LogicOperator = 2;
#/@}

#! Maps DataProvider expression subtype codes to descriptions
public const ExpressionSubtypeCodeMap = {
    DES_Generic: "generic",
    DES_LogicOperator: "logic-operator",
};

#! Maps DataProvider expression subtype descriptions to integer codes
public const ExpressionSubtypeDescMap = map {$1.value: $1.key.toInt()}, ExpressionSubtypeCodeMap.pairIterator();

#! Data provider expression arg info
public hashdecl DataProviderArgInfo {
    #! Argument display name
    string display_name;

    #! Argument short description in plain text
    *string short_desc;

    #! The argument description
    string desc;

    #! Any default value
    auto default_value;

    #! Is the argument value sensitive?
    bool sensitive = False;

    #! List of allowed values (enum)
    /* In case the arg type is a list, then this will provide the discrete values that can appear in the list
    */
    *softlist<hash<Util::AllowedValueInfo>> allowed_values;

    #! Does the option accept values not defined in allowed_values too?
    *bool allowed_values_creatable;

    #! List of allowed values for elements of list types (enum)
    /* In case the option type is a list, then this will provide the discrete values that can appear in the list
    */
    *softlist<hash<Util::AllowedValueInfo>> element_allowed_values;

    #! Does the option accept values for elements not defined in element_allowed_values too?
    bool element_allowed_values_creatable;

    #! Any optional example value for the argument
    /** The value must be acceptable by the argument's type
    */
    auto example_value;

    #! A label hint for display before the argument
    *string label_before;

    #! A label hint for display after the argument
    *string label_after;
}

#! Data provider signature info
public hashdecl DataProviderExpressionInfo {
    #! The type of expression; see @ref dataprovider_expression_type_codes
    int type;

    #! The expression subtype
    /** @see @ref dataprovider_expression_subtype_codes
    */
    int subtype = DES_Generic;

    #! The technical name or internal label used for the expression
    /** Must correspond to the key used in any expression hash
    */
    string name;

    #! The display name
    string display_name;

    #! The short description of the operation
    string short_desc;

    #! The description of the operation
    /** With markdown formatting
    */
    string desc;

    #! The symbol to use when rendering expressions
    string symbol;

    #! The groups that the expression belongs to
    softlist<string> groups = ("Miscellaneous",);

    #! The expression role code / bitfield
    /** @see @ref expression_role_codes
    */
    int role = ER_All;

    #! Any template to render the expression
    /** Placeholders are:
        - $symbol: the expression symbol
        - $arg[n]: the nth argument (0-based index), followed optionally by a dot and attribute name to access attributes of
          argument hashes
        - $args: all arguments as a comma-separated list
    */
    *string render_template;

    #! Argument types
    softlist<hash<DataProviderSignatureTypeInfo>> args;

    #! Argument info
    *softlist<hash<DataProviderArgInfo>> arg_info;

    #! The return type
    AbstractDataProviderType return_type;

    #! The last argument can be repeated indefinitely
    bool varargs = False;

    #! Minimum arguments when varargs is set
    *int min_args;

    #! Return type argument priority list
    /** A list of types that indicate the priority order when determining the final return type
        based on argument types; to determine the final return type, the first type in this list that matches
        any of the argument types is used as the return type; i.e. the earlier the type appears in this list, the
        higher its priority

        If there are no matches, the declared \a return_type is used; normally \c "any"

        This is useful for expressions that take multiple argument types and return different types depending on the
        argument types

        @note It is an error to set both this and \c return_type_first_arg to @ref True

        @since DataProvider 3.3
    */
    *list<string> return_type_arg_priority;

    #! Use the first argument's type as the return type
    /** If set to @ref True, the type of the first argument is used as the return type

        @note It is an error to set both this and \c return_type_arg_priority

        @since DataProvider 3.3
    */
    *bool return_type_first_arg;
}

#! Range specification for DPQL slice accessors
/** Represents a range with optional start and end bounds.
    Either start or end (or both) must be present.
    Both bounds are inclusive (Qore-style).

    @since DataProvider 3.3
*/
public hashdecl DataProviderRangeSpec {
    #! Start index of the range (NOTHING for open start, e.g., \c [..5])
    *int start;
    #! End index of the range (NOTHING for open end, e.g., \c [2..])
    *int end;
}

#! Data provider field reference
public hashdecl DataProviderFieldReference {
    #! The name of the field being referenced
    string field;
}

#! Data provider template reference
/** Represents a contextual data reference resolved at runtime via callback.
    @par Syntax
    - Simple: \c $context:value (e.g., \c $static:account.id)
    - Bracketed: \c $context:{value with special chars}
    @since DataProvider 3.3
*/
public hashdecl DataProviderTemplateReference {
    #! Template context name (e.g., "static", "dynamic", "config")
    string tmpl_context;
    #! Template value path (e.g., "account.id", "threshold")
    string tmpl_value;
    #! Original raw text for round-trip serialization
    string raw;
}

#! Data provider expression
public hashdecl DataProviderExpression {
    #! Expression name
    string exp;

    #! Expression arguments
    /** Either values, field references (hash<DataProviderFieldReference>), or further expressions
        (hash<DataProviderExpression> values) to be evaluated
    */
    softlist<auto> args;
}

#! Data provider info
public hashdecl DataProviderInfo {
    #! Data provider name
    /** @note this is a dynamic attribute; it differs for different data providers of the same type
    */
    string name;

    #! Data provider display name
    string display_name;

    #! Data provider short description in plain text
    *string short_desc;

    #! A markdown description of the data provider
    /** @note this is a dynamic attribute; it differs for different data providers of the same type
    */
    *string desc;

    #! The name of the provider type
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    string type;

    #! Does the data provider support reading
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_read = False;

    #! Does the data provider support record creation?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_create = False;

    #! Does the data provider support record updates?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_update = False;

    #! Does the data provider support record upserts (create or update)?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_upsert = False;

    #! Does the data provider support record deletion?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_delete = False;

    #! Does the data provider support native record searching?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_native_search = False;

    #! Does the data provider support native / optimized bulk reads?
    /** if this is @ref False and \a supports_read is @ref True, the data provider will still support bulk read
        APIs but without any performance improvements for bulk data (simulated bulk reads)

        @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_bulk_read = False;

    #! Does the data provider support native / optimized bulk creation?
    /** if this is @ref False and \a supports_creation is @ref True, the data provider will still support bulk write
        APIs but without any performance improvements for bulk data (simulated bulk creates)

        @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_bulk_create = False;

    #! Does the data provider support native / optimized bulk upserts?
    /** if this is @ref False and \a supports_upsert is @ref True, the data provider will still support bulk upsert
        APIs but without any performance improvements for bulk data (simulated bulk upserts)

        @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_bulk_upsert = False;

    #! Does the data provider support the request API?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_request = False;

    #! Does the data provider support children?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool supports_children = False;

    #! Does the data provider require transaction management?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool transaction_management = False;

    #! Does the data provider provide a record?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool has_record = False;

    #! Do we require search options to retrieve the record type?
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    bool record_requires_search_options = False;

    #! Does the data provider support creating new child providers?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    bool supports_child_create = False;

    #! Does the data provider support deleting new child providers?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    bool supports_child_delete = False;

    #! Does the data provider support adding new fields?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    bool supports_add_field = False;

    #! Does the data provider support updating existing fields?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    bool supports_update_field = False;

    #! Does the data provider support deleting fields?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    bool supports_delete_field = False;

    #! Does the data provider support a schema?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.2
    */
    bool supports_schema = False;

    #! Does this data provide support advanced search experssions?
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.2
    */
    bool supports_search_expressions = False;

    #! Does this data provider support the event API / observer pattern?
    /** If this is @ref True, then the data provider will inherit @ref Observable and can be observed

        @since DataProvider 2.5
    */
    bool supports_observable = False;

    #! Output message support
    /** @see dataprovider_message_support

        This attribute defines if the message API is supported by the data provider as well as the relationship to
        observable events, if any

        @since DataProvider 2.5
    */
    string supports_messages = MSG_None;

    #! Does the data provider support connections and connection status reporting?
    /** @since DataProvider 3.0
    */
    bool supports_connections = False;

    #! Does the data provider support automatic reconnection?
    /** @since DataProvider 3.0
    */
    bool supports_auto_reconnect = False;

    #! Can any child data providers offer API services (request - response data providers)?
    /** @since DataProvider 2.6
    */
    bool children_can_support_apis = False;

    #! Can any child data providers offer record-based providers?
    /** @since DataProvider 2.6
    */
    bool children_can_support_records = False;

    #! Can any child data providers support the observer pattern / event API?
    /** @since DataProvider 2.6
    */
    bool children_can_support_observers = False;

    #! Can any child data providers support messages?
    /** @since DataProvider 2.7
    */
    bool children_can_support_messages = False;

    #! Can any child data providers support transaction management?
    /** @since DataProvider 2.8
    */
    bool children_can_support_transactions = False;

    #! Are all children always of the same data provider class that support the same options?
    /** @since DataProvider 3.0
    */
    bool children_identical = False;

    #! Returns the schema type supported by this data provider
    /**
        @since DataProvider 2.2
    */
    *string schema_type;

    #! Constructor options supported by the data provider for the constructor variant taking a hash
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    *hash<string, hash<DataProviderOptionInfo>> constructor_options;

    #! Required constructor option combinations
    /** If set it indicates that there are multiple possibilities for the set of required options for the data
        provider's hash constructor (if any); the format of this string is the required option names separated by
        commas (\c ","); multiple possibilities must be separated by pipe chars (\c "|""); ex:
        \c "client_id,client_secret,tenant|token";if this option is set, then the \c required attribute of options is
        ignored
    */
    *string required_constructor_options;

    #! Create options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    *hash<string, hash<DataProviderOptionInfo>> create_options;

    #! Upsert options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    *hash<string, hash<DataProviderOptionInfo>> upsert_options;

    #! Search options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    *hash<string, hash<DataProviderOptionInfo>> search_options;

    #! Request options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type
    */
    *hash<string, hash<DataProviderOptionInfo>> request_options;

    #! Child creation options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> child_create_options;

    #! Child deletion options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> child_delete_options;

    #! Add field options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> add_field_options;

    #! Update field options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> update_field_options;

    #! Delete field options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> delete_field_options;

    #! Send message options supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.5
    */
    *hash<string, hash<DataProviderOptionInfo>> send_message_options;

    #! Expressions (operators and functions) supported by the data provider
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.3
    */
    *hash<string, hash<DataProviderExpressionInfo>> expressions;

    #! hash of observable events and event types
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.5
    */
    *hash<string, hash<DataProviderMessageInfo>> events;

    #! List of output message IDs and message types
    /** @note this is a static attribute; it is the same for all data providers of the same type

        @since DataProvider 2.5
    */
    *hash<string, hash<DataProviderMessageInfo>> messages;

    #! Supported search logic capabilities
    /** This is a bitfield of capabilities; see @ref logic_capability_codes for more info
    */
    int search_logic_capabilities = LC_All;

    #! A hash of mapper key information
    /** @note
        - this is a static attribute; it is the same for all data providers of the same type
        - the \a handler keys are not returned as they are only valid at runtime
    */
    hash<string, hash<MapperRuntimeKeyInfo>> mapper_keys = Mapper::MapperKeyInfo;

    #! Connection event for event-based connection-capable data providers
    /** @since DataProvider 3.0
    */
    *string connection_event;

    #! Disconnection event for event-based connection-capable data providers
    /** @since DataProvider 3.0
    */
    *string disconnection_event;

    #! A list of child data providers in this data provider
    /** @note this is a dynamic attribute; it differs for different data providers of the same type
    */
    *list<string> children;

    #! Any optional provider-specific information
    *hash<auto> info;

    #! Does the data provider support being used as a pipeline processor?
    /** @since %DataProvider 3.3
    */
    bool supports_pipeline_processor = False;

    #! Processor capability bitfield (DPC_* constants)
    /** @since %DataProvider 3.3
    */
    int processor_capabilities = 0;

    #! Pipeline processor options, if applicable
    /** @since %DataProvider 3.3
    */
    *hash<string, hash<DataProviderOptionInfo>> pipeline_processor_options;

    #! Can any child data providers support being used as pipeline processors?
    /** @since %DataProvider 3.3
    */
    bool children_can_support_pipeline_processors = False;
}

#! Data provider connection info
public hashdecl DataProviderConnectionInfo {
    #! Is the underlying transport connected?
    bool connected;

    #! Is a reconnect in progress?
    bool reconnecting;

    #! Is auto reconnect supported?
    bool auto_reconnect;

    #! Any target URL if available
    *string url;

    #! Additional data-provider-dependent connection information
    *hash<auto> info;
}

#! Data provider generic expression map
public const DataProviderGenericExpressions = cast<hash<string, hash<DataProviderExpressionInfo>>>(
    map {$1.key: $1.value."exp"}, AbstractDataProvider::GenericExpressionImplementations.pairIterator()
);

#! Regular expression options
public const RegexOpts = {
    "RE_Caseless": RE_Caseless,
    "RE_DotAll": RE_DotAll,
    "RE_Extended": RE_Extended,
    "RE_MultiLine": RE_MultiLine,
    "RE_Unicode": RE_Unicode,
    "RE_Global": RE_Global,
};

#! Regular expression argument info
public const RegularExpressionArgInfo = (
    <DataProviderArgInfo>{
        "display_name": "String Value",
        "short_desc": "The string value to be matched to the next argument",
        "desc": "The string value to be matched to the next argument",
        "label_after": "~= /",
    },
    <DataProviderArgInfo>{
        "display_name": "Regular Expression",
        "short_desc": "The regular expression to use to match with the first argument",
        "desc": "The regular expression to use to match with the first argument",
        "example_value": "^[A-Z][0-9]+$",
        "label_after": "/",
    },
    <DataProviderArgInfo>{
        "display_name": "Regex Options",
        "short_desc": "The regular expression options to use when performing the match",
        "desc": "The regular expression options to use when performing the match",
        "allowed_values": (
            <AllowedValueInfo>{
                "display_name": "Dot Matches Newline?",
                "short_desc": "The dot '.' character matches also a newline",
                "desc": "The dot `.` character matches also a newline",
                "value": "RE_DotAll",
            },
            <AllowedValueInfo>{
                "display_name": "Extended Regex?",
                "short_desc": "Ignores whitespace characters and enables comments prefixed by '#'",
                "desc": "Ignores whitespace characters and enables comments prefixed by `#`",
                "value": "RE_Extended",
            },
            <AllowedValueInfo>{
                "display_name": "Full Unicode?",
                "short_desc": "Character matches match the full Unicode character set",
                "desc": "Character matches match the full Unicode character set",
                "value": "RE_Unicode",
            },
            <AllowedValueInfo>{
                "display_name": "Ignore Case?",
                "short_desc": "Ignore case when matching",
                "desc": "Ignore case when matching",
                "value": "RE_Caseless",
            },
            <AllowedValueInfo>{
                "display_name": "Multiline?",
                "short_desc": "Makes start-of-line '^' or end-of-line '$' match after or before any "
                    "newline in the subject string",
                "desc": "Makes start-of-line `^` or end-of-line `$` match after or before any "
                    "newline in the subject string",
                "value": "RE_MultiLine",
            },
            <AllowedValueInfo>{
                "display_name": "Global?",
                "short_desc": "Match all occurrences, not just the first",
                "desc": "Match all occurrences, not just the first",
                "value": "RE_Global",
            },
        ),
    },
);

#! Returns all member names of a typed hash including inherited members
/** @param th the TypedHash to get member names from
    @return a list of all member names including inherited members

    @since %DataProvider 3.1
*/
list<string> sub dp_get_all_typed_hash_member_names(TypedHash th) {
    list<string> rv = map $1.getName(), th.getMembers();
    *TypedHash parent = th.getParentTypedHash();
    if (parent) {
        rv += dp_get_all_typed_hash_member_names(parent);
    }
    return rv;
}

#! The AbstractDataProvider class
public class AbstractDataProvider inherits Logger::LoggerWrapper, Qore::Serializable, AbstractDataProcessor {
    public {
        #! Generic expression implementations for data providers without native search functionality
        const GenericExpressionImplementations = {
            DP_OP_AND: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "subtype": DES_LogicOperator,
                    "name": DP_OP_AND,
                    "display_name": "and (&&)",
                    "short_desc": "Returns True if all arguments are True with logic short-circuiting",
                    "desc": "Returns `True` if all arguments are `True`; supports logic short-circuiting",
                    "symbol": "&&",
                    "groups": "Logical",
                    "args": (DataProviderSignatureBoolType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Boolean",
                            "short_desc": "A boolean expression or value",
                            "desc": "A boolean expression or value; all conditions must be `True` for the "
                                "overall expression to return `True`",
                        },
                    ),
                    "varargs": True,
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    foreach auto arg in (exp.args) {
                        if (!AbstractDataProvider::evalGenericExpressionValue(rec, arg, template_context)) {
                            return False;
                        }
                    }
                    return True;
                },
            },
            DP_OP_OR: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "subtype": DES_LogicOperator,
                    "name": DP_OP_OR,
                    "display_name": "or (||)",
                    "short_desc": "Returns True if at least one argument is True with logic short-circuiting",
                    "desc": "Returns `True` if at least one argument is `True`; supports logic short-circuiting",
                    "symbol": "||",
                    "groups": "Logical",
                    "args": (DataProviderSignatureBoolType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Boolean",
                            "short_desc": "A boolean expression or value",
                            "desc": "A boolean expression or value; at least one condition must be `True` for the "
                                "overall expression to return `True`",
                        },
                    ),
                    "varargs": True,
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    foreach auto arg in (exp.args) {
                        if (AbstractDataProvider::evalGenericExpressionValue(rec, arg, template_context)) {
                            return True;
                        }
                    }
                    return False;
                },
            },
            DP_SEARCH_OP_NOT: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_NOT,
                    "display_name": "logical not (!)",
                    "short_desc": "This operator reverses the logcal value of the argument",
                    "desc": "This operator reverses the logcal value of the argument",
                    "symbol": "!",
                    "groups": "Logical",
                    "args": (DataProviderSignatureBoolType,),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Boolean",
                            "short_desc": "A boolean expression or value",
                            "desc": "A boolean expression or value; the operator will negate the value of the "
                                "expression",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return (!AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context));
                },
            },
            DP_SEARCH_OP_EQ: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_EQ,
                    "display_name": "equals (==)",
                    "short_desc": "Returs True if the two arguments are logically equal; types are converted if "
                        "necessary",
                    "desc": "Returs `True` if the two arguments are logically equal; types are converted if "
                        "necessary",
                    "symbol": "==",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; both values must be equal for the "
                                "overall expression to return `True`",
                            "label_after": "==",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; both values must be equal for the "
                                "overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    auto arg0 = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    auto arg1 = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                    # for backwards compatibility, if the first argument is a field reference, and the second is a
                    # hash
                    return arg0 == arg1;
                },
            },
            DP_SEARCH_OP_NE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_NE,
                    "display_name": "not equals (!=)",
                    "short_desc": "Returns True if the arguments are not equal",
                    "desc": "Returns `True` if the arguments are not equal; type conversions are performed",
                    "symbol": "!=",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; both values must not be equal for the "
                                "overall expression to return `True`",
                            "label_after": "!=",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; both values must not be equal for the "
                                "overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context)
                        != AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                },
            },
            DP_SEARCH_OP_LT: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_LT,
                    "display_name": "less than (<)",
                    "short_desc": "Returns True if the first argument is strictly less than the second argument",
                    "desc": "Returns `True` if the first argument is strictly less than the second argument; type "
                        "conversions are performed if necessary",
                    "symbol": "<",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be strictly less than the second value "
                                "for the overall expression to return `True`",
                            "label_after": "<",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be strictly less than the second value "
                                "for the overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context)
                        < AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                },
            },
            DP_SEARCH_OP_LE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_LE,
                    "display_name": "less than or equals (<=)",
                    "short_desc": "Returns True if the first argument is less than or equal to the second argument",
                    "desc": "Returns `True` if the first argument is less than or equal to the second argument; type "
                        "conversions are performed if necessary",
                    "symbol": "<=",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be less than or equal to the second "
                                "value for the overall expression to return `True`",
                            "label_after": "<=",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be less than or equal to the second "
                                "value for the overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context)
                        <= AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                },
            },
            DP_SEARCH_OP_GT: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_GT,
                    "display_name": "greater than (>)",
                    "short_desc": "Returns True if the first argument is strictly greater than the second argument",
                    "desc": "Returns `True` if the first argument is strictly greater than the second argument; type "
                        "conversions are performed if necessary",
                    "symbol": ">",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be strictly greater than the second "
                                "value for the overall expression to return `True`",
                            "label_after": ">",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be strictly greater than the second "
                                "value for the overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context)
                        > AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                },
            },
            DP_SEARCH_OP_GE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_GE,
                    "display_name": "greater than or equals (>=)",
                    "short_desc": "Returns True if the first argument is greater than or equal to the second argument",
                    "desc": "Returns `True` if the first argument is greater than or equal to the second argument; "
                        "type conversions are performed if necessary",
                    "symbol": ">=",
                    "groups": "Comparison",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be greater than or equal to the second "
                                "value for the overall expression to return `True`",
                            "label_after": ">=",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; the first value must be greater than or equal to the second "
                                "value for the overall expression to return `True`",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context)
                        >= AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                },
            },
            DP_SEARCH_OP_BETWEEN: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_BETWEEN,
                    "display_name": "between",
                    "short_desc": "Returns True if the first argument is greater than the second and less than the "
                        "third",
                    "desc": "Returns `True` if the first argument is greater than the second and less than the third",
                    "symbol": "between",
                    "groups": "Comparison",
                    "render_template": "$arg[0] between $arg[1] and $arg[2]",
                    "args": (
                        DataProviderSignatureAnyType,
                        DataProviderSignatureAnyType,
                        DataProviderSignatureAnyType,
                    ),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to compare",
                            "desc": "A value to compare; first value must be greater than the second and less than "
                                "the third for the overall expression to return `True`",
                            "label_after": "between",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Lower Bound",
                            "short_desc": "The lower bound for the first argument",
                            "desc": "The lower bound for the first argument",
                            "label_after": "and",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Upper Bound",
                            "short_desc": "The upper bound for the first argument",
                            "desc": "The upper bound for the first argument",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    auto arg0 = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    return arg0 > AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context)
                        && arg0 < AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[2], template_context);
                },
            },
            DP_SEARCH_OP_IN: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_IN,
                    "display_name": "in",
                    "short_desc": "Returns True if the first argument is found in the second argument",
                    "desc": "Returns `True` if the first argument is found in the second argument",
                    "symbol": "in",
                    "groups": "Comparison",
                    "render_template": "$arg[0] in ($arg[1])",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "arg_info": (
                        <DataProviderArgInfo>{
                            "display_name": "Value",
                            "short_desc": "A value to look for (needle)",
                            "desc": "A value to look for; first value must be found in the second for the overall "
                                "expression to return `True`",
                            "label_after": "in",
                        },
                        <DataProviderArgInfo>{
                            "display_name": "Search Source",
                            "short_desc": "The value to look for the value in (haystack)",
                            "desc": "The value to look for the value in (haystack)",
                        },
                    ),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return inlist(AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context),
                        AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context));
                },
            },
            DP_SEARCH_OP_REGEX: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_SEARCH_OP_REGEX,
                    "display_name": "regular expression match",
                    "desc": "regular expression operator; the first argument will be processed with the second as a "
                        "regular expression pattern",
                    "symbol": "regex",
                    "groups": "Comparison",
                    "render_template": "$arg[0] =~ /$arg[1]/$arg[2]",
                    "args": (
                        DataProviderSignatureStringType,
                        DataProviderSignatureStringType,
                        DataProviderSignatureOptionalStringListValueType,
                    ),
                    "arg_info": RegularExpressionArgInfo,
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    string str = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    string pattern = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                    int opts;
                    if (*list<auto> options = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[2], template_context)) {
                        opts = foldl $1 | $2, (map RegexOpts{$1}, options);
                    }
                    return regex(str, pattern, opts);
                },
            },
            DP_EXPR_VALUE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_EXPR_VALUE,
                    "display_name": "value",
                    "short_desc": "Evaluates and returns a field reference value",
                    "desc": "Evaluates the given field reference and returns its value. "
                        "This allows field references to be used as standalone expressions.",
                    "symbol": "",
                    "args": (DataProviderSignatureAnyType,),
                    "return_type": AbstractDataProviderTypeMap."any",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                },
            },
            DP_EXPR_TEMPLATE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": DP_EXPR_TEMPLATE,
                    "display_name": "template",
                    "short_desc": "Evaluates and returns a template reference value",
                    "desc": "Evaluates the given template reference via the registered callback and returns "
                        "its value. This allows template references to be used as standalone expressions.",
                    "symbol": "",
                    "args": (DataProviderSignatureAnyType,),
                    "return_type": AbstractDataProviderTypeMap."any",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    return AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                },
            },
            DP_FUNC_INDEX: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Function,
                    "role": ER_All,
                    "name": DP_FUNC_INDEX,
                    "display_name": "index",
                    "short_desc": "Returns a single element from a list by index",
                    "desc": "Returns a single element from a list by index; supports negative indices",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureIntType),
                    "return_type": AbstractDataProviderTypeMap."any",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    auto value = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    int index = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[1], template_context);
                    if (value.typeCode() != NT_LIST) {
                        throw "INVALID-OPERATION", sprintf("index accessor [%d] applied to non-list value of "
                            "type %y", index, value.fullType());
                    }
                    int len = value.size();
                    int idx = index >= 0 ? index : len + index;
                    if (idx < 0 || idx >= len) {
                        return NOTHING;
                    }
                    return value[idx];
                },
            },
            DP_FUNC_SLICE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Function,
                    "role": ER_All,
                    "name": DP_FUNC_SLICE,
                    "display_name": "slice",
                    "short_desc": "Returns a sub-list from a list by indices and/or ranges",
                    "desc": "Returns a sub-list from a list by indices and/or ranges; supports negative indices "
                        "and open-ended ranges",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureAnyType),
                    "varargs": True,
                    "return_type": AbstractDataProviderTypeMap."any",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    auto value = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    if (value.typeCode() != NT_LIST) {
                        throw "INVALID-OPERATION", sprintf("slice accessor applied to non-list value of "
                            "type %y", value.fullType());
                    }
                    int len = value.size();
                    list<auto> result = ();
                    for (int i = 1; i < exp.args.size(); ++i) {
                        auto item = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[i], template_context);
                        if (item.typeCode() == NT_HASH) {
                            hash<auto> range = item;
                            int start = exists range.start
                                ? (range.start >= 0 ? range.start : len + range.start) : 0;
                            int end = exists range.end
                                ? (range.end >= 0 ? range.end : len + range.end) : len - 1;
                            if (start < 0) start = 0;
                            if (end >= len) end = len - 1;
                            for (int j = start; j <= end; ++j) {
                                result += value[j];
                            }
                        } else {
                            int idx = item >= 0 ? item : len + item;
                            if (idx >= 0 && idx < len) {
                                result += value[idx];
                            }
                        }
                    }
                    return result;
                },
            },
            DP_FUNC_KEY: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Function,
                    "role": ER_All,
                    "name": DP_FUNC_KEY,
                    "display_name": "key",
                    "short_desc": "Returns hash values by key",
                    "desc": "Returns hash values by one or more keys; for a single key returns the value directly; "
                        "for multiple keys returns a hash subset; when applied to a list, maps over each element",
                    "args": (DataProviderSignatureAnyType, DataProviderSignatureStringType),
                    "varargs": True,
                    "return_type": AbstractDataProviderTypeMap."any",
                },
                "impl": auto sub (hash<auto> rec, hash<DataProviderExpression> exp, *hash<auto> template_context) {
                    auto value = AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[0], template_context);
                    list<string> key_list = ();
                    for (int i = 1; i < exp.args.size(); ++i) {
                        key_list += AbstractDataProvider::evalGenericExpressionValue(rec, exp.args[i], template_context);
                    }
                    return AbstractDataProvider::applyKeyAccessorImpl(value, key_list);
                },
            },
        };

        #! Generic search operator expressions
        /** The following are generic search operator expressions implemented for all data providers that do not
            provide native search functionality

            @since DataProvider 2.3
        */
        const GenericExpressions = map {$1.key: $1.value.exp}, GenericExpressionImplementations.pairIterator();

        #! A list of member names of the DataProviderSummaryInfo hashdecl (including inherited members)
        const DataProviderSummaryInfoKeys = dp_get_all_typed_hash_member_names(
            TypedHash::forName("DataProviderSummaryInfo"));

        #! 'Requires Result' option info
        const RequiresResultOption = <DataProviderOptionInfo>{
            "display_name": "Requires Result",
            "short_desc": "If True, the operation requires a result or affected records",
            "desc": "If `True`, then an operation that returns no result or affects no records results in an error",
            "type": AbstractDataProviderTypeMap."bool",
        };

        #! Generic record search options; see details below
        /** Each record-based data provider should support the following search options:
            - \c columns: a list of one or more field names; allows a subset of fields to be output in searches
            - \c limit: the maximum number of records to return
            - \c max_records: the maximum number of records to return or to affect; if more records are returned
              or affected, the operation results in an error; for data provider supporting transactions; this will
              normally result in a transaction rollback if a transaction is in progress
            - \c offset: the offset number in records to return
            - \c requires_result: if \c True, then an operation that returns no result or affects no records
              results in an error
        */
        const GenericRecordSearchOptions = {
            "columns": <DataProviderOptionInfo>{
                "display_name": "Columns",
                "short_desc": "Columns to include in output",
                "desc": "Allows a subset of fields to be output in searches",
                "type": AbstractDataProviderTypeMap."any",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "The limit on the number of records to return from the data provider",
                "desc": "The limit on the number of records to return from the data provider; additional "
                    "records are silently ignored. This option is normally enforced by the server interfaced by the "
                    "data provider",
                "type": AbstractDataProviderTypeMap."int",
            },
            "max_records": <DataProviderOptionInfo>{
                "display_name": "Max Records",
                "short_desc": "The maximum number of records to return or to affect",
                "desc": "The maximum number of records to return or to affect; if more records are returned or "
                    "affected, the operation results in an error; for data provider supporting transactions; this "
                    "will normally result in a transaction rollback if a transaction is in progress. This option is "
                    "noormally enforced externally from the server interfaced by the data provider",
                "type": AbstractDataProviderTypeMap."int",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "The offset in the record list for records to return",
                "desc": "The offset in the record list for records to return",
                "type": AbstractDataProviderType::get(IntType),
            },
            "requires_result": RequiresResultOption,
        };
    }

    private {
        #! flag if callbacks are locked
        static bool callbacks_locked = False;

        #! static callback the returns a bool if the value needs dynamic resolution
        static code cb_value_needs_resolution;

        #! static callback for dynamic value resolution
        static code cb_resolve_value;

        #! static callback for template reference expansion
        /** @since DataProvider 3.3
        */
        static *code<auto(string, string, *hash<auto>)> cb_expand_template;

        #! static callback for resolving template reference types
        /** @since DataProvider 3.3
        */
        static *code<*AbstractDataProviderType(string, string, *hash<auto>)> cb_resolve_template_type;

        #! static callback for listing available template contexts
        /** @since DataProvider 3.3
        */
        static *code<auto(*hash<auto>)> cb_list_template_contexts;

        #! static callback for listing available template values for a context
        /** @since DataProvider 3.3
        */
        static *code<auto(string, string, *hash<auto>)> cb_list_template_values;

        #! flag if template callbacks are locked
        static bool template_callbacks_locked = False;
    }

    private:internal {
        #! Optional debug info collector
        /** @since DataProvider 3.3
        */
        *AbstractDebugInfoCollector debug_collector;
    }

    #! Creates the data provider
    constructor() {
        DataProvider::checkStaticInit();
    }

    #! Creates the data provider with the given Logger
    constructor(LoggerInterface logger) : LoggerWrapper(logger) {
        DataProvider::checkStaticInit();
    }

    #! Returns static provider information as data; no objects are returned
    /** @param with_type_info if @ref True then more type information will be returned in the response

        @note the \c name and \c children attributes are not returned as they are dynamic attributes
    */
    hash<auto> getInfoAsData(*bool with_type_info) {
        return DataProvider::getInfoAsData(getInfo(), with_type_info);
    }

    #! Returns data provider info
    hash<DataProviderInfo> getInfo() {
        hash<DataProviderInfo> rv();
        rv += getStaticInfoImpl() + {
            "children": getChildProviderNames(),
        };
        if (!rv.name) {
            rv.name = getName();
        }
        if (!rv.display_name) {
            rv.display_name = getDisplayName();
        }
        if (!rv.short_desc) {
            rv.short_desc = getShortDesc() ?? "no description available";
        }
        if (!rv.desc) {
            rv.desc = getDesc() ?? "no description available";
        }
        if (!exists rv.has_record) {
            rv.has_record = rv.supports_read;
        }
        if (!rv.events && rv.supports_observable) {
            rv.events = getEventTypes();
        }
        if (!rv.messages && rv.supports_messages != MSG_None) {
            rv.messages = getMessageTypes();
        }
        if (!rv.info) {
            rv.info = getAdditionalInfo();
        }
        return rv;
    }

    #! Return data provider summary info
    hash<DataProviderSummaryInfo> getSummaryInfo() {
        hash<DataProviderInfo> info = getStaticInfoImpl();
        if (!info.name) {
            info.name = getName();
        }
        if (!info.display_name) {
            info.display_name = getDisplayName();
        }
        if (!info.short_desc) {
            info.short_desc = getShortDesc() ?? "no description available";
        }
        if (!info.desc) {
            info.desc = getDesc() ?? "no description available";
        }
        return cast<hash<DataProviderSummaryInfo>>(info{DataProviderSummaryInfoKeys});
    }

    #! Returns the data provider display name
    /** @since DataProvider 3.0
    */
    string getDisplayName() {
        hash<DataProviderInfo> info = getStaticInfoImpl();
        return info.display_name ?? AbstractDataField::getDisplayName(info.name ?? getName());
    }

    #! Returns the data provider short description
    /** @since DataProvider 3.0
    */
    *string getShortDesc() {
        hash<DataProviderInfo> info = getStaticInfoImpl();
        return info.short_desc ?? AbstractDataField::getShortDescription(info.desc ?? getDesc());
    }

    #! Returns the data provider description
    *string getDesc() {
        return getStaticInfoImpl().desc;
    }

    #! Returns additional provider-specific information
    /** @since DataProvider 3.0
    */
    *hash<auto> getAdditionalInfo() {
    }

    #! Creates the given record in the data provider from create action options
    /** @param options create action options

        @return the data written to the data provider

        @throw INVALID-OPERATION the data provider does not support record creation
        @throw CREATE-RECORD-ERROR the data provider raised an exception in the record creation operation
        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record

        @since DataProvider 3.3
    */
    *hash<auto> createRecordWithOptions(*hash<auto> options) {
        return createRecord(options.rec, options - "rec");
    }

    #! Creates the given record in the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options; will be processed by validateCreateOptions()

        @return the data written to the data provider

        @throw INVALID-OPERATION the data provider does not support record creation
        @throw CREATE-RECORD-ERROR the data provider raised an exception in the record creation operation
        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record
    */
    *hash<auto> createRecord(hash<auto> rec, *hash<auto> create_options) {
        checkCreate();
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "CREATE-RECORD-ERROR") {
                rethrow "CREATE-RECORD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        return createRecordImpl(rec, validateCreateOptions(create_options));
    }

    #! Creates the given records in the data provider from create action options
    /** @param options create action options

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record

        @since DataProvider 3.3
    */
    *hash<auto> createRecordsBulkWithOptions(*hash<auto> options) {
        return createRecordsBulk(options.recs, options - "recs");
    }

    #! Creates the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are field names and values are either
        lists of field values or a single value that applies to all records
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record
    */
    *hash<auto> createRecordsBulk(hash<auto> recs, *hash<auto> create_options) {
        checkCreate();
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "CREATE-RECORD-ERROR") {
                rethrow "CREATE-RECORD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        create_options = validateCreateOptions(create_options);
        return createRecordsBulkIntern(recs, create_options);
    }

    #! Creates the given records in the data provider
    /** @param recs a list of hashes representing input records, where the keys are field names and values are field
        values
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record
    */
    *hash<auto> createRecordsBulk(list<auto> recs, *hash<auto> create_options) {
        hash<auto> h;
        foreach auto rec in (recs) {
            if (rec.typeCode() != NT_HASH) {
                throw "CREATE-RECORD-ERROR", sprintf("Each record must be a hash; got type %y instead", rec.fullType());
            }
            if (!h) {
                h = map {$1: ()}, keys rec;
            }
            map h{$1.key} += $1.value, rec.pairIterator();
        }
        return createRecordsBulk(h, create_options);
    }

    #! Creates the given records in the data provider
    /** @param stream a record stream for insert
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record

        @since DataProvider 3.3
    */
    *hash<auto> createRecordsBulk(AbstractDataProviderBulkRecordInterface stream, *hash<auto> create_options) {
        checkCreate();
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "CREATE-RECORD-ERROR") {
                rethrow "CREATE-RECORD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        create_options = validateCreateOptions(create_options);
        hash<auto> rv;
        while (*hash<auto> recs = stream.getValue()) {
            *hash<auto> r = createRecordsBulkIntern(recs, create_options);
            if (!rv) {
                rv = r;
            } else {
                map rv{$1.key} += $1.value, r.pairIterator();
            }
        }
        return rv;
    }

    #! Creates the given records in the data provider
    /** @param i a record iterator for insert
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record

        @since DataProvider 3.3
    */
    *hash<auto> createRecordsBulk(HashListIterator i, *hash<auto> create_options) {
        checkCreate();
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "CREATE-RECORD-ERROR") {
                rethrow "CREATE-RECORD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        create_options = validateCreateOptions(create_options);
        hash<auto> rv;
        while (i.next()) {
            hash<auto> recs = i.getValue();
            *hash<auto> r = createRecordsBulkIntern(recs, create_options);
            if (!rv) {
                rv = r;
            } else {
                map rv{$1.key} += $1.value, r.pairIterator();
            }
        }
        return rv;
    }

    #! Upserts the given record in the data provider from upsert action options
    /** @param options upsert action options

        @return see UpsertResultInserted and related constants for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations

        @since DataProvider 3.3
    */
    string upsertRecordWithOptions(*hash<auto> options) {
        return upsertRecord(options.rec, options - "rec");
    }

    #! Upserts the given record in the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return see UpsertResultInserted and related constants for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations
    */
    string upsertRecord(hash<auto> rec, *hash<auto> upsert_options) {
        checkUpsert();
        return upsertRecordImpl(rec, validateUpsertOptions(upsert_options));
    }

    #! Upserts the given records in the data provider from upsert action options
    /** @param options upsert action options

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations

        @since DataProvider 3.3
    */
    *list<string> upsertRecordsBulkWithOptions(*hash<auto> options) {
        return upsertRecordsBulk(options.recs, options - "recs");
    }

    #! Upserts the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are field names and values are either
        lists of field values or a single value that applies to all records to be upserted
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations
    */
    *list<string> upsertRecordsBulk(hash<auto> recs, *hash<auto> upsert_options) {
        checkUpsert();
        upsert_options = validateUpsertOptions(upsert_options);
        if (!supportsBulkUpsert()) {
            return map upsertRecordImpl($1, upsert_options), recs.contextIterator();
        }
        return upsertRecordsBulkImpl(recs, upsert_options);
    }

    #! Creates the given records in the data provider
    /** @param recs a list of hashes representing input records, where the keys are field names and values are field
        values
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations
    */
    *list<string> upsertRecordsBulk(list<auto> recs, *hash<auto> upsert_options) {
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "CREATE-RECORD-ERROR") {
                rethrow "CREATE-RECORD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        hash<auto> h;
        foreach auto rec in (recs) {
            if (rec.typeCode() != NT_HASH) {
                throw "CREATE-RECORD-ERROR", sprintf("Each record must be a hash; got type %y instead", rec.fullType());
            }
            if (!h) {
                h = map {$1: ()}, keys rec;
            }
            map h{$1.key} += $1.value, rec.pairIterator();
        }
        return upsertRecordsBulk(h, upsert_options);
    }

    #! Upserts the given records in the data provider
    /** @param stream a record stream for upserting records
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations

        @since DataProvider 3.3
    */
    *list<string> upsertRecordsBulk(AbstractDataProviderBulkRecordInterface stream, *hash<auto> upsert_options) {
        checkUpsert();
        upsert_options = validateUpsertOptions(upsert_options);
        list<string> rv;
        while (*hash<auto> recs = stream.getValue()) {
            if (!supportsBulkUpsert()) {
                rv += map upsertRecordImpl($1, upsert_options), recs.contextIterator();
            } else {
                rv += upsertRecordsBulkImpl(recs, upsert_options);
            }
        }
        return rv;
    }

    #! Upserts the given records in the data provider
    /** @param i a record iterator for upserting records
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values

        @throw INVALID-OPERATION the data provider does not support upsert operations

        @since DataProvider 3.3
    */
    *list<string> upsertRecordsBulk(HashListIterator i, *hash<auto> upsert_options) {
        checkUpsert();
        upsert_options = validateUpsertOptions(upsert_options);
        list<string> rv;
        while (i.next()) {
            hash<auto> recs = i.getValue();
            if (!supportsBulkUpsert()) {
                rv += map upsertRecordImpl($1, upsert_options), recs.contextIterator();
            } else {
                rv += upsertRecordsBulkImpl(recs, upsert_options);
            }
        }
        return rv;
    }

    #! Returns the first record matching the search action options
    /** @param options search action options

        @throw INVALID-OPERATION the data provider does not support reading
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 3.3
    */
    *hash<auto> searchFirstRecordWithOptions(*hash<auto> options) {
        return searchFirstRecord(options.where_cond, options - "where_cond");
    }

    #! Returns the first record matching the DPQL search expression
    /** @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 3.3
    */
    *hash<auto> searchFirstRecord(string dpql_where, *hash<auto> search_options) {
        return searchFirstRecord(DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Returns the first record matching the search options
    /** @param where_cond the search criteria; will be processed by processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 2.3
    */
    *hash<auto> searchFirstRecord(hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        *hash<auto> rv = searchFirstRecordImpl(processSearchParameters(where_cond, search_options), search_options);
        if (!rv && search_options.requires_result) {
            error("NO-DATA", "No record found for search: %y", where_cond);
        }
        return rv;
    }

    #! Returns the first record matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw NO-DATA the \c required_result search option was set and no record was found
    */
    *hash<auto> searchFirstRecord(hash<auto> where_cond, *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        *hash<auto> rv = searchFirstRecordImpl(getSearchExpression(where_cond, search_options), search_options);
        if (!rv && search_options.requires_result) {
            error("NO-DATA", "No record found for search: %y", where_cond);
        }
        return rv;
    }

    #! Returns a single record matching the search action options
    /** @param options search action options

        @throw INVALID-OPERATION the data provider does not support reading
        @throw MULTIPLE-RECORDS-ERROR multiple records found
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 3.3
    */
    *hash<auto> searchSingleRecordWithOptions(*hash<auto> options) {
        return searchSingleRecord(options.where_cond, options - "where_cond");
    }

    #! Returns a single record matching the DPQL search expression
    /** @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw MULTIPLE-RECORDS-ERROR multiple records found
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 3.3
    */
    *hash<auto> searchSingleRecord(string dpql_where, *hash<auto> search_options) {
        return searchSingleRecord(DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Returns a single record matching the search options
    /** @param where_cond the search criteria; will be processed by processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw MULTIPLE-RECORDS-ERROR multiple records found
        @throw NO-DATA the \c required_result search option was set and no record was found

        @since DataProvider 2.3
    */
    *hash<auto> searchSingleRecord(hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        *hash<auto> rv = searchSingleRecordImpl(processSearchParameters(where_cond, search_options), search_options);
        if (!rv && search_options.requires_result) {
            error("NO-DATA", "No record found for search: %y", where_cond);
        }
        return rv;
    }

    #! Returns a single record matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
        @throw MULTIPLE-RECORDS-ERROR multiple records found
        @throw NO-DATA the \c required_result search option was set and no record was found
    */
    *hash<auto> searchSingleRecord(*hash<auto> where_cond, *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        *hash<auto> rv = searchSingleRecordImpl(getSearchExpression(where_cond, search_options), search_options);
        if (!rv && search_options.requires_result) {
            error("NO-DATA", "No record found for search: %y", where_cond);
        }
        return rv;
    }

    #! Returns an iterator iterating all records
    /** @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
    */
    AbstractDataProviderRecordIterator getRecordIterator(*hash<auto> search_options) {
        checkRead();
        return searchRecords(NOTHING, validateSearchOptions(search_options));
    }

    #! Returns an iterator iterating all records with the bulk read API
    /** @param block_size the number of records in a read block; must be a positive number
        @param search_options the search options; will be processed by validateSearchOptions()

        @return a bulk record interface object that will return the records in bulk format

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading
    */
    AbstractDataProviderBulkRecordInterface getBulkRecordInterface(int block_size = 1000,
            *hash<auto> search_options) {
        checkRead();
        return searchRecordsBulk(block_size, NOTHING, validateSearchOptions(search_options));
    }

    #! Returns an iterator for zero or more records matching the search action options
    /** @param options search action options

        @return a bulk record interface object that will return the records in bulk format

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 3.3
    */
    AbstractDataProviderBulkRecordInterface searchRecordsBulkWithOptions(*hash<auto> options) {
        return searchRecordsBulk(options.block_size, options.where_cond, options - ("block_size", "where_cond"));
    }

    #! Returns an iterator for zero or more records matching the DPQL search expression
    /** @param block_size the number of records in a read block; must be a positive number
        @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @return a bulk record interface object that will return the records in bulk format

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 3.3
    */
    AbstractDataProviderBulkRecordInterface searchRecordsBulk(int block_size = 1000,
            string dpql_where, *hash<auto> search_options) {
        return searchRecordsBulk(block_size, DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria; will be processed by processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return a bulk record interface object that will return the records in bulk format

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 2.3
    */
    AbstractDataProviderBulkRecordInterface searchRecordsBulk(int block_size = 1000,
            *hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkRead();
        if (block_size <= 0) {
            throw "INVALID-BLOCK-SIZE", sprintf("block_size %d is invalid; the block size mst be a positive number",
                block_size);
        }
        search_options = validateSearchOptions(search_options);
        return searchRecordsBulkImpl(block_size, processSearchParameters(where_cond, search_options),
            search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return a bulk record interface object that will return the records in bulk format

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading
    */
    AbstractDataProviderBulkRecordInterface searchRecordsBulk(int block_size = 1000, *hash<auto> where_cond,
            *hash<auto> search_options) {
        checkRead();
        if (block_size <= 0) {
            throw "INVALID-BLOCK-SIZE", sprintf("block_size %d is invalid; the block size mst be a positive number",
                block_size);
        }
        search_options = validateSearchOptions(search_options);
        return searchRecordsBulkImpl(block_size, getSearchExpression(where_cond, search_options), search_options);
    }

    #! Returns an iterator for zero or more records matching the search action options
    /** param options search action options

        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 3.3
    */
    AbstractDataProviderRecordIterator searchRecordsWithOptions(*hash<auto> options) {
        return searchRecords(options.where_cond, options - "where_cond");
    }

    #! Returns an iterator for zero or more records matching the DPQL search expression
    /** @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 3.3
    */
    AbstractDataProviderRecordIterator searchRecords(string dpql_where, *hash<auto> search_options) {
        return searchRecords(DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria; will be processed by processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading

        @since DataProvider 2.3
    */
    AbstractDataProviderRecordIterator searchRecords(*hash<DataProviderExpression> where_cond,
            *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        return searchRecordsImpl(processSearchParameters(where_cond, search_options), search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @throw INVALID-OPERATION the data provider does not support reading
    */
    AbstractDataProviderRecordIterator searchRecords(*hash<auto> where_cond, *hash<auto> search_options) {
        checkRead();
        search_options = validateSearchOptions(search_options);
        return searchRecordsImpl(getSearchExpression(where_cond, search_options), search_options);
    }

    #! Returns an iterator for zero or more records matching the search options according to an API request
    /** @param req the request to serialize and make according to the request type
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned

        @throw INVALID-OPERATION the data provider does not support reading records or the request / response API

        @since DataProvider 2.3
    */
    AbstractDataProviderRecordIterator requestSearchRecords(auto req, *hash<DataProviderExpression> where_cond,
            *hash<auto> search_options) {
        checkRead();
        checkRequest();
        return requestSearchRecordsImpl(req, processSearchParameters(where_cond, search_options), search_options);
    }

    #! Returns an iterator for zero or more records matching the search options according to an API request
    /** @param req the request to serialize and make according to the request type
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned

        @throw INVALID-OPERATION the data provider does not support reading records or the request / response API
    */
    AbstractDataProviderRecordIterator requestSearchRecords(auto req, *hash<auto> where_cond,
            *hash<auto> search_options) {
        checkRead();
        checkRequest();
        return requestSearchRecordsImpl(req, getSearchExpression(where_cond, search_options), search_options);
    }

    #! Updates a single record using update action options
    /** @param options update action options

        @returns @ref True if the record was updated, @ref False if not (no record found)

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records

        @since DataProvider 3.3
    */
    bool updateSingleRecordWithOptions(*hash<auto> options) {
        return updateSingleRecord(options.set, options.where_cond, options - ("set", "where_cond"));
    }

    #! Updates a single record matching the search options
    /** @param set the hash of field data to set; will be processed by processFieldValues()
        @param where_cond the search criteria; will be processed by processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @returns @ref True if the record was updated, @ref False if not (no record found)

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records

        @since DataProvider 2.3
    */
    bool updateSingleRecord(hash<auto> set, hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkUpdate();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "UPDATE-RECORDS-ERROR") {
                rethrow "UPDATE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        return updateSingleRecordImpl(processFieldValues(set, search_options),
            processSearchParameters(where_cond, search_options), search_options);
    }

    #! Updates a single record matching the search options
    /** @param set the hash of field data to set; will be processed by processFieldValues()
        @param where_cond the search criteria; will be processed by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @returns @ref True if the record was updated, @ref False if not (no record found)

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records
    */
    bool updateSingleRecord(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        checkUpdate();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "UPDATE-RECORDS-ERROR") {
                rethrow "UPDATE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        return updateSingleRecordImpl(processFieldValues(set, search_options),
            getSearchExpression(where_cond, search_options), search_options);
    }

    #! Updates zero or more records matching the update action options
    /** @param options update action options

        @return the number of records updated

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records

        @since DataProvider 3.3
    */
    int updateRecordsWithOptions(*hash<auto> options) {
        return updateRecords(options.set, options.where_cond, options - ("set", "where_cond"));
    }

    #! Updates zero or more records matching the DPQL search expression
    /** @param set the hash of field data to set
        @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records updated

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records

        @since DataProvider 3.3
    */
    int updateRecords(hash<auto> set, string dpql_where, *hash<auto> search_options) {
        return updateRecords(set, DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated; will be processed by
        processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records updated

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records

        @since DataProvider 2.3
    */
    int updateRecords(hash<auto> set, *hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkUpdate();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "UPDATE-RECORDS-ERROR") {
                rethrow "UPDATE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        int rc = updateRecordsImpl(processFieldValues(set, search_options),
            processSearchParameters(where_cond, search_options), search_options);
        if (!rc && search_options.requires_result) {
            throw "UPDATE-RECORDS-ERROR", "no records updated but 'requires_result' is True";
        }
        return rc;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated; will be processed by
        processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records updated

        @throw INVALID-OPERATION the data provider does not support record updating
        @throw UPDATE-RECORDS-ERROR error updating records
    */
    int updateRecords(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        checkUpdate();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DUPLICATE-RECORD" && $1.err != "UPDATE-RECORDS-ERROR") {
                rethrow "UPDATE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        int rc = updateRecordsImpl(processFieldValues(set, search_options),
            getSearchExpression(where_cond, search_options), search_options);
        if (!rc && search_options.requires_result) {
            throw "UPDATE-RECORDS-ERROR", "no records updated but 'requires_result' is True";
        }
        return rc;
    }

    #! Deletes zero or more records from delete action options
    /** @param options action options
        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record deletion
        @throw DELETE-RECORDS-ERROR error deleting records

        @since DataProvider 3.3
    */
    int deleteRecordsWithOptions(*hash<auto> options) {
        return deleteRecords(options.where_cond, options - "where_cond");
    }

    #! Deletes zero or more records matching the DPQL search expression
    /**
        @param dpql_where the DPQL search expression string (e.g. \c '\@name == "John"')
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record deletion
        @throw DELETE-RECORDS-ERROR error deleting records

        @since DataProvider 3.3
    */
    int deleteRecords(string dpql_where, *hash<auto> search_options) {
        return deleteRecords(DataProvider::parseDpqlExpression(dpql_where), search_options);
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted; will be processed by
        processSearchParameters()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record deletion
        @throw DELETE-RECORDS-ERROR error deleting records

        @since DataProvider 2.3
    */
    int deleteRecords(*hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        checkDelete();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DELETE-RECORDS-ERROR") {
                rethrow "DELETE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        int rc = deleteRecordsImpl(processSearchParameters(where_cond, search_options), search_options);
        if (!rc && search_options.requires_result) {
            throw "DELETE-RECORDS-ERROR", "no records deleted but 'requires_result' is True";
        }
        return rc;
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted; will be processed by
        processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record deletion
        @throw DELETE-RECORDS-ERROR error deleting records
    */
    int deleteRecords(*hash<auto> where_cond, *hash<auto> search_options) {
        checkDelete();
        search_options = validateSearchOptions(search_options);
        on_error {
            if ($1.err != "DELETE-RECORDS-ERROR") {
                rethrow "DELETE-RECORDS-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
            }
        }
        int rc = deleteRecordsImpl(getSearchExpression(where_cond, search_options), search_options);
        if (!rc && search_options.requires_result) {
            throw "DELETE-RECORDS-ERROR", "no records deleted but 'requires_result' is True";
        }
        return rc;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw INVALID-REQUEST the request argument provided do not match the expected request type
    */
    auto doRequest(auto req, *hash<auto> request_options) {
        checkRequest();
        *AbstractDataProviderType request_type = getRequestTypeWithData(req);
        if (request_type) {
            try {
                req = request_type.acceptsValue(req);
            } catch (hash<ExceptionInfo> ex) {
                rethrow "INVALID-REQUEST", sprintf("invalid request argument for request %y: %s: %s", getName(),
                    ex.err, ex.desc);
            }
        }
        return doRequestImpl(req, validateRequestOptions(request_options));
    }

    #! Creates a new child data provider and returns it after adding as a child
    /** @param name the name of the new child data provider
        @param fields the fields for the new child data provider; must have at least one field
        @param child_create_options the options for creating the new child data provider

        @return the new child data provider

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw CREATE-CHILD-PROVIDER-ERROR error creating the child provider

        @since DataProvider 2.0
    */
    AbstractDataProvider createChildProvider(string name, hash<string, AbstractDataField> fields,
            *hash<auto> child_create_options) {
        checkCreateChild();
        child_create_options = validateChildCreateOptions(child_create_options);
        if (!fields) {
            throw "CREATE-CHILD-PROVIDER-ERROR", "at least one field must be present in the field hash";
        }
        on_error if ($1.err != "CREATE-CHILD-PROVIDER-ERROR") {
            rethrow "CREATE-CHILD-PROVIDER-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
        }
        AbstractDataProvider child = createChildProviderImpl(name, fields, child_create_options);
        if (logger && !child.logger) {
            child.setLogger(logger);
        }
        return child;
    }

    #! Deletes a child data provider
    /** @param name the name of the new child data provider
        @param child_delete_options the options for deleting a child data provider

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw DELETE-CHILD-PROVIDER-ERROR error creating the child provider

        @since DataProvider 2.0
    */
    deleteChildProvider(string name, *hash<auto> child_delete_options) {
        checkDeleteChild();
        child_delete_options = validateChildDeleteOptions(child_delete_options);
        on_error if ($1.err != "DELETE-CHILD-PROVIDER-ERROR") {
            rethrow "DELETE-CHILD-PROVIDER-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
        }
        return deleteChildProviderImpl(name, child_delete_options);
    }

    #! Creates a new field
    /**
        @param field the field to add
        @param field_add_options the options for adding the new field

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw ADD-FIELD-ERROR error adding the field

        @since DataProvider 2.0
    */
    addField(AbstractDataField field, *hash<auto> field_add_options) {
        checkAddField();
        field_add_options = validateFieldAddOptions(field_add_options);
        on_error if ($1.err != "ADD-FIELD-ERROR") {
            rethrow "ADD-FIELD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
        }
        addFieldImpl(field, field_add_options);
    }

    #! Updates an existing field
    /** @param name the name of the current field to update
        @param field the new definition of the field
        @param field_update_options the options for adding the new field

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw UPDATE-FIELD-ERROR error adding the field

        @since DataProvider 2.0
    */
    updateField(string name, AbstractDataField field, *hash<auto> field_update_options) {
        checkUpdateField();
        field_update_options = validateFieldUpdateOptions(field_update_options);
        on_error if ($1.err != "UPDATE-FIELD-ERROR") {
            rethrow "UPDATE-FIELD-ERROR", sprintf("%s: %s", $1.err, $1.desc), $1.arg;
        }
        return updateFieldImpl(name, field, field_update_options);
    }

    #! Deletes an existing field
    /** @param name the name of the field to delete
        @param field_delete_options the options for deleting the new field; processed by validateFieldDeleteOptions()

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw DELETE-FIELD-ERROR error adding the field

        @since DataProvider 2.0
    */
    deleteField(string name, *hash<auto> field_delete_options) {
        checkDeleteField();
        field_delete_options = validateFieldDeleteOptions(field_delete_options);
        on_error if ($1.err != "DELETE-FIELD-ERROR") {
            rethrow "DELETE-FIELD-ERROR", sprintf("%s: %s", $1.err, $1.desc);
        }
        return deleteFieldImpl(name, field_delete_options);
    }

    #! Sends a message from message-capable data providers
    /**
        @param message_id the message ID to send; must correspond to a valid message ID
        @param msg the message to send
        @param send_message_options the options for sending the message; processed by validateSendMessageOptions()

        @throw INVALID-OPERATION thrown if the data provider does not support this API
        @throw INVALID-MESSAGE the message_id is not valid
        @throw SEND-MESSAGE-ERROR error sending the message

        @since DataProvider 2.5
    */
    sendMessage(string message_id, auto msg, *hash<auto> send_message_options) {
        checkMessages();
        send_message_options = validateSendMessageOptions(send_message_options);
        hash<DataProviderMessageInfo> message_info = getMessageInfoImpl(message_id);
        on_error if ($1.err != "SEND-MESSAGE-ERROR") {
            rethrow "SEND-MESSAGE-ERROR", sprintf("%s: (message ID %y) %s", $1.err, message_id, $1.desc);
        }
        msg = message_info.type.acceptsValue(msg);
        sendMessageImpl(message_id, msg, send_message_options);
    }

    #! Returns the schema supporting this data provider
    /** @return the schema supporting this data provider

        @throw INVALID-OPERATION thrown if the data provider does not support this API

        @since DataProvider 2.2
    */
    object getSchemaObject() {
        checkSchema();
        return getSchemaObjectImpl();
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider

        @throw INVALID-OPERATION the data provider does not support the request API
    */
    *AbstractDataProviderType getRequestType() {
        checkRequest();
        return getRequestTypeImpl();
    }

    #! Returns the description of a successful request message with call data, if any
    /** @param req sample request data that can be used to determine the final type

        @return the request type for this provider according to any call data passed

        @throw INVALID-OPERATION the data provider does not support the request API

        @note This API is meant to provide information about request data types that can depend on the call data

        @since DataProvider 3.0
    */
    *AbstractDataProviderType getRequestTypeWithData(auto req) {
        checkRequest();
        return getRequestTypeWithDataImpl(req);
    }

    #! Returns the description of a successful request message in the context of the given option data
    /** @param options action option data for the data provider

        @return the request type for this provider in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @note This API is meant to provide information about request data types that can depend on the option data

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getRequestTypeWithOptions(*hash<auto> options) {
        checkRequest();
        return getRequestTypeWithOptionsImpl(options);
    }

    #! Returns action options corresponding to the request type with the given request data, if possible
    /** @param req sample request data that can be used to determine the final type

        @return action options corresponding to the request type

        @throw INVALID-OPERATION the data provider does not support the request API

        @note This API is meant to provide information about request data types that can depend on the call data

        @since DataProvider 3.0
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsFromRequestTypeWithData(auto req) {
        try {
            checkRequest();
            *AbstractDataProviderType rt;
            try {
                rt = getRequestTypeWithData(req);
            } catch (hash<ExceptionInfo> ex) {
                LoggerWrapper::error("Failed to request arg type with data, options: %y, ignoring options: %s",
                    req, get_exception_string(ex));
                rt = getRequestType();
            }
            if (rt && (*hash<auto> fields = rt.getFields())) {
                return DataProviderActionCatalog::getActionOptionFromFields(fields);
            }
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get request arg type with data (%y), cannot return options: %s", req,
                get_exception_string(ex));
        }
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message

        @throw INVALID-OPERATION the data provider does not support the request API
    */
    *AbstractDataProviderType getResponseType() {
        checkRequest();
        return getResponseTypeImpl();
    }

    #! Returns the description of a successful response message with call data, if any
    /** @param req sample request data that can be used to determine the final type

        @return the response type for this provider according to any call data passed

        @throw INVALID-OPERATION the data provider does not support the request / response API

        @note This API is meant to provide information about response data types that can depend on the call data

        @since DataProvider 3.0
    */
    *AbstractDataProviderType getResponseTypeWithData(auto req) {
        checkRequest();
        return getResponseTypeWithDataImpl(req);
    }

    #! Returns the description of a successful response message in the context of the given option data
    /** @param options action option data for the data provider

        @return the response type for this provider in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request / response API

        @note This API is meant to provide information about response data types that can depend on the call data

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getResponseTypeWithOptions(*hash<auto> options) {
        checkRequest();
        return getResponseTypeWithOptionsImpl(options);
    }

    #! Returns a hash of error responses, if any
    /** @return a hash of error responses, if any, keyed by response code or string

        @throw INVALID-OPERATION the data provider does not support the request API
    */
    *hash<string, AbstractDataProviderType> getErrorResponseTypes() {
        checkRequest();
        return getErrorResponseTypesImpl();
    }

    #! Returns the type for the given error code
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return the type for the given error code

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known
    */
    AbstractDataProviderType getErrorResponseType(string error_code) {
        checkRequest();
        return getErrorResponseTypeImpl(error_code);
    }

    #! Returns the input type for the pipeline processor, if any
    /** @return the input type for the pipeline processor; NOTHING means any type is accepted

        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getPipelineProcessorInputType() {
        checkPipelineProcessor();
        return getPipelineProcessorInputTypeImpl();
    }

    #! Returns the input type for the pipeline processor in the context of the given option data
    /** @param options action option data for the data provider

        @return the input type for the pipeline processor in the context of the given option data;
        NOTHING means any type is accepted

        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @note This API is meant to provide information about input data types that can depend on the option data

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getPipelineProcessorInputTypeWithOptions(*hash<auto> options) {
        checkPipelineProcessor();
        return getPipelineProcessorInputTypeWithOptionsImpl(options);
    }

    #! Returns the output type for the pipeline processor, if any
    /** @return the output type for the pipeline processor; NOTHING means any type is produced

        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getPipelineProcessorOutputType() {
        checkPipelineProcessor();
        return getPipelineProcessorOutputTypeImpl();
    }

    #! Returns the output type for the pipeline processor in the context of the given option data
    /** @param options action option data for the data provider

        @return the output type for the pipeline processor in the context of the given option data;
        NOTHING means any type is produced

        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @note This API is meant to provide information about output data types that can depend on the option data

        @since DataProvider 3.3
    */
    *AbstractDataProviderType getPipelineProcessorOutputTypeWithOptions(*hash<auto> options) {
        checkPipelineProcessor();
        return getPipelineProcessorOutputTypeWithOptionsImpl(options);
    }

    #! Returns the description of an observable event, if any
    /** @param event_id the event ID of the event

        @return the observable event type for this provider

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 2.5
    */
    hash<DataProviderMessageInfo> getEventInfo(string event_id) {
        checkObservable();
        return getEventInfoImpl(event_id);
    }

    #! Returns the description of an observable event, if any with option data, if any
    /** @param event_id the event ID of the event
        @param options example option data that can be used to determine the final type

        @return the event type for this provider according to any call data passed

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @note This API is meant to provide information about event data types that can depend on option data

        @since DataProvider 3.0
    */
    hash<DataProviderMessageInfo> getEventInfoWithOptions(string event_id, *hash<auto> options) {
        checkObservable();
        return getEventInfoWithOptionsImpl(event_id, options);
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API

        @since DataProvider 2.5
    */
    hash<string, hash<DataProviderMessageInfo>> getEventTypes() {
        checkObservable();
        return getEventTypesImpl();
    }

    #! Returns a hash of all supported event types in the context of the given options
    /** @return a hash of all supported event types in the context of the given options

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API

        @since DataProvider 3.3
    */
    hash<string, hash<DataProviderMessageInfo>> getEventTypesWithOptions(*hash<auto> options) {
        checkObservable();
        return getEventTypesWithOptionsImpl(options);
    }

    #! Returns action options corresponding to the given option data
    /** @param action action info for the event action
        @param options sample option data that can be used to determine the final type

        @return action options according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.1
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsWithOptions(hash<DataProviderActionInfo> action,
            *hash<auto> options) {
        if (!action.data_dependent_options) {
            return action.options;
        }
        try {
            return tryGetActionOptionsWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for an event action
    /** @param action action info for the request action
        @param options sample option data that can be used to determine the final type

        @return action options for an event action according to the available option data

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForEventWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkObservable();
            return tryGetActionOptionsForEventWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for event with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a request action
    /** @param action action info for the send message action
        @param options sample option data that can be used to determine the final type

        @return request options for a request action according to the available option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForRequestWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkRequest();
            return tryGetActionOptionsForRequestWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for request with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a send message action
    /** @param action action info for the search action
        @param options sample option data that can be used to determine the final type

        @return request options for a send message action according to the available option data

        @throw INVALID-OPERATION the data provider does not support sending messages

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForMessageWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkMessages();
            return tryGetActionOptionsForMessageWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for message with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a search action
    /** @param action action info for the create action
        @param options sample option data that can be used to determine the final type

        @return action options for a search action according to the available option data

        @throw INVALID-OPERATION the data provider does not support reading

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForSearchWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkRead();
            return tryGetActionOptionsForSearchWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for search with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a create action
    /** @param action action info for the upsert action
        @param options sample option data that can be used to determine the final type

        @return action options for a create action according to the available option data

        @throw INVALID-OPERATION the data provider does not support record creation

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForCreateWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkCreate();
            return tryGetActionOptionsForCreateWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for create with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for an upsert action
    /** @param action action info for the update action
        @param options sample option data that can be used to determine the final type

        @return action options for an upsert action according to the available option data

        @throw INVALID-OPERATION the data provider does not support record upserting

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForUpsertWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkUpsert();
            return tryGetActionOptionsForUpsertWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for upsert with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for an update action
    /** @param action action info for the delete action
        @param options sample option data that can be used to determine the final type

        @return action options for an update action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForUpdateWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkUpdate();
            return tryGetActionOptionsForUpdateWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for update with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a delete action
    /** @param action action info for the event action
        @param options sample option data that can be used to determine the final type

        @return action options for a delete action according to the available option data

        @throw INVALID-OPERATION the data provider does not support record deletion

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForDeleteWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkDelete();
            return tryGetActionOptionsForDeleteWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for delete with options (%y): %s", options,
                get_exception_string(ex));
        }
    }

    #! Returns action options corresponding to the given option data for a pipeline processor action
    /** @param action action info for the pipeline processor action
        @param options sample option data that can be used to determine the final type

        @return action options for a pipeline processor action according to the available option data

        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForPipelineProcessorWithOptions(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        try {
            checkPipelineProcessor();
            return tryGetActionOptionsForPipelineProcessorWithOptionsImpl(action, options);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error("Failed to get action options for pipeline processor with options (%y): %s",
                options, get_exception_string(ex));
        }
    }

    #! Returns the description of an outbound message, if any
    /** @param message_id the ID of the message

        @return the message type for this provider

        @throw INVALID-OPERATION the data provider does not support sending messages
        @throw INVALID-MESSAGE the message_id is not valid

        @since DataProvider 2.5
    */
    hash<DataProviderMessageInfo> getMessageInfo(string message_id) {
        checkMessages();
        return getMessageInfoImpl(message_id);
    }

    #! Returns a hash of all supported outbound messages
    /** @return a hash of all supported outbound messages

        @throw INVALID-OPERATION the data provider does not support sending messages

        @since DataProvider 2.5
    */
    hash<string, hash<DataProviderMessageInfo>> getMessageTypes() {
        checkMessages();
        return getMessageTypesImpl();
    }

    #! Returns a hash of all supported outbound messages with options
    /** @param options any action options that can affect the message types returned

        @return a hash of all supported outbound messages

        @throw INVALID-OPERATION the data provider does not support sending messages

        @since DataProvider 3.3
    */
    hash<string, hash<DataProviderMessageInfo>> getMessageTypesWithOptions(*hash<auto> options) {
        checkMessages();
        return getMessageTypesWithOptionsImpl(options);
    }

    #! Returns example data for a request message
    /** @return example data for a request message

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.0
    */
    auto getExampleRequestData() {
        checkRequest();
        if (exists (auto v = getExampleRequestDataImpl())) {
            return v;
        }
        if (*AbstractDataProviderType rt = getRequestTypeImpl()) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for a successful request message
    /** @return example data for a successful request message

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.0
    */
    auto getExampleResponseData() {
        checkRequest();
        if (exists (auto v = getExampleResponseDataImpl())) {
            return v;
        }
        if (*AbstractDataProviderType rt = getResponseTypeImpl()) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the error message with the given error code
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return example data for the error message with the given error code

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known

        @since DataProvider 3.0
    */
    auto getExampleErrorResponseData(string error_code) {
        checkRequest();
        if (exists (auto v = getExampleErrorResponseDataImpl(error_code))) {
            return v;
        }
        if (*AbstractDataProviderType rt = getErrorResponseTypeImpl(error_code)) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the given observable event
    /** @param event_id the event ID of the event

        @return example data for the given observable event

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.0
    */
    auto getExampleEventData(string event_id) {
        checkObservable();
        if (exists (auto v = getExampleEventDataImpl(event_id))) {
            return v;
        }
        if (*AbstractDataProviderType rt = getEventInfoImpl(event_id).type) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the record type
    /** @return example data for the record type

        @since DataProvider 3.0
    */
    hash<auto> getExampleRecordData() {
        checkRead();
        if (exists (auto v = getExampleRecordDataImpl())) {
            return v;
        }
        if (*hash<string, AbstractDataField> rt = getRecordTypeImpl()) {
            return generateExampleData(rt);
        }
        return {};
    }

    #! Returns example data for a request message in the context of the given option data
    /** @param options option data that can be used to determine the final type

        @return example data for a request message in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.3
    */
    auto getExampleRequestDataWithOptions(*hash<auto> options) {
        checkRequest();
        if (exists (auto v = getExampleRequestDataWithOptionsImpl(options))) {
            return v;
        }
        return getExampleRequestDataWithOptionsImpl(options)
            ?? getRequestTypeWithOptionsImpl(options).getExampleValue();
    }

    #! Returns example data for a successful request message in the context of the given option data
    /** @param options option data that can be used to determine the final type

        @return example data for a successful request message in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.3
    */
    auto getExampleResponseDataWithOptions(*hash<auto> options) {
        checkRequest();
        if (exists (auto v = getExampleResponseDataWithOptionsImpl(options))) {
            return v;
        }
        if (*AbstractDataProviderType rt = getResponseTypeWithOptionsImpl(options)) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the error message with the given error code in the context of the given option data
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @param options option data that can be used to determine the final type
        @return example data for the error message with the given error code in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known

        @since DataProvider 3.3
    */
    auto getExampleErrorResponseDataWithOptions(string error_code, *hash<auto> options) {
        checkRequest();
        if (exists (auto v = getExampleErrorResponseDataWithOptionsImpl(error_code, options))) {
            return v;
        }
        if (*AbstractDataProviderType rt = getErrorResponseTypeWithOptionsImpl(error_code, options)) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the given observable event in the context of the given option data
    /** @param options option data that can be used to determine the final type
        @param event_id the event ID of the event

        @return example data for the given observable event in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.3
    */
    auto getExampleEventDataWithOptions(string event_id, *hash<auto> options) {
        checkObservable();
        if (exists (auto v = getExampleEventDataWithOptionsImpl(event_id, options))) {
            return v;
        }
        if (*AbstractDataProviderType rt = getEventInfoWithOptionsImpl(event_id, options).type) {
            return rt.getExampleValue();
        }
    }

    #! Returns example data for the record type in the context of the given option data
    /** @param options option data that can be used to determine the final type

        @return example data for the record type in the context of the given option data

        @since DataProvider 3.3
    */
    hash<auto> getExampleRecordDataWithOptions(*hash<auto> options) {
        checkRead();
        if (exists (auto v = getExampleRecordDataWithOptionsImpl(options))) {
            return v;
        }
        if (*hash<string, AbstractDataField> rt = getRecordTypeWithOptionsImpl(options)) {
            return generateExampleData(rt);
        }
        return {};
    }

    #! Returns connection info if the data provider supports connections, otherwise returns @ref NOTHING
    /** @return connection info if the data provider supports connections, otherwise returns @ref NOTHING

        @since DataProvider 3.0
    */
    *hash<DataProviderConnectionInfo> getConnectionInfo() {
        return getConnectionInfoImpl();
    }

    #! Return data provider summary info
    /** @note This creates an AbstractDataProvider object for each child; for cases when this is expensive, this
        method must be overridden / reimplemented in the child class for performance reasons
    */
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<hash<DataProviderSummaryInfo>> rv;
        foreach string name in (getChildProviderNames()) {
            try {
                rv += getChildProviderEx(name).getSummaryInfo();
            } catch (hash<ExceptionInfo> ex) {
                # ignore exception
            }
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    *list<string> getChildProviderNames() {
        return getChildProviderNamesImpl();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @param name the name of the child

        @return the given child provider or @ref nothing if the given child is unknown

        @throw INVALID-CHILD-PROVIDER unknown child provider

        @see getChildProviderEx()
    */
    *AbstractDataProvider getChildProvider(string name) {
        *AbstractDataProvider child = getChildProviderImpl(name);
        if (child && logger && !child.logger) {
            child.setLogger(logger);
        }
        return child;
    }

    #! Returns the given child provider or throws an exception if the given child is unknown
    /** @param name the name of the child

        @return the given child provider or throws an exception if the given child is unknown

        @throw INVALID-CHILD-PROVIDER unknown child provider

        @see getChildProvider()
    */
    AbstractDataProvider getChildProviderEx(string name) {
        *AbstractDataProvider provider = getChildProviderImpl(name);
        if (!provider) {
            throw "INVALID-CHILD-PROVIDER", sprintf("provider %y is unknown; known children of %y: %y", name,
                getName(), getChildProviderNames());
        }
        if (logger && !provider.logger) {
            provider.setLogger(logger);
        }
        return provider;
    }

    #! Returns the given child provider from a \c "/" separated path string
    /** throws an exception with unknown path elements

        @param path a string giving a path to the target provider where child elements are separated by \c "/" characters

        @return the target child provider; throws an exception if any element in the path is unknown

        @throw INVALID-CHILD-PROVIDER unknown child provider
    */
    AbstractDataProvider getChildProviderPath(string path) {
        AbstractDataProvider provider = self;
        if (path =~ /^\//) {
            path =~ s/^\/+//;
        }
        map provider = provider.getChildProviderEx($1), path.split("/");
        if (logger && !provider.logger) {
            provider.setLogger(logger);
        }
        return provider;
    }

    #! Returns @ref True if the data provider requires transaction management
    /** @return @ref True if the data provider requires transaction management, in which case commit() or rollback()
        must be called to flush or discard pending changes to the data provider
    */
    bool requiresTransactionManagement() {
        return getStaticInfoImpl().transaction_management;
    }

    #! Begins a transaction with a data provider
    /** Has no effect if the data provider does not require transaction management

        @see:
        - commit()
        - rollback()

        @since DataProvider 1.2.2
    */
    beginTransaction() {
    }

    #! Commits data written to the data provider
    /** Has no effect if the data provider does not support transaction management

        @see:
        - beginTransaction()
        - rollback()
    */
    commit() {
    }

    #! Rolls back data written to the data provider
    /** Has no effect if the data provider does not support transaction management

        @see:
        - beginTransaction()
        - commit()
    */
    rollback() {
    }

    #! Returns @ref True if the current thread is in a transaction
    /** @return @ref True if the current thread is in a transaction

        @throw INVALID-OPERATION the data provider does not support transaction management

        @since DataProvider 3.3
    */
    bool currentThreadInTransaction() {
        if (!requiresTransactionManagement()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support transaction management",
                getName(), self.className());
        }
        return currentThreadInTransactionImpl();
    }

    #! Returns a bulk insert operation object for the data provider
    /** @param block_size the block size for the bulk operation; if not given, the default block size for the data
        provider is used (normally 1000 records)

        @return a bulk insert operation object for the data provider

        @throw INVALID-OPERATION the data provider does not support create operations
    */
    AbstractDataProviderBulkOperation getBulkInserter(*int block_size) {
        checkCreate();
        return new DefaultBulkInserter(self, block_size);
    }

    #! Returns a bulk upsert operation object for the data provider
    /** @param block_size the block size for the bulk operation; if not given, the default block size for the data
        provider is used (normally 1000 records)

        @return a bulk upsert operation object for the data provider

        @throw INVALID-OPERATION the data provider does not support upsert operations
    */
    AbstractDataProviderBulkOperation getBulkUpserter(*int block_size) {
        checkUpsert();
        return new DefaultBulkUpserter(self, block_size);
    }

    #! Returns custom data mapper runtime keys
    /** @return custom data mapper runtime keys

        This base method returns @ref nothing; reimplment in child classes to return a value
    */
    *hash<string, hash<MapperRuntimeKeyInfo>> getMapperRuntimeKeys() {
        # this method intentionally left empty
    }

    #! Ensures that the data provider supports read operations
    /**
        @throw INVALID-OPERATION the data provider does not support read operations
    */
    checkRead() {
        if (!supportsRead()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support reading records",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports record creation
    /**
        @throw INVALID-OPERATION the data provider does not support record creation
    */
    checkCreate() {
        if (!supportsCreate()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support record creation",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports record upserts
    /**
        @throw INVALID-OPERATION the data provider does not support upsert operations
    */
    checkUpsert() {
        if (!supportsUpsert()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support record upserts",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports record upserts
    /**
        @throw INVALID-OPERATION the data provider does not support record updating
    */
    checkUpdate() {
        if (!supportsUpdate()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support record updating",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports record deletion
    /**
        @throw INVALID-OPERATION the data provider does not support record deletion
    */
    checkDelete() {
        if (!supportsDelete()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support record deletion",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports the request API
    /**
        @throw INVALID-OPERATION the data provider does not support the request API
    */
    checkRequest() {
        if (!supportsRequest()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support the request API",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports the pipeline processor API
    /**
        @throw INVALID-OPERATION the data provider does not support the pipeline processor API

        @since DataProvider 3.3
    */
    checkPipelineProcessor() {
        if (!supportsPipelineProcessor()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support the pipeline "
                "processor API", getName(), self.className());
        }
    }

    #! Ensures that the data provider supports creating children
    /**
        @throw INVALID-OPERATION the data provider does not support the child creation API

        @since DataProvider 2.0
    */
    checkCreateChild() {
        if (!supportsCreateChild()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support creating children",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports deleting children
    /**
        @throw INVALID-OPERATION the data provider does not support the child deletion API

        @since DataProvider 2.0
    */
    checkDeleteChild() {
        if (!supportsDeleteChild()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support deleting children",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports adding fields
    /**
        @throw INVALID-OPERATION the data provider does not support the add field API

        @since DataProvider 2.0
    */
    checkAddField() {
        if (!supportsAddField()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support adding fields",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports updating fields
    /**
        @throw INVALID-OPERATION the data provider does not support the update field API

        @since DataProvider 2.0
    */
    checkUpdateField() {
        if (!supportsUpdateField()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support updating fields",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports deleting fields
    /**
        @throw INVALID-OPERATION the data provider does not support the delete field API

        @since DataProvider 2.0
    */
    checkDeleteField() {
        if (!supportsDeleteField()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support deleting fields",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports a schema
    /**
        @throw INVALID-OPERATION the data provider does not support a schema

        @since DataProvider 2.2
    */
    checkSchema() {
        if (!supportsSchema()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support schemas",
                getName(), self.className());
        }
    }

    #! Ensures that the data provider supports advanced search expressions
    /**
        @throw INVALID-OPERATION the data provider does not support advanced search expressions

        @since DataProvider 2.2
    */
    checkSearchExpressions() {
        if (!supportsSearchExpressions()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support advanced search "
                "expressions", getName(), self.className());
        }
    }

    #! Ensures that the data provider supports observer pattern / event API
    /**
        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API

        @since DataProvider 2.5
    */
    checkObservable() {
        if (!supportsObservable()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support the observer pattern / "
                "event API", getName(), self.className());
        }
    }

    #! Ensures that the data provider supports sending messages
    /**
        @throw INVALID-OPERATION the data provider does not support sending messages

        @since DataProvider 2.5
    */
    checkMessages() {
        if (!supportsMessages()) {
            throw "INVALID-OPERATION", sprintf("data provider %y (type %y) does not support sending messages",
                getName(), self.className());
        }
    }

    #! Returns a search expression for a standard search hash
    /** @param where_cond the search criteria; if the data provider supports advanced search, and the value is an
        expression, it will be processed by processSearchParameters(), otherwise by processFieldValues()
        @param search_options the search options; will be processed by validateSearchOptions()

        @return a search expression corresponding to \a where_cond

        @since DataProvider 2.3
    */
    *hash<auto> getSearchExpression(*hash<auto> where_cond, *hash<auto> search_options) {
        if (where_cond instanceof hash<DataProviderExpression>) {
            return processSearchParameters(where_cond, search_options);
        }
        if (!where_cond) {
            return where_cond;
        }
        if (!supportsSearchExpressions()) {
            return processFieldValues(where_cond, search_options);
        }
        where_cond = processFieldValues(where_cond, search_options);
        hash<DataProviderInfo> info = getInfo();
        hash<DataProviderExpressionInfo> expinfo = getExpression(ER_Search, DP_OP_AND, info.expressions);
        hash<DataProviderExpression> rv;
        if (where_cond.size() != 1) {
            rv = <DataProviderExpression>{
                "exp": DP_OP_AND,
                "args": map getSimpleArgumentIntern(ER_Search, info, expinfo, $1.key, $1.value),
                    where_cond.pairIterator(),
            };
        } else {
            rv = getSimpleArgumentIntern(ER_Search, info, expinfo, where_cond.firstKey(), where_cond.firstValue());
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        \c getReferenceData()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedReferenceData() {
        # intentionally empty
    }

    #! Returns reference data of the given kind if available; logs but ignores exceptions retrieving the data
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data or nothing if an exception occured retrieving the reference
        data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getReferenceDataSafe(string type, *hash<auto> action_opts) {
        try {
            return getReferenceData(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data or nothing if an exception occured retrieving the reference
        data

        Logs but ignores exceptions retrieving the data; throws an exception if the data type is unsupported

        @throw UNSUPPORTED-REFERENCE-DATA thrown if \a type is not supported as reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getReferenceDataSafeEx(string type, *hash<auto> action_opts) {
        try {
            return getReferenceData(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }

        if (!(*hash<string, bool> rd = getSupportedReferenceData()){type}) {
            throw "UNSUPPORTED-REFERENCE-DATA", sprintf("Reference data type %y is unsupported; supported "
                "reference data types: %y", type, rd);
        }
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @throw REFERENCE-DATA-ERROR thrown if there is an error retrieving the reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getReferenceData(string type, *hash<auto> action_opts) {
        on_error rethrow "REFERENCE-DATA-ERROR", sprintf("Failed to retrieve reference data of type %y: %s: %s",
            type, $1.err, $1.desc);
        return getReferenceDataImpl(type, action_opts);
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @throw REFERENCE-DATA-ERROR thrown if there is an error retrieving the reference data
        @throw UNSUPPORTED-REFERENCE-DATA thrown if \a type is not supported as reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getReferenceDataEx(string type, *hash<auto> action_opts) {
        if (*list<hash<AllowedValueInfo>> rv = getReferenceData(type, action_opts)) {
            return rv;
        }
        if (!(*hash<string, bool> rd = getSupportedReferenceData()){type}) {
            throw "UNSUPPORTED-REFERENCE-DATA", sprintf("Reference data type %y is unsupported; supported "
                "reference data types: %y", type, rd);
        }
    }

    #! Returns information on supported element reference data
    /** @return a hash of supported element reference data; keys in the hash returned are supported in calls to
        @ref getElementReferenceData()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedElementReferenceData() {
        # intentionally empty
    }

    #! Returns element reference data of the given kind if available; logs but ignores exceptions retrieving the data
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data or nothing if an exception occured retrieving the element
        reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getElementReferenceDataSafe(string type, *hash<auto> action_opts) {
        try {
            return getElementReferenceData(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }
    }

    #! Returns element reference data of the given kind if available
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data or nothing if an exception occured retrieving the element
        reference data

        Logs but ignores exceptions retrieving the data; throws an exception if the data type is unsupported

        @throw UNSUPPORTED-ELEMENT-REFERENCE-DATA thrown if \a type is not supported as element reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getElementReferenceDataSafeEx(string type, *hash<auto> action_opts) {
        try {
            return getElementReferenceData(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }

        if (!(*hash<string, bool> rd = getSupportedElementReferenceData()){type}) {
            throw "UNSUPPORTED-ELEMENT-REFERENCE-DATA", sprintf("Element reference data type %y is unsupported; "
                "supported element reference data types: %y", type, rd);
        }
    }

    #! Returns element reference data of the given kind if available
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @throw ELEMENT-REFERENCE-DATA-ERROR thrown if there is an error retrieving the element reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getElementReferenceData(string type, *hash<auto> action_opts) {
        on_error rethrow "ELEMENT-REFERENCE-DATA-ERROR", sprintf("Failed to retrieve element reference data of "
            "type %y: %s: %s", type, $1.err, $1.desc);
        return getElementReferenceDataImpl(type, action_opts);
    }

    #! Returns element reference data of the given kind if available
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @throw ELEMENT-REFERENCE-DATA-ERROR thrown if there is an error retrieving the element reference data
        @throw UNSUPPORTED-ELEMENT-REFERENCE-DATA thrown if \a type is not supported as element reference data

        @since DataProvider 3.0
    */
    *list<hash<AllowedValueInfo>> getElementReferenceDataEx(string type, *hash<auto> action_opts) {
        if (*list<hash<AllowedValueInfo>> rv = getElementReferenceData(type, action_opts)) {
            return rv;
        }
        if (!(*hash<string, bool> rd = getSupportedElementReferenceData()){type}) {
            throw "UNSUPPORTED-ELEMENT-REFERENCE-DATA", sprintf("Element reference data type %y is unsupported; "
                "supported element reference data types: %y", type, rd);
        }
    }

    #! Returns information on supported reference data values
    /** @return a hash of supported reference data values; keys in the hash returned are supported in calls to
        \c getReferenceDataValue()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedReferenceDataValues() {
        # intentionally empty
    }

    #! Returns a reference data value of the given kind if available; logs but ignores exceptions retrieving the data
    /** @param type the unique type name of the reference data value
        @param action_opts an optional hash of action options when called when working with an app action

        @return the reference data value or nothing if an exception occured retrieving the reference data value

        @since DataProvider 3.0
    */
    auto getReferenceDataValueSafe(string type, *hash<auto> action_opts) {
        try {
            return getReferenceDataValue(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data or nothing if an exception occured retrieving the reference
        data

        Logs but ignores exceptions retrieving the data; throws an exception if the data type is unsupported

        @throw UNSUPPORTED-REFERENCE-DATA thrown if \a type is not supported as reference data

        @since DataProvider 3.0
    */
    auto getReferenceDataValueSafeEx(string type, *hash<auto> action_opts) {
        try {
            return getReferenceDataValue(type, action_opts);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::debug("%s", get_exception_string(ex));
        }

        if (!(*hash<string, bool> rd = getSupportedReferenceDataValues()){type}) {
            throw "UNSUPPORTED-REFERENCE-DATA", sprintf("Reference data value type %y is unsupported; supported "
                "reference data value types: %y", type, rd);
        }
    }

    #! Returns a reference data value of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return the value of the reference data

        @throw REFERENCE-DATA-ERROR thrown if there is an error retrieving the reference data

        @since DataProvider 3.0
    */
    auto getReferenceDataValue(string type, *hash<auto> action_opts) {
        on_error rethrow "REFERENCE-DATA-ERROR", sprintf("Failed to retrieve reference data value of type %y: %s: %s",
            type, $1.err, $1.desc);
        return getReferenceDataValueImpl(type, action_opts);
    }

    #! Returns a reference data value of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return the value of the reference data

        @throw REFERENCE-DATA-ERROR thrown if there is an error retrieving the reference data
        @throw UNSUPPORTED-REFERENCE-DATA thrown if \a type is not supported as reference data

        @since DataProvider 3.0
    */
    auto getReferenceDataValueEx(string type, *hash<auto> action_opts) {
        if (exists (auto rv = getReferenceDataValue(type, action_opts))) {
            return rv;
        }
        if (!(*hash<string, bool> rd = getSupportedReferenceDataValues()){type}) {
            throw "UNSUPPORTED-REFERENCE-DATA", sprintf("Reference data value type %y is unsupported; supported "
                "reference data value types: %y", type, rd);
        }
    }

    #! Update any connection used by the data provider
    /** @param conn the new AbstractConnection object
        @param connobj the new connection object itself (ex: RestClient for a "rest" connection)

        This is useful for event-driven providers that require a connection object to generate events.

        Subclasses should implement appropriate logic

        @since DataProvider 3.0
    */
    updateConnectionObjects(object conn, object connobj) {
        # intentially blank
    }

    #! Creates the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are field names and values are either
        lists of field values or a single value that applies to all records
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)
    */
    private *hash<auto> createRecordsBulkIntern(hash<auto> recs, *hash<auto> create_options) {
        if (!supportsBulkCreate()) {
            hash<auto> rv;
            foreach hash<auto> row in (recs.contextIterator()) {
                hash<auto> out = createRecordImpl(row, create_options);
                if (!rv) {
                    rv = map {$1: ()}, keys out;
                }
                map rv{$1.key} += $1.value, out.pairIterator();
            }
            return rv;
        }
        return createRecordsBulkImpl(recs, create_options);
    }

    #! Returns create options as action options
    private *hash<string, hash<ActionOptionInfo>> getCreateOptionsAsActionOptions() {
        return getOptionsAsActionOptions(getCreateOptions());
    }

    #! Returns search options as action options
    private hash<string, hash<ActionOptionInfo>> getSearchOptionsAsActionOptions() {
        return cast<hash<string, hash<ActionOptionInfo>>>({
            "block_size": <ActionOptionInfo>{
                "display_name": "Block Size",
                "desc": "The maximum number of records to return in a single block when opening a record stream",
                "type": AbstractDataProviderTypeMap."*int",
            },
        } + getOptionsAsActionOptions(getSearchOptions()));
    }

    #! Returns upsert options as action options
    private *hash<string, hash<ActionOptionInfo>> getUpsertOptionsAsActionOptions() {
        return getOptionsAsActionOptions(getUpsertOptions());
    }

    #! Returns search options as action options
    private *hash<string, hash<ActionOptionInfo>> getOptionsAsActionOptions(
                *hash<string, hash<DataProviderOptionInfo>> options) {
        return cast<*hash<string, hash<ActionOptionInfo>>>(map {$1.key: cast<hash<ActionOptionInfo>>($1.value - "type" + {
            "type": $1.value.type[0],
        })}, options.pairIterator());
    }

    #! Returns action options corresponding to the given option data for an event action
    /** @param action action info for the request action
        @param options sample option data that can be used to determine the final type

        @return action options for an event action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForEventWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        hash<string, hash<ActionOptionInfo>> rv = {
            "event_id": <ActionOptionInfo>{
                "display_name": "Event",
                "desc": "The event to subscribe to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (map <AllowedValueInfo>{
                    "value": $1.key,
                    "desc": $1.value.desc,
                }, getEventTypesWithOptions(options).pairIterator()) ?? (),
            },
        };
        return action.options + rv;
    }

    #! Returns action options corresponding to the given option data for a request action
    /** @param action action info for the send message action
        @param options sample option data that can be used to determine the final type

        @return request options for a request action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForRequestWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        *hash<string, hash<ActionOptionInfo>> rv = action.options;
        if ((*AbstractDataProviderType type = getRequestTypeWithOptions(options))) {
            if (*hash<auto> fields = type.getFields()) {
                rv += DataProviderActionCatalog::getActionOptionFromFields(fields);
            } else {
                rv.request_data = <ActionOptionInfo>{
                    "display_name": "API Request Data",
                    "desc": "The request data",
                    "type": type,
                };
            }
        }
        return rv + getOptionsAsActionOptions(getRequestOptions());
    }

    #! Returns action options corresponding to the given option data for a send message action
    /** @param action action info for the search action
        @param options sample option data that can be used to determine the final type

        @return request options for a send message action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForMessageWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        hash<string, hash<ActionOptionInfo>> rv = {
            "message_id": <ActionOptionInfo>{
                "display_name": "Message",
                "desc": "The message to send",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (map <AllowedValueInfo>{
                    "value": $1.key,
                    "desc": $1.value.desc,
                }, getMessageTypesWithOptions(options).pairIterator()) ?? (),
            },
        };
        return action.options + rv + getOptionsAsActionOptions(getSendMessageOptions());
    }

    #! Returns action options corresponding to the given option data for a search action
    /** @param action action info for the create action
        @param options sample option data that can be used to determine the final type

        @return action options for a search action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForSearchWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        *hash<string, hash<DataProviderExpressionInfo>> expressions = getExpressionsWithOptions(options);
        return action.options + {
            "where_cond": getWhereCondActionOption(expressions),
        } + getSearchOptionsAsActionOptions();
    }

    #! Returns action options corresponding to the given option data for a create action
    /** @param action action info for the upsert action
        @param options sample option data that can be used to determine the final type

        @return action options for a create action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForCreateWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return addCreateRecordOption(action, options) + getCreateOptionsAsActionOptions();
    }

    #! Returns action options corresponding to the given option data for an upsert action
    /** @param action action info for the update action
        @param options sample option data that can be used to determine the final type

        @return action options for an upsert action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForUpsertWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return addCreateRecordOption(action, options) + getUpsertOptionsAsActionOptions();
    }

    #! Returns action options corresponding to the given option data for an update action
    /** @param action action info for the delete action
        @param options sample option data that can be used to determine the final type

        @return action options for an update action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForUpdateWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        hash<string, hash<ActionOptionInfo>> rv;
        if (*hash<string, AbstractDataField> rt = getUpdateRecordTypeWithOptionsImpl(options)) {
            rv.set = <ActionOptionInfo>{
                "display_name": "Set",
                "desc": "The field data to set on the records matched",
                "type": new HashDataType(AutoHashType, "RecordInfo", rt),
            };
        }
        *hash<string, hash<DataProviderExpressionInfo>> expressions = getExpressionsWithOptions(options);
        return action.options + rv + {
            "where_cond": getWhereCondActionOption(expressions),
        } + getSearchOptionsAsActionOptions();
    }

    #! Returns action options corresponding to the given option data for a delete action
    /** @param action action info for the delete action
        @param options sample option data that can be used to determine the final type

        @return action options for a delete action according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForDeleteWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        *hash<string, hash<DataProviderExpressionInfo>> expressions = getExpressionsWithOptions(options);
        return action.options + {
            "where_cond": getWhereCondActionOption(expressions),
        } + getSearchOptionsAsActionOptions();
    }

    #! Returns action options corresponding to the given option data for a pipeline processor action
    /** @param action action info for the pipeline processor action
        @param options sample option data that can be used to determine the final type

        @return action options for a pipeline processor action according to the available option data

        The default implementation returns the static options unchanged; subclasses can override this to add
        dynamic options (e.g. a \c where_cond option for filters).

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForPipelineProcessorWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        return action.options;
    }

    #! Returns action options corresponding to the given option data
    /** @param action action info to determine options
        @param options sample option data that can be used to determine the final type

        @return action options according to the available option data

        @note This API is meant to provide information about options that can depend on option data

        @since DataProvider 3.1
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        switch (action.action_code) {
            case DPAT_EVENT:
                return tryGetActionOptionsForEventWithOptionsImpl(action, options);
            case DPAT_API:
                return tryGetActionOptionsForRequestWithOptionsImpl(action, options);
            case DPAT_SEND_MESSAGE:
                return tryGetActionOptionsForMessageWithOptionsImpl(action, options);
            case DPAT_FIND:
            case DPAT_FIND_SINGLE:
                return tryGetActionOptionsForSearchWithOptionsImpl(action, options);
            case DPAT_CREATE:
                return tryGetActionOptionsForCreateWithOptionsImpl(action, options);
            case DPAT_UPSERT:
                return tryGetActionOptionsForUpsertWithOptionsImpl(action, options);
            case DPAT_UPDATE:
                return tryGetActionOptionsForUpdateWithOptionsImpl(action, options);
            case DPAT_DELETE:
                return tryGetActionOptionsForDeleteWithOptionsImpl(action, options);
            case DPAT_PIPELINE_PROCESSOR:
                return tryGetActionOptionsForPipelineProcessorWithOptionsImpl(action, options);
        }
        return action.options;
    }

    #! Adds a record option to the given action options based on the given option data
    /**
        @since DataProvider 3.3
    */
    private *hash<string, hash<ActionOptionInfo>> addCreateRecordOption(hash<DataProviderActionInfo> action,
            *hash<auto> options) {
        *hash<string, hash<ActionOptionInfo>> rv = action.options;
        *HashDataType record_type;
        if (*hash<string, AbstractDataField> rt = getCreateRecordTypeWithOptionsImpl(options)) {
            record_type = new HashDataType(AutoHashType, "RecordInfo", rt);
        }
        if (!rv.recs) {
            rv.recs = <ActionOptionInfo>{
                "display_name": "Record Data",
                "desc": "The records to create",
                "type": new SoftListDataType("RecordSet", record_type),
            };
        } else if (record_type) {
            rv.recs.type = new SoftListDataType("RecordSet", record_type);
        }
        return rv;
    }

    #! Returns an action where condition option in the context of the given option data
    /**
        @since DataProvider 3.3
    */
    private hash<ActionOptionInfo> getWhereCondActionOption(
            *hash<string, hash<DataProviderExpressionInfo>> expressions
    ) {
        return <ActionOptionInfo>{
            "display_name": "Search Expression",
            "desc": "A boolean expression to be applied to each record to determine if it will be "
                "processed by the current operation",
            "type": AbstractDataProviderTypeMap."*bool",
            "supports_templates": False,
            "supports_custom_values": False,
            "supports_expressions": True,
            "server_expression_handling": (expressions && expressions != DataProviderGenericExpressions)
                ? True
                : False,
            "preselected": True,
        };
    }

    #! Returns supported expression in the context of the given option data
    /**
        @since DataProvider 3.3
    */
    *hash<string, hash<DataProviderExpressionInfo>> getExpressionsWithOptions(*hash<auto> options) {
        return getStaticInfoImpl().expressions;
    }

    #! Returns an argument for an expression
    /**
        @since DataProvider 2.3
    */
    private hash<DataProviderExpression> getSimpleArgumentIntern(int role, hash<DataProviderInfo> info,
            hash<DataProviderExpressionInfo> expinfo, string key, auto value) {
        # remove any unique suffix from key to get field name
        key =~ s/:.*//g;
        hash<DataProviderExpression> rv;
        if (value.typeCode() == NT_HASH && value.op && value.hasKey("arg")) {
            hash<DataProviderExpressionInfo> expinfo = getExpression(role, value.op, info.expressions);
            auto args;
            args[0] = <DataProviderFieldReference>{"field": key};
            if (value.arg.op && value.arg.arg) {
                if (exists value.arg.arg) {
                    if (value.op_args && (value.op_args != 1)) {
                        args += value.arg.arg;
                    } else {
                        push args, value.arg.arg;
                    }
                }
                args = <DataProviderExpression>{
                    "exp": getExpression(role, value.arg.op, info.expressions).name,
                    "args": args,
                };
            } else {
                if (exists value.arg) {
                    if (value.op_args && (value.op_args != 1)) {
                        args += value.arg;
                    } else {
                        push args, value.arg;
                    }
                }
            }
            rv = <DataProviderExpression>{
                "exp": expinfo.name,
                "args": args,
            };
        } else {
            rv = getEqualityComparisonExpression(role, info, key, value);
        }
        int caps = role == ER_Search ? info.search_logic_capabilities : 0;
        verifyExpression(role, info.expressions, caps, AbstractDataProviderTypeMap."bool", rv);
        return rv;
    }

    #! Returns an equality comparison expression
    /** Must be reimplemented by data provider implementations that support expressions and use a symbol different
        from '=' for the comparison operator

        @since DataProvider 2.3
    */
    private hash<DataProviderExpression> getEqualityComparisonExpression(int role, hash<DataProviderInfo> info,
            string key, auto value) {
        if (!info.expressions."==") {
            throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("data provider %s (type %y) provides no '==' operator; "
                "operators provided: %y", getName(), self.className(), keys info.expressions);
        }
        return <DataProviderExpression>{
            "exp": "==",
            "args": (<DataProviderFieldReference>{"field": key}, value),
        };
    }

    #! Returns an expression definition for an expression code or throws an exception
    /**
        @since DataProvider 2.3
    */
    static private hash<DataProviderExpressionInfo> getExpression(int role, string exp,
            *hash<string, hash<DataProviderExpressionInfo>> expmap) {
        *hash<DataProviderExpressionInfo> expinfo = expmap{exp} ?? expmap{OperatorAliasMap{exp}};
        if (!expinfo) {
            throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("expression %y is invalid; known expressions: %y", exp,
                keys expmap);
        }
        if (!(expinfo.role & role)) {
            throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("expression %y is invalid in role %y; valid "
                "expressions for this role: %y", exp, RoleCodeMap{role},
                (map $1.key, expmap.pairIterator(), $1.value & role));
        }
        return expinfo;
    }

    #! Ensures that no callbacks can be set for dynamic URI resolution
    /** @return False if this call was ignored because callbacks are locked, True if the call was successful

        @note after this call, callbacks cannot be changed
    */
    static bool setDynamicValueCallbacks() {
        if (!AbstractDataProvider::checkCallbacks()) {
            return !exists cb_resolve_value;
        }
        return True;
    }

    #! Evaluates the given expression with the generic internal implementation and returns the result
    /** @param rec the record hash to evaluate against
        @param val the value to evaluate (field reference, template reference, expression, or literal)
        @param template_context optional context hash passed through to template callbacks

        @since DataProvider 2.3
    */
    static auto evalGenericExpressionValue(hash<auto> rec, auto val, *hash<auto> template_context) {
        if (val instanceof hash<DataProviderFieldReference>) {
            # allow for dotted references to be respected; allow escaping of dots
            list<string> f = map regex_subst($1, "\\\\.", ".", RE_Global), val.field.splitRegex("(?<!\\\\)\\.");
            auto rv = rec;
            foreach string field in (f) {
                if (rv.typeCode() != NT_HASH && rv.typeCode() != NT_OBJECT) {
                    # only throw exceptions on the top-level key
                    if (!$#) {
                        throw "INVALID-OPERATION", sprintf("expression references invalid key %y of a value of type "
                            "%y in field reference %y", field, rv.fullType(), val.field);
                    }
                    # otherwise stop searching; no value will be returned
                    break;
                }
                auto new_rv = rv{field};
                # only throw exceptions on the top-level key
                if (!$# && !exists new_rv && !rv.hasKey(field)) {
                    throw "INVALID-OPERATION", sprintf("expression references invalid key %y in field reference "
                        "%y; valid keys: %y", field, val.field, keys rv);
                }
                rv = new_rv;
            }
            return rv;
        }
        if (val instanceof hash<DataProviderTemplateReference>) {
            if (!cb_expand_template) {
                throw "TEMPLATE-RESOLUTION-ERROR",
                    sprintf("template reference %y cannot be resolved: no template callback registered", val.raw);
            }
            return call_function_args(cb_expand_template,
                (val.tmpl_context, val.tmpl_value, template_context));
        }
        if (val instanceof hash<DataProviderExpression>) {
            return AbstractDataProvider::evalGenericExpression(rec, val, template_context);
        }
        return val;
    }

    #! Applies key access to a value (used by DP_FUNC_KEY expression operator)
    /** @param value the value to access (must be a hash, or a list to map over)
        @param key_list the keys to access

        @return for a single key, the value; for multiple keys, a hash subset

        @since DataProvider 3.3
    */
    static auto applyKeyAccessorImpl(auto value, list<string> key_list) {
        # Handle list values by mapping key accessor over each element
        # Use push to preserve nesting (avoid flattening nested lists)
        if (value.typeCode() == NT_LIST) {
            list<auto> result = ();
            foreach auto elem in (value) {
                push result, applyKeyAccessorImpl(elem, key_list);
            }
            return result;
        }
        if (value.typeCode() != NT_HASH && value.typeCode() != NT_OBJECT) {
            throw "INVALID-OPERATION", sprintf("key accessor applied to non-hash value of type %y",
                value.fullType());
        }
        if (key_list.size() == 1) {
            # Single key: return value directly
            return value{key_list[0]};
        }
        # Multiple keys: return hash subset
        hash<auto> result = {};
        foreach string key in (key_list) {
            if (value.hasKey(key)) {
                result{key} = value{key};
            }
        }
        return result;
    }

    #! Evaluates the given expression with the generic internal implementation and returns the result
    /** @param rec the record hash to evaluate against
        @param exp the expression to evaluate
        @param template_context optional context hash passed through to template callbacks

        @since DataProvider 2.3
    */
    static auto evalGenericExpression(hash<auto> rec, hash<DataProviderExpression> exp,
            *hash<auto> template_context) {
        *hash<auto> expinfo = AbstractDataProvider::GenericExpressionImplementations{exp.exp}
            ?? AbstractDataProvider::GenericExpressionImplementations{OperatorAliasMap{exp.exp}};
        if (!expinfo) {
            throw "INVALID-OPERATION", sprintf("operator %y is unknown; supported "
                "operators: %y", exp.exp, keys GenericExpressionImplementations);
        }
        on_error rethrow $1.err, sprintf("error in return value type %y for %y expression: %s",
            expinfo.exp.return_type.getName(), expinfo.exp.name, $1.desc);
        return expinfo.exp.return_type.acceptsValue(expinfo.impl(rec, exp, template_context));
    }

    #! Set callbacks for dynamic URI resolution to allow for variable URI path elements to be resolved at runtime
    /** @param value_needs_resolution signature <tt>bool sub (string value) { ... }</tt>; this callback will be used
        to check if a URI path needs dynamic resolution
        @param resolve_value signature <tt>auto sub (auto value) { ... }</tt>; if \c value_needs_resolution returns
        @ref True on a URI path, then this callback will be called at runtime for each request to resolve the URI path
        to the final path

        @return False if this call was ignored because callbacks are locked, True if the call was successful

        @note after this call, callbacks cannot be changed
    */
    static bool setDynamicValueCallbacks(code value_needs_resolution, code resolve_value) {
        if (!AbstractDataProvider::checkCallbacks()) {
            return value_needs_resolution == cb_value_needs_resolution && resolve_value == cb_resolve_value;
        }
        cb_value_needs_resolution = value_needs_resolution;
        cb_resolve_value = resolve_value;
        return True;
    }

    #! Sets callbacks for template reference resolution
    /** @param expand resolves a template reference to its value
        @param resolve_type optional; returns the expected type for a template reference
        @param list_contexts optional; returns available template contexts for completion
        @param list_values optional; returns available template values for a context for completion

        @return @ref True if callbacks were successfully set (first call) or if the same callbacks are being
        re-registered; @ref False if different callbacks are already locked

        @note
        - After this call, template callbacks are locked and cannot be changed (except via @ref clearTemplateCallbacks)
        - When locked, re-registration with the same callbacks returns @ref True; with different callbacks returns
          @ref False. The comparison uses Qore's \c == operator on closures, which performs identity comparison
          (same closure reference), not semantic equality
        - This method is designed to be called once at startup before any concurrent access; the static callback
          variables are not protected by a lock, following the same pattern as @ref setDynamicValueCallbacks().
          Concurrent modification after initialization is not supported

        @since DataProvider 3.3
    */
    static bool setTemplateCallbacks(
            code<auto(string, string, *hash<auto>)> expand,
            *code<*AbstractDataProviderType(string, string, *hash<auto>)> resolve_type,
            *code<auto(*hash<auto>)> list_contexts,
            *code<auto(string, string, *hash<auto>)> list_values) {
        if (template_callbacks_locked) {
            return expand == cb_expand_template && resolve_type == cb_resolve_template_type
                && list_contexts == cb_list_template_contexts && list_values == cb_list_template_values;
        }
        template_callbacks_locked = True;
        cb_expand_template = expand;
        cb_resolve_template_type = resolve_type;
        cb_list_template_contexts = list_contexts;
        cb_list_template_values = list_values;
        return True;
    }

    #! Clears all template callbacks and resets the lock
    /** This method is primarily for testing purposes.

        @since DataProvider 3.3
    */
    static clearTemplateCallbacks() {
        template_callbacks_locked = False;
        delete cb_expand_template;
        delete cb_resolve_template_type;
        delete cb_list_template_contexts;
        delete cb_list_template_values;
    }

    #! Returns the list of available template contexts, if a callback is registered
    /** @param template_context optional context data for filtering (e.g., sandboxed, interface_type)
        @return a list of template context info hashes, or NOTHING if no callback is registered

        @since DataProvider 3.3
    */
    static *list<hash<auto>> getTemplateContexts(*hash<auto> template_context) {
        if (!cb_list_template_contexts) {
            return NOTHING;
        }
        return call_function_args(cb_list_template_contexts, (template_context,));
    }

    #! Returns the list of available template values for a context, if a callback is registered
    /** @param ctx the template context name
        @param prefix the value prefix to filter by
        @param template_context optional context data for filtering (e.g., sandboxed, interface_type)
        @return a list of template value info hashes, or NOTHING if no callback is registered

        @since DataProvider 3.3
    */
    static *list<hash<auto>> getTemplateValues(string ctx, string prefix,
            *hash<auto> template_context) {
        if (!cb_list_template_values) {
            return NOTHING;
        }
        return call_function_args(cb_list_template_values, (ctx, prefix, template_context));
    }

    #! Resolves the type of a template reference using the registered callback
    /** @param ctx the template context name (e.g., "env", "timestamp")
        @param value the template value path (e.g., "HOME", "timegm")
        @param template_context optional context data for filtering (e.g., sandboxed, interface_type)
        @return the resolved type, or NOTHING if no callback is registered or the type cannot be determined

        @since DataProvider 3.3
    */
    static *AbstractDataProviderType resolveTemplateType(string ctx, string value,
            *hash<auto> template_context) {
        if (!cb_resolve_template_type) {
            return NOTHING;
        }
        return call_function_args(cb_resolve_template_type, (ctx, value, template_context));
    }

    #! processes an expression in a certain context
    /** @param role the role of the operation being performed
        @param expmap the map of supported expressions for this data provider
        @param caps the logic capabilities supported by the data provider for the current operation
        @param expected_type the type the expression should be compatible with
        @param exp the expression to process

        @throw DATA-PROVIDER-EXPRESSION-ERROR the expression has an error; argument mismatch, type error, etc
    */
    static verifyExpression(int role, *hash<string, hash<DataProviderExpressionInfo>> expmap, int caps,
            AbstractDataProviderType expected_type, hash<DataProviderExpression> exp) {
        hash<DataProviderExpressionInfo> expinfo = AbstractDataProvider::getExpression(role, exp.exp, expmap);
        return AbstractDataProvider::verifyExpression(role, expinfo, caps, expected_type, exp);
    }

    #! processes an expression in a certain context
    /** @param role the role of the operation being performed
        @param expinfo the definition of the expression to evaluate
        @param caps the logic capabilities supported by the data provider for the current operation
        @param expected_type the type the expression should be compatible with
        @param exp the expression to process

        @throw DATA-PROVIDER-EXPRESSION-ERROR the expression has an error; argument mismatch, type error, etc
    */
    static verifyExpression(int role, hash<DataProviderExpressionInfo> expinfo, int caps,
            AbstractDataProviderType expected_type, hash<DataProviderExpression> exp) {
        # check arguments
        if (exp.args.lsize() > expinfo.args.size() && !expinfo.varargs) {
            throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("Value provided for expression %y has too many values "
                "(%d); only max %d are accepted (%y)", expinfo.name, exp.args.lsize(), expinfo.args.size(), exp.args);
        }
        map AbstractDataProvider::verifyExpressionArgValue(role, caps, expinfo, exp.args, $#), expinfo.args;
        if (exp.args.lsize() > expinfo.args.size()) {
            # process the rest of the arguments
            map AbstractDataProvider::verifyExpressionArgValue(role, caps, expinfo, exp.args, expinfo.args.size() + $#),
                xrange(exp.args.lsize() - expinfo.args.size());
        }
        if (!expected_type.isAssignableFrom(expinfo.return_type)) {
            throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("expecting a %y value, but expression %y returns a %y "
                "value instead", expected_type.getName(), expinfo.name, expinfo.return_type.getName());
        }
    }

    #! processes an expression in a certain context
    /** @param role the role of the operation being performed
        @param caps the logic capabilities supported by the data provider for the current operation
        @param expinfo the expression being processed
        @param values the values / arguments for the expression
        @param pos the position in the argument list
    */
    static verifyExpressionArgValue(int role, int caps, hash<DataProviderExpressionInfo> expinfo,
            softlist<auto> values, int pos) {
        hash<DataProviderSignatureTypeInfo> arginfo = expinfo.args[pos] ?? expinfo.args.last();
        if (arginfo.type_code == ST_Any) {
            return;
        }
        auto val = values[pos];
        bool is_exp = val instanceof hash<DataProviderExpression>;
        bool is_ref = is_exp ? False : val instanceof hash<DataProviderFieldReference>;
        bool is_tmpl = !is_exp && !is_ref && val instanceof hash<DataProviderTemplateReference>;

        switch (arginfo.type_code) {
            case ST_Any:
                return;
            case ST_Value:
                if (is_exp || is_ref || is_tmpl) {
                    throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("expression %y argument %d/%d expects a value; "
                        "got %y instead", expinfo.name, pos + 1, values.size(),
                        is_exp ? "nested expression" : (is_ref ? "field reference" : "template reference"));
                }
                if (!arginfo.type.isAssignableFrom(Type::getType(val))) {
                    throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("argument %d expects type %y; got %y instead",
                        pos + 1, arginfo.type.getName(), val.fullType());
                }
                return;
            case ST_Field:
                if (!is_ref) {
                    throw "DATA-PROVIDER-EXPRESSION-ERROR", sprintf("expression %y argument %d/%d expects a field "
                        "reference; got %y instead", expinfo.name, pos + 1, values.size(),
                        is_exp ? "nested expression" : (is_tmpl ? "template reference" : "value"));
                }
                return;
        }
    }

    #! Checks if callbacks have already been set or locked
    private static bool checkCallbacks() {
        if (callbacks_locked) {
            return False;
        }
        return callbacks_locked = True;
    }

    #! validates create options
    /** @throw CREATE-OPTION-ERROR invalid or unsupported create option
    */
    private *hash<auto> validateCreateOptions(*hash<auto> create_options) {
        return checkOptions("CREATE-OPTION-ERROR", getCreateOptions(), create_options);
    }

    #! validates upsert options
    /** @throw UPSERT-OPTION-ERROR invalid or unsupported upsert option
    */
    private *hash<auto> validateUpsertOptions(*hash<auto> upsert_options) {
        return checkOptions("UPSERT-OPTION-ERROR", getUpsertOptions(), upsert_options);
    }

    #! validates search options
    /** @throw SEARCH-OPTION-ERROR invalid or unsupported search option
    */
    private *hash<auto> validateSearchOptions(*hash<auto> search_options) {
        return checkOptions("SEARCH-OPTION-ERROR", getSearchOptions(), search_options);
    }

    #! validates request options
    /** @throw REQUEST-OPTION-ERROR invalid or unsupported request option
    */
    private *hash<auto> validateRequestOptions(*hash<auto> request_options) {
        return checkOptions("REQUEST-OPTION-ERROR", getRequestOptions(), request_options);
    }

    #! validates child create options
    /** @throw CHILD-CREATE-OPTION-ERROR invalid or unsupported child create option

        @since DataProvider 2.0
    */
    private *hash<auto> validateChildCreateOptions(*hash<auto> child_create_options) {
        return checkOptions("CHILD-CREATE-OPTION-ERROR", getChildCreateOptions(), child_create_options);
    }

    #! validates child delete options
    /** @throw CHILD-DELETE-OPTION-ERROR invalid or unsupported child delete option

        @since DataProvider 2.0
    */
    private *hash<auto> validateChildDeleteOptions(*hash<auto> child_delete_options) {
        return checkOptions("CHILD-DELETE-OPTION-ERROR", getChildDeleteOptions(), child_delete_options);
    }

    #! validates field add options
    /** @throw FIELD-ADD-OPTION-ERROR invalid or unsupported field add option

        @since DataProvider 2.0
    */
    private *hash<auto> validateFieldAddOptions(*hash<auto> field_add_options) {
        return checkOptions("FIELD-ADD-OPTION-ERROR", getFieldAddOptions(), field_add_options);
    }

    #! validates field update options
    /** @throw FIELD-UPDATE-OPTION-ERROR invalid or unsupported field update option

        @since DataProvider 2.0
    */
    private *hash<auto> validateFieldUpdateOptions(*hash<auto> field_update_options) {
        return checkOptions("FIELD-UPDATE-OPTION-ERROR", getFieldUpdateOptions(), field_update_options);
    }

    #! validates field delete options
    /** @throw FIELD-DELETE-OPTION-ERROR invalid or unsupported field delete option

        @since DataProvider 2.0
    */
    private *hash<auto> validateFieldDeleteOptions(*hash<auto> field_delete_options) {
        return checkOptions("FIELD-DELETE-OPTION-ERROR", getFieldDeleteOptions(), field_delete_options);
    }

    #! validates send message options
    /** @throw SEND-MESSAGE-OPTION-ERROR invalid or unsupported send message option

        @since DataProvider 2.5
    */
    private *hash<auto> validateSendMessageOptions(*hash<auto> send_message_options) {
        return checkOptions("SEND-MESSAGE-OPTION-ERROR", getSendMessageOptions(), send_message_options);
    }

    #! Processes options passed to the constructor
    private processConstructorOptions(*hash<string, hash<DataProviderOptionInfo>> option_desc, *hash<auto> options,
            *string required_options) {
        self += checkOptions("CONSTRUCTOR-OPTION-ERROR", option_desc, options, required_options);
    }

    #! Verifies options according to the option
    /**
        @param err The error code in case of any errors; this is the exception code (\c err) that will be raised
        @param option_desc Description of the options
        @param uoptions the options provided
        @param required_options If provided, it indicates that there are multiple possibilities for the set of
        required options; the format of this string is the required option names separated by commas (\c ",");
        multiple possibilities must be separated by pipe chars (\c "|""); ex: client_id,client_secret,tenant|token;
        if this option is passed, then the \c required attribute of options is ignored

        @return All options after processing

        @note The error code passed as \c err is thrown if any errors are detected
    */
    private *hash<auto> checkOptions(string err, *hash<string, hash<DataProviderOptionInfo>> option_desc,
            *hash<auto> uoptions, *string required_options) {
        # to ensure that the return value has type "hash<auto>"
        hash<auto> options += uoptions;
        if (!option_desc && options) {
            error(err, "invalid options: %y; no options are supported",
                keys options);
        }
        if (*list<string> invalid_keys = keys (options - (keys option_desc))) {
            error(err, "invalid options: %y; supported options: %y", invalid_keys, keys option_desc);
        }
        # check types and required keys
        foreach hash<auto> i in (option_desc.pairIterator()) {
            # NOTE: treat the case when the key is present and the value is missing as if the key were not there
            bool present = exists options{i.key};
            if (!present) {
                if (exists i.value.default_value) {
                    options{i.key} = i.value.default_value;
                    continue;
                } else if (!required_options && i.value.required) {
                    error(err, "missing required option %y%s", i.key,
                        i.value.desc ? sprintf(" (%s)", i.value.desc) : "");
                }
                continue;
            }
            # process value
            bool ok;
            string err;
            foreach AbstractDataProviderType type in (i.value.type) {
                try {
                    options{i.key} = type.acceptsValue(options{i.key});
                    ok = True;
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "RUNTIME-TYPE-ERROR") {
                        err = sprintf("option %y of type %y cannot be assigned a value of type %y (value %y): %s: %s",
                            i.key, type.getName(), options{i.key}.type(), options{i.key}, ex.err, ex.desc);
                        continue;
                    }
                    rethrow;
                }
            }
            if (!ok) {
                if (err) {
                    error("RUNTIME-TYPE-ERROR", err);
                }
                error("RUNTIME-TYPE-ERROR", "no type information available for option %y", i.key);
            }
            # check allowed values
            if (i.value.allowed_values && !i.value.allowed_values_creatable) {
                bool ok = False;
                foreach auto v in (i.value.allowed_values) {
                    if (options{i.key} === v.value) {
                        ok = True;
                        break;
                    }
                }
                if (!ok) {
                    error("OPTION-VALUE-ERROR", sprintf("option %y does not accept value %y; possible values: %y",
                        i.key, options{i.key}, (map $1.value, i.value.allowed_values)));
                }
            }
            if (i.value.element_allowed_values && !i.value.element_allowed_values_creatable) {
                foreach auto opt in (options{i.key}) {
                    bool ok = False;
                    foreach auto v in (i.value.element_allowed_values) {
                        if (opt === v.value) {
                            ok = True;
                            break;
                        }
                    }
                    if (!ok) {
                        error("OPTION-VALUE-ERROR", sprintf("Option %y does not accept element value %y; possible "
                            "element values: %y", i.key, opt, (map $1.value, i.value.element_allowed_values)));
                    }
                }
            }
        }
        # verify required options if "required_options" is set
        if (required_options) {
            verifyRequiredOptions(err, options, required_options);
        }
        return options;
    }

    #! Verifies options in case multiple combinations of options are supported
    /**
        @param err The error code in case of any errors; this is the exception code (\c err) that will be raised
        @param options The options provided
        @param required_options The format of this string is the required option names separated by commas (\c ",");
        multiple possibilities must be separated by pipe chars (\c "|""); ex: client_id,client_secret,tenant|token

        @note The error code passed as \c err is thrown if any errors are detected
    */
    private verifyRequiredOptions(string err, *hash<auto> options, string required_options) {
        list<string> req_list = required_options.split("|");
        bool ok;
        list<string> missing;
        foreach string req in (req_list) {
            list<string> req_opt = req.split(",");
            # check which options are missing
            *hash<auto> have = options{req_opt};
            if (have.size() == req_opt.size()) {
                ok = True;
                break;
            }
            if (req_list.size() == 1) {
                missing = keys ((map {$1: True}, req_opt) - keys have);
            }
        }
        if (!ok) {
            if (missing) {
                throw err, sprintf("missing required options %y; options provided: %y", missing, options);
            } else {
                throw err, sprintf("missing required options %s; options provided: %y",
                    (foldl $1 + " OR " + $2, (map sprintf("%y", $1), req_list)), options);
            }
        }
    }

    #! processes search parameter expressions
    /** @param exp the expression to process
        @param search_options the search options after processing by validateSearchOptions()

        @return the processed expression
    */
    private *hash<DataProviderExpression> processSearchParameters(*hash<DataProviderExpression> exp,
            *hash<auto> search_options) {
        if (!exp) {
            return exp;
        }
        checkSearchExpressions();
        hash<DataProviderInfo> info = getInfo();
        verifyExpression(ER_Search, info.expressions, info.search_logic_capabilities,
            AbstractDataProviderTypeMap."bool", exp);
        return exp;
    }

    #! processes search or set values to convert types if necessary
    /** @param h the hash of field values to process
        @param search_options the search options after processing by validateSearchOptions()

        @return processed field values, all non-hash values of \a h are processed with the field's type to ensure
        that the values are valid for the comparison or set operation; hash values are assumed to be provider-
        specific comparison or set operations and should be processed by the provider-specific method
    */
    private *hash<auto> processFieldValues(*hash<auto> h, *hash<auto> search_options) {
        if (!h) {
            return h;
        }
        *hash<string, AbstractDataField> rec = getRecordTypeImpl(search_options);
        hash<auto> rv;
        foreach hash<auto> i in (h.pairIterator()) {
            if (!rec{i.key}) {
                list<string> f = map regex_subst($1, "\\\\.", ".", RE_Global), i.key.splitRegex("(?<!\\\\)\\.");
                if (rec && !rec{f[0]} && !searchAcceptsForeignField(f[0])) {
                    error("INVALID-FIELD", "field %y is not a defined field; valid fields: %y", f[0], keys rec);
                }
                rv{i.key} = i.value;
                continue;
            }
            # we assume a hash is a provider-specific search option
            if (i.value.typeCode() == NT_HASH) {
                rv{i.key} = i.value;
                continue;
            }
            rv{i.key} = rec
                ? rec{i.key}.acceptsValue(i.value)
                : i.value;
        }
        return rv;
    }

    #! Allows data providers to accept foreign or virtual field names
    bool searchAcceptsForeignField(string field) {
        return False;
    }

    #! Returns the description of the record type, if any
    /** @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type or @ref NOTHING if the data provider does not support a record type
    */
    *hash<string, AbstractDataField> getRecordType(*hash<auto> search_options) {
        if (!hasRecord()) {
            return;
        }
        return getRecordTypeImpl(recordRequiresSearchOptions() ? validateSearchOptions(search_options) : NOTHING);
    }

    #! Returns the description of the record type for creating records, if any
    /** @param create_options the create options; will be processed by validateCreateOptions()

        @return the record type for creating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.0
    */
    *hash<string, AbstractDataField> getCreateRecordType(*hash<auto> create_options) {
        if (!hasRecord()) {
            return;
        }
        return getCreateRecordTypeImpl(validateCreateOptions(create_options));
    }

    #! Returns the description of the record type for updating records, if any
    /** @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type for updating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.0
    */
    *hash<string, AbstractDataField> getUpdateRecordType(*hash<auto> search_options) {
        if (!hasRecord()) {
            return;
        }
        return getUpdateRecordTypeImpl(recordRequiresSearchOptions() ? validateSearchOptions(search_options) :
            NOTHING);
    }

    #! Returns the description of the record type with soft types, if any
    *hash<string, AbstractDataField> getSoftRecordType(*hash<auto> search_options) {
        return map {
            $1.key: $1.value.getSoftType()
        }, getRecordType(search_options).pairIterator();
    }

    #! Returns the description of the record type with "or nothing" types, if any
    *hash<string, AbstractDataField> getOrNothingRecordType(*hash<auto> search_options) {
        return map {
            $1.key: $1.value.getOrNothingType()
        }, getRecordType(search_options).pairIterator();
    }

    #! Returns the description of the record type in the context of the given options
    /** @param options action options that may influence the record type
        @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type or @ref NOTHING if the data provider does not support a record type

        @since DataProvider 3.3
    */
    *hash<string, AbstractDataField> getRecordTypeWithOptions(*hash<auto> options, *hash<auto> search_options) {
        if (!hasRecord()) {
            return;
        }
        if (recordRequiresSearchOptions()) {
            search_options = validateSearchOptions(search_options);
        } else {
            remove search_options;
        }
        return getRecordTypeWithOptionsImpl(options, search_options);
    }

    #! Returns the description of the record type for creating records in the context of the given options
    /** @param options action options that may influence the record type
        @param create_options the create options; will be processed by validateCreateOptions()

        @return the record type for creating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.3
    */
    *hash<string, AbstractDataField> getCreateRecordTypeWithOptions(*hash<auto> options, *hash<auto> create_options) {
        if (!hasRecord()) {
            return;
        }
        create_options = validateCreateOptions(create_options);
        return getCreateRecordTypeWithOptionsImpl(options, create_options);
    }

    #! Returns the description of the record type for updating records in the context of the given options
    /** @param options action options that may influence the record type
        @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type for updating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.3
    */
    *hash<string, AbstractDataField> getUpdateRecordTypeWithOptions(*hash<auto> options, *hash<auto> search_options) {
        if (!hasRecord()) {
            return;
        }
        if (recordRequiresSearchOptions()) {
            search_options = validateSearchOptions(search_options);
        } else {
            remove search_options;
        }
        return getUpdateRecordTypeWithOptionsImpl(options, search_options);
    }

    #! Returns the description of the record type with soft types, if any
    *hash<string, AbstractDataField> getSoftRecordTypeWithOptions(*hash<auto> options, *hash<auto> search_options) {
        return map {
            $1.key: $1.value.getSoftType()
        }, getRecordTypeWithOptions(options, search_options).pairIterator();
    }

    #! Returns the description of the record type with "or nothing" types, if any
    *hash<string, AbstractDataField> getOrNothingRecordTypeWithOptions(*hash<auto> options,
            *hash<auto> search_options) {
        return map {
            $1.key: $1.value.getOrNothingType()
        }, getRecordTypeWithOptions(options, search_options).pairIterator();
    }

    #! Returns options that can be used for searching
    /** @return a hash of options that can be used for searching; keys are search option names, values describe the
        search option; if @ref nothing is returned, then the provider does not support any search options
    */
    *hash<string, hash<DataProviderOptionInfo>> getSearchOptions() {
        return getStaticInfoImpl().search_options;
    }

    #! Returns options that can be used for creating records
    /** @return a hash of options that can be used for creating records; keys are option names, values describe the
        option; if @ref nothing is returned, then the provider does not support any creation options
    */
    *hash<string, hash<DataProviderOptionInfo>> getCreateOptions() {
        return getStaticInfoImpl().create_options;
    }

    #! Returns options that can be used for upserting records
    /** @return a hash of options that can be used for upserting records; keys are option names, values describe the
        option; if @ref nothing is returned, then the provider does not support any upsert options
    */
    *hash<string, hash<DataProviderOptionInfo>> getUpsertOptions() {
        return getStaticInfoImpl().upsert_options;
    }

    #! Returns options that can be used for requests
    /** @return a hash of options that can be used for requests; keys are request option names, values describe the
        request option; if @ref nothing is returned, then the provider does not support any request options
    */
    *hash<string, hash<DataProviderOptionInfo>> getRequestOptions() {
        return getStaticInfoImpl().request_options;
    }

    #! Returns options that can be used for creating child data providers
    /** @return a hash of options that can be used for creating child data providers; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any child creation
        options

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getChildCreateOptions() {
        return getStaticInfoImpl().child_create_options;
    }

    #! Returns options that can be used for deleting child data providers
    /** @return a hash of options that can be used for delete child data providers; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any child deletion
        options

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getChildDeleteOptions() {
        return getStaticInfoImpl().child_delete_options;
    }

    #! Returns options that can be used for adding new fields to the data provider
    /** @return a hash of options that can be used for adding new fields; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any add field
        options

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getFieldAddOptions() {
        return getStaticInfoImpl().add_field_options;
    }

    #! Returns options that can be used for updating fields of the data provider
    /** @return a hash of options that can be used for updating fields; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any update field
        options

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getFieldUpdateOptions() {
        return getStaticInfoImpl().update_field_options;
    }

    #! Returns options that can be used for deleting fields from the data provider
    /** @return a hash of options that can be used for deleting fields; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any delete field
        options

        @since DataProvider 2.0
    */
    *hash<string, hash<DataProviderOptionInfo>> getFieldDeleteOptions() {
        return getStaticInfoImpl().delete_field_options;
    }

    #! Returns options that can be used for sending messages from the data provider
    /** @return a hash of options that can be used for sending messages; keys are option names, values
        describe the option; if @ref nothing is returned, then the provider does not support any message send
        options

        @since DataProvider 2.5
    */
    *hash<string, hash<DataProviderOptionInfo>> getSendMessageOptions() {
        return getStaticInfoImpl().send_message_options;
    }

    #! Returns a code describing how the data provider supports sending messages
    /** @since DataProvider 2.5
    */
    string getMessageSupport() {
        return getStaticInfoImpl().supports_messages;
    }

    #! Returns @ref True if the data provider supports reading
    /**
    */
    bool supportsRead() {
        return getStaticInfoImpl().supports_read;
    }

    #! Returns @ref True if the data provider supports native bulk reading
    /**
    */
    bool supportsBulkRead() {
        return getStaticInfoImpl().supports_bulk_read;
    }

    #! Returns @ref True if the data provider supports the record creation API
    /**
    */
    bool supportsCreate() {
        return getStaticInfoImpl().supports_create;
    }

    #! Returns @ref True if the data provider supports the record update API
    /**
    */
    bool supportsUpdate() {
        return getStaticInfoImpl().supports_update;
    }

    #! Returns @ref True if the data provider supports the record upsert API
    /**
    */
    bool supportsUpsert() {
        return getStaticInfoImpl().supports_upsert;
    }

    #! Returns @ref True if the data provider supports the record deletion API
    /**
    */
    bool supportsDelete() {
        return getStaticInfoImpl().supports_delete;
    }

    #! Returns @ref True if the data provider supports the record search API natively
    /**
    */
    bool supportsNativeSearch() {
        return getStaticInfoImpl().supports_native_search;
    }

    #! Returns True if the data provider supports bulk creation output
    /**
    */
    bool supportsBulkCreate() {
        return getStaticInfoImpl().supports_bulk_create;
    }

    #! Returns True if the data provider supports bulk upserts
    /**
    */
    bool supportsBulkUpsert() {
        return getStaticInfoImpl().supports_bulk_upsert;
    }

    #! Returns True if the data provider supports requests
    /**
    */
    bool supportsRequest() {
        return getStaticInfoImpl().supports_request;
    }

    #! Returns True if the data provider requires search options to retrieve the record type
    /**
    */
    bool recordRequiresSearchOptions() {
        return getStaticInfoImpl().record_requires_search_options;
    }

    #! Returns @ref True if the data provider has a record type
    /** @return @ref True if the data provider has a record type
    */
    bool hasRecord() {
        return getStaticInfoImpl().has_record;
    }

    #! Returns True if the data provider supports creating new child data providers
    /** @since DataProvider 2.0
    */
    bool supportsCreateChild() {
        return getStaticInfoImpl().supports_child_create;
    }

    #! Returns True if the data provider supports creating deleting child data providers
    /** @since DataProvider 2.0
    */
    bool supportsDeleteChild() {
        return getStaticInfoImpl().supports_child_delete;
    }

    #! Returns True if the data provider supports creating adding new fields
    /** @since DataProvider 2.0
    */
    bool supportsAddField() {
        return getStaticInfoImpl().supports_add_field;
    }

    #! Returns True if the data provider supports updating existing fields
    /** @since DataProvider 2.0
    */
    bool supportsUpdateField() {
        return getStaticInfoImpl().supports_update_field;
    }

    #! Returns True if the data provider supports deleting existing fields
    /** @since DataProvider 2.0
    */
    bool supportsDeleteField() {
        return getStaticInfoImpl().supports_delete_field;
    }

    #! Returns True if the data provider supports a schema
    /** @since DataProvider 2.2
    */
    bool supportsSchema() {
        return getStaticInfoImpl().supports_schema;
    }

    #! Returns True if the data provider supports advanced search expressions
    /** @since DataProvider 2.2
    */
    bool supportsSearchExpressions() {
        return getStaticInfoImpl().supports_search_expressions;
    }

    #! Returns True if the data provider supports the observer pattern / event API
    /** @since DataProvider 2.5
    */
    bool supportsObservable() {
        return getStaticInfoImpl().supports_observable;
    }

    #! Returns True if the data provider supports sending messages
    /** @since DataProvider 2.5
    */
    bool supportsMessages() {
        return getStaticInfoImpl().supports_messages != MSG_None;
    }

    #! Returns the schema type for this data provider or @ref nothing if no schema type is supported
    /** @since DataProvider 2.2
    */
    *object getSchemaType() {
        return getStaticInfoImpl().schema_type;
    }

    #! Returns True if the data provider supports children
    /** @since DataProvider 3.0
    */
    bool supportsChildren() {
        return getStaticInfoImpl().supports_children;
    }

    #! Returns True if the data provider's children are all of the same class and all support the same options
    /** @since DataProvider 3.0
    */
    bool childrenIdentical() {
        return getStaticInfoImpl().children_identical;
    }

    #! Registers a child data provider if the data provider supports children and is extensible
    /** @param name the child's name
        @param generator code to generate the child; prototype: sub (string name, AbstractConnection conn) {}

        @throw UNSUPPORTED-ERROR this exception is thrown by default, to support adding children, override this method

        @since DataProvider 3.0
    */
    static registerChild(string name, code generator) {
        throw "UNSUPPORTED-ERROR", "This data provider does not support dynamically adding child providers";
    }

    #! thrown an exception
    private error(string err, string fmt, ...) {
        string desc_name;
        # the getName() method cannot be run in the constructor before the object has been created and the name set
        try {
            desc_name = sprintf("%y (%y)", getName(), self.className());
        } catch (hash<ExceptionInfo> ex) {
            # this block intentionally left blank
        }
        if (!desc_name) {
            desc_name = sprintf("class %y", self.className());
        }
        throw err, sprintf("data provider %s: %s", desc_name, vsprintf(fmt, argv));
    }

    #! Returns the first record matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @note This default method returns immediately as soon as the first matching record is found
    */
    private *hash<auto> searchFirstRecordImpl(hash<auto> where_cond,
            *hash<auto> search_options) {
        # scans the whole data set to find a single record
        AbstractDataProviderRecordIterator i = searchRecordsImpl(where_cond, search_options);
        if (i.next()) {
            return i.getValue();
        }
    }

    #! Returns a single record matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @throw MULTIPLE-RECORDS-ERROR multiple records found

        @note This default methods searches the entire data set for a matching record
    */
    private *hash<auto> searchSingleRecordImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        # scans the whole data set to find a single record
        *list<*hash<auto>> records = map $1, searchRecordsImpl(where_cond, search_options);
        if (records.lsize() > 1) {
            error("MULTIPLE-RECORDS-ERROR", "%d records found for search: %y", records.size(), where_cond);
        }
        return records[0];
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any

        This base class method returns @ref nothing
    */
    private *list<string> getChildProviderNamesImpl() {
        # intentionally empty
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        # intentionally empty
    }

    #! Returns connection info if the data provider supports connections, otherwise returns @ref NOTHING
    /** @return connection info if the data provider supports connections, otherwise returns @ref NOTHING

        @since DataProvider 3.0
    */
    private *hash<DataProviderConnectionInfo> getConnectionInfoImpl() {
        # intentionally empty
    }

    #! Generates example data for the given fields
    /** @param fields the fields to generate data for

        @return example data for the given fields

        @since DataProvider 3.0
    */
    private hash<auto> generateExampleData(hash<string, AbstractDataField> fields) {
        return map {$1.key: $1.value.getExampleValue()}, fields.pairIterator();
    }

    #! Returns the description of the record type, if any
    /** @param search_options any search options
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Returns the description of the record type for updates, if any
    /** @param search_options any search options
    */
    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return getRecordTypeImpl(search_options);
    }

    #! Returns the description of the record type for creating records, if any
    /** @param create_options the create options
    */
    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> create_options) {
        return getRecordTypeImpl();
    }

    #! Returns the description of the record type in the context of the given options
    /** @param options action options that may influence the record type
        @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type or @ref NOTHING if the data provider does not support a record type

        @since DataProvider 3.3
    */
    private *hash<string, AbstractDataField> getRecordTypeWithOptionsImpl(*hash<auto> options,
            *hash<auto> search_options) {
        return getRecordTypeImpl(search_options);
    }

    #! Returns the description of the record type for creating records in the context of the given options
    /** @param options action options that may influence the record type
        @param create_options the create options; will be processed by validateCreateOptions()

        @return the record type for creating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.3
    */
    private *hash<string, AbstractDataField> getCreateRecordTypeWithOptionsImpl(*hash<auto> options,
            *hash<auto> create_options) {
        return getCreateRecordTypeImpl(create_options);
    }

    #! Returns the description of the record type for updating records in the context of the given options
    /** @param options action options that may influence the record type
        @param search_options to be included and processed by validateSearchOptions() if
        \a recordRequiresSearchOptions() is @ref True for this provider, otherwise any value provided in this argument
        is ignored

        @return the record type for updating records or @ref NOTHING if the data provider does not support a record
        type

        @since DataProvider 3.3
    */
    private *hash<string, AbstractDataField> getUpdateRecordTypeWithOptionsImpl(*hash<auto> options,
            *hash<auto> search_options) {
        return getUpdateRecordTypeImpl(search_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param block_size the number of records in a read block; must be a positive number
        @param where_cond the search criteria; after processing by processSearchParameters()
        @param search_options the search options after processing by validateSearchOptions()

        @throw INVALID-BLOCK-SIZE the block size must be a positive number
        @throw INVALID-OPERATION the data provider does not support reading
    */
    private AbstractDataProviderBulkRecordInterface searchRecordsBulkImpl(int block_size = 1000,
            *hash<auto> where_cond, *hash<auto> search_options) {
        return new DefaultBulkRecordInterface(block_size, searchRecordsImpl(where_cond, search_options),
            search_options.requires_result, search_options.max_records, search_options.limit);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria; after processing by processSearchParameters()
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Returns an iterator for zero or more records matching the search options according to an API request
    /** @param req the request to serialize and make according to the request type
        @param where_cond the search criteria; after processing by processSearchParameters()
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned

        @see searchRecordsImpl()
    */
    private AbstractDataProviderRecordIterator requestSearchRecordsImpl(auto req, *hash<auto> where_cond,
            *hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Creates the given record in the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        throwUnimplementedException();
    }

    #! Creates the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are field names and values are either
        lists of field values or a single value that applies to all records
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider for each record in bulk format (keys are field names, values
        are lists of field values)
    */
    private *hash<auto> createRecordsBulkImpl(hash<auto> recs, *hash<auto> create_options) {
        throwUnimplementedException();
    }

    #! Upserts the given record to the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the create options after processing by validateUpsertOptions()

        @return see UpsertResultInserted and related constants for possible values
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        throwUnimplementedException();
    }

    #! Upserts the given records in the data provider
    /** @param recs a hash representing multiple input records, where the keys are field names and values are either
        lists of field values or a single value that applies to all records to be upserted
        @param upsert_options the upsert options; will be processed by validateUpsertOptions()

        @return an array of upsert results corresponding to each record in order; see UpsertResultInserted and related constants
        for possible values
    */
    private *list<string> upsertRecordsBulkImpl(hash<auto> recs, *hash<auto> upsert_options) {
        throwUnimplementedException();
    }

    #! Updates a single record matching the search options
    /** @param set the hash of field data to set
        @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private bool updateSingleRecordImpl(hash<auto> set, hash<auto> where_cond,
            *hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond,
            *hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        throwUnimplementedException();
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        throwUnimplementedException();
    }

    #! Creates a new child data provider and returns it after adding as a child
    /** @param name the name of the new child data provider
        @param fields the fields for the new child data provider
        @param child_create_options the options for creating the new child data provider

        @return the new child data provider

        @since DataProvider 2.0
    */
    private AbstractDataProvider createChildProviderImpl(string name, hash<string, AbstractDataField> fields,
            *hash<auto> child_create_options) {
        throwUnimplementedException();
    }

    #! Deletes a child data provider
    /** @param name the name of the new child data provider
        @param child_delete_options the options for deleting a child data provider

        @since DataProvider 2.0
    */
    private deleteChildProviderImpl(string name, *hash<auto> child_delete_options) {
        throwUnimplementedException();
    }

    #! Creates a new field
    /**
        @param field the field to add
        @param field_add_options the options for adding the new field

        @since DataProvider 2.0
    */
    private addFieldImpl(AbstractDataField field, *hash<auto> field_add_options) {
        throwUnimplementedException();
    }

    #! Updates an existing field
    /** @param name the name of the current field to update
        @param field the new definition of the field
        @param field_update_options the options for adding the new field

        @since DataProvider 2.0
    */
    private updateFieldImpl(string name, AbstractDataField field, *hash<auto> field_update_options) {
        throwUnimplementedException();
    }

    #! Deletes an existing field
    /** @param name the name of the field to delete
        @param field_delete_options the options for deleting the new field

        @since DataProvider 2.0
    */
    private deleteFieldImpl(string name, *hash<auto> field_delete_options) {
        throwUnimplementedException();
    }

    #! Sends a message from message-capable data providers
    /**
        @param message_id the message ID to send; must correspond to a valid message ID
        @param msg the message to send
        @param send_message_options the options for sending the message

        @since DataProvider 2.5
    */
    private sendMessageImpl(string message_id, auto msg, *hash<auto> send_message_options) {
        throwUnimplementedException();
    }

    #! Returns the schema object supporting this data provider
    /** @return the schema object supporting this data provider

        @since DataProvider 2.2
    */
    private object getSchemaObjectImpl() {
        throwUnimplementedException();
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider

        @note only called if the provider supports the request / response API
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        throwUnimplementedException();
    }

    #! Returns the description of a successful request message with call data, if any
    /** @param req sample request data that can be used to determine the final type

        @return the request type for this provider according to any call data passed

        @note
        - Only called if the provider supports the request / response API
        - This API is meant to provide information about request data types that can depend on the call data

        @since DataProvider 3.0
    */
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        # the default is to return the standard request data and verify the request data against it
        *AbstractDataProviderType rv = getRequestTypeImpl();
        if (rv && exists req) {
            try {
                rv.acceptsValue(req);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "MISSING-VALUE-ERROR") {
                    rethrow;
                }
            }
        }
        return rv;
    }

    #! Returns the description of a successful request message in the context of the given option data
    /** @param options action option data for the data provider

        @return the request type for this provider in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @note This API is meant to provide information about request data types that can depend on the option data

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return getRequestTypeImpl();
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message

        @note only called if the provider supports the request / response API
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        throwUnimplementedException();
    }

    #! Returns the description of a successful response message with call data, if any
    /** @param req sample request data that can be used to determine the final type

        @return the response type for this provider according to any call data passed

        @note
        - Only called if the provider supports the request / response API
        - This API is meant to provide information about response data types that can depend on the call data

        @since DataProvider 3.0
    */
    private *AbstractDataProviderType getResponseTypeWithDataImpl(auto req) {
        # the default is to return the standard response data and verify the request data against it
        return getResponseTypeImpl();
    }

    #! Returns the description of a successful response message in the context of the given option data
    /** @param options action option data for the data provider

        @return the response type for this provider in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request / response API

        @note This API is meant to provide information about response data types that can depend on the call data

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getResponseTypeWithOptionsImpl(*hash<auto> options) {
        return getResponseTypeImpl();
    }

    #! Returns a hash of error responses, if any
    /** @return a hash of error responses, if any, keyed by response code or string

        @note only called if the provider supports the request / response API
    */
    private *hash<string, AbstractDataProviderType> getErrorResponseTypesImpl() {
        throwUnimplementedException();
    }

    #! Returns a hash of error responses, if any, in the context of the given option data
    /** @return a hash of error responses, if any, keyed by response code or string, in the context of the given
        option data

        @note only called if the provider supports the request / response API
    */
    private *hash<string, AbstractDataProviderType> getErrorResponseTypesWithOptionsImpl(*hash<auto> options) {
        return getErrorResponseTypesImpl();
    }

    #! Returns the type for the given error code
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return the type for the given error code

        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known
    */
    private AbstractDataProviderType getErrorResponseTypeImpl(string error_code) {
        # note: this should be reimplemented with a more efficient implementation if creating the error type hash on
        # demand is expensive
        *hash<string, AbstractDataProviderType> err_map = getErrorResponseTypesImpl();
        *AbstractDataProviderType type = err_map{error_code};
        if (!type) {
            throw "UNKNOWN-ERROR-RESPONSE", sprintf("error code %y is unknown; known error codes: %y", error_code,
                keys err_map);
        }
        return type;
    }

    #! Returns the type for the given error code in the context of the given option data
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @param options option data that can be used to determine the final type
        @return the type for the given error code in the context of the given option data

        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known

        @since DataProvider 3.3
    */
    private AbstractDataProviderType getErrorResponseTypeWithOptionsImpl(string error_code, *hash<auto> options) {
        *hash<string, AbstractDataProviderType> err_map = getErrorResponseTypesWithOptionsImpl(options);
        *AbstractDataProviderType type = err_map{error_code};
        if (!type) {
            throw "UNKNOWN-ERROR-RESPONSE", sprintf("error code %y is unknown; known error codes: %y", error_code,
                keys err_map);
        }
        return type;
    }

    #! Returns the type of an observable event, if any
    /** @param event_id the event ID of the event

        @return the observable event type for this provider

        @note only called if the provider supports the observer pattern / event API

        @throw INVALID-EVENT the event is not supported by this data provider

        @since DataProvider 2.5
    */
    private hash<DataProviderMessageInfo> getEventInfoImpl(string event_id) {
        *hash<string, hash<DataProviderMessageInfo>> h = getEventTypes();
        if (!h{event_id}) {
            throw "INVALID-EVENT", sprintf("provider %y (type %y) does not support event %y; supported events: %y",
                getName(), self.className(), event_id, keys h);
        };
        return h{event_id};
    }

    #! Returns the description of an observable event, if any with option data, if any
    /** @param event_id the event ID of the event
        @param options example option data that can be used to determine the final type

        @return the event type for this provider according to any call data passed

        @throw INVALID-EVENT the event_id is not valid

        @note This API is meant to provide information about event data types that can depend on option data

        @since DataProvider 3.0
    */
    private hash<DataProviderMessageInfo> getEventInfoWithOptionsImpl(string event_id, *hash<auto> options) {
        return getEventInfoImpl(event_id);
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API

        @since DataProvider 2.5
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        throwUnimplementedException();
    }

    #! Returns a hash of all supported event types in the context of the given options
    /** @param options option data that can be used to determine the final type

        @return a hash of all supported event types in the context of the given options

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API

        @since DataProvider 3.3
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesWithOptionsImpl(*hash<auto> options) {
        return getEventTypesImpl();
    }

    #! Returns the type of an outbound message, if any
    /** @param message_id the ID of the message

        @return the message type for this provider

        @throw INVALID-MESSAGE the message is not supported by this data provider

        @since DataProvider 2.5
    */
    private hash<DataProviderMessageInfo> getMessageInfoImpl(string message_id) {
        *hash<string, hash<DataProviderMessageInfo>> h = getMessageTypes();
        if (!h{message_id}) {
            throw "INVALID-MESSAGE", sprintf("provider %y (type %y) does not support message %y; supported "
                "messages: %y", getName(), self.className(), message_id, keys h);
        };
        return h{message_id};
    }

    #! Returns a hash of all supported outbound messages
    /** @return a hash of all supported outbound messages

        @throw INVALID-OPERATION the data provider does not support sending messages

        @since DataProvider 2.5
    */
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesImpl() {
        throwUnimplementedException();
    }

    #! Returns a hash of all supported outbound messages with options
    /** @param options any action options that can affect the message types returned

        @return a hash of all supported outbound messages

        @throw INVALID-OPERATION the data provider does not support sending messages

        @since DataProvider 3.3
    */
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesWithOptionsImpl(*hash<auto> options) {
        return getMessageTypesImpl();
    }

    #! Throws an \c INVALID-OPERATION exception
    /** @throw INVALID-OPERATION this exception is thrown unconditionally by this method
    */
    private throwUnimplementedException() {
        # should never be called
        throw "UNIMPLEMENTED", sprintf("Data provider %y of class %y has an error; the operation is not supported; "
            "this indicates a bug in the implementation of the data provider; please report this bug to the author "
            "of the data provider",
            getName(), self.className());
    }

    #! Returns example data for a request message
    /** @return example data for a request message

        @since DataProvider 3.0
    */
    private auto getExampleRequestDataImpl() {
        return getRequestTypeImpl().getExampleValue();
    }

    #! Returns example data for a request message in the context of the given option data
    /** @param options action option data for the request

        @return example data for a request message in the context of the given option data

        @since DataProvider 3.3
    */
    private auto getExampleRequestDataWithOptionsImpl(*hash<auto> options) {
        return getExampleRequestDataImpl();
    }

    #! Returns example data for a successful request message
    /** @return example data for a successful request message

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.0
    */
    private auto getExampleResponseDataImpl() {
        *AbstractDataProviderType t = getResponseTypeImpl();
        return t ? t.getExampleValue() : NOTHING;
    }

    #! Returns example data for a successful request message in the context of the given option data
    /** @param options action option data for the request

        @return example data for a successful request message in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API

        @since DataProvider 3.3
    */
    private auto getExampleResponseDataWithOptionsImpl(*hash<auto> options) {
        return getExampleResponseDataImpl();
    }

    #! Returns example data for the error message with the given error code
    /** @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return example data for the error message with the given error code

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known

        @since DataProvider 3.0
    */
    private auto getExampleErrorResponseDataImpl(string error_code) {
        *AbstractDataProviderType t = getErrorResponseTypeImpl(error_code);
        return t ? t.getExampleValue() : NOTHING;
    }

    #! Returns example data for the error message with the given error code in the context of the given option data
    /** @param options action option data for the request
        @param error_code the error code for the response; must be a known error code, or an \c UNKNOWN-ERROR-RESPONSE
        exception is thrown
        @return example data for the error message with the given error code in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the request API
        @throw UNKNOWN-ERROR-RESPONSE the error response given is not known

        @since DataProvider 3.3
    */
    private auto getExampleErrorResponseDataWithOptionsImpl(string error_code, *hash<auto> options) {
        return getExampleErrorResponseDataImpl(error_code);
    }

    #! Returns example data for the given observable event
    /** @param event_id the event ID of the event

        @return example data for the given observable event

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.0
    */
    private auto getExampleEventDataImpl(string event_id) {
        *AbstractDataProviderType t = getEventInfoImpl(event_id).type;
        return t ? t.getExampleValue() : NOTHING;
    }

    #! Returns example data for the given observable event in the context of the given option data
    /** @param event_id the event ID of the event
        @param options action option data

        @return example data for the given observable event in the context of the given option data

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.3
    */
    private auto getExampleEventDataWithOptionsImpl(string event_id, *hash<auto> options) {
        return getExampleEventDataImpl(event_id);
    }

    #! Returns example data for the record type
    /** @return example data for the record type

        @since DataProvider 3.0
    */
    private hash<auto> getExampleRecordDataImpl() {
        try {
            return searchFirstRecord({});
        } catch (hash<ExceptionInfo> ex) {
            error("EXAMPLE-RECORD-ERROR", "Failed to retrieve example row; returning generated data: %s",
                get_exception_string(ex));
        }
        *hash<string, AbstractDataField> rec = getRecordTypeImpl();
        return rec ? generateExampleData(rec) : {};
    }

    #! Returns example data for the record type in the context of the given option data
    /** @param options action option data

        @return example data for the record type in the context of the given option data

        @since DataProvider 3.3
    */
    private hash<auto> getExampleRecordDataWithOptionsImpl(*hash<auto> options) {
        return getExampleRecordDataImpl();
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @since DataProvider 3.0
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        # intentionally empty
    }

    #! Returns element reference data of the given kind if available
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed element values for list types for this data

        @since DataProvider 3.0
    */
    private *list<hash<AllowedValueInfo>> getElementReferenceDataImpl(string type, *hash<auto> action_opts) {
        # intentionally empty
    }

    #! Returns a reference data value of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return the value of the reference data

        @since DataProvider 3.0
    */
    private auto getReferenceDataValueImpl(string type, *hash<auto> action_opts) {
        # intentionally empty
    }

    #! Returns @ref True if the current thread is in a transaction
    /** @return @ref True if the current thread is in a transaction
    */
    private bool currentThreadInTransactionImpl() {
        throwUnimplementedException();
    }

    #! Returns the data provider name
    abstract string getName();

    #! Returns data provider static info
    private abstract hash<DataProviderInfo> getStaticInfoImpl();

    #! Sets the debug info collector for this data provider
    /** @param collector the debug collector to use, or @ref nothing to disable debug collection

        When a debug collector is set, the data provider will call the collector's
        @ref AbstractDebugInfoCollector::collect method
        with technical debug information about operations (HTTP requests, WebSocket messages, etc.).

        @par Example:
        @code{.py}
BufferedDebugInfoCollector collector();
provider.setDebugCollector(collector);

# Make requests - debug info is collected
auto response = provider.doRequest(request);

# Retrieve collected debug info
list<hash<DataProviderDebugInfo>> info = collector.getAll();
        @endcode

        @since DataProvider 3.3
    */
    setDebugCollector(*AbstractDebugInfoCollector collector) {
        debug_collector = collector;
    }

    #! Returns the current debug info collector, if any
    /** @return the current debug collector, or @ref nothing if none is set

        @since DataProvider 3.3
    */
    *AbstractDebugInfoCollector getDebugCollector() {
        return debug_collector;
    }

    #! Collects debug info if a collector is registered
    /** @param info the debug information to collect

        Subclasses should call this method to report debug information when performing operations.
        If no collector is registered, this method does nothing.

        @since DataProvider 3.3
    */
    private collectDebugInfo(hash<DataProviderDebugInfo> info) {
        if (debug_collector) {
            debug_collector.collect(info);
        }
    }

    #! Returns @ref True if debug collection is enabled
    /** @return @ref True if a debug collector is registered

        @since DataProvider 3.3
    */
    bool debugCollectionEnabled() {
        return debug_collector.val();
    }

    #! Returns @ref True if the data provider supports being used as a pipeline processor
    /** @return @ref True if the data provider supports pipeline processor capabilities

        @since %DataProvider 3.3
    */
    bool supportsPipelineProcessor() {
        return getStaticInfoImpl().supports_pipeline_processor;
    }

    #! Returns the input type for the pipeline processor, if any
    /** @return the input type for the pipeline processor; NOTHING means any type is accepted

        @note only called if the provider supports the pipeline processor API

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getPipelineProcessorInputTypeImpl() {
    }

    #! Returns the input type for the pipeline processor in the context of the given option data
    /** @param options action option data for the data provider

        @return the input type for the pipeline processor in the context of the given option data;
        NOTHING means any type is accepted

        @note
        - Only called if the provider supports the pipeline processor API
        - This API is meant to provide information about input data types that can depend on the option data

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getPipelineProcessorInputTypeWithOptionsImpl(*hash<auto> options) {
        return getPipelineProcessorInputTypeImpl();
    }

    #! Returns the output type for the pipeline processor, if any
    /** @return the output type for the pipeline processor; NOTHING means any type is produced

        @note only called if the provider supports the pipeline processor API

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
    }

    #! Returns the output type for the pipeline processor in the context of the given option data
    /** @param options action option data for the data provider

        @return the output type for the pipeline processor in the context of the given option data;
        NOTHING means any type is produced

        @note
        - Only called if the provider supports the pipeline processor API
        - This API is meant to provide information about output data types that can depend on the option data

        @since DataProvider 3.3
    */
    private *AbstractDataProviderType getPipelineProcessorOutputTypeWithOptionsImpl(*hash<auto> options) {
        return getPipelineProcessorOutputTypeImpl();
    }

    #! Overrides the processor API to throw by default
    /** Data providers do not support the pipeline processor API by default; subclasses must override this method
        to provide pipeline processor capabilities.

        @throws UNIMPLEMENTED thrown if the data provider does not implement pipeline processor capabilities

        @since %DataProvider 3.3
    */
    private *auto processRecordImpl(auto record) {
        throw "UNIMPLEMENTED", sprintf("%s does not implement pipeline processor capabilities",
            self.className());
    }

    #! Returns @ref True for the bulk API
    /** @since %DataProvider 3.3
    */
    private bool supportsBulkApiImpl() {
        return True;
    }

    #! Returns 0 (no processor capabilities) by default
    /** @since %DataProvider 3.3
    */
    private int getProcessorCapabilitiesImpl() {
        return 0;
    }
}
}
