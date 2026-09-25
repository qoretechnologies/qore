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

#! The environment variable that starts a process in index build mode
/** If set to a true value (ex: \c 1 or \c true), the process starts in index build mode, as if
    @ref ProviderIndexUtil::ProviderIndexUtil::setBuildMode() "ProviderIndexUtil::setBuildMode(True)" had been called
    before any provider module was loaded

    @since ProviderIndexUtil 1.2
*/
public const ProviderIndexBuildEnvVar = "QORE_PROVIDER_INDEX_BUILD";

#! The environment variable that makes index builds also write the canonical index file
/** If set to a true value (ex: \c 1 or \c true), every index written also includes the canonical index file
    (\c qore-data-index.yaml), as if
    @ref ProviderIndexUtil::ProviderIndexUtil::writeDataProviderIndex() "ProviderIndexUtil::writeDataProviderIndex()"
    had been called with \c write_canonical_yaml set; it is only a diagnostic and transition aid, since no reader
    needs the file

    @since ProviderIndexUtil 1.2
*/
public const ProviderIndexCanonicalYamlEnvVar = "QORE_PROVIDER_INDEX_CANONICAL_YAML";

#! The environment variable that sets the largest canonical index file that is read
/** The value is a size in bytes; if unset or not a positive integer,
    @ref ProviderIndexUtil::DefaultMaxCanonicalIndexSize "DefaultMaxCanonicalIndexSize" is used

    @since ProviderIndexUtil 1.2
*/
public const ProviderIndexMaxCanonicalSizeEnvVar = "QORE_PROVIDER_INDEX_MAX_CANONICAL_BYTES";

#! The default size in bytes of the largest canonical index file that is read (128 MiB)
/** The canonical index file is parsed in full, which takes several times its size in memory, so a larger file is
    reported as an index read error with the \c PROVIDER-INDEX-TOO-LARGE code instead of being parsed

    @see ProviderIndexMaxCanonicalSizeEnvVar

    @since ProviderIndexUtil 1.2
*/
public const DefaultMaxCanonicalIndexSize = 128 * 1024 * 1024;

#! Version 3 records whether a canonical index file was published with the routing index
const RoutingIndexVersion = 3;
#! Version 5 names each metadata cache file after the digest of its contents, so that publishing an index never
#! changes a file that the previous index references
const MetadataCacheVersion = 5;
#! The earliest routing index and metadata cache versions that are read; they name the metadata cache files after
#! their apps
const MinRoutingIndexVersion = 2;
const MinMetadataCacheVersion = 4;

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

    #! App to module-name map; readers also accept explicit paths from legacy indexes
    *hash<string, hash<string, bool>> appmap;

    #! Scheme to module-name map; readers also accept explicit paths from legacy indexes
    *hash<string, hash<string, bool>> schememap;

    #! App to app info map
    *hash<string, hash<auto>> appinfomap;

    #! Maps apps to actions to action info hashes
    /** When read, values are \c DataProvider::DataProviderIndexedActionInfo hashes.

        @note Dynamic action callbacks and output types are omitted.  Since ProviderIndex 1.2, each option's type
        is described by plain data in \c type_info; readers convert primitive type names from earlier indexes.
    */
    *hash<string, hash<string, hash<auto>>> actioninfomap;

    #! Maps app actions to action info hashes
    /** When read, values are \c DataProvider::DataProviderIndexedActionInfo hashes.
    */
    *hash<string, hash<auto>> top_actioninfomap;
}

#! Index info
public hashdecl ProviderIndexInfo {
    #! Path to the index file
    /** The canonical index file (\c qore-data-index.yaml) if it was written, otherwise the routing index
        (\c qore-routing-index.yaml), which identifies the published index
    */
    string path;

    #! Size of the index file
    /** The size of the canonical index file if it was written, otherwise the total size of the routing index and
        the metadata cache files
    */
    int size;

    #! Elapsed time to create the Index
    date delta;

    #! Summary info for the indexing operation
    hash<auto> summary;
}

#! A published data provider index read from its routing index and metadata cache
/** @since ProviderIndexUtil 1.2
*/
public hashdecl SerializedDataProviderIndexInfo {
    #! The \c factorymap, \c appmap, \c schememap, \c appinfomap, and \c actioninfomap members of the index
    /** App and action metadata is in the serialized form stored in the index, as in the YAML written by
        @ref ProviderIndexUtil::dumpIndexYaml() "ProviderIndexUtil::dumpIndexYaml()"
    */
    *hash<auto> index;

    #! The SHA-256 digest of the routing index
    /** The routing index holds the index maps and records the digests of the metadata cache manifest, which
        records the digest of each app's metadata, and of any canonical index file published with it, so this
        digest identifies the complete index
    */
    string sha256;

    #! The total size in bytes of the files read
    int size;
}

#! Internal context for reconstructing typed provider metadata
hashdecl MetadataDeserializeContext {
    TypedHash app_info_th;
    TypedHash conn_opt_th;
    TypedHash action_info_th;
    TypedHash option_info_th;
    TypedHash type_info_th;
    TypedHash field_info_th;
    TypedHash type_opt_info_th;
    TypedHash constraint_info_th;
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
    # the digest of the canonical file, if one was published with the routing index
    *string data_sha256;
    # the size of the canonical file; while it is unchanged and the file is not newer than the routing index, the
    # canonical file is not read to verify its digest
    *int data_size;
    # set if a canonical file was published with the routing index (version 3); a version 2 routing index was
    # always published with one
    *bool canonical_yaml;
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
    # the app's serialized info, so that listing the apps never reads the chunks (manifest version 4)
    *hash<auto> info;
}

#! Versioned manifest for app-specific metadata cache chunks
hashdecl DataProviderMetadataCacheManifest {
    int version;
    hash<string, DataProviderMetadataCacheChunkInfo> apps;
}

#! A published index's routing index and metadata cache manifest, verified
hashdecl CompactIndexInfo {
    hash<DataProviderRoutingIndexInfo> routing;
    # the routing index as read, whose digest identifies the index
    binary routing_data;
    hash<DataProviderMetadataCacheManifest> manifest;
    # the total size of the routing index and the manifest
    int size;
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
        #! Set when the info of every app has been loaded from the metadata cache; their actions are still deferred
        static bool raw_metadata_apps_loaded;

        #! Validated manifest digest set only by serialized index initialization and immutable after initialization
        static *string metadata_cache_sha256;
        #! The version of the validated manifest, which determines the names of the metadata cache files
        static *int metadata_cache_version;
        static *hash<DataProviderMetadataCacheManifest> metadata_cache_manifest;
        static bool metadata_cache_manifest_checked;

        #! All apps loaded flag
        static bool apps_loaded;

        #! Set of modules that have been successfully loaded; used to avoid calling load_module() which acquires
        #! the global module lock even when a module is already loaded
        static hash<string, bool> loaded_module_cache;

        #! The last error encountered when reading the published data provider index, if any
        /** This is set when the index exists but could not be read (ex: a permissions problem or a corrupt file);
            it is not set when no index has been created yet
        */
        static *string index_read_error;

        #! Index build mode flag; while set, the published index is never read
        static bool build_mode = parse_boolean(ENV{ProviderIndexBuildEnvVar});
    }

    #! Enters or leaves index build mode
    /** A process building the data provider index loads every provider it indexes, so it never needs the published
        index; reading it would only load the provider modules it lists and hold its metadata in memory while the
        index that replaces it is built.  In build mode, lookups behave as if no index had been published: app,
        factory, and scheme lookups load no modules from the index, and no cached app or action metadata is read
        from it.

        The in-memory index state is discarded whenever the mode changes, so that entering build mode releases any
        index already read, and leaving it lets the next lookup read the index that has just been published.

        @param build @ref True to enter build mode, @ref False to leave it

        @return the previous mode

        @par Example:
        @code{.py}
bool build_mode = ProviderIndexUtil::setBuildMode(True);
on_exit ProviderIndexUtil::setBuildMode(build_mode);
        @endcode

        @note @ref ProviderIndex::ProviderIndex::createDataProviderIndex() "ProviderIndex::createDataProviderIndex()"
        enters build mode itself; an embedder that loads provider modules before it, or that loads modules whose
        initialization looks up providers, should enter build mode first, or set the @ref ProviderIndexBuildEnvVar
        environment variable for the process

        @since ProviderIndexUtil 1.2
    */
    static bool setBuildMode(bool build) {
        m.lock();
        on_exit m.unlock();
        bool rv = build_mode;
        if (build != build_mode) {
            # do not discard the state while another thread is reading the index into it
            while (initializing || raw_metadata_loading || metadata_initializing || app_metadata_initializing
                    || action_metadata_initializing) {
                if (initializing_tid == gettid() || raw_metadata_loading_tid == gettid()
                        || metadata_initializing_tid == gettid()) {
                    throw "PROVIDER-INDEX-ERROR", "cannot change the index build mode while the data provider "
                        "index is being read in the same thread";
                }
                if (initializing) {
                    init_cond.wait(m);
                } else {
                    metadata_init_cond.wait(m);
                }
            }
            build_mode = build;
            resetIndexState();
        }
        return rv;
    }

    #! Returns @ref True if MessagePack support is available
    /** The compact index files (the routing index and the metadata cache) and the results of index workers are
        written with MessagePack; without it, only the canonical index file is written, and an index is built in the
        calling process.

        MessagePack support is available if the \c msgpack module was available when this module was parsed or, for
        a compiled module, compiled.

        @return @ref True if MessagePack support is available

        @since ProviderIndexUtil 1.2
    */
    static bool hasMessagePack() {
%ifdef NoMsgPack
        return False;
%else
        return True;
%endif
    }

    #! Returns @ref True if the process is in index build mode
    /** @return @ref True if the process is in index build mode, in which case the published index is never read

        @see setBuildMode()

        @since ProviderIndexUtil 1.2
    */
    static bool inBuildMode() {
        return build_mode;
    }

    #! Discards the in-memory index state; the next lookup initializes it again; must be called with the lock held
    static private resetIndexState() {
        remove index;
        initialized = False;
        raw_appinfomap = {};
        raw_actioninfomap = {};
        app_metadata_initialized = {};
        action_metadata_initialized = {};
        raw_metadata_deferred = False;
        raw_metadata_app_loaded = {};
        raw_metadata_apps_loaded = False;
        remove metadata_cache_sha256;
        remove metadata_cache_version;
        remove metadata_cache_manifest;
        metadata_cache_manifest_checked = False;
        apps_loaded = False;
    }

    #! Returns any error encountered while reading the published data provider index
    /** @return the description of the error encountered when trying to read the data provider index, if any, as
        \c "<code>: <description>"; @ref nothing is returned if the index was read successfully or if no index has
        been created yet

        Callers can use this to distinguish an unreadable or corrupt index (which silently degrades the set of
        known apps and factories) from a system where no index has been created at all.  The error is reported when
        the routing index, the metadata cache manifest, or the metadata of any app cannot be read or does not match
        the digest recorded for it, and when the canonical index file cannot be read, or is larger than
        @ref getMaxCanonicalIndexSize() (\c PROVIDER-INDEX-TOO-LARGE); the metadata of an app is read when it is
        first needed, so an error can be reported after the index has been initialized.

        @par Example:
        @code{.py}
if (*string err = ProviderIndexUtil::getIndexReadError()) {
    log(LoggerLevel::LevelError, "the data provider index is unavailable: %s", err);
}
        @endcode
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

    #! Returns the indexed actions of the given app
    /** Reading indexed actions never loads the provider modules that implement them: option types are described by
        plain data in each option's \c type_info, and output types are not indexed.

        @param app the app name

        @return the app's indexed actions as \c DataProvider::DataProviderIndexedActionInfo hashes; @ref nothing if
        the app has no indexed actions

        @par Example:
        @code{.py}
*list<hash<auto>> actions = ProviderIndexUtil::tryGetActionMetadataForApp("OpenSearch");
        @endcode

        @note An action whose metadata cannot be read is logged and omitted

        @note A type object can be created from an option's \c type_info with
        \c DataProvider::AbstractDataProviderType::get(); it has the described structure and constraints but not
        any custom behavior of the original type's class.  The exact types are available from the registered
        action with \c DataProvider::DataProviderActionCatalog::getAppAction(), which loads the app's modules.

        @see tryGetActionMetadata()

        @since ProviderIndexUtil 1.2
    */
    static *list<hash<auto>> tryGetActionMetadataForApp(string app) {
        checkActionMetadata(app);
        return index.actioninfomap{app} ? index.actioninfomap{app}.values() : NOTHING;
    }

    #! Returns the given indexed action
    /** @param app the app name
        @param action the action name

        @return the action as a \c DataProvider::DataProviderIndexedActionInfo hash; @ref nothing if the action is
        not indexed

        @par Example:
        @code{.py}
*hash<auto> action = ProviderIndexUtil::tryGetActionMetadata("OpenSearch", "search-index");
        @endcode

        @see tryGetActionMetadataForApp() for how option types are described

        @since ProviderIndexUtil 1.2
    */
    static *hash<auto> tryGetActionMetadata(string app, string action) {
        checkActionMetadata(app);
        return index.actioninfomap{app}{action};
    }

    #! Searches for cached application actions whose descriptions match a regular expression pattern
    /** @param app The application to search for actions in
        @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached actions that match the regular expression pattern in the \c short_desc or \c desc fields, as
        \c DataProvider::DataProviderIndexedActionInfo hashes

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

        @return All actions that match the substring argument in the \c short_desc or \c desc fields, as
        \c DataProvider::DataProviderIndexedActionInfo hashes

        @note If the application is not cached, no results are returned
    */
    static *list<hash<auto>> searchCachedAppActionSubstr(string app, string substr, bool ignore_case = True) {
        checkActionMetadata(app);
        if (ignore_case) {
            substr = substr.upr();
            return map $1, index.actioninfomap{app}.values(),
                (($1.short_desc ?? "").upr().find(substr) >= 0) || (($1.desc ?? "").upr().find(substr) >= 0);
        }
        return map $1, index.actioninfomap{app}.values(),
            (($1.short_desc ?? "").find(substr) >= 0) || (($1.desc ?? "").find(substr) >= 0);
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
                (($1.short_desc ?? "").upr().find(substr) >= 0) || (($1.desc ?? "").upr().find(substr) >= 0);
        }
        return map $1, index.top_actioninfomap.values(),
            (($1.short_desc ?? "").find(substr) >= 0) || (($1.desc ?? "").find(substr) >= 0);
    }

    #! Returns all cached actions
    /** @return all cached actions as \c DataProvider::DataProviderIndexedActionInfo hashes
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

    #! Adds the given action to the cache
    /** @param action a registered action as a \c DataProvider::DataProviderActionInfo hash; it is cached as a
        \c DataProvider::DataProviderIndexedActionInfo hash, like an indexed action
    */
    static addActionToCache(hash<auto> action) {
        checkAppMetadata(action.app);
        checkActionMetadata(action.app);
        if (!checkMetadataContext()) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action %y of app %y to the cache while the data "
                "provider index is initializing", action.action, action.app);
        }
        action = getIndexedAction(action, metadata_context);
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
        remove raw_actioninfomap{action.app}{action.action};
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
        map remove index.top_actioninfomap{app + "/" + $1}, keys (remove index.actioninfomap{app});
        remove raw_actioninfomap{app};
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
        hash<string, bool> loaded;
        foreach hash<auto> mod in (get_module_hash().values()) {
            loaded{mod.name} = True;
            if (mod.path.val()) {
                loaded{mod.path} = True;
            }
            if (mod.filename.val()) {
                loaded{mod.filename} = True;
            }
        }
        hash<string, bool> rv;
        foreach hash<string, bool> h in (index.appmap.iterator()) {
            foreach string dependency in (keys h) {
                if (!loaded{dependency}) {
                    rv{dependency} = True;
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

        bool build;
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
            build = build_mode;
        }

        *hash<DataProviderIndexInfo> new_index;
        try {
            # a process building the index never reads the index it replaces
            if (!build) {
                new_index = readInitialDataProviderIndex();
            }
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
            # rebuilt from the typed action metadata when it is restored
            new_index.top_actioninfomap = {};
            index = new_index;
        }
        initialized = True;
        initializing = False;
        remove initializing_tid;
        init_cond.broadcast();
    }

    #! Loads serialized metadata after routing-only initialization
    /** @param app an optional app name; when present, only that app's cache chunk is loaded
        @param actions if @ref False and no app is given, only the info of every app is loaded; the actions of each
        app remain deferred until they are needed, since the actions of all apps take many times the memory of
        their app info
    */
    static private checkRawMetadataIndex(*string app = NOTHING, bool actions = True) {
        checkDataIndex();
        if (!initialized || !raw_metadata_deferred || (app && raw_metadata_app_loaded{app})
                || (!app && !actions && raw_metadata_apps_loaded)) {
            return;
        }

        {
            m.lock();
            on_exit m.unlock();
            while (raw_metadata_loading && raw_metadata_loading_tid != gettid()) {
                metadata_init_cond.wait(m);
            }
            if (!raw_metadata_deferred || (app && raw_metadata_app_loaded{app})
                    || (!app && !actions && raw_metadata_apps_loaded)) {
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
            result = readDeferredRawDataProviderIndex(app, actions);
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
            # an app that was already loaded on its own may since have been restored, updated, or removed, so a
            # later load must not bring back its serialized entries; neither is serialized metadata kept for an app
            # or actions that have already been restored
            raw_appinfomap += map {$1.key: $1.value}, result.index.appinfomap.pairIterator(),
                !raw_metadata_app_loaded{$1.key} && !app_metadata_initialized{$1.key};
            # top_actioninfomap is rebuilt from the typed action metadata when it is restored; serialized
            # entries from the file are never published
            raw_actioninfomap += map {$1.key: $1.value}, result.index.actioninfomap.pairIterator(),
                !raw_metadata_app_loaded{$1.key} && !action_metadata_initialized{$1.key};
        }
        if (result.complete) {
            raw_metadata_deferred = False;
        } else if (app) {
            raw_metadata_app_loaded{app} = True;
        } else {
            raw_metadata_apps_loaded = True;
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
        # the actions of each app are loaded only when needed
        checkRawMetadataIndex(NOTHING, False);
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
            apps = select keys raw_actioninfomap, !action_metadata_initialized{$1};
            if (!apps) {
                return;
            }
            int tid = gettid();
            map action_metadata_initializing{$1} = tid, apps.iterator();
            raw_actions = map {$1: raw_actioninfomap{$1}}, apps.iterator();
        }

        hash<auto> action_map;
        try {
            action_map = deserializeActionsWithContext(raw_actions, metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing actions from the data provider index: %s",
                get_exception_string(ex));
            action_map = map {$1: {}}, apps.iterator();
        }

        m.lock();
        on_exit m.unlock();
        index.actioninfomap += action_map;
        foreach hash<auto> app_actions in (action_map.pairIterator()) {
            map index.top_actioninfomap{app_actions.key + "/" + $1.key} = $1.value,
                app_actions.value.pairIterator();
        }
        map remove raw_actioninfomap{$1}, apps.iterator();
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
    /** @param h the index
        @param delta the time taken to create the index
        @param summary summary information about the index
        @param write_canonical_yaml if @ref True, the canonical index file (\c qore-data-index.yaml) is written too;
        see @ref writeDataProviderIndexByApp()

        @return information about the written index

        @note The index is written one app at a time, so the complete serialized index is never held in memory

        @see writeDataProviderIndexByApp()
    */
    static hash<ProviderIndexInfo> writeDataProviderIndex(*hash<DataProviderIndexInfo> h, date delta,
            hash<auto> summary, *bool write_canonical_yaml) {
        *hash<string, hash<string, hash<auto>>> actioninfomap = h.actioninfomap;
        hash<auto> maps = h ? (h - ("actioninfomap", "top_actioninfomap")) : {};
        return writeDataProviderIndexByApp(maps, keys actioninfomap, *hash<string, hash<auto>> sub (string app) {
            return actioninfomap{app};
        }, delta, summary, write_canonical_yaml);
    }

    #! Writes the data provider index, retrieving the actions of each app only while that app is written
    /** The app-sharded metadata cache and the routing index are written in one pass over the apps, so that only one
        app's actions are held in memory at a time; apps and map entries are written in sorted order, so the same
        index is always written identically.

        The routing index and the metadata cache are the published index: the routing index holds the index maps
        and the digest of the metadata cache manifest, which holds the size and digest of each app's metadata.  Each
        metadata cache file is named after the digest of its contents, so publishing an index never changes a file
        that the previous index references; the routing index is replaced last, which publishes the new index
        atomically, and the files that it no longer references are then removed.

        The canonical index file (\c qore-data-index.yaml) holds the complete index as YAML; it is many times larger
        than the other files, and no reader needs it, so it is only written if requested, or if the
        @ref ProviderIndexCanonicalYamlEnvVar "QORE_PROVIDER_INDEX_CANONICAL_YAML" environment variable is set, or
        if MessagePack support is not available, since the metadata cache cannot be written without it; otherwise
        any canonical index file left by an earlier index is removed.  The same YAML can be produced from a
        published index with @ref dumpIndexYaml().

        @param maps the \c factorymap, \c appmap, \c schememap, and \c appinfomap members of the index
        @param action_apps the apps that have actions
        @param get_app_actions returns the actions of the given app, keyed by action name, in the serialized form
        stored in the index
        @param delta the time taken to create the index
        @param summary summary information about the index
        @param write_canonical_yaml if @ref True, the canonical index file is written too

        @return information about the written index

        @throw PROVIDER-INDEX-ERROR the index directory cannot be written to, or YAML support is not available, or
        the metadata cache or routing index cannot be written when no canonical index file is written

        @par Example:
        @code{.py}
# publish an index that also includes qore-data-index.yaml for a consumer that still reads it
hash<ProviderIndexInfo> info = ProviderIndexUtil::writeDataProviderIndexByApp(maps, keys actions,
    *hash<auto> sub (string app) { return actions{app}; }, now_us() - start, {}, True);
        @endcode

        @since ProviderIndexUtil 1.2
    */
    static hash<ProviderIndexInfo> writeDataProviderIndexByApp(hash<auto> maps, *list<string> action_apps,
            code get_app_actions, date delta, hash<auto> summary, *bool write_canonical_yaml) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", "YAML support is not available; cannot write the data provider index";
%else
        # verify that the target directory can be written to before writing anything
        getWritableIndexDir();

        bool canonical = write_canonical_yaml || parse_boolean(getenv(ProviderIndexCanonicalYamlEnvVar));
%ifdef NoMsgPack
        # the metadata cache is written with MessagePack, so without it the canonical index file is the only index
        canonical = True;
        bool write_cache = False;
%else
        bool write_cache = True;
%endif

        foreach string key in (("factorymap", "appmap", "schememap")) {
            if (exists maps{key}) {
                maps{key} = sortIndexMap(maps{key}, True);
            }
        }

        hash<string, bool> app_set = (map {$1: True}, keys maps.appinfomap) ?? {};
        map app_set{$1} = True, action_apps;
        hash<string, bool> action_app_set = (map {$1: True}, action_apps) ?? {};

        string path = getIndexPath("data");
        int size = 0;
        # metadata cache chunks; without a canonical index file, any chunk that cannot be written fails the write,
        # otherwise the optional cache is not written
        hash<string, DataProviderMetadataCacheChunkInfo> chunks = {};

        *File f;
        *string tmp_path;
        on_error if (tmp_path) {
            try {
                unlink(tmp_path);
            } catch (hash<ExceptionInfo> ex) {
                # ignore errors removing the temporary file; the original error must be propagated
            }
        }
        code<nothing(string)> write = sub (string str) {
            f.write(str);
            size += str.size();
        };
        if (canonical) {
            tmp_path = sprintf("%s.tmp.%d.%d", path, getpid(), gettid());
            f = new File();
            f.open2(path: tmp_path, flags: O_CREAT | O_TRUNC | O_WRONLY, mode: IndexFileMode);
            writeCanonicalMaps(write, maps);
        }

        bool actions_written;
        foreach string app in (sort(keys app_set)) {
            *hash<auto> actions;
            if (action_app_set{app}) {
                actions = sortIndexMap(get_app_actions(app), False);
            }
            if (canonical && exists actions) {
                writeCanonicalAppActions(write, app, actions, !actions_written);
                actions_written = True;
            }
%ifndef NoMsgPack
            if (write_cache) {
                try {
                    binary data = packMetadataCacheChunk(app, maps.appinfomap{app}, actions);
                    string sha256 = digest("sha256", data).toHex();
                    writeFileAtomic(getMetadataCacheChunkPath(app, sha256, MetadataCacheVersion), data);
                    chunks{app} = <DataProviderMetadataCacheChunkInfo>{
                        "size": data.size(),
                        "sha256": sha256,
                        "info": maps.appinfomap{app},
                    };
                } catch (hash<ExceptionInfo> ex) {
                    if (!canonical) {
                        rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider metadata of app %y: "
                            "%s: %s", app, ex.err, ex.desc);
                    }
                    log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                        get_exception_string(ex));
                    write_cache = False;
                }
            }
%endif
        }
        if (canonical) {
            if (!actions_written && action_apps) {
                write("actioninfomap: {}\n");
            }
            # ensure that the data is on disk before the file is renamed over the target
            f.sync();
            f.close();
            rename(tmp_path, path);
            remove tmp_path;
        }

        *string cache_sha256;
        int cache_size = 0;
        if (write_cache) {
            try {
                binary manifest = packMetadataCacheManifest(chunks);
                cache_sha256 = digest("sha256", manifest).toHex();
                writeFileAtomic(getMetadataCachePath(cache_sha256), manifest);
                cache_size = manifest.size() + ((foldl $1 + $2, (map $1.size, chunks.iterator())) ?? 0);
            } catch (hash<ExceptionInfo> ex) {
                if (!canonical) {
                    rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider metadata cache "
                        "manifest: %s: %s", ex.err, ex.desc);
                }
                remove cache_sha256;
                log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                    get_exception_string(ex));
            }
        }

        # the routing index is written last: replacing it publishes the new index
        int routing_size;
        try {
            if (canonical) {
                # the file is read as binary data, since converting it to a string would double the memory used
                routing_size = writeRoutingDataProviderIndex(maps, cache_sha256,
                    digest("sha256", ReadOnlyFile::readBinaryFile(path)).toHex(), hstat(path).size);
            } else {
                routing_size = writeRoutingDataProviderIndex(maps, cache_sha256);
            }
        } catch (hash<ExceptionInfo> ex) {
            if (!canonical) {
                rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider routing index: %s: %s",
                    ex.err, ex.desc);
            }
            log(LoggerLevel::LevelWarn, "Unable to write the optional data provider routing index: %s",
                get_exception_string(ex));
            return <ProviderIndexInfo>{
                "path": path,
                "size": size,
                "delta": delta,
                "summary": summary,
            };
        }

        removeUnreferencedIndexFiles(cache_sha256 ? chunks : NOTHING, cache_sha256, canonical);

        return <ProviderIndexInfo>{
            "path": canonical ? path : getIndexPath("routing"),
            "size": canonical ? size : routing_size + cache_size,
            "delta": delta,
            "summary": summary,
        };
%endif
    }

    #! Writes the \c factorymap, \c appmap, \c schememap, and \c appinfomap members of the canonical index file
    /** @param write writes a string to the output
        @param maps the maps; every map is written, even if empty, so that the same index is always written
        identically
        @param names the maps to write, in the order written
    */
    static private writeCanonicalMaps(code<nothing(string)> write, hash<auto> maps,
            list<string> names = ("factorymap", "appmap", "schememap", "appinfomap")) {
%ifndef NoYaml
        foreach string key in (names) {
            write(make_yaml({key: (key == "appinfomap" ? sortIndexMap(maps{key}, False) : maps{key}) ?? {}},
                BlockStyle));
        }
%endif
    }

    #! Writes the actions of one app to the \c actioninfomap member of the canonical index file
    /** @param write writes a string to the output
        @param app the app name
        @param actions the app's actions in the serialized form stored in the index, sorted by action name
        @param first @ref True if this is the first app written, in which case the \c actioninfomap key is written
        first
    */
    static private writeCanonicalAppActions(code<nothing(string)> write, string app, hash<auto> actions, bool first) {
%ifndef NoYaml
        if (first) {
            write("actioninfomap:\n");
        }
        # each app's entry is indented under the top-level key; strings are written in quoted flow style, so
        # indenting every line leaves their values unchanged
        string yaml = make_yaml({app: actions}, BlockStyle);
        write("  " + replace(yaml.substr(0, -1), "\n", "\n  ") + "\n");
%endif
    }

    #! Returns the given index map with its keys sorted
    /** @param h the map
        @param sort_values if @ref True, the keys of hash values are sorted too
    */
    static private *hash<auto> sortIndexMap(*hash<auto> h, bool sort_values) {
        if (!h) {
            return h;
        }
        hash<auto> rv = {};
        foreach string key in (sort(keys h)) {
            auto value = h{key};
            if (sort_values && value.typeCode() == NT_HASH) {
                value = map {$1: value{$1}}, sort(keys value);
            }
            rv{key} = value;
        }
        return rv;
    }

    #! Packs one app's metadata into a metadata cache chunk
    /** @param app the app name
        @param app_info the app's serialized info, if any
        @param actions the app's serialized actions, if any

        @return the compressed chunk

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available

        @since ProviderIndexUtil 1.2
    */
    static private binary packMetadataCacheChunk(string app, *hash<auto> app_info, *hash<auto> actions) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot write data provider metadata";
%else
        hash<DataProviderIndexInfo> app_index = createEmptyDataProviderIndex();
        if (exists app_info) {
            app_index.appinfomap{app} = app_info;
        }
        if (exists actions) {
            app_index.actioninfomap{app} = actions;
        }
        # type descriptions repeat many keys and values, so chunks compress by about 10 times
        return compress(msgpack_pack(app_index, MSGPACK_QORE_MODE));
%endif
    }

    #! Unpacks a metadata cache chunk
    /** @param data the compressed chunk

        @return the chunk's index, with the app's info and actions

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available

        @since ProviderIndexUtil 1.2
    */
    static private hash<DataProviderIndexInfo> unpackMetadataCacheChunk(binary data) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        return cast<hash<DataProviderIndexInfo>>(msgpack_unpack(uncompress_to_binary(data), MSGPACK_QORE_MODE));
%endif
    }

    #! Serializes plain data exactly, preserving Qore types
    /** @param value the value to serialize

        @return the serialized value

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available

        @since ProviderIndexUtil 1.2
    */
    static private binary packIndexData(auto value) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot write data provider metadata";
%else
        return msgpack_pack(value, MSGPACK_QORE_MODE);
%endif
    }

    #! Deserializes data serialized with packIndexData()
    /** @param data the serialized value

        @return the value

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available

        @since ProviderIndexUtil 1.2
    */
    static private auto unpackIndexData(binary data) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        return msgpack_unpack(data, MSGPACK_QORE_MODE);
%endif
    }

    #! Packs the metadata cache manifest
    /** @param chunks the chunks written

        @return the packed manifest
    */
    static private binary packMetadataCacheManifest(hash<string, DataProviderMetadataCacheChunkInfo> chunks) {
        return packIndexData(<DataProviderMetadataCacheManifest>{
            "version": MetadataCacheVersion,
            "apps": chunks,
        });
    }

    #! Removes the index files that the routing index just written does not reference
    /** A metadata cache file is only ever reached through the routing index: the manifest through the digest that
        the routing index records, and each app's chunk through the manifest, which records its size and digest.
        Once a new routing index has been written, the files that only the previous index referenced can never be
        read again, so they are removed rather than left to accumulate; this includes the files of an app that is no
        longer indexed, and the files of metadata cache versions that named them after their apps.

        Removal is deliberately driven by the manifest rather than by comparing against the modules present:
        the manifest is what the reader consults, so "unreferenced" is exactly "unreachable".

        A canonical index file is removed if the routing index was published without one, so that it can never be
        mistaken for the current index.

        @param chunks the chunks that the manifest references, if a metadata cache was written
        @param manifest_sha256 the digest of the manifest, if a metadata cache was written
        @param canonical @ref True if a canonical index file was written with the routing index

        @par Example
        Uninstalling a provider module and rebuilding the index removes that app's chunk, so the index directory
        holds one chunk per indexed app and nothing else.

        @note Errors are logged and ignored: failing to remove a stale file must never fail an index write, because
        the index itself is already published at this point.
    */
    static private removeUnreferencedIndexFiles(*hash<string, DataProviderMetadataCacheChunkInfo> chunks,
            *string manifest_sha256, bool canonical) {
        *string dir = tryGetIndexDir();
        if (!dir.val()) {
            return;
        }
        hash<string, bool> keep = (map {basename(getMetadataCacheChunkPath($1.key, $1.value.sha256,
            MetadataCacheVersion)): True}, chunks.pairIterator()) ?? {};
        if (manifest_sha256) {
            keep{basename(getMetadataCachePath(manifest_sha256))} = True;
        }
        try {
            Dir d();
            d.chdir(dir);
            # chunks, current and legacy manifests; temporary files of a concurrent write never match
            foreach string name in (d.listFiles("^qore-data-index(-manifest)?(-[0-9a-f]{64})?\\.msgpack$")) {
                if (keep{name}) {
                    continue;
                }
                removeIndexFile(dir + DirSep + name);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to scan the data provider index directory for stale metadata "
                "cache files: %s: %s", ex.err, ex.desc);
        }
        if (!canonical) {
            string path = getIndexPath("data", dir);
            if (hlstat(path)) {
                removeIndexFile(path);
            }
        }
    }

    #! Removes a stale index file, logging any error
    static private removeIndexFile(string path) {
        try {
            unlink(path);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to remove stale data provider index file %y: %s: %s", path, ex.err,
                ex.desc);
        }
    }

    #! Reads a published index from its routing index and metadata cache, without the canonical index file
    /** The routing index and the metadata cache are the published index; a canonical index file
        (\c qore-data-index.yaml), which holds the same metadata uncompressed and as text, and so is many times
        larger, is only published on request.  This reads the compact files and verifies them: the routing index
        against its own checksum, the metadata cache manifest against the digest recorded in the routing index, and
        each app's metadata against the size and digest recorded in the manifest.

        @param dir the index directory
        @param per_app optional code called with each app's name, info, and actions, in app name order, as each
        app's metadata is read; the index then holds only its \c factorymap, \c appmap, and \c schememap
        members, so the metadata of only one app is in memory at a time

        @return the index, or @ref nothing if the directory holds no routing index with a metadata cache, or if the
        files are inconsistent; if the index was published with a canonical index file, the caller can then read it

        @par Example:
        @code{.py}
*hash<SerializedDataProviderIndexInfo> info = ProviderIndexUtil::readSerializedDataProviderIndex(dir);
if (info) {
    printf("%d apps; index %s\n", info.index.appinfomap.size(), info.sha256);
}
        @endcode

        @note Any canonical index file is not read, so its content is not verified against the digest recorded in
        the routing index
        @note With @a per_app, apps are passed on as they are verified, so if @ref nothing is returned, the apps
        already passed must be discarded; an exception raised by @a per_app is passed on to the caller

        @since ProviderIndexUtil 1.2
    */
    static *hash<SerializedDataProviderIndexInfo> readSerializedDataProviderIndex(string dir,
            *code<nothing(string, *hash<auto>, *hash<auto>)> per_app) {
        return readSerializedIndexIntern(dir, True, per_app);
    }

    #! Returns the digest and size of a published index from its routing index and metadata cache
    /** Verifies the same files as @ref readSerializedDataProviderIndex() and returns the same digest and size, but
        does not unpack any metadata, so it can cheaply check whether an index read before has changed.

        @param dir the index directory

        @return the digest and size of the index, with no \c index member, or @ref nothing if
        @ref readSerializedDataProviderIndex() would return @ref nothing

        @since ProviderIndexUtil 1.2
    */
    static *hash<SerializedDataProviderIndexInfo> getSerializedDataProviderIndexSource(string dir) {
        return readSerializedIndexIntern(dir, False);
    }

    #! Writes a published index as YAML in the format of the canonical index file
    /** Builds the YAML from the routing index and the metadata cache, so the canonical index file, which is not
        published by default, never needs to be stored for a person to read the index.  The metadata of one app at
        a time is held in memory; when the output is written to a file or to standard output, it is written as each
        app is read.

        The YAML of the whole index is the canonical index file that
        @ref writeDataProviderIndexByApp() would have written for the index, except that an index in which no app
        has actions has no \c actioninfomap member.  The YAML of one app has the \c appmap, \c appinfomap, and
        \c actioninfomap members of the index restricted to that app.

        @param dir the index directory; if not given, the configured index directory
        @param app the app to write; if not given, the whole index is written
        @param output_path the file to write the YAML to, or \c "-" for standard output; if not given, the YAML
        is returned

        @return the YAML, if no @a output_path is given

        @throw PROVIDER-INDEX-ERROR the directory holds no published index with a routing index and metadata cache,
        or the files are inconsistent, or the app is not in the index, or YAML or MessagePack support is not
        available

        @par Example:
        @code{.py}
# write the actions indexed for OpenSearch for review
ProviderIndexUtil::dumpIndexYaml(NOTHING, "OpenSearch", "/tmp/opensearch-index.yaml");
        @endcode

        From the command line:
        @verbatim
qore -l ProviderIndexUtil -e 'ProviderIndexUtil::dumpIndexYaml(NOTHING, NOTHING, "-");' > qore-data-index.yaml
        @endverbatim

        @since ProviderIndexUtil 1.2
    */
    static *string dumpIndexYaml(*string dir, *string app, *string output_path) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", "YAML support is not available; cannot write the data provider index as YAML";
%else
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read the data provider metadata";
%else
        if (!dir.val()) {
            dir = tryGetIndexDir(True);
        }
        *hash<CompactIndexInfo> compact = readCompactIndex(dir);
        if (!compact) {
            throw "PROVIDER-INDEX-ERROR", sprintf("%y does not hold a published data provider index with a "
                "consistent routing index and metadata cache", dir);
        }
        hash<string, DataProviderMetadataCacheChunkInfo> apps = compact.manifest.apps;
        if (exists app && !apps{app}) {
            throw "PROVIDER-INDEX-ERROR", sprintf("app %y is not in the data provider index in %y", app, dir);
        }

        *string rv;
        *File f;
        code<nothing(string)> write;
        if (output_path == "-") {
            write = sub (string str) {
                stdout.write(str);
            };
        } else if (output_path.val()) {
            f = new File();
            f.open2(output_path, O_CREAT | O_TRUNC | O_WRONLY, IndexFileMode);
            write = sub (string str) {
                f.write(str);
            };
        } else {
            rv = "";
            write = sub (string str) {
                rv += str;
            };
        }

        hash<DataProviderRoutingIndexInfo> routing = compact.routing;
        list<string> dump_apps;
        if (exists app) {
            dump_apps = (app,);
            writeCanonicalMaps(write, {
                "appmap": exists routing.appmap{app} ? {app: routing.appmap{app}} : {},
                "appinfomap": exists apps{app}.info ? {app: apps{app}.info} : {},
            }, ("appmap", "appinfomap"));
        } else {
            dump_apps = sort(keys apps);
            writeCanonicalMaps(write, {
                "factorymap": routing.factorymap,
                "appmap": routing.appmap,
                "schememap": routing.schememap,
                "appinfomap": (map {$1.key: $1.value.info}, apps.pairIterator(), exists $1.value.info),
            });
        }
        bool actions_written;
        foreach string chunk_app in (dump_apps) {
            hash<DataProviderMetadataCacheChunkInfo> info = apps{chunk_app};
            binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath(chunk_app, info.sha256,
                compact.manifest.version, dir));
            if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
                throw "PROVIDER-INDEX-ERROR", sprintf("the metadata of app %y in the data provider index in %y is "
                    "corrupt", chunk_app, dir);
            }
            *hash<auto> actions = unpackMetadataCacheChunk(data).actioninfomap{chunk_app};
            if (exists actions) {
                writeCanonicalAppActions(write, chunk_app, actions, !actions_written);
                actions_written = True;
            }
        }
        if (f) {
            f.close();
        }
        return rv;
%endif
%endif
    }

    #! Reads and verifies a published index from its routing index and metadata cache
    /** @param dir the index directory
        @param unpack @ref True to unpack the metadata, @ref False to only verify it
        @param per_app optional code called with each app's metadata; see readSerializedDataProviderIndex()
    */
    static private *hash<SerializedDataProviderIndexInfo> readSerializedIndexIntern(string dir, bool unpack,
            *code<nothing(string, *hash<auto>, *hash<auto>)> per_app) {
%ifdef NoYaml
        return;
%else
%ifdef NoMsgPack
        return;
%else
        # an exception raised by the caller's code is not an error reading the index
        bool per_app_error;
        try {
            *hash<CompactIndexInfo> compact = readCompactIndex(dir);
            if (!compact) {
                return;
            }

            int size = compact.size;
            hash<auto> appinfomap = {};
            hash<auto> actioninfomap = {};
            foreach string app in (sort(keys compact.manifest.apps)) {
                hash<DataProviderMetadataCacheChunkInfo> chunk_info = compact.manifest.apps{app};
                binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath(app, chunk_info.sha256,
                    compact.manifest.version, dir));
                if (data.size() != chunk_info.size || digest("sha256", data).toHex() != chunk_info.sha256) {
                    return;
                }
                size += data.size();
                if (unpack) {
                    hash<DataProviderIndexInfo> chunk = unpackMetadataCacheChunk(data);
                    if (per_app) {
                        per_app_error = True;
                        per_app(app, chunk.appinfomap{app}, chunk.actioninfomap{app});
                        per_app_error = False;
                        continue;
                    }
                    if (exists chunk.appinfomap{app}) {
                        appinfomap{app} = chunk.appinfomap{app};
                    }
                    if (exists chunk.actioninfomap{app}) {
                        actioninfomap{app} = chunk.actioninfomap{app};
                    }
                }
            }

            hash<SerializedDataProviderIndexInfo> rv = <SerializedDataProviderIndexInfo>{
                "sha256": digest("sha256", compact.routing_data).toHex(),
                "size": size,
            };
            if (unpack) {
                rv.index = {
                    "factorymap": compact.routing.factorymap ?? {},
                    "appmap": compact.routing.appmap ?? {},
                    "schememap": compact.routing.schememap ?? {},
                };
                if (!per_app) {
                    rv.index += {
                        "appinfomap": appinfomap,
                        "actioninfomap": actioninfomap,
                    };
                }
            }
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            if (per_app_error) {
                rethrow;
            }
            # a missing or unreadable file, as while files are copied
            log(LoggerLevel::LevelDebug, "Cannot read the data provider metadata cache in %y: %s: %s", dir, ex.err,
                ex.desc);
        }
%endif
%endif
    }

    #! Reads and verifies the routing index and metadata cache manifest of a published index
    /** @param dir the index directory

        @return the routing index and manifest, or @ref nothing if the directory holds no routing index with a
        metadata cache, or if they are inconsistent

        @throw any error reading the files
    */
    static private *hash<CompactIndexInfo> readCompactIndex(string dir) {
        string routing_path = getIndexPath("routing", dir);
        if (!is_file(routing_path)) {
            return;
        }
        binary routing_data = ReadOnlyFile::readBinaryFile(routing_path);
        *hash<DataProviderRoutingIndexInfo> routing = parseRoutingIndex(routing_data);
        if (!routing || !checkRoutingIndex(routing) || !checkMetadataCacheVersion(routing.metadata_cache_version)
                || !routing.metadata_cache_sha256) {
            return;
        }

        binary manifest_data = ReadOnlyFile::readBinaryFile(getMetadataCachePath(routing.metadata_cache_sha256,
            routing.metadata_cache_version, dir));
        if (digest("sha256", manifest_data).toHex() != routing.metadata_cache_sha256) {
            return;
        }
        *hash<DataProviderMetadataCacheManifest> manifest =
            cast<*hash<DataProviderMetadataCacheManifest>>(unpackIndexData(manifest_data));
        if (!manifest || manifest.version != routing.metadata_cache_version) {
            return;
        }
        return <CompactIndexInfo>{
            "routing": routing,
            "routing_data": routing_data,
            "manifest": manifest,
            "size": routing_data.size() + manifest_data.size(),
        };
    }

    #! Parses a routing index
    static private *hash<DataProviderRoutingIndexInfo> parseRoutingIndex(binary data) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", "YAML support is not available; cannot read the data provider routing index";
%else
        return cast<*hash<DataProviderRoutingIndexInfo>>(parse_yaml(data.toString("UTF-8")));
%endif
    }

    #! Returns @ref True if the routing index has a supported version and matches its checksum
    static private bool checkRoutingIndex(hash<DataProviderRoutingIndexInfo> routing) {
%ifdef NoYaml
        return False;
%else
        return routing.version >= MinRoutingIndexVersion && routing.version <= RoutingIndexVersion
            && routing.routing_sha256
                == digest("sha256", make_yaml(getRoutingIndexChecksumPayload(routing), BlockStyle)).toHex();
%endif
    }

    #! Returns @ref True if the given metadata cache version can be read
    static private bool checkMetadataCacheVersion(*int version) {
        return exists version && version >= MinMetadataCacheVersion && version <= MetadataCacheVersion;
    }

    #! Returns @ref True if a canonical index file was published with the given routing index
    static private bool isCanonicalRoutingIndex(hash<DataProviderRoutingIndexInfo> routing) {
        # a version 2 routing index was always published with a canonical index file
        return routing.version < 3 || routing.canonical_yaml;
    }

    #! Reads the complete data provider index from the configured directory
    /** The index currently published is read, independently of the index state of the process.

        @return the index, or @ref nothing if no index is published, or it cannot be read, in which case the error
        is reported by @ref getIndexReadError()
    */
    static *hash<DataProviderIndexInfo> readDataProviderIndex() {
        *hash<DataProviderRoutingIndexInfo> routing = readValidRoutingIndex();
        if (!routing.metadata_cache_sha256) {
            return deserializeDataProviderIndex(readCanonicalDataProviderIndex());
        }
        bool failed;
        *hash<DataProviderIndexInfo> cache;
        if (*hash<DataProviderMetadataCacheManifest> manifest =
                readMetadataCacheManifestFile(routing.metadata_cache_sha256, routing.metadata_cache_version)) {
            cache = readMetadataCacheChunks(manifest, NOTHING, True, \failed);
            cache += getRoutingIndexPayload(routing);
        } else {
            failed = True;
        }
        if (failed) {
            cache = readCanonicalDataProviderIndex() ?? cache;
        }
        return deserializeDataProviderIndex(cache);
    }

    #! Returns the size in bytes of the largest canonical index file that is read
    /** @return the value of the @ref ProviderIndexMaxCanonicalSizeEnvVar "QORE_PROVIDER_INDEX_MAX_CANONICAL_BYTES"
        environment variable if it is a positive integer, otherwise
        @ref DefaultMaxCanonicalIndexSize "DefaultMaxCanonicalIndexSize"

        @since ProviderIndexUtil 1.2
    */
    static int getMaxCanonicalIndexSize() {
        *string value = getenv(ProviderIndexMaxCanonicalSizeEnvVar);
        if (value =~ /^[0-9]+$/ && value.toInt() > 0) {
            return value.toInt();
        }
        return DefaultMaxCanonicalIndexSize;
    }

    #! Reads a validated routing sidecar when available, otherwise reads the canonical index
    static private *hash<DataProviderIndexInfo> readInitialDataProviderIndex() {
        # each read of the index reports its own errors
        remove index_read_error;
        *hash<DataProviderIndexInfo> rv = readRoutingDataProviderIndex();
        if (rv) {
            raw_metadata_deferred = True;
            return rv;
        }
        return readCanonicalDataProviderIndex();
    }

    #! Writes the compact routing sidecar for a data provider index
    /** @param h the index maps
        @param cache_sha256 the digest of the metadata cache manifest, if written
        @param data_sha256 the digest of the canonical index file, if written
        @param data_size the size of the canonical index file, if written

        @return the size of the routing index
    */
    static private int writeRoutingDataProviderIndex(*hash<auto> h, *string cache_sha256, *string data_sha256,
            *int data_size) {
        hash<auto> sidecar = getRoutingIndexPayload(h) + {
            "version": RoutingIndexVersion,
        };
        if (data_sha256) {
            sidecar.data_sha256 = data_sha256;
            sidecar.data_size = data_size;
        }
        sidecar.canonical_yaml = exists data_sha256;
        if (cache_sha256) {
            sidecar.metadata_cache_version = MetadataCacheVersion;
            sidecar.metadata_cache_sha256 = cache_sha256;
        }
%ifndef NoYaml
        sidecar.routing_sha256 =
            digest("sha256", make_yaml(getRoutingIndexChecksumPayload(sidecar), BlockStyle)).toHex();
%endif
        return writeIndex(sidecar, "routing", 0s, {}).size;
    }

    #! Records and logs an error reading a published index
    /** @param err the error code
        @param desc the error description
    */
    static private setIndexReadError(string err, string desc) {
        index_read_error = sprintf("%s: %s", err, desc);
        log(LoggerLevel::LevelError, "Error reading the data provider index: %s: %s", err, desc);
    }

    #! Reads and validates the compact routing sidecar
    /** @return the index maps, with the metadata deferred, or @ref nothing if the routing index cannot be used, in
        which case the canonical index file is read if there is one
    */
    static private *hash<DataProviderIndexInfo> readRoutingDataProviderIndex() {
        remove metadata_cache_sha256;
        remove metadata_cache_version;
        *hash<DataProviderRoutingIndexInfo> routing = readValidRoutingIndex();
        if (!routing) {
            return;
        }
        if (routing.metadata_cache_sha256) {
            metadata_cache_sha256 = routing.metadata_cache_sha256;
            metadata_cache_version = routing.metadata_cache_version;
        }
        return <DataProviderIndexInfo>{
            "factorymap": routing.factorymap ?? {},
            "appmap": routing.appmap ?? {},
            "schememap": routing.schememap ?? {},
            "appinfomap": {},
            "actioninfomap": {},
            "top_actioninfomap": {},
        };
    }

    #! Reads and validates the routing index in the configured directory
    /** An index is published with a routing index and a metadata cache, and optionally with a canonical index file;
        this validates the routing index against its own checksum, and against the canonical index file if one was
        published with it.

        @return the routing index, or @ref nothing if it cannot be used, in which case the canonical index file is
        read if there is one; the returned routing index has a metadata cache with a supported version, or was
        published with a canonical index file; any error is reported by @ref getIndexReadError()
    */
    static private *hash<DataProviderRoutingIndexInfo> readValidRoutingIndex() {
        # skip silently when the index env var is not configured, as in readIndex()
        if (!ENV{ProviderIndexEnvVar}.val()) {
            return;
        }
        string routing_path;
        *hash<StatInfo> routing_stat;
        *hash<DataProviderRoutingIndexInfo> routing;
        try {
            routing_path = getIndexPath("routing");
            routing_stat = hstat(routing_path);
            if (!routing_stat) {
                return;
            }
            routing = parseRoutingIndex(ReadOnlyFile::readBinaryFile(routing_path));
        } catch (hash<ExceptionInfo> ex) {
            # the routing index is replaced atomically, so a file that cannot be read is not a partial write
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the routing index %y: %s: %s",
                routing_path ?? "<unknown>", ex.err, ex.desc));
            return;
        }
        if (!routing || !checkRoutingIndex(routing)) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the routing index %y is corrupt or has an "
                "unsupported version (%y)", routing_path, routing.version));
            return;
        }

        bool canonical = isCanonicalRoutingIndex(routing);
        try {
            string data_path = getIndexPath("data");
            *hash<StatInfo> data_stat = hstat(data_path);
            if (data_stat) {
                if (canonical) {
                    # the canonical index can be large (hundreds of megabytes), so its digest is verified only if
                    # it may have changed since the routing index was written, which is always written after it:
                    # if its size is different, or it is newer than the routing index, as when it has been replaced
                    # or the files have been copied; reading it as binary data does not copy it into a string
                    if (routing.data_size != data_stat.size || data_stat.mtime > routing_stat.mtime) {
                        if (routing.data_sha256
                                != digest("sha256", ReadOnlyFile::readBinaryFile(data_path)).toHex()) {
                            # a canonical index file written after the routing index is the current index
                            return;
                        }
                    }
                } else if (data_stat.mtime > routing_stat.mtime) {
                    # a canonical index file left by an index published without one is removed after its routing
                    # index is written, so a newer one was written after it, and is the current index
                    return;
                }
            } else if (canonical && !routing.metadata_cache_sha256) {
                # no file holds the metadata of the index
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            # the canonical index file is read instead, which reports the error
            return;
        }

        if (!checkMetadataCacheVersion(routing.metadata_cache_version) || !routing.metadata_cache_sha256) {
            if (!canonical) {
                # the metadata cache is the only copy of the metadata of an index published without a canonical
                # file
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the routing index %y has no metadata cache with "
                    "a supported version (%y)", routing_path, routing.metadata_cache_version));
                return;
            }
            # the canonical index file holds the metadata
            remove routing.metadata_cache_sha256;
            remove routing.metadata_cache_version;
        }
        return routing;
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
        };
        # the members are added in the order of earlier versions, so that their checksums are unchanged
        if (exists h.data_sha256) {
            rv.data_sha256 = h.data_sha256;
        }
        if (exists h.data_size) {
            rv.data_size = h.data_size;
        }
        if (exists h.canonical_yaml) {
            rv.canonical_yaml = h.canonical_yaml;
        }
        if (exists h.metadata_cache_version) {
            rv.metadata_cache_version = h.metadata_cache_version;
        }
        if (exists h.metadata_cache_sha256) {
            rv.metadata_cache_sha256 = h.metadata_cache_sha256;
        }
        return rv;
    }

    #! Reads deferred metadata from the metadata cache
    /** If the metadata cache cannot be read, the canonical index file is read in full, if there is one; otherwise,
        the metadata that could be read is returned, and the error is reported by @ref getIndexReadError().

        @param app an optional app name; when present, only that app's metadata is read
        @param actions if @ref False and no app is given, only the info of every app is read from the cache
    */
    static private hash<RawMetadataIndexResult> readDeferredRawDataProviderIndex(*string app, bool actions = True) {
        bool failed;
        *hash<DataProviderIndexInfo> cache = readMetadataDataProviderIndex(app, actions || exists app, \failed);
        if (cache && !failed) {
            return <RawMetadataIndexResult>{
                "index": cache,
                "complete": !app && actions,
            };
        }

        if (*hash<DataProviderIndexInfo> canonical = readCanonicalDataProviderIndex()) {
            return <RawMetadataIndexResult>{
                "index": canonical,
                "complete": True,
            };
        }
        # the apps whose metadata could be read are still used; if no metadata can be read, it is not read again
        return <RawMetadataIndexResult>{
            "index": cache,
            "complete": !cache || (!app && actions),
        };
    }

    #! Reads the canonical index file, if there is one
    /** @return the index, or @ref nothing if there is no canonical index file, or if it cannot be read, in which
        case the error is reported by @ref getIndexReadError(); a file larger than @ref getMaxCanonicalIndexSize()
        is not read
    */
    static private *hash<DataProviderIndexInfo> readCanonicalDataProviderIndex() {
        # a missing canonical index file is the normal state of an index published without one; it must not clear
        # an error reading the metadata cache
        if (!ENV{ProviderIndexEnvVar}.val()) {
            return;
        }
        try {
            if (!hstat(getIndexPath("data"))) {
                return;
            }
            return cast<*hash<DataProviderIndexInfo>>(readIndex("data"));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
        }
    }

    #! Reads and validates app-specific chunks from the MessagePack metadata cache of the process's index
    /** @param app an optional app name; when present, only that app's chunk is read
        @param actions if @ref False, the actions in each chunk are dropped as soon as it is read
        @param failed set to @ref True if the manifest or any chunk cannot be read, in which case the error is
        reported by @ref getIndexReadError(); the chunks that can be read are returned

        @return the metadata read, or @ref nothing if no metadata cache is published, or if its manifest cannot be
        read
    */
    static private *hash<DataProviderIndexInfo> readMetadataDataProviderIndex(*string app, bool actions,
            reference<bool> failed) {
        *hash<DataProviderMetadataCacheManifest> manifest = readMetadataCacheManifest();
        if (!manifest) {
            # the routing index references a manifest that cannot be read
            failed = exists metadata_cache_sha256;
            return;
        }
        return readMetadataCacheChunks(manifest, app, actions, \failed);
    }

    #! Reads and validates app-specific chunks from a MessagePack metadata cache
    /** @param manifest the verified manifest
        @param app an optional app name; when present, only that app's chunk is read
        @param actions if @ref False, the actions in each chunk are dropped as soon as it is read
        @param failed set to @ref True if any chunk cannot be read, in which case the error is reported by
        @ref getIndexReadError(); the chunks that can be read are returned

        @return the metadata read
    */
    static private hash<DataProviderIndexInfo> readMetadataCacheChunks(hash<DataProviderMetadataCacheManifest> manifest,
            *string app, bool actions, reference<bool> failed) {
        hash<DataProviderIndexInfo> rv = createEmptyDataProviderIndex();
        if (!app && !actions) {
            # the manifest holds the info of every app; the chunks also hold the actions of each app, which take
            # many times the memory of the app info while they are read
            rv.appinfomap = map {$1.key: $1.value.info}, manifest.apps.pairIterator(), exists $1.value.info;
            return rv;
        }
        list<string> apps = app ? (app,) : keys manifest.apps;
        foreach string chunk_app in (apps) {
            if (!exists manifest.apps{chunk_app}) {
                continue;
            }
            *hash<DataProviderIndexInfo> chunk = readMetadataCacheChunk(chunk_app, manifest.apps{chunk_app},
                manifest.version);
            if (!chunk) {
                failed = True;
                continue;
            }
            rv.appinfomap += chunk.appinfomap ?? {};
            if (actions) {
                rv.actioninfomap += chunk.actioninfomap ?? {};
            }
        }
        return rv;
    }

    #! Reads and validates the metadata cache manifest of the process's index once per process
    static private *hash<DataProviderMetadataCacheManifest> readMetadataCacheManifest() {
        if (metadata_cache_manifest_checked) {
            return metadata_cache_manifest;
        }
        metadata_cache_manifest_checked = True;
        if (!metadata_cache_sha256) {
            return;
        }
        return metadata_cache_manifest = readMetadataCacheManifestFile(metadata_cache_sha256,
            metadata_cache_version ?? MetadataCacheVersion);
    }

    #! Reads and validates a metadata cache manifest in the configured directory
    /** @param sha256 the digest of the manifest recorded in the routing index
        @param version the metadata cache version recorded in the routing index

        @return the manifest, or @ref nothing if it cannot be read, in which case the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<DataProviderMetadataCacheManifest> readMetadataCacheManifestFile(string sha256,
            int version) {
%ifdef NoMsgPack
        setIndexReadError("PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read the metadata "
            "cache");
        return;
%else
        *string path;
        try {
            path = getMetadataCachePath(sha256, version);
            binary data = ReadOnlyFile::readBinaryFile(path);
            if (digest("sha256", data).toHex() != sha256) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the metadata cache manifest %y does not match "
                    "the digest recorded in the routing index", path));
                return;
            }
            *hash<DataProviderMetadataCacheManifest> manifest =
                cast<*hash<DataProviderMetadataCacheManifest>>(unpackIndexData(data));
            if (!manifest || manifest.version != version) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the metadata cache manifest %y has version %y; "
                    "expecting %y", path, manifest.version, version));
                return;
            }
            return manifest;
        } catch (hash<ExceptionInfo> ex) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the metadata cache manifest %y: %s: %s",
                path ?? "<unknown>", ex.err, ex.desc));
        }
%endif
    }

    #! Reads and validates one app-specific metadata cache chunk
    /** @param app the app name
        @param info the chunk's size and digest from the manifest
        @param version the metadata cache version

        @return the chunk, or @ref nothing if it cannot be read, in which case the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<DataProviderIndexInfo> readMetadataCacheChunk(string app,
            hash<DataProviderMetadataCacheChunkInfo> info, int version) {
        *string path;
        try {
            path = getMetadataCacheChunkPath(app, info.sha256, version);
            binary data = ReadOnlyFile::readBinaryFile(path);
            if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the metadata of app %y in %y does not match the "
                    "size and digest recorded in the metadata cache manifest", app, path));
                return;
            }
            return unpackMetadataCacheChunk(data);
        } catch (hash<ExceptionInfo> ex) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the metadata of app %y from %y: %s: %s",
                app, path ?? "<unknown>", ex.err, ex.desc));
        }
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
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
        }
        # rebuilt from the typed action metadata; serialized entries from the file are never returned
        rv.top_actioninfomap = {};
        if (rv.actioninfomap) {
            try {
                rv.actioninfomap = deserializeActionsWithContext(rv.actioninfomap, ctx);
            } catch (hash<ExceptionInfo> ex) {
                # serialized entries must not be returned in place of typed ones
                log(LoggerLevel::LevelError, "Ignoring error reading actions from the data provider index: %s",
                    get_exception_string(ex));
                rv.actioninfomap = {};
            }
            foreach hash<auto> i in (rv.actioninfomap.pairIterator()) {
                map rv.top_actioninfomap{i.key + "/" + $1.key} = $1.value, i.value.pairIterator();
            }
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
            "action_info_th": TypedHash::forName("DataProvider::DataProviderIndexedActionInfo"),
            "option_info_th": TypedHash::forName("DataProvider::IndexedActionOptionInfo"),
            "type_info_th": TypedHash::forName("DataProvider::DataTypeInfo"),
            "field_info_th": TypedHash::forName("DataProvider::DataFieldInfo"),
            "type_opt_info_th": TypedHash::forName("DataProvider::DataProviderTypeOptionInfo"),
            "constraint_info_th": TypedHash::forName("DataProvider::DataTypeConstraintInfo"),
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
    /** @param actioninfomap action metadata as stored in the index, keyed by app and then by action
        @param ctx the prepared deserialization context

        @return \c DataProvider::DataProviderIndexedActionInfo hashes keyed by app and then by action; every app in
        @a actioninfomap is present, but an action whose metadata cannot be read is logged and omitted
    */
    static private hash<auto> deserializeActionsWithContext(
            hash<string, hash<string, hash<auto>>> actioninfomap,
            hash<MetadataDeserializeContext> ctx) {
        hash<auto> rv;
        foreach hash<auto> i in (actioninfomap.pairIterator()) {
            hash<auto> actions = {};
            foreach hash<auto> j in (i.value.pairIterator()) {
                try {
                    actions{j.key} = getIndexedAction(j.value, ctx, True);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::LevelError, "Omitting action %y of app %y from the data provider index "
                        "metadata; it cannot be read: %s: %s", j.key, i.key, ex.err, ex.desc);
                }
            }
            rv{i.key} = actions;
        }
        return rv;
    }

    #! Returns the given action as a DataProviderIndexedActionInfo hash
    /** @param action the action as stored in the index, or a registered action
        @param ctx the prepared deserialization context
        @param indexed @ref True if @a action is stored in the index, in which case its nested data provider info
        is in serialized form

        @return a \c DataProvider::DataProviderIndexedActionInfo hash
    */
    static private hash<auto> getIndexedAction(hash<auto> action, hash<MetadataDeserializeContext> ctx,
            *bool indexed) {
        # dynamic callbacks and output types are not indexed; earlier indexes may contain output types
        action -= ("output_type", "get_output_type", "get_dynamic_options");
        return ctx.action_info_th.doCastWithOverrides(action, {
            "path_vars": (action.path_vars
                ? ctx.path_vars_th.doCastValues(action.path_vars)
                : NOTHING),
            "options": (exists action.options ? ctx.option_info_th.doCastValuesWithOverrides(
                (map {$1.key: $1.value - "type"}, action.options.pairIterator()), map {
                $1.key: {
                    "allowed_values": (exists $1.value.allowed_values
                        ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                        : NOTHING),
                    "element_allowed_values": (exists $1.value.element_allowed_values
                        ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                        : NOTHING),
                    "type_info": getOptionTypeInfo(action, $1.key, $1.value, ctx),
                    "default_view": indexed ? ctx.ui_view_map{$1.value.default_view} : $1.value.default_view,
                }
            }, action.options.pairIterator()) : NOTHING),
            "data_provider_info": (action.data_provider_info && indexed
                ? deserializeDataProviderInfo(action.data_provider_info, ctx.dp_info_th,
                    ctx.dp_opt_info_th, ctx.dp_exp_info_th, ctx.mapper_key_info_th, ctx.dp_msg_info_th,
                    ctx.dp_sig_info_th, ctx.dp_arg_info_th, ctx.type_map, ctx.shape_map)
                : action.data_provider_info),
        });
    }

    #! Returns the type description of an action option
    /** @param action the action
        @param name the option name
        @param option the option as stored in the index, or an option of a registered action
        @param ctx the prepared deserialization context

        @return a \c DataProvider::DataTypeInfo hash
    */
    static private hash<auto> getOptionTypeInfo(hash<auto> action, string name, hash<auto> option,
            hash<MetadataDeserializeContext> ctx) {
        # a registered action's option is a typed hash without this key
        if (option.hasKey("type_info") && option.type_info) {
            return castTypeInfo(option.type_info, ctx);
        }
        switch (option.type.typeCode()) {
            case NT_OBJECT:
                # a registered action's option
                return option.type.getInfo();
            case NT_STRING:
                # a primitive type name from an index written before ProviderIndex 1.2
                return (ctx.type_map{option.type} ?? ctx.type_map."auto").getInfo();
        }
        if (exists option.type) {
            # types serialized by unreleased development versions of ProviderIndex 1.2 are not restored, since
            # restoring them loads the provider modules that define them
            log(LoggerLevel::LevelWarn, "The type of option %y of action %y of app %y in the data provider index "
                "cannot be read; the index must be recreated", name, action.action, action.app);
        }
        return ctx.type_map."auto".getInfo();
    }

    #! Returns the given plain type description as a DataTypeInfo hash, including all nested descriptions
    /** @param info the type description as stored in the index
        @param ctx the prepared deserialization context

        @return a \c DataProvider::DataTypeInfo hash
    */
    static private hash<auto> castTypeInfo(hash<auto> info, hash<MetadataDeserializeContext> ctx) {
        return ctx.type_info_th.doCastWithOverrides(info, {
            "supported_options": (exists info.supported_options
                ? ctx.type_opt_info_th.doCastValues(info.supported_options)
                : NOTHING),
            "fields": (exists info.fields ? ctx.field_info_th.doCastValuesWithOverrides(info.fields, map {
                $1.key: {
                    "type": castTypeInfo($1.value.type, ctx),
                    "allowed_values": (exists $1.value.allowed_values
                        ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                        : NOTHING),
                    "element_allowed_values": (exists $1.value.element_allowed_values
                        ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                        : NOTHING),
                },
            }, info.fields.pairIterator()) : NOTHING),
            "element_type": exists info.element_type ? castTypeInfo(info.element_type, ctx) : NOTHING,
            "union_types": exists info.union_types ? (map castTypeInfo($1, ctx), info.union_types) : NOTHING,
            "default_field_type_info": (exists info.default_field_type_info
                ? castTypeInfo(info.default_field_type_info, ctx)
                : NOTHING),
            "allowed_values": (exists info.allowed_values
                ? map cast<hash<AllowedValueInfo>>($1), info.allowed_values
                : NOTHING),
            "element_allowed_values": (exists info.element_allowed_values
                ? map cast<hash<AllowedValueInfo>>($1), info.element_allowed_values
                : NOTHING),
            "constraints": exists info.constraints ? ctx.constraint_info_th.doCast(info.constraints) : NOTHING,
        });
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
            if (type == "data") {
                # the canonical index file is parsed in full, which takes several times its size in memory
                int max = getMaxCanonicalIndexSize();
                *hash<StatInfo> info = hstat(path);
                if (info && info.size > max) {
                    throw "PROVIDER-INDEX-TOO-LARGE", sprintf("the canonical data provider index %y is %d bytes, "
                        "which is larger than the limit of %d bytes (set with the %s environment variable); it is "
                        "not read", path, info.size, max, ProviderIndexMaxCanonicalSizeEnvVar);
                }
            }
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
    static string getIndexPath(string type, *string dir) {
        return sprintf("%s%sqore-%s-index.yaml", dir ?? tryGetIndexDir(True), DirSep, type);
    }

    #! Returns the path of a metadata cache manifest
    /** @param sha256 the digest of the manifest
        @param version the metadata cache version; the manifest of a version before 5 has a fixed name
        @param dir the index directory; if not given, the configured index directory
    */
    static private string getMetadataCachePath(string sha256, int version = MetadataCacheVersion, *string dir) {
        if (version < 5) {
            return sprintf("%s%sqore-data-index.msgpack", dir ?? tryGetIndexDir(True), DirSep);
        }
        return sprintf("%s%sqore-data-index-manifest-%s.msgpack", dir ?? tryGetIndexDir(True), DirSep, sha256);
    }

    #! Returns the path of an app-specific metadata cache chunk
    /** @param app the app name
        @param sha256 the digest of the chunk
        @param version the metadata cache version; the chunk of a version before 5 is named after the digest of its
        app's name instead of its own
        @param dir the index directory; if not given, the configured index directory
    */
    static private string getMetadataCacheChunkPath(string app, string sha256, int version = MetadataCacheVersion,
            *string dir) {
        return sprintf("%s%sqore-data-index-%s.msgpack", dir ?? tryGetIndexDir(True), DirSep,
            version < 5 ? digest("sha256", app).toHex() : sha256);
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
