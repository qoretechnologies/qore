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
    /** When @ref False (or unset, the default), the framework automatically registers a `make-api-call`
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

#! Data provider action option definition
public hashdecl ActionOptionInfo inherits BaseOptionInfo {
    #! Data type of the option
    AbstractDataProviderType type;

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
    @ref AbstractDataProvider::getPipelineProcessorOutputTypeWithInputType()
    "getPipelineProcessorOutputTypeWithInputType()" with the input type from the preceding
    pipeline state.

    @since %DataProvider 3.6
*/
public const string DPAD_MODIFIED_PASS_THROUGH = "modified-pass-through";
#/@}

#! DataProvider action definition
public hashdecl DataProviderActionInfo {
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

    #! Options that can be provided without a data provider
    *hash<string, hash<ActionOptionInfo>> options;

    #! Do options depend on the data for the call?
    bool data_dependent_options = False;

    #! Output type for the action
    *AbstractDataProviderType output_type;

    #! Does the action support dynamic options?
    bool has_dynamic_options;

    #! In case the output type is expensive to acquire or requires I/O, this closure can be used to set it
    *code get_output_type;

    #! For the case when options should be generated dynamically
    *code get_dynamic_options;

    #! Data provider static info, useful for record-based data providers
    *hash<DataProviderInfo> data_provider_info;

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

#! DataProvider action and application catalog class
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

    private {
        #! Static builder callback that constructs the framework-injected `make-api-call` action info hash
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
        /** Signature: <tt>*hash<auto> sub (string scheme)</tt>. Returns the
            @c ConnectionSchemeInfo hash (with @c cls) for the given scheme, or @ref NOTHING /
            throws if the scheme is unknown. Used by @ref isRestScheme() to walk the connection
            class hierarchy without forming a hard dependency on @c ConnectionProvider (which
            already @c %requires(reexport) @c DataProvider — a hard import here would create a
            cycle).

            @since DataProvider 3.5
        */
        static *code<*hash<auto>(string)> cb_lookup_scheme;

        #! Apps awaiting retroactive `make-api-call` registration once the builder is set
        /** Keys are app names. Populated by @ref registerApp() when a REST-capable app is registered before
            the generic API call action builder has been set; drained by @ref setGenericApiCallActionBuilder()
            and @ref clearGenericApiCallActionBuilder() when the builder becomes available.

            @since DataProvider 3.5
        */
        static hash<string, bool> pending_generic_api_call_apps;

        #! Mutex for atomic operations
        static Mutex lck();

        #! Top-level master application cache keyed by application name
        static hash<string, hash<DataProviderAppInfo>> appcache;

        #! Master action cache, the key is \c app/action
        static hash<string, hash<DataProviderActionInfo>> top_actionmap;

        #! Action cache, the key is the application key, the second key is the action string
        static hash<string, hash<string, hash<DataProviderActionInfo>>> app_actionmap;

        #! Scheme map; keys are connection schemes
        static hash<string, list<hash<DataProviderActionInfo>>> smap;

        #! Set of actions requiring output type resolution; app -> action -> True
        static hash<string, hash<string, bool>> resolve_output_types;

        #! Pending application init handler map: app -> init handler
        static hash<string, hash<auto>> initmap;

        #! Pending application action init handler map: app -> action -> init metadata
        static hash<string, hash<string, hash<auto>>> initactionmap;

        #! Maps apps to the modules that define them: app -> module path -> True
        static hash<string, hash<string, bool>> appmodmap;

        #! Maps actions to the modules that define them: app -> action -> module path -> True
        static hash<string, hash<string, hash<string, bool>>> actionmodmap;

        #! File API map: app -> file API info
        static hash<string, hash<DataProviderFileApiInfo>> app_fileapimap;

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

    #! Registers an app for deferred initialization / initialization on demand
    /** @param app the application to register
        @param init the initialization code that will register the app
        @param module_context optional module path to associate with the app while running \a init

        @since DataProvider 3.0
    */
    static registerPendingApp(string app, code init, *string module_context) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        if (appcache{app}) {
            throw "APP-ERROR", sprintf("Application %y is already registered", app);
        }
        if (initmap{app}) {
            throw "APP-ERROR", sprintf("Application %y is already pending", app);
        }

        initmap{app} = {
            "init": init,
            "module_context": module_context
                ?? DataProviderActionCatalog::registrationModulePath(),
        };
    }

    #! Registers an app action for deferred initialization / initialization on demand
    /** @param app the application to register
        @param action the action to register
        @param init the initialization code that will register the app

        @since DataProvider 3.0
    */
    static registerPendingAppAction(string app, string action, code init) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        if (app_actionmap{app}{action}) {
            throw "APP-ERROR", sprintf("Application %y action %y is already registered", app, action);
        }
        if (initactionmap{app}{action}) {
            throw "APP-ERROR", sprintf("Application %y action %y is already pending", app, action);
        }

        initactionmap{app}{action} = {
            "init": init,
            "module_context": DataProviderActionCatalog::registrationModulePath(),
        };
    }

    #! Registers a data provider application
    /** @param app the application to register

        @throw APP-ERROR missing or invalid attributes, app already registered
    */
    static registerApp(hash<DataProviderAppInfo> app) {
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
                lck.unlock();
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
                        appmodmap{app.name}{module_path} = True;
                        string mac_name = AbstractDataProvider::GenericApiCallActionName;
                        if (app_actionmap{app.name}{mac_name}) {
                            actionmodmap{app.name}{mac_name}{module_path} = True;
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
            if (module_path) {
                appmodmap{app.name}{module_path} = True;
            }

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

        The @ref ConnectionSchemeCache lookup is late-bound via @c call_static_method_args() to avoid
        a hard dependency on @c ConnectionProvider — which already declares
        @c "%requires(reexport) DataProvider", so a hard import here would form a cycle.

        @par Failure mode
        Returns @ref False on any error (scheme not registered, @c ConnectionProvider not loaded, @c cls
        absent, etc.).  This is the conservative default: if we cannot prove the scheme is REST, we do
        not inject.  In normal init order, both real registration paths (qlib @c *RestClient.qm and
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

    #! Registers the framework-injected `make-api-call` action for the given REST-capable app
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
            stderr.printf("DataProviderActionCatalog: REST app %y qualifies for the framework-injected "
                "%y action, but the builder-provider module %y could not be loaded; the action will be "
                "missing from this app: %s: %s\n", app.name, AbstractDataProvider::GenericApiCallActionName,
                GenericApiCallBuilderModule, ex.err, ex.desc);
        }
    }

    #! Sets the static builder used to construct the framework-injected `make-api-call` action info hash
    /** This setter is normally called once at module initialization time by the @c RestClientDataProvider
        module. Subsequent calls with the same builder return @ref True; calls with a different builder when
        a builder is already registered return @ref False without changing the registered builder.

        When the builder is set for the first time, any apps registered earlier that qualified for the
        generic API call action (REST-capable, not opted out) are processed retroactively.

        @param builder a closure with signature
        <tt>hash<DataProviderActionInfo> sub (hash<DataProviderAppInfo> app)</tt> that constructs the
        action info hash for the framework-injected `make-api-call` action

        @return @ref True if the builder was successfully set (first call) or if the same builder is being
        re-registered; @ref False if a different builder is already registered

        @since DataProvider 3.5
    */
    static bool setGenericApiCallActionBuilder(
            code<hash<DataProviderActionInfo>(hash<DataProviderAppInfo>)> builder) {
        list<hash<DataProviderAppInfo>> to_register;
        {
            bool needs_lock = !lck.lockOwner();
            if (needs_lock) {
                lck.lock();
            }
            on_exit if (needs_lock) {
                lck.unlock();
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

        @since DataProvider 3.5
    */
    static clearGenericApiCallActionBuilder() {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
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

        @param cb a closure with signature <tt>*hash<auto> sub (string scheme)</tt> that returns
        the @c ConnectionSchemeInfo hash for the given scheme name. @c cls on the returned hash
        must be the @c Class object for the scheme's connection class (used by
        @ref isRestScheme() to walk the inheritance chain).

        @return @ref True if the callback was successfully set (first call) or if the same callback
        is being re-registered; @ref False if a different callback is already registered

        @since DataProvider 3.5
    */
    static bool setSchemeLookupCallback(code<*hash<auto>(string)> cb) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        if (cb_lookup_scheme) {
            return cb_lookup_scheme == cb;
        }
        cb_lookup_scheme = cb;
        return True;
    }

    #! Clears the registered scheme-lookup callback
    /** This method is primarily for testing purposes.

        @since DataProvider 3.5
    */
    static clearSchemeLookupCallback() {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        delete cb_lookup_scheme;
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

        AutoLock al(lck);
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

        @throw APP-ERROR app not dynamic
        @throw PERMISSION-DENIED Caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static *hash<DataProviderAppInfo> deregisterDynamicApp(string name) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", sprintf("Cannot remove app %y from a context with restricted process control",
                name);
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        remove initmap{name};
        # remove the app's actions from both maps together; removing app_actionmap{name} on its
        # own here (and leaving the top_actionmap cleanup to deregisterAppIntern below) left the
        # top_actionmap entries - and their option/data-type graphs - leaked in the static map
        *hash<string, hash<DataProviderActionInfo>> dactions = remove app_actionmap{name};
        map remove top_actionmap{name + "/" + $1.action}, dactions.values();
        remove initactionmap{name};
        if (exists (*string path = DataProviderActionCatalog::registrationModulePath())) {
            remove appmodmap{name}{path};
        }
        *hash<DataProviderAppInfo> app = appcache{name};
        if (!app) {
            return;
        }
        if (!app.dynamic) {
            throw "PERMISSION-DENIED", sprintf("Cannot remove non-dynamic app %y", name);
        }
        deregisterAppIntern(name);
        return app;
    }

    #! Returns whether executing an action can change provider or external state
    /** An explicit @ref DataProviderActionInfo::mutates_state "mutates_state" value is authoritative.  When
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
    /** @param action the action to register

        @throw ACTION-ERROR missing or invalid attributes, action already registered

        The \c app and \c action attributes must be unique for every action registered
    */
    static registerAction(hash<DataProviderActionInfo> action) {
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

        if (action.hasKey("get_dynamic_options") && !action.has_dynamic_options) {
            action.has_dynamic_options = True;
        }

        # auto-detect the module providing the class so callers can load it before Class::forName()
        if (action.cls && !action.cls_mod) {
            action.cls_mod = get_module_context_name();
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

        AutoLock al(lck);
        # check that the application exists
        if (*hash<auto> h = remove initmap{action.app}) {
            DataProviderActionCatalog::doInit(h);
        }
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
            # ignore duplicates to cover injection scenarios
            if (!DataProviderActionCatalog::compareActions(action, act)) {
                if (module_path) {
                    actionmodmap{action.app}{action.action}{module_path} = True;
                }
                return;
            }
            list<string> diff = ();
            foreach hash<auto> elem in (act.pairIterator()) {
                if (elem.value != action{elem.key}) {
                    diff += elem.key;
                }
            }
            throw "ACTION-ERROR", sprintf("Action with application %y, action %y already registered; diff: %y",
                act.app, act.action, diff);
        }
        # add to caches
        top_actionmap{key} = action;
        app_actionmap{action.app}{action.action} = action;
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
    */
    static *list<hash<DataProviderAppInfo>> getAllAppsNoInit() {
        return appcache.values();
    }

    #! Returns all applications in the cache
    /**
        @return all applications in the cache
    */
    static *list<hash<DataProviderAppInfo>> getAllApps() {
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        foreach string app in (keys initmap) {
            if (*hash<auto> h = remove initmap{app}) {
                DataProviderActionCatalog::doInit(h);
            }
        }
        # initialize all pending actions to ensure supports_* flags are set
        map (map DataProviderActionCatalog::doInit($1), $1.value.iterator()), (remove initactionmap).pairIterator();
        return appcache.values();
    }

    #! Returns the application definition for the given application name
    /** @param app the app name

        @return the application definition for the given application name or @ref nothing if not present

        @see getAppEx()
    */
    static *hash<DataProviderAppInfo> getApp(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app to ensure supports_* flags are set
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return appcache{app};
    }

    #! Returns the application definition for the given application name
    /** @param app the app name

        @return the application definition for the given application name; throws an exception if the app is not
        registered

        @throw APP-ERROR the given application is not registered

        @see getApp()
    */
    static hash<DataProviderAppInfo> getAppEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app to ensure supports_* flags are set
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        if (*hash<DataProviderAppInfo> rv = appcache{app}) {
            return rv;
        }
        throw "APP-ERROR", sprintf("application %y is not registered", app);
    }

    #! Searches for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All applications that match the regular expression pattern in the \c name or \c desc fields
    */
    static *list<hash<DataProviderAppInfo>> searchAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        # initialize any matching pending apps
        map DataProviderActionCatalog::doInit(remove initmap{$1}), keys initmap,
            regex($1, regex_pattern, regex_options);
        # initialize pending actions for matching apps to ensure supports_* flags are set
        foreach string appname in (keys initactionmap) {
            if (appcache{appname} && (regex(appname, regex_pattern, regex_options)
                    || regex(appcache{appname}.desc, regex_pattern, regex_options))) {
                map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{appname};
            }
        }
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return map $1, appcache.values(),
            regex($1.name, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for applications whose names or descriptions match a substring
    /** @param substr A substring to match in the \c name and \c desc fields; pending apps only are matched with the
        name
        @param ignore_case Ignore case when searching

        @return All applications that match the substring argument in the \c name or \c desc fields
    */
    static *list<hash<DataProviderAppInfo>> searchAppSubstr(string substr, bool ignore_case = True) {
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        if (ignore_case) {
            substr = substr.upr();

            # initialize any matching pending apps
            map DataProviderActionCatalog::doInit(remove initmap{$1}), keys initmap, ($1.upr().find(substr) >= 0);

            # initialize pending actions for matching apps to ensure supports_* flags are set
            foreach string appname in (keys initactionmap) {
                if (appcache{appname} && ((appname.upr().find(substr) >= 0)
                        || (appcache{appname}.desc.upr().find(substr) >= 0))) {
                    map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{appname};
                }
            }

            return map $1, appcache.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }

        # case-sensitive path: initialize any matching pending apps
        map DataProviderActionCatalog::doInit(remove initmap{$1}), keys initmap, ($1.find(substr) >= 0);

        # initialize pending actions for matching apps to ensure supports_* flags are set
        foreach string appname in (keys initactionmap) {
            if (appcache{appname} && ((appname.find(substr) >= 0)
                    || (appcache{appname}.desc.find(substr) >= 0))) {
                map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{appname};
            }
        }

        return map $1, appcache.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Returns all actions for a particular connection scheme
    /** @param scheme the connection scheme

        @return a list of actions associated with the given scheme
    */
    static *list<hash<DataProviderActionInfo>> getActionsForScheme(string scheme) {
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return smap{scheme};
    }

    #! Returns a hash of all actions for the given application
    /** @param app the data provider application

        @return a hash of actions keyed by action name for the given application

        @see getActionHashEx()
    */
    static *hash<string, hash<DataProviderActionInfo>> getActionHash(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return app_actionmap{app};
    }

    #! Returns a hash of all actions for the given application
    /** @param app the data provider application

        @return a hash of actions keyed by action name for the given application

        @throw APP-ERROR the given application is not registered

        @see getActionHash()
    */
    static *hash<string, hash<DataProviderActionInfo>> getActionHashEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
            foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActions(app.name,
                False)) {
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
    */
    static *list<hash<DataProviderActionInfo>> getActions(string app, bool check_index = True) {
        # load app on demand
        if (check_index) {
            ProviderIndexUtil::checkAppModule(app);
        }

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        return app_actionmap{app}.values();
    }

    #! Returns the currently registered actions for one application without loading or initializing provider modules
    /** This accessor exposes only actions already present in the in-process catalog. Pending lazy registrations are
        intentionally omitted so callers can overlay current authoritative metadata on an approximate provider index
        without paying the cost of loading modules, resolving output types, or retaining provider schemas.

        @param app the exact data-provider application identifier
        @return currently registered actions, or @ref nothing when none are registered

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
    */
    static list<hash<DataProviderActionInfo>> getActionsEx(string app) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        if (*list<hash<DataProviderActionInfo>> rv = app_actionmap{app}.values()) {
            return rv;
        }
        throw "APP-ERROR", sprintf("application %y is not registered", app);
    }

    #! Returns all actions in the cache
    /** @return all actions in the cache
    */
    static *list<hash<DataProviderActionInfo>> getAllActions() {
        # initialize all pending apps
        map DataProviderActionCatalog::doInit(remove initmap{$1}), keys initmap;
        # initialize all pending actions
        map (map DataProviderActionCatalog::doInit($1), $1.value.iterator()), (remove initactionmap).pairIterator();
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
    */
    static *hash<DataProviderActionInfo> getAppAction(string app, string action) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending action for the app
        if (*hash<auto> init = remove initactionmap{app}{action}) {
            DataProviderActionCatalog::doInit(init);
            if (!initactionmap{app}) {
                remove initactionmap{app};
            }
        }
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
    */
    static hash<DataProviderActionInfo> getAppActionEx(string app, string action) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending action for the app
        if (*hash<auto> init = remove initactionmap{app}{action}) {
            DataProviderActionCatalog::doInit(init);
            if (!initactionmap{app}) {
                remove initactionmap{app};
            }
        }
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
            lck.unlock();
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
            lck.unlock();
        }

        return app_fileapimap{app};
    }

    #! Removes the file API registration for an application
    /** @param app the application name

        @return the mapping that was removed, if any

        @since %DataProvider 3.6
    */
    static *hash<DataProviderFileApiInfo> deregisterAppFileApi(string app) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
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
            lck.unlock();
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
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
        }
        # load the module into this program's context so Class::forName() can resolve the class;
        # load_module() in ProviderIndexUtil loads into ProviderIndexUtil's program context,
        # but Class::forName() resolves in the calling program's context
        if (action.cls_mod) {
            load_module(action.cls_mod);
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
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionRegex(string app, string regex_pattern,
            int regex_options = RE_Caseless) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionSubstr(string app, string substr,
            bool ignore_case = True) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionRegexEx(string app, string regex_pattern,
            int regex_options = RE_Caseless) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
    */
    static *list<hash<DataProviderActionInfo>> searchAppActionSubstrEx(string app, string substr,
            bool ignore_case = True) {
        # load app on demand
        ProviderIndexUtil::checkAppModule(app);

        # initialize any pending app
        if (*hash<auto> h = remove initmap{app}) {
            DataProviderActionCatalog::doInit(h);
        }
        # initialize any pending actions for the app
        map (map DataProviderActionCatalog::doInit($1), $1.iterator()), remove initactionmap{app};
        if (resolve_output_types) {
            DataProviderActionCatalog::resolveOutputTypes();
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
    */
    static *list<hash<DataProviderActionInfo>> searchActionRegex(string regex_pattern,
            int regex_options = RE_Caseless) {
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
    */
    static *list<hash<DataProviderActionInfo>> searchActionSubstr(string substr, bool ignore_case = True) {
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
        hash<string, hash<ActionOptionInfo>> rv;
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
                    "allowed_values", "allowed_values_creatable", "element_allowed_values",
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
                if (info.attr.ui_type.val()) {
                    i.value.ui_type = info.attr.ui_type;
                }
                if (info.attr.depends_on) {
                    i.value.depends_on = info.attr.depends_on;
                }
                if (exists info.attr.on_change) {
                    i.value.on_change = info.attr.on_change;
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
                if (raw_info) {
                    i.value += raw_info{"required", "sensitive", "ref_data", "element_ref_data",
                        "default_ref_data", "preselected", "required_groups", "exclusive_with",
                        "validation_regex", "depends_on", "on_change", "metadata", "messages",
                        "icon", "image", "disabled", "readonly", "supports_custom_values",
                        "supports_templates", "supports_expressions", "server_expression_handling",
                        "option_type_name", "ui_type"};
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
                info += i.value{"default_value", "required", "sensitive", "example_value", "ref_data",
                    "element_ref_data", "preselected", "required_groups", "exclusive_with",
                    "on_change", "metadata", "messages", "icon", "image", "server_expression_handling",
                    "ui_type"}
                    + additional_fields;
            } else {
                info += i.value{"default_value", "required", "sensitive", "example_value", "ref_data",
                    "element_ref_data", "preselected", "required_groups", "exclusive_with",
                    "validation_regex", "depends_on", "on_change", "metadata", "icon", "image", "supports_templates",
                    "supports_custom_values", "supports_expressions", "server_expression_handling", "ui_type"} +
                    additional_fields;
                if (i.value.option_type_name
                    && info.type.getTag("qore.external_name") != i.value.option_type_name) {
                    info.type = info.type.copyWithTags(info.type.getTags() + {
                        "qore.external_name": i.value.option_type_name,
                    });
                }
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
                "default_value", "example_value"} + {
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
            if (*list<string> l = i.value.on_change) {
                fh.attr.on_change = l;
            }
            fields{i.key} = new QoreDataField(fh);
        }
        return new HashDataType(mand ? AutoHashType : AutoHashOrNothingType, action.action);
    }

    #! Returns a normalized path to ensure that comparisons always work
    private static string normalizePath(string path) {
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

    #! Resolves output types in actions
    private static resolveOutputTypes() {
        list<hash<auto>> pending;
        {
            AutoLock al(lck);
            foreach hash<auto> i in ((remove resolve_output_types).pairIterator()) {
                foreach hash<auto> j in (i.value.pairIterator()) {
                    string key = sprintf("%s/%s", i.key, j.key);
                    *hash<DataProviderActionInfo> top_action =
                        top_actionmap{key};
                    *hash<DataProviderActionInfo> app_action =
                        app_actionmap{i.key}{j.key};
                    if (!top_action || !app_action
                            || !top_action.get_output_type) {
                        continue;
                    }
                    pending += {
                        "app": i.key,
                        "action": j.key,
                        "get_output_type": top_action.get_output_type,
                    };
                }
            }
        }

        # Output-type callbacks can lazily load provider modules whose
        # initializers register apps or actions.  Never hold the catalog mutex
        # while invoking them.
        list<hash<auto>> resolved;
        foreach hash<auto> item in (pending) {
            try {
                item.output_type = item.get_output_type();
                resolved += item;
            } catch (hash<ExceptionInfo> ex) {
                # ignore exceptions
            }
        }

        {
            AutoLock al(lck);
            foreach hash<auto> item in (resolved) {
                string key = sprintf("%s/%s", item.app, item.action);
                *hash<DataProviderActionInfo> top_action = top_actionmap{key};
                *hash<DataProviderActionInfo> app_action =
                    app_actionmap{item.app}{item.action};
                # Do not publish a result into an action that was removed and
                # replaced while its callback ran.
                if (!top_action || !app_action
                        || top_action.get_output_type
                            != item.get_output_type) {
                    continue;
                }
                top_action.output_type = item.output_type;
                app_action.output_type = item.output_type;
                remove top_action.get_output_type;
                remove app_action.get_output_type;
                top_actionmap{key} = top_action;
                app_actionmap{item.app}{item.action} = app_action;
                if (*string scheme = appcache{item.app}.scheme) {
                    *list<hash<DataProviderActionInfo>> actions = smap{scheme};
                    if (actions) {
                        for (int idx = 0; idx < actions.size(); ++idx) {
                            if (actions[idx].app == item.app
                                    && actions[idx].action == item.action) {
                                actions[idx] = app_action;
                                smap{scheme} = actions;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    #! Deregisters an app and all its actions if an exception occurs during the other module's initialization
    private:internal static *bool deregisterApp(string name) {
        AutoLock al(lck);

        return deregisterAppIntern(name);
    }

    #! Deregisters an app and all its actions if an exception occurs during the other module's initialization
    private:internal static *bool deregisterAppIntern(string name) {
        *hash<DataProviderAppInfo> app = remove appcache{name};
        # capture the app's actions BEFORE dropping the map entry; the top_actionmap cleanup must
        # iterate the captured actions. Previously app_actionmap{name} was removed first and then
        # iterated (now empty) to clean top_actionmap, so every action - and the entire option /
        # data-type graph it holds - was leaked permanently in the static top_actionmap on each
        # deregistration (unbounded growth under app/connection churn)
        *hash<string, hash<DataProviderActionInfo>> actions = remove app_actionmap{name};
        remove initactionmap{name};
        remove resolve_output_types{name};
        remove appmodmap{name};
        remove actionmodmap{name};
        if (actions) {
            map remove top_actionmap{name + "/" + $1.action}, actions.values();
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
    private static bool modulePathsContain(*hash<string, bool> paths,
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
    private static *string registrationModulePath() {
        *string path = get_module_context_path();
        if (exists _mod_init_ctx && path == _mod_init_load_ctx) {
            return _mod_init_ctx;
        }
        return path ?? _mod_init_ctx;
    }

    #! Makes the framework-injected generic API action part of every app owner's presentation catalog
    private static setGenericApiCallActionOwners(string app) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }
        # A dynamically registered app has no module owner.  Qore lvalues are
        # NOTHING until assigned, so normalize that valid state to an empty
        # typed hash before assigning it to the non-null action-owner map.
        hash<string, bool> owners = appmodmap{app} ?? {};
        actionmodmap{app}{AbstractDataProvider::GenericApiCallActionName}
            = owners;
    }

    #! Runs initialization and catches exceptions
    private static doInit(hash<auto> h) {
        try {
            *string save = _mod_init_ctx;
            *string save_load = _mod_init_load_ctx;
            on_exit {
                _mod_init_ctx = save;
                _mod_init_load_ctx = save_load;
            }
            _mod_init_ctx = h.module_context;
            # capture the module load context in effect here, so that a module loaded by the
            # closure below can be told apart from this one and own what it registers itself
            _mod_init_load_ctx = get_module_context_path();
            h.init();
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("%s\n", get_exception_string(ex));
        }
    }

    #! Runs initialization and catches exceptions
}
}

namespace Priv {
thread_local *string _mod_init_ctx;
#! The module load context in effect when the deferred initialization in progress was started
thread_local *string _mod_init_load_ctx;
}
