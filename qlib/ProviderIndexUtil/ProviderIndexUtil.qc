# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProviderIndexUtil.qc module for indexing data providers

/*  ProviderIndexUtil.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ProviderIndexUtil module
public namespace ProviderIndexUtil {
#! The environment variable for the provider index
public const ProviderIndexEnvVar = "QORE_PROVIDER_INDEX_DIR";

const RoutingIndexVersion = 2;
const MetadataCacheVersion = 2;

#! The mode used when creating the provider index directory
/** @note the execute bit is required, otherwise the directory cannot be traversed, and no index files in it can be
    read or written
*/
const IndexDirMode = 0755;

#! The mode used when creating provider index files
const IndexFileMode = 0644;

#! Data provider index
public hashdecl DataProviderIndexInfo {
    #! Factory to module map
    *hash<string, string> factorymap;

    #! App to module map
    *hash<string, hash<string, bool>> appmap;

    #! Scheme to module map
    *hash<string, hash<string, bool>> schememap;

    #! App to app info map
    *hash<string, hash<auto>> appinfomap;

    #! Maps apps to actions to action info hashes
    /** @note Actions are missing the "data_provider_info", "get_dynamic_options" and "get_output_type" keys, which
        cannot be serialized to YAML, so they are not included in the action info hashes

        Additionally, action option types are approximated from the base type name of each option type
    */
    *hash<string, hash<string, hash<auto>>> actioninfomap;

    #! Maps app actions to action info hashes
    /** @note Actions are missing the "data_provider_info", "get_dynamic_options" and "get_output_type" keys, which
        cannot be serialized to YAML, so they are not included in the action info hashes

        Additionally, action option types are approximated from the base type name of each option type
    */
    *hash<string, hash<auto>> top_actioninfomap;
}

#! Index info
public hashdecl ProviderIndexInfo {
    #! Path to the index file
    string path;

    #! Size of the index file
    int size;

    #! Elapsed time to create the Index
    date delta;

    #! Summary info for the indexing operation
    hash<auto> summary;
}

#! Internal context for reconstructing typed provider metadata
hashdecl MetadataDeserializeContext {
    TypedHash app_info_th;
    TypedHash conn_opt_th;
    TypedHash action_info_th;
    TypedHash option_info_th;
    TypedHash path_vars_th;
    TypedHash dp_info_th;
    TypedHash dp_opt_info_th;
    TypedHash dp_exp_info_th;
    TypedHash mapper_key_info_th;
    TypedHash dp_msg_info_th;
    TypedHash dp_sig_info_th;
    TypedHash dp_arg_info_th;
    hash<auto> type_map;
    hash<auto> shape_map;
    *hash<auto> ui_view_map;
}

#! Versioned routing sidecar for a data provider index
hashdecl DataProviderRoutingIndexInfo {
    int version;
    string data_sha256;
    string routing_sha256;
    *int metadata_cache_version;
    *string metadata_cache_sha256;
    *hash<string, string> factorymap;
    *hash<string, hash<string, bool>> appmap;
    *hash<string, hash<string, bool>> schememap;
}

#! Integrity metadata for one app-specific metadata cache chunk
hashdecl DataProviderMetadataCacheChunkInfo {
    int size;
    string sha256;
}

#! Versioned manifest for app-specific metadata cache chunks
hashdecl DataProviderMetadataCacheManifest {
    int version;
    hash<string, DataProviderMetadataCacheChunkInfo> apps;
}

#! Result of loading deferred raw metadata
hashdecl RawMetadataIndexResult {
    *hash<DataProviderIndexInfo> index;
    bool complete;
}

#! The main utility class
public class ProviderIndexUtil {
    public {}

    private {
        #! Global logger
        static *LoggerInterface logger;

        #! Global mutex
        static Mutex m();

        #! Global data provider index
        static *hash<DataProviderIndexInfo> index;

        #! Initialized flag
        static bool initialized;

        #! Initialization-in-progress flag
        static bool initializing;

        #! Thread currently initializing the data provider index
        static *int initializing_tid;

        #! Condition for threads waiting for data provider index initialization
        static Condition init_cond();

        #! Serialized metadata retained until the corresponding app is requested
        static hash<string, hash<auto>> raw_appinfomap;
        static hash<string, hash<string, hash<auto>>> raw_actioninfomap;

        #! Shared metadata deserialization context
        static *hash<MetadataDeserializeContext> metadata_context;
        static bool metadata_initialized;
        static bool metadata_initializing;
        static *int metadata_initializing_tid;
        static Condition metadata_init_cond();

        #! Per-app metadata materialization state
        static hash<string, bool> app_metadata_initialized;
        static hash<string, int> app_metadata_initializing;
        static hash<string, bool> action_metadata_initialized;
        static hash<string, int> action_metadata_initializing;

        #! Deferred full-index metadata loading state
        static bool raw_metadata_deferred;
        static bool raw_metadata_loading;
        static *int raw_metadata_loading_tid;
        static hash<string, bool> raw_metadata_app_loaded;

        #! Validated manifest digest set only by serialized index initialization and immutable after initialization
        static *string metadata_cache_sha256;
        static *hash<DataProviderMetadataCacheManifest> metadata_cache_manifest;
        static bool metadata_cache_manifest_checked;

        #! All apps loaded flag
        static bool apps_loaded;

        #! Set of modules that have been successfully loaded; used to avoid calling load_module() which acquires
        #! the global module lock even when a module is already loaded
        static hash<string, bool> loaded_module_cache;

        #! The last error encountered when reading the canonical data provider index, if any
        /** This is set when the index exists but could not be read (ex: a permissions problem or a corrupt file);
            it is not set when no index has been created yet
        */
        static *string index_read_error;
    }

    #! Returns any error encountered while reading the canonical data provider index
    /** @return the description of the error encountered when trying to read the data provider index, if any;
        @ref nothing is returned if the index was read successfully or if no index has been created yet

        Callers can use this to distinguish an unreadable or corrupt index (which silently degrades the set of
        known apps and factories) from a system where no index has been created at all
    */
    static *string getIndexReadError() {
        return index_read_error;
    }

    #! Returns a list of cached app info
    static *list<hash<auto>> getCachedApps() {
        checkAllAppMetadata();
        return index.appinfomap.values();
    }

    #! Searches the app info cache for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached applications that match the regular expression pattern in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        checkAllAppMetadata();
        return map $1, index.appinfomap.values(),
            regex($1.name, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cachde applications whose names or descriptions match a substring
    /** @param substr A substring to match in the \c name and \c desc fields; pending apps only are matched with the
        name
        @param ignore_case Ignore case when searching

        @return All cached applications that match the substring argument in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppSubstr(string substr, bool ignore_case = True) {
        checkAllAppMetadata();
        if (ignore_case) {
            substr = substr.upr();

            return map $1, index.appinfomap.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, index.appinfomap.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Try to return any cached app info for the given app
    static *hash<auto> tryGetAppInfo(string app) {
        checkAppMetadata(app);
        return index.appinfomap{app};
    }

    #! Try to return any cached approximate app action list for the given app
    static *list<hash<auto>> tryGetApproximateActionsForApp(string app) {
        checkActionMetadata(app);
        return index.actioninfomap{app}.values();
    }

    #! Try to return any cached approximate app action for the given app and action
    static *hash<auto> tryGetApproximateAppAction(string app, string action) {
        checkActionMetadata(app);
        return index.actioninfomap{app}{action};
    }

    #! Searches for cached application actions whose descriptions match a regular expression pattern
    /** @param app The application to search for actions in
        @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached actions that match the regular expression pattern in the \c short_desc or \c desc fields

        @note If the application is not cached, no results are returned
    */
    static *list<hash<auto>> searchCachedAppActionRegex(string app, string regex_pattern,
            int regex_options = RE_Caseless) {
        checkActionMetadata(app);
        return map $1, index.actioninfomap{app}.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cached application actions whose descriptions match a substring
    /** @param app The application to search for actions in
        @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All actions that match the substring argument in the \c short_desc or \c desc fields

        @note If the application is not cached, no results are returned
    */
    static *list<hash<auto>> searchCachedAppActionSubstr(string app, string substr, bool ignore_case = True) {
        checkActionMetadata(app);
        if (ignore_case) {
            substr = substr.upr();
            return map $1, index.actioninfomap{app}.values(),
                ($1.short_desc.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, index.actioninfomap{app}.values(),
            ($1.short_desc.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Searches for cached actions whose descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached actions that match the regular expression pattern in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionRegex(string regex_pattern,
            int regex_options = RE_Caseless) {
        checkAllActionMetadata();
        return map $1, index.top_actioninfomap.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cached actions whose descriptions match a substring
    /** @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All cached actions that match the substring argument in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionSubstr(string substr, bool ignore_case = True) {
        checkAllActionMetadata();
        if (ignore_case) {
            substr = substr.upr();
            return map $1, index.top_actioninfomap.values(),
                ($1.short_desc.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, index.top_actioninfomap.values(),
            ($1.short_desc.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Returns all cached actions
    /** @return all cached actions
    */
    static *list<hash<auto>> getAllCachedActions() {
        checkAllActionMetadata();
        return index.top_actioninfomap.values();
    }

    #! Adds the given app and actions to the cache
    static addAppToCache(hash<auto> app) {
        if (!app.name) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add app info to cache: app name is missing (%y)", app);
        }
        checkAppMetadata(app.name);
        m.lock();
        on_exit m.unlock();
        index.appinfomap{app.name} = app;
        remove raw_appinfomap{app.name};
        app_metadata_initialized{app.name} = True;
    }

    #! Adds the given app and actions to the cache
    static addActionToCache(hash<auto> action) {
        checkAppMetadata(action.app);
        checkActionMetadata(action.app);
        m.lock();
        on_exit m.unlock();
        if (!index.appinfomap{action.app}) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action for unknown app %y to cache", action.app);
        }
        if (!action.action) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action info to cache for app %y: action name is "
                "missing", action.app);
        }
        index.actioninfomap{action.app}{action.action} = action;
        index.top_actioninfomap{action.app + "/" + action.action} = action;
        remove raw_actioninfomap{action.app};
        action_metadata_initialized{action.app} = True;
    }

    #! Removes the given app and all its actions from the cache
    static bool removeCachedApp(string app) {
        checkAppMetadata(app);
        checkActionMetadata(app);
        m.lock();
        on_exit m.unlock();
        if (!remove index.appinfomap{app}) {
            return False;
        }
        map remove index.top_actioninfomap{app + "/" + $1}, keys (index.actioninfomap{app});
        return True;
    }

    #! Try to ensure that any required module is loaded for the given factory
    /** @param factory the data provider factory name

        @return @ref True if the module was loaded, @ref False if it was not
    */
    static bool checkFactoryModule(string factory) {
        checkDataIndex();

        if (exists (*string mod = index.factorymap{factory})) {
            return tryLoadModule(mod);
        }
        return False;
    }

    #! Try to ensure that any required modules are loaded for the given data provider application
    /** @param app the data provider application

        @return @ref True if at lease one required module was loaded, @ref False if was not
    */
    static bool checkAppModule(string app) {
        checkDataIndex();

        if (*hash<string, bool> h = index.appmap{app}) {
            bool loaded = False;
            foreach string mod in (keys h) {
                if (tryLoadModule(mod) && !loaded) {
                    loaded = True;
                }
            }
            return loaded;
        }
        return False;
    }

    #! Try to ensure that any required modules are loaded for the given connection scheme
    /** @param scheme the scheme

        @return @ref True if at lease one required module was loaded, @ref False if was not
    */
    static bool checkSchemeModule(string scheme) {
        checkDataIndex();

        if (*hash<string, bool> h = index.schememap{scheme}) {
            bool loaded = False;
            foreach string mod in (keys h) {
                if (tryLoadModule(mod) && !loaded) {
                    loaded = True;
                }
            }
            return loaded;
        }
        return False;
    }

    #! Tries to load the given module or path to the module
    /** @param mod the module to load

        @return @ref True if the module was loaded, @ref False if it was not
    */
    static bool tryLoadModule(string mod) {
        # check local cache first to avoid acquiring the global module lock
        if (loaded_module_cache{mod}) {
            return True;
        }
        try {
            load_module(mod);
            loaded_module_cache{mod} = True;
            return True;
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Error loading module %y: %s: %s", mod, ex.err, ex.desc);
            return False;
        }
    }

    #! Returns a list of all app modules that have not been returned by a call to this method yet and are not loaded
    static *list<string> getAllAppModulesToLoad() {
        if (apps_loaded) {
            return;
        }
        checkDataIndex();
        if (!initialized) {
            return;
        }

        m.lock();
        on_exit m.unlock();

        # check again in the lock
        if (apps_loaded) {
            return;
        }

        # iterate all apps and calculate the set of unloaded modules
        *hash<auto> mh = get_module_hash();
        hash<string, bool> rv;
        foreach hash<string, bool> h in (index.appmap.iterator()) {
            foreach string path in (keys h) {
                string mod = basename(path);
                mod =~ s/\.qm$//i;
                if (!mh{mod}) {
                    rv{path} = True;
                }
            }
        }
        apps_loaded = True;

        return keys rv;
    }

    #! Checks if the data provider index has been populated
    static private checkDataIndex() {
        if (initialized) {
            return;
        }

        {
            m.lock();
            on_exit m.unlock();
            while (initializing && initializing_tid != gettid()) {
                init_cond.wait(m);
            }

            # check again in the lock
            if (initialized) {
                return;
            }

            if (initializing) {
                # Re-entrant lookups can happen while readDataProviderIndex() loads modules or resolves typed hash fields.
                # Do not recursively initialize the index or try to relock this mutex from the same thread.
                return;
            }

            initializing = True;
            initializing_tid = gettid();
            index = createEmptyDataProviderIndex();
        }

        *hash<DataProviderIndexInfo> new_index;
        try {
            new_index = readInitialDataProviderIndex();
        } catch (hash<ExceptionInfo> ex) {
            m.lock();
            on_exit m.unlock();
            initializing = False;
            remove initializing_tid;
            init_cond.broadcast();
            rethrow;
        }

        m.lock();
        on_exit m.unlock();
        raw_appinfomap = {};
        raw_actioninfomap = {};
        if (new_index) {
            raw_appinfomap = new_index.appinfomap ?? {};
            raw_actioninfomap = new_index.actioninfomap ?? {};
            new_index.appinfomap = {};
            new_index.actioninfomap = {};
            index = new_index;
        }
        initialized = True;
        initializing = False;
        remove initializing_tid;
        init_cond.broadcast();
    }

    #! Loads serialized metadata after routing-only initialization
    /** @param app an optional app name; when present, only that app's cache chunk is loaded
    */
    static private checkRawMetadataIndex(*string app = NOTHING) {
        checkDataIndex();
        if (!initialized || !raw_metadata_deferred || (app && raw_metadata_app_loaded{app})) {
            return;
        }

        {
            m.lock();
            on_exit m.unlock();
            while (raw_metadata_loading && raw_metadata_loading_tid != gettid()) {
                metadata_init_cond.wait(m);
            }
            if (!raw_metadata_deferred || (app && raw_metadata_app_loaded{app})) {
                return;
            }
            if (raw_metadata_loading) {
                return;
            }
            raw_metadata_loading = True;
            raw_metadata_loading_tid = gettid();
        }

        hash<RawMetadataIndexResult> result;
        try {
            result = readDeferredRawDataProviderIndex(app);
        } catch (hash<ExceptionInfo> ex) {
            m.lock();
            on_exit m.unlock();
            raw_metadata_loading = False;
            remove raw_metadata_loading_tid;
            metadata_init_cond.broadcast();
            rethrow;
        }

        m.lock();
        on_exit m.unlock();
        if (result.index) {
            raw_appinfomap += result.index.appinfomap ?? {};
            raw_actioninfomap += result.index.actioninfomap ?? {};
            if (result.index.top_actioninfomap) {
                index.top_actioninfomap += result.index.top_actioninfomap;
            }
        }
        if (result.complete) {
            raw_metadata_deferred = False;
        } else if (app) {
            raw_metadata_app_loaded{app} = True;
        }
        raw_metadata_loading = False;
        remove raw_metadata_loading_tid;
        metadata_init_cond.broadcast();
    }

    #! Ensures that the shared metadata deserialization context has been initialized
    static private bool checkMetadataContext() {
        if (metadata_initialized) {
            return True;
        }

        {
            m.lock();
            on_exit m.unlock();
            while (metadata_initializing && metadata_initializing_tid != gettid()) {
                metadata_init_cond.wait(m);
            }

            if (metadata_initialized) {
                return True;
            }

            if (metadata_initializing) {
                return False;
            }

            metadata_initializing = True;
            metadata_initializing_tid = gettid();
        }

        *hash<MetadataDeserializeContext> new_context;
        try {
            new_context = createMetadataDeserializeContext();
        } catch (hash<ExceptionInfo> ex) {
            m.lock();
            on_exit m.unlock();
            metadata_initializing = False;
            remove metadata_initializing_tid;
            metadata_init_cond.broadcast();
            rethrow;
        }

        m.lock();
        on_exit m.unlock();
        metadata_context = new_context;
        metadata_initialized = True;
        metadata_initializing = False;
        remove metadata_initializing_tid;
        metadata_init_cond.broadcast();
        return True;
    }

    #! Ensures that app metadata for the given app has been deserialized
    static private checkAppMetadata(string app) {
        checkRawMetadataIndex(app);
        if (!initialized || app_metadata_initialized{app} || !checkMetadataContext()) {
            return;
        }

        hash<auto> raw_info;
        {
            m.lock();
            on_exit m.unlock();
            while (exists app_metadata_initializing{app} && app_metadata_initializing{app} != gettid()) {
                metadata_init_cond.wait(m);
            }
            if (app_metadata_initialized{app} || exists app_metadata_initializing{app}) {
                return;
            }
            if (!exists raw_appinfomap{app}) {
                app_metadata_initialized{app} = True;
                return;
            }
            app_metadata_initializing{app} = gettid();
            raw_info = raw_appinfomap{app};
        }

        hash<auto> app_info;
        try {
            app_info = deserializeAppInfo(raw_info, metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing app %y from the data provider index: %s",
                app, get_exception_string(ex));
            app_info = raw_info;
        }

        m.lock();
        on_exit m.unlock();
        index.appinfomap{app} = app_info;
        remove raw_appinfomap{app};
        app_metadata_initialized{app} = True;
        remove app_metadata_initializing{app};
        metadata_init_cond.broadcast();
    }

    #! Ensures that action metadata for the given app has been deserialized
    static private checkActionMetadata(string app) {
        checkRawMetadataIndex(app);
        if (!initialized || action_metadata_initialized{app} || !checkMetadataContext()) {
            return;
        }

        hash<string, hash<auto>> raw_actions;
        {
            m.lock();
            on_exit m.unlock();
            while (exists action_metadata_initializing{app} && action_metadata_initializing{app} != gettid()) {
                metadata_init_cond.wait(m);
            }
            if (action_metadata_initialized{app} || exists action_metadata_initializing{app}) {
                return;
            }
            if (!exists raw_actioninfomap{app}) {
                action_metadata_initialized{app} = True;
                return;
            }
            action_metadata_initializing{app} = gettid();
            raw_actions = raw_actioninfomap{app};
        }

        hash<auto> action_map;
        try {
            action_map = deserializeActionsWithContext({app: raw_actions}, metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing actions for app %y from the data provider "
                "index: %s", app, get_exception_string(ex));
            action_map{app} = raw_actions;
        }

        m.lock();
        on_exit m.unlock();
        index.actioninfomap{app} = action_map{app} ?? {};
        foreach hash<auto> action in (index.actioninfomap{app}.pairIterator()) {
            index.top_actioninfomap{app + "/" + action.key} = action.value;
        }
        remove raw_actioninfomap{app};
        action_metadata_initialized{app} = True;
        remove action_metadata_initializing{app};
        metadata_init_cond.broadcast();
    }

    #! Ensures that all app metadata has been deserialized
    static private checkAllAppMetadata() {
        checkRawMetadataIndex();
        if (!initialized || !checkMetadataContext()) {
            return;
        }

        hash<string, hash<auto>> raw_apps;
        list<string> apps;
        {
            m.lock();
            on_exit m.unlock();
            while (app_metadata_initializing.size()) {
                foreach int tid in (app_metadata_initializing.iterator()) {
                    if (tid == gettid()) {
                        return;
                    }
                }
                metadata_init_cond.wait(m);
            }
            apps = keys raw_appinfomap;
            if (!apps) {
                return;
            }
            int tid = gettid();
            map app_metadata_initializing{$1} = tid, apps.iterator();
            raw_apps = raw_appinfomap;
        }

        hash<string, hash<auto>> app_map;
        try {
            app_map = map {
                $1.key: deserializeAppInfo($1.value, metadata_context),
            }, raw_apps.pairIterator();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing apps from the data provider index: %s",
                get_exception_string(ex));
            app_map = raw_apps;
        }

        m.lock();
        on_exit m.unlock();
        index.appinfomap += app_map;
        raw_appinfomap = {};
        map app_metadata_initialized{$1} = True, apps.iterator();
        app_metadata_initializing = {};
        metadata_init_cond.broadcast();
    }

    #! Ensures that all action metadata has been deserialized
    static private checkAllActionMetadata() {
        checkRawMetadataIndex();
        if (!initialized || !checkMetadataContext()) {
            return;
        }

        hash<string, hash<string, hash<auto>>> raw_actions;
        list<string> apps;
        {
            m.lock();
            on_exit m.unlock();
            while (action_metadata_initializing.size()) {
                foreach int tid in (action_metadata_initializing.iterator()) {
                    if (tid == gettid()) {
                        return;
                    }
                }
                metadata_init_cond.wait(m);
            }
            apps = keys raw_actioninfomap;
            if (!apps) {
                return;
            }
            int tid = gettid();
            map action_metadata_initializing{$1} = tid, apps.iterator();
            raw_actions = raw_actioninfomap;
        }

        hash<auto> action_map;
        try {
            action_map = deserializeActionsWithContext(raw_actions, metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing actions from the data provider index: %s",
                get_exception_string(ex));
            action_map = raw_actions;
        }

        m.lock();
        on_exit m.unlock();
        index.actioninfomap += action_map;
        foreach hash<auto> app_actions in (action_map.pairIterator()) {
            map index.top_actioninfomap{app_actions.key + "/" + $1.key} = $1.value,
                app_actions.value.pairIterator();
        }
        raw_actioninfomap = {};
        map action_metadata_initialized{$1} = True, apps.iterator();
        action_metadata_initializing = {};
        metadata_init_cond.broadcast();
    }

    #! Returns an empty data provider index with all map fields initialized
    static private hash<DataProviderIndexInfo> createEmptyDataProviderIndex() {
        return <DataProviderIndexInfo>{
            "factorymap": {},
            "appmap": {},
            "schememap": {},
            "appinfomap": {},
            "actioninfomap": {},
            "top_actioninfomap": {},
        };
    }

    #! Sets or clears the global logger
    /** @param logger the global data provider logger
    */
    static setGlobalLogger(*LoggerInterface logger) {
        ProviderIndexUtil::logger = logger;
    }

    #! Returns the global data provider logger
    /** @return the global data provider logger
    */
    static *LoggerInterface getGlobalLogger() {
        return logger;
    }

    #! Writes the data provider index to the configured directory
    static hash<ProviderIndexInfo> writeDataProviderIndex(*hash<DataProviderIndexInfo> h, date delta,
            hash<auto> summary) {
        # verify that the target directory can be written to before writing anything
        getWritableIndexDir();
        hash<ProviderIndexInfo> info = writeIndex(h, "data", delta, summary);
        *string cache_sha256;
        try {
            cache_sha256 = writeMetadataDataProviderIndex(h);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                get_exception_string(ex));
        }
        try {
            writeRoutingDataProviderIndex(h, digest("sha256", File::readTextFile(info.path)).toHex(), cache_sha256);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to write the optional data provider routing index: %s",
                get_exception_string(ex));
        }
        return info;
    }

    #! Writes the app-sharded MessagePack metadata cache when MessagePack support is available
    static private *string writeMetadataDataProviderIndex(*hash<DataProviderIndexInfo> h) {
%ifdef NoMsgPack
        return;
%else
        hash<string, bool> apps = map {$1: True}, keys (h.appinfomap ?? {});
        map apps{$1} = True, keys (h.actioninfomap ?? {});

        hash<string, DataProviderMetadataCacheChunkInfo> chunks;
        foreach string app in (keys apps) {
            hash<DataProviderIndexInfo> app_index = createEmptyDataProviderIndex();
            if (exists h.appinfomap{app}) {
                app_index.appinfomap{app} = h.appinfomap{app};
            }
            if (exists h.actioninfomap{app}) {
                app_index.actioninfomap{app} = h.actioninfomap{app};
            }

            binary data = msgpack_pack(app_index, MSGPACK_QORE_MODE);
            writeFileAtomic(getMetadataCacheChunkPath(app), data);
            chunks{app} = <DataProviderMetadataCacheChunkInfo>{
                "size": data.size(),
                "sha256": digest("sha256", data).toHex(),
            };
        }

        binary data = msgpack_pack(<DataProviderMetadataCacheManifest>{
            "version": MetadataCacheVersion,
            "apps": chunks,
        }, MSGPACK_QORE_MODE);
        writeFileAtomic(getMetadataCachePath(), data);
        # the manifest is the only way a chunk is ever reached, so every chunk it does not name is
        # unreachable from this point on; remove them rather than leaving them to accumulate
        removeUnreferencedMetadataCacheChunks(keys chunks);
        return digest("sha256", data).toHex();
%endif
    }

    #! Removes app metadata cache chunks that the current manifest does not reference
    /** A chunk is only ever read through the manifest written above (@ref readMetadataCacheChunk is called only
        for an app the manifest names, and re-verifies the recorded size and SHA-256), so a chunk for an app that
        is no longer indexed can never be read again.  Nothing removed them, so renaming or uninstalling a
        provider module stranded its chunk in the index directory permanently: the files are regenerable cache,
        but they are not small, and they accumulated for the life of the installation.

        Removal is deliberately driven by the manifest rather than by comparing against the modules present:
        the manifest is what the reader consults, so "unreferenced" is exactly "unreachable".

        @param apps the app names the manifest references

        @par Example
        Uninstalling a provider module and rebuilding the index removes that app's chunk, so the index directory
        holds one chunk per indexed app and nothing else.

        @note Errors are logged and ignored: failing to remove a stale cache file must never fail an index write,
            because the index itself is already valid at this point.
    */
    static private removeUnreferencedMetadataCacheChunks(list<string> apps) {
        hash<string, bool> keep = map {sprintf("qore-data-index-%s.msgpack", digest("sha256", $1).toHex()): True},
            apps;
        *string dir = tryGetIndexDir();
        if (!dir.val()) {
            return;
        }
        try {
            Dir d();
            d.chdir(dir);
            # the manifest itself (`qore-data-index.msgpack`) has no chunk suffix and must not match
            foreach string name in (d.listFiles("^qore-data-index-[0-9a-f]{64}\\.msgpack$")) {
                if (keep{name}) {
                    continue;
                }
                try {
                    unlink(dir + DirSep + name);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::LevelWarn, "Unable to remove stale data provider metadata cache chunk %y: "
                        "%s: %s", name, ex.err, ex.desc);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to scan the data provider index directory for stale metadata "
                "cache chunks: %s: %s", ex.err, ex.desc);
        }
    }

    #! Reads the data provider index from the configured directory
    static *hash<DataProviderIndexInfo> readDataProviderIndex() {
        return deserializeDataProviderIndex(readRawDataProviderIndex());
    }

    #! Reads a validated routing sidecar when available, otherwise reads the canonical index
    static private *hash<DataProviderIndexInfo> readInitialDataProviderIndex() {
        *hash<DataProviderIndexInfo> rv = readRoutingDataProviderIndex();
        if (rv) {
            raw_metadata_deferred = True;
            return rv;
        }
        return readRawDataProviderIndex();
    }

    #! Writes the compact routing sidecar for a canonical data provider index
    static private writeRoutingDataProviderIndex(*hash<DataProviderIndexInfo> h, string data_sha256,
            *string cache_sha256) {
        hash<auto> routing = getRoutingIndexPayload(h);
        hash<auto> sidecar = routing + {
            "version": RoutingIndexVersion,
            "data_sha256": data_sha256,
        };
        if (cache_sha256) {
            sidecar.metadata_cache_version = MetadataCacheVersion;
            sidecar.metadata_cache_sha256 = cache_sha256;
        }
        sidecar.routing_sha256 =
            digest("sha256", make_yaml(getRoutingIndexChecksumPayload(sidecar), BlockStyle)).toHex();
        writeIndex(sidecar, "routing", 0s, {});
    }

    #! Reads and validates the compact routing sidecar
    static private *hash<DataProviderIndexInfo> readRoutingDataProviderIndex() {
        remove metadata_cache_sha256;
        try {
            *hash<DataProviderRoutingIndexInfo> routing =
                cast<*hash<DataProviderRoutingIndexInfo>>(readIndex("routing", True));
            if (!routing || routing.version != RoutingIndexVersion) {
                return;
            }

            if (routing.routing_sha256
                    != digest("sha256", make_yaml(getRoutingIndexChecksumPayload(routing), BlockStyle)).toHex()) {
                return;
            }

            string data_yaml = File::readTextFile(getIndexPath("data"));
            if (routing.data_sha256 != digest("sha256", data_yaml).toHex()) {
                return;
            }

            if (routing.metadata_cache_version == MetadataCacheVersion
                    && routing.metadata_cache_sha256) {
                metadata_cache_sha256 = routing.metadata_cache_sha256;
            }

            return <DataProviderIndexInfo>{
                "factorymap": routing.factorymap ?? {},
                "appmap": routing.appmap ?? {},
                "schememap": routing.schememap ?? {},
                "appinfomap": {},
                "actioninfomap": {},
                "top_actioninfomap": {},
            };
        } catch (hash<ExceptionInfo> ex) {
            return;
        }
    }

    #! Returns the canonical routing payload for sidecar checksumming
    static private hash<auto> getRoutingIndexPayload(*hash<auto> h) {
        return {
            "factorymap": h.factorymap ?? {},
            "appmap": h.appmap ?? {},
            "schememap": h.schememap ?? {},
        };
    }

    #! Returns the payload covered by the routing sidecar checksum
    static private hash<auto> getRoutingIndexChecksumPayload(*hash<auto> h) {
        hash<auto> rv = getRoutingIndexPayload(h) + {
            "version": h.version,
            "data_sha256": h.data_sha256,
        };
        if (exists h.metadata_cache_version) {
            rv.metadata_cache_version = h.metadata_cache_version;
        }
        if (exists h.metadata_cache_sha256) {
            rv.metadata_cache_sha256 = h.metadata_cache_sha256;
        }
        return rv;
    }

    #! Reads the serialized data provider index without reconstructing typed metadata
    static private *hash<DataProviderIndexInfo> readRawDataProviderIndex() {
        *hash<DataProviderIndexInfo> cache = readMetadataDataProviderIndex();
        if (cache) {
            return cache;
        }
        try {
            return cast<*hash<DataProviderIndexInfo>>(readIndex("data"));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
        }
    }

    #! Reads deferred metadata and falls back to the complete canonical index on any cache error
    static private hash<RawMetadataIndexResult> readDeferredRawDataProviderIndex(*string app) {
        *hash<DataProviderIndexInfo> cache = readMetadataDataProviderIndex(app);
        if (cache) {
            return <RawMetadataIndexResult>{
                "index": cache,
                "complete": !app,
            };
        }

        *hash<DataProviderIndexInfo> canonical;
        try {
            canonical = cast<*hash<DataProviderIndexInfo>>(readIndex("data"));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s",
                get_exception_string(ex));
        }
        return <RawMetadataIndexResult>{
            "index": canonical,
            "complete": True,
        };
    }

    #! Reads and validates app-specific chunks from the MessagePack metadata cache
    static private *hash<DataProviderIndexInfo> readMetadataDataProviderIndex(*string app = NOTHING) {
%ifdef NoMsgPack
        return;
%else
        *hash<DataProviderMetadataCacheManifest> manifest = readMetadataCacheManifest();
        if (!manifest) {
            return;
        }

        hash<DataProviderIndexInfo> rv = createEmptyDataProviderIndex();
        list<string> apps = app ? (app,) : keys manifest.apps;
        foreach string chunk_app in (apps) {
            if (!exists manifest.apps{chunk_app}) {
                continue;
            }
            *hash<DataProviderIndexInfo> chunk = readMetadataCacheChunk(chunk_app, manifest.apps{chunk_app});
            if (!chunk) {
                return;
            }
            rv.appinfomap += chunk.appinfomap ?? {};
            rv.actioninfomap += chunk.actioninfomap ?? {};
        }
        return rv;
%endif
    }

    #! Reads and validates the metadata cache manifest once per process
    static private *hash<DataProviderMetadataCacheManifest> readMetadataCacheManifest() {
%ifdef NoMsgPack
        return;
%else
        if (metadata_cache_manifest_checked) {
            return metadata_cache_manifest;
        }
        metadata_cache_manifest_checked = True;
        if (!metadata_cache_sha256) {
            return;
        }
        try {
            binary data = ReadOnlyFile::readBinaryFile(getMetadataCachePath());
            if (digest("sha256", data).toHex() != metadata_cache_sha256) {
                return;
            }
            *hash<DataProviderMetadataCacheManifest> manifest =
                cast<*hash<DataProviderMetadataCacheManifest>>(msgpack_unpack(data, MSGPACK_QORE_MODE));
            if (!manifest || manifest.version != MetadataCacheVersion) {
                return;
            }
            metadata_cache_manifest = manifest;
            return manifest;
        } catch (hash<ExceptionInfo> ex) {
            return;
        }
%endif
    }

    #! Reads and validates one app-specific metadata cache chunk
    static private *hash<DataProviderIndexInfo> readMetadataCacheChunk(string app,
            hash<DataProviderMetadataCacheChunkInfo> info) {
%ifdef NoMsgPack
        return;
%else
        try {
            binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath(app));
            if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
                return;
            }
            return cast<*hash<DataProviderIndexInfo>>(msgpack_unpack(data, MSGPACK_QORE_MODE));
        } catch (hash<ExceptionInfo> ex) {
            return;
        }
%endif
    }

    #! Reconstructs typed app and action metadata in a serialized data provider index
    static private *hash<DataProviderIndexInfo> deserializeDataProviderIndex(*hash<DataProviderIndexInfo> rv) {
        if (!rv) {
            return;
        }

        hash<MetadataDeserializeContext> ctx = createMetadataDeserializeContext();

        try {
            if (rv.appinfomap) {
                rv.appinfomap = map {
                    $1.key: deserializeAppInfo($1.value, ctx),
                }, rv.appinfomap.pairIterator();
            }
            if (rv.actioninfomap) {
                rv.actioninfomap = deserializeActionsWithContext(rv.actioninfomap, ctx);
                foreach hash<auto> i in (rv.actioninfomap.pairIterator()) {
                    map rv.top_actioninfomap{i.key + "/" + $1.key} = $1.value, i.value.pairIterator();
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
        }
        return rv;
    }

    #! Creates the shared context used to reconstruct typed provider metadata
    static private hash<MetadataDeserializeContext> createMetadataDeserializeContext() {
        # dynamically load the DataProvider module; cannot be required or it would cause a circular dependency
        load_module("DataProvider");
        load_module("MapperUtil");

        return <MetadataDeserializeContext>{
            "app_info_th": TypedHash::forName("DataProvider::DataProviderAppInfo"),
            "conn_opt_th": TypedHash::forName("ConnectionProvider::ConnectionOptionInfo"),
            "action_info_th": TypedHash::forName("DataProvider::DataProviderActionInfo"),
            "option_info_th": TypedHash::forName("DataProvider::ActionOptionInfo"),
            "path_vars_th": TypedHash::forName("DataProvider::DataProviderPathVarInfo"),
            "dp_info_th": TypedHash::forName("DataProvider::DataProviderInfo"),
            "dp_opt_info_th": TypedHash::forName("DataProvider::DataProviderOptionInfo"),
            "dp_exp_info_th": TypedHash::forName("DataProvider::DataProviderExpressionInfo"),
            "mapper_key_info_th": TypedHash::forName("Mapper::MapperRuntimeKeyInfo"),
            "dp_msg_info_th": TypedHash::forName("DataProvider::DataProviderMessageInfo"),
            "dp_sig_info_th": TypedHash::forName("DataProvider::DataProviderSignatureTypeInfo"),
            "dp_arg_info_th": TypedHash::forName("DataProvider::DataProviderArgInfo"),
            "type_map": Constant::forName("DataProvider::AbstractDataProviderTypeMap").getValue(),
            "shape_map": getDataProviderShapeMap(),
            "ui_view_map": Constant::forName("DataProvider::UiOptionViewMap").getValue(),
        };
    }

    #! Reconstructs one typed app metadata hash
    static private hash<auto> deserializeAppInfo(hash<auto> info, hash<MetadataDeserializeContext> ctx) {
        return ctx.app_info_th.doCastWithOverrides(info, (
            info.connection_options
            ? {
                "connection_options": ctx.conn_opt_th.doCastValuesWithOverrides(info.connection_options, map {
                    $1.key: {
                        "allowed_values": (exists $1.value.allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                            : NOTHING),
                        "element_allowed_values": (exists $1.value.element_allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                            : NOTHING),
                    },
                }, info.connection_options.pairIterator()),
            }
            : {}
        ));
    }

    #! Reconstructs typed action metadata using a prepared context
    static private hash<auto> deserializeActionsWithContext(
            hash<string, hash<string, hash<auto>>> actioninfomap,
            hash<MetadataDeserializeContext> ctx) {
        return deserializeActions(actioninfomap, ctx.action_info_th, ctx.option_info_th, ctx.path_vars_th,
            ctx.dp_info_th, ctx.dp_opt_info_th, ctx.dp_exp_info_th, ctx.mapper_key_info_th,
            ctx.dp_msg_info_th, ctx.dp_sig_info_th, ctx.dp_arg_info_th, ctx.type_map,
            ctx.shape_map, ctx.ui_view_map);
    }

    #! Deserialize action options
    static private hash<auto> deserializeActions(hash<string, hash<string, hash<auto>>> actioninfomap,
            TypedHash action_info_th, TypedHash option_info_th, TypedHash path_vars_th, TypedHash dp_info_th,
            TypedHash dp_opt_info_th, TypedHash dp_exp_info_th, TypedHash mapper_key_info_th,
            TypedHash dp_msg_info_th, TypedHash dp_sig_info_th, TypedHash dp_arg_info_th, hash<auto> type_map,
            hash<auto> shape_map, *hash<auto> ui_view_map) {
        hash<auto> rv;
        foreach hash<auto> i in (actioninfomap.pairIterator()) {
            hash<auto> overrides;
            foreach hash<auto> j in (i.value.pairIterator()) {
                on_error rethrow $1.err, sprintf("%s (app %y action %y)", $1.desc, i.key, j.key);
                overrides{j.key} = {
                    "path_vars": (j.value.path_vars
                        ? path_vars_th.doCastValues(j.value.path_vars)
                        : NOTHING),
                    "options": (exists j.value.options ? option_info_th.doCastValuesWithOverrides(
                        j.value.options, map {
                        $1.key: {
                            "allowed_values": (exists $1.value.allowed_values
                                ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                                : NOTHING),
                            "element_allowed_values": (exists $1.value.element_allowed_values
                                ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                                : NOTHING),
                            "type": type_map{$1.value.type} ?? type_map."auto",
                            "default_view": ui_view_map{$1.value.default_view},
                        }
                    }, j.value.options.pairIterator()) : NOTHING),
                    "data_provider_info": (j.value.data_provider_info
                        ? deserializeDataProviderInfo(j.value.data_provider_info, dp_info_th,
                            dp_opt_info_th, dp_exp_info_th, mapper_key_info_th, dp_msg_info_th,
                            dp_sig_info_th, dp_arg_info_th, type_map, shape_map)
                        : NOTHING),
                };
            }
            on_error rethrow $1.err, sprintf("%s (app %y)", $1.desc, i.key);
            rv{i.key} = action_info_th.doCastValuesWithOverrides(i.value, overrides);
        }
        return rv;
    }

    #! Deserialize data provider info
    static private *hash<auto> deserializeDataProviderInfo(*hash<auto> info, TypedHash dp_info_th,
            TypedHash dp_opt_info_th, TypedHash dp_exp_info_th, TypedHash mapper_key_info_th,
            TypedHash dp_msg_info_th, TypedHash dp_sig_info_th, TypedHash dp_arg_info_th, hash<auto> type_map,
            hash<auto> shape_map) {
        if (!info) {
            return;
        }
        hash<auto> rv;
        foreach hash<auto> i in (info.pairIterator()) {
            if (i.key == "bulk_read_shapes" || i.key == "bulk_create_shapes" || i.key == "bulk_upsert_shapes") {
                rv{i.key} = map deserializeDataProviderShape($1, shape_map), i.value;
            } else {
                rv{i.key} = i.value;
            }
        }
        foreach string key in ("constructor", "create", "upsert", "search", "request", "child_create", "child_delete",
            "add_field", "update_field", "delete_field", "send_message") {
            key += "_options";
            if (rv{key}) {
                rv{key} = dp_opt_info_th.doCastValuesWithOverrides(rv{key}, map {
                    $1.key: {
                        "allowed_values": (exists $1.value.allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                            : NOTHING),
                        "element_allowed_values": (exists $1.value.element_allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                            : NOTHING),
                        "type": (map type_map{$1} ?? type_map."auto", $1.value.type),
                    },
                }, rv{key}.pairIterator());
            }
        }
        return dp_info_th.doCastWithOverrides(rv, {
            "expressions": (exists rv.expressions ? dp_exp_info_th.doCastValuesWithOverrides(
                rv.expressions, map {
                $1.key: {
                    "args": (exists $1.value.args ? map dp_sig_info_th.doCastWithOverrides($1, {
                        "type": type_map{$1.type} ?? type_map."auto",
                    }), $1.value.args : NOTHING),
                    "arg_info": (exists $1.value.arg_info ? map dp_arg_info_th.doCastWithOverrides($1, {
                        "allowed_values": (exists $1.allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.allowed_values
                            : NOTHING),
                    }), $1.value.arg_info : NOTHING),
                    "return_type": type_map{$1.value.return_type} ?? type_map."auto",
                }
            }, rv.expressions.pairIterator()) : NOTHING),
            "events": (exists rv.events ? dp_msg_info_th.doCastValues(rv.events) : NOTHING),
            "mapper_keys": (exists rv.mapper_keys ? mapper_key_info_th.doCastValues(rv.mapper_keys) : NOTHING),
            "messages": (exists rv.messages ? dp_msg_info_th.doCastValues(rv.messages) : NOTHING),
        });
    }

    #! Returns the map used to deserialize data provider shapes from the provider index
    static private hash<auto> getDataProviderShapeMap() {
        return {
            "RowRecords": Constant::forName("DataProvider::DataProviderShape::RowRecords").getValue(),
            "1": Constant::forName("DataProvider::DataProviderShape::RowRecords").getValue(),
            "HashOfLists": Constant::forName("DataProvider::DataProviderShape::HashOfLists").getValue(),
            "2": Constant::forName("DataProvider::DataProviderShape::HashOfLists").getValue(),
            "BufferColumns": Constant::forName("DataProvider::DataProviderShape::BufferColumns").getValue(),
            "3": Constant::forName("DataProvider::DataProviderShape::BufferColumns").getValue(),
            "DataFrameBlock": Constant::forName("DataProvider::DataProviderShape::DataFrameBlock").getValue(),
            "4": Constant::forName("DataProvider::DataProviderShape::DataFrameBlock").getValue(),
        };
    }

    #! Deserialize a data provider shape from the provider index
    static private auto deserializeDataProviderShape(auto shape, hash<auto> shape_map) {
        if (exists shape_map{shape.toString()}) {
            return shape_map{shape.toString()};
        }
        throw "PROVIDER-INDEX-ERROR", sprintf("unknown data provider shape %y", shape);
    }

    #! Writes the index to the configured directory
    static private hash<ProviderIndexInfo> writeIndex(*hash<auto> h, string type, date delta, hash<auto> summary) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", sprintf("YAML support is not available; cannot write %s provider index", type);
%else
        string path = getIndexPath(type);
        string yaml = make_yaml(h, BlockStyle);
        writeFileAtomic(path, yaml);
        return <ProviderIndexInfo>{
            "path": path,
            "size": yaml.size(),
            "delta": delta,
            "summary": summary,
        };
%endif
    }

    #! Writes the given contents to the given path atomically
    /** The contents are written to a temporary file in the target directory, flushed to disk, and then renamed
        over the target path, so that the target file is never truncated or left partially written; if the process
        crashes or is killed at any point, any existing file at the target path is left untouched

        @param path the target path
        @param contents the contents to write
    */
    static private writeFileAtomic(string path, data contents) {
        string tmp_path = sprintf("%s.tmp.%d.%d", path, getpid(), gettid());
        # ensure that the temporary file is removed if the write or the rename fails
        on_error {
            try {
                unlink(tmp_path);
            } catch (hash<ExceptionInfo> ex) {
                # ignore errors removing the temporary file; the original error must be propagated
            }
        }
        {
            File f();
            f.open2(path: tmp_path, flags: O_CREAT | O_TRUNC | O_WRONLY, mode: IndexFileMode);
            f.write(contents);
            # ensure that the data is on disk before the file is renamed over the target
            f.sync();
        }
        rename(tmp_path, path);
    }

    #! Reads the index from the configured directory
    static private *hash<auto> readIndex(string type, bool quiet = False) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", sprintf("YAML support is not available; cannot read %s provider index", type);
%else
        # Skip silently when the index env var is not configured — this is
        # the default in tests and non-Qorus deployments, not an error.
        # Check the env directly to avoid tryGetIndexDir()'s WARN log.
        if (!ENV{ProviderIndexEnvVar}.val()) {
            return;
        }
        *string path;
        try {
            path = getIndexPath(type);
            *hash<auto> rv = parse_yaml(File::readTextFile(path));
            if (type == "data") {
                remove index_read_error;
            }
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            # an index that has not been created yet is not an error; anything else (ex: a permissions
            # problem or a corrupt file) is recorded so that callers can detect the degraded state
            bool missing = ex.arg == ENOENT;
            if (type == "data") {
                if (missing) {
                    remove index_read_error;
                } else {
                    index_read_error = sprintf("%s: %s", ex.err, ex.desc);
                }
            }
            if (!quiet) {
                if (missing) {
                    # the normal state before the first write; reporting it as an error would make every
                    # fresh deployment look broken
                    log(LoggerLevel::LevelInfo, "No %s provider index at %y yet", type, path ?? "<unknown>");
                } else {
                    # the file is named because without it neither a permissions problem nor a corrupt file
                    # can be acted on
                    log(LoggerLevel::LevelError, "Ignoring error reading the %s provider index %y: %s: %s",
                        type, path ?? "<unknown>", ex.err, ex.desc);
                }
            }
        }
%endif
    }

    #! Returns the index path for the given type
    static string getIndexPath(string type) {
        return sprintf("%s%sqore-%s-index.yaml", tryGetIndexDir(True), DirSep, type);
    }

    #! Returns the optional data provider metadata cache path
    static private string getMetadataCachePath() {
        return sprintf("%s%sqore-data-index.msgpack", tryGetIndexDir(True), DirSep);
    }

    #! Returns the app-specific data provider metadata cache chunk path
    static private string getMetadataCacheChunkPath(string app) {
        return sprintf("%s%sqore-data-index-%s.msgpack", tryGetIndexDir(True), DirSep,
            digest("sha256", app).toHex());
    }

    #! Sets the environment variable
    static setEnvironmentVariable(string dir) {
        setenv(ProviderIndexEnvVar, dir);
        ENV{ProviderIndexEnvVar} = dir;
    }

    #! Try to get the index directory from the environment variable
    /** @param throw_exception if @ref True then errors result in a \c PROVIDER-INDEX-ERROR exception, otherwise
        they are logged as warnings and @ref nothing is returned
        @param for_write if @ref True then the directory is also verified to be readable, writable, and traversable
        by the current user; this must be used before any index files are written, so that a permissions problem
        results in an immediate and actionable error instead of a failure after the index has been built

        @return the index directory
    */
    static *string tryGetIndexDir(*bool throw_exception, *bool for_write) {
        *string dir = ENV{ProviderIndexEnvVar};
        if (!dir.val()) {
            string err = sprintf("Environment variable %y is not set; provider index APIs cannot be used",
                ProviderIndexEnvVar);
            if (throw_exception) {
                throw "PROVIDER-INDEX-ERROR", err;
            }
            log(LoggerLevel::LevelWarn, "%s", err);
            return;
        }
        dir =~ s/[\/\\]+$//;
        dir = normalize_dir(dir);
        *hash<StatInfo> info = hstat(dir);
        if (!info) {
            try {
                # NOTE: the directory must be created with the execute bit set, otherwise it cannot be traversed,
                # and no index files in it can be read or written
                mkdir_ex(dir, IndexDirMode, True);
            } catch (hash<ExceptionInfo> ex) {
                string err = sprintf("Cannot create provider index directory %y (from environment variable %y): "
                    "%s: %s", dir, ProviderIndexEnvVar, ex.err, ex.desc);
                if (throw_exception) {
                    rethrow "PROVIDER-INDEX-ERROR", err;
                }
                log(LoggerLevel::LevelWarn, "%s", err);
                return;
            }
        } else if (info.type != "DIRECTORY") {
            string err = sprintf("Environment variable %y is pointing to %y with type %y; must be a directory",
                ProviderIndexEnvVar, dir, info.type);
            if (throw_exception) {
                throw "PROVIDER-INDEX-ERROR", err;
            }
            log(LoggerLevel::LevelWarn, "%s", err);
            return;
        }
        if (for_write) {
            if (*string err = checkIndexDirAccess(dir)) {
                if (throw_exception) {
                    throw "PROVIDER-INDEX-ERROR", err;
                }
                log(LoggerLevel::LevelWarn, "%s", err);
                return;
            }
        }
        return dir;
    }

    #! Returns the index directory after verifying that index files can be written to it
    /** @return the index directory

        @throw PROVIDER-INDEX-ERROR the environment variable is not set, or the directory cannot be created, or the
        directory is not readable, writable, and traversable by the current user
    */
    static string getWritableIndexDir() {
        return tryGetIndexDir(True, True);
    }

    #! Checks that the given directory can be used for writing index files
    /** @param dir the directory to check

        @return an actionable error description if the directory cannot be used, otherwise @ref nothing
    */
    static private *string checkIndexDirAccess(string dir) {
        # NOTE: is_writable() on a directory verifies that a file can actually be created in it, which requires
        # both the write and the execute (traverse) bits
        if (is_readable(dir) && is_writable(dir)) {
            return;
        }
        *hash<StatInfo> info = hstat(dir);
        return sprintf("Provider index directory %y (from environment variable %y) is not readable, writable, and "
            "traversable by the current user (uid %d, gid %d); the directory has permissions %y (owner uid %d, "
            "gid %d); fix the permissions (ex: \"chmod %s %s\") and retry", dir, ProviderIndexEnvVar, getuid(),
            getgid(), info.perm ?? "<unknown>", info.uid ?? -1, info.gid ?? -1,
            sprintf("%o", IndexDirMode), dir);
    }

    #! Log a message to the global logger if possible
    static log(Qore::Logger::LoggerLevel level, string fmt, ...) {
        logArgs(level, fmt, argv);
    }

    #! Log a message to the global logger if possible; falls back to stderr for errors
    static logArgs(Qore::Logger::LoggerLevel level, string fmt, *softlist<auto> args) {
        if (logger) {
            logger.logArgs(level, fmt, args);
        } else if (level.getValue() >= LoggerLevel::LevelError.getValue()) {
            # ensure errors are never silently dropped when the logger is not yet configured
            stderr.printf("ProviderIndexUtil %s: %s\n", level.getStr(), vsprintf(fmt, args));
        }
    }
}
}
