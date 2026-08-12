# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaActionDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
/** @defgroup RestSchemaActionLocations REST Schema Action Transport Locations
    Identifies where a flattened action option belongs in the outgoing HTTP request.

    Every field in an action's request type carries one of these locations in its
    @ref RestSchemaActions::RestSchemaFlatFieldInfo "RestSchemaFlatFieldInfo" entry, which is what lets
    @ref RestSchemaActions::RestSchemaActionOperation::unflatten() "unflatten()" rebuild the transport
    request from a flat hash of user-supplied options.
*/
#/@{
#! Transport location: an OpenAPI path template variable
public const RSAL_PATH = "path";

#! Transport location: a URI query argument
public const RSAL_QUERY = "query";

#! Transport location: an HTTP header
public const RSAL_HEADER = "header";

#! Transport location: a property of an object-valued request body
public const RSAL_BODY = "body";

#! Transport location: the entire request body, which is not an object
/** Used when the request body schema is a scalar or an array, so that there are no properties to lift;
    the action then presents a single \c "body" option carrying the whole body.
*/
public const RSAL_BODY_ROOT = "body-root";

#! All valid transport locations
public const RestSchemaActionLocations = {
    RSAL_PATH: True,
    RSAL_QUERY: True,
    RSAL_HEADER: True,
    RSAL_BODY: True,
    RSAL_BODY_ROOT: True,
};
#/@}

#! HTTP methods whose defined semantics are read-only
/** The comparison is performed against normalized protocol tokens, never action names or presentation text.
    Methods not listed here fail closed as potentially mutating.

    @since %RestSchemaActions 1.0
*/
public const RestSchemaReadOnlyHttpMethods = DataProviderReadOnlyHttpMethods;

#! Describes where one flattened action option belongs in the outgoing HTTP request
/** This is the reverse map that makes flattening lossless; it is the contract tested by
    @ref RestSchemaActions::RestSchemaActionOperation::flatten() "flatten()" /
    @ref RestSchemaActions::RestSchemaActionOperation::unflatten() "unflatten()" round-trip tests.
*/
public hashdecl RestSchemaFlatFieldInfo {
    #! The action-facing option name; unique within the action
    string name;

    #! The transport location
    /** @see @ref RestSchemaActionLocations
    */
    string loc;

    #! The name of the value in its transport location
    /** Differs from @ref name when the manifest renamed the field to resolve a collision or to improve
        the presentation.
    */
    string wire_name;

    #! The name of any wire codec applied to values of this field
    /** @see @ref RestSchemaActions::RestSchemaActionCodecs "RestSchemaActionCodecs"
    */
    *string codec;
}

#! Describes a bidirectional conversion between an action-facing value and its wire representation
/** A codec exists so that an action can present a semantically correct type — a %Qore
    @ref date "date", say — for a wire value that is something else, such as an integer count of
    seconds since the UNIX epoch, without the UX overlay having to carry executable code.

    @see @ref RestSchemaActions::RestSchemaActionCodecs "RestSchemaActionCodecs"
*/
public hashdecl RestSchemaWireCodecInfo {
    #! The unique codec name, as referenced from a field overlay
    string name;

    #! The action-facing type presented in place of the schema-derived type
    AbstractDataProviderType type;

    #! Converts an action-facing value to its wire representation
    /** Must return @ref nothing unchanged, so that an unset optional value stays unset.
    */
    code<auto(auto)> to_wire;

    #! Converts a wire value to its action-facing representation
    /** Must return @ref nothing unchanged, so that a value the service omitted stays unset.
    */
    code<auto(auto)> from_wire;

    #! A description of the conversion, used in generated field documentation
    *string desc;
}

#! A declarative UX override for a single field derived from a REST schema
/** Applied after the schema-derived request or response type has been flattened and before action options
    are generated, so that every override is visible to
    @ref DataProvider::DataProviderActionCatalog::getActionOptionFromFields() "getActionOptionFromFields()".

    An overlay expresses **presentation**: what a field is called, whether it is shown first, whether it
    offers a dropdown.  It must never be used to repair a schema — a missing or malformed reference belongs
    in a normalization step, and a valid construct %Qore cannot parse belongs in
    @ref openapi3intro "OpenApi3".
*/
public hashdecl RestSchemaFieldOverlayInfo {
    #! Drop the field from the action entirely
    /** Use for values the connection owns — an API key header, an account identifier fixed by the
        connection — which must never appear as an action option.
    */
    *bool ignore;

    #! Rename the action-facing field
    /** The wire name is unchanged; the reverse map records the original.  This is how a collision between
        two transport locations is resolved.
    */
    *string name;

    #! The display name for the field
    *string display_name;

    #! The short description of the field in plain text
    *string short_desc;

    #! The description of the field with Markdown formatting
    *string desc;

    #! An example value for the field
    auto example_value;

    #! The default value for the field
    auto default_value;

    #! Static allowed values for the field
    *softlist<auto> allowed_values;

    #! Whether values outside @ref allowed_values may also be supplied
    *bool allowed_values_creatable;

    #! Narrow the field to required
    /** Only narrowing is supported: an action may present a stricter contract than the API, but declaring a
        schema-required field optional would produce requests the server rejects, so it is an error.
    */
    *bool required;

    #! Show the field in the initial (unexpanded) action form
    *bool preselected;

    #! Mark the field as holding sensitive data
    *bool sensitive;

    #! Names the either/or groups this field participates in
    *softlist<string> required_groups;

    #! Names fields that may not be supplied together with this one
    *softlist<string> exclusive_with;

    #! The reference-data provider name supplying a dropdown for this field
    /** Resolved through @ref DataProvider::AbstractDataProvider::getSupportedReferenceData() "getSupportedReferenceData()"
        and @ref DataProvider::AbstractDataProvider::getReferenceDataImpl() "getReferenceDataImpl()"
    */
    *string ref_data;

    #! The reference-data provider name supplying a dropdown for elements of a list-valued field
    *string element_ref_data;

    #! Names fields whose values this field's presentation depends on
    *softlist<string> depends_on;

    #! Marks the field as one that other fields depend on
    *bool has_dependents;

    #! Declares that changing this field's value invalidates dependent fields
    auto on_change;

    #! A regular expression the value must match
    *string validation_regex;

    #! An explicit UI type hint
    *string ui_type;

    #! Allow a value to be entered by hand as well as chosen from @ref ref_data
    /** A dropdown is an aid, not a constraint: a workflow usually chains an identifier out of a previous
        step's output, and a credential scoped narrowly enough may not be able to list the values at all.
        Set this so that such a field is still usable.
    */
    *bool supports_custom_values;

    #! The name of a registered wire codec to apply to this field
    /** Replaces the schema-derived type with the codec's action-facing type and converts values in both
        directions.

        @see @ref RestSchemaActions::RestSchemaActionCodecs "RestSchemaActionCodecs"
    */
    *string codec;

    #! An explicit replacement for the schema-derived type
    /** Use for semantic string types such as
        @ref DataProvider::AbstractDataProviderTypeMap "AbstractDataProviderTypeMap.\"*uri\"".  A type
        override changes only how the value is presented and validated; it does not convert the value, so
        use @ref codec when the wire representation differs.
    */
    *AbstractDataProviderType type;

    #! Raw field attributes merged after every other override
    *hash<auto> attr;

    #! Overrides for fields nested inside this field's type
    /** Overriding a nested field rewrites only that field; the rest of the derived type is preserved.
    */
    *hash<string, hash<RestSchemaFieldOverlayInfo>> fields;
}

#! Describes one operation exported from a REST schema as an application action
/** The manifest is the application's own list of what it exports.  It exists so that action identity is
    **stable**: an OpenAPI \c operationId may be used to locate an operation and is checked for drift, but it
    is never the source of persistent identity, because an upstream rename must be reported rather than
    silently rename an action that existing workflows call.

    It also makes additions deliberate — an endpoint that appears in an updated schema does not become a
    public action until somebody adds it here.
*/
public hashdecl RestSchemaActionInfo {
    #! The stable action ID; unique within the application
    string action;

    #! The HTTP method
    string method;

    #! The OpenAPI path template, including any \c {variable} placeholders
    string path;

    #! The \c operationId the schema is expected to declare for this operation
    /** Checked when the operation is resolved and reported as drift when it changes; never used as action
        identity.
    */
    *string operation_id;

    #! Names the action this one is a deliberately narrowed variant of
    /** An operation may normally be exported only once, so that a copy-and-paste slip in the manifest is
        caught rather than producing two actions nobody meant to have.  Setting this declares the exception:
        a second action on the same operation whose overlay narrows it to one job — an "archive" action that
        is an update with the status fixed, say — where the general action remains available for everything
        else.

        The named action must be in the same manifest and must resolve to the same method and path, so that
        the marker cannot be used to wave through an unrelated duplicate.
    */
    *string variant_of;

    #! The action display name
    string display_name;

    #! The action short description in plain text
    string short_desc;

    #! The action description with Markdown formatting
    string desc;

    #! The action groups within the application
    *softlist<string> groups;

    #! The order in which options are presented
    /** Fields named here come first, in this order; any remaining fields follow in derived order.  Naming a
        field that the action does not have is an error, so that a rename upstream cannot silently drop the
        intended ordering.
    */
    *softlist<string> field_order;

    #! Request field overrides, keyed by derived (pre-rename) field name
    *hash<string, hash<RestSchemaFieldOverlayInfo>> fields;

    #! Response field overrides, keyed by field name in the success response body
    *hash<string, hash<RestSchemaFieldOverlayInfo>> response_fields;

    #! Lift the properties of an object-valued request body to the top level of the action request
    /** Set to @ref False for an operation whose body is a meaningful domain object that should be supplied
        as a whole.
    */
    bool flatten_body = True;

    #! The HTTP status code of the response to publish as the action output type
    /** Required only when the operation declares more than one success response with a body, in which case
        the output contract is ambiguous and registration fails without it.
    */
    *string response_code;

    #! A path of field names to unwrap from the success response body
    /** Publishes a value nested inside the response body as the action output.  Use only when the enclosing
        object carries nothing a caller needs.
    */
    *softlist<string> response_unwrap;

    #! Free-form metadata attached to the generated action
    *hash<auto> metadata;
}
}
