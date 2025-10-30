# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderActionCatalog.qc provides the DataProvider catalog API

/** DataProviderActionCatalog.qc Copyright 2019 - 2025 Qore Technologies, s.r.o.

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

    #! Any legal text, links, copyright notice(s), or disclaimer(s) related to the use of the app; markdown supported
    string legal;

    #! The application group, if any
    *string group;

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

    #! Was the application added dynamically?
    bool dynamic = False;

    #! Should this app be hidden in the front end?
    bool hidden = False;

    #! User-defined info about the app
    *hash<auto> info;
}

/** @defgroup dp_action_type DataProvider Action Type Codes

    Each action implements only one type, so these codes do not make up a bitfield
*/
#/@{
#! Data Provider Action Type: wait for an event
public const DPAT_EVENT = 1;

#! Data Provider Action Type: make an API call
public const DPAT_API = 2;

#! Data Provider Action Type: send a message
public const DPAT_SEND_MESSAGE = 3;

#! Data Provider Action Type: find all matching records
public const DPAT_FIND = 4;

#! Data Provider Action Type: find a single record
public const DPAT_FIND_SINGLE = 5;

#! Data Provider Action Type: create a record
public const DPAT_CREATE = 6;

#! Data Provider Action Type: upsert a record
public const DPAT_UPSERT = 7;

#! Data Provider Action Type: update a record
public const DPAT_UPDATE = 8;

#! Data Provider Action Type: delete a record
public const DPAT_DELETE = 9;
#/@}

#! Maps @ref dp_action_type "action type codes"  to app action flags
public const ActionCodeFlagMap = {
    DPAT_EVENT: "supports_events",
    DPAT_API: "supports_apis",
    DPAT_SEND_MESSAGE: "supports_messages",
    DPAT_FIND: "supports_find",
    DPAT_FIND_SINGLE: "supports_find_single",
    DPAT_CREATE: "supports_create",
    DPAT_UPSERT: "supports_upsert",
    DPAT_UPDATE: "supports_update",
    DPAT_DELETE: "supports_delete",
};

#! List of app action flags
public const AppActionFlags = ActionCodeFlagMap.values();

#! Maps @ref dp_action_type "action type codes" to a data provider attribute string
public const ActionAttrMap = {
    DPAT_EVENT: "supports_observable",
    DPAT_API: "supports_request",
    DPAT_SEND_MESSAGE: "supports_messages",
    DPAT_FIND: "supports_read",
    DPAT_FIND_SINGLE: "supports_read",
    DPAT_CREATE: "supports_create",
    DPAT_UPSERT: "supports_upsert",
    DPAT_UPDATE: "supports_update",
    DPAT_DELETE: "supports_delete",
};

#! Maps @ref dp_action_type "action type codes" to a user-friendly name
public const ActionNameMap = {
    DPAT_EVENT: "EVENT",
    DPAT_API: "API",
    DPAT_SEND_MESSAGE: "MESSAGE",
    DPAT_FIND: "FIND",
    DPAT_FIND_SINGLE: "FIND_SINGLE",
    DPAT_CREATE: "CREATE",
    DPAT_UPSERT: "UPSERT",
    DPAT_UPDATE: "UPDATE",
    DPAT_DELETE: "DELETE",
};

#! Data provider action option definition
public hashdecl ActionOptionInfo {
    #! Data type of the option
    AbstractDataProviderType type;

    #! Display name of the option
    string display_name;

    #! Short description of the option in plain text
    string short_desc;

    #! Description of the option with markdown rendering
    string desc;

    #! The group name for the option, if any
    *string group;

    #! Is the option required or not?
    bool required;

    #! Is the option part of a required group?
    /** A required group is a group of options of which at least one must be set
    */
    *softlist<string> required_groups;

    #! Is the option exclusive with other options?
    *softlist<string> exclusive_with;

    #! Any default value for the option
    auto default_value;

    #! Is the option value sensitive?
    bool sensitive;

    #! Are allowed values elements of the list's element type that indicate what values the list can have?
    bool multiselect;

    #! List of allowed values (enum)
    *softlist<hash<AllowedValueInfo>> allowed_values;

    #! Does the option accept values not defined in allowed_values too?
    bool allowed_values_creatable;

    #! List of allowed values for elements of list types (enum)
    *softlist<hash<AllowedValueInfo>> element_allowed_values;

    #! Does the option accept values not defined in element_allowed_values too?
    bool element_allowed_values_creatable;

    #! An optional example value for the option
    auto example_value;

    #! An optional regex pattern for validating string values
    *string validation_regex;

    #! A list of other options that must be set before this option can be chosen
    /** Each element in the list is either
       - a string: giving a single option that the element depends on
       - a list of strings: giving a group of options, one of which must be set before this option can be chosen
    */
    *softlist<softstring> depends_on;

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

    #! If the option should be preselected
    bool preselected;

    #! Is this option read-only?
    bool readonly;

    #! Is this option disabled?
    bool disabled;

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

    #! Any information about how to react to changes
    *softlist<softstring> on_change;

    #! Free-form metadata for the option
    *hash<auto> metadata;
}

#! DataProvider path var info
public hashdecl DataProviderPathVarInfo {
    #! The display name for the action
    string display_name;

    #! Action short description, plain text formatting
    string short_desc;

    #! Action long description, markdown formatting
    string desc;

    #! An optional example value for the option
    *string example_value;
}

#! DataProvider action definition
public hashdecl DataProviderActionInfo {
    #! Name of the application that provides the action
    string app;

    #! The unique name for the action; unique in the application
    string action;

    #! The action group within the app, if any
    *string group;

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

    #! Action type code
    /** @see @ref dp_action_type for valid values
    */
    int action_code;

    #! Message or event type
    /** if \c action_code is:
        - @ref DPAT_EVENT "DPAT_EVENT": this is the event type
        - @ref DPAT_SEND_MESSAGE "DPAT_SEND_MESSAGE": this is the message type
    */
    *string action_val;

    #! The DataProvider class
    /** This is for the case when there is no application \c scheme and the application is not implied by the context.

        It is an error to supply both application \c scheme and \c cls.
    */
    *string cls;

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

    #! A free-form string giving information about the exec type
    *string exec_type;
}

#! DataProvider action and application catalog class
public class DataProviderActionCatalog {
    public {
        #! Required application key set
        const RequiredAppKeys = {
            "name": True,
            "display_name": True,
            "short_desc": True,
            "desc": True,
            "logo": True,
            "logo_file_name": True,
            "logo_mime_type": True,
        };

        #! Required action key set (without action_code and path_vars, which are checked separately)
        const RequiredActionKeys = {
            "app": True,
            "action": True,
            "path": True,
            "display_name": True,
            "short_desc": True,
            "desc": True,
        };
    }

    private {
        #! Mutex for atomic operations
        static Mutex lck();

        #! Top-level master application cache keyed by application name
        static hash<string, hash<DataProviderAppInfo>> appcache;

        #! Master action cache, the key <app>/<action>
        static hash<string, hash<DataProviderActionInfo>> top_actionmap;

        #! Action cache, the key is the application key, the second key is the action string
        static hash<string, hash<string, hash<DataProviderActionInfo>>> app_actionmap;

        #! Scheme map; keys are connection schemes
        static hash<string, list<hash<DataProviderActionInfo>>> smap;

        #! Set of actions requiring output type resolution; app -> action -> True
        static hash<string, hash<string, bool>> resolve_output_types;

        #! Pending application init handler map: app -> init handler
        static hash<string, hash<auto>> initmap;

        #! Pending application action init handler map: app -> action -> init handler
        static hash<string, hash<string, code>> initactionmap;

        #! Maps apps to the modules that define them: app -> module path -> True
        static hash<string, hash<string, bool>> appmodmap;

        #! Default description
        const DefaultDescription = "";
    }

    #! Registers an app for deferred initialization / initialization on demand
    /** @param app the application to register
        @param init the initialization code that will register the app

        @since DataProvider 3.0
    */
    static registerPendingApp(string app, code init) {
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
            "module_context": get_module_context_path(),
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

        initactionmap{app}{action} = init;
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
        if (*list<string> missing = (map $1.key, app{keys RequiredAppKeys}.pairIterator(), !$1.value.val())) {
            throw "APP-ERROR", sprintf("Application is missing the following required keys: %y (app: %y)",
                missing, app);
        }
        if (DataProviderActionCatalog::checkName(app.name)) {
            throw "APP-ERROR", sprintf("Application name %y is illegal; only A-Za-z0-9_- are accepted", app.name);
        }
        if (exists (*string path = _mod_init_ctx ?? get_module_context_path())) {
            appmodmap{app.name}{path} = True;
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }
        if (*hash<DataProviderAppInfo> existing_app = appcache{app.name}) {
            # ignore if the application has the same definition
            if ((existing_app - AppActionFlags) == (app - AppActionFlags)) {
                return;
            }
            throw "APP-ERROR", sprintf("Application %y already registered: %y", app.name, existing_app);
        }
        remove initmap{app};

        # add to cache
        appcache{app.name} = app;

        # deregister the app automatically if module initialization fails
        ImplicitModuleTransaction::add(sub () { DataProviderActionCatalog::deregisterApp(app.name); });
    }

    #! Returns zero or more module paths for the given application
    /** @param app the application name

        @return zero or more module paths for the given application
    */
    static *list<string> getModulePathsForApp(string app) {
        return keys appmodmap{app};
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
        remove app_actionmap{name};
        remove initactionmap{name};
        if (exists (*string path = _mod_init_ctx ?? get_module_context_path())) {
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

    #! Registers a data provider action
    /** @param action the action to register

        @throw ACTION-ERROR missing or invalid attributes, action already registered

        The \c app and \c action attributes must be unique for every action registered
    */
    static registerAction(hash<DataProviderActionInfo> action) {
        # check for required keys
        if (*hash<auto> missing = RequiredActionKeys - keys action) {
            throw "ACTION-ERROR", sprintf("action is missing the following required keys: %y (action: %y)",
                keys missing, action);
        }
        if (*list<string> missing = (map $1.key, action{keys RequiredActionKeys}.pairIterator(), !$1.value.val())) {
            throw "ACTION-ERROR", sprintf("action is missing the following required keys: %y (action: %y)",
                missing, action);
        }
        if (action.action_code < 1 || action.action_code > DPAT_DELETE) {
            throw "ACTION-ERROR", sprintf("invalid action code %y in action: %y", action.action_code, action);
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

        if (action.output_type && action.get_output_type) {
            throw "ACTION-ERROR", sprintf("action has both \"output_type\" and \"get_output_type\"; path: %y",
                action.path);
        }

        if (action.get_dynamic_options && !action.has_dynamic_options) {
            action.has_dynamic_options = True;
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
                return;
            }
            list<string> diff = map $1.key, act.pairIterator(), $1.value != action{$1.key};
            throw "ACTION-ERROR", sprintf("Action with application %y, action %y already registered; diff: %y",
                act.app, act.action, diff);
        }
        # add to caches
        top_actionmap{key} = action;
        app_actionmap{action.app}{action.action} = action;
        # add to scheme map
        if (app.scheme) {
            if (!smap{app.scheme}) {
                smap{app.scheme} = ();
            }
            smap{app.scheme} += action;
        }
        if (action.get_output_type) {
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

            return map $1, appcache.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
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

    #! Returns all actions for the given application
    /** @param app the data provider application

        @return a list of actions for the given application

        @see getActionsEx()
    */
    static *list<hash<DataProviderActionInfo>> getActions(string app) {
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
        if (*code init = remove initactionmap{app}{action}) {
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
        if (*code init = remove initactionmap{app}{action}) {
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
        foreach hash<auto> i in (fields.pairIterator()) {
            on_error rethrow $1.err, sprintf("%s (while processing field %y)", $1.desc, i.key);
            if (i.value instanceof AbstractDataField) {
                hash<DataFieldInfo> info = i.value.getLocalInfo();
                i.value = info{"display_name", "short_desc", "desc", "default_value", "example_value",
                    "multiselect", "allowed_values", "allowed_values_creatable", "element_allowed_values",
                    "element_allowed_values_creatable"} + {
                    "type": i.value.getType(),
                };
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
                if (info.attr.depends_on) {
                    i.value.depends_on = info.attr.depends_on;
                }
                if (exists info.attr.on_change) {
                    i.value.on_change = info.attr.on_change;
                }
                if (info.attr.metadata) {
                    i.value.metadata = info.attr.metadata;
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
                if (info.attr.option_type_name) {
                    i.value.option_type_name = info.attr.option_type_name;
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
                    "element_ref_data", "preselected", "required_groups", "exclusive_with", "multiselect", "metadata"}
                    + additional_fields;
            } else {
                info += i.value{"default_value", "required", "sensitive", "example_value", "ref_data",
                    "element_ref_data", "preselected", "required_groups", "exclusive_with", "multiselect",
                    "validation_regex", "depends_on", "on_change", "metadata", "supports_templates",
                    "supports_custom_values", "supports_expressions"} + additional_fields;
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
            hash<auto> fh = i.value{"display_name", "short_desc", "desc", "multiselect", "allowed_values",
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

    #! Resolves output fypes in actions
    private static resolveOutputTypes() {
        AutoLock al(lck);
        foreach hash<auto> i in ((remove resolve_output_types).pairIterator()) {
            foreach hash<auto> j in (i.value.pairIterator()) {
                string key = sprintf("%s/%s", i.key, j.key);
                try {
                    code get_output_type = remove top_actionmap{key}.get_output_type;
                    remove app_actionmap{i.key}{j.key}.get_output_type;
                    top_actionmap{key}.output_type = app_actionmap{i.key}{j.key}.output_type = get_output_type();
                } catch (hash<ExceptionInfo> ex) {
                    # ignore exceptions
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
        remove app_actionmap{name};
        remove initactionmap{name};
        if (!app) {
            return;
        }

        if (app.scheme) {
            remove smap{app.scheme};
        }
        map remove top_actionmap{name + "/" + $1.action}, app_actionmap{name}.values();
        remove app_actionmap{name};

        return True;
    }

    #! Runs initialization and catches exceptions
    private static doInit(hash<auto> h) {
        try {
            *string save = _mod_init_ctx;
            on_exit _mod_init_ctx = save;
            _mod_init_ctx = h.module_context;
            h.init();
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("%s\n", get_exception_string(ex));
        }
    }

    #! Runs initialization and catches exceptions
    private static doInit(code init) {
        try {
            init();
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("%s\n", get_exception_string(ex));
        }
    }
}
}

namespace Priv {
thread_local *string _mod_init_ctx;
}