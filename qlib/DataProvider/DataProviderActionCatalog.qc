# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderActionCatalog.qc provides the DataProvider catalog API

/** DataProviderActionCatalog.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Application information hash
/** Note that action paths are relative from the data provider created from the connection and optionally the subtype

    If both \c scheme and \c action.cls are missing, it implies that the data provider will be implicitly provided by
    the context where the action is used
*/
public hashdecl DataProviderAppInfo {
    #! Unique application name
    /** This technical name should have no spaces and be in camel case or all caps by convention
    */
    string name;

    #! The display name for the application
    string display_name;

    #! A short description for the app, plain text formatting
    string short_desc;

    #! Description of the application with markdown formatting
    string desc;

    #! Optional native i18n catalog domain for localized app and action presentation
    /** When omitted, @ref DataProviderPresentation::defaultCatalogDomain() derives a stable domain from \c name.
        The catalog changes presentation fields only; \c name and action identifiers remain exact technical
        identities.

        @since %DataProvider 3.7
    */
    *string presentation_catalog_domain;

    #! BCP 47 locale of the producer-owned presentation strings
    /** Omit this field when the producer language is unknown; consumers must then treat the source presentation as
        opaque instead of assuming a language.

        @since %DataProvider 3.7
    */
    *string presentation_source_locale;

    #! Any legal text, links, copyright notice(s), or disclaimer(s) related to the use of the app; markdown supported
    string legal;

    #! The application groups, if any
    *softlist<string> groups;

    #! Connection scheme for the application
    /** This is the connection scheme for the \c ConnectionProvider module and indicates that the given connection
        type produces a data provider which will be the base data provider for application actions
    */
    *string scheme;

    #! The application logo
    /** Must be an SVG or PNG; all logos must be square, and PNG must be 200x200 px
    */
    data logo;

    #! The logo file name
    string logo_file_name;

    #! The mime type for \c logo
    /** Must be one of:
        - SVG: \c image/svg+xml
        - PNG: \c image/png
    */
    string logo_mime_type;

    #! If the apps supports event actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_events = False;

    #! If the apps supports API actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_apis = False;

    #! If the apps supports message actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_messages = False;

    #! If the apps supports find actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_find = False;

    #! If the apps supports find_single actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_find_single = False;

    #! If the apps supports create actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_create = False;

    #! If the apps supports upsert actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_upsert = False;

    #! If the apps supports update actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_update = False;

    #! If the apps supports delete actions
    /** This attribute is filled automatically when actions are registered
    */
    bool supports_delete = False;

    #! If the app supports pipeline processor actions
    /** This attribute is filled automatically when actions are registered

        @since %DataProvider 3.3
    */
    bool supports_pipeline_processors = False;

    #! Was the application added dynamically?
    bool dynamic = False;

    #! Should this app be hidden in the front end?
    bool hidden = False;

    #! User-defined info about the app
    *hash<auto> info;

    #! Per-app connection options override
    /** When set, these options are used instead of @c ConnectionSchemeInfo.options for connection
        creation forms. This allows multiple apps to share the same connection scheme (e.g. @c "db")
        while presenting different form fields.

        @since %DataProvider 3.3
    */
    *hash<string, hash<ConnectionProvider::ConnectionOptionInfo>> connection_options;

    #! Per-app auto_url override
    /** When @ref True, the connection URL is generated automatically from options and the URL
        field is not shown in the connection creation form, regardless of the scheme's auto_url setting.

        @since %DataProvider 3.3
    */
    *bool auto_url;

    #! Disables the framework-injected generic "Make an API call" action for this app
    /** When @ref False (or unset, the default), the framework automatically registers a <tt>make-api-call</tt>
        DPAT_API action for every app whose connection scheme is REST-derived (the scheme's
        @c cls inherits @c RestClient::RestConnection). The action accepts arbitrary
        method/path/query/headers/body input and dispatches the request through the connection's
        authenticated REST client.

        Set this to @ref True for REST apps that should @em not expose the generic call action — for
        example apps that already provide an equivalent canonical action (e.g. GenericRest's own
        @c /call), or compliance-sensitive integrations where the framework's generic surface is
        undesirable.

        @note Non-REST apps (Db, FTP, WebSocket, MongoDB, Redis, Memcached, SSE, Filesystem, etc.) do
        @em not need this flag — they are automatically excluded by the positive REST predicate.

        @since %DataProvider 3.5
    */
    *bool disable_generic_api_call;

    #! The app does network or filesystem I/O that the %Qore sandbox does not see
    /** Set this to @ref True for an app whose actions open connections or files outside %Qore's own I/O: in Java
        code called through the \c jni module, in Python or JavaScript code, or in a native client library that
        connects or opens files itself.  The filesystem and network policy of a sandbox
        (@ref Qore::SandboxManager "SandboxManager") is enforced only for the I/O that goes through %Qore, so such
        I/O is not checked unless the app checks it itself (see "I/O That Qore Does Not See" in the %Qore sandboxing
        documentation).

        A sandboxed caller cannot use an app with this flag set unless the app also sets
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware"; see
        @ref DataProvider::DataProviderActionCatalog::checkSandboxAccess()
        "DataProviderActionCatalog::checkSandboxAccess()" and @ref dataprovider_external_io.

        @since %DataProvider 3.12
    */
    *bool external_io;

    #! The app checks all of its external I/O against the policy of its caller's sandbox
    /** Set this to @ref True only for an app whose external I/O has been audited to check every network destination
        and every path against the filesystem and network policy of the caller's sandbox before using it; for Java
        code this is done with the \c jni module's \c org.qore.jni.QoreSandbox class, following the audit rules in
        \c design/module-sandboxing-audit-guide.md.  It allows sandboxed callers to use an app with
        @ref DataProvider::DataProviderAppInfo::external_io "external_io" set; it has no effect on an app without it.

        @since %DataProvider 3.12
    */
    *bool sandbox_aware;
}

#! Data Provider Action Type Codes
/** Each action implements only one type, so these codes do not make up a bitfield

    @since %DataProvider 3.3 converted from individual constants to an enum
*/
public enum DataProviderActionType {
    #! Wait for an event
    DPAT_EVENT = 1,
    #! Make an API call
    DPAT_API = 2,
    #! Send a message
    DPAT_SEND_MESSAGE = 3,
    #! Find all matching records
    DPAT_FIND = 4,
    #! Find a single record
    DPAT_FIND_SINGLE = 5,
    #! Create a record
    DPAT_CREATE = 6,
    #! Upsert a record
    DPAT_UPSERT = 7,
    #! Update a record
    DPAT_UPDATE = 8,
    #! Delete a record
    DPAT_DELETE = 9,
    #! Pipeline processor
    DPAT_PIPELINE_PROCESSOR = 10
}

#! UI option view modes
/** Determines the default view for the option in the front end

    @since %DataProvider 3.3
*/
public enum UiOptionView {
    #! Template view
    Template = "template",
    #! Expression view
    Expression = "expression",
}

#! Maps string values to UiOptionView enum values for deserialization
/** @since DataProvider 3.3
*/
public const hash<auto> UiOptionViewMap = {
    "template": UiOptionView::Template,
    "expression": UiOptionView::Expression,
};

#! Maps DPAT_* action type codes to app action flags
public const hash<string, string> ActionCodeFlagMap = {
    DPAT_EVENT: "supports_events",
    DPAT_API: "supports_apis",
    DPAT_SEND_MESSAGE: "supports_messages",
    DPAT_FIND: "supports_find",
    DPAT_FIND_SINGLE: "supports_find_single",
    DPAT_CREATE: "supports_create",
    DPAT_UPSERT: "supports_upsert",
    DPAT_UPDATE: "supports_update",
    DPAT_DELETE: "supports_delete",
    DPAT_PIPELINE_PROCESSOR: "supports_pipeline_processors",
};

#! List of app action flags
public const AppActionFlags = ActionCodeFlagMap.values();

#! HTTP methods whose protocol semantics establish that an action is read-only
/** Comparisons must use normalized protocol tokens.  Action names, descriptions, and translated presentation text
    are never evidence of mutation behavior.  Methods not listed here fail closed as potentially mutating.

    @since %DataProvider 3.7
*/
public const hash<string, bool> DataProviderReadOnlyHttpMethods = {
    "GET": True,
    "HEAD": True,
    "OPTIONS": True,
    "TRACE": True,
};

#! Maps DPAT_* action type codes to a data provider attribute string
public const hash<string, string> ActionAttrMap = {
    DPAT_EVENT: "supports_observable",
    DPAT_API: "supports_request",
    DPAT_SEND_MESSAGE: "supports_messages",
    DPAT_FIND: "supports_read",
    DPAT_FIND_SINGLE: "supports_read",
    DPAT_CREATE: "supports_create",
    DPAT_UPSERT: "supports_upsert",
    DPAT_UPDATE: "supports_update",
    DPAT_DELETE: "supports_delete",
    DPAT_PIPELINE_PROCESSOR: "supports_pipeline_processor",
};

#! Maps DPAT_* action type codes to a user-friendly name
public const hash<string, string> ActionNameMap = {
    DPAT_EVENT: "EVENT",
    DPAT_API: "API",
    DPAT_SEND_MESSAGE: "MESSAGE",
    DPAT_FIND: "FIND",
    DPAT_FIND_SINGLE: "FIND_SINGLE",
    DPAT_CREATE: "CREATE",
    DPAT_UPSERT: "UPSERT",
    DPAT_UPDATE: "UPDATE",
    DPAT_DELETE: "DELETE",
    DPAT_PIPELINE_PROCESSOR: "PIPELINE_PROCESSOR",
};

#! Set of actions that support search expressions
public const hash<string, bool> ActionsWithSearch = {
    DPAT_FIND: True,
    DPAT_FIND_SINGLE: True,
    DPAT_UPDATE: True,
    DPAT_DELETE: True,
};

#! Data provider action option definition without its type
/** @see
    - @ref ActionOptionInfo for a registered action option with its type
    - @ref IndexedActionOptionInfo for an action option from the data provider index with its type description

    @since %DataProvider 3.8
*/
public hashdecl ActionOptionBaseInfo inherits BaseOptionInfo {
    #! The group names for the option, if any
    *softlist<string> groups;

    #! Is the option part of a required group?
    /** A required group is a group of options of which at least one must be set
    */
    *softlist<string> required_groups;

    #! Is the option exclusive with other options?
    *softlist<string> exclusive_with;

    #! An optional regex pattern for validating string values
    *string validation_regex;

    #! An optional description in plain words of the values that @ref validation_regex accepts
    /** Written to complete "Must be ...", so that a user interface can say why it refuses a value instead of saying
        that the value does not match the pattern, for example \c "a two-letter language code, such as en"; it
        describes the pattern only, and is not used for validation

        @since %DataProvider 3.11
    */
    *string validation_regex_desc;

    #! A tag allowing allowed values to be returned dynamically as reference data
    /** The value is the name of the reference data; if allowed_values are not present, calls will be made to
        @ref DataProvider::AbstractDataProvider::getReferenceData() "AbstractDataProvider::getReferenceData()"
        in the available DataProvider when resolving application actions if this attribute is set
    */
    *string ref_data;

    #! A tag allowing element allowed values to be returned dynamically as element reference data
    /** The value is the name of the element reference data; if allowed_values are not present, calls will be made to
        @ref DataProvider::AbstractDataProvider::getElementReferenceData() "AbstractDataProvider::getElementReferenceData()"
        in the available DataProvider when resolving application actions if this attribute is set
    */
    *string element_ref_data;

    #! A tag allowing the default value to be returned dynamically as reference data
    /** The value is the name of the reference data; if allowed_values are not present, calls will be made to
        @ref DataProvider::AbstractDataProvider::getReferenceDataValue() "AbstractDataProvider::getReferenceDataValue()"
        in the available DataProvider when resolving application actions if this attribute is set
    */
    *string default_ref_data;

    #! A tag allowing the type of the option to be retrieved at runtime
    *string dynamic_type;

    #! Does this option support templates?
    bool supports_templates = True;

    #! Does this option support literal values?
    bool supports_custom_values = True;

    #! Does this option support expressions?
    bool supports_expressions = True;

    #! Option location for record-based data providers and data providers provided by a class
    /** values:
        - \c where: in the where clause
        - \c options: in the options argument
        - \c set: in the set argument for update statements
        - \c constructor: option to be used in the class constructor
    */
    *string loc;

    #! Does the value of this option determine the structure of other options?
    bool structural_determinate;

    #! The default view for the option in the front end
    /** @since DataProvider 3.3
    */
    *enum<UiOptionView> default_view;
}

#! Data provider action option definition
public hashdecl ActionOptionInfo inherits ActionOptionBaseInfo {
    #! Data type of the option
    AbstractDataProviderType type;
}

#! Data provider action option definition from the data provider index
/** The option's type is described by plain data, so reading it never requires the provider module that defines
    the type.  A type object can be created from the description with
    @ref AbstractDataProviderType::get() "AbstractDataProviderType::get()", which returns a type
    with the described structure and constraints but not any custom behavior of the original type's class; the
    exact type is available from the registered action.

    @since %DataProvider 3.8
*/
public hashdecl IndexedActionOptionInfo inherits ActionOptionBaseInfo {
    #! Description of the data type of the option
    hash<DataTypeInfo> type_info;
}

#! DataProvider path var info
public hashdecl DataProviderPathVarInfo inherits DisplayDescInfo {
    #! An optional example value for the option
    *string example_value;
}

/** @defgroup dp_action_data_types DataProvider Action Data Type Codes

    The action data type indicates how the nature of the input or output data and how the declared type relates to it
*/
#/@{
#! Normal data
/** The type describes the input or output data directly
*/
public const string DPAD_NORMAL = "normal";

#! Record stream data
/** The data is a record stream, and the type describes the records provided by it. The data type is
    @ref DataProvider::AbstractDataProviderBulkRecordInterface "AbstractDataProviderBulkRecordInterface"
*/
public const string DPAD_RECORD_STREAM = "record-stream";

#! Record set data
/** The data is a hash of record info; keys are field names, values are lists of field values
*/
public const string DPAD_RECORD_SET = "record-set";

#! Pass-through data
/** The output type is the same as the input type; the processor passes records through unchanged
    (e.g., filter processors that drop non-matching records but do not modify matching ones)
*/
public const string DPAD_PASS_THROUGH = "pass-through";

#! Modified pass-through data
/** The output type is a modification of the input type based on the processor's configuration.
    The actual output type must be computed using
    @ref AbstractDataProvider::getPipelineProcessorOutputTypeWithInputType() "getPipelineProcessorOutputTypeWithInputType()" with the input type from the preceding
    pipeline state.

    @since %DataProvider 3.6
*/
public const string DPAD_MODIFIED_PASS_THROUGH = "modified-pass-through";

#! Create output data
/** Only valid for @ref DPAT_CREATE actions: the action creates the input records with the data provider, and its
    output is the single value the data provider generates from them, as returned by
    @ref AbstractDataProvider::finishCreate() "AbstractDataProvider::finishCreate()" and described by the action's
    output type (for example a spreadsheet generated in memory, described by @ref GeneratedDataOutputDataType),
    instead of the created records

    @since %DataProvider 3.9
*/
public const string DPAD_CREATE_OUTPUT = "create-output";
#/@}

#! DataProvider action definition without its options and output type
/** @see
    - @ref DataProviderActionInfo for a registered action
    - @ref DataProviderIndexedActionInfo for an action from the data provider index

    @since %DataProvider 3.8
*/
public hashdecl DataProviderActionBaseInfo {
    #! Name of the application that provides the action
    string app;

    #! The unique name for the action; unique in the application
    string action;

    #! The action groups within the app, if any
    *softlist<string> groups;

    #! Any data provider subtype
    /** This is the value that will be passed to \c AbstractConnection::getDataProvider(string subtype)
    */
    *string subtype;

    #! Data provider path to action
    /** The path is relative from the data provider provided by the application
    */
    string path;

    #! Description of any path variables
    /** Keys are path variables, values provide info about the path variable
    */
    *hash<string, hash<DataProviderPathVarInfo>> path_vars;

    #! The display name for the action
    string display_name;

    #! Action short description, plain text formatting
    string short_desc;

    #! Action long description, markdown formatting
    string desc;

    #! Optional native i18n catalog domain overriding the app presentation catalog
    /** @since %DataProvider 3.7
    */
    *string presentation_catalog_domain;

    #! Optional source-app identity for action presentation message IDs
    /** Set this only when the action is an exact structural projection of an
        action owned by another application.  Catalog lookup then uses the
        source application's action, option, and allowed-value message IDs
        while @c app remains the unchanged executable application identity.

        An action referring to another application's catalog should also set
        @c presentation_catalog_domain and @c presentation_source_locale
        explicitly so lookup and producer-language attribution do not depend
        on the executable application's presentation policy.

        @since %DataProvider 3.7
    */
    *string presentation_catalog_app;

    #! Optional BCP 47 locale overriding the app producer-presentation locale
    /** @since %DataProvider 3.7
    */
    *string presentation_source_locale;

    #! Action type code
    /** @see DataProviderActionType for valid values
    */
    int action_code;

    #! Whether executing the action can change provider or external state
    /** This is structural producer metadata for safety-sensitive consumers.  It must be derived from the
        action's implementation or protocol contract, never from its name, display name, description, or
        translated presentation text.

        If unset, @ref DataProviderActionCatalog::actionMutatesState() applies the action-code default.  In
        particular, generic @ref DPAT_API "DPAT_API" actions default to mutating so unknown actions fail
        closed.

        @since %DataProvider 3.7
    */
    *bool mutates_state;

    #! Message or event type
    /** if \c action_code is:
        - @ref DPAT_EVENT "DPAT_EVENT": this is the event type
        - @ref DPAT_SEND_MESSAGE "DPAT_SEND_MESSAGE": this is the message type
    */
    *string action_val;

    #! The DataProvider class
    /** This is for the case when there is no application \c scheme and the application is not implied by the context.

        It is an error to supply both application \c scheme and \c cls.

        If the class is provided by a module that must be loaded at runtime, set \c cls_mod to the module name so
        that \c load_module() can be called before \c Class::forName() to ensure the class is available in the
        calling program's context.
    */
    *string cls;

    #! The module that provides the class in \c cls
    /** If set, \c load_module() will be called with this value before resolving the class with
        \c Class::forName().

        This is necessary because \c Class::forName() resolves in the calling program's context, and the module
        providing the class may not yet be loaded into that context.
    */
    *string cls_mod;

    #! Values to add to any "where" argument for record-based actions
    *hash<auto> where_add;

    #! Values to add to any "options" argument for record-based actions
    *hash<auto> options_add;

    #! Values to add to any "set" argument for record-based actions
    *hash<auto> set_add;

    #! Do options depend on the data for the call?
    bool data_dependent_options = False;

    #! Does the action support dynamic options?
    bool has_dynamic_options;

    #! Describes acceptable input data types for the action
    hash<string, bool> input_data_types;

    #! Describes the output data of the action and how the output type relates to it
    string output_data_type;

    #! If True, indicates the action can only be added when adding in the given action context
    /** This implies that the action also defines an \c action_context value.
    */
    *bool requires_action_context;

    #! Optional context set by this action for child actions
    *string action_context;

    #! If True, the UI should use a child eligibility API to determine which children can be added
    /** This is used for actions that can have child actions with specific constraints, such as data pipeline states.
    */
    *bool requires_child_eligibility_check;

    #! A free-form string giving information about the exec type
    *string exec_type;

    #! Free-form metadata for the action
    *hash<auto> metadata;

    #! Processor capability bitfield (DPC_* constants) for pipeline processor actions
    /** @since %DataProvider 3.3
    */
    int processor_capabilities = 0;
}

#! DataProvider action definition
public hashdecl DataProviderActionInfo inherits DataProviderActionBaseInfo {
    #! Options that can be provided without a data provider
    *hash<string, hash<ActionOptionInfo>> options;

    #! Data provider static info, useful for record-based data providers
    *hash<DataProviderInfo> data_provider_info;

    #! Output type for the action
    *AbstractDataProviderType output_type;

    #! In case the output type is expensive to acquire or requires I/O, this closure can be used to set it
    /** The closure is called when a getter returning the action needs the output type, and the result is published
        as \c output_type; see @ref dataprovider_deferred_output_types
    */
    *code get_output_type;

    #! For the case when options should be generated dynamically
    *code get_dynamic_options;
}

#! DataProvider action definition from the data provider index
/** The types of options and of the data provider info are described by plain data, and output types are not indexed,
    so an indexed action can be serialized, and reading it never requires the provider module that implements it;
    the registered action is available from
    @ref DataProviderActionCatalog::getAppAction() "DataProviderActionCatalog::getAppAction()".

    @since %DataProvider 3.8
*/
public hashdecl DataProviderIndexedActionInfo inherits DataProviderActionBaseInfo {
    #! Options that can be provided without a data provider
    *hash<string, hash<IndexedActionOptionInfo>> options;

    #! Data provider static info, useful for record-based data providers, with types described by plain data
    /** @see @ref DataProvider::DataProvider::restoreIndexedInfo() "DataProvider::restoreIndexedInfo()"
    */
    *hash<IndexedDataProviderInfo> data_provider_info;
}

#! The action-level record of an action from the data provider index
/** Holds the action-level members of an indexed action (its names, descriptions, codes, groups, metadata, and path
    variables) without its options or data provider info, whose type descriptions are only restored when the action
    itself is read, so the actions of every app can be listed and selected without restoring any type.

    @see
    - @ref ProviderIndexUtil::ProviderIndexUtil::tryGetActionSummariesForApp() "ProviderIndexUtil::tryGetActionSummariesForApp()"
    - @ref DataProviderIndexedActionInfo for the complete indexed action

    @since %DataProvider 3.8
*/
public hashdecl DataProviderIndexedActionSummaryInfo inherits DataProviderActionBaseInfo {
    #! The names of the action's options, in index order; empty if the action has no options
    list<string> option_names = ();

    #! @ref True if the action's data provider info declares expressions
    bool has_expressions = False;
}

/** @defgroup DataProviderFileReadModes Data Provider File API Read Modes
*/
#/@{
#! File API read mode: the response field named by \c read_data_field holds the file data
public const string DPFR_DATA = "data";

#! File API read mode: the response field named by \c read_data_field holds a URL to retrieve the file data from
/** Used by applications that return a short-lived pre-authenticated download URL rather than the file content
    itself; \c OneDrive is one such application
*/
public const string DPFR_URL = "url";
#/@}

/** @defgroup DataProviderFileWriteFormats Data Provider File API Write Formats
*/
#/@{
#! File API write format: the option takes a @ref DataProvider::FileDataType "FileDataType" hash
public const string DPFW_FILE = "file";

#! File API write format: the option takes base64-encoded data
public const string DPFW_BASE64 = "base64";

#! File API write format: the option takes raw string or binary data
public const string DPFW_RAW = "raw";
#/@}

#! Describes how an application's data provider actions map onto single-file read and write operations
/** Applications expose file operations under their own names -- \c get, \c get-file, \c download-file, and
    \c download_file all appear in the catalog for the same operation -- with option and response fields that
    differ as well.  This hash declares the mapping once per application so that generic consumers, notably the
    \c "conn://" scheme in the \c FileLocationHandler module, can read and write a file through any registered
    application without knowing its vocabulary.

    Register with @ref DataProviderActionCatalog::registerAppFileApi().

    @since %DataProvider 3.6
*/
public hashdecl DataProviderFileApiInfo {
    #! The name of the action that reads (downloads) a file
    string read_action;

    #! The option in the read action that takes the file path, object key, or ID
    string read_path_option;

    #! The option in the read action that takes a container (bucket, drive, or parent folder), if any
    *string read_container_option;

    #! How the read action returns the file data
    /** @see @ref DataProviderFileReadModes for valid values
    */
    string read_mode = DPFR_DATA;

    #! The response field holding the data (@ref DPFR_DATA) or the URL to retrieve it from (@ref DPFR_URL)
    string read_data_field;

    #! The encoding of \c read_data_field when \c read_mode is @ref DPFR_DATA
    /** Either \c "base64" or \c "raw"
    */
    string read_data_encoding = "base64";

    #! The response field holding the MIME type of the data, if any
    *string read_mime_type_field;

    #! The name of the action that writes (uploads) a file; if not set, the application is read-only
    *string write_action;

    #! The option in the write action that takes the file path, object key, or ID
    *string write_path_option;

    #! The option in the write action that takes a container (bucket, drive, or parent folder), if any
    *string write_container_option;

    #! The option in the write action that takes the file data
    *string write_data_option;

    #! The shape of the value passed in \c write_data_option
    /** @see @ref DataProviderFileWriteFormats for valid values
    */
    string write_data_format = DPFW_FILE;

    #! If \c write_path_option receives the parent directory rather than the whole path
    /** Some applications address a new file by a parent folder plus a name carried in the file value itself
        rather than by a single path; \c OneDrive, which takes a \c parent_id and a
        @ref DataProvider::FileDataType "FileDataType" holding the name, is one such application.

        When set, the last segment of the location path is taken as the filename and everything before it is
        passed in \c write_path_option.  This requires \c write_data_format to be @ref DPFW_FILE, as no other
        format carries a filename.
    */
    bool write_path_is_parent = False;

    #! If paths are opaque IDs rather than hierarchical paths
    /** Applications that address files by an immutable identifier rather than by name set this flag; consumers
        must not attempt to interpret, split, or normalize the value
    */
    bool path_is_id = False;

    #! Fixed options added to every read and write call
    *hash<auto> options_add;
}

#! Describes an application or action initialization failure retained by the data provider catalog
/** Deferred initialization normally allows healthy applications and actions to remain available after another
    registration fails. This record preserves the failed technical identity and original exception separately from
    any human-readable logging so that complete-catalog consumers can reject a partial result reliably.

    @since %DataProvider 3.7
*/
public hashdecl DataProviderInitializationFailureInfo {
    #! Initialization unit that failed; either \c app or \c action
    string kind;

    #! Exact technical application identifier
    string app;

    #! Exact technical action identifier; present when \c kind is \c action
    *string action;

    #! Original exception error code
    string error;

    #! Original exception description
    string description;

    #! Original exception argument
    auto argument;

    #! Module path associated with the failed registration, when known
    *string module_context;
}

#! One exact app or app/action identity expected during a discovery generation
/** @since %DataProvider 3.8 */
public hashdecl DataProviderDiscoveryExpectedIdentityInfo {
    #! Exact technical application identifier
    string app;

    #! Exact technical action identifier; omit to require only the application
    *string action;
}

#! One structured discovery qualification failure
/** @since %DataProvider 3.8 */
public hashdecl DataProviderDiscoveryFailureInfo {
    #! Discovery generation
    int generation;

    #! Failing boundary: source, factory, app, action, or scheme
    string kind;

    #! Qualification phase
    string phase;

    #! Exact technical failure identity
    string identity;

    #! Exact technical application identifier, when applicable
    *string app;

    #! Exact technical action identifier, when applicable
    *string action;

    #! Original error code
    string error;

    #! Original error description
    string description;

    #! Original exception argument
    auto argument;

    #! Owner module path, when known
    *string module_context;
}

#! Immutable report for one sealed discovery generation
/** @since %DataProvider 3.8 */
public hashdecl DataProviderDiscoveryReportInfo {
    #! Discovery generation
    int generation;

    #! Catalog revision when discovery began
    int start_revision;

    #! Catalog revision when discovery was sealed
    int sealed_revision;

    #! Whether the generation is complete: no failure blocks it, and no application or action is unavailable
    bool complete;

    #! Exact expected identities supplied by discovery sources
    list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected;

    #! Exact application identities present when the generation was sealed
    list<string> apps;

    #! Exact app/action identities present when the generation was sealed
    list<string> actions;

    #! Structured failures that block the generation; empty for a qualified generation
    list<hash<DataProviderDiscoveryFailureInfo>> failures;

    #! Application and action failures recorded without blocking the generation
    /** A generation sealed with \c record_app_failures (see @ref DataProviderActionCatalog::sealDiscovery()) is
        qualified despite the failures of individual applications and actions (see
        @ref DataProviderActionCatalog::isAppDiscoveryFailure()); they are listed here, so that the consumer can
        record them, and the generation is not \c complete.  Always empty for a generation sealed without it.

        @since %DataProvider 3.9
    */
    list<hash<DataProviderDiscoveryFailureInfo>> unavailable = ();
}

#! Opaque handle for one open discovery generation
/** Instances can only be created by @ref DataProviderActionCatalog::beginDiscovery().
    @since %DataProvider 3.8
*/
public class DataProviderDiscoverySession {
    private {
        int generation;
    }

    #! Constructor used by DataProviderActionCatalog; externally constructed handles are rejected by identity
    /** @param generation discovery generation number */
    constructor(int generation) {
        self.generation = generation;
    }

    #! Returns the generation number
    /** @return the discovery generation number */
    int getGeneration() {
        return generation;
    }
}

#! Opaque proof that one discovery generation was complete at a catalog revision
/** Instances can only be created by @ref DataProviderActionCatalog::sealDiscovery(). A token becomes invalid after
    any catalog or retained-qualification mutation.

    @since %DataProvider 3.8
*/
public class DataProviderQualifiedDiscovery {
    private {
        int generation;
        int revision;
        hash<DataProviderDiscoveryReportInfo> report;
    }

    #! Constructor used by DataProviderActionCatalog; externally constructed handles are rejected by identity
    /** @param generation discovery generation number
        @param revision exact qualified catalog revision
        @param report immutable qualification report
    */
    constructor(int generation, int revision,
            hash<DataProviderDiscoveryReportInfo> report) {
        self.generation = generation;
        self.revision = revision;
        self.report = report;
    }

    #! Returns the qualified generation
    /** @return the qualified discovery generation */
    int getGeneration() {
        return generation;
    }

    #! Returns the exact qualified catalog revision
    /** @return the exact qualified catalog revision */
    int getRevision() {
        return revision;
    }

    #! Returns the immutable qualification report
    /** @return the immutable qualification report */
    hash<DataProviderDiscoveryReportInfo> getReport() {
        return report;
    }
}

#! Internal, strongly typed state for one open discovery generation
/** Keeping every collection strongly typed prevents empty-list values stored in
    an untyped hash from degrading to \c list<nothing> at a later merge.
*/
hashdecl DataProviderDiscoveryStateInfo {
    int start_revision;
    *int snapshot_revision;
    list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected;
    list<hash<DataProviderDiscoveryExpectedIdentityInfo>> base_expected;
    list<hash<DataProviderDiscoveryExpectedIdentityInfo>> producer_expected;
    list<hash<DataProviderDiscoveryFailureInfo>> failures;
    list<hash<DataProviderDiscoveryFailureInfo>> inventory_failures;
    DataProviderDiscoverySession session;
    *int inventory_revision;
    hash<string, bool> base_expected_keys;
    hash<string, bool> expected_keys;
    *hash<string, bool> app_scope;
}

#! DataProvider action and application catalog class
/** @anchor dataprovider_deferred_output_types
    @par Deferred Output Types

    An action whose output type is expensive to build registers a \c get_output_type callback in place of an
    \c output_type.  The getters that return actions call the callbacks of the actions they return, and return
    each output type in \c output_type:
    - getAppAction() and getAppActionEx() resolve the one action, and getActions(), getActionsEx(), getActionHash(),
      getActionHashEx(), getActionsForScheme() and the \c searchAppAction*() methods resolve the actions of the
      application they return; if a callback fails, they throw an \c ACTION-OUTPUT-TYPE-ERROR exception once every
      action they return has been attempted, and each call attempts a failed callback again
    - getAllActions(), searchActionRegex() and searchActionSubstr() resolve the output types that have not been
      attempted yet and never throw for an output type: a callback that failed is not attempted again by them, and
      its action is returned with \c get_output_type and no \c output_type
    - getAllApps(), getApp(), getAppEx(), searchAppRegex(), searchAppSubstr() and getDataProviderForAction() return
      no output types and do not resolve them (before %DataProvider 3.11 they resolved every deferred output type,
      so a callback that loaded a module registering further applications could add applications to their
      result; no callback in the modules distributed with %Qore does)
    - getActionsNoInit() and getAllAppsNoInit() never call callbacks: an action not resolved yet carries
      \c get_output_type and no \c output_type

    A failed callback is retained like a failed deferred initialization: @ref getInitializationFailures() returns it
    with kind \c "action" and the callback's exception, and a discovery generation reports it, until the output type
    is resolved or the action is registered again.  Retrying a callback that fails the same way does not change the
    catalog revision.

    Callbacks run without the catalog lock held, so a callback can load provider modules.  Two threads asking for
    the same action can both call its callback; the first result is published, and both threads return it.

    @par Example
    @code{.py}
# throws ACTION-OUTPUT-TYPE-ERROR if the output type of the action cannot be built
hash<DataProviderActionInfo> action = DataProviderActionCatalog::getAppActionEx("stripe", "create-customer");
AbstractDataProviderType output_type = action.output_type;
    @endcode

    @anchor dataprovider_lazy_actions
    @par Lazy Action Registration

    A module whose action definitions are expensive to build, such as actions generated from a REST schema,
    registers its application when it is loaded and the code that registers the actions with
    @ref DataProvider::DataProviderActionCatalog::registerLazyAppActions() "registerLazyAppActions()": the actions
    are built when the catalog is first asked for the application or its actions, so loading the module does not
    build them, and a program that never reads them never does.

    The getters that use the application run its lazy registrations first: getApp(), getAppEx(), getActions(),
    getActionsEx(), getActionHash(), getActionHashEx(), getAppAction(), getAppActionEx(), the search methods,
    getActionsForScheme(), getAllApps(), getAllActions() and getPresentationContributionsForModule(), and the
    discovery boundaries checkpointDiscoverySnapshot() and sealDiscovery() for the applications in their scope.
    They therefore return the same as if the module had registered the actions when it was loaded; only the order of
    getAllActions(), which lists actions in the order they were registered, follows the order in which the
    applications were first used.  The actions belong to the module that made the lazy registration (see
    getModulePathsForApp() and getPresentationContributionsForModule()).

    The \c NoInit getters, getActionsNoInit() and getAllAppsNoInit(), never run a deferred registration, pending or
    lazy, and never wait for one: they return what is registered when they are called.  For an application with
    lazy registrations that have not run (see hasLazyAppActions()), that is its definition with the capability flags
    of the actions registered so far, and those actions only.  Listings and other readers that need an
    application's actions without using it read them from the provider index, which holds them for every
    application, lazy or not.

    A reader that no sandbox governs runs the lazy registrations of an application on its own thread, as the module
    would have run them when it was loaded.  A sandboxed reader does not run trusted registrations: what a
    registration creates is shared by every reader of the catalog, and an object created while serving sandboxed
    code keeps that code's sandbox, including its interrupts.  The catalog worker runs them for it: a thread of the
    %DataProvider Program that no sandbox governs, started by the first sandboxed reader that needs it through a
    capability that the module creates when it is loaded (see Qore::SandboxManager::createSystemThreadCall()), so
    that no thread runs until one is needed.  The reader waits for the worker; an interrupt or limit of the reader
    ends only its wait, and the registration runs to its end for every reader.  An exit hook (register_exit_hook())
    stops the worker before the library waits for the threads of the process; the readers still waiting for it then
    get \c DATA-PROVIDER-LAZY-WORKER-STOPPED.  A sandboxed reader is refused with \c DATA-PROVIDER-LAZY-INIT-ERROR,
    and the registration stays pending, when there is no worker (after it was stopped, or when the module was
    loaded by code that may not create the capability) and when it is loading a module: a registration can load a
    module, which another thread cannot load until this thread has loaded its own, so it cannot wait for the worker.
    A module from the module path is library code: its initialization belongs to no sandbox, also when sandboxed code
    loads it first, so its registrations are trusted ones, and what they create belongs to no tenant (see Qore's
    module initialization in the sandboxing guide).  A registration made by code that a sandbox governs (the
    sandboxed code itself, or a module that it loaded by path, also under a policy barrier), lazy or pending, runs
    when it is made, on the registering thread, as before registrations were
    deferred, and so do the callbacks of an action that it registers (\c get_output_type, \c get_dynamic_options):
    no code registered under a sandbox runs later on another thread, where it could block other tenants or trusted
    code.  For the same reason, sandboxed code cannot add a catalog change listener or set a process-wide callback
    (setSchemeLookupCallback(), setGenericApiCallActionBuilder()), and the listeners, which belong to trusted code,
    never run on a sandboxed thread: a worker delivers the changes that it makes, and those made on a sandboxed
    thread; the workers are a small pool, so that a slow registration delays no other sandboxed reader.
    registerLazyValue() builds other shared values, such as types built from a vendored API description, the same
    way.  Every decision on where code runs uses Qore::SandboxManager::isSandboxed(), and every refusal
    (listeners, process-wide callbacks, registrations) Qore::SandboxManager::isRestricted(), which is true only for a
    sandbox that refuses uncontrolled APIs; neither is restricted, so the catalog decides the same for callers
    restricted with
    @ref Qore::PO_NO_UNCONTROLLED_APIS "PO_NO_UNCONTROLLED_APIS".  There is no worker when the module was loaded first
    by code that may not create the worker's capability (restricted with \c PO_NO_UNCONTROLLED_APIS, or sandboxed code
    loading the module by path): then every deferred registration runs, and every lazy value is built, when it is
    registered.

    The deferred initialization of an application, pending or lazy, runs on one thread at a time: a thread that
    uses the application while another thread initializes it waits until the initialization has ended.  A thread
    does not wait if it is loading a module, or if the wait would close a cycle with a thread initializing another
    application that waits for this one; a thread that holds the catalog lock runs no deferred registration.  These
    threads read what has been registered so far.

    A lazy registration that raises an exception is not run again, and the exception is retained as an
    initialization failure of the application (kind \c "app"); see getInitializationFailures().  When the actions
    were registered while the module was loaded, the same exception made the module fail to load.  A registration
    stopped because its thread was interrupted, cancelled or over a sandbox limit, or that ended without an
    exception, as when its thread exits, is not retained: the exception is raised to the reader, and the next reader
    runs the registration again, which replaces the actions that the stopped run registered (each run builds new
    objects for its actions, so they differ from the stopped run's).  Nothing is retained for an application
    deregistered while its registration ran.
*/
#! Holds the catalog lock for a scope, and calls a function once it has released it; for the catalog only
/** Like @ref Qore::Thread::AutoLock "AutoLock", it releases the lock when it goes out of scope, also when an
    exception is raised; then it calls the function given, which delivers the changes made while the lock was held to
    the catalog change listeners (see DataProviderActionCatalog::addCatalogChangeListener()).

    @since %DataProvider 3.11
*/
class DataProviderCatalogLock {
    private {
        Mutex lck;
        code after_unlock;
    }

    constructor(Mutex lck, code after_unlock) {
        lck.lock();
        self.lck = lck;
        self.after_unlock = after_unlock;
    }

    #! Releases the lock, then delivers: both happen here, when the guard goes out of scope
    destructor() {
        lck.unlock();
        call_function(after_unlock);
    }
}

#! A failure of a catalog change listener, retained by the catalog
/** @see DataProviderActionCatalog::getCatalogChangeListenerFailures()

    @since %DataProvider 3.11
*/
public hashdecl DataProviderCatalogChangeListenerFailureInfo {
    #! The listener's handle, as returned by @ref DataProviderActionCatalog::addCatalogChangeListener()
    int listener_id;
    #! The catalog revision the listener was called with
    int revision;
    #! When the listener failed
    date failed;
    #! The exception the listener raised
    hash<ExceptionInfo> error;
}

#! Receives the changes of the data provider catalog
/** Register an object of a subclass with @ref DataProviderActionCatalog::addCatalogChangeListener().  The catalog
    keeps only a weak reference to it: it is called while its owner keeps it, and is dropped once its owner releases
    it, so a listener never keeps an object or a @ref Qore::Program "Program" alive.

    @par Example
    @code{.py}
class ReferenceRearm inherits DataProviderCatalogChangeListener {
    catalogChanged(int revision) {
        # only records the change; the work runs elsewhere
        changes.push(revision);
    }
}
    @endcode

    @since %DataProvider 3.11
*/
public class DataProviderCatalogChangeListener {
    #! Called after the catalog revision advanced
    /** @param revision the catalog revision when the call was made; it is at least the revision of the change that
        caused the call; see @ref DataProviderActionCatalog::getCatalogRevision()

        @note
        - called on the thread that changed the catalog, after it released the catalog lock, so the listener may
          read the catalog; it must return quickly and must not wait for another thread that may be changing the
          catalog
        - calls for successive changes can run concurrently on different threads, and their revisions can arrive
          out of order; one call can stand for several changes
        - an exception raised here is retained by the catalog
          (@ref DataProviderActionCatalog::getCatalogChangeListenerFailures()) and never reaches the code that
          changed the catalog, or the other listeners
    */
    abstract catalogChanged(int revision);
}

public class DataProviderActionCatalog {
    public {
        #! Required application key set
        const hash<string, bool> RequiredAppKeys = {
            "name": True,
            "display_name": True,
            "short_desc": True,
            "desc": True,
            "groups": True,
            "logo": True,
            "logo_file_name": True,
            "logo_mime_type": True,
        };

        #! Required action key set (without action_code and path_vars, which are checked separately)
        const hash<string, bool> RequiredActionKeys = {
            "app": True,
            "action": True,
            "path": True,
            "display_name": True,
            "short_desc": True,
            "desc": True,
        };
    }

    private:internal {
        #! Static builder callback that constructs the framework-injected <tt>make-api-call</tt> action info hash
        /** Signature: <tt>hash<DataProviderActionInfo> sub (hash<DataProviderAppInfo> app)</tt>. The builder is
            normally registered once by the @c RestClientDataProvider module's init block via
            @ref setGenericApiCallActionBuilder().

            When the builder is registered, @ref registerApp() automatically calls it for every app
            whose connection scheme is REST-derived (the scheme's @c cls inherits
            @c RestClient::RestConnection) and which does not set @c disable_generic_api_call to
            @ref True. The returned action is then registered through @ref registerAction(). Apps
            that were registered before the builder was set are processed retroactively when the
            builder is first registered.

            @since DataProvider 3.5
        */
        static *code<hash<DataProviderActionInfo>(hash<DataProviderAppInfo>)> cb_build_generic_api_call_action;

        #! Name of the module that registers the generic @c make-api-call action builder
        /** Loaded on demand by @ref registerApp() when a REST-capable app is registered before the
            builder has been set — i.e. when the embedding application skipped
            @ref DataProvider::registerKnownFactories() (typically because it loaded the data-provider
            index from disk, so the persisted index advertises @c make-api-call but the live catalog
            never receives it). Loading this module runs its init block, which calls
            @ref setGenericApiCallActionBuilder() and drains the pending-app backlog.

            This is a runtime (string) load, not a @c %requires dependency, so it does not create a
            module cycle (@c RestClientDataProvider already @c %requires(reexport) @c DataProvider). The
            same name-based indirection to this module is used by @c DataProvider::FactoryMap.

            @since DataProvider 3.5
        */
        const string GenericApiCallBuilderModule = "RestClientDataProvider";

        #! Late-bound scheme lookup hook — set by @c ConnectionProvider at init time
        /** Signature: <tt>*hash&lt;auto&gt; sub (string scheme)</tt>. Returns the
            @c ConnectionSchemeInfo hash (with @c cls) for the given scheme, or @ref NOTHING /
            throws if the scheme is unknown. Used by @ref isRestScheme() to walk the connection
            class hierarchy without forming a hard dependency on @c ConnectionProvider (which
            already @c %requires(reexport) @c DataProvider — a hard import here would create a
            cycle).

            @since DataProvider 3.5
        */
        static *code<*hash<auto>(string)> cb_lookup_scheme;

        #! Apps awaiting retroactive <tt>make-api-call</tt> registration once the builder is set
        /** Keys are app names. Populated by @ref registerApp() when a REST-capable app is registered before
            the generic API call action builder has been set; drained by @ref setGenericApiCallActionBuilder()
            and @ref clearGenericApiCallActionBuilder() when the builder becomes available.

            @since DataProvider 3.5
        */
        static hash<string, bool> pending_generic_api_call_apps = {};

        #! Mutex for atomic operations
        /** Every critical section holds it with a DataProviderCatalogLock, whose destructor releases it and then
            delivers the changes made while it was held to the catalog change listeners (deliverCatalogChanges()); a
            section entered only when the lock is not held already releases it with unlockCatalog(), which does the
            same
        */
        static Mutex lck();

        #! Guards the catalog change listeners; never held while a listener runs or the catalog lock is taken
        static Mutex listener_lck();

        #! Signalled when a call of a listener being removed has ended
        static Condition listener_cond();

        #! Catalog change listeners by handle
        /** Each entry has \c listener (a weak reference), \c since (the revision when it was added; it is called
            only for later revisions), \c active (calls running), \c tids (thread ID -> calls running on that thread)
            and \c removed
        */
        static hash<string, hash<auto>> listeners = {};

        #! The last listener handle issued
        static int listener_seq = 0;

        #! The last catalog revision delivered to the listeners
        static int notified_revision = 0;

        #! Retained listener failures, oldest first, at most @ref MaxListenerFailures
        static list<hash<DataProviderCatalogChangeListenerFailureInfo>> listener_failures = ();

        #! Top-level master application cache keyed by application name
        static hash<string, hash<DataProviderAppInfo>> appcache = {};

        #! Master action cache, the key is \c app/action
        static hash<string, hash<DataProviderActionInfo>> top_actionmap = {};

        #! Action cache, the key is the application key, the second key is the action string
        static hash<string, hash<string, hash<DataProviderActionInfo>>> app_actionmap = {};

        #! Scheme map; keys are connection schemes
        static hash<string, list<hash<DataProviderActionInfo>>> smap = {};

        #! Actions whose output type is resolved on demand: app -> action -> state
        /** @ref True if the action's \c get_output_type callback has not been called yet, @ref False if it was called
            and failed; an action leaves the map when its output type is resolved
        */
        static hash<string, hash<string, bool>> resolve_output_types = {};

        #! The callbacks that resolved output types: app -> action -> callback
        /** A registration of an action carrying the callback that resolved its output type is the same action
        */
        static hash<string, hash<string, code>> resolved_output_type_callbacks = {};

        #! Pending application init handler map: app -> init handler
        static hash<string, hash<auto>> initmap = {};

        #! Pending application action init handler map: app -> action -> init metadata
        static hash<string, hash<string, hash<auto>>> initactionmap = {};

        #! Lazy action registrations of registered applications: app -> registrations in registration order
        /** Each registration has the same keys as an entry of @ref initmap, with \c kind \c "app", the name of
            the registering module in \c module_name, and the registration serial of the app in \c serial; its
            \c init is the code registered; registrations made under a sandbox run when they are made, so none of
            these is bound to a sandbox; see registerLazyAppActions()
        */
        static hash<string, list<hash<auto>>> lazyactionmap = {};

        #! The last lazy registration ID; see registerLazyAppActions()
        static int lazy_reg_seq = 0;

        #! The actions registered by lazy registrations that have not ended: app -> action -> registration ID
        /** A registration that runs again, because a run was stopped (see doLazyInit()), replaces the actions that its
            stopped run registered: a run builds new objects for its actions, which differ from the stopped run's
        */
        static hash<string, hash<string, int>> lazy_action_regs = {};

        #! The registration serial of each registered application: app -> serial
        /** A lazy registration belongs to the registration of its app that it was made for; it is not retained or
            restored for an app that was deregistered, or deregistered and registered again, while it ran
        */
        static hash<string, int> app_serials = {};

        #! The last application registration serial
        static int app_serial = 0;

        #! The applications whose deferred initialization is running: app -> the initialization
        /** Each entry has the TID of the thread running the initialization in \c tid and, in \c done, a
            @ref Qore::Thread::Counter "Counter" that reaches zero when it ends.  An entry is added, under
            @ref init_lck, before the initialization removes the pending registrations it runs, and removed after
            they have run, so that another thread reading the app's state waits for it (see initPending()).  The
            entry is removed and the counter decremented without a lock, so that ending an initialization cannot
            fail, also for a thread that is interrupted, cancelled or exits
        */
        static hash<string, hash<auto>> init_owners = {};

        #! The application each thread waits to initialize: TID -> app; set under @ref init_lck
        /** Used to detect a cycle of waits between threads initializing different applications
        */
        static hash<string, string> init_waits = {};

        #! Serializes taking an initialization and the cycle check; never held while an initialization runs
        static Mutex init_lck();

        #! The process that the deferred initialization state belongs to; see checkFork()
        static int init_pid = 0;

        #! The queue of the catalog worker, which runs lazy registrations for sandboxed readers; see runOnWorker()
        /** Created when the module is loaded; each job is a hash with the job's \c id and either the \c app whose
            lazy action registrations run, or the \c value ID of a lazy value to build (see registerLazyValue())
        */
        static Queue lazy_jobs();

        #! Guards the catalog worker's state and the results of its jobs
        static Mutex lazy_worker_lck();

        #! Signalled when the catalog worker has ended a job
        static Condition lazy_job_cond();

        #! The jobs whose readers still wait: job ID -> True; removed by the reader, however its wait ends
        static hash<string, bool> lazy_job_waiting = {};

        #! The results of the jobs whose readers wait: job ID -> \c error (@ref NOTHING if it succeeded)
        static hash<string, hash<auto>> lazy_job_results = {};

        #! The last job ID
        static int lazy_job_seq = 0;

        #! The catalog workers running: TID -> @ref True; set under @ref lazy_worker_lck by each worker when it starts
        static hash<string, bool> lazy_workers = {};

        #! The catalog workers started that have not registered in @ref lazy_workers yet; see startLazyWorker()
        /** Set under @ref lazy_worker_lck: a thread runs as a worker only if it takes one of them (see lazyWorkerMain())
        */
        static int lazy_workers_starting = 0;

        #! The jobs queued or running: each worker runs one at a time; set under @ref lazy_worker_lck
        /** Counted when a job is queued and until it has ended, so that a job that a worker has taken from the queue
            but not started yet is counted (see startLazyWorker())
        */
        static int lazy_pending = 0;

        #! The deliveries of catalog changes requested since a worker last started one; see queueDelivery()
        /** Changed with atomic operations only, without a lock, which a thread whose sandbox is interrupted cannot take
        */
        static int lazy_delivery_requests = 0;
%ifdef QoreDebug
        #! Called when a thread that a sandbox governs requests a delivery, before it is queued (debug builds only)
        static *code delivery_test_hook;
%endif

        #! The jobs that catalog workers run: job ID -> the TID of the worker; set under @ref init_lck
        static hash<string, int> lazy_job_running = {};

        #! The process that the catalog workers run in
        static int lazy_worker_pid = getpid();

        #! Set by the exit hook: no worker is started afterwards, and a running one ends
        static bool lazy_worker_shutdown = False;

        #! The capability that starts the catalog worker in a thread that no sandbox governs, or @ref NOTHING
        /** Created when the module is loaded, with the exit hook that stops the worker; see initLazyWorker()
        */
        static *code lazy_worker_start = DataProviderActionCatalog::initLazyWorker();

        #! Lazy values: ID -> \c builder, \c value and \c built; see buildLazyValue()
        /** See registerLazyValue()
        */
        static hash<string, hash<auto>> lazy_values = {};

        #! The last lazy value ID
        static int lazy_value_seq = 0;

%ifdef QoreDebug
        #! The number of jobs queued for the catalog worker since the process started; for tests (debug builds only)
        static int lazy_jobs_submitted = 0;

        #! Signalled when a job is queued for the catalog worker; for tests (debug builds only)
        static Condition lazy_queue_cond();
%endif


        #! Declared pending identities retained until the exact identity succeeds or is explicitly deregistered
        /** An empty action key represents the app itself. This registry makes a pending initializer that returns
            without registering or throwing remain visible to later discovery generations.
        */
        static hash<string, hash<string, bool>> expected_initialization_map = {};

        #! Apps whose deferred initializer returned without registering the app or raising an error
        /** Their pending declaration is kept for discovery (\c expected_initialization_map), but no application
            getter returns them, so getAppNamesNoInit() leaves them out.  An app leaves the map when it is
            registered, registered pending again, or deregistered.
        */
        static hash<string, bool> unregistered_init_map = {};

        #! Retained initialization failures: app -> action (or an empty key for the app) -> failure
        static hash<string, hash<string, hash<DataProviderInitializationFailureInfo>>> initialization_failure_map = {};

        #! Monotonically increasing revision of catalog and qualification state
        static int catalog_revision = 0;

        #! Last allocated discovery generation
        static int discovery_generation = 0;

        #! Open discovery state keyed by generation
        static hash<string, hash<DataProviderDiscoveryStateInfo>> discovery_map = {};

        #! Issued qualified-discovery handle keyed by generation
        static hash<string, DataProviderQualifiedDiscovery> qualified_discovery_map = {};

        #! Lazy expected-inventory providers keyed by a stable technical owner name
        static hash<string, code<list<hash<DataProviderDiscoveryExpectedIdentityInfo>>()>>
            discovery_inventory_provider_map = {};

        #! Revision of registered inventory callbacks and their producer-owned identity content
        static int discovery_inventory_revision = 0;

        #! Maps apps to the modules that define them: app -> module path -> True
        static hash<string, hash<string, bool>> appmodmap = {};

        #! The apps with uncontrolled external I/O by the module that registers them: module name -> app -> True
        /** An app is listed if it declares @ref DataProvider::DataProviderAppInfo::external_io "external_io" and not
            @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware"; maintained with @ref appmodmap
            under the catalog lock, and read without a lock by checkFactorySandboxAccess()
        */
        static hash<string, hash<string, bool>> external_io_module_apps = {};

        #! Whether sandboxed callers may use each registered app: app -> isSandboxAllowed()
        /** @ref False for an app that declares @ref DataProvider::DataProviderAppInfo::external_io "external_io" and
            not @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware", @ref True for every other
            registered app, and no entry for an app that is not registered; maintained with @ref appcache under the
            catalog lock, and read without a lock by getDataProviderForAction(), whose common path is this one lookup
        */
        static hash<string, bool> app_sandbox_state = {};

        #! Maps actions to the modules that define them: app -> action -> module path -> True
        static hash<string, hash<string, hash<string, bool>>> actionmodmap = {};

        #! File API map: app -> file API info
        static hash<string, hash<DataProviderFileApiInfo>> app_fileapimap = {};

        #! Deferred initialization of an app: its pending app registration
        const InitPendingApp = (1 << 0);
        #! Deferred initialization of an app: all of its pending action registrations
        const InitPendingActions = (1 << 1);
        #! Deferred initialization of an app: the pending registration of one action
        const InitPendingAction = (1 << 2);
        #! Deferred initialization of an app: its lazy action registrations
        const InitLazyActions = (1 << 3);
        #! Deferred initialization of an app: everything deferred
        const InitAll = InitPendingApp | InitPendingActions | InitLazyActions;

        #! The build of a lazy value, in @ref init_owners: no getter of an app initializes it
        const InitValue = (1 << 6);

        #! runGated(): run every deferred registration
        const RunAll = 0;
        #! runGated(): run only the deferred registrations that trusted code made (on the catalog worker)
        const RunTrusted = 1;
        #! runGated(): run only the deferred registrations made under a sandbox (on a sandboxed reader's thread)
        const RunGoverned = 2;

        #! The most catalog workers that run at once: a slow registration does not delay the others
        const MaxLazyWorkers = 4;

        #! What a thread waiting for a catalog worker waits for in @ref init_waits, followed by the job's ID; it is
        #! never the beginning of an application name
        const LazyWorkerKey = "<catalog-worker>";

        #! The errors raised when a thread is stopped: interrupted, cancelled, or over a sandbox limit
        /** A lazy registration ended by one of them is run again by the next reader; see isTransientFailure()
        */
        const TransientFailures = {
            "PROGRAM-INTERRUPTED": True,
            "THREAD-CANCELLED": True,
            "SANDBOX-TIMEOUT": True,
            "SANDBOX-MEMORY-LIMIT": True,
            "SANDBOX-ERROR": True,
            "STACK-LIMIT-EXCEEDED": True,
        };

        #! Valid file API write formats
        /** @see @ref DataProviderFileWriteFormats

            @since %DataProvider 3.6
        */
        const hash<string, bool> ValidWriteFormats = {
            DPFW_FILE: True,
            DPFW_BASE64: True,
            DPFW_RAW: True,
        };

        #! Default description
        const string DefaultDescription = "";
    }

    public {
        #! The most catalog change listener failures retained; see @ref getCatalogChangeListenerFailures()
        /** @since %DataProvider 3.11 */
        const MaxListenerFailures = 100;
    }

    #! Registers an app for deferred initialization / initialization on demand
    /** @param app the application to register
        @param init the initialization code that will register the app
        @param module_context optional module path to associate with the app while running \a init

        Code that a sandbox governs (also under a policy barrier) cannot register a pending app, as it registers no
        app (see registerApp()); for a sandboxed reader, the registrations that trusted code made run on a catalog
        worker (see @ref dataprovider_lazy_actions).  When the module has no catalog worker (it was loaded first by
        sandboxed code, or by code that may not create the worker's capability), \a init runs at once for every
        caller.

        @throw DATA-PROVIDER-REGISTRATION-DENIED sandboxed code registers a pending app (since %DataProvider 3.12)

        @since DataProvider 3.0
    */
    static registerPendingApp(string app, code init, *string module_context) {
        # an app name taken by sandboxed code would be denied to the module that owns it
        DataProviderSandboxState::checkRegistration("application", app);
        # without a catalog worker (see initLazyWorker()), and for code that a sandbox governs, the registration runs
        # now, on this thread, so that no code registered under a sandbox runs later on another thread, where it could
        # block other tenants or trusted code
        if (!lazy_worker_start || DataProviderActionCatalog::isGoverned()) {
            DataProviderActionCatalog::checkNotPending(app);
            DataProviderActionCatalog::doInit({
                "init": init,
                "kind": "app",
                "app": app,
                "module_context": module_context ?? DataProviderActionCatalog::registrationModulePath(),
            });
            return;
        }
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        if (appcache{app}) {
            throw "APP-ERROR", sprintf("Application %y is already registered", app);
        }
        if (initmap{app}) {
            throw "APP-ERROR", sprintf("Application %y is already pending", app);
        }

        *string module_path = module_context ?? DataProviderActionCatalog::registrationModulePath();
        initmap{app} = {
            "init": DataProviderActionCatalog::makeLazyCall(init),
            "kind": "app",
            "app": app,
            "module_context": module_path,
            "governed": DataProviderActionCatalog::isGoverned(),
        };
        # its declarations are not known until it is initialized: a sandboxed caller of an object of the module
        # initializes it first (see DataProviderSandboxCheck)
        if (module_path) {
            DataProviderSandboxState::setAppPending(DataProviderActionCatalog::moduleNameFromPath(module_path), app,
                True);
        }
        expected_initialization_map{app}{""} = True;
        remove unregistered_init_map{app};
        DataProviderActionCatalog::addOpenDiscoveryExpectedIdentity(app,
            NOTHING);
        ++catalog_revision;
    }

    #! Registers an app action for deferred initialization / initialization on demand
    /** @param app the application to register
        @param action the action to register
        @param init the initialization code that will register the app

        When the module has no catalog worker, \a init runs at once, on the calling thread (see registerPendingApp());
        sandboxed code cannot register a pending action (see registerAction())

        @throw DATA-PROVIDER-REGISTRATION-DENIED the code registering the action is sandboxed (since %DataProvider 3.12)

        @since DataProvider 3.0
    */
    static registerPendingAppAction(string app, string action, code init) {
        DataProviderSandboxState::checkRegistration("action", app + "/" + action);
        # without a catalog worker, and for code that a sandbox governs, the registration runs now, on this thread
        # (see registerPendingApp())
        if (!lazy_worker_start || DataProviderActionCatalog::isGoverned()) {
            DataProviderActionCatalog::checkNotPending(app, action);
            DataProviderActionCatalog::doInit({
                "init": init,
                "kind": "action",
                "app": app,
                "action": action,
                "module_context": DataProviderActionCatalog::registrationModulePath(),
            });
            return;
        }
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        if (app_actionmap{app}{action}) {
            throw "APP-ERROR", sprintf("Application %y action %y is already registered", app, action);
        }
        if (initactionmap{app}{action}) {
            throw "APP-ERROR", sprintf("Application %y action %y is already pending", app, action);
        }

        initactionmap{app}{action} = {
            "init": DataProviderActionCatalog::makeLazyCall(init),
            "kind": "action",
            "app": app,
            "action": action,
            "module_context": DataProviderActionCatalog::registrationModulePath(),
            "governed": DataProviderActionCatalog::isGoverned(),
        };
        expected_initialization_map{app}{action} = True;
        DataProviderActionCatalog::addOpenDiscoveryExpectedIdentity(app,
            action);
        ++catalog_revision;
    }

    #! Registers code that registers actions of an application when its actions are first read
    /** Use this in a module's initialization to defer building action definitions that are expensive to build,
        such as those generated from a REST schema, until the catalog is first asked for them: loading the module
        then costs nothing for them, and a program that never reads the application's actions never builds them.

        The application must be registered already.  \a init is called once, the first time that the application
        or its actions are read from the catalog (see @ref dataprovider_lazy_actions), and registers the actions
        with registerAction(); everything it registers belongs to the module that called this method, as if that
        module had registered it while it was loaded.

        Every catalog getter that uses the application, and the discovery boundaries checkpointDiscoverySnapshot() and
        sealDiscovery(), runs the application's lazy registrations first, so they return the same as if the actions
        had been registered when the module was loaded; getActionsNoInit() and getAllAppsNoInit() do not (see
        @ref dataprovider_lazy_actions).  A thread that uses the application while another thread runs its lazy
        registrations waits for them to finish.  \a init runs on the thread that first uses the application, or, for a
        sandboxed thread, on the catalog worker, so that it never runs in the sandbox of the thread that first uses the
        application; see @ref dataprovider_lazy_actions.  Sandboxed code cannot make a lazy registration: its actions
        would be decided by the declarations of an app that it does not own (see registerAction()); a deferred
        initialization that trusted code registered may make one when it runs on a sandboxed reader's thread.

        @param app the registered application whose actions \a init registers
        @param init the code that registers the actions; an exception it raises is retained as an initialization
        failure of kind \c "app" (see getInitializationFailures()), and \a init is not called again, unless the
        exception stopped the thread (an interrupt, a cancellation or a sandbox limit), in which case the next
        reader calls \a init again

        @throw APP-ERROR the application is not registered
        @throw DATA-PROVIDER-REGISTRATION-DENIED the code making the registration is sandboxed (since %DataProvider
        3.12)

        @par Example
        @code{.py}
DataProviderActionCatalog::registerApp(<DataProviderAppInfo>{
    "name": AppName,
    # ...
});
DataProviderActionCatalog::registerLazyAppActions(AppName, sub () {
    # the schema is parsed the first time that the app's actions are read
    MySchema::getActionSet().registerActions(AppName);
});
        @endcode

        @since %DataProvider 3.11
    */
    static registerLazyAppActions(string app, code init) {
        DataProviderSandboxState::checkRegistration("lazy action registration", app);
        # without a catalog worker (see initLazyWorker()), and for code that a sandbox governs (a module that sandboxed
        # code loads, also under a policy barrier), the
        # registration runs now, on this thread, as it did when it was not lazy: no code registered under a sandbox
        # runs later on another thread, where it could block other tenants or trusted code; it registers through the
        # catalog, so it runs without the catalog lock
        if (!lazy_worker_start || DataProviderActionCatalog::isGoverned()) {
            if (!appcache{app}) {
                throw "APP-ERROR", sprintf("cannot register lazy actions for application %y, which is not "
                    "registered", app);
            }
            DataProviderActionCatalog::doInitIntern({
                "init": init,
                "kind": "app",
                "app": app,
                "module_context": DataProviderActionCatalog::registrationModulePath(),
                "module_name": DataProviderActionCatalog::registrationModuleName(),
            });
            return;
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        if (!appcache{app}) {
            throw "APP-ERROR", sprintf("cannot register lazy actions for application %y, which is not registered",
                app);
        }

        hash<auto> registration = {
            "init": DataProviderActionCatalog::makeLazyCall(init),
            "kind": "app",
            "app": app,
            "module_context": DataProviderActionCatalog::registrationModulePath(),
            "module_name": DataProviderActionCatalog::registrationModuleName(),
            "serial": app_serials{app},
            "governed": DataProviderActionCatalog::isGoverned(),
            "reg_id": ++lazy_reg_seq,
        };
        if (lazyactionmap{app}) {
            lazyactionmap{app} += registration;
        } else {
            lazyactionmap{app} = (registration,);
        }
        ++catalog_revision;
    }

    #! Registers code that builds a value the first time it is used, in the context of the module registering it
    /** A module that builds something expensive and shared, such as a type built from a vendored API description,
        registers its builder when it is loaded and gets the value with getLazyValue() when it needs it, so that
        loading the module does not build it.

        The value is built where the registering module would have built it when it was loaded: on the reading
        thread for a reader that no sandbox governs, and on the catalog worker for a sandboxed reader, so that what
        the builder creates, which every reader shares afterwards, does not keep the reader's sandbox, including its
        interrupts, and the module's own resources are not denied to it (see @ref dataprovider_lazy_actions).  A
        module whose initialization a sandbox governs (one that sandboxed code loaded by path; a module from the
        module path is initialized with no sandbox provenance) gets its value built at once, as before values were
        lazy: no code registered under a sandbox runs later on another thread.

        The API is for modules: a value can only be registered while a module is being loaded, and lives until the
        end of the process, as the module's Program does.

        @param builder the code that builds the value; it takes no arguments.  An exception that it raises is raised
        to the reader, and the next reader builds the value again

        @return the ID of the value, for getLazyValue()

        @throw DATA-PROVIDER-LAZY-VALUE-ERROR no module is being loaded

        @par Example:
        @code{.py}
class MyProvider {
    private {
        static int request_type_id = DataProviderActionCatalog::registerLazyValue(
            \MyProvider::buildRequestType());
        static *MyRequestType request_type;
    }

    static MyRequestType requestType() {
        return request_type ?? (request_type = DataProviderActionCatalog::getLazyValue(request_type_id));
    }

    private static MyRequestType buildRequestType() {
        return new MyRequestType();
    }
}
        @endcode

        @since %DataProvider 3.11
    */
    static int registerLazyValue(code builder) {
        # the API is for modules, whose Programs live until the end of the process, as the values do
        if (!get_module_context_path()) {
            throw "DATA-PROVIDER-LAZY-VALUE-ERROR", "a lazy value can only be registered while a module is being "
                "loaded";
        }
        # a hash of any values: the value replaces the builder's entry when it is built
        hash<auto> v;
        # without a catalog worker, and for a module that sandboxed code loaded by path, the value is built now, as it was when
        # it was not lazy: no code registered under a sandbox runs later, on another thread
        if (!lazy_worker_start || DataProviderActionCatalog::isGoverned()) {
            v.value = builder();
            v.built = True;
        } else {
            v.builder = builder;
        }
        int id;
        {
            AutoLock al(lazy_worker_lck);
            id = ++lazy_value_seq;
            lazy_values{id.toString()} = v;
        }
        # a module whose load fails removes its values with its other registrations (see ImplicitModuleTransaction)
        string key = id.toString();
        ImplicitModuleTransaction::add(sub () { DataProviderActionCatalog::removeLazyValue(key); });
        return id;
    }

    #! Removes a lazy value of a module whose load failed; a build running ends without storing its value
    private:internal static removeLazyValue(string key) {
        AutoLock al(lazy_worker_lck);
        remove lazy_values{key};
    }

    #! Returns a value registered with registerLazyValue(), building it the first time
    /** @param id the ID returned by registerLazyValue()

        @return the value

        @throw DATA-PROVIDER-LAZY-VALUE-ERROR no value is registered with the ID
        @throw DATA-PROVIDER-LAZY-INIT-ERROR a sandbox governs the reader, the value is not built, and there is no
        catalog worker, or the reader is loading a module

        @since %DataProvider 3.11
    */
    static auto getLazyValue(int id) {
        string key = id.toString();
        *hash<auto> v = lazy_values{key};
        if (!v) {
            throw "DATA-PROVIDER-LAZY-VALUE-ERROR", sprintf("no lazy value is registered with ID %d", id);
        }
        if (v.built) {
            return v.value;
        }
        if (!DataProviderActionCatalog::isGoverned() || DataProviderActionCatalog::isLazyWorker()) {
            return DataProviderActionCatalog::buildLazyValue(key);
        } else {
            # as for lazy registrations (see initPending()): a sandboxed thread loading a module cannot wait for the worker,
            # whose builder can load the module that this thread is loading
            if (get_module_context_path()) {
                throw "DATA-PROVIDER-LAZY-INIT-ERROR", sprintf("lazy value %d has not been built, and a sandboxed "
                    "thread that is loading a module cannot build it or wait for it to be built", id);
            }
            return DataProviderActionCatalog::runOnWorker({"value": id});
        }
    }

    #! Returns @ref True if the application has lazy action registrations that have not run yet
    /** @param app the application

        @return @ref True if registerLazyAppActions() was called for the application and the code it registered has
        not run yet, because the application's actions have not been read from the catalog since

        @note this method does not run the lazy registrations

        @since %DataProvider 3.11
    */
    static bool hasLazyAppActions(string app) {
        return exists lazyactionmap{app};
    }

    #! Retains a structured application or action initialization failure
    /** Provider integrations that perform eager initialization should call this method before propagating an
        initialization exception. Deferred catalog initialization records failures automatically.

        A later successful registration of the same application or action clears the matching failure. Until then,
        the failure remains available across repeated discovery attempts through @ref getInitializationFailures().

        @param kind initialization unit; must be \c app or \c action
        @param app exact technical application identifier
        @param action exact technical action identifier; required for an action failure and omitted for an app failure
        @param ex original exception information
        @param module_context optional module path associated with the registration

        @throw INVALID-ARGUMENT-ERROR for an invalid identity

        @since %DataProvider 3.7
    */
    static reportInitializationFailure(string kind, string app, *string action, hash<ExceptionInfo> ex,
            *string module_context = NOTHING) {
        if ((kind != "app" && kind != "action") || !app.val()
                || (kind == "action" && !action.val()) || (kind == "app" && exists action)) {
            throw "INVALID-ARGUMENT-ERROR", sprintf("invalid data provider initialization failure identity: "
                "kind=%y app=%y action=%y", kind, app, action);
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        initialization_failure_map{app}{action ?? ""} = <DataProviderInitializationFailureInfo>{
            "kind": kind,
            "app": app,
            "action": action,
            "error": ex.err,
            "description": ex.desc,
            "argument": ex.arg,
            "module_context": module_context ?? DataProviderActionCatalog::registrationModulePath(),
        };
        ++catalog_revision;
    }

    #! Returns retained application and action initialization failures
    /** @param apps optional application filter; when omitted, failures for all applications are returned

        @return a snapshot of structured failures; reading the failures does not clear them

        @since %DataProvider 3.7
    */
    static list<hash<DataProviderInitializationFailureInfo>> getInitializationFailures(
            *list<string> apps = NOTHING) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        hash<string, bool> filter = apps ? map {$1: True}, apps.iterator() : {};
        list<hash<DataProviderInitializationFailureInfo>> rv = ();
        foreach string app in (sort(keys initialization_failure_map)) {
            if (apps && !filter{app}) {
                continue;
            }
            foreach string failure_key in (sort(keys
                    initialization_failure_map{app})) {
                rv += initialization_failure_map{app}{failure_key};
            }
        }
        return rv;
    }

    #! Returns the current catalog/qualification revision
    /** Reading the revision is O(1). Ordinary action execution does not read or update it; only discovery and
        registration lifecycle operations use it.

        @return the current monotonically increasing catalog revision

        @since %DataProvider 3.8
    */
    static int getCatalogRevision() {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        return catalog_revision;
    }

    #! Registers a listener called after each change of the catalog revision
    /** @param listener the listener; the catalog keeps a weak reference to it, so it is called only while its owner
        keeps it, and is dropped once it has been released

        @return the listener's handle, for @ref removeCatalogChangeListener()

        The listener is called for every catalog revision after the one current when it was added (see
        @ref getCatalogRevision()); a revision that does not change does not call it.  Calls run on the thread that
        changed the catalog after it released the catalog lock; see
        @ref DataProviderCatalogChangeListener::catalogChanged() for what a listener may do.  An exception raised by a
        listener does not fail the change, nor stop the other listeners: it is retained, see
        @ref getCatalogChangeListenerFailures().

        @par Example
        @code{.py}
ReferenceRearm rearm();
int id = DataProviderActionCatalog::addCatalogChangeListener(rearm);
on_exit DataProviderActionCatalog::removeCatalogChangeListener(id);
        @endcode

        @throw DATA-PROVIDER-LISTENER-ERROR a sandbox governs the caller

        @since %DataProvider 3.11
    */
    static int addCatalogChangeListener(DataProviderCatalogChangeListener listener) {
        # a listener runs on the threads that change the catalog, of other tenants and of trusted code
        if (SandboxManager::isRestricted()) {
            throw "DATA-PROVIDER-LISTENER-ERROR", "code that a sandbox governs cannot add a catalog change listener: it "
                "would run on the threads of other code that changes the catalog";
        }
        # taken before the listener lock, which is never held while the catalog lock is taken
        int since = getCatalogRevision();
        AutoLock al(listener_lck);
        string id = (++listener_seq).toString();
        hash<auto!> entry = {"since": since, "active": 0, "tids": {}, "removed": False, "listener": NOTHING};
        listeners{id} = entry;
        listeners{id}.listener := listener;
        return listener_seq;
    }

    #! Removes a catalog change listener
    /** @param id the handle returned by @ref addCatalogChangeListener()

        @return @ref True if the listener was registered, @ref False if not (also when it was removed already, or
        dropped because its owner released it)

        Once this returns, no call of the listener runs, except one running on this thread (a listener that removes
        itself): calls running on other threads are waited for.  A notification running meanwhile does not call it.

        @since %DataProvider 3.11
    */
    static bool removeCatalogChangeListener(int id) {
        string key = id.toString();
        int tid = gettid();
        AutoLock al(listener_lck);
        *hash<auto> entry = listeners{key};
        if (!entry || entry.removed) {
            return False;
        }
        listeners{key}.removed = True;
        while (listeners{key}.active > (listeners{key}.tids{tid} ?? 0)) {
            listener_cond.wait(listener_lck);
        }
        if (!listeners{key}.active) {
            remove listeners{key};
        }
        # otherwise this thread is in a call of the listener: the entry is removed when the call ends
        return True;
    }

    #! Returns the retained failures of catalog change listeners, oldest first
    /** @param listener_id the handle of one listener; all listeners' failures when omitted

        @return at most @ref MaxListenerFailures failures; reading them does not clear them

        @since %DataProvider 3.11
    */
    static list<hash<DataProviderCatalogChangeListenerFailureInfo>> getCatalogChangeListenerFailures(
            *int listener_id) {
        AutoLock al(listener_lck);
        return exists listener_id
            ? (select listener_failures, $1.listener_id == listener_id)
            : listener_failures;
    }

%ifdef QoreDebug
    #! Runs code while this thread holds the catalog lock (debug builds only)
    /** For tests: catalog code that the catalog runs under its lock can register (see checkNotPending())

        @throw PERMISSION-DENIED the caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static auto runWithCatalogLock(code c) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", "cannot hold the catalog lock from a context with restricted process control";
        }
        # test hooks are for trusted test code only
        if (SandboxManager::isRestricted()) {
            throw "PERMISSION-DENIED", "code that a sandbox governs cannot use the catalog's test hooks";
        }
        lck.lock();
        on_exit DataProviderActionCatalog::unlockCatalog();
        return c();
    }
%endif

    #! Throws \c APP-ERROR if an app, or an action of it, is registered or pending already
    /** Takes the catalog lock unless this thread holds it already: a registration that runs at once can be made by
        code that the catalog runs under its lock
    */
    private:internal static checkNotPending(string app, *string action) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }
        if (exists action) {
            if (app_actionmap{app}{action}) {
                throw "APP-ERROR", sprintf("Application %y action %y is already registered", app, action);
            }
            if (initactionmap{app}{action}) {
                throw "APP-ERROR", sprintf("Application %y action %y is already pending", app, action);
            }
            return;
        }
        if (appcache{app}) {
            throw "APP-ERROR", sprintf("Application %y is already registered", app);
        }
        if (initmap{app}) {
            throw "APP-ERROR", sprintf("Application %y is already pending", app);
        }
    }

    #! Releases the catalog lock and delivers the changes made while it was held to the catalog change listeners
    private:internal static unlockCatalog() {
        lck.unlock();
        DataProviderActionCatalog::deliverCatalogChanges();
    }

    #! Calls the catalog change listeners if the revision advanced since the last delivery; never throws
    /** Does nothing while this thread holds the catalog lock: the section that releases it delivers.

        The listeners belong to code that no sandbox governs (see addCatalogChangeListener()), and never run on a
        thread that a sandbox governs, where they would run under its policy, and could be interrupted with it: the
        changes made on such a thread are delivered by a catalog worker (see queueDelivery()).  A worker delivers the
        changes that it makes itself, so that they are delivered also when the reader that it runs a job for stops
        waiting.
    */
    private:internal static deliverCatalogChanges() {
        if (lck.lockOwner()) {
            return;
        }
        # a thread that a sandbox governs leaves the delivery to a worker, and takes no lock to request it, as it
        # cannot take one when its sandbox is interrupted; without a worker (see initLazyWorker()), there is no
        # listener that code a sandbox does not govern added, as no such code loaded the module
        if (lazy_worker_start && DataProviderActionCatalog::isGoverned()) {
            # read without the lock: a listener added meanwhile is not told of the changes made before it was added
            if (listeners && catalog_revision != notified_revision) {
                DataProviderActionCatalog::queueDelivery();
            }
            return;
        }
        int revision;
        list<hash<auto>> targets = ();
        {
            AutoLock al(listener_lck);
            # the revision only advances, so a delivery never reports an older one than an earlier delivery
            revision = catalog_revision;
            if (revision == notified_revision) {
                return;
            }
            if (!listeners) {
                notified_revision = revision;
                return;
            }
            notified_revision = revision;
            foreach hash<auto> i in (listeners.pairIterator()) {
                if (i.value.removed || revision <= i.value.since) {
                    continue;
                }
                if (!(i.value.listener instanceof DataProviderCatalogChangeListener)) {
                    # its owner released it
                    if (!i.value.active) {
                        remove listeners{i.key};
                    }
                    continue;
                }
                # a strong reference for the call only
                hash<auto!> target = {"id": i.key, "listener": i.value.listener};
                targets += target;
            }
        }
        int tid = gettid();
        foreach hash<auto> target in (targets) {
            if (!DataProviderActionCatalog::beginListenerCall(target.id, tid)) {
                # removed while this notification ran
                continue;
            }
            *hash<ExceptionInfo> failure;
            try {
                target.listener.catalogChanged(revision);
            } catch (hash<ExceptionInfo> ex) {
                failure = ex;
            }
            DataProviderActionCatalog::endListenerCall(target.id, tid, revision, failure);
        }
    }

    #! Queues a delivery of the catalog changes for a catalog worker, unless one is queued already; never throws
    /** For a thread that a sandbox governs (see deliverCatalogChanges()); it does not wait for the delivery.

        Nothing here can be interrupted: a thread whose sandbox is interrupted cannot take a lock (a lock is a
        cancellation point), and the delivery would be lost until the next change.  The request is counted and the job
        queued with atomic operations and a queue push, which are no cancellation points, and a worker is started if
        none runs or is starting, with the start reserved in the same way; a start reserved past the pool's maximum
        ends at once (see lazyWorkerMain()).  At the end of the process, when no worker runs or can start, the changes
        are not delivered.
    */
    private:internal static queueDelivery() {
%ifdef QoreDebug
        if (*code hook = delivery_test_hook) {
            try {
                hook();
            } catch (hash<ExceptionInfo> ex) {
            }
        }
%endif
        # one job for the requests made until a worker starts it, which resets the count first
        if (++lazy_delivery_requests != 1) {
            return;
        }
        if (lazy_worker_pid != getpid()) {
            # the child of a fork(): the worker state of the parent is reset under the lock (see startLazyWorker())
            try {
                AutoLock al(lazy_worker_lck);
                DataProviderActionCatalog::startLazyWorker(True);
            } catch (hash<ExceptionInfo> ex) {
            }
            if (lazy_worker_pid != getpid()) {
                # interrupted: the next request queues the delivery
                lazy_delivery_requests = 0;
                return;
            }
        }
        lazy_jobs.push({"deliver": True});
        if (lazy_workers || lazy_workers_starting || lazy_worker_shutdown) {
            return;
        }
        ++lazy_workers_starting;
        try {
            code start = lazy_worker_start;
            start();
        } catch (hash<ExceptionInfo> ex) {
            # the exit hooks have started: the process is ending
            --lazy_workers_starting;
        }
    }

%ifdef QoreDebug
    #! Sets code that runs when a thread that a sandbox governs requests a delivery, before it is queued
    /** For tests (debug builds only): the code can interrupt the thread's sandbox at that point

        @throw PERMISSION-DENIED the caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static setDeliveryTestHook(*code hook) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", "cannot set a catalog test hook from a context with restricted process control";
        }
        # test hooks are for trusted test code only
        if (SandboxManager::isRestricted()) {
            throw "PERMISSION-DENIED", "code that a sandbox governs cannot use the catalog's test hooks";
        }
        delivery_test_hook = hook;
    }

%endif

    #! Records the start of a listener call unless the listener was removed; never throws
    private:internal static bool beginListenerCall(string id, int tid) {
        AutoLock al(listener_lck);
        if (!listeners{id} || listeners{id}.removed) {
            return False;
        }
        ++listeners{id}.active;
        listeners{id}.tids{tid} = (listeners{id}.tids{tid} ?? 0) + 1;
        return True;
    }

    #! Records the end of a listener call, and its failure; never throws
    private:internal static endListenerCall(string id, int tid, int revision, *hash<ExceptionInfo> failure) {
        AutoLock al(listener_lck);
        if (failure && failure.err != "OBJECT-ALREADY-DELETED") {
            listener_failures += <DataProviderCatalogChangeListenerFailureInfo>{
                "listener_id": id.toInt(),
                "revision": revision,
                "failed": now_us(),
                "error": failure,
            };
            if (listener_failures.size() > MaxListenerFailures) {
                shift listener_failures;
            }
        }
        if (!--listeners{id}.tids{tid}) {
            remove listeners{id}.tids{tid};
        }
        if (!--listeners{id}.active && listeners{id}.removed) {
            remove listeners{id};
        }
        listener_cond.broadcast();
    }

    #! Runs a registry mutation in the same critical section as discovery sealing and publication
    /** Registry owners outside this class use this boundary when their state contributes to an authoritative
        provider index. The mutation and revision increment are one linearizable operation, so a collector cannot
        observe the new registry state with the old catalog revision (or vice versa).

        This API is for registry implementations whose state is serialized with the data-provider catalog. Ordinary
        action execution does not use it; application code should use the typed registration APIs instead.

        @param mutation mutation callback returning whether registry state changed; the revision advances only after
            a successful callback returning @ref True

        @return the callback result

        @since %DataProvider 3.8
    */
    static bool commitDiscoveryCatalogMutation(
            code<bool()> mutation) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }
        bool changed = mutation();
        if (changed) {
            ++catalog_revision;
        }
        return changed;
    }

    #! Registers a lazy expected-inventory provider for discovery qualification
    /** The callback must return the exact application and action identities owned by the producer without
        materializing action schemas. Callbacks are invoked without the catalog lock held.

        Registering or replacing a provider invalidates previously issued qualification tokens.

        @param name stable technical name for the inventory source
        @param provider typed callback returning the source's expected inventory

        @throw INVALID-ARGUMENT-ERROR if \a name is empty

        @since %DataProvider 3.8
    */
    static registerDiscoveryInventoryProvider(string name,
            code<list<hash<DataProviderDiscoveryExpectedIdentityInfo>>()> provider) {
        # it would run in every index build, and could take a provider name from trusted code
        DataProviderSandboxState::checkRegistration("discovery inventory provider", name);
        if (!name.val()) {
            throw "INVALID-ARGUMENT-ERROR",
                "a discovery inventory provider name cannot be empty";
        }
        *code<list<hash<DataProviderDiscoveryExpectedIdentityInfo>>()> previous_provider;
        {
            DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
            previous_provider = discovery_inventory_provider_map{name};
            discovery_inventory_provider_map{name} = provider;
            ++discovery_inventory_revision;
            ++catalog_revision;
        }

        # A provider registered from module init must not survive a failed
        # module load. Restore the exact previous callback (if any) through
        # the same transaction used by apps, types, schemes, and factories.
        ImplicitModuleTransaction::add(sub () {
            DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
            if (discovery_inventory_provider_map{name} !== provider) {
                return;
            }
            if (previous_provider) {
                discovery_inventory_provider_map{name} = previous_provider;
            } else {
                remove discovery_inventory_provider_map{name};
            }
            ++discovery_inventory_revision;
            ++catalog_revision;
        });
    }

    #! Marks one registered producer's lightweight identity inventory as changed
    /** Producers call this only when an exact app/action identity is added or removed. It invalidates inventory
        snapshots and qualified publication tokens without adding work to provider action execution.

        @param name stable technical name passed to @ref registerDiscoveryInventoryProvider()

        @throw INVALID-ARGUMENT-ERROR if the inventory provider is not registered

        @since %DataProvider 3.8
    */
    static markDiscoveryInventoryChanged(string name) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        if (!exists discovery_inventory_provider_map{name}) {
            throw "INVALID-ARGUMENT-ERROR", sprintf(
                "data provider discovery inventory source %y is not registered",
                name);
        }
        ++discovery_inventory_revision;
        ++catalog_revision;
    }

    #! Deregisters a producer inventory callback
    /** Existing discovery snapshots and qualified publication tokens are invalidated only when the named callback
        existed.

        @param name stable technical inventory-source name

        @since %DataProvider 3.8
    */
    static deregisterDiscoveryInventoryProvider(string name) {
        DataProviderSandboxState::checkRegistration("discovery inventory provider", name, True);
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        if (!exists discovery_inventory_provider_map{name}) {
            return;
        }
        remove discovery_inventory_provider_map{name};
        ++discovery_inventory_revision;
        ++catalog_revision;
    }

    #! Adds expected identities to an open discovery generation
    /** Duplicate identities are coalesced by their exact technical app/action key.

        @param session open discovery generation
        @param expected exact app/action identities to add

        @throw INVALID-ARGUMENT-ERROR if an identity is empty or malformed
        @throw DATA-PROVIDER-DISCOVERY-SESSION-ERROR if the session is not open

        @since %DataProvider 3.8
    */
    static addDiscoveryExpectedInventory(DataProviderDiscoverySession session,
            list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected) {
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> normalized
            = DataProviderActionCatalog::normalizeDiscoveryExpectedInventory(
                expected);
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = session.getGeneration().toString();
        *hash<DataProviderDiscoveryStateInfo> state
            = discovery_map{generation_key};
        if (!state || state.session !== session) {
            throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                "data provider discovery generation %d is not open",
                session.getGeneration());
        }
        foreach hash<DataProviderDiscoveryExpectedIdentityInfo> identity in (
                normalized) {
            string key = identity.app + "/" + (identity.action ?? "");
            if (!state.base_expected_keys{key}) {
                state.base_expected_keys{key} = True;
                state.base_expected += identity;
                if (!state.expected_keys{key}) {
                    state.expected_keys{key} = True;
                    state.expected += identity;
                }
            }
        }
        discovery_map{generation_key} = state;
    }

    #! Collects all registered producer inventories into an open discovery generation
    /** A provider exception becomes a structured source/inventory failure in the generation; remaining providers
        are still collected.

        @param session open discovery generation; its scope filters unselected producer identities

        @throw DATA-PROVIDER-DISCOVERY-SESSION-ERROR if the session is not open

        @since %DataProvider 3.8
    */
    static collectDiscoveryExpectedInventory(
            DataProviderDiscoverySession session) {
        hash<string, code<list<hash<DataProviderDiscoveryExpectedIdentityInfo>>()>>
            providers;
        int inventory_revision;
        *hash<string, bool> app_scope;
        int generation = session.getGeneration();
        {
            DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
            *hash<DataProviderDiscoveryStateInfo> state
                = discovery_map{generation.toString()};
            if (!state || state.session !== session) {
                throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                    "data provider discovery generation %d is not open",
                    generation);
            }
            providers = discovery_inventory_provider_map;
            inventory_revision = discovery_inventory_revision;
            app_scope = state.app_scope;
        }
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> collected
            = new list<hash<DataProviderDiscoveryExpectedIdentityInfo>>();
        list<hash<DataProviderDiscoveryFailureInfo>> failures
            = new list<hash<DataProviderDiscoveryFailureInfo>>();
        foreach string name in (sort(keys providers)) {
            try {
                code<list<hash<DataProviderDiscoveryExpectedIdentityInfo>>()>
                    provider = providers{name};
                list<hash<DataProviderDiscoveryExpectedIdentityInfo>> inventory
                    = DataProviderActionCatalog::normalizeDiscoveryExpectedInventory(
                        provider());
                if (app_scope) {
                    inventory = map $1, inventory, app_scope{$1.app};
                }
                collected += inventory;
            } catch (hash<ExceptionInfo> ex) {
                failures += <DataProviderDiscoveryFailureInfo>{
                    "generation": generation,
                    "kind": "source",
                    "phase": "inventory",
                    "identity": name,
                    "error": ex.err,
                    "description": ex.desc,
                    "argument": ex.arg,
                    "module_context": name,
                };
            }
        }
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = generation.toString();
        *hash<DataProviderDiscoveryStateInfo> state
            = discovery_map{generation_key};
        if (!state || state.session !== session) {
            throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                "data provider discovery generation %d is not open",
                generation);
        }
        # Do not certify a callback snapshot that changed while callbacks ran.
        # The caller can start a new generation and collect the new exact state.
        # Preserve callback failures even for a stale snapshot: staleness is an
        # additional qualification failure, not a reason to discard the
        # machine-readable producer error that occurred in the same attempt.
        state.inventory_failures = failures;
        if (inventory_revision != discovery_inventory_revision) {
            state.inventory_revision = NOTHING;
        } else {
            state.producer_expected = collected;
            state.expected = DataProviderActionCatalog::normalizeDiscoveryExpectedInventory(
                state.base_expected + state.producer_expected);
            state.expected_keys = map {
                $1.app + "/" + ($1.action ?? ""): True
            }, state.expected.iterator();
            state.inventory_revision = inventory_revision;
        }
        discovery_map{generation_key} = state;
    }

    #! Starts one generation-scoped discovery qualification session
    /** @param expected optional exact app/action inventory fragments
        @param apps optional exact application scope for pending identities registered while the session is open
        @return an opaque open-session handle

        @throw INVALID-ARGUMENT-ERROR if an expected identity is empty or malformed

        @since %DataProvider 3.8
    */
    static DataProviderDiscoverySession beginDiscovery(
            list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected
                = (), *list<string> apps = NOTHING) {
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> normalized
            = DataProviderActionCatalog::normalizeDiscoveryExpectedInventory(
                expected);
        if (exists apps && !apps) {
            throw "INVALID-ARGUMENT-ERROR",
                "a discovery application scope cannot be empty";
        }
        foreach string app in (apps) {
            if (!app.val()) {
                throw "INVALID-ARGUMENT-ERROR",
                    "a discovery application scope cannot contain an empty identity";
            }
        }
        hash<string, bool> expected_keys = map {
            $1.app + "/" + ($1.action ?? ""): True
        }, normalized.iterator();
        *hash<string, bool> app_scope = apps
            ? map {$1: True}, apps.iterator() : NOTHING;
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        foreach string declared_app in (sort(keys
                expected_initialization_map)) {
            if (app_scope && !app_scope{declared_app}) {
                continue;
            }
            foreach string declared_action in (sort(keys
                    expected_initialization_map{declared_app})) {
                string key = declared_app + "/" + declared_action;
                if (!expected_keys{key}) {
                    expected_keys{key} = True;
                    hash<DataProviderDiscoveryExpectedIdentityInfo> identity
                        = <DataProviderDiscoveryExpectedIdentityInfo>{
                            "app": declared_app,
                        };
                    if (declared_action.val()) {
                        identity.action = declared_action;
                    }
                    normalized += identity;
                }
            }
        }
        int generation = ++discovery_generation;
        DataProviderDiscoverySession session
            = new DataProviderDiscoverySession(generation);
        discovery_map{generation.toString()}
            = <DataProviderDiscoveryStateInfo>{
            "start_revision": catalog_revision,
            "snapshot_revision": NOTHING,
            "expected": normalized,
            "base_expected": normalized,
            "producer_expected": new list<hash<DataProviderDiscoveryExpectedIdentityInfo>>(),
            "failures": new list<hash<DataProviderDiscoveryFailureInfo>>(),
            "inventory_failures": new list<hash<DataProviderDiscoveryFailureInfo>>(),
            "session": session,
            "inventory_revision": discovery_inventory_provider_map
                ? NOTHING : discovery_inventory_revision,
            "base_expected_keys": expected_keys,
            "expected_keys": expected_keys,
            "app_scope": app_scope,
        };
        return session;
    }

    #! Records the revision at which an authoritative consumer starts copying registry state
    /** Source loading, deferred initialization, and producer-inventory collection must finish before this call.
        The consumer then copies its candidate output and calls @ref sealDiscovery(). Any catalog or participating
        registry mutation between this checkpoint and sealing is reported as a structured stale-snapshot failure.

        The lazy action registrations of the applications in the generation's scope (see registerLazyAppActions())
        are run before the revision is recorded.

        @param session open discovery generation

        @throw DATA-PROVIDER-DISCOVERY-SESSION-ERROR if the session is not open

        @since %DataProvider 3.8
    */
    static checkpointDiscoverySnapshot(DataProviderDiscoverySession session) {
        # lazy action registrations are part of the registered state that the snapshot describes
        DataProviderActionCatalog::initDiscoveryLazyActions(session);
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = session.getGeneration().toString();
        *hash<DataProviderDiscoveryStateInfo> state
            = discovery_map{generation_key};
        if (!state || state.session !== session) {
            throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                "data provider discovery generation %d is not open",
                session.getGeneration());
        }
        state.snapshot_revision = catalog_revision;
        discovery_map{generation_key} = state;
    }

    #! Adds one pending registration identity to every applicable open discovery session
    private:internal static addOpenDiscoveryExpectedIdentity(string app,
            *string action) {
        string identity_key = app + "/" + (action ?? "");
        foreach hash<auto> entry in (discovery_map.pairIterator()) {
            hash<DataProviderDiscoveryStateInfo> state = entry.value;
            if ((state.app_scope && !state.app_scope{app})
                    || state.base_expected_keys{identity_key}) {
                continue;
            }
            state.base_expected_keys{identity_key} = True;
            hash<DataProviderDiscoveryExpectedIdentityInfo> identity
                = <DataProviderDiscoveryExpectedIdentityInfo>{
                    "app": app,
                };
            if (action) {
                identity.action = action;
            }
            state.base_expected += identity;
            if (!state.expected_keys{identity_key}) {
                state.expected_keys{identity_key} = True;
                state.expected += identity;
            }
            discovery_map{entry.key} = state;
        }
    }

    #! Validates and coalesces exact discovery identities
    private:internal static list<hash<DataProviderDiscoveryExpectedIdentityInfo>>
            normalizeDiscoveryExpectedInventory(
                list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected) {
        hash<string, bool> seen;
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> normalized
            = new list<hash<DataProviderDiscoveryExpectedIdentityInfo>>();
        foreach hash<DataProviderDiscoveryExpectedIdentityInfo> identity in (
                expected) {
            if (!identity.app.val() || (exists identity.action
                    && !identity.action.val())) {
                throw "INVALID-ARGUMENT-ERROR", sprintf(
                    "invalid expected data provider identity: %y", identity);
            }
            string key = identity.app + "/" + (identity.action ?? "");
            if (!seen{key}) {
                seen{key} = True;
                normalized += identity;
            }
        }
        return normalized;
    }

    #! Adds a structured failure to an open discovery generation
    /** This API is for discovery-source and publication-boundary failures that do not originate in a deferred
        app/action initializer. Such initializer failures are retained by @ref reportInitializationFailure() and
        merged automatically when the session is sealed.

        @param session open discovery generation
        @param kind failing boundary: source, factory, app, action, or scheme
        @param phase qualification phase
        @param identity exact technical failure identity
        @param error original error code
        @param description original error description
        @param argument original exception argument
        @param app exact technical application identifier, when applicable
        @param action exact technical action identifier, when applicable
        @param module_context owner module path, when known

        @throw INVALID-ARGUMENT-ERROR if a required failure field is empty
        @throw DATA-PROVIDER-DISCOVERY-SESSION-ERROR if the session is not open

        @since %DataProvider 3.8
    */
    static reportDiscoveryFailure(DataProviderDiscoverySession session,
            string kind, string phase, string identity, string error,
            string description, auto argument = NOTHING,
            *string app = NOTHING, *string action = NOTHING,
            *string module_context = NOTHING) {
        if (!kind.val() || !phase.val() || !identity.val()
                || !error.val()) {
            throw "INVALID-ARGUMENT-ERROR", sprintf(
                "invalid data provider discovery failure: kind=%y phase=%y identity=%y error=%y",
                kind, phase, identity, error);
        }
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = session.getGeneration().toString();
        *hash<DataProviderDiscoveryStateInfo> state
            = discovery_map{generation_key};
        if (!state || state.session !== session) {
            throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                "data provider discovery generation %d is not open",
                session.getGeneration());
        }
        state.failures += <DataProviderDiscoveryFailureInfo>{
            "generation": session.getGeneration(),
            "kind": kind,
            "phase": phase,
            "identity": identity,
            "app": app,
            "action": action,
            "error": error,
            "description": description,
            "argument": argument,
            "module_context": module_context,
        };
        discovery_map{generation_key} = state;
    }

    #! Aborts an open discovery generation
    /** Safe to call after sealing or after an earlier abort.

        @param session discovery generation to abort

        @since %DataProvider 3.8
    */
    static abortDiscovery(DataProviderDiscoverySession session) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = session.getGeneration().toString();
        if (discovery_map{generation_key}.session === session) {
            remove discovery_map{generation_key};
        }
    }

    #! Returns @ref True if the given discovery failure belongs to one application or action only
    /** Such a failure makes its application or action unavailable without affecting any other: an application or
        action whose initialization failed (phase \c registration), an expected application or action that is not
        registered (phase \c expected-inventory), an application that cannot be published because it has no module
        path or its connection scheme is not registered (phase \c publication), and an action whose metadata is not
        plain data (phase \c serialize-action-metadata).  Failures of discovery sources, factories, and the catalog
        itself, such as a stale snapshot, affect more than one application and are not application failures.

        @param failure the discovery failure

        @return @ref True if the failure identifies an application and is one of the failures above

        @par Example
        @code{.py}
list<hash<DataProviderDiscoveryFailureInfo>> blocking = map $1, report.failures,
    !DataProviderActionCatalog::isAppDiscoveryFailure($1);
        @endcode

        @see sealDiscovery()

        @since %DataProvider 3.9
    */
    static bool isAppDiscoveryFailure(hash<DataProviderDiscoveryFailureInfo> failure) {
        if (!failure.app.val()) {
            return False;
        }
        switch (failure.phase) {
            case "registration":
            case "expected-inventory":
                return failure.kind == "app" || failure.kind == "action";
            case "publication":
                return failure.kind == "app" || failure.kind == "scheme";
            case "serialize-action-metadata":
                return failure.kind == "action";
        }
        return False;
    }

    #! Seals and qualifies one discovery generation
    /** @param session open discovery generation
        @param record_app_failures if @ref True, the failures of individual applications and actions (see
            @ref isAppDiscoveryFailure()) do not block the generation; they are returned in the \c unavailable member
            of the report, which is then not \c complete, so that the consumer can record them; only the other
            failures block the generation

        @return an opaque qualification token bound to the current catalog revision

        The lazy action registrations of the applications in the generation's scope (see registerLazyAppActions())
        are run first; after checkpointDiscoverySnapshot(), only those made since then remain, and they change the
        catalog revision, which is reported as a stale snapshot.

        @throw DATA-PROVIDER-DISCOVERY-ERROR if an expected identity is missing or any applicable structured failure
            remains (with \a record_app_failures, only a failure that is not an application failure); the exception
            argument is a @ref DataProviderDiscoveryReportInfo record
        @throw DATA-PROVIDER-DISCOVERY-SESSION-ERROR if the session is not open

        @par Example
        @code{.py}
# an index is published with the failed apps and actions recorded as unavailable
DataProviderQualifiedDiscovery qualification = DataProviderActionCatalog::sealDiscovery(session, True);
map log("unavailable: %s: %s", $1.identity, $1.error), qualification.getReport().unavailable;
        @endcode

        @since %DataProvider 3.8; \a record_app_failures since %DataProvider 3.9
    */
    static DataProviderQualifiedDiscovery sealDiscovery(
            DataProviderDiscoverySession session, bool record_app_failures = False) {
        # lazy action registrations are part of the registered state that is qualified
        DataProviderActionCatalog::initDiscoveryLazyActions(session);
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        int generation = session.getGeneration();
        string generation_key = generation.toString();
        *hash<DataProviderDiscoveryStateInfo> state
            = discovery_map{generation_key};
        if (!state || state.session !== session) {
            throw "DATA-PROVIDER-DISCOVERY-SESSION-ERROR", sprintf(
                "data provider discovery generation %d is not open",
                generation);
        }
        remove discovery_map{generation_key};

        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected
            = state.expected;
        list<hash<DataProviderDiscoveryFailureInfo>> failures
            = state.failures + state.inventory_failures;
        if (exists state.snapshot_revision
                && state.snapshot_revision != catalog_revision) {
            failures += <DataProviderDiscoveryFailureInfo>{
                "generation": generation,
                "kind": "source",
                "phase": "catalog-snapshot-stale",
                "identity": "data-provider-catalog",
                "error": "DATA-PROVIDER-DISCOVERY-SNAPSHOT-STALE",
                "description": sprintf(
                    "data provider catalog changed from revision %d to %d while the authoritative snapshot was "
                        "copied", state.snapshot_revision, catalog_revision),
            };
        }
        if (state.inventory_revision != discovery_inventory_revision) {
            failures += <DataProviderDiscoveryFailureInfo>{
                "generation": generation,
                "kind": "source",
                "phase": "inventory-stale",
                "identity": "discovery-inventory",
                "error": "DATA-PROVIDER-DISCOVERY-INVENTORY-STALE",
                "description": "data provider discovery inventory changed after its stable snapshot was collected",
            };
        }
        foreach string app in (sort(keys initialization_failure_map)) {
            # Expected-inventory fragments do not imply a selection scope:
            # a full discovery can combine inventories from several producers.
            # Only the caller's explicit app scope may exclude another app's
            # retained failure.
            if (state.app_scope && !state.app_scope{app}) {
                continue;
            }
            foreach string failure_key in (sort(keys
                    initialization_failure_map{app})) {
                hash<DataProviderInitializationFailureInfo> failure
                    = initialization_failure_map{app}{failure_key};
                failures += <DataProviderDiscoveryFailureInfo>{
                    "generation": generation,
                    "kind": failure.kind,
                    "phase": "registration",
                    "identity": failure.action
                        ? failure.app + "/" + failure.action
                        : failure.app,
                    "app": failure.app,
                    "action": failure.action,
                    "error": failure.error,
                    "description": failure.description,
                    "argument": failure.argument,
                    "module_context": failure.module_context,
                };
            }
        }
        foreach hash<DataProviderDiscoveryExpectedIdentityInfo> identity in (
                expected) {
            bool present = identity.action
                ? exists app_actionmap{identity.app}{identity.action}
                : exists appcache{identity.app};
            if (!present) {
                string technical_identity = identity.action
                    ? identity.app + "/" + identity.action : identity.app;
                failures += <DataProviderDiscoveryFailureInfo>{
                    "generation": generation,
                    "kind": identity.action ? "action" : "app",
                    "phase": "expected-inventory",
                    "identity": technical_identity,
                    "app": identity.app,
                    "action": identity.action,
                    "error": "DATA-PROVIDER-EXPECTED-IDENTITY-MISSING",
                    "description": sprintf(
                        "expected data provider identity %y is not registered",
                        technical_identity),
                };
            }
        }
        list<hash<DataProviderDiscoveryFailureInfo>> unavailable = ();
        if (record_app_failures) {
            unavailable = map $1, failures, DataProviderActionCatalog::isAppDiscoveryFailure($1);
            failures = map $1, failures, !DataProviderActionCatalog::isAppDiscoveryFailure($1);
        }
        hash<DataProviderDiscoveryReportInfo> report
            = <DataProviderDiscoveryReportInfo>{
                "generation": generation,
                "start_revision": state.start_revision,
                "sealed_revision": catalog_revision,
                "complete": !failures && !unavailable,
                "expected": expected,
                "apps": sort(keys appcache),
                "actions": sort(keys top_actionmap),
                "failures": failures,
                "unavailable": unavailable,
            };
        if (failures) {
            throw "DATA-PROVIDER-DISCOVERY-ERROR", sprintf(
                "data provider discovery generation %d has %d failure(s)",
                generation, failures.size()), report;
        }
        DataProviderQualifiedDiscovery qualification
            = new DataProviderQualifiedDiscovery(generation,
                catalog_revision, report);
        qualified_discovery_map{generation_key} = qualification;
        return qualification;
    }

    #! Verifies that a qualification token still describes the current catalog
    /** @param qualification token returned by @ref sealDiscovery()

        @throw DATA-PROVIDER-DISCOVERY-TOKEN-ERROR if the token is forged, discarded, or already used
        @throw DATA-PROVIDER-DISCOVERY-STALE if registration or qualification state changed after sealing

        @since %DataProvider 3.8
    */
    static assertQualifiedDiscovery(
            DataProviderQualifiedDiscovery qualification) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = qualification.getGeneration().toString();
        if (qualified_discovery_map{generation_key} !== qualification) {
            throw "DATA-PROVIDER-DISCOVERY-TOKEN-ERROR", sprintf(
                "data provider discovery generation %d has no active qualification token",
                qualification.getGeneration());
        }
        if (qualification.getRevision() != catalog_revision) {
            remove qualified_discovery_map{generation_key};
            throw "DATA-PROVIDER-DISCOVERY-STALE", sprintf(
                "qualified data provider discovery generation %d describes revision %d, but the catalog is now at "
                    "revision %d", qualification.getGeneration(),
                qualification.getRevision(), catalog_revision),
                qualification.getReport();
        }
    }

    #! Discards an unused qualification token
    /** The operation is idempotent and does not change the catalog revision.

        @param qualification token returned by @ref sealDiscovery()

        @since %DataProvider 3.8
    */
    static discardQualifiedDiscovery(
            DataProviderQualifiedDiscovery qualification) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = qualification.getGeneration().toString();
        if (qualified_discovery_map{generation_key} === qualification) {
            remove qualified_discovery_map{generation_key};
        }
    }

    #! Publishes a result while its qualified catalog revision remains stable
    /** The qualification token is single-use. The publisher runs while registration is excluded, closing the
        revision-check/publication race without adding work to ordinary provider execution.

        @param qualification token returned by @ref sealDiscovery()
        @param publisher zero-argument publication callback

        @return the publisher's result

        @throw DATA-PROVIDER-DISCOVERY-TOKEN-ERROR if the token is forged, discarded, or already used
        @throw DATA-PROVIDER-DISCOVERY-STALE if catalog or inventory state changed after sealing

        @since %DataProvider 3.8
    */
    static auto publishQualifiedDiscovery(
            DataProviderQualifiedDiscovery qualification,
            code<auto()> publisher) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        string generation_key = qualification.getGeneration().toString();
        if (qualified_discovery_map{generation_key} !== qualification) {
            throw "DATA-PROVIDER-DISCOVERY-TOKEN-ERROR", sprintf(
                "data provider discovery generation %d has no active qualification token",
                qualification.getGeneration());
        }
        if (qualification.getRevision() != catalog_revision) {
            remove qualified_discovery_map{generation_key};
            throw "DATA-PROVIDER-DISCOVERY-STALE", sprintf(
                "qualified data provider discovery generation %d describes revision %d, but the catalog is now at "
                    "revision %d", qualification.getGeneration(),
                qualification.getRevision(), catalog_revision),
                qualification.getReport();
        }
        remove qualified_discovery_map{generation_key};
        return publisher();
    }

    #! Registers a data provider application
    /** Sandboxed code registers no app: an app name that it took would be denied to the module that owns it, and it
        would decide the declarations of a name that it does not own; modules register their apps in their
        initialization, which no sandbox governs (see @ref dataprovider_external_io)

        @param app the application to register

        @throw APP-ERROR missing or invalid attributes, app already registered
        @throw DATA-PROVIDER-REGISTRATION-DENIED the code registering the app is sandboxed
        (Qore::SandboxManager::isRestricted()) (since %DataProvider 3.12)
    */
    static registerApp(hash<DataProviderAppInfo> app) {
        # an app name taken by sandboxed code would be denied to the module that owns it
        DataProviderSandboxState::checkRegistration("application", app.name ?? "");
        # check for required keys
        if (*hash<auto> missing = RequiredAppKeys - keys app) {
            throw "APP-ERROR", sprintf("Application is missing the following required keys: %y (app: %y)",
                keys missing, app);
        }
        list<string> missing_keys = ();
        foreach hash<auto> elem in (app{keys RequiredAppKeys}.pairIterator()) {
            if (!elem.value.val()) {
                missing_keys += elem.key;
            }
        }
        if (missing_keys) {
            throw "APP-ERROR", sprintf("Application is missing the following required keys: %y (app: %y)",
                missing_keys, app);
        }
        if (DataProviderActionCatalog::checkName(app.name)) {
            throw "APP-ERROR", sprintf("Application name %y is illegal; only A-Za-z0-9_- are accepted", app.name);
        }
        *string module_path = DataProviderActionCatalog::registrationModulePath();

        bool should_auto_register_generic_call = False;
        bool should_load_generic_api_call_builder = False;
        bool app_is_rest_scheme = !app.disable_generic_api_call
            && DataProviderActionCatalog::isRestScheme(app.scheme);
        {
            bool needs_lock = !lck.lockOwner();
            if (needs_lock) {
                lck.lock();
            }
            on_exit if (needs_lock) {
                DataProviderActionCatalog::unlockCatalog();
            }
            if (*hash<DataProviderAppInfo> existing_app = appcache{app.name}) {
                # ignore if the application has the same definition; framework-managed fields
                # (auto-set capability flags and the disable_generic_api_call opt-out, which one
                # caller may set and a duplicate-registration caller may omit) are excluded from
                # the equality check so duplicate registrations from related modules (e.g.
                # FtpClientDataProvider + FtpPoller) keep working
                hash<auto> existing_cmp = existing_app - AppActionFlags - ("disable_generic_api_call",);
                hash<auto> incoming_cmp = app - AppActionFlags - ("disable_generic_api_call",);
                if (existing_cmp == incoming_cmp) {
                    if (module_path) {
                        bool new_owner = !appmodmap{app.name}{module_path};
                        appmodmap{app.name}{module_path} = True;
                        DataProviderActionCatalog::addExternalIoModuleApp(existing_app, module_path);
                        string mac_name = AbstractDataProvider::GenericApiCallActionName;
                        if (app_actionmap{app.name}{mac_name}) {
                            actionmodmap{app.name}{mac_name}{module_path} = True;
                        }
                        if (new_owner) {
                            ++catalog_revision;
                        }
                    }
                    # OR-merge the disable_generic_api_call opt-out: if EITHER caller sets the
                    # flag, the cached app stays opted out (the more-restrictive wins). This
                    # makes the dedupe path order-independent — without it, a duplicate
                    # registration that adds the opt-out flag would be silently lost if a
                    # previous registration didn't have it, and the framework-injected
                    # make-api-call action would remain registered against the caller's intent.
                    bool incoming_disable = app.disable_generic_api_call ?? False;
                    bool existing_disable = existing_app.disable_generic_api_call ?? False;
                    if (incoming_disable && !existing_disable) {
                        ++catalog_revision;
                        appcache{app.name}.disable_generic_api_call = True;
                        remove pending_generic_api_call_apps{app.name};
                        # if the make-api-call action was already auto-registered against the
                        # cached app (because the original registration didn't carry the opt-out),
                        # undo that registration now that the caller has signalled the opt-out
                        string mac_name = AbstractDataProvider::GenericApiCallActionName;
                        if (app_actionmap{app.name}{mac_name}) {
                            int removed_code = app_actionmap{app.name}{mac_name}.action_code;
                            remove app_actionmap{app.name}{mac_name};
                            remove top_actionmap{app.name + "/" + mac_name};
                            remove actionmodmap{app.name}{mac_name};
                            if (existing_app.scheme && smap{existing_app.scheme}) {
                                smap{existing_app.scheme} = map $1, smap{existing_app.scheme},
                                    $1.action != mac_name;
                            }
                            # recompute the corresponding capability flag (supports_apis for
                            # DPAT_API): if no remaining action on this app carries the same
                            # action_code, clear the flag so the cached app metadata doesn't lie
                            # about the app's capabilities
                            *string cap_flag = ActionCodeFlagMap{removed_code};
                            if (cap_flag) {
                                bool still_has = False;
                                foreach hash<DataProviderActionInfo> remaining in
                                        (app_actionmap{app.name}.values()) {
                                    if (remaining.action_code == removed_code) {
                                        still_has = True;
                                        break;
                                    }
                                }
                                if (!still_has) {
                                    appcache{app.name}{cap_flag} = False;
                                }
                            }
                        }
                    }
                    DataProviderActionCatalog::clearSuccessfulInitializationState(
                        app.name, NOTHING);
                    return;
                }
                throw "APP-ERROR", sprintf("Application %y already registered: %y", app.name, existing_app);
            }
            # initmap is keyed by app name (string), not by the DataProviderAppInfo hash —
            # the previous `remove initmap{app}` was a silent no-op (Qore even warns about it
            # at parse time: "cannot be converted to a string, will always evaluate to an empty
            # string at runtime"), leaking pending init entries.
            remove initmap{app.name};

            # add to cache
            appcache{app.name} = app;
            app_sandbox_state{app.name} = DataProviderActionCatalog::isSandboxAllowed(app);
            app_serials{app.name} = ++app_serial;
            ++catalog_revision;
            DataProviderActionCatalog::clearSuccessfulInitializationState(
                app.name, NOTHING);
            if (module_path) {
                appmodmap{app.name}{module_path} = True;
                DataProviderActionCatalog::addExternalIoModuleApp(app, module_path);
            }
            # after its declarations are recorded, so that no check sees the module neither pending nor decided
            DataProviderSandboxState::clearAppPending(app.name);

            # Register a module-init rollback so that a failed module load automatically undoes this
            # registration. This is only meaningful inside a module-load transaction (see
            # DataProvider::tryLoad(), which brackets load_module() with savepoint()/commit()/
            # rollback()). Dynamic apps are registered at runtime - outside any such transaction -
            # and are removed explicitly via deregisterDynamicApp(), so a rollback closure for them
            # is never drained and would leak on the thread_local ImplicitModuleTransaction list;
            # skip it for dynamic apps.
            if (!app.dynamic) {
                ImplicitModuleTransaction::add(sub () { DataProviderActionCatalog::deregisterApp(app.name); });
            }

            # decide whether to auto-register the framework-injected "Make an API call" action for
            # this app. Positive REST detection: inject iff the connection scheme's `cls` inherits
            # RestClient::RestConnection (see isRestScheme()). The opt-out flag remains as a semantic
            # escape hatch for REST apps that intentionally exclude themselves (today only
            # GenericRest, whose own /call action would otherwise be duplicated). The actual action
            # registration is deferred to after the lock is released because registerAction()
            # acquires the same mutex non-reentrantly. The REST predicate is evaluated before taking
            # this lock because scheme lookup may load indexed modules, whose module init can register
            # data provider apps/actions and re-enter this catalog. The same REST predicate is
            # re-applied inside registerGenericApiCallActionForApp() so the backlog drain doesn't
            # re-register against a scheme that has since been seen to be non-REST.
            if (app_is_rest_scheme) {
                if (cb_build_generic_api_call_action) {
                    should_auto_register_generic_call = True;
                } else {
                    # builder is not yet set — queue the app for retroactive registration when
                    # setGenericApiCallActionBuilder() is eventually called, and arrange (after the
                    # lock is released) to load the module that provides the builder, so the backlog is
                    # drained promptly instead of waiting for an external trigger that may never come
                    pending_generic_api_call_apps{app.name} = True;
                    should_load_generic_api_call_builder = True;
                }
            }
        }

        # register the framework-injected "Make an API call" action outside the lock; registerAction()
        # (and the builder-provider module load below) re-acquire the lock cleanly, so this must run
        # unlocked. Errors are swallowed by the helpers to avoid breaking app registration.
        if (should_auto_register_generic_call) {
            # the builder was already available when this app was registered
            DataProviderActionCatalog::registerGenericApiCallActionForApp(app);
        } else if (should_load_generic_api_call_builder) {
            # the app qualifies for the make-api-call action but the builder has not been registered,
            # which means the module that provides it (GenericApiCallBuilderModule) is not loaded in
            # this process. This is the normal case when the embedding application loaded the
            # data-provider index from disk and therefore skipped DataProvider::registerKnownFactories()
            # at startup: the persisted index still advertises make-api-call, but the live catalog would
            # never receive it and action lookups would 404. Load the builder-provider module
            # (idempotent); its init registers the builder and drains the pending backlog. Then register
            # this app explicitly: it is a no-op if the drain already covered it, and it closes the race
            # where a concurrent registration drained the backlog before this app was queued.
            DataProviderActionCatalog::loadGenericApiCallBuilder(app);
            DataProviderActionCatalog::registerGenericApiCallActionForApp(app);
        }
    }

    #! Returns @ref True iff @a scheme's connection class inherits @c RestClient::RestConnection
    /** Implements the positive REST signal used by @ref registerApp() to decide whether to auto-inject
        the framework's @c make-api-call action.  The predicate walks the connection class hierarchy by
        name, so this module does not need to import @c RestClient.

        The @ref ConnectionProvider::ConnectionSchemeCache "ConnectionSchemeCache" lookup is late-bound via @c call_static_method_args() to avoid
        a hard dependency on @c ConnectionProvider — which already declares
        @c "%requires(reexport) DataProvider", so a hard import here would form a cycle.

        @par Failure mode
        Returns @ref False on any error (scheme not registered, @c ConnectionProvider not loaded, @c cls
        absent, etc.).  This is the conservative default: if we cannot prove the scheme is REST, we do
        not inject.  In normal init order, both real registration paths (qlib <tt>*RestClient.qm</tt> and
        module-v8 @c TypeScriptActionInterface) register the scheme @em before the app, so this
        unknown-state path is not expected.

        @param scheme connection scheme name (the @c scheme field on @ref DataProviderAppInfo)

        @return @ref True if @a scheme's connection class is or inherits @c RestClient::RestConnection;
        @ref False otherwise (including for non-REST connections like @c DatasourceConnection,
        @c WebSocketConnectionObject, etc.)

        @since DataProvider 3.5
    */
    private:internal static bool isRestScheme(*string scheme) {
        if (!scheme.val() || !cb_lookup_scheme) {
            return False;
        }
        try {
            *hash<auto> scheme_info = call_function_args(cb_lookup_scheme, (scheme,));
            if (!scheme_info) {
                return False;
            }
            *Class cls = scheme_info.cls;
            if (!cls) {
                return False;
            }
            foreach Class c in (cls.getClassHierarchy()) {
                if (c.getPathName() == "RestClient::RestConnection") {
                    return True;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # scheme lookup callback threw (scheme not registered, etc.) — be conservative and
            # skip injection rather than ship a broken action that would fail at click-time
        }
        return False;
    }

    #! Registers the framework-injected <tt>make-api-call</tt> action for the given REST-capable app
    /** Internal helper; called from @ref registerApp() and from @ref setGenericApiCallActionBuilder() to
        drain the pending app backlog. Swallows any error to avoid breaking the surrounding app
        registration when the builder produces invalid action info.

        @note Gating uses @ref isRestScheme(), a positive predicate over the connection class
        hierarchy (does the scheme's @c cls inherit @c RestClient::RestConnection?). This was chosen
        over the original opt-out denylist because it is provably complete for today's REST apps and
        cannot be silently bypassed by a future non-REST app author forgetting an opt-out flag.

        Belt-and-suspenders: the runtime side (@ref AbstractDataProvider::getRestClientForGenericCallImpl())
        also reflectively discovers a @c rest or @c conn member on the produced provider class and
        logs @c INVALID-CHILD-PROVIDER if none is found.

        @since DataProvider 3.5
    */
    private:internal static registerGenericApiCallActionForApp(hash<DataProviderAppInfo> app) {
        if (!cb_build_generic_api_call_action) {
            return;
        }
        # do not re-register if the action already exists for this app
        if (app_actionmap{app.name}{AbstractDataProvider::GenericApiCallActionName}) {
            DataProviderActionCatalog::setGenericApiCallActionOwners(app.name);
            return;
        }
        # positive REST check: skip non-REST schemes here as well (the same predicate is also applied
        # in registerApp() before queueing, but the backlog drain at setGenericApiCallActionBuilder()
        # calls us with apps that were queued before the builder was registered, so re-checking is
        # cheap and keeps the gating logic in exactly one place)
        if (!DataProviderActionCatalog::isRestScheme(app.scheme)) {
            return;
        }
        try {
            hash<DataProviderActionInfo> action = call_function_args(cb_build_generic_api_call_action, (app,));
            DataProviderActionCatalog::registerAction(action);
            DataProviderActionCatalog::setGenericApiCallActionOwners(app.name);
        } catch (hash<ExceptionInfo> ex) {
            DataProviderActionCatalog::reportInitializationFailure("action", app.name,
                AbstractDataProvider::GenericApiCallActionName, ex);
            # log but do not fail the surrounding registration
            stderr.printf("DataProviderActionCatalog: failed to auto-register generic API call action for "
                "app %y: %s: %s\n", app.name, ex.err, ex.desc);
        }
    }

    #! Loads the module that provides the generic @c make-api-call action builder, on demand
    /** Called from @ref registerApp() when a REST-capable app is registered before the builder has been
        set — e.g. the embedding application loaded the provider index from disk and skipped
        @ref DataProvider::registerKnownFactories(), so @ref GenericApiCallBuilderModule was never
        loaded. Loading it runs its init block, which calls @ref setGenericApiCallActionBuilder() and
        drains the pending-app backlog, so the live catalog gains the @c make-api-call action for the
        queued REST apps and stops diverging from the persisted index.

        The load is idempotent (so it runs effectively once per process — only apps registered before
        the first load reach this path) and is bracketed in a module-init savepoint so a failed or
        partial load rolls back cleanly, mirroring @ref DataProvider::tryLoad(). Must be called with no
        catalog lock held, because draining the backlog re-acquires the lock through registerAction().

        @param app the REST-capable app whose registration triggered the load (used for diagnostics)

        @since DataProvider 3.5
    */
    private:internal static loadGenericApiCallBuilder(hash<DataProviderAppInfo> app) {
        int sp = ImplicitModuleTransaction::savepoint();
        try {
            load_module(GenericApiCallBuilderModule);
            ImplicitModuleTransaction::commit(sp);
        } catch (hash<ExceptionInfo> ex) {
            # roll back any partial registrations from the failed load, then warn loudly: a REST app
            # will silently lack its make-api-call action otherwise
            ImplicitModuleTransaction::rollback(sp);
            DataProviderActionCatalog::reportInitializationFailure("action", app.name,
                AbstractDataProvider::GenericApiCallActionName, ex);
            stderr.printf("DataProviderActionCatalog: REST app %y qualifies for the framework-injected "
                "%y action, but the builder-provider module %y could not be loaded; the action will be "
                "missing from this app: %s: %s\n", app.name, AbstractDataProvider::GenericApiCallActionName,
                GenericApiCallBuilderModule, ex.err, ex.desc);
        }
    }

    #! Sets the static builder used to construct the framework-injected <tt>make-api-call</tt> action info hash
    /** This setter is normally called once at module initialization time by the @c RestClientDataProvider
        module. Subsequent calls with the same builder return @ref True; calls with a different builder when
        a builder is already registered return @ref False without changing the registered builder.

        When the builder is set for the first time, any apps registered earlier that qualified for the
        generic API call action (REST-capable, not opted out) are processed retroactively.

        @param builder a closure with signature
        <tt>hash<DataProviderActionInfo> sub (hash<DataProviderAppInfo> app)</tt> that constructs the
        action info hash for the framework-injected <tt>make-api-call</tt> action

        @return @ref True if the builder was successfully set (first call) or if the same builder is being
        re-registered; @ref False if a different builder is already registered

        @throw DATA-PROVIDER-CALLBACK-ERROR a sandbox governs the caller (see @ref dataprovider_lazy_actions)

        @since DataProvider 3.5
    */
    static bool setGenericApiCallActionBuilder(
            code<hash<DataProviderActionInfo>(hash<DataProviderAppInfo>)> builder) {
        # a process-wide callback runs on the threads of every caller, of other tenants and of trusted code
        DataProviderActionCatalog::checkNotGoverned("set the generic API call action builder");
        list<hash<DataProviderAppInfo>> to_register;
        {
            bool needs_lock = !lck.lockOwner();
            if (needs_lock) {
                lck.lock();
            }
            on_exit if (needs_lock) {
                DataProviderActionCatalog::unlockCatalog();
            }

            if (cb_build_generic_api_call_action) {
                return cb_build_generic_api_call_action == builder;
            }
            cb_build_generic_api_call_action = builder;

            # snapshot the pending backlog while under the lock; defer the actual registerAction() calls
            # until after the lock is released, because registerAction() acquires the same mutex
            foreach string appname in (keys pending_generic_api_call_apps) {
                if (*hash<DataProviderAppInfo> app = appcache{appname}) {
                    to_register += app;
                }
            }
            remove pending_generic_api_call_apps;
        }

        # drain the snapshot outside the lock
        foreach hash<DataProviderAppInfo> app in (to_register) {
            DataProviderActionCatalog::registerGenericApiCallActionForApp(app);
        }
        return True;
    }

    #! Clears the registered generic API call action builder
    /** This method is primarily for testing purposes.

        @throw DATA-PROVIDER-CALLBACK-ERROR a sandbox governs the caller (see @ref dataprovider_lazy_actions)

        @since DataProvider 3.5
    */
    static clearGenericApiCallActionBuilder() {
        # a process-wide callback runs on the threads of every caller, of other tenants and of trusted code
        DataProviderActionCatalog::checkNotGoverned("clear the generic API call action builder");
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        delete cb_build_generic_api_call_action;
        remove pending_generic_api_call_apps;
    }

    #! Returns @ref True if a generic API call action builder is currently registered
    /** @since DataProvider 3.5
    */
    static bool hasGenericApiCallActionBuilder() {
        return exists cb_build_generic_api_call_action;
    }

    #! Registers the late-bound scheme-lookup callback used by @ref isRestScheme()
    /** This setter is normally called once at module initialization time by the
        @c ConnectionProvider module. Subsequent calls with the same callback return @ref True;
        calls with a different callback when one is already registered return @ref False without
        changing the registered callback.

        When the callback is set for the first time, any apps that were registered earlier and are
        currently queued in the pending make-api-call backlog are re-evaluated against the now-
        available scheme lookup; non-REST apps drop out of the queue silently.

        @param cb a closure with signature <tt>*hash&lt;auto&gt; sub (string scheme)</tt> that returns
        the @c ConnectionSchemeInfo hash for the given scheme name. @c cls on the returned hash
        must be the @c Class object for the scheme's connection class (used by
        @ref isRestScheme() to walk the inheritance chain).

        @return @ref True if the callback was successfully set (first call) or if the same callback
        is being re-registered; @ref False if a different callback is already registered

        @throw DATA-PROVIDER-CALLBACK-ERROR a sandbox governs the caller (see @ref dataprovider_lazy_actions)

        @since DataProvider 3.5
    */
    static bool setSchemeLookupCallback(code<*hash<auto>(string)> cb) {
        # a process-wide callback runs on the threads of every caller, of other tenants and of trusted code
        DataProviderActionCatalog::checkNotGoverned("set the scheme lookup callback");
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        if (cb_lookup_scheme) {
            return cb_lookup_scheme == cb;
        }
        cb_lookup_scheme = cb;
        return True;
    }

    #! Clears the registered scheme-lookup callback
    /** This method is primarily for testing purposes.

        @throw DATA-PROVIDER-CALLBACK-ERROR a sandbox governs the caller (see @ref dataprovider_lazy_actions)

        @since DataProvider 3.5
    */
    static clearSchemeLookupCallback() {
        # a process-wide callback runs on the threads of every caller, of other tenants and of trusted code
        DataProviderActionCatalog::checkNotGoverned("clear the scheme lookup callback");
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        delete cb_lookup_scheme;
    }

    #! Throws \c DATA-PROVIDER-CALLBACK-ERROR if a sandbox governs the caller, who cannot change a process-wide callback
    /** Such a callback runs on the threads of every caller of the catalog, of other tenants and of trusted code; a
        module that sandboxed code loads by path cannot set it either (a module from the module path is initialized
        with no sandbox provenance, and can)
    */
    private:internal static checkNotGoverned(string what) {
        if (SandboxManager::isRestricted()) {
            throw "DATA-PROVIDER-CALLBACK-ERROR", sprintf("code that a sandbox governs cannot %s: it would run on the "
                "threads of other tenants and of trusted code", what);
        }
    }

    #! Returns @ref True if a scheme-lookup callback is currently registered
    /** @since DataProvider 3.5
    */
    static bool hasSchemeLookupCallback() {
        return exists cb_lookup_scheme;
    }

    #! Returns zero or more module paths for the given application
    /** @param app the application name

        @return zero or more module paths for the given application
    */
    static *list<string> getModulePathsForApp(string app) {
        return keys appmodmap{app};
    }

    #! Returns the source-owned presentation contributions registered by a module
    /** The module match is based only on the technical module identity in the registration context. Ambient modules
        and transitive dependencies do not contribute entries unless they registered the app or action themselves.

        Each returned entry contains the app metadata, an \c include_app flag indicating whether the module owns the
        app-level presentation, and the actions owned by the module. An action-only contributor to a shared app
        therefore emits only its own action messages into the shared catalog domain.

        @param module_name exact technical module name
        @return presentation contributions owned by the module

        @throw MODULE-ERROR if a module_name is empty

        @par Example
        Extract only the presentation metadata registered by the c json module:
        @code{.qore}
        load_module("json");
        list<hash<auto>> contributions =
            DataProviderActionCatalog::getPresentationContributionsForModule("json");
        @endcode

        @since %DataProvider 3.7
    */
    static list<hash<auto>> getPresentationContributionsForModule(string module_name) {
        if (!module_name.val()) {
            throw "MODULE-ERROR", "module name cannot be empty";
        }

        # Complete deferred registrations before taking the ownership snapshot.
        DataProviderActionCatalog::getAllApps();

        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        list<hash<auto>> rv = ();
        foreach hash<DataProviderAppInfo> app in (appcache.values()) {
            bool include_app = DataProviderActionCatalog::modulePathsContain(
                appmodmap{app.name}, module_name);
            list<hash<DataProviderActionInfo>> actions = map $1,
                app_actionmap{app.name}.values(),
                DataProviderActionCatalog::modulePathsContain(
                    actionmodmap{app.name}{$1.action}, module_name);
            if (include_app || actions) {
                rv += {
                    "app": app,
                    "include_app": include_app,
                    "actions": actions,
                };
            }
        }
        return rv;
    }

    #! Removes a dynamic data provider application
    /** @param name the name of the dynamic application to remove

        @return the app, if it was deregistered

        The caller must not be restricted with @ref Qore::PO_NO_PROCESS_CONTROL

        @throw PERMISSION-DENIED if the app is not dynamic or the caller is restricted with
            @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static *hash<DataProviderAppInfo> deregisterDynamicApp(string name) {
        # sandboxed code would remove an app that it does not own from every caller
        DataProviderSandboxState::checkRegistration("application", name, True);
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", sprintf("Cannot remove app %y from a context with restricted process control",
                name);
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        *hash<DataProviderAppInfo> app = appcache{name};
        if (app && !app.dynamic) {
            throw "PERMISSION-DENIED", sprintf(
                "Cannot remove non-dynamic app %y", name);
        }
        bool had_unmaterialized_state = exists initmap{name}
            || exists initialization_failure_map{name}
            || exists app_actionmap{name} || exists initactionmap{name}
            || exists lazyactionmap{name}
            || exists expected_initialization_map{name}
            || exists resolve_output_types{name} || exists appmodmap{name}
            || exists actionmodmap{name}
            || exists pending_generic_api_call_apps{name};
        remove initmap{name};
        DataProviderSandboxState::clearAppPending(name);
        remove initialization_failure_map{name};
        remove expected_initialization_map{name};
        remove unregistered_init_map{name};
        # remove the app's actions from both maps together; removing app_actionmap{name} on its
        # own here (and leaving the top_actionmap cleanup to deregisterAppIntern below) left the
        # top_actionmap entries - and their option/data-type graphs - leaked in the static map
        *hash<string, hash<DataProviderActionInfo>> dactions = remove app_actionmap{name};
        map remove top_actionmap{name + "/" + $1.action}, dactions.values();
        remove initactionmap{name};
        remove lazyactionmap{name};
        remove lazy_action_regs{name};
        remove app_serials{name};
        remove resolve_output_types{name};
        remove resolved_output_type_callbacks{name};
        DataProviderActionCatalog::removeExternalIoModuleApp(name);
        remove app_sandbox_state{name};
        remove appmodmap{name};
        remove actionmodmap{name};
        remove pending_generic_api_call_apps{name};
        if (!app) {
            if (had_unmaterialized_state) {
                ++catalog_revision;
            }
            return;
        }
        deregisterAppIntern(name);
        return app;
    }

    #! Returns whether executing an action can change provider or external state
    /** An explicit @ref DataProviderActionBaseInfo::mutates_state "mutates_state" value is authoritative.  When
        it is unset, event and record-read actions are read-only and every other action type is treated as
        mutating.  The conservative default is intentional: an unknown or generic API action must not become
        executable in a read-only context merely because its identifier or translated description resembles a
        read operation.

        @param action action definition to classify
        @return False only when structural producer metadata or the exact action type establishes that the
        action is read-only

        @throw no exceptions are expected

        @par Example
        A schema-generated HTTP \c GET action carries \c mutates_state = \c False and is classified as read-only.
        An unannotated generic @ref DPAT_API "DPAT_API" action is classified as mutating.

        @note Producers must derive an explicit read-only value from implementation or protocol semantics; action
        identifiers, descriptions, and translated presentation text are never evidence that an action is read-only.

        @since %DataProvider 3.7
    */
    static bool actionMutatesState(hash<DataProviderActionInfo> action) {
        if (action.mutates_state.typeCode() == NT_BOOLEAN) {
            return action.mutates_state;
        }
        switch (action.action_code) {
            case DPAT_EVENT:
            case DPAT_FIND:
            case DPAT_FIND_SINGLE:
                return False;
        }
        return True;
    }

    #! Registers a data provider action
    /** Sandboxed code registers no action: an action's provider is decided by the declarations of the app it belongs
        to (see @ref dataprovider_external_io), so an action that sandboxed code added to an app that it does not own
        would be decided by that app's declarations

        @param action the action to register

        @throw ACTION-ERROR missing or invalid attributes, action already registered
        @throw DATA-PROVIDER-REGISTRATION-DENIED the code registering the action is sandboxed, also through a policy
        barrier or a system-policy capability (since %DataProvider 3.12)

        The \c app and \c action attributes must be unique for every action registered
    */
    static registerAction(hash<DataProviderActionInfo> action) {
        # an action registered by sandboxed code would be decided by the declarations of an app it does not own
        DataProviderSandboxState::checkRegistration("action", sprintf("%s/%s", action.app ?? "", action.action ?? ""));
        *string module_path = DataProviderActionCatalog::registrationModulePath();

        # check for required keys
        if (*hash<auto> missing = RequiredActionKeys - keys action) {
            throw "ACTION-ERROR", sprintf("action is missing the following required keys: %y (action: %y)",
                keys missing, action);
        }
        list<string> missing_keys = ();
        foreach hash<auto> elem in (action{keys RequiredActionKeys}.pairIterator()) {
            if (!elem.value.val()) {
                missing_keys += elem.key;
            }
        }
        if (missing_keys) {
            throw "ACTION-ERROR", sprintf("action is missing the following required keys: %y (action: %y)",
                missing_keys, action);
        }
        if (!ActionCodeFlagMap.hasKey(action.action_code)) {
            throw "ACTION-ERROR", sprintf("invalid action code %y in action: %y", action.action_code, action);
        }
        if (action.action_code == DPAT_PIPELINE_PROCESSOR && !action.processor_capabilities) {
            throw "ACTION-ERROR", sprintf("pipeline processor action %y/%y must have processor_capabilities set",
                action.app, action.action);
        }
        if (DataProviderActionCatalog::checkName(action.action)) {
            throw "APP-ERROR", sprintf("Action name %y is illegal; only A-Za-z0-9_- are accepted", action.action);
        }
        action.path = DataProviderActionCatalog::normalizePath(action.path);

        # check that all path variables are defined
        if (action.path_vars) {
            softlist<string> vars = (action.path =~ x/\{([^\}]*)}/g);
            if (*hash<string, hash<DataProviderPathVarInfo>> pvc = (action.path_vars - vars)) {
                throw "ACTION-ERROR", sprintf("action has definitions for nonexistent path variables: %y", keys pvc);
            }
            if (vars) {
                hash<string, bool> vmap = map {$1: True}, vars;
                if (*hash<string, bool> ev = vmap - keys action.path_vars) {
                    throw "ACTION-ERROR", sprintf("action is missing definitions for path variables: %y", keys ev);
                }
            }
        } else {
            if (*string var = (action.path =~ x/\{([^\}]*)}/[0])) {
                throw "ACTION-ERROR", sprintf("action has path variables without definitions; path: %y", action.path);
            }
        }

        bool has_get_output_type = action.hasKey("get_output_type");
        if (action.output_type && has_get_output_type) {
            throw "ACTION-ERROR", sprintf("action has both \"output_type\" and \"get_output_type\"; path: %y",
                action.path);
        }
        if (action.output_data_type == DPAD_CREATE_OUTPUT) {
            if (action.action_code != DPAT_CREATE) {
                throw "ACTION-ERROR", sprintf("action %s/%s has output data type %y, which is only valid for create "
                    "actions", action.app, action.action, DPAD_CREATE_OUTPUT);
            }
            if (!action.output_type && !has_get_output_type) {
                throw "ACTION-ERROR", sprintf("action %s/%s has output data type %y but no output type describing the "
                    "generated output", action.app, action.action, DPAD_CREATE_OUTPUT);
            }
        }

        if (action.hasKey("get_dynamic_options") && !action.has_dynamic_options) {
            action.has_dynamic_options = True;
        }

        # code that a sandbox governs: the action's callbacks run now, on this thread, so that none runs later on the
        # thread of another reader, where it could block other tenants or trusted code
        if (DataProviderActionCatalog::isGoverned()) {
            if (has_get_output_type) {
                *code get_output_type = remove action.get_output_type;
                has_get_output_type = False;
                if (get_output_type) {
                    action.output_type = get_output_type();
                }
            }
            if (*code get_dynamic_options = remove action.get_dynamic_options) {
                get_dynamic_options(\action);
            }
        }

        # auto-detect the module providing the class so callers can load it before Class::forName()
        if (action.cls && !action.cls_mod) {
            action.cls_mod = DataProviderActionCatalog::registrationModuleName();
        }

        if (!action.input_data_types) {
            switch (action.action_code) {
                case DPAT_CREATE:
                case DPAT_UPSERT:
                    action.input_data_types = {
                        DPAD_NORMAL: True,
                        DPAD_RECORD_SET: True,
                        DPAD_RECORD_STREAM: True,
                    };
                    break;

                case DPAT_PIPELINE_PROCESSOR:
                    action.input_data_types = {
                        DPAD_NORMAL: True,
                        DPAD_RECORD_SET: True,
                    };
                    break;

                default:
                    action.input_data_types = {
                        DPAD_NORMAL: True,
                    };
                    break;
            }
        }

        if (!action.output_data_type) {
            switch (action.action_code) {
                case DPAT_FIND:
                    action.output_data_type = DPAD_RECORD_STREAM;
                    break;

                case DPAT_CREATE:
                    action.output_data_type = DPAD_RECORD_SET;
                    break;

                case DPAT_PIPELINE_PROCESSOR:
                    action.output_data_type = DPAD_NORMAL;
                    break;

                default:
                    action.output_data_type = DPAD_NORMAL;
                    break;
            }
        }

        string key = sprintf("%s/%s", action.app, action.action);

        # a pending app is initialized before the catalog lock is taken: its initialization registers through the
        # catalog, and can read other apps, whose deferred initializations are not run under the lock
        DataProviderActionCatalog::initPending(action.app, InitPendingApp);

        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
        remove initactionmap{action.app}{action.action};

        *hash<DataProviderAppInfo> app = appcache{action.app};
        if (!app) {
            throw "ACTION-ERROR", sprintf("Application %y not registered (action %y)", action.app, action);
        }
        if (app.scheme && exists action.cls) {
            throw "ACTION-ERROR", sprintf("Application %y has scheme %y and action has cls %y; it is not allowed to "
                "provide an action class when the application has a scheme", action.app, app.scheme, action.cls);
        }
        appcache{action.app}{ActionCodeFlagMap{action.action_code}} = True;
        if (*hash<DataProviderActionInfo> act = app_actionmap{action.app}{action.action}) {
            # a lazy registration that runs again replaces what its stopped run registered
            if (exists _lazy_reg_id && lazy_action_regs{action.app}{action.action} == _lazy_reg_id) {
                DataProviderActionCatalog::replaceAction(app, key, action, has_get_output_type);
                return;
            }
            # the definition of an action whose deferred output type was resolved carries the callback that
            # resolved it, and is the same action
            hash<DataProviderActionInfo> registered = action;
            if (has_get_output_type && !act.get_output_type
                    && action.get_output_type == resolved_output_type_callbacks{action.app}{action.action}) {
                remove registered.get_output_type;
                registered.output_type = act.output_type;
            }
            # ignore duplicates to cover injection scenarios
            if (!DataProviderActionCatalog::compareActions(registered, act)) {
                if (module_path) {
                    bool new_owner = !actionmodmap{action.app}{action.action}{module_path};
                    actionmodmap{action.app}{action.action}{module_path} = True;
                    if (new_owner) {
                        ++catalog_revision;
                    }
                }
                DataProviderActionCatalog::clearSuccessfulInitializationState(
                    action.app, action.action);
                return;
            }
            list<string> diff = ();
            foreach hash<auto> elem in (act.pairIterator()) {
                if (elem.value != registered{elem.key}) {
                    diff += elem.key;
                }
            }
            throw "ACTION-ERROR", sprintf("Action with application %y, action %y already registered; diff: %y",
                act.app, act.action, diff);
        }
        # add to caches
        top_actionmap{key} = action;
        app_actionmap{action.app}{action.action} = action;
        ++catalog_revision;
        if (exists _lazy_reg_id) {
            lazy_action_regs{action.app}{action.action} = _lazy_reg_id;
        }
        if (module_path) {
            actionmodmap{action.app}{action.action}{module_path} = True;
        }
        # add to scheme map
        if (app.scheme) {
            if (!smap{app.scheme}) {
                smap{app.scheme} = ();
            }
            smap{app.scheme} += action;
        }
        if (has_get_output_type) {
            resolve_output_types{action.app}{action.action} = True;
        }
        DataProviderActionCatalog::clearSuccessfulInitializationState(
            action.app, action.action);
    }

    #! Replaces an action that a stopped run of the lazy registration running now registered; lck is held
    private:internal static replaceAction(hash<DataProviderAppInfo> app, string key, hash<DataProviderActionInfo> action,
            bool has_get_output_type) {
        top_actionmap{key} = action;
        app_actionmap{action.app}{action.action} = action;
        if (app.scheme && smap{app.scheme}) {
            smap{app.scheme} = map ($1.app == action.app && $1.action == action.action) ? action : $1,
                smap{app.scheme};
        }
        remove resolved_output_type_callbacks{action.app}{action.action};
        if (has_get_output_type) {
            resolve_output_types{action.app}{action.action} = True;
        } else {
            remove resolve_output_types{action.app}{action.action};
        }
        ++catalog_revision;
        DataProviderActionCatalog::clearSuccessfulInitializationState(action.app, action.action);
    }

    #! Checks for a valid name
    /** @return -1 if the name is invalid, meaning empty or containing invalid characters
    */
    static *int checkName(string name) {
        return name.val() && name =~ /^[-a-z0-9_]+$/i ? 0 : -1;
    }

    #! Compares two actions for equality
    /** @return 0 for equal, -1 for not equal
    */
    static int compareActions(hash<DataProviderActionInfo> act0, hash<DataProviderActionInfo> act1) {
        *hash<string, hash<ActionOptionInfo>> co0 = remove act0.options;
        *hash<string, hash<ActionOptionInfo>> co1 = remove act1.options;

        return act0 != act1 || DataProviderActionCatalog::compareOptions(co0, co1) ? -1 : 0;
    }

    #! Compares two option hashes for equality
    /** @return 0 for equal, -1 for not equal
    */
    static int compareOptions(*hash<string, hash<ActionOptionInfo>> opt0,
            *hash<string, hash<ActionOptionInfo>> opt1) {
        if (opt0.size() != opt1.size()) {
            return -1;
        }
        if (!opt0 && !opt1) {
            return 0;
        }
        foreach hash<auto> i in (opt0.pairIterator()) {
            *hash<ActionOptionInfo> opt = opt1{i.key};
            if (!opt) {
                return -1;
            }
            *Type t0 = (remove i.value.type).getValueType();
            *Type t1 = (remove opt.type).getValueType();
            if (DataProviderActionCatalog::compareBaseTypes(t0, t1)) {
                return -1;
            }
            if (i.value != opt) {
                return -1;
            }
        }
        return 0;
    }

    #! Compares two types for equality
    static int compareBaseTypes(*Type t0, *Type t1) {
        if (!t0) {
            return !t1 ? 0 : -1;
        }
        if (!t1) {
            return -1;
        }
        return t0.isEqual(t1) ? 0 : -1;
    }

    #! Returns all applications in the cache without initializing actions
    /**
        @return all applications in the cache without initializing actions

        @note no deferred registration is run or waited for: an application whose lazy registrations have not run
        (see hasLazyAppActions()) has the capability flags of the actions registered so far; see
        @ref dataprovider_lazy_actions
    */
    static *list<hash<DataProviderAppInfo>> getAllAppsNoInit() {
        return appcache.values();
    }

    #! Returns the names of all registered applications, including pending ones, without initializing any
    /** An application registered with @ref registerPendingApp() is initialized when it is first used, so
        @ref getAllAppsNoInit() leaves it out until then, and @ref getAllApps() initializes every pending application
        to list it.  This returns the names of both kinds without initializing anything:
        - the applications registered with @ref registerApp()
        - the applications pending initialization
        - the applications whose deferred initialization is running
        - the applications with lazy action registrations, without running those registrations: such an
          application is registered, so it is listed with the others

        An application whose deferred initialization failed, or ended without registering the application or
        raising an error, is left out, as no application getter returns it; so is a deregistered application, also
        one deregistered while its initialization runs.

        The names are read from one consistent state of the catalog, so an application that another thread is
        initializing meanwhile is included, whether it is still pending, initializing, or registered.

        @return the application names, each once, in their original case, sorted; empty if no application is
        registered

        @par Example
        @code{.py}
# initialize only the applications that the provider index does not describe
foreach string app in (DataProviderActionCatalog::getAppNamesNoInit()) {
    if (!ProviderIndexUtil::tryGetAppInfo(app)) {
        *hash<DataProviderAppInfo> info = DataProviderActionCatalog::getApp(app);
    }
}
        @endcode

        @note @ref getApp() initializes a pending application and returns it

        @since %DataProvider 3.11
    */
    static list<string> getAppNamesNoInit() {
        hash<string, bool> names = {};
        {
            bool needs_lock = !lck.lockOwner();
            if (needs_lock) {
                lck.lock();
            }
            on_exit if (needs_lock) {
                DataProviderActionCatalog::unlockCatalog();
            }
            map names{$1} = True, keys appcache;
            map names{$1} = True, keys initmap;
            # an app whose deferred initialization is running is in neither map: its initializer is removed from
            # initmap before it is called, and the app enters appcache when the initializer registers it; its
            # pending declaration is kept until then, a failure is retained if the initializer fails, and an
            # initializer that registers nothing is recorded when it returns
            foreach hash<auto> i in (expected_initialization_map.pairIterator()) {
                if (exists i.value{""} && !exists initialization_failure_map{i.key}{""}
                        && !unregistered_init_map{i.key}) {
                    names{i.key} = True;
                }
            }
        }
        return sort(keys names);
    }

    #! Returns all applications in the cache
    /**
        @return all applications in the cache
        @note application getters do not resolve the deferred output types of actions; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderAppInfo>> getAllApps() {
        DataProviderActionCatalog::initAllPending();
        return appcache.values();
    }

    #! Completes the deferred initialization of apps whose modules are loaded, as getApp() does
    /** Unlike getApp(), the provider index is not consulted and no module is loaded: an app whose module is not
        loaded, or that is not registered, is ignored.  An initialization that fails is recorded as getApp() records
        it, and does not raise an exception here, unless the thread is stopped (interrupted, cancelled or over a
        sandbox limit).

        @param apps the names of the apps

        @see DataProviderSandboxCheck, which initializes the deferred apps of a module before deciding whether a
        sandboxed caller may use its objects

        @since %DataProvider 3.12
    */
    static initLoadedApps(list<string> apps) {
        foreach string app in (apps) {
            try {
                DataProviderActionCatalog::initApp(app);
            } catch (hash<ExceptionInfo> ex) {
                # a stopped thread stops here too
                if (DataProviderActionCatalog::isTransientFailure(ex)) {
                    rethrow;
                }
                # otherwise the failure is recorded with the app's initialization state, and the app stays pending
            }
        }
    }

    #! Returns the application definition for the given application name
    /** @param app the app name

        @return the application definition for the given application name or @ref nothing if not present

        @see getAppEx()
        @note application getters do not resolve the deferred output types of actions; see
        @ref dataprovider_deferred_output_types
    */
    static *hash<DataProviderAppInfo> getApp(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        return appcache{app};
    }

    #! Returns the application definition for the given application name
    /** @param app the app name

        @return the application definition for the given application name; throws an exception if the app is not
        registered

        @throw APP-ERROR the given application is not registered

        @see getApp()
        @note application getters do not resolve the deferred output types of actions; see
        @ref dataprovider_deferred_output_types
    */
    static hash<DataProviderAppInfo> getAppEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (*hash<DataProviderAppInfo> rv = appcache{app}) {
            return rv;
        }
        throw "APP-ERROR", sprintf("application %y is not registered", app);
    }

    #! Returns @ref True if sandboxed callers may use the given app
    /** An app that declares @ref DataProvider::DataProviderAppInfo::external_io "external_io" (network or filesystem
        I/O that the %Qore sandbox does not see) may be used by sandboxed callers only if it also declares
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware" (it checks that I/O against the
        caller's policy itself); every other app may be used by them.  This is the rule of
        @ref dataprovider_external_io, without deciding whether the caller is sandboxed.

        @param app the app's registered info

        @return @ref True if sandboxed callers may use the app, @ref False if they must be refused

        @note Consumers decide from the registered info: use isSandboxAllowed(string), which reads it, rather than
        info that a caller supplied

        @see checkSandboxAccess()

        @since %DataProvider 3.12
    */
    static bool isSandboxAllowed(hash<DataProviderAppInfo> app) {
        return !app.external_io || app.sandbox_aware;
    }

    #! Returns @ref True if sandboxed callers may use the registered app with the given name
    /** The decision is made from the app's registered info: the catalog's, or the provider index's if the app is
        not loaded; nothing is initialized or loaded.  An app that is registered nowhere cannot be decided on, and is
        not allowed.

        @param app the app's name

        @return @ref True if sandboxed callers may use the app, @ref False if they must be refused or if the app is
        not registered

        @since %DataProvider 3.12
    */
    static bool isSandboxAllowed(string app) {
        *hash<DataProviderAppInfo> info = DataProviderActionCatalog::getRegisteredAppInfo(app);
        return info ? DataProviderActionCatalog::isSandboxAllowed(info) : False;
    }

    #! Refuses a sandboxed caller the use of an app whose external I/O the sandbox cannot control
    /** A sandboxed caller is refused an app that declares
        @ref DataProvider::DataProviderAppInfo::external_io "external_io" without
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware"; trusted callers are never refused;
        see @ref dataprovider_external_io.

        The catalog applies it itself (with no \a sandboxed argument) where an app's I/O can be reached:
        getDataProviderForAction(), and every AbstractDataProviderFactory method that creates a provider, for the
        factories of a module that registers such an app.  Data provider consumers apply it, with
        checkSandboxAccess(string, *bool), before they use an app on behalf of a caller on a thread that the caller's
        sandbox does not govern.

        @param app the app's registered info; its \c name and \c display_name name it in the error
        @param sandboxed whether the caller is sandboxed; if not given, the caller is sandboxed if a sandbox that
        refuses uncontrolled APIs governs the current thread's code (Qore::SandboxManager::isRestricted()), also
        through a policy barrier or a
        system-policy capability, so that the check fails closed: trusted code that uses the app on purpose in a
        capability called by sandboxed code passes @ref False.  Consumers that serve a sandboxed caller from a
        thread that no sandbox governs (for example a server thread that runs a request for a tenant) pass
        @ref True.  A consumer decides this value from what it knows about the caller, and never forwards a value
        that the caller supplied.  Qore::SandboxManager::isRestricted() is used because it is not restricted, so the
        module can be loaded first by a Program restricted with
        @ref Qore::PO_NO_UNCONTROLLED_APIS "PO_NO_UNCONTROLLED_APIS"

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and the app does I/O that the sandbox does not
        control and is not declared sandbox-aware

        @note A Program that is only restricted with parse options (ex:
        @ref Qore::PO_NO_UNCONTROLLED_APIS "PO_NO_UNCONTROLLED_APIS", @ref Qore::PO_NO_NETWORK "PO_NO_NETWORK" or
        @ref Qore::PO_NO_FILESYSTEM "PO_NO_FILESYSTEM") and has no @ref Qore::SandboxManager "SandboxManager" is not
        sandboxed for this rule; such a Program cannot use the \c jni module's Java classes itself

        @see isSandboxAllowed()

        @since %DataProvider 3.12
    */
    static checkSandboxAccess(hash<DataProviderAppInfo> app, *bool sandboxed) {
        if (DataProviderActionCatalog::isSandboxAllowed(app)) {
            return;
        }
        if (!(sandboxed ?? SandboxManager::isRestricted())) {
            return;
        }
        throw "APP-SANDBOX-ACCESS-DENIED", sprintf("application %y (%s) does network or filesystem I/O that the "
            "sandbox does not control and is not declared sandbox-aware, so sandboxed code cannot use it",
            app.name, app.display_name ?? app.name);
    }

    #! Refuses a sandboxed caller the use of the registered app with the given name, as checkSandboxAccess() does
    /** The decision is made from the app's registered info: the catalog's, or the provider index's if the app is
        not loaded; nothing is initialized or loaded.  A sandboxed caller is refused an app that is registered
        nowhere, as whether it may use it cannot be decided.

        @param app the app's name
        @param sandboxed whether the caller is sandboxed; see checkSandboxAccess(hash<DataProviderAppInfo>, *bool)

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and the app does I/O that the sandbox does not
        control and is not declared sandbox-aware, or it is not registered

        @par Example:
        @code{.py}
# a tenant's request: its caller is sandboxed, whatever thread serves it
DataProviderActionCatalog::checkSandboxAccess(app_name, True);
        @endcode

        @since %DataProvider 3.12
    */
    static checkSandboxAccess(string app, *bool sandboxed) {
        if (*hash<DataProviderAppInfo> info = DataProviderActionCatalog::getRegisteredAppInfo(app)) {
            DataProviderActionCatalog::checkSandboxAccess(info, sandboxed);
            return;
        }
        if (!(sandboxed ?? SandboxManager::isRestricted())) {
            return;
        }
        throw "APP-SANDBOX-ACCESS-DENIED", sprintf("application %y is not registered, so whether sandboxed code may "
            "use it cannot be decided", app);
    }

    #! Refuses a sandboxed caller the factory of a module that registers an app with uncontrolled external I/O
    /** The factory belongs to the module that defines its class; if that module registers an app with
        @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware", the factory reaches the same I/O, so
        a sandboxed caller is refused it.  The declarations are made per app: a factory of a module that registers no
        app, or only apps without uncontrolled external I/O, is not refused.

        @param factory_name the name of the factory, for the error
        @param module_name the name of the module that defines the factory's class, if known
        @param sandboxed whether the caller is sandboxed; see checkSandboxAccess(hash<DataProviderAppInfo>, *bool)

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and the factory's module registers an app with
        external I/O that is not sandbox-aware

        @since %DataProvider 3.12
    */
    static checkFactorySandboxAccess(string factory_name, *string module_name, *bool sandboxed) {
        DataProviderActionCatalog::checkModuleSandboxAccess("data provider factory", factory_name, module_name,
            sandboxed);
    }

    #! Refuses a sandboxed caller an object of a module that registers an app with uncontrolled external I/O
    /** An object of a module that registers an app with
        @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
        @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware", such as its connections and factories,
        reaches the same I/O, so a sandboxed caller is refused it.  DataProviderSandboxCheck makes this check for
        the modules of an object's class hierarchy.

        @param what what the object is, for the error (ex: \c "connection")
        @param name the name of the object, for the error
        @param module_name the name of the module that defines the object's class, if known
        @param sandboxed whether the caller is sandboxed; see checkSandboxAccess(hash<DataProviderAppInfo>, *bool)

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and the module registers an app with external I/O
        that is not sandbox-aware

        @since %DataProvider 3.12
    */
    static checkModuleSandboxAccess(string what, string name, *string module_name, *bool sandboxed) {
        # the common path: one lookup, no lock
        if (!module_name || !exists external_io_module_apps{module_name}) {
            return;
        }
        if (!(sandboxed ?? SandboxManager::isRestricted())) {
            return;
        }
        throw "APP-SANDBOX-ACCESS-DENIED", sprintf("%s %y belongs to module %y, which registers application(s) %y "
            "that do network or filesystem I/O that the sandbox does not control and are not declared sandbox-aware, "
            "so sandboxed code cannot use it", what, name, module_name,
            sort(keys external_io_module_apps{module_name}));
    }

    #! Refuses a sandboxed caller the provider of an action, as getDataProviderForAction() describes
    /** The caller passes the action, so its app does not decide alone: the class that is created must be the one
        that the catalog registered for the action, or its own class hierarchy decides (see DataProviderSandboxCheck),
        so that an action hash naming an allowed app with the class of another app is refused like that app.

        @throw APP-SANDBOX-ACCESS-DENIED the app or the class reaches I/O that the sandbox does not control
    */
    private:internal static checkActionSandboxAccess(hash<DataProviderActionInfo> action) {
        if (!app_sandbox_state{action.app}) {
            DataProviderActionCatalog::checkSandboxAccess(action.app, True);
        }
        if (!exists action.cls) {
            return;
        }
        *hash<DataProviderActionInfo> registered = app_actionmap{action.app}{action.action};
        if (registered && registered.cls == action.cls && registered.cls_mod == action.cls_mod) {
            return;
        }
        # a class that the catalog did not register for the action
        DataProviderSandboxCheck check();
        check.check("data provider class", action.cls, (Class::forName(action.cls),), True);
    }

    #! Records an app with uncontrolled external I/O under the module that registers it; the catalog lock is held
    private:internal static addExternalIoModuleApp(hash<DataProviderAppInfo> app, string module_path) {
        if (!DataProviderActionCatalog::isSandboxAllowed(app)) {
            string module_name = DataProviderActionCatalog::moduleNameFromPath(module_path);
            external_io_module_apps{module_name}{app.name} = True;
            DataProviderSandboxState::setModuleUncontrolled(module_name, True);
        }
    }

    #! Removes an app from @ref external_io_module_apps; the catalog lock is held, and appmodmap is not changed yet
    private:internal static removeExternalIoModuleApp(string app) {
        foreach string path in (keys appmodmap{app}) {
            string module_name = DataProviderActionCatalog::moduleNameFromPath(path);
            if (exists external_io_module_apps{module_name}{app}) {
                remove external_io_module_apps{module_name}{app};
                if (!external_io_module_apps{module_name}) {
                    remove external_io_module_apps{module_name};
                    DataProviderSandboxState::setModuleUncontrolled(module_name, False);
                }
            }
        }
    }

    #! Returns the name of the module of a module path, as modulePathsContain() matches it
    private:internal static string moduleNameFromPath(string path) {
        string name = basename(path);
        foreach string suffix in ((".qmod", ".qm")) {
            if (name.size() > suffix.size() && name.substr(-suffix.size()) == suffix) {
                return name.substr(0, name.size() - suffix.size());
            }
        }
        return name;
    }

    #! Returns the registered info of an app without initializing anything: the catalog's, else the index's
    private:internal static *hash<DataProviderAppInfo> getRegisteredAppInfo(string app) {
        if (*hash<DataProviderAppInfo> info = appcache{app}) {
            return info;
        }
        if (*hash<auto> indexed = ProviderIndexUtil::tryGetAppInfo(app)) {
            return cast<hash<DataProviderAppInfo>>(indexed);
        }
    }

    #! Searches for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All applications that match the regular expression pattern in the \c name or \c desc fields
        @note application getters do not resolve the deferred output types of actions; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderAppInfo>> searchAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        # initialize matching pending apps, and the pending and lazy actions of matching apps to ensure that their
        # supports_* flags are set
        DataProviderActionCatalog::initMatchingApps(bool sub (string name, *string desc) {
            return regex(name, regex_pattern, regex_options) || (exists desc && regex(desc, regex_pattern,
                regex_options));
        });
        return map $1, appcache.values(),
            regex($1.name, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for applications whose names or descriptions match a substring
    /** @param substr A substring to match in the \c name and \c desc fields; pending apps only are matched with the
        name
        @param ignore_case Ignore case when searching

        @return All applications that match the substring argument in the \c name or \c desc fields
        @note application getters do not resolve the deferred output types of actions; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderAppInfo>> searchAppSubstr(string substr, bool ignore_case = True) {
        if (ignore_case) {
            substr = substr.upr();

            # initialize matching pending apps, and the pending and lazy actions of matching apps to ensure that
            # their supports_* flags are set
            DataProviderActionCatalog::initMatchingApps(bool sub (string name, *string desc) {
                return (name.upr().find(substr) >= 0) || (exists desc && desc.upr().find(substr) >= 0);
            });

            return map $1, appcache.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }

        # case-sensitive path: initialize matching pending apps, and the pending and lazy actions of matching apps
        DataProviderActionCatalog::initMatchingApps(bool sub (string name, *string desc) {
            return (name.find(substr) >= 0) || (exists desc && desc.find(substr) >= 0);
        });

        return map $1, appcache.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Returns all actions for a particular connection scheme
    /** @param scheme the connection scheme

        @return a list of actions associated with the given scheme
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> getActionsForScheme(string scheme) {
        # the lazy actions of the applications with this scheme are part of their registered actions
        foreach string app in (DataProviderActionCatalog::getInitApps(lazyactionmap)) {
            if (appcache{app}.scheme == scheme) {
                DataProviderActionCatalog::initPending(app, InitLazyActions);
            }
        }
        # the actions returned are those of the applications with this scheme
        if (*list<string> apps = (map $1.name, appcache.values(), $1.scheme == scheme
                && exists resolve_output_types{$1.name})) {
            DataProviderActionCatalog::resolveOutputTypes(apps, NOTHING, True);
        }
        return smap{scheme};
    }

    #! Returns a hash of all actions for the given application
    /** @param app the data provider application

        @return a hash of actions keyed by action name for the given application

        @see getActionHashEx()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *hash<string, hash<DataProviderActionInfo>> getActionHash(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        return app_actionmap{app};
    }

    #! Returns a hash of all actions for the given application
    /** @param app the data provider application

        @return a hash of actions keyed by action name for the given application

        @throw APP-ERROR the given application is not registered

        @see getActionHash()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *hash<string, hash<DataProviderActionInfo>> getActionHashEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        if (*hash<string, hash<DataProviderActionInfo>> rv = app_actionmap{app}) {
            return rv;
        }
        throw "APP-ERROR", sprintf("application %y is not registered", app);
    }

    #! Populates the provider index app cache from all currently-registered applications
    /** This makes all registered apps and their actions visible through the \c ProviderIndexUtil cache APIs
        without reading the data provider index from the filesystem

        Use this to recover a complete app catalog when the data provider index cannot be read (ex: a permissions
        problem or a corrupt index file); note that all provider modules must be loaded first (ex: with
        @ref DataProvider::registerKnownFactories()), otherwise only apps registered by already-loaded modules are
        cached

        @return the number of applications added to the cache
    */
    static int cacheAllRegisteredApps() {
        int count = 0;
        foreach hash<DataProviderAppInfo> app in (DataProviderActionCatalog::getAllApps()) {
            try {
                ProviderIndexUtil::addAppToCache(app);
                ++count;
            } catch (hash<ExceptionInfo> ex) {
                ProviderIndexUtil::log(LoggerLevel::LevelError, "Cannot cache data provider app %y: %s: %s",
                    app.name, ex.err, ex.desc);
                continue;
            }
            *list<hash<DataProviderActionInfo>> actions;
            try {
                actions = DataProviderActionCatalog::getActions(app.name, False);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "ACTION-OUTPUT-TYPE-ERROR") {
                    rethrow;
                }
                # the index cache holds no output types, so an action whose output type cannot be built is
                # cached all the same; the failure is retained by the catalog
                ProviderIndexUtil::log(LoggerLevel::LevelError, "Data provider app %y: %s: %s", app.name, ex.err,
                    ex.desc);
                actions = DataProviderActionCatalog::getActionsNoInit(app.name);
            }
            foreach hash<DataProviderActionInfo> action in (actions) {
                try {
                    ProviderIndexUtil::addActionToCache(action);
                } catch (hash<ExceptionInfo> ex) {
                    ProviderIndexUtil::log(LoggerLevel::LevelError, "Cannot cache action %y for data provider app "
                        "%y: %s: %s", action.action, app.name, ex.err, ex.desc);
                }
            }
        }
        return count;
    }

    #! Returns all actions for the given application
    /** @param app the data provider application
        @param check_index if \c True, check the provider index for an on-demand module before reading registered
            actions

        @return a list of actions for the given application

        @see getActionsEx()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> getActions(string app, bool check_index = True) {
        # load app on demand
        if (check_index) {
            ProviderIndexUtil::checkAppModule(app);
        }

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        return app_actionmap{app}.values();
    }

    #! Returns the currently registered actions for one application without loading or initializing provider modules
    /** This accessor exposes only actions already present in the in-process catalog. Pending lazy registrations
        (see registerPendingApp() and registerPendingAppAction()) are intentionally omitted so callers can overlay
        current authoritative metadata on an approximate provider index without paying the cost of loading modules,
        resolving output types, or retaining provider schemas.

        @param app the exact data-provider application identifier
        @return currently registered actions, or @ref nothing when none are registered

        @note no deferred registration is run or waited for: for an application whose lazy registrations have not run
        (see hasLazyAppActions()), only the actions registered so far are returned; readers that need all of its
        actions without using it read them from the provider index; see @ref dataprovider_lazy_actions

        @since %DataProvider 3.7
    */
    static *list<hash<DataProviderActionInfo>> getActionsNoInit(string app) {
        return app_actionmap{app}.values();
    }

    #! Returns all actions for the given application; throws an exception if the app is not registered
    /** @param app the data provider application

        @return a list of actions for the given application

        @throw APP-ERROR the given application is not registered

        @see getActions()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static list<hash<DataProviderActionInfo>> getActionsEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        if (*list<hash<DataProviderActionInfo>> rv = app_actionmap{app}.values()) {
            return rv;
        }
        throw "APP-ERROR", sprintf("application %y is not registered", app);
    }

    #! Returns all actions in the cache
    /** @return all actions in the cache
        @note an output type whose callback failed is not attempted again, and its action is returned with
        \c get_output_type and no \c output_type; see @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> getAllActions() {
        # initialize all pending apps, all pending actions and all lazy actions
        DataProviderActionCatalog::initAllPending();
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return top_actionmap.values();
    }

    #! Returns the action definition for the given application and action
    /** @param app the app name
        @param action the action name

        @return the action definition for the given application name and action or @ref nothing if not present

        @see getAppActionEx()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of the action cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *hash<DataProviderActionInfo> getAppAction(string app, string action) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the deferred initialization of the action: a pending app, the pending action and the app's lazy
        # actions
        DataProviderActionCatalog::initAppAction(app, action);
        if (exists resolve_output_types{app}{action}) {
            DataProviderActionCatalog::resolveOutputTypes(app, action, True);
        }
        return app_actionmap{app}{action};
    }

    #! Returns the application definition for the given application name
    /** @param app the app name
        @param action the action name

        @return the action definition for the given application name and action; throws an exception if the app is not
        registered

        @throw APP-ERROR the given application is not registered
        @throw ACTION-ERROR the given action is not registered

        @see getAppAction()
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of the action cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static hash<DataProviderActionInfo> getAppActionEx(string app, string action) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the deferred initialization of the action: a pending app, the pending action and the app's lazy
        # actions
        DataProviderActionCatalog::initAppAction(app, action);
        if (exists resolve_output_types{app}{action}) {
            DataProviderActionCatalog::resolveOutputTypes(app, action, True);
        }
        *hash<string, hash<DataProviderActionInfo>> actionmap = app_actionmap{app};
        if (!actionmap) {
            throw "APP-ACTION-ERROR", sprintf("application %y is not registered", app);
        }
        if (*hash<DataProviderActionInfo> rv = actionmap{action}) {
            return rv;
        }
        throw "ACTION-ERROR", sprintf("application %y has no action %y (known actions: %y)", app, action,
            keys actionmap);
    }

    #! Registers the single-file read/write mapping for an application
    /** @param app the application name; the application does not have to be registered yet
        @param info the mapping from this application's actions onto file read and write operations

        Lets generic consumers read and write a file through the application without knowing its action and
        option names; see @ref DataProviderFileApiInfo.

        The registration is undone automatically if the module registering it fails to load.

        @throw FILE-API-ERROR a file API has already been registered for the application; \c read_mode or
        \c write_data_format is invalid; \c write_action is set without \c write_path_option or
        \c write_data_option

        @since %DataProvider 3.6

        @see getAppFileApi()
    */
    static registerAppFileApi(string app, hash<DataProviderFileApiInfo> info) {
        # sandboxed code would attach a file API to an app that it does not own
        DataProviderSandboxState::checkRegistration("file API", app);
        if (info.read_mode != DPFR_DATA && info.read_mode != DPFR_URL) {
            throw "FILE-API-ERROR", sprintf("application %y: read_mode %y is invalid; expecting %y or %y", app,
                info.read_mode, DPFR_DATA, DPFR_URL);
        }
        if (info.read_data_encoding != "base64" && info.read_data_encoding != "raw") {
            throw "FILE-API-ERROR", sprintf("application %y: read_data_encoding %y is invalid; expecting \"base64\" "
                "or \"raw\"", app, info.read_data_encoding);
        }
        if (info.write_action.val()) {
            if (!info.write_path_option.val()) {
                throw "FILE-API-ERROR", sprintf("application %y: write_action %y is set without a "
                    "write_path_option", app, info.write_action);
            }
            if (!info.write_data_option.val()) {
                throw "FILE-API-ERROR", sprintf("application %y: write_action %y is set without a "
                    "write_data_option", app, info.write_action);
            }
            if (!DataProviderActionCatalog::ValidWriteFormats{info.write_data_format}) {
                throw "FILE-API-ERROR", sprintf("application %y: write_data_format %y is invalid; expecting one "
                    "of: %y", app, info.write_data_format, keys DataProviderActionCatalog::ValidWriteFormats);
            }
            if (info.write_path_is_parent && info.write_data_format != DPFW_FILE) {
                throw "FILE-API-ERROR", sprintf("application %y: write_path_is_parent requires write_data_format "
                    "%y, as no other format carries a filename; got %y", app, DPFW_FILE, info.write_data_format);
            }
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        if (app_fileapimap{app}) {
            throw "FILE-API-ERROR", sprintf("a file API has already been registered for application %y", app);
        }
        app_fileapimap{app} = info;
        ImplicitModuleTransaction::add(sub () { DataProviderActionCatalog::deregisterAppFileApi(app); });
    }

    #! Returns the single-file read/write mapping for an application, if any
    /** @param app the application name

        @return the mapping for the application, or @ref nothing if the application has not registered one

        @since %DataProvider 3.6

        @see registerAppFileApi()
    */
    static *hash<DataProviderFileApiInfo> getAppFileApi(string app) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        return app_fileapimap{app};
    }

    #! Removes the file API registration for an application
    /** @param app the application name

        @return the mapping that was removed, if any

        @since %DataProvider 3.6
    */
    static *hash<DataProviderFileApiInfo> deregisterAppFileApi(string app) {
        DataProviderSandboxState::checkRegistration("file API", app, True);
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        return remove app_fileapimap{app};
    }

    #! Returns the names of all applications that have registered a file API
    /** @return the names of all applications that have registered a file API

        @since %DataProvider 3.6
    */
    static *list<string> getAppFileApiNames() {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }

        return keys app_fileapimap;
    }

    #! Returns a data provider option for the given action and options
    /** @param action the action to use to return the data provider
        @param options a reference to options where constructor options will be taken from

        @return a data provider option for the given action and options
    */
    static AbstractDataProvider getDataProviderForAction(hash<DataProviderActionInfo> action,
            *reference<hash<auto>> options) {
        # load the module into this program's context so Class::forName() can resolve the class;
        # load_module() in ProviderIndexUtil loads into ProviderIndexUtil's program context,
        # but Class::forName() resolves in the calling program's context
        if (action.cls_mod) {
            load_module(action.cls_mod);
        }
        # the provider reaches the app's I/O: a sandboxed caller is refused an app whose external I/O its sandbox does
        # not control (see @ref dataprovider_external_io); checked after the module is loaded, which can register the
        # app; an app that is not registered is refused to sandboxed callers too
        if (SandboxManager::isRestricted()) {
            DataProviderActionCatalog::checkActionSandboxAccess(action);
        }
        hash<auto> constructor_opts;
        map constructor_opts{$1.key} = remove options{$1.key}, action.options.pairIterator(),
            $1.value.loc == "constructor" && options.hasKey($1.key);
        return Class::forName(action.cls).newObject(constructor_opts);
    }

    #! Searches for application actions whose descriptions match a regular expression pattern
    /** @param app The application to search for actions in
        @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All actions that match the regular expression pattern in the \c short_desc or \c desc fields

        @note If the application does not exist, no results are returned
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionRegex(string app, string regex_pattern,
            int regex_options = RE_Caseless) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        return map $1, app_actionmap{app}.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for application actions whose descriptions match a substring
    /** @param app The application to search for actions in
        @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All actions that match the substring argument in the \c short_desc or \c desc fields

        @note If the application does not exist, no results are returned
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionSubstr(string app, string substr,
            bool ignore_case = True) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        if (ignore_case) {
            substr = substr.upr();
            return map $1, app_actionmap{app}.values(),
                ($1.short_desc.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, app_actionmap{app}.values(), ($1.short_desc.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Searches for application actions whose descriptions match a regular expression pattern
    /** @param app The application to search for actions in
        @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All actions that match the regular expression pattern in the \c short_desc or \c desc fields

        @throw APP-ERROR the given application is not registered

        @note If the application does not exist, an exception is thrown
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionRegexEx(string app, string regex_pattern,
            int regex_options = RE_Caseless) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        *hash<string, hash<DataProviderActionInfo>> appmap = app_actionmap{app};
        if (!appmap) {
            throw "APP-ERROR", sprintf("application %y is not registered", app);
        }
        return map $1, appmap.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for application actions whose descriptions match a substring
    /** @param app The application to search for actions in
        @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All actions that match the substring argument in the \c short_desc or \c desc fields

        @throw APP-ERROR the given application is not registered

        @note If the application does not exist, an exception is thrown
        @throw ACTION-OUTPUT-TYPE-ERROR the output type of an action returned cannot be built; see
        @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionSubstrEx(string app, string substr,
            bool ignore_case = True) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # complete the app's deferred initialization: a pending app, its pending actions and its lazy actions
        DataProviderActionCatalog::initApp(app);
        if (exists resolve_output_types{app}) {
            DataProviderActionCatalog::resolveOutputTypes(app, NOTHING, True);
        }
        *hash<string, hash<DataProviderActionInfo>> appmap = app_actionmap{app};
        if (!appmap) {
            throw "APP-ERROR", sprintf("application %y is not registered", app);
        }
        if (ignore_case) {
            substr = substr.upr();
            return map $1, appmap.values(),
                ($1.short_desc.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, appmap.values(), ($1.short_desc.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Searches for actions whose descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All actions that match the regular expression pattern in the \c short_desc or \c desc fields
        @note an output type whose callback failed is not attempted again, and its action is returned with
        \c get_output_type and no \c output_type; see @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchActionRegex(string regex_pattern,
            int regex_options = RE_Caseless) {
        # lazy actions are part of the registered actions searched
        DataProviderActionCatalog::initAllLazyActions();
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return map $1, top_actionmap.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for actions whose descriptions match a substring
    /** @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All actions that match the substring argument in the \c short_desc or \c desc fields
        @note an output type whose callback failed is not attempted again, and its action is returned with
        \c get_output_type and no \c output_type; see @ref dataprovider_deferred_output_types
    */
    static *list<hash<DataProviderActionInfo>> searchActionSubstr(string substr, bool ignore_case = True) {
        # lazy actions are part of the registered actions searched
        DataProviderActionCatalog::initAllLazyActions();
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        if (ignore_case) {
            substr = substr.upr();
            return map $1, top_actionmap.values(),
                ($1.short_desc.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, top_actionmap.values(), ($1.short_desc.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Converts field definitions into option descriptions
    static hash<string, hash<ActionOptionInfo>> getActionOptionFromFields(hash<auto> fields,
            *hash<auto> additional_fields) {
        hash<string, hash<ActionOptionInfo>> rv = {};
        hash plain_fields = hash(fields);
        foreach hash<auto> i in (plain_fields.pairIterator()) {
            on_error rethrow $1.err, sprintf("%s (while processing field %y)", $1.desc, i.key);
            *hash<auto> raw_info;
            if (!(i.value instanceof AbstractDataField) && i.value.typeCode() == NT_HASH
                    && i.value.type.typeCode() != NT_LIST) {
                raw_info = i.value;
                if (!exists i.value.name) {
                    i.value.name = i.key;
                }
                i.value = new QoreDataField(i.value);
            }
            if (i.value instanceof AbstractDataField) {
                hash<DataFieldInfo> info = i.value.getLocalInfo();
                i.value = info{"display_name", "short_desc", "desc", "default_value", "example_value",
                    "remote_default_value", "allowed_values", "allowed_values_creatable", "element_allowed_values",
                    "element_allowed_values_creatable"} + {
                    "type": i.value.getType(),
                };
                # Raw field definitions only expose explicitly configured example values. QoreDataField
                # synthesizes examples from the field type, and temporal types use the current time; retaining
                # those generated values makes otherwise identical action definitions compare unequal when they
                # are registered more than once.
                if (raw_info && !exists raw_info.example_value) {
                    remove i.value.example_value;
                }
                # remove missing attributes
                map remove i.value{$1.key}, i.value.pairIterator(), !exists $1.value;
                if (info.attr.ref_data.val()) {
                    i.value.ref_data = info.attr.ref_data;
                }
                if (info.attr.element_ref_data.val()) {
                    i.value.element_ref_data = info.attr.element_ref_data;
                }
                if (info.attr.required) {
                    i.value.required = True;
                }
                if (info.attr.preselected) {
                    i.value.preselected = True;
                }
                if (info.attr.sensitive) {
                    i.value.sensitive = True;
                }
                if (info.attr.disabled) {
                    i.value.disabled = True;
                }
                if (info.attr.readonly) {
                    i.value.readonly = True;
                }
                if (info.attr.required_groups) {
                    i.value.required_groups = info.attr.required_groups;
                }
                if (info.attr.exclusive_with) {
                    i.value.exclusive_with = info.attr.exclusive_with;
                }
                if (info.attr.validation_regex.val()) {
                    i.value.validation_regex = info.attr.validation_regex;
                }
                if (info.attr.validation_regex_desc.val()) {
                    i.value.validation_regex_desc = info.attr.validation_regex_desc;
                }
                if (info.attr.ui_type.val()) {
                    i.value.ui_type = info.attr.ui_type;
                }
                if (info.attr.depends_on) {
                    i.value.depends_on = info.attr.depends_on;
                }
                if (exists info.attr.on_change) {
                    i.value.on_change = info.attr.on_change;
                }
                if (info.attr.has_dependents) {
                    i.value.has_dependents = info.attr.has_dependents;
                }
                if (info.attr.metadata) {
                    i.value.metadata = info.attr.metadata;
                }
                if (info.attr.messages) {
                    i.value.messages = info.attr.messages;
                }
                if (info.attr.icon) {
                    i.value.icon = info.attr.icon;
                }
                if (info.attr.image) {
                    i.value.image = info.attr.image;
                }
                if (info.attr.hasKey("supports_custom_values")) {
                    i.value.supports_custom_values = info.attr.supports_custom_values;
                }
                if (info.attr.hasKey("supports_templates")) {
                    i.value.supports_templates = info.attr.supports_templates;
                }
                if (info.attr.hasKey("supports_expressions")) {
                    i.value.supports_expressions = info.attr.supports_expressions;
                }
                if (info.attr.hasKey("server_expression_handling")) {
                    i.value.server_expression_handling = info.attr.server_expression_handling;
                }
                if (info.attr.option_type_name) {
                    i.value.option_type_name = info.attr.option_type_name;
                }
                if (info.attr.input_field_names) {
                    i.value.input_field_names = True;
                }
                if (raw_info) {
                    i.value += raw_info{"required", "sensitive", "ref_data", "element_ref_data",
                        "default_ref_data", "preselected", "required_groups", "exclusive_with",
                        "validation_regex", "validation_regex_desc", "depends_on", "on_change", "metadata",
                        "messages",
                        "icon", "image", "disabled", "readonly", "has_dependents", "supports_custom_values",
                        "supports_templates", "supports_expressions", "server_expression_handling",
                        "option_type_name", "ui_type", "input_field_names"};
                }
            }
            on_error rethrow $1.err, sprintf("%s (field: %y)", $1.desc, i.value);
            hash<ActionOptionInfo> info = <ActionOptionInfo>{
                "display_name": i.value.display_name ?? AbstractDataField::getDisplayName(i.key),
                "short_desc": i.value.short_desc ??
                    AbstractDataField::getShortDescription(i.value.desc ?? DefaultDescription),
                "desc": i.value.desc ?? DefaultDescription,
            };
            auto type = i.value.type;
            if (type.typeCode() == NT_LIST) {
                type = type[0];
            }
            switch (type.typeCode()) {
                case NT_OBJECT:
                    if (type instanceof AbstractDataProviderType) {
                        info.type = type;
                    } else {
                        info.type = AbstractDataProviderType::get(type, NOTHING, i.value.option_type_name ? {
                            "qore.external_name": i.value.option_type_name,
                        } : NOTHING);
                    }
                    break;

                case NT_STRING:
                    *AbstractDataProviderType datatype = AbstractDataProviderTypeMap{type};
                    if (!datatype) {
                        throw "OPTION-ERROR", sprintf("Cannot set option type from unknown type %y", type);
                    }
                    info.type = datatype;
                    break;

                default:
                    throw "OPTION-ERROR", sprintf("Cannot set option type for option %y from \"type\" option with "
                        "type %y", i.key, type);
            }
            if (!exists i.value.required && info.type.isMandatory()) {
                i.value.required = True;
            }
            if (i.value instanceof hash<DataProviderOptionInfo>) {
                info += i.value{"default_value", "required", "sensitive", "example_value", "remote_default_value",
                    "ref_data",
                    "element_ref_data", "preselected", "required_groups", "exclusive_with",
                    "depends_on", "has_dependents", "on_change", "metadata", "messages", "icon", "image",
                    "server_expression_handling", "ui_type", "input_field_names"}
                    + additional_fields;
            } else {
                info += i.value{"default_value", "required", "sensitive", "example_value", "remote_default_value",
                    "ref_data",
                    "element_ref_data", "preselected", "required_groups", "exclusive_with",
                    "validation_regex", "validation_regex_desc", "depends_on", "has_dependents", "on_change",
                    "metadata", "icon",
                    "image", "supports_templates", "supports_custom_values", "server_expression_handling",
                    "ui_type", "input_field_names"} +
                    additional_fields;
                if (i.value.option_type_name
                    && info.type.getTag("qore.external_name") != i.value.option_type_name) {
                    info.type = info.type.copyWithTags(info.type.getTags() + {
                        "qore.external_name": i.value.option_type_name,
                    });
                }
            }
            # an option without a value for supports_expressions keeps the default (True)
            if (exists i.value.supports_expressions && !additional_fields.hasKey("supports_expressions")) {
                info.supports_expressions = i.value.supports_expressions;
            }
            if (info.required && !exists info.preselected) {
                info.preselected = True;
            }
            if (i.value.allowed_values) {
                if (i.value.allowed_values instanceof list<hash<AllowedValueInfo>>
                    || i.value.allowed_values instanceof hash<AllowedValueInfo>) {
                    info.allowed_values = i.value.allowed_values;
                } else if (i.value.allowed_values.typeCode() == NT_LIST
                    && i.value.allowed_values.size()
                    && i.value.allowed_values[0].typeCode() == NT_HASH
                    && i.value.allowed_values[0].hasKey("value")) {
                    info.allowed_values =
                        map cast<hash<AllowedValueInfo>>($1), i.value.allowed_values;
                } else {
                    info.allowed_values = map <AllowedValueInfo>{
                        "value": $1,
                        "desc": DefaultDescription,
                    }, i.value.allowed_values;
                }
            }
            if (i.value.allowed_values_creatable) {
                info.allowed_values_creatable = True;
            }
            if (i.value.element_allowed_values) {
                if (i.value.element_allowed_values instanceof list<hash<AllowedValueInfo>>
                    || i.value.element_allowed_values instanceof hash<AllowedValueInfo>) {
                    info.element_allowed_values = i.value.element_allowed_values;
                } else if (i.value.element_allowed_values.typeCode() == NT_LIST
                    && i.value.element_allowed_values.size()
                    && i.value.element_allowed_values[0].typeCode() == NT_HASH
                    && i.value.element_allowed_values[0].hasKey("value")) {
                    info.element_allowed_values =
                        map cast<hash<AllowedValueInfo>>($1), i.value.element_allowed_values;
                } else {
                    info.element_allowed_values = map <AllowedValueInfo>{
                        "value": $1,
                        "desc": DefaultDescription,
                    }, i.value.element_allowed_values;
                }
            }
            if (i.value.element_allowed_values_creatable) {
                info.element_allowed_values_creatable = True;
            }
            rv{i.key} = info;
        }
        return rv;
    }

    #! Check that the action has had its options initialized if necessary and return the possibly updated action info
    static hash<DataProviderActionInfo> checkAppActionOptions(string app, string action) {
        hash<DataProviderActionInfo> info = getAppActionEx(app, action);
        if (*code c = remove info.get_dynamic_options) {
            c(\info);
            app_actionmap{app}{action} = info;
        }
        return info;
    }

    #! Returns a data type for the action options
    static *AbstractDataProviderType getTypeForActionOptions(hash<auto> action) {
        return getTypeForActionOptions(getAppActionEx(action.app, action.action));
    }

    #! Returns a data type for the action options
    /** @param action the action

        @return a hash type with a field for each option of the action, with the option's type, which is mandatory
        if the option is required, and its description, default value and allowed values; other keys are accepted
        as they are; no value is returned if the action has no options

        @since DataProvider 3.11 the type has a field for each option; it had no fields
    */
    static *AbstractDataProviderType getTypeForActionOptions(hash<DataProviderActionInfo> action) {
        if (!action.options) {
            return;
        }
        hash<string, AbstractDataField> fields;
        bool mand;
        foreach hash<auto> i in (action.options.pairIterator()) {
            if (!mand && i.value.type.isMandatory()) {
                mand = True;
            }
            hash<auto> fh = i.value{"display_name", "short_desc", "desc", "allowed_values",
                "allowed_values_creatable", "element_allowed_values", "element_allowed_values_creatable",
                "default_value", "example_value", "remote_default_value"} + {
                "name": i.key,
                "type": i.value.required ? i.value.type.getMandatoryType() : i.value.type.getOrNothingType(),
            };
            # set option attributes not directly supported in fields as free-form field attributes
            if (i.value.preselected) {
                fh.attr.preselected = True;
            }
            if (i.value.sensitive) {
                fh.attr.sensitive = True;
            }
            if (i.value.readonly) {
                fh.attr.readonly = True;
            }
            if (i.value.disabled) {
                fh.attr.disabled = True;
            }
            if ((*string str = i.value.ref_data).val()) {
                fh.attr.ref_data = str;
            }
            if ((*string str = i.value.element_ref_data).val()) {
                fh.attr.element_ref_data = str;
            }
            if ((*string str = i.value.default_ref_data).val()) {
                fh.attr.default_ref_data = str;
            }
            if ((*string str = i.value.dynamic_type).val()) {
                fh.attr.dynamic_type = str;
            }
            if (*list<string> l = i.value.required_groups) {
                fh.attr.required_groups = l;
            }
            if (*list<string> l = i.value.exclusive_with) {
                fh.attr.exclusive_with = l;
            }
            if (*list<string> l = i.value.depends_on) {
                fh.attr.depends_on = l;
            }
            if ((*string str = i.value.validation_regex).val()) {
                fh.attr.validation_regex = str;
            }
            if ((*string str = i.value.validation_regex_desc).val()) {
                fh.attr.validation_regex_desc = str;
            }
            if (*list<string> l = i.value.on_change) {
                fh.attr.on_change = l;
            }
            fields{i.key} = new QoreDataField(fh);
        }
        HashDataType rv(mand ? AutoHashType : AutoHashOrNothingType, action.action);
        # the type stays open: keys that are not options are accepted as they are, as by the type without fields;
        # this is set before the fields are added, as adding a field otherwise refuses other keys
        rv.setDefaultOtherFieldType(AbstractDataProviderType::get(AbstractDataProviderType::anyType));
        map rv.addField($1), fields.iterator();
        return rv;
    }

    #! Returns a normalized path to ensure that comparisons always work
    private:internal static string normalizePath(string path) {
        # ensure that "/" characters are never duplicated
        if (path =~ /\/\//) {
            throw "ACTION-ERROR", sprintf("%y: illegal consecutive \"/\" characters", path);
        }
        # remove any leading or trailing "/" chars
        path =~ s/^\/?(.*[^\/])\/?/$1/;
        if (!path.val()) {
            throw "ACTION-ERROR", sprintf("empty action path provided", path);
        }
        # ensure path starts with a leading "/"
        return path == "/" ? path : "/" + path;
    }

    #! Resolves the deferred output types of the actions in scope
    /** @param scope_apps the applications whose actions are resolved; every application when omitted
        @param scope_action the one action of the application in @p scope_apps that is resolved; every action of
        the applications when omitted
        @param scoped @ref True if the caller returns exactly the actions in scope: an output type whose callback
        failed before is attempted again, and a failure throws an \c ACTION-OUTPUT-TYPE-ERROR exception once every
        action in scope has been attempted; @ref False for a getter returning every action, which runs only
        callbacks that have not run yet and records their failures without throwing

        A failure is retained like a failed deferred action initialization: it is returned by
        @ref getInitializationFailures() and reported by a discovery generation, until the output type is resolved
        or the action is registered again.  A failure that repeats the retained one does not change the catalog
        revision, so retrying it does not invalidate a discovery snapshot.

        @throw ACTION-OUTPUT-TYPE-ERROR (only if @p scoped is @ref True) the output type of an action in scope cannot
        be built; the exception argument is the list of the failures (hash<DataProviderInitializationFailureInfo>)
    */
    private:internal static resolveOutputTypes(*softlist<string> scope_apps, *string scope_action, bool scoped = False) {
        hash<string, bool> app_filter = scope_apps ? map {$1: True}, scope_apps.iterator() : {};
        # identities attempted by this call, so a callback that registers further actions is attempted once
        hash<string, bool> attempted = {};
        list<hash<DataProviderInitializationFailureInfo>> failures = ();
        while (True) {
            list<hash<auto>> pending = ();
            {
                DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
                foreach hash<auto> i in (resolve_output_types.pairIterator()) {
                    if (scope_apps && !app_filter{i.key}) {
                        continue;
                    }
                    foreach hash<auto> j in (i.value.pairIterator()) {
                        if (scope_action && j.key != scope_action) {
                            continue;
                        }
                        # a callback that failed is called again only for a caller that asked for its action
                        if (!j.value && !scoped) {
                            continue;
                        }
                        string key = i.key + "/" + j.key;
                        if (attempted{key}) {
                            continue;
                        }
                        attempted{key} = True;
                        *code get_output_type = app_actionmap{i.key}{j.key}.get_output_type;
                        if (!get_output_type || !top_actionmap{key}) {
                            continue;
                        }
                        pending += {
                            "app": i.key,
                            "action": j.key,
                            "get_output_type": get_output_type,
                        };
                    }
                }
            }
            if (!pending) {
                break;
            }

            # Output-type callbacks can lazily load provider modules whose initializers register apps or actions.
            # Never hold the catalog mutex while invoking them.  Two threads asking for the same action both call
            # its callback; the first result is published and the second finds the action resolved.
            list<hash<auto>> results = ();
            foreach hash<auto> item in (pending) {
                try {
                    item.output_type = item.get_output_type();
                } catch (hash<ExceptionInfo> ex) {
                    item.ex = ex;
                }
                results += item;
            }

            DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
            foreach hash<auto> item in (results) {
                if (item.ex) {
                    if (*hash<DataProviderInitializationFailureInfo> failure
                            = DataProviderActionCatalog::recordOutputTypeFailure(item.app, item.action,
                                item.get_output_type, item.ex)) {
                        failures += failure;
                    }
                } else {
                    DataProviderActionCatalog::publishOutputType(item.app, item.action, item.get_output_type,
                        item.output_type);
                }
            }
        }

        if (scoped && failures) {
            hash<DataProviderInitializationFailureInfo> failure = failures[0];
            throw "ACTION-OUTPUT-TYPE-ERROR", sprintf("the output type of action %y of application %y cannot be "
                "built: %s: %s%s", failure.action, failure.app, failure.error, failure.description,
                failures.size() > 1
                    ? sprintf(" (the output types of %d other action(s) cannot be built either: %y)",
                        failures.size() - 1, (map $1.app + "/" + $1.action, failures[1..]))
                    : ""), failures;
        }
    }

    #! Publishes a resolved output type; the catalog lock must be held
    /** Nothing is published into an action that was resolved, removed or replaced while its callback ran.
    */
    private:internal static publishOutputType(string app, string action, code get_output_type,
            *AbstractDataProviderType output_type) {
        string key = app + "/" + action;
        *hash<DataProviderActionInfo> top_action = top_actionmap{key};
        *hash<DataProviderActionInfo> app_action = app_actionmap{app}{action};
        if (!top_action || !app_action || top_action.get_output_type != get_output_type) {
            return;
        }
        top_action.output_type = output_type;
        app_action.output_type = output_type;
        remove top_action.get_output_type;
        remove app_action.get_output_type;
        top_actionmap{key} = top_action;
        app_actionmap{app}{action} = app_action;
        remove resolve_output_types{app}{action};
        if (!resolve_output_types{app}) {
            remove resolve_output_types{app};
        }
        resolved_output_type_callbacks{app}{action} = get_output_type;
        ++catalog_revision;
        if (*string scheme = appcache{app}.scheme) {
            *list<hash<DataProviderActionInfo>> actions = smap{scheme};
            if (actions) {
                for (int idx = 0; idx < actions.size(); ++idx) {
                    if (actions[idx].app == app && actions[idx].action == action) {
                        actions[idx] = app_action;
                        smap{scheme} = actions;
                        break;
                    }
                }
            }
        }
        # a failure retained from an earlier attempt no longer applies
        DataProviderActionCatalog::clearSuccessfulInitializationState(app, action);
    }

    #! Retains the failure of an output-type callback; the catalog lock must be held
    /** @return the failure retained, or @ref nothing if the action was resolved, removed or replaced while its
        callback ran, in which case the failure no longer describes the action
    */
    private:internal static *hash<DataProviderInitializationFailureInfo> recordOutputTypeFailure(string app, string action,
            code get_output_type, hash<ExceptionInfo> ex) {
        *hash<DataProviderActionInfo> app_action = app_actionmap{app}{action};
        if (!app_action || app_action.get_output_type != get_output_type) {
            return;
        }
        *list<string> owners = actionmodmap{app}{action} ? sort(keys actionmodmap{app}{action}) : NOTHING;
        hash<DataProviderInitializationFailureInfo> failure = <DataProviderInitializationFailureInfo>{
            "kind": "action",
            "app": app,
            "action": action,
            "error": ex.err,
            "description": ex.desc,
            "argument": ex.arg,
            "module_context": owners ? owners[0] : NOTHING,
        };
        resolve_output_types{app}{action} = False;
        # a retry that fails the same way changes nothing a discovery generation can observe
        if (initialization_failure_map{app}{action} != failure) {
            initialization_failure_map{app}{action} = failure;
            ++catalog_revision;
        }
        return failure;
    }

    #! Deregisters an app and all its actions if an exception occurs during the other module's initialization
    private:internal static *bool deregisterApp(string name) {
        DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());

        return deregisterAppIntern(name);
    }

    #! Deregisters an app and all its actions if an exception occurs during the other module's initialization
    private:internal static *bool deregisterAppIntern(string name) {
        bool had_state = exists appcache{name} || exists app_actionmap{name}
            || exists initactionmap{name} || exists lazyactionmap{name}
            || exists initialization_failure_map{name}
            || exists expected_initialization_map{name}
            || exists resolve_output_types{name} || exists appmodmap{name}
            || exists actionmodmap{name}
            || exists pending_generic_api_call_apps{name};
        *hash<DataProviderAppInfo> app = remove appcache{name};
        # capture the app's actions BEFORE dropping the map entry; the top_actionmap cleanup must
        # iterate the captured actions. Previously app_actionmap{name} was removed first and then
        # iterated (now empty) to clean top_actionmap, so every action - and the entire option /
        # data-type graph it holds - was leaked permanently in the static top_actionmap on each
        # deregistration (unbounded growth under app/connection churn)
        *hash<string, hash<DataProviderActionInfo>> actions = remove app_actionmap{name};
        remove initactionmap{name};
        remove lazyactionmap{name};
        remove lazy_action_regs{name};
        remove app_serials{name};
        remove initialization_failure_map{name};
        remove expected_initialization_map{name};
        remove unregistered_init_map{name};
        remove resolve_output_types{name};
        remove resolved_output_type_callbacks{name};
        DataProviderActionCatalog::removeExternalIoModuleApp(name);
        remove app_sandbox_state{name};
        remove appmodmap{name};
        remove actionmodmap{name};
        remove pending_generic_api_call_apps{name};
        if (actions) {
            map remove top_actionmap{name + "/" + $1.action}, actions.values();
        }
        if (had_state) {
            ++catalog_revision;
        }
        if (!app) {
            return;
        }

        if (app.scheme) {
            remove smap{app.scheme};
        }

        return True;
    }

    #! Returns true when a registration-context path identifies the requested module
    private:internal static bool modulePathsContain(*hash<string, bool> paths,
            string module_name) {
        foreach string path in (keys paths) {
            string context_name = basename(path);
            if (context_name == module_name) {
                return True;
            }
            foreach string suffix in ((".qmod", ".qm")) {
                if (context_name == module_name + suffix) {
                    return True;
                }
            }
        }
        return False;
    }

    #! Returns the technical owner path for a provider registration
    /** Registrations made while a module is being loaded belong to that module, which
        \c get_module_context_path() reports.

        Deferred initialization closures (see registerPendingApp() and registerPendingAppAction())
        run when the app is first used and therefore normally outside any module load; the owning
        module path is then taken from \c _mod_init_ctx, which doInit() sets to the path captured
        when the pending registration was made.

        Such a closure can load further modules, and everything a module registers while its own
        initializer runs belongs to it, not to the module whose deferred initialization triggered
        the load.  A nested load is exactly the case where the live module load context differs
        from the one in effect when the deferred initialization started, so that captured context -
        and not merely the presence of a live context - decides here; a deferred initialization
        driven from inside an unrelated module's load must still attribute the closure's own
        registrations to the module that registered them.

        @return the active module path, the deferred initializer path, or
        @ref NOTHING outside a module-owned registration
    */
    private:internal static *string registrationModulePath() {
        *string path = get_module_context_path();
        if (exists _mod_init_ctx && path == _mod_init_load_ctx) {
            return _mod_init_ctx;
        }
        return path ?? _mod_init_ctx;
    }

    #! Makes the framework-injected generic API action part of every app owner's presentation catalog
    private:internal static setGenericApiCallActionOwners(string app) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }
        # A dynamically registered app has no module owner.  Qore lvalues are
        # NOTHING until assigned, so normalize that valid state to an empty
        # typed hash before assigning it to the non-null action-owner map.
        hash<string, bool> owners = appmodmap{app} ?? {};
        actionmodmap{app}{AbstractDataProvider::GenericApiCallActionName}
            = owners;
    }

    #! Clears retained failure and pending-declaration state after the same identity succeeds
    private:internal static clearSuccessfulInitializationState(string app,
            *string action) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }
        string failure_key = action ?? "";
        bool changed;
        if (!exists action) {
            remove unregistered_init_map{app};
        }
        if (exists expected_initialization_map{app}{failure_key}) {
            remove expected_initialization_map{app}{failure_key};
            if (!expected_initialization_map{app}) {
                remove expected_initialization_map{app};
            }
            changed = True;
        }
        if (exists initialization_failure_map{app}{failure_key}) {
            remove initialization_failure_map{app}{failure_key};
            if (!initialization_failure_map{app}) {
                remove initialization_failure_map{app};
            }
            changed = True;
        }
        if (!changed) {
            return;
        }
        ++catalog_revision;
    }

    #! Returns the name of the module that owns a registration
    /** Like registrationModulePath(): the module being loaded, or, in a deferred initialization that loads no
        other module, the module that registered it, if it was recorded (see registerLazyAppActions())
    */
    private:internal static *string registrationModuleName() {
        if (exists _mod_init_name && get_module_context_path() == _mod_init_load_ctx) {
            return _mod_init_name;
        }
        return get_module_context_name() ?? _mod_init_name;
    }

    #! Returns the applications in a pending registration map and those whose deferred initialization is running
    /** The running ones are read after the map: a thread adds an application to them before it takes the
        application's registrations from the map, so an application whose registrations have just been taken is
        returned while they run, and the caller waits for them in initPending()
    */
    private:internal static list<string> getInitApps(hash<auto> pending) {
        list<string> rv = (keys pending) ?? ();
        rv += (keys init_owners) ?? ();
        return rv;
    }

    #! Runs every pending app registration, then every pending action registration, then every lazy registration
    private:internal static initAllPending() {
        foreach string app in (DataProviderActionCatalog::getInitApps(initmap)) {
            DataProviderActionCatalog::initPending(app, InitPendingApp);
        }
        # all pending actions are initialized to ensure that the supports_* flags are set
        foreach string app in (DataProviderActionCatalog::getInitApps(initactionmap)) {
            DataProviderActionCatalog::initPending(app, InitPendingActions);
        }
        DataProviderActionCatalog::initAllLazyActions();
    }

    #! Runs the lazy action registrations of every application
    private:internal static initAllLazyActions() {
        foreach string app in (DataProviderActionCatalog::getInitApps(lazyactionmap)) {
            DataProviderActionCatalog::initPending(app, InitLazyActions);
        }
    }

    #! Runs the pending app registrations whose names match, and the pending and lazy actions of matching apps
    /** @param match called with the app's name and, for a registered app, its description
    */
    private:internal static initMatchingApps(code<bool(string, *string)> match) {
        foreach string app in (DataProviderActionCatalog::getInitApps(initmap)) {
            # a pending app has no description yet
            if (match(app)) {
                DataProviderActionCatalog::initPending(app, InitPendingApp);
            }
        }
        foreach string app in (DataProviderActionCatalog::getInitApps(initactionmap)
                + DataProviderActionCatalog::getInitApps(lazyactionmap)) {
            *hash<DataProviderAppInfo> info = appcache{app};
            if (info && match(app, info.desc)) {
                DataProviderActionCatalog::initPending(app, InitPendingActions | InitLazyActions);
            }
        }
    }

    #! Runs the lazy action registrations of the applications in the scope of a discovery generation
    /** Does nothing if the generation is not open; the caller then raises the error
    */
    private:internal static initDiscoveryLazyActions(DataProviderDiscoverySession session) {
        *hash<string, bool> scope;
        {
            DataProviderCatalogLock al(lck, \DataProviderActionCatalog::deliverCatalogChanges());
            *hash<DataProviderDiscoveryStateInfo> state = discovery_map{session.getGeneration().toString()};
            if (!state || state.session !== session) {
                return;
            }
            scope = state.app_scope;
        }
        foreach string app in (DataProviderActionCatalog::getInitApps(lazyactionmap)) {
            if (!scope || scope{app}) {
                DataProviderActionCatalog::initPending(app, InitLazyActions);
            }
        }
    }

    #! Initializes the deferred registrations of an app; for one app, only one thread does so at a time
    /** @param app the app
        @param what what to initialize: a combination of @ref InitPendingApp, @ref InitPendingActions or
        @ref InitPendingAction, and @ref InitLazyActions
        @param action the action for @ref InitPendingAction

        A thread reading the app while another thread initializes it waits until the initialization has ended, so
        that it reads the app with everything the initialization registers, and not a part of it; see enterInit()
        for the cases in which it does not wait.

        A thread that holds the catalog lock runs nothing: what it would run registers through the catalog, which
        it cannot lock again.  It reads what has been registered so far.

        Each pending registration runs once, and an exception it raises is retained as an initialization failure (see
        doInit()); lazy registrations run as doLazyInit() describes.

        A reader that no sandbox governs runs the registrations on its own thread, as the module would have run them
        when it was loaded.  A sandboxed reader runs only those made under a sandbox, of which there are none, as they
        run when they are made (see registerLazyAppActions()): what a
        registration that trusted code made creates, which every reader of the catalog shares afterwards, would keep
        the reader's sandbox, including its interrupts (an object created while serving sandboxed code keeps that
        code's sandbox), and the module's own resources would be denied to it.  The catalog worker runs those for it:
        a thread of the %DataProvider Program that no sandbox governs, started by the first sandboxed reader that needs
        it (see initLazyWorker()).  The reader waits for the worker; an interrupt or limit of the reader ends only its
        wait, and the registrations run to their end for every reader.

        A sandboxed thread that is loading a module is refused with \c DATA-PROVIDER-LAZY-INIT-ERROR when trusted
        registrations are pending: it cannot run them itself, and it cannot wait for the worker, because a
        registration can load a module, and the module that this thread is loading cannot be loaded by another thread
        until this thread has loaded it, whose wait the module loader does not see, so the threads would wait for
        each other for ever.  Without a worker, as after the worker was stopped at the end of the process, a
        sandboxed reader is refused as well.  The registrations stay pending for the next reader.

        @throw DATA-PROVIDER-LAZY-INIT-ERROR a sandboxed reader needs a trusted registration and is loading a module,
        or there is no worker
        @throw DATA-PROVIDER-LAZY-WORKER-STOPPED the worker ended before it ran the registrations
    */
    private:internal static initPending(string app, int what, *string action) {
        # nothing to do when nothing is pending and no thread is initializing the app
        if (!DataProviderActionCatalog::isInitPending(app, what, action) || lck.lockOwner()) {
            return;
        }
        DataProviderActionCatalog::checkFork();
        int filter = RunAll;
        # a sandboxed reader runs no registration that trusted code made: the catalog worker does (see initPending())
        if (DataProviderActionCatalog::isGoverned() && !DataProviderActionCatalog::isLazyWorker()) {
            if (DataProviderActionCatalog::hasTrustedPending(app, what, action)) {
                if (get_module_context_path()) {
                    throw "DATA-PROVIDER-LAZY-INIT-ERROR", sprintf("the deferred registrations of application %y "
                        "have not run, and a sandboxed thread that is loading a module cannot run them or wait for "
                        "them", app);
                }
                hash<auto> job = {"app": app, "what": what};
                if (exists action) {
                    job.action = action;
                }
                DataProviderActionCatalog::runOnWorker(job);
            }
            filter = RunGoverned;
        }
        DataProviderActionCatalog::runPending(app, what, action, filter);
    }

    #! Runs the deferred registrations of an app that a filter selects, in their order: the pending app, the lazy
    #! actions, the pending actions
    private:internal static runPending(string app, int what, *string action, int filter) {
        if (what & InitPendingApp) {
            DataProviderActionCatalog::runGated(app, InitPendingApp, NOTHING, filter);
        }
        if (what & InitLazyActions) {
            DataProviderActionCatalog::runGated(app, InitLazyActions, NOTHING, filter);
        }
        if (what & (InitPendingActions | InitPendingAction)) {
            DataProviderActionCatalog::runGated(app, what & (InitPendingActions | InitPendingAction), action, filter);
        }
    }

    #! Returns @ref True if a deferred registration that trusted code made is pending for what the caller needs
    private:internal static bool hasTrustedPending(string app, int what, *string action) {
        if ((what & InitPendingApp) && initmap{app} && !initmap{app}.governed) {
            return True;
        }
        if ((what & InitLazyActions) && (map 1, lazyactionmap{app}, !$1.governed)) {
            return True;
        }
        if ((what & InitPendingActions) && (map 1, initactionmap{app}.iterator(), !$1.governed)) {
            return True;
        }
        if ((what & InitPendingAction) && exists action && initactionmap{app}{action}
                && !initactionmap{app}{action}.governed) {
            return True;
        }
        # another thread initializes the app: the worker waits for it
        return exists init_owners{app} && (init_owners{app}.what & what);
    }

    #! Returns @ref True if a filter of runGated() selects a registration
    private:internal static bool selects(int filter, hash<auto> h) {
        return filter == RunAll || ((filter == RunGoverned) == (h.governed ?? False));
    }

    #! Runs deferred registrations of an app as the thread that initializes it; see initPending()
    private:internal static runGated(string app, int what, *string action, int filter = RunAll) {
        if (!DataProviderActionCatalog::isInitPending(app, what, action)) {
            return;
        }
        *Counter done = DataProviderActionCatalog::enterInit(app, what);
        # ending the initialization cannot fail
        on_exit if (done) {
            DataProviderActionCatalog::exitInit(app, done);
        }
        if (what & InitPendingApp) {
            if (initmap{app} && DataProviderActionCatalog::selects(filter, initmap{app})) {
                if (*hash<auto> h = remove initmap{app}) {
                    DataProviderActionCatalog::doInit(h);
                }
            }
        }
        if (what & InitPendingActions) {
            foreach string act in (keys initactionmap{app}) {
                *hash<auto> h = initactionmap{app}{act};
                if (h && DataProviderActionCatalog::selects(filter, h)) {
                    remove initactionmap{app}{act};
                    DataProviderActionCatalog::doInit(h);
                }
            }
            if (!initactionmap{app}) {
                remove initactionmap{app};
            }
        } else if (what & InitPendingAction) {
            *hash<auto> h = initactionmap{app}{action};
            if (h && DataProviderActionCatalog::selects(filter, h)) {
                remove initactionmap{app}{action};
                DataProviderActionCatalog::doInit(h);
                if (!initactionmap{app}) {
                    remove initactionmap{app};
                }
            }
        }
        if (what & InitLazyActions) {
            list<hash<auto>> lazy = ();
            list<hash<auto>> keep = ();
            foreach hash<auto> h in (lazyactionmap{app}) {
                if (DataProviderActionCatalog::selects(filter, h)) {
                    lazy += h;
                } else {
                    keep += h;
                }
            }
            if (lazy) {
                if (keep) {
                    lazyactionmap{app} = keep;
                } else {
                    remove lazyactionmap{app};
                }
                # kept with the initialization, so that the child of a fork() can run them again
                if (done) {
                    init_owners{app}.running = lazy;
                }
                # the registrations not run yet are restored if one of them is interrupted
                int next = 0;
                on_exit if (next < lazy.size()) {
                    DataProviderActionCatalog::restoreLazy(lazy[next..]);
                }
                while (next < lazy.size()) {
                    hash<auto> h = lazy[next];
                    ++next;
                    DataProviderActionCatalog::doLazyInit(h);
                }
            }
        }
    }

    #! Builds a lazy value unless it is built, and returns it; see getLazyValue()
    /** One thread builds a value at a time, as one thread initializes an app (see enterInit()): another thread that
        needs the value meanwhile waits for it, unless it is loading a module, or the wait would close a cycle of
        waits; such a thread builds a copy of the value for itself, which is not kept.  A value whose builder needs
        the value itself, directly or through other values, cannot be built.

        @throw DATA-PROVIDER-LAZY-VALUE-ERROR the value is needed while this thread builds it
    */
    private:internal static auto buildLazyValue(string key) {
        if (lazy_values{key}.built) {
            return lazy_values{key}.value;
        }
        string owner_key = "<value:" + key + ">";
        if (init_owners{owner_key}.tid == gettid()) {
            throw "DATA-PROVIDER-LAZY-VALUE-ERROR", sprintf("lazy value %s is needed by its own builder", key);
        }
        *Counter done = DataProviderActionCatalog::enterInit(owner_key, InitValue);
        # ending the build cannot fail
        on_exit if (done) {
            DataProviderActionCatalog::exitInit(owner_key, done);
        }
        *hash<auto> v = lazy_values{key};
        if (v.built) {
            return v.value;
        }
        if (!v) {
            throw "DATA-PROVIDER-LAZY-VALUE-ERROR", sprintf("lazy value %s was removed: the module that registered it "
                "failed to load", key);
        }
        auto value = v.builder();
        if (done) {
            AutoLock al(lazy_worker_lck);
            # not stored if the value was removed while it was built
            if (lazy_values{key}) {
                lazy_values{key}.value = value;
                lazy_values{key}.built = True;
                remove lazy_values{key}.builder;
            }
        }
        return value;
    }

    #! Returns @ref True if the current thread is the catalog worker
    private:internal static bool isLazyWorker() {
        return lazy_worker_pid == getpid() && (lazy_workers{gettid().toString()} ?? False);
    }

    #! Runs a job on the catalog worker and waits for it, starting the worker if needed
    /** An interrupt or limit of the reader ends its wait, and leaves nothing behind; the worker runs the job to its
        end, so that the registration it runs ends for every reader

        @throw DATA-PROVIDER-LAZY-INIT-ERROR there is no worker and none can be started
        @throw DATA-PROVIDER-LAZY-WORKER-STOPPED the worker was stopped before it ran the job
    */
    private:internal static auto runOnWorker(hash<auto> job) {
        string tkey = gettid().toString();
        string id;
        {
            AutoLock al(lazy_worker_lck);
            id = (++lazy_job_seq).toString();
        }
        # the wait for the worker is registered before the job is queued, so that a worker, which can read an app
        # that this thread initializes, sees it in its cycle check (see initWaitCycle()); a worker waiting already,
        # directly or through others, for an app that this thread initializes would never run the job
        {
            AutoLock al(init_lck);
            if (DataProviderActionCatalog::lazyWorkerWaitsFor(gettid())) {
                throw "DATA-PROVIDER-LAZY-INIT-ERROR", sprintf("%s cannot be initialized: a catalog worker waits "
                    "for an initialization that this thread runs", DataProviderActionCatalog::describeJob(job));
            }
            init_waits{tkey} = LazyWorkerKey + id;
        }
        # removing the entries takes no lock and cannot fail, however the wait ends: the wait of a sandboxed reader
        # ends when its sandbox is interrupted, and then this thread cannot take a lock; the wait is removed before
        # the result, and finishJob() removes a result that it stores after the wait was removed, so that no result
        # is left behind
        on_exit {
            remove init_waits{tkey};
            remove lazy_job_waiting{id};
            remove lazy_job_results{id};
        }
        {
            AutoLock al(lazy_worker_lck);
            if (!DataProviderActionCatalog::startLazyWorker(True)) {
                throw "DATA-PROVIDER-LAZY-INIT-ERROR", sprintf("%s cannot be initialized for code that a sandbox "
                    "governs: the catalog worker %s", DataProviderActionCatalog::describeJob(job),
                    lazy_worker_shutdown ? "has been stopped" : "is not available");
            }
            lazy_job_waiting{id} = True;
            # a hash of any values: the job given can be narrowed to the type of its values
            hash<auto> queued;
            queued.id = id;
            queued += job;
            lazy_jobs.push(queued);
            ++lazy_pending;
            # another worker when every worker is busy: a slow registration does not delay this job
            DataProviderActionCatalog::startLazyWorker(False);
%ifdef QoreDebug
            ++lazy_jobs_submitted;
            lazy_queue_cond.broadcast();
%endif
        }
        hash<auto> result;
        {
            AutoLock al(lazy_worker_lck);
            # an interrupt or limit of the reader ends its wait; the worker runs the job to its end
            while (!exists lazy_job_results{id}) {
                lazy_job_cond.wait(lazy_worker_lck);
            }
            result = lazy_job_results{id};
        }
        # the worker delivered the changes that it made (see deliverCatalogChanges())
        if (result.error) {
            throw result.error.err, result.error.desc, result.error.arg;
        }
        return result.value;
    }

    #! Describes a job of the catalog worker for an error message
    private:internal static string describeJob(hash<auto> job) {
        if (job.deliver) {
            return "the delivery of catalog changes";
        }
        return exists job.app
            ? sprintf("the actions of application %y", job.app)
            : sprintf("lazy value %d", job.value);
    }

%ifdef QoreDebug
    #! Waits until a number of jobs have been queued for the catalog worker since the process started
    /** For tests of the readers that wait for the worker (debug builds only): a test makes the worker run a job that
        blocks, and then knows when another reader's job is queued behind it

        @param count the number of jobs queued since the process started to wait for
        @param timeout_ms the most to wait, in milliseconds

        @return @ref True if the number was reached, @ref False if the time ran out

        @throw PERMISSION-DENIED the caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static bool waitForLazyInitsQueued(int count, timeout timeout_ms) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", "cannot wait for the catalog worker from a context with restricted process "
                "control";
        }
        # test hooks are for trusted test code only
        if (SandboxManager::isRestricted()) {
            throw "PERMISSION-DENIED", "code that a sandbox governs cannot use the catalog's test hooks";
        }
        AutoLock al(lazy_worker_lck);
        while (lazy_jobs_submitted < count) {
            if (lazy_queue_cond.wait(lazy_worker_lck, timeout_ms)) {
                return lazy_jobs_submitted >= count;
            }
        }
        return True;
    }

    #! Returns the number of jobs queued for the catalog worker since the process started (debug builds only)
    static int getLazyInitsQueued() {
        return lazy_jobs_submitted;
    }

    #! Returns the TIDs of the catalog workers running (debug builds only)
    static list<int> getLazyWorkers() {
        AutoLock al(lazy_worker_lck);
        return map $1.toInt(), keys lazy_workers;
    }
%endif

    #! Creates the capability that starts the catalog worker, and the exit hook that stops it
    /** Called when the module is loaded, in the context of the code loading it: the capability starts the worker in a
        thread that no sandbox governs, also when a sandboxed reader needs it first, and no worker runs until one
        does.  The capability can only be created by code that no sandbox governs, also through a policy barrier, and
        that may use the \c UNCONTROLLED_API domain (the module calls the method by name, so that it can be loaded by
        any Program): from the module path, the module is initialized with no sandbox provenance, also when sandboxed
        code loads it first, and has a worker; when it is loaded first by code restricted with
        @ref Qore::PO_NO_UNCONTROLLED_APIS "PO_NO_UNCONTROLLED_APIS", or by sandboxed code by path, there is no worker,
        and every deferred registration runs, and every lazy value is built, when it is registered, as before
        registrations were deferred.

        @return the capability, or @ref NOTHING if there is no worker
    */
    private:internal static *code initLazyWorker() {
        try {
            code start = call_static_method("SandboxManager", "createSystemThreadCall",
                \DataProviderActionCatalog::lazyWorkerMain());
            register_exit_hook(\DataProviderActionCatalog::stopLazyWorker());
            return start;
        } catch (hash<ExceptionInfo> ex) {
            # no worker: sandboxed readers are refused trusted registrations
        }
    }

    #! Returns @ref True if a sandbox governs the current thread's code, also through a policy barrier
    /** Every decision the catalog makes on where code runs uses this: Qore::SandboxManager::isSandboxed() sees the
        sandbox of the code being served also under a policy barrier (Qore::SandboxManager::callWithSystemPolicy())
        or in a system-policy capability, which mask only the sandbox's filesystem and network policy, not its
        interrupts or the provenance of what the code creates; and it is not restricted, so the catalog decides the
        same for callers restricted with @ref Qore::PO_NO_UNCONTROLLED_APIS "PO_NO_UNCONTROLLED_APIS"
    */
    private:internal static bool isGoverned() {
        return SandboxManager::isSandboxed();
    }

    #! Starts a catalog worker if none runs, or if every worker is busy; lazy_worker_lck is held
    /** The workers run in threads that no sandbox governs (see initLazyWorker()), started when the sandboxed readers
        need them, at most @ref MaxLazyWorkers, counting those started that have not registered yet
        (@ref lazy_workers_starting); none is started after the exit hook stopped them

        @param first @ref True to start one only if none runs, @ref False to start another one if every worker is
        busy

        @return @ref True if a worker runs
    */
    private:internal static bool startLazyWorker(bool first) {
        int pid = getpid();
        # the child of a fork() has none of the parent's threads; the jobs queued for them are dropped
        if (lazy_worker_pid != pid) {
            lazy_workers = {};
            lazy_workers_starting = 0;
            lazy_pending = 0;
            lazy_jobs.clear();
            lazy_delivery_requests = 0;
            lazy_worker_pid = pid;
        }
        int count = lazy_workers.size() + lazy_workers_starting;
        if (first ? count : (lazy_pending <= count || count >= MaxLazyWorkers)) {
            return count > 0;
        }
        if (lazy_worker_shutdown || !lazy_worker_start) {
            return count > 0;
        }
        # reserved before the thread starts: the thread registers itself when it takes the reservation
        ++lazy_workers_starting;
        try {
            code start = lazy_worker_start;
            start();
        } catch (hash<ExceptionInfo> ex) {
            # the exit hooks have started: the process is ending
            --lazy_workers_starting;
        }
        return (lazy_workers.size() + lazy_workers_starting) > 0;
    }

    #! The catalog worker: runs the jobs queued until it is stopped
    /** Started by the capability that the module created when it was loaded, so it runs only this fixed code; its
        work arrives only through the module's own queue: an app, whose lazy registrations made by trusted modules it
        runs, or a lazy value registered by a trusted module, which it builds.  When it ends, also when a
        registration ends its thread, every job still queued ends with \c DATA-PROVIDER-LAZY-WORKER-STOPPED, so that
        no reader waits for it.
    */
    private:internal static lazyWorkerMain() {
        int tid = gettid();
        {
            AutoLock al(lazy_worker_lck);
            # only a thread that startLazyWorker() started runs as a worker, and no more than the pool's maximum:
            # one that the capability starts otherwise, or that calls this method otherwise, ends at once
            if (!lazy_workers_starting || lazy_worker_pid != getpid() || SandboxManager::isSandboxed()) {
                return;
            }
            --lazy_workers_starting;
            if (lazy_workers.size() >= MaxLazyWorkers) {
                return;
            }
            lazy_workers{tid.toString()} = True;
        }
        on_exit {
            DataProviderActionCatalog::endLazyWorker(tid);
        }
        while (*hash<auto> job = lazy_jobs.get()) {
            # a delivery has no reader; the requests made from now on queue another one
            if (job.deliver) {
                lazy_delivery_requests = 0;
                if (!lazy_worker_shutdown) {
                    DataProviderActionCatalog::deliverCatalogChanges();
                }
                continue;
            }
            {
                AutoLock al(init_lck);
                lazy_job_running{job.id} = tid;
            }
            # the reader is released however the job ends, also when a registration ends this thread
            hash<auto> result = {"error": DataProviderActionCatalog::stoppedError(job)};
            on_exit {
                remove lazy_job_running{job.id};
                DataProviderActionCatalog::finishJob(job.id, result);
            }
            if (lazy_worker_shutdown) {
                continue;
            }
            try {
                if (exists job.app) {
                    DataProviderActionCatalog::runPending(job.app, job.what, job.action, RunTrusted);
                    result = {};
                } else {
                    result = {"value": DataProviderActionCatalog::buildLazyValue(job.value.toString())};
                }
            } catch (hash<ExceptionInfo> ex) {
                result = {"error": ex};
            }
        }
    }

    #! Delivers the result of a job to its reader, if it still waits
    private:internal static finishJob(string id, hash<auto> result) {
        AutoLock al(lazy_worker_lck);
        --lazy_pending;
        if (lazy_job_waiting{id}) {
            lazy_job_results{id} = result;
            # the reader removes its wait, and then the result, without the lock (see runOnWorker()): a wait removed
            # after it was checked above, and before the result was stored, leaves the result to be removed here
            if (!lazy_job_waiting{id}) {
                remove lazy_job_results{id};
            }
            lazy_job_cond.broadcast();
        }
    }

    #! Returns the error of a job that the catalog worker did not run to its end
    private:internal static hash<ExceptionInfo> stoppedError(hash<auto> job) {
        return <ExceptionInfo>{
            "err": "DATA-PROVIDER-LAZY-WORKER-STOPPED",
            "desc": sprintf("the catalog worker ended before it initialized %s",
                DataProviderActionCatalog::describeJob(job)),
        };
    }

    #! Ends the catalog worker: no job is queued for it any more, and the jobs still queued fail
    private:internal static endLazyWorker(int tid) {
        AutoLock al(lazy_worker_lck);
        remove lazy_workers{tid.toString()};
        # the other workers, and those starting, run the jobs queued
        if (lazy_workers || lazy_workers_starting) {
            return;
        }
        bool deliver;
        while (lazy_jobs.size()) {
            *hash<auto> job = lazy_jobs.get();
            if (!job) {
                continue;
            }
            if (job.deliver) {
                deliver = True;
                continue;
            }
            --lazy_pending;
            if (lazy_job_waiting{job.id}) {
                lazy_job_results{job.id} = {"error": DataProviderActionCatalog::stoppedError(job)};
            }
        }
        lazy_job_cond.broadcast();
        # a delivery requested is not dropped with the worker: another worker delivers it
        if (deliver && !lazy_worker_shutdown) {
            lazy_jobs.push({"deliver": True});
            DataProviderActionCatalog::startLazyWorker(True);
        }
    }

    #! Stops the catalog worker for good; the exit hook created when the module was loaded
    /** The worker ends after the job it runs, if any; the jobs queued for it end with
        \c DATA-PROVIDER-LAZY-WORKER-STOPPED for their readers, and no worker is started afterwards: a later sandboxed
        reader is refused (see initPending())
    */
    private:internal static stopLazyWorker() {
        AutoLock al(lazy_worker_lck);
        lazy_worker_shutdown = True;
        # one end marker for every worker, also for those started that have not registered yet
        if (lazy_worker_pid == getpid()) {
            map lazy_jobs.push(NOTHING), xrange(lazy_workers.size() + lazy_workers_starting);
        }
    }

    #! Recovers the deferred initialization state in the child of a fork()
    /** The child has none of the parent's threads: the worker and the initializations running in the parent are
        dropped, and the lazy registrations that those initializations had taken are restored, so that the child
        runs them again
    */
    private:internal static checkFork() {
        int pid = getpid();
        if (init_pid == pid) {
            return;
        }
        AutoLock al(init_lck);
        if (init_pid == pid) {
            return;
        }
        if (init_pid) {
            foreach hash<auto> i in (init_owners.pairIterator()) {
                if (i.value.pid != pid && i.value.running) {
                    DataProviderActionCatalog::restoreLazy(i.value.running);
                }
            }
            init_owners = map {$1.key: $1.value}, init_owners.pairIterator(), $1.value.pid == pid;
            init_waits = {};
            # the parent's worker does not run here; the first sandboxed reader starts one (see startLazyWorker())
        }
        init_pid = pid;
    }

    #! Completes the deferred initialization of an app: a pending app, its pending actions and its lazy actions
    private:internal static initApp(string app) {
        DataProviderActionCatalog::initPending(app, InitAll);
    }

    #! Completes the deferred initialization of an action: a pending app, the pending action and lazy actions
    private:internal static initAppAction(string app, string action) {
        DataProviderActionCatalog::initPending(app, InitPendingApp | InitPendingAction | InitLazyActions, action);
    }

    #! Returns @ref True if a deferred registration of the app is pending, or a thread is initializing the app
    /** The registrations are read before the running initializations; see getInitApps()
    */
    private:internal static bool isInitPending(string app, int what, *string action) {
        return ((what & InitPendingApp) && exists initmap{app})
            || ((what & InitPendingActions) && exists initactionmap{app})
            || ((what & InitPendingAction) && exists action && exists initactionmap{app}{action})
            || ((what & InitLazyActions) && exists lazyactionmap{app})
            # another thread runs what the caller needs; a caller that needs nothing it runs does not wait for it, as
            # registerAction() for an app whose lazy actions another thread registers
            || (init_owners{app}.what & what);
    }

    #! Makes this thread the one initializing an app, waiting while another thread initializes it
    /** @return the counter to pass to exitInit() if this thread now initializes the app; @ref NOTHING if it
        initializes it already, or if it must not wait

        A thread does not wait, and then reads what has been registered so far:
        - if it is initializing the app already: an initialization that reads the app it initializes
        - if it is loading a module: the initialization it would wait for can be waiting for that module to load
        - if the wait would close a cycle: the other thread waits, directly or through others, for an app that
          this thread initializes

        An exception, as when the thread is interrupted while it waits, leaves the initialization to the thread
        running it, and this thread owns nothing
    */
    private:internal static *Counter enterInit(string app, int what) {
        int tid = gettid();
        string tkey = tid.toString();
        while (True) {
            Counter wait_for;
            {
                AutoLock al(init_lck);
                *hash<auto> owner = init_owners{app};
                if (!owner) {
                    Counter done(1);
                    init_owners{app} = {"tid": tid, "pid": getpid(), "done": done, "what": what};
                    return done;
                }
                if (owner.tid == tid || get_module_context_path()
                        || DataProviderActionCatalog::initWaitCycle(app, tid)) {
                    return;
                }
                init_waits{tkey} = app;
                wait_for = owner.done;
            }
            # removing the entry cannot fail
            on_exit remove init_waits{tkey};
            wait_for.waitForZero();
        }
    }

    #! Returns @ref True if the worker waits, directly or through others, for an app the thread inits; init_lck is held
    private:internal static bool lazyWorkerWaitsFor(int tid) {
        if (lazy_worker_pid != getpid()) {
            return False;
        }
        foreach string worker in (keys lazy_workers) {
            *string app = init_waits{worker};
            if (exists app && !app.equalPartial(LazyWorkerKey) && DataProviderActionCatalog::initWaitCycle(app, tid)) {
                return True;
            }
        }
        return False;
    }

    #! Returns @ref True if waiting for the initialization of the app would close a cycle of waits; init_lck is held
    private:internal static bool initWaitCycle(string app, int tid) {
        hash<string, bool> seen = {};
        *int owner = init_owners{app}.tid;
        while (exists owner) {
            if (owner == tid) {
                return True;
            }
            string okey = owner.toString();
            if (seen{okey}) {
                return False;
            }
            seen{okey} = True;
            *string next = init_waits{okey};
            if (!exists next) {
                return False;
            }
            if (next.equalPartial(LazyWorkerKey)) {
                # a thread waiting for a job waits for the worker running it; for a job that no worker runs yet, any
                # worker could be the one: a worker waiting for it closes a cycle
                *int worker = lazy_job_running{next.substr(LazyWorkerKey.size())};
                if (!exists worker) {
                    return DataProviderActionCatalog::isLazyWorker() && gettid() == tid;
                }
                owner = worker;
            } else {
                owner = init_owners{next}.tid;
            }
        }
        return False;
    }

    #! Ends this thread's initialization of an app, and wakes the threads waiting for it
    /** Takes no lock and raises no exception, so that the initialization ends also for a thread that is interrupted,
        cancelled, over a sandbox limit or exits
    */
    private:internal static exitInit(string app, Counter done) {
        if (init_owners{app}.done == done) {
            remove init_owners{app};
        }
        done.dec();
    }

    #! Returns the code that runs a deferred registration in the context of the module that makes it
    /** A registration made under a sandbox would be bound to that sandbox (SandboxManager::bindCall()), but such
        registrations run when they are made and are never deferred (see registerLazyAppActions() and
        registerPendingApp()); this keeps a deferred registration bound if that changes
    */
    private:internal static code makeLazyCall(code init) {
        return DataProviderActionCatalog::isGoverned() ? SandboxManager::bindCall(init) : init;
    }

    #! Returns @ref True if an exception ended a lazy registration because its thread was stopped
    /** Such a failure belongs to the reader, not to the registration, which is therefore run again by the next
        reader instead of being retained
    */
    private:internal static bool isTransientFailure(hash<ExceptionInfo> ex) {
        return TransientFailures{ex.err} ?? False;
    }

    #! Restores lazy registrations that did not run to their end, so that the next reader runs them
    /** Takes no lock and raises no exception; a registration of an app that was deregistered, or deregistered and
        registered again, meanwhile is dropped
    */
    private:internal static restoreLazy(list<hash<auto>> registrations) {
        list<hash<auto>> keep = ();
        foreach hash<auto> h in (registrations) {
            if (exists h.serial && app_serials{h.app} == h.serial) {
                keep += h;
            }
        }
        if (!keep) {
            return;
        }
        string app = keep[0].app;
        lazyactionmap{app} = keep + (lazyactionmap{app} ?? ());
    }

    #! Runs a lazy registration
    /** A failure is retained as an initialization failure of the app, unless the registration was stopped (see
        isTransientFailure()), in which case it is restored and the exception is raised to the reader, or unless the
        app was deregistered meanwhile.  A registration that ends without either, as when its thread exits, is
        restored as well.
    */
    private:internal static doLazyInit(hash<auto> h) {
        bool settled;
        on_exit {
            if (!settled) {
                DataProviderActionCatalog::restoreLazy((h,));
            } else {
                # the registration has ended: what it registered is no longer replaced
                DataProviderActionCatalog::endLazyActionRegs(h.app, h.reg_id);
            }
        }
        try {
            DataProviderActionCatalog::doInitIntern(h);
            settled = True;
        } catch (hash<ExceptionInfo> ex) {
            if (DataProviderActionCatalog::isTransientFailure(ex)) {
                rethrow;
            }
            # nothing is retained for an app that is no longer the one registered
            if (app_serials{h.app} == h.serial) {
                DataProviderActionCatalog::reportInitializationFailure(h.kind, h.app, h.action, ex,
                    h.module_context);
            }
            settled = True;
        }
    }

    #! Forgets the actions that an ended lazy registration registered; takes no lock and raises no exception
    private:internal static endLazyActionRegs(string app, int reg_id) {
        foreach hash<auto> i in (lazy_action_regs{app}.pairIterator()) {
            if (i.value == reg_id) {
                remove lazy_action_regs{app}{i.key};
            }
        }
        if (!lazy_action_regs{app}) {
            remove lazy_action_regs{app};
        }
    }

    #! Runs initialization and catches exceptions
    private:internal static doInit(hash<auto> h) {
        try {
            DataProviderActionCatalog::doInitIntern(h);
        } catch (hash<ExceptionInfo> ex) {
            DataProviderActionCatalog::reportInitializationFailure(h.kind, h.app, h.action, ex,
                h.module_context);
        }
    }

    #! Runs initialization in the registration context of its module
    private:internal static doInitIntern(hash<auto> h) {
        *string save = _mod_init_ctx;
        *string save_load = _mod_init_load_ctx;
        *string save_name = _mod_init_name;
        *int save_reg = _lazy_reg_id;
        *bool save_deferred = _trusted_deferred_init;
        on_exit {
            _mod_init_ctx = save;
            _mod_init_load_ctx = save_load;
            _mod_init_name = save_name;
            _lazy_reg_id = save_reg;
            _trusted_deferred_init = save_deferred;
        }
        # sandboxed code registers no deferred initialization, so this one was registered by trusted code, which may
        # register also when a sandboxed reader runs it (see DataProviderSandboxState::checkRegistration())
        _trusted_deferred_init = True;
        # set only for lazy action registrations; see replaceAction()
        _lazy_reg_id = h.reg_id;
        _mod_init_ctx = h.module_context;
        # recorded only for lazy action registrations; see registrationModuleName()
        _mod_init_name = h.module_name;
        # capture the module load context in effect here, so that a module loaded by the
        # closure below can be told apart from this one and own what it registers itself
        _mod_init_load_ctx = get_module_context_path();
        h.init();
        if (h.kind == "app") {
            DataProviderActionCatalog::checkAppInitialized(h.app);
        }
    }

    #! Records an app whose deferred initializer returned without registering it or raising an error
    /** Such an app stays declared for discovery, but no application getter returns it; see
        @ref getAppNamesNoInit()
    */
    private static checkAppInitialized(string app) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            DataProviderActionCatalog::unlockCatalog();
        }
        # an initializer can also register the app pending again, or fail and have the failure retained
        if (!appcache{app} && !initmap{app} && exists expected_initialization_map{app}{""}
                && !exists initialization_failure_map{app}{""}) {
            unregistered_init_map{app} = True;
        }
    }
}
}

namespace Priv {
thread_local *string _mod_init_ctx;
#! The module load context in effect when the deferred initialization in progress was started
thread_local *string _mod_init_load_ctx;
#! The name of the module that made the lazy action registration in progress
thread_local *string _mod_init_name;

#! The ID of the lazy action registration that the current thread runs, if any; see DataProviderActionCatalog
thread_local *int _lazy_reg_id;

#! True while the current thread runs a deferred initialization that trusted code registered; module-private
thread_local *bool _trusted_deferred_init;
}
