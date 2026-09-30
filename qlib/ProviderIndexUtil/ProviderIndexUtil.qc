# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProviderIndexUtil.qc module for indexing data providers

/*  ProviderIndexUtil.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Version 3 records whether a canonical index file was published with the routing index; version 4 records the
#! actions declaring an alternative to their app's scheme connection
const RoutingIndexVersion = 4;
#! Version 5 names each metadata cache file after the digest of its contents, so that publishing an index never
#! changes a file that the previous index references; version 6 records the info of the indexed data provider
#! factories in a file of its own that the manifest references; version 7 stores each distinct option type
#! description and field description of an app once, in tables of its chunk that the descriptions refer to by position;
#! version 8 records the action-level record of every action in a file of its own that the manifest references;
#! version 9 records the apps and actions that could not be initialized when the index was built in a file of its own
#! that the manifest references
const MetadataCacheVersion = 9;
#! The first metadata cache version whose chunks store the type descriptions of an app in tables
const MetadataCacheTypeTableVersion = 7;
#! The first metadata cache version that records the action summaries in a file of their own
const MetadataCacheActionSummaryVersion = 8;
#! The first metadata cache version that records the apps and actions that could not be initialized
const MetadataCacheFailureVersion = 9;

#! The members of a serialized action that are not in its action summary
/** The options and the data provider info hold the type descriptions of the action; the others are not indexed, or
    only by earlier versions, or are internal to the module
*/
const ActionSummaryExcludedKeys = ("options", "data_provider_info", "output_type", "get_output_type",
    "get_dynamic_options", IndexedTypeTableKey);

#! The key of a restored chunk's action that holds the chunk's type description tables until the action is restored
/** The key is removed when the action is restored, so it is never visible outside of the module
*/
const IndexedTypeTableKey = "type_table";
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

    #! App to action map of the actions declaring an alternative to their app's scheme connection
    /** Each action whose metadata has a \c connection_alternative key is recorded as @ref True; the declaration
        itself is validated by its consumer.  @ref nothing if the index does not record the declarations, because
        it was published by an earlier version of this module.

        @see ProviderIndexUtil::tryGetConnectionAlternativeActions()

        @since ProviderIndexUtil 1.2
    */
    *hash<string, hash<string, bool>> altmap;

    #! Factory to factory info map
    /** The info of each indexed data provider factory as returned by
        \c DataProvider::AbstractDataProviderFactory::getInfoAsData() with type information, which is plain data;
        the unique keys in its type descriptions do not depend on the process that built the index.

        Every factory in \c factorymap that was registered when the index was built has an entry; a factory in
        \c factorymap without one registered none.  @ref nothing if the index does not record factory info, because
        it was published by an earlier version of this module, or its writer did not provide it.

        @see ProviderIndexUtil::tryGetIndexedFactoryInfo()

        @since ProviderIndexUtil 1.2
    */
    *hash<string, hash<auto>> factoryinfomap;

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

    #! Maps apps to the failures that made the app or some of its actions unavailable when the index was built
    /** Each failure has the members of @ref ProviderIndexFailureInfo, the app's own failures first, then those of
        its actions in action name order.  An app that could not be initialized at all has no info in
        \c appinfomap; an app whose actions failed is indexed with the actions that did not fail.

        Written to and read from the metadata cache only, not the canonical index file, which earlier versions of
        this module read; @ref nothing if the index does not record failures, because it was published by an earlier
        version of this module, or its writer did not provide them.

        @see ProviderIndexUtil::getIndexedFailures()

        @since ProviderIndexUtil 1.2
    */
    *hash<string, list<hash<auto>>> failuremap;
}

#! An app or action that could not be initialized when the data provider index was built
/** The app or action is unavailable in the index: an app whose own initialization failed has no info in the index,
    unless it registered its info before failing, and an action that failed is not in the index.  The other apps and
    actions of the index are not affected.

    @see ProviderIndexUtil::getIndexedFailures()

    @since ProviderIndexUtil 1.2
*/
public hashdecl ProviderIndexFailureInfo {
    #! The unit that failed: \c app, \c action, or \c scheme (the app's connection scheme is not registered)
    string kind;

    #! The app name
    string app;

    #! The action name, for an action failure
    *string action;

    #! The phase in which the failure occurred
    /** One of:
        - \c registration: the app's or action's initialization raised an error
        - \c expected-inventory: the app or action was declared, but was not registered by its initialization
        - \c publication: the app has no module path, or its connection scheme is not registered
        - \c serialize-action-metadata: the action's metadata is not plain data
    */
    string phase;

    #! The original error code
    string error;

    #! The original error description
    string description;

    #! The name of the module that registered the app or action, if known
    *string module;
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
    #! The \c factorymap, \c appmap, \c schememap, \c altmap, \c factoryinfomap, \c failuremap, \c appinfomap, and \c actioninfomap members of the index
    /** App and action metadata is in the serialized form stored in the index, as in the YAML written by
        @ref ProviderIndexUtil::dumpIndexYaml() "ProviderIndexUtil::dumpIndexYaml()"; \c altmap,
        \c factoryinfomap, and \c failuremap are only present if the index records them
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
    TypedHash action_summary_th;
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
    # the descriptions of the types in type_map
    hash<auto> type_info_map;
    hash<auto> shape_map;
    *hash<auto> ui_view_map;
    # DataProvider::getIndexedInfo(), which converts the data provider info of a registered action
    code get_indexed_dp_info;
    # DataProvider::DataProviderInfoOptionMembers
    list<string> dp_option_members;
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
    # the actions declaring an alternative to their app's scheme connection (version 4)
    *hash<string, hash<string, bool>> altmap;
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
    # the size and digest of the file holding the info of the indexed factories (version 6), with the members of
    # DataProviderMetadataCacheChunkInfo; not set if the index does not record it; a nested hashdecl member is not
    # converted when the unpacked manifest is cast
    *hash<auto> factories;
    # the size and digest of the file holding the action summaries of every app (version 8), with the members of
    # DataProviderMetadataCacheChunkInfo; not set if the index does not record them
    *hash<auto> actions;
    # the size and digest of the file holding the failures of the apps and actions that could not be initialized
    # (version 9), with the members of DataProviderMetadataCacheChunkInfo; not set if the index does not record them
    *hash<auto> failures;
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

#! The result of reloading the published data provider index
/** @see ProviderIndexUtil::reloadIndex()

    @since ProviderIndexUtil 1.2
*/
public hashdecl ProviderIndexReloadInfo {
    #! @ref True if the published index was read and is now used by the process
    /** @ref False if it could not be read, in which case the process keeps using the index it used before, and the
        error is in \c error
    */
    bool reloaded = False;

    #! The digest of the routing index of the index now in use
    /** This is the digest returned for the index by
        @ref ProviderIndexUtil::ProviderIndexUtil::getSerializedDataProviderIndexSource() "ProviderIndexUtil::getSerializedDataProviderIndexSource()";
        @ref nothing if no index is published, or if the index was read from its canonical index file
    */
    *string sha256;

    #! The digest of the routing index of the index used before the reload, if any
    *string previous_sha256;

    #! The sorted names of the apps in the index read that were not in the previous index
    list<string> added = ();

    #! The sorted names of the apps in the previous index that are not in the index read
    list<string> removed = ();

    #! The sorted names of the apps in both indexes whose metadata or module dependencies are different
    /** An app is also reported as changed if its metadata in the two indexes cannot be compared; see
        @ref ProviderIndexUtil::ProviderIndexUtil::reloadIndex() "ProviderIndexUtil::reloadIndex()"
    */
    list<string> changed = ();

    #! The error reading the published index, if it was not reloaded
    /** The error is also reported by
        @ref ProviderIndexUtil::ProviderIndexUtil::getIndexReadError() "ProviderIndexUtil::getIndexReadError()"
    */
    *string error;
}

#! The in-memory state of a data provider index as read from its files
hashdecl IndexStateInfo {
    # the routing maps of the index; its metadata maps are empty
    hash<DataProviderIndexInfo> index;
    # serialized app and action metadata not yet materialized
    hash<string, hash<auto>> raw_appinfomap = {};
    hash<string, hash<string, hash<auto>>> raw_actioninfomap = {};
    # set if the metadata is read from the metadata cache or the canonical index file when needed
    bool deferred = False;
    *string metadata_cache_sha256;
    *int metadata_cache_version;
    *hash<DataProviderMetadataCacheManifest> manifest;
    bool manifest_checked = False;
    # the digest of the routing index, if the index was read from it
    *string sha256;
    # the apps of an index read from its canonical index file
    *hash<string, bool> canonical_apps;
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

        #! Set when the factory info of the index has been read, or found not to be recorded; the info is then in
        #! \c index.factoryinfomap
        static bool factory_info_checked;

        #! Set when the failures recorded by the index have been read, or found not to be recorded; they are then in
        #! \c index.failuremap
        static bool failure_info_checked;

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

        #! The generation of the in-memory index state, incremented whenever the state is replaced
        /** A lookup that materializes metadata and then reads it retries if the generation changed in between, so
            it never combines the state of two indexes
        */
        static int index_generation = 0;

        #! The digest of the routing index of the index in use, if it was read from its routing index
        static *string index_sha256;

        #! The apps of the index in use, if it was read from its canonical index file
        static *hash<string, bool> canonical_apps;

        #! Set while the index is reloaded; reloads are serialized
        static bool reloading = False;

        #! The thread reloading the index
        static *int reload_tid;

        #! Condition for threads waiting for a reload to finish
        static Condition reload_cond();

        #! The last error reading the index while it is reloaded; only set by the reloading thread
        static *string reload_read_error;

        #! App info added to the cache by the process, kept when the index is reloaded
        static hash<string, hash<auto>> live_appinfomap;

        #! Actions added to the cache by the process, as indexed actions, kept when the index is reloaded
        static hash<string, hash<string, hash<auto>>> live_actioninfomap;

        #! Apps removed from the cache by the process, whose indexed metadata stays hidden when the index is reloaded
        static hash<string, bool> removed_apps;

        #! The action summaries of the indexed actions, by app and action; read when they are first needed
        /** Actions added to the cache by the process and apps removed from it are applied when the summaries are
            returned
        */
        static hash<string, hash<string, hash<auto>>> action_summaries = {};

        #! Set when the action summaries of the index in use have been read
        static bool action_summaries_loaded;

        #! The action summaries of the actions added to the cache by the process, kept when the index is reloaded
        static hash<string, hash<string, hash<auto>>> live_action_summaries = {};

        #! The indexed actions of each app that have been restored on their own, or that could not be restored
        /** Their serialized form stays in \c raw_actioninfomap until all the actions of their app are restored, so
            that the order of the app's actions is kept, and because the actions of an app share its type
            description tables
        */
        static hash<string, hash<string, bool>> action_restored = {};
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
            # a reload reads the published index, which build mode never does
            while (reloading) {
                if (reload_tid == gettid()) {
                    throw "PROVIDER-INDEX-ERROR", "cannot change the index build mode while the data provider "
                        "index is being reloaded in the same thread";
                }
                reload_cond.wait(m);
            }
            waitForIndexReaders("change the index build mode");
            build_mode = build;
            resetIndexState();
        }
        return rv;
    }

    #! Waits until no thread is reading the index into the in-memory index state; must be called with the lock held
    /** @param action what the caller is doing, for the exception

        @throw PROVIDER-INDEX-ERROR the calling thread is reading the index
    */
    static private waitForIndexReaders(string action) {
        # do not replace the state while another thread is reading the index into it
        while (initializing || raw_metadata_loading || metadata_initializing || app_metadata_initializing
                || action_metadata_initializing) {
            if (initializing_tid == gettid() || raw_metadata_loading_tid == gettid()
                    || metadata_initializing_tid == gettid() || hasThread(app_metadata_initializing)
                    || hasThread(action_metadata_initializing)) {
                throw "PROVIDER-INDEX-ERROR", sprintf("cannot %s while the data provider index is being read in "
                    "the same thread", action);
            }
            if (initializing) {
                init_cond.wait(m);
            } else {
                metadata_init_cond.wait(m);
            }
        }
    }

    #! Returns @ref True if the current thread is one of the threads in the given map of names to thread IDs
    static private bool hasThread(hash<string, int> tids) {
        int tid = gettid();
        foreach int t in (tids.iterator()) {
            if (t == tid) {
                return True;
            }
        }
        return False;
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
    /** The cache edits made by the process are discarded too
    */
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
        factory_info_checked = False;
        failure_info_checked = False;
        apps_loaded = False;
        remove index_sha256;
        remove canonical_apps;
        live_appinfomap = {};
        live_actioninfomap = {};
        removed_apps = {};
        action_summaries = {};
        action_summaries_loaded = False;
        live_action_summaries = {};
        action_restored = {};
        ++index_generation;
    }

    #! Makes the given index state the in-memory index state; must be called with the lock held
    /** The cache edits made by the process are applied to the new state, so an app, action, or removal from the
        cache is not lost when the index is read or reloaded
    */
    static private installIndexState(hash<IndexStateInfo> state) {
        index = state.index;
        raw_appinfomap = state.raw_appinfomap;
        raw_actioninfomap = state.raw_actioninfomap;
        app_metadata_initialized = {};
        action_metadata_initialized = {};
        raw_metadata_deferred = state.deferred;
        raw_metadata_app_loaded = {};
        raw_metadata_apps_loaded = False;
        metadata_cache_sha256 = state.metadata_cache_sha256;
        metadata_cache_version = state.metadata_cache_version;
        metadata_cache_manifest = state.manifest;
        metadata_cache_manifest_checked = state.manifest_checked;
        factory_info_checked = False;
        failure_info_checked = False;
        apps_loaded = False;
        index_sha256 = state.sha256;
        canonical_apps = state.canonical_apps;
        action_summaries = {};
        action_summaries_loaded = False;
        action_restored = {};
        initialized = True;
        ++index_generation;

        # a removed app's indexed metadata stays hidden, even if it has been added again since
        foreach string app in (keys removed_apps) {
            remove raw_appinfomap{app};
            remove raw_actioninfomap{app};
            app_metadata_initialized{app} = True;
            action_metadata_initialized{app} = True;
            remove index.altmap{app};
        }
        foreach hash<auto> i in (live_appinfomap.pairIterator()) {
            index.appinfomap{i.key} = i.value;
            remove raw_appinfomap{i.key};
            app_metadata_initialized{i.key} = True;
        }
        # the indexed actions of an app are merged with the actions added by the process when they are restored
        foreach hash<auto> i in (live_actioninfomap.pairIterator()) {
            foreach hash<auto> action in (i.value.iterator()) {
                setCachedActionIntern(action);
            }
        }
    }

    #! Sets the given indexed action in the in-memory index; must be called with the lock held
    static private setCachedActionIntern(hash<auto> action) {
        index.actioninfomap{action.app}{action.action} = action;
        index.top_actioninfomap{action.app + "/" + action.action} = action;
        # an index that does not record the declarations is not completed by one action
        if (exists index.altmap) {
            if (declaresConnectionAlternative(action)) {
                index.altmap{action.app}{action.action} = True;
            } else if (index.altmap{action.app}) {
                remove index.altmap{action.app}{action.action};
                if (!index.altmap{action.app}) {
                    remove index.altmap{action.app};
                }
            }
        }
    }

    #! Merges the indexed actions of an app with the actions already in the cache; must be called with the lock held
    /** @param app the app name
        @param actions the app's indexed actions

        Actions added by the process win over indexed actions with the same name
    */
    static private mergeIndexedActions(string app, *hash<auto> actions) {
        index.actioninfomap{app} = (actions ?? {}) + (index.actioninfomap{app} ?? {});
        foreach hash<auto> action in (index.actioninfomap{app}.pairIterator()) {
            index.top_actioninfomap{app + "/" + action.key} = action.value;
        }
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

    #! Reloads the published data provider index
    /** Reads the index currently published, as after it has been regenerated, and makes the process use it in place
        of the index it read before; a process otherwise reads the published index once, when it is first needed.

        The routing index and the metadata cache manifest of the published index are read and verified before the
        process uses the index; the metadata of each app is read when it is first needed, as when the index is first
        read.  If the index cannot be read, the process keeps using the index it used before, and the error is
        returned and reported by @ref getIndexReadError().

        The index is replaced atomically: the metadata of the index is never mixed with that of the index used
        before, and lookups in other threads never fail because the index is reloaded; a lookup that started before
        the index was replaced returns its result from the index used before, or from the reloaded index.

        App info and actions added to the cache with @ref addAppToCache() and @ref addActionToCache() are kept, and
        apps removed from the cache with @ref removeCachedApp() stay removed; modules already loaded stay loaded.

        The apps whose metadata has changed are reported, so that state derived from their metadata can be
        refreshed: an app is reported as changed if its metadata or module dependencies are different, or if its
        metadata in the two indexes cannot be compared, which is the case if either index was read from its canonical
        index file, or if the metadata cache manifest of the index used before was not read and is no longer
        published.  No app is reported if the routing index of the two indexes is identical.

        @return the result of the reload; see @ref ProviderIndexReloadInfo

        @throw PROVIDER-INDEX-ERROR the process is in index build mode (see @ref setBuildMode()), in which the
        published index is never read, or the calling thread is reading the index or reloading it

        @par Example:
        @code{.py}
# regenerate the index, then use it in the running process
hash<ProviderIndexReloadInfo> info = ProviderIndexUtil::reloadIndex();
if (!info.reloaded) {
    throw "INDEX-ERROR", sprintf("the regenerated index cannot be read: %s", info.error);
}
map refreshTools($1), info.added + info.changed;
map removeTools($1), info.removed;
        @endcode

        @note A reload can fail if the index is published again while it is read, since the files of an index that
        is replaced are removed; it can then be reloaded again

        @see getIndexSha256()

        @since ProviderIndexUtil 1.2
    */
    static hash<ProviderIndexReloadInfo> reloadIndex() {
        # the index used before is read first, so that its changes can be reported
        checkDataIndex();

        # the index used before, for the changes
        bool old_initialized;
        *string old_sha256;
        *hash<string, hash<string, bool>> old_appmap;
        *hash<DataProviderMetadataCacheManifest> old_manifest;
        bool old_manifest_checked;
        *string old_metadata_cache_sha256;
        *int old_metadata_cache_version;
        *hash<string, bool> old_canonical_apps;
        {
            m.lock();
            on_exit m.unlock();
            if (build_mode) {
                throw "PROVIDER-INDEX-ERROR", "cannot reload the data provider index in index build mode, in which "
                    "the published index is never read";
            }
            while (reloading) {
                if (reload_tid == gettid()) {
                    throw "PROVIDER-INDEX-ERROR", "cannot reload the data provider index while it is being reloaded "
                        "in the same thread";
                }
                reload_cond.wait(m);
            }
            waitForIndexReaders("reload the data provider index");
            # build mode cannot be entered while the index is reloaded
            if (build_mode) {
                throw "PROVIDER-INDEX-ERROR", "cannot reload the data provider index in index build mode, in which "
                    "the published index is never read";
            }
            reloading = True;
            reload_tid = gettid();
            remove reload_read_error;

            old_initialized = initialized;
            if (initialized) {
                old_sha256 = index_sha256;
                old_appmap = index.appmap;
                old_manifest = metadata_cache_manifest;
                old_manifest_checked = metadata_cache_manifest_checked;
                old_metadata_cache_sha256 = metadata_cache_sha256;
                old_metadata_cache_version = metadata_cache_version;
                old_canonical_apps = canonical_apps;
            }
        }
        on_exit {
            m.lock();
            reloading = False;
            remove reload_tid;
            remove reload_read_error;
            reload_cond.broadcast();
            m.unlock();
        }

        hash<string, *string> old_apps = {};
        if (old_initialized) {
            # the manifest of the index used before is read if it has not been read yet; its files are removed when
            # an index is published, in which case its apps cannot be compared
            if (!old_manifest_checked && old_metadata_cache_sha256) {
                old_manifest = readMetadataCacheManifestFile(old_metadata_cache_sha256,
                    old_metadata_cache_version ?? MetadataCacheVersion, False);
            }
            old_apps = getIndexAppDigests(old_manifest, old_canonical_apps, old_appmap);
        }

        hash<IndexStateInfo> state = readIndexState(True);

        m.lock();
        on_exit m.unlock();
        # set only in this thread
        if (*string err = reload_read_error) {
            # the process keeps using its index
            index_read_error = err;
            return <ProviderIndexReloadInfo>{
                "sha256": index_sha256,
                "previous_sha256": index_sha256,
                "error": err,
            };
        }

        hash<ProviderIndexReloadInfo> rv = <ProviderIndexReloadInfo>{
            "reloaded": True,
            "sha256": state.sha256,
            "previous_sha256": old_sha256,
        };
        # the same routing index identifies the same index
        if (!old_sha256 || old_sha256 != state.sha256) {
            hash<string, *string> new_apps = getIndexAppDigests(state.manifest, state.canonical_apps,
                state.index.appmap);
            foreach string app in (sort(keys new_apps)) {
                if (!old_apps.hasKey(app)) {
                    rv.added += app;
                } else if (!exists old_apps{app} || old_apps{app} != new_apps{app}
                        || (old_appmap{app} ?? {}) != (state.index.appmap{app} ?? {})) {
                    rv.changed += app;
                }
            }
            foreach string app in (sort(keys old_apps)) {
                if (!new_apps.hasKey(app)) {
                    rv.removed += app;
                }
            }
        }

        # the state is not replaced while another thread is reading the index into it
        waitForIndexReaders("reload the data provider index");
        installIndexState(state);
        remove index_read_error;
        log(LoggerLevel::LevelInfo, "Reloaded the data provider index %y (previous index %y): %d app(s) added, %d "
            "removed, %d changed", rv.sha256, rv.previous_sha256, rv.added.size(), rv.removed.size(),
            rv.changed.size());
        return rv;
    }

    #! Returns the digest of the routing index of the data provider index used by the process
    /** The published index is read first if the process has not read it yet.

        @return the digest of the routing index of the index used by the process, which is the digest returned for
        the index by @ref getSerializedDataProviderIndexSource(); @ref nothing if no index is published, if the
        index was read from its canonical index file, or if the process is in index build mode

        @par Example:
        @code{.py}
# reload the index if it has been published again since the process read it
*hash<SerializedDataProviderIndexInfo> published =
    ProviderIndexUtil::getSerializedDataProviderIndexSource(ProviderIndexUtil::tryGetIndexDir(True));
if (published && published.sha256 != ProviderIndexUtil::getIndexSha256()) {
    ProviderIndexUtil::reloadIndex();
}
        @endcode

        @see reloadIndex()

        @since ProviderIndexUtil 1.2
    */
    static *string getIndexSha256() {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        return index_sha256;
    }

    #! Returns the apps of an index with the digests of their metadata
    /** @param manifest the metadata cache manifest of the index, if it has been read
        @param canonical_apps the apps of an index read from its canonical index file
        @param appmap the routing app map of the index

        @return the apps of the index with the digests of their metadata, which is @ref nothing for an app whose
        metadata has no known digest; if neither the manifest nor the apps of a canonical index file are known, the
        apps are those of the routing app map
    */
    static private hash<string, *string> getIndexAppDigests(*hash<DataProviderMetadataCacheManifest> manifest,
            *hash<string, bool> canonical_apps, *hash<string, hash<string, bool>> appmap) {
        if (manifest) {
            return (map {$1.key: $1.value.sha256}, manifest.apps.pairIterator()) ?? {};
        }
        return (map {$1: NOTHING}, keys (canonical_apps ?? appmap)) ?? {};
    }

    #! Returns a list of cached app info
    static *list<hash<auto>> getCachedApps() {
        return getAllAppInfo().values();
    }

    #! Searches the app info cache for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached applications that match the regular expression pattern in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        return map $1, getAllAppInfo().values(),
            regex($1.name, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cachde applications whose names or descriptions match a substring
    /** @param substr A substring to match in the \c name and \c desc fields; pending apps only are matched with the
        name
        @param ignore_case Ignore case when searching

        @return All cached applications that match the substring argument in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppSubstr(string substr, bool ignore_case = True) {
        *hash<string, hash<auto>> apps = getAllAppInfo();
        if (ignore_case) {
            substr = substr.upr();

            return map $1, apps.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, apps.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Try to return any cached app info for the given app
    static *hash<auto> tryGetAppInfo(string app) {
        return getConsistent(sub () { checkAppMetadata(app); }, auto sub () { return index.appinfomap{app}; });
    }

    #! Returns the info of all cached apps
    static private *hash<string, hash<auto>> getAllAppInfo() {
        return getConsistent(\ProviderIndexUtil::checkAllAppMetadata(), auto sub () { return index.appinfomap; });
    }

    #! Returns the cached actions of the given app
    static private *hash<string, hash<auto>> getAppActions(string app) {
        return getConsistent(sub () { checkActionMetadata(app); },
            auto sub () { return index.actioninfomap{app}; });
    }

    #! Returns all cached actions, keyed by \c "<app>/<action>"
    static private *hash<string, hash<auto>> getAllActions() {
        return getConsistent(\ProviderIndexUtil::checkAllActionMetadata(),
            auto sub () { return index.top_actioninfomap; });
    }

    #! Makes sure that metadata is in the cache, and returns it from the same index state
    /** @param check makes sure that the metadata is in the cache
        @param get returns the metadata; called with the lock held

        @return the value returned by @a get

        If the index is reloaded after @a check was called, it is called again for the reloaded index, so the
        metadata of two indexes is never mixed
    */
    static private auto getConsistent(code<nothing()> check, code<auto()> get) {
        while (True) {
            int gen = index_generation;
            check();
            m.lock();
            on_exit m.unlock();
            if (gen == index_generation) {
                return get();
            }
        }
    }

    #! Returns the indexed actions of the given app
    /** Reading indexed actions never loads the provider modules that implement them: option types are described by
        plain data in each option's \c type_info, the types in the data provider info are described by plain data
        too (\c DataProvider::IndexedDataProviderInfo), and output types are not indexed, so an indexed action has
        no objects and can be serialized.

        @param app the app name

        @return the app's indexed actions as \c DataProvider::DataProviderIndexedActionInfo hashes; @ref nothing if
        the app has no indexed actions

        @par Example:
        @code{.py}
*list<hash<auto>> actions = ProviderIndexUtil::tryGetActionMetadataForApp("OpenSearch");
        @endcode

        @note An action whose metadata cannot be read is logged and omitted

        @note A type object can be created from an option's \c type_info with
        \c DataProvider::AbstractDataProviderType::get(), and the data provider info with type objects with
        \c DataProvider::DataProvider::restoreIndexedInfo(); a restored type has the described structure and
        constraints but not any custom behavior of the original type's class.  The exact types are available from
        the registered action with \c DataProvider::DataProviderActionCatalog::getAppAction(), which loads the app's
        modules.

        @see tryGetActionMetadata()

        @since ProviderIndexUtil 1.2
    */
    static *list<hash<auto>> tryGetActionMetadataForApp(string app) {
        *hash<string, hash<auto>> actions = getAppActions(app);
        return actions ? actions.values() : NOTHING;
    }

    #! Returns the actions of the given app that declare an alternative to its scheme connection
    /** An action declares an alternative with a \c connection_alternative key in its metadata; the declaration
        itself is not validated here.  The declarations are recorded in the routing index, so no app's metadata is
        read and no provider module is loaded.

        @param app the app name

        @return a hash of the declaring actions' names, each with the value @ref True, which is empty if the app
        declares none or is not indexed; @ref nothing if the index does not record the declarations, because it
        was published by an earlier version of this module, in which case they can only be found by reading the
        app's metadata; see @ref hasConnectionAlternatives()

        @par Example:
        @code{.py}
*hash<string, bool> actions = ProviderIndexUtil::tryGetConnectionAlternativeActions("Discord");
if (!exists actions) {
    # the index predates the declarations: read the app's metadata instead
}
        @endcode

        @since ProviderIndexUtil 1.2
    */
    static *hash<string, bool> tryGetConnectionAlternativeActions(string app) {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        if (!exists index.altmap) {
            return NOTHING;
        }
        return index.altmap{app} ?? {};
    }

    #! Returns @ref True if the index records which actions declare an alternative to their app's scheme connection
    /** @return @ref True if @ref tryGetConnectionAlternativeActions() answers from the index; @ref False if the
        index was published by an earlier version of this module that does not record the declarations

        @since ProviderIndexUtil 1.2
    */
    static bool hasConnectionAlternatives() {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        return exists index.altmap;
    }

    #! Returns the names of the data provider factories in the index
    /** The names are read from the routing index, so no factory info is read and no provider module is loaded.

        @return the sorted names of the factories that the index maps to a module; @ref nothing if no index is
        published or the process is in index build mode

        @note A factory is only in the index if it was known when the index was built; a factory in the index is
        not necessarily registered in the current process, since its module is loaded on demand

        @see tryGetIndexedFactoryInfo()

        @since ProviderIndexUtil 1.2
    */
    static *list<string> getIndexedFactories() {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        return index.factorymap ? sort(keys index.factorymap) : NOTHING;
    }

    #! Returns the info of the given data provider factory from the index
    /** The info is read from the metadata cache when the info of any factory is first requested, and then kept in
        memory; no provider module is loaded.

        @param factory the factory name

        @return the factory info as returned by \c DataProvider::AbstractDataProviderFactory::getInfoAsData() with
        type information; @ref nothing if the factory has no info in the index, or if the index does not record
        factory info, because it was published by an earlier version of this module, in which case the factory's
        module must be loaded to get its info; see @ref hasIndexedFactoryInfo()

        @par Example:
        @code{.py}
*hash<auto> info = ProviderIndexUtil::tryGetIndexedFactoryInfo("swagger");
if (!info && !ProviderIndexUtil::hasIndexedFactoryInfo()) {
    # the index predates factory info: load the factory instead
    info = DataProvider::getFactoryEx("swagger").getInfoAsData(True);
}
        @endcode

        @note \c DataProvider::DataProvider::tryGetFactoryInfoAsData() returns the info of a factory from the
        registered factory, from the index, or by loading the factory's module, in that order

        @since ProviderIndexUtil 1.2
    */
    static *hash<auto> tryGetIndexedFactoryInfo(string factory) {
        return getFactoryInfoMap(){factory};
    }

    #! Returns the info of all data provider factories with info in the index
    /** @return the info of each factory with info in the index, keyed by factory name, as returned by
        \c DataProvider::AbstractDataProviderFactory::getInfoAsData() with type information; @ref nothing if the
        index does not record factory info; see @ref hasIndexedFactoryInfo()

        @par Example:
        @code{.py}
*hash<string, hash<auto>> factories = ProviderIndexUtil::getIndexedFactoryInfoMap();
        @endcode

        @since ProviderIndexUtil 1.2
    */
    static *hash<string, hash<auto>> getIndexedFactoryInfoMap() {
        return getFactoryInfoMap();
    }

    #! Returns @ref True if the index records the info of its data provider factories
    /** @return @ref True if @ref tryGetIndexedFactoryInfo() answers from the index; @ref False if no index is
        published, the process is in index build mode, or the index was published by an earlier version of this
        module, or its factory info cannot be read, in which case the error is reported by
        @ref getIndexReadError()

        @since ProviderIndexUtil 1.2
    */
    static bool hasIndexedFactoryInfo() {
        return exists getFactoryInfoMap();
    }

    #! Returns the factory info of the index, reading it on first use
    static private *hash<string, hash<auto>> getFactoryInfoMap() {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        if (!factory_info_checked) {
            # an index read from its canonical index file has its factory info already; an index read from its
            # routing index has it in its metadata cache
            if (metadata_cache_sha256 && !exists index.factoryinfomap) {
                index.factoryinfomap = readProcessFactoryInfo();
            }
            factory_info_checked = True;
        }
        return index.factoryinfomap;
    }

    #! Reads the factory info of the process's index from its metadata cache; must be called with the lock held
    /** @return the factory info, or @ref nothing if the index does not record it; if it cannot be read from the
        metadata cache, it is read from any canonical index file, and the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<string, hash<auto>> readProcessFactoryInfo() {
        *hash<DataProviderMetadataCacheManifest> manifest = readMetadataCacheManifest();
        if (manifest) {
            if (!manifest.factories) {
                return;
            }
            # an index that records factory info can have none, which is not the same as not recording it
            *hash<string, hash<auto>> rv = readFactoryInfoChunk(manifest.factories, manifest.version);
            if (exists rv) {
                return rv;
            }
        }
        # the metadata cache cannot be read; a canonical index file published with the index holds the same info
        return readCanonicalDataProviderIndex().factoryinfomap;
    }

    #! Returns the apps and actions that could not be initialized when the index was built
    /** An index is built even if some apps or actions cannot be initialized, for example because a resource that
        an app needs is not available; each such failure is recorded in the index, and the app or action is
        unavailable in it, while the other apps and actions are indexed normally.  The failures are read from the
        metadata cache when they are first requested, and then kept in memory; no provider module is loaded.

        @param app the app whose failures are returned; if not given, the failures of every app are returned

        @return the failures, the failures of each app in app name order, the app's own failures first, then those
        of its actions in action name order; empty if there are none; @ref nothing if the index does not record
        failures, because it was published by an earlier version of this module, or its failures cannot be read, in
        which case the error is reported by @ref getIndexReadError(); see @ref hasIndexedFailureInfo()

        @par Example:
        @code{.py}
foreach hash<ProviderIndexFailureInfo> failure in (ProviderIndexUtil::getIndexedFailures()) {
    printf("%s%s is unavailable: %s: %s\n", failure.app, failure.action ? "/" + failure.action : "",
        failure.error, failure.description);
}
        @endcode

        @see
        - tryGetAppFailure()
        - tryGetActionFailure()

        @since ProviderIndexUtil 1.2
    */
    static *list<hash<ProviderIndexFailureInfo>> getIndexedFailures(*string app) {
        *hash<string, list<hash<auto>>> failuremap = getFailureMap();
        if (!exists failuremap) {
            return NOTHING;
        }
        list<hash<ProviderIndexFailureInfo>> rv = ();
        foreach string failed_app in (exists app ? (app,) : sort(keys failuremap)) {
            map rv += cast<hash<ProviderIndexFailureInfo>>($1), failuremap{failed_app};
        }
        return rv;
    }

    #! Returns the failure that made the given app unavailable when the index was built, if any
    /** @param app the app name

        @return the app's own failure; @ref nothing if the app did not fail, if only some of its actions failed
        (see @ref tryGetActionFailure()), or if the index does not record failures; see @ref getIndexedFailures()

        @par Example:
        @code{.py}
if (*hash<ProviderIndexFailureInfo> failure = ProviderIndexUtil::tryGetAppFailure("Trello")) {
    throw "APP-UNAVAILABLE", sprintf("%s: %s", failure.error, failure.description);
}
        @endcode

        @since ProviderIndexUtil 1.2
    */
    static *hash<ProviderIndexFailureInfo> tryGetAppFailure(string app) {
        foreach hash<auto> failure in (getFailureMap(){app}) {
            if (!exists failure.action) {
                return cast<hash<ProviderIndexFailureInfo>>(failure);
            }
        }
    }

    #! Returns the failure that made the given action unavailable when the index was built, if any
    /** @param app the app name
        @param action the action name

        @return the action's failure, or, if the action has none, the failure of its app, which makes all of its
        actions unavailable; @ref nothing if neither failed, or if the index does not record failures; see
        @ref getIndexedFailures()

        @par Example:
        @code{.py}
*hash<ProviderIndexFailureInfo> failure = ProviderIndexUtil::tryGetActionFailure("Trello", "create-card");
        @endcode

        @since ProviderIndexUtil 1.2
    */
    static *hash<ProviderIndexFailureInfo> tryGetActionFailure(string app, string action) {
        *hash<auto> app_failure;
        foreach hash<auto> failure in (getFailureMap(){app}) {
            if (failure.action === action) {
                return cast<hash<ProviderIndexFailureInfo>>(failure);
            }
            if (!exists failure.action && !app_failure) {
                app_failure = failure;
            }
        }
        return app_failure ? cast<hash<ProviderIndexFailureInfo>>(app_failure) : NOTHING;
    }

    #! Returns @ref True if the index records the apps and actions that could not be initialized when it was built
    /** @return @ref True if @ref getIndexedFailures() answers from the index; @ref False if no index is published,
        the process is in index build mode, the index was published by an earlier version of this module or read
        from its canonical index file, or its failures cannot be read, in which case the error is reported by
        @ref getIndexReadError()

        @since ProviderIndexUtil 1.2
    */
    static bool hasIndexedFailureInfo() {
        return exists getFailureMap();
    }

    #! Returns the failures recorded by the index, reading them on first use
    static private *hash<string, list<hash<auto>>> getFailureMap() {
        checkDataIndex();
        m.lock();
        on_exit m.unlock();
        if (!failure_info_checked) {
            # the failures are only recorded in the metadata cache; the canonical index file does not hold them
            if (metadata_cache_sha256 && !exists index.failuremap) {
                index.failuremap = readProcessFailureInfo();
            }
            failure_info_checked = True;
        }
        return index.failuremap;
    }

    #! Reads the failures recorded by the process's index from its metadata cache; must be called with the lock held
    /** @return the failures, or @ref nothing if the index does not record them, or if they cannot be read, in which
        case the error is reported by @ref getIndexReadError()
    */
    static private *hash<string, list<hash<auto>>> readProcessFailureInfo() {
        *hash<DataProviderMetadataCacheManifest> manifest = readMetadataCacheManifest();
        if (!manifest.failures) {
            return;
        }
        return readFailureInfoChunk(manifest.failures, manifest.version);
    }

    #! Returns @ref True if the given action declares an alternative to its app's scheme connection
    /** @param action an action in serialized, indexed, or registered form
    */
    static private bool declaresConnectionAlternative(*hash<auto> action) {
        return exists action.metadata.connection_alternative;
    }

    #! Returns the app to action map of the actions declaring an alternative to their app's scheme connection
    /** @param actioninfomap the app to action map of all actions
    */
    static private hash<string, hash<string, bool>> getConnectionAlternativeMap(*hash<auto> actioninfomap) {
        hash<string, hash<string, bool>> rv = {};
        foreach hash<auto> app in (actioninfomap.pairIterator()) {
            foreach hash<auto> action in (app.value.pairIterator()) {
                if (declaresConnectionAlternative(action.value)) {
                    rv{app.key}{action.key} = True;
                }
            }
        }
        return rv;
    }

    #! Returns the given indexed action
    /** Only this action is restored: the other actions of its app are not, and of the type descriptions of the app,
        only those that the action refers to are restored.  Descriptions shared with the app's other actions are
        restored once, when they are first needed.

        @param app the app name
        @param action the action name

        @return the action as a \c DataProvider::DataProviderIndexedActionInfo hash; @ref nothing if the action is
        not indexed, or if its metadata cannot be read, in which case the error is logged once

        @par Example:
        @code{.py}
*hash<auto> action = ProviderIndexUtil::tryGetActionMetadata("OpenSearch", "search-index");
        @endcode

        @see
        - tryGetActionMetadataForApp() for how option types are described
        - tryGetActionSummary() for the action-level fields of an action, which restores no type

        @since ProviderIndexUtil 1.2
    */
    static *hash<auto> tryGetActionMetadata(string app, string action) {
        return getConsistent(sub () { checkSingleActionMetadata(app, action); },
            auto sub () { return index.actioninfomap{app}{action}; });
    }

    #! Returns the action summaries of the given app
    /** An action summary holds the action-level fields of an indexed action (its names, descriptions, codes, groups,
        metadata, and path variables), the names of its options, and whether its data provider info declares
        expressions, but not its options or data provider info, so no type description is read or restored: the
        summaries of every app are read from one file of the index when they are first needed, and the metadata of
        the apps is not read.  Actions can be listed and selected with their summaries, and only the selected actions
        restored with @ref tryGetActionMetadata().

        Actions added to the cache with @ref addActionToCache() replace or add to the indexed actions, and the actions
        of apps removed with @ref removeCachedApp() are not returned, as for the other action lookups.

        @param app the app name

        @return the app's action summaries as \c DataProvider::DataProviderIndexedActionSummaryInfo hashes, in index
        order; @ref nothing if the app has no indexed or cached actions

        @par Example:
        @code{.py}
# restore only the actions that send messages
list<hash<auto>> actions = map ProviderIndexUtil::tryGetActionMetadata($1.app, $1.action),
    ProviderIndexUtil::tryGetActionSummariesForApp("Slack") ?? (), $1.action_code == DPAT_SEND_MESSAGE;
        @endcode

        @note An index published before action summaries were recorded (ProviderIndexUtil 1.2 metadata cache
        versions 4 to 7) has its summaries read from the metadata of each of its apps, still without restoring any
        type; this is logged once, since recreating the index makes it faster

        @note An action whose metadata cannot be restored can have a summary; @ref tryGetActionMetadata() then
        returns @ref nothing for it

        @see
        - tryGetActionSummary()
        - getAllActionSummaries()
        - selectActionSummaries()

        @since ProviderIndexUtil 1.2
    */
    static *list<hash<auto>> tryGetActionSummariesForApp(string app) {
        *hash<string, hash<auto>> actions = getActionSummaries(app){app};
        return actions ? actions.values() : NOTHING;
    }

    #! Returns the action summary of the given action
    /** @param app the app name
        @param action the action name

        @return the action summary as a \c DataProvider::DataProviderIndexedActionSummaryInfo hash; @ref nothing if
        the action is not indexed or cached

        @par Example:
        @code{.py}
*hash<auto> summary = ProviderIndexUtil::tryGetActionSummary("OpenSearch", "search-index");
        @endcode

        @see tryGetActionSummariesForApp() for what an action summary holds

        @since ProviderIndexUtil 1.2
    */
    static *hash<auto> tryGetActionSummary(string app, string action) {
        return getActionSummaries(app){app}{action};
    }

    #! Returns the action summaries of all apps
    /** @return the action summaries as \c DataProvider::DataProviderIndexedActionSummaryInfo hashes keyed by app,
        then by action, in index order; apps with no actions are not included

        @par Example:
        @code{.py}
int count = foldl $1 + $2, (map $1.size(), ProviderIndexUtil::getAllActionSummaries().iterator()) ?? 0;
        @endcode

        @see tryGetActionSummariesForApp() for what an action summary holds

        @since ProviderIndexUtil 1.2
    */
    static hash<string, hash<string, hash<auto>>> getAllActionSummaries() {
        return getActionSummaries();
    }

    #! Returns the action summaries selected by the given code
    /** @param selector called with each action summary; the summaries for which it returns @ref True are returned
        @param app if given, only the summaries of this app are passed to @a selector

        @return the selected action summaries as \c DataProvider::DataProviderIndexedActionSummaryInfo hashes, in
        index order

        @par Example:
        @code{.py}
# restore only the actions marked for alert delivery
list<hash<auto>> actions = map ProviderIndexUtil::tryGetActionMetadata($1.app, $1.action),
    ProviderIndexUtil::selectActionSummaries(bool sub (hash<auto> action) {
        return action.metadata.alert_delivery_capable ?? False;
    });
        @endcode

        @note an exception thrown by @a selector propagates

        @see tryGetActionSummariesForApp() for what an action summary holds

        @since ProviderIndexUtil 1.2
    */
    static list<hash<auto>> selectActionSummaries(code<bool(hash<auto>)> selector, *string app) {
        list<hash<auto>> rv = ();
        foreach hash<auto> app_actions in (getActionSummaries(app).iterator()) {
            foreach hash<auto> action in (app_actions.iterator()) {
                if (selector(action)) {
                    push rv, action;
                }
            }
        }
        return rv;
    }

    #! Returns the action summaries of all apps, or of the given app, keyed by app, then by action
    static private hash<string, hash<string, hash<auto>>> getActionSummaries(*string app) {
        return getConsistent(\ProviderIndexUtil::checkActionSummaries(),
            auto sub () { return getActionSummariesIntern(app); });
    }

    #! Returns the action summaries of all apps, or of the given app, with the cache edits applied
    /** Must be called with the lock held
    */
    static private hash<string, hash<string, hash<auto>>> getActionSummariesIntern(*string app) {
        hash<string, hash<string, hash<auto>>> rv = {};
        list<string> apps = exists app ? (app,) : keys (action_summaries + live_action_summaries);
        foreach string name in (apps) {
            hash<string, hash<auto>> actions = {};
            # a removed app's indexed actions stay hidden
            if (!removed_apps{name} && action_summaries{name}) {
                actions = action_summaries{name};
            }
            if (live_action_summaries{name}) {
                actions += live_action_summaries{name};
            }
            if (actions) {
                rv{name} = actions;
            }
        }
        return rv;
    }

    #! Returns the actions matching the given action summary code, restored, from one index state
    /** @param app if given, only the actions of this app are matched
        @param match called with each action summary

        @return the matching actions, restored; @ref nothing if there are no actions to match; an action whose metadata
        cannot be read is left out
    */
    static private *list<hash<auto>> searchActions(*string app, code<bool(hash<auto>)> match) {
        while (True) {
            int gen = index_generation;
            hash<string, hash<string, hash<auto>>> summaries = getActionSummaries(app);
            if (!summaries) {
                return;
            }
            list<hash<auto>> rv = ();
            foreach hash<auto> app_actions in (summaries.iterator()) {
                foreach hash<auto> summary in (app_actions.iterator()) {
                    if (!match(summary)) {
                        continue;
                    }
                    if (*hash<auto> action = tryGetActionMetadata(summary.app, summary.action)) {
                        push rv, action;
                    }
                }
            }
            # the actions of two indexes are never mixed
            if (gen == index_generation) {
                return rv;
            }
        }
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
        return searchActions(app, getRegexMatch(regex_pattern, regex_options));
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
        return searchActions(app, getSubstrMatch(substr, ignore_case));
    }

    #! Searches for cached actions whose descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c short_desc and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached actions that match the regular expression pattern in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionRegex(string regex_pattern,
            int regex_options = RE_Caseless) {
        return searchActions(NOTHING, getRegexMatch(regex_pattern, regex_options));
    }

    #! Searches for cached actions whose descriptions match a substring
    /** @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All cached actions that match the substring argument in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionSubstr(string substr, bool ignore_case = True) {
        return searchActions(NOTHING, getSubstrMatch(substr, ignore_case));
    }

    #! Returns code that matches actions whose descriptions match the given regular expression
    static private code<bool(hash<auto>)> getRegexMatch(string regex_pattern, int regex_options) {
        return bool sub (hash<auto> action) {
            return regex(action.short_desc ?? "", regex_pattern, regex_options)
                || regex(action.desc ?? "", regex_pattern, regex_options);
        };
    }

    #! Returns code that matches actions whose descriptions contain the given substring
    static private code<bool(hash<auto>)> getSubstrMatch(string substr, bool ignore_case) {
        if (ignore_case) {
            substr = substr.upr();
            return bool sub (hash<auto> action) {
                return ((action.short_desc ?? "").upr().find(substr) >= 0)
                    || ((action.desc ?? "").upr().find(substr) >= 0);
            };
        }
        return bool sub (hash<auto> action) {
            return ((action.short_desc ?? "").find(substr) >= 0) || ((action.desc ?? "").find(substr) >= 0);
        };
    }

    #! Returns all cached actions
    /** @return all cached actions as \c DataProvider::DataProviderIndexedActionInfo hashes
    */
    static *list<hash<auto>> getAllCachedActions() {
        return getAllActions().values();
    }

    #! Adds the given app and actions to the cache
    /** @param app the app info

        @note The app info replaces any indexed info of the app, and is kept when the index is reloaded with
        @ref reloadIndex()
    */
    static addAppToCache(hash<auto> app) {
        if (!app.name) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add app info to cache: app name is missing (%y)", app);
        }
        while (True) {
            int gen = index_generation;
            checkAppMetadata(app.name);
            m.lock();
            on_exit m.unlock();
            # the index was replaced after the app's metadata was checked
            if (gen != index_generation) {
                continue;
            }
            index.appinfomap{app.name} = app;
            remove raw_appinfomap{app.name};
            app_metadata_initialized{app.name} = True;
            live_appinfomap{app.name} = app;
            return;
        }
    }

    #! Adds the given action to the cache
    /** @param action a registered action as a \c DataProvider::DataProviderActionInfo hash; it is cached as a
        \c DataProvider::DataProviderIndexedActionInfo hash, like an indexed action

        @note The action replaces any indexed action with the same name, and is kept when the index is reloaded with
        @ref reloadIndex()
    */
    static addActionToCache(hash<auto> action) {
        if (!checkMetadataContext()) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action %y of app %y to the cache while the data "
                "provider index is initializing", action.action, action.app);
        }
        action = getIndexedAction(action, metadata_context);
        hash<auto> summary = castActionSummary(getSerializedActionSummary(action), metadata_context);
        while (True) {
            int gen = index_generation;
            checkAppMetadata(action.app);
            checkActionMetadata(action.app);
            m.lock();
            on_exit m.unlock();
            # the index was replaced after the app's metadata was checked
            if (gen != index_generation) {
                continue;
            }
            if (!index.appinfomap{action.app}) {
                throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action for unknown app %y to cache", action.app);
            }
            if (!action.action) {
                throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action info to cache for app %y: action name is "
                    "missing", action.app);
            }
            setCachedActionIntern(action);
            remove raw_actioninfomap{action.app}{action.action};
            action_metadata_initialized{action.app} = True;
            live_actioninfomap{action.app}{action.action} = action;
            live_action_summaries{action.app}{action.action} = summary;
            return;
        }
    }

    #! Removes the given app and all its actions from the cache
    /** @param app the app name

        @return @ref True if the app was in the cache and has been removed, @ref False if it was not in the cache

        @note A removed app's indexed metadata stays hidden when the index is reloaded with @ref reloadIndex(); only
        info and actions added again with @ref addAppToCache() and @ref addActionToCache() are cached for it
    */
    static bool removeCachedApp(string app) {
        while (True) {
            int gen = index_generation;
            # the app's actions are not restored: they are discarded
            checkAppMetadata(app);
            m.lock();
            on_exit m.unlock();
            # actions of the app being restored in another thread would be added after they are removed
            while (exists action_metadata_initializing{app} && action_metadata_initializing{app} != gettid()) {
                metadata_init_cond.wait(m);
            }
            # the index was replaced after the app's metadata was checked
            if (gen != index_generation) {
                continue;
            }
            if (!remove index.appinfomap{app}) {
                return False;
            }
            map remove index.top_actioninfomap{app + "/" + $1}, keys (remove index.actioninfomap{app});
            remove raw_actioninfomap{app};
            remove action_restored{app};
            # the app's indexed actions are never read again
            action_metadata_initialized{app} = True;
            remove index.altmap{app};
            remove live_appinfomap{app};
            remove live_actioninfomap{app};
            remove live_action_summaries{app};
            removed_apps{app} = True;
            return True;
        }
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

        hash<IndexStateInfo> state;
        try {
            # a process building the index never reads the index it replaces
            state = build ? getEmptyIndexState() : readIndexState(False);
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
        installIndexState(state);
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
        int gen = index_generation;
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
            # the index was replaced; the caller checks the replacement
            if (gen != index_generation) {
                return;
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
        int gen = index_generation;
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
            # the index was replaced after its raw metadata was loaded; the caller checks the replacement
            if (gen != index_generation) {
                return;
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
        int gen = index_generation;
        checkRawMetadataIndex(app);
        if (!initialized || action_metadata_initialized{app} || !checkMetadataContext()) {
            return;
        }

        hash<string, hash<auto>> raw_actions;
        # the actions restored on their own
        *hash<string, bool> restored;
        {
            m.lock();
            on_exit m.unlock();
            while (exists action_metadata_initializing{app} && action_metadata_initializing{app} != gettid()) {
                metadata_init_cond.wait(m);
            }
            # the index was replaced after its raw metadata was loaded; the caller checks the replacement
            if (gen != index_generation) {
                return;
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
            restored = action_restored{app};
        }

        hash<auto> action_map;
        try {
            action_map = deserializeActionsWithContext({app: getUnrestoredActions(raw_actions, restored)},
                metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing actions for app %y from the data provider "
                "index: %s", app, get_exception_string(ex));
        }

        m.lock();
        on_exit m.unlock();
        mergeIndexedActions(app, getRestoredAppActions(app, raw_actions, action_map{app}));
        remove raw_actioninfomap{app};
        remove action_restored{app};
        action_metadata_initialized{app} = True;
        remove action_metadata_initializing{app};
        metadata_init_cond.broadcast();
    }

    #! Returns the serialized actions of an app that have not been restored on their own
    /** @param raw_actions the app's serialized actions
        @param restored the app's actions restored on their own, or that could not be restored
    */
    static private hash<string, hash<auto>> getUnrestoredActions(hash<string, hash<auto>> raw_actions,
            *hash<string, bool> restored) {
        if (!restored) {
            return raw_actions;
        }
        return (map {$1.key: $1.value}, raw_actions.pairIterator(), !restored{$1.key}) ?? {};
    }

    #! Returns the restored actions of an app in index order; must be called with the lock held
    /** @param app the app name
        @param raw_actions the app's serialized actions, in index order
        @param restored the actions just restored

        @return the actions just restored and those restored on their own before, in index order; an action that
        could not be restored is left out
    */
    static private hash<auto> getRestoredAppActions(string app, hash<string, hash<auto>> raw_actions,
            *hash<auto> restored) {
        if (!action_restored{app}) {
            return restored ?? {};
        }
        hash<auto> rv = {};
        foreach string action in (keys raw_actions) {
            if (exists restored{action}) {
                rv{action} = restored{action};
            } else if (action_restored{app}{action} && exists index.actioninfomap{app}{action}) {
                rv{action} = index.actioninfomap{app}{action};
            }
        }
        return rv;
    }

    #! Ensures that the given indexed action has been restored, without restoring the other actions of its app
    /** Only the type descriptions that the action refers to are restored; the descriptions shared with other actions
        of the app are restored once, when first needed by any of them.  An action that cannot be restored is logged
        once and left out, as when all the actions of its app are restored.

        @param app the app name
        @param action the action name
    */
    static private checkSingleActionMetadata(string app, string action) {
        int gen = index_generation;
        checkRawMetadataIndex(app);
        if (!initialized || action_metadata_initialized{app} || !checkMetadataContext()) {
            return;
        }

        hash<auto> raw_action;
        {
            m.lock();
            on_exit m.unlock();
            while (exists action_metadata_initializing{app} && action_metadata_initializing{app} != gettid()) {
                metadata_init_cond.wait(m);
            }
            # the index was replaced after its raw metadata was loaded; the caller checks the replacement
            if (gen != index_generation) {
                return;
            }
            if (action_metadata_initialized{app} || exists action_metadata_initializing{app}) {
                return;
            }
            if (!exists raw_actioninfomap{app}) {
                action_metadata_initialized{app} = True;
                return;
            }
            if (!exists raw_actioninfomap{app}{action} || action_restored{app}{action}) {
                return;
            }
            action_metadata_initializing{app} = gettid();
            raw_action = raw_actioninfomap{app}{action};
        }

        *hash<auto> restored;
        try {
            restored = getIndexedAction(raw_action, metadata_context, True);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Omitting action %y of app %y from the data provider index metadata; it "
                "cannot be read: %s: %s", action, app, ex.err, ex.desc);
        }

        m.lock();
        on_exit m.unlock();
        # an action added to the cache by the process wins over the indexed action with the same name
        if (restored && !exists index.actioninfomap{app}{action}) {
            index.actioninfomap{app}{action} = restored;
            index.top_actioninfomap{app + "/" + action} = restored;
        }
        action_restored{app}{action} = True;
        remove action_metadata_initializing{app};
        metadata_init_cond.broadcast();
    }

    #! Ensures that all app metadata has been deserialized
    static private checkAllAppMetadata() {
        int gen = index_generation;
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
                if (hasThread(app_metadata_initializing)) {
                    return;
                }
                metadata_init_cond.wait(m);
            }
            # the index was replaced after its raw metadata was loaded; the caller checks the replacement
            if (gen != index_generation) {
                return;
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
        int gen = index_generation;
        checkRawMetadataIndex();
        if (!initialized || !checkMetadataContext()) {
            return;
        }

        hash<string, hash<string, hash<auto>>> raw_actions;
        list<string> apps;
        # the actions restored on their own
        hash<string, hash<string, bool>> restored;
        {
            m.lock();
            on_exit m.unlock();
            while (action_metadata_initializing.size()) {
                if (hasThread(action_metadata_initializing)) {
                    return;
                }
                metadata_init_cond.wait(m);
            }
            # the index was replaced after its raw metadata was loaded; the caller checks the replacement
            if (gen != index_generation) {
                return;
            }
            apps = select keys raw_actioninfomap, !action_metadata_initialized{$1};
            if (!apps) {
                return;
            }
            int tid = gettid();
            map action_metadata_initializing{$1} = tid, apps.iterator();
            raw_actions = map {$1: raw_actioninfomap{$1}}, apps.iterator();
            restored = action_restored;
        }

        hash<auto> action_map;
        try {
            action_map = deserializeActionsWithContext(
                (map {$1.key: getUnrestoredActions($1.value, restored{$1.key})}, raw_actions.pairIterator()),
                metadata_context);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error deserializing actions from the data provider index: %s",
                get_exception_string(ex));
            action_map = map {$1: {}}, apps.iterator();
        }

        m.lock();
        on_exit m.unlock();
        foreach string app in (apps) {
            mergeIndexedActions(app, getRestoredAppActions(app, raw_actions{app}, action_map{app}));
        }
        map remove raw_actioninfomap{$1}, apps.iterator();
        map remove action_restored{$1}, apps.iterator();
        map action_metadata_initialized{$1} = True, apps.iterator();
        action_metadata_initializing = {};
        metadata_init_cond.broadcast();
    }

    #! Ensures that the action summaries of the index in use have been read
    /** The summaries are read from the action summary file of the metadata cache; an index that predates it has
        them read from the metadata of each of its apps, without restoring any type description, and an index
        without a metadata cache from the metadata in memory
    */
    static private checkActionSummaries() {
        int gen = index_generation;
        checkDataIndex();
        if (!initialized || action_summaries_loaded || !checkMetadataContext()) {
            return;
        }

        bool from_cache;
        {
            m.lock();
            on_exit m.unlock();
            # the summaries are read like the other serialized metadata: by one thread, which a reload waits for
            while (raw_metadata_loading && raw_metadata_loading_tid != gettid()) {
                metadata_init_cond.wait(m);
            }
            # the index was replaced; the caller checks the replacement
            if (gen != index_generation || action_summaries_loaded || raw_metadata_loading) {
                return;
            }
            from_cache = exists metadata_cache_sha256;
            if (from_cache) {
                raw_metadata_loading = True;
                raw_metadata_loading_tid = gettid();
            }
        }

        *hash<string, hash<string, hash<auto>>> summaries;
        if (from_cache) {
            try {
                summaries = readCachedActionSummaries();
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
            raw_metadata_loading = False;
            remove raw_metadata_loading_tid;
            metadata_init_cond.broadcast();
            if (exists summaries) {
                if (gen == index_generation) {
                    action_summaries = summaries;
                    action_summaries_loaded = True;
                }
                return;
            }
        }

        # there is no metadata cache, or its manifest cannot be read: the summaries are taken from the serialized
        # and restored actions in memory, which are read from the canonical index file, if there is one
        checkRawMetadataIndex();
        hash<string, hash<string, hash<auto>>> serialized = {};
        {
            m.lock();
            on_exit m.unlock();
            if (gen != index_generation || action_summaries_loaded) {
                return;
            }
            foreach hash<auto> app in (raw_actioninfomap.pairIterator()) {
                serialized{app.key} = map {$1.key: getSerializedActionSummary($1.value)}, app.value.pairIterator();
            }
            foreach hash<auto> app in (index.actioninfomap.pairIterator()) {
                serialized{app.key} = (serialized{app.key} ?? {})
                    + (map {$1.key: getSerializedActionSummary($1.value)}, app.value.pairIterator());
            }
        }
        summaries = castActionSummaries(serialized, metadata_context);

        m.lock();
        on_exit m.unlock();
        if (gen == index_generation && !action_summaries_loaded) {
            action_summaries = summaries;
            action_summaries_loaded = True;
        }
    }

    #! Reads the action summaries of the process's index from its metadata cache
    /** @return the action summaries, or @ref nothing if the manifest of the metadata cache cannot be read; if the
        action summary file cannot be read, the error is reported by @ref getIndexReadError(), and the summaries are
        read from the metadata of each app, as for an index that predates the file
    */
    static private *hash<string, hash<string, hash<auto>>> readCachedActionSummaries() {
        *hash<DataProviderMetadataCacheManifest> manifest = readMetadataCacheManifest();
        if (!manifest) {
            return;
        }
        if (manifest.actions) {
            *string path;
            try {
                path = getMetadataCacheChunkPath("", manifest.actions.sha256, manifest.version);
                return castActionSummaries(readActionSummaryFile(path, manifest.actions), metadata_context);
            } catch (hash<ExceptionInfo> ex) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the action summaries of the data "
                    "provider index from %y: %s: %s", path ?? "<unknown>", ex.err, ex.desc));
            }
        } else {
            log(LoggerLevel::LevelInfo, "The data provider index was published before action summaries were "
                "recorded; they are read from the metadata of each app; recreate the index to list actions faster");
        }
        # the actions of each chunk are dropped as soon as their summaries are taken, and no type description is
        # restored
        hash<auto> serialized = {};
        foreach hash<auto> app in (manifest.apps.pairIterator()) {
            *hash<DataProviderIndexInfo> chunk = readMetadataCacheChunk(app.key, app.value, manifest.version);
            if (chunk.actioninfomap{app.key}) {
                serialized{app.key} = map {$1.key: getSerializedActionSummary($1.value)},
                    chunk.actioninfomap{app.key}.pairIterator();
            }
        }
        return castActionSummaries(serialized, metadata_context);
    }

    #! Reads and verifies an action summary file
    /** @param path the path of the file
        @param info the file's size and digest from the manifest

        @return the serialized action summaries, keyed by app, then by action

        @throw PROVIDER-INDEX-ERROR the file does not match its size or digest, or is not a hash of apps
    */
    static private hash<auto> readActionSummaryFile(string path, hash<auto> info) {
        binary data = ReadOnlyFile::readBinaryFile(path);
        if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
            throw "PROVIDER-INDEX-ERROR", "the file does not match the size and digest recorded in the metadata "
                "cache manifest";
        }
        return unpackActionSummaryChunk(data);
    }

    #! Returns the serialized action summary of the given action
    /** @param action an action in the serialized form stored in the index, or an indexed action

        @return the action-level members of the action, with the \c option_names and \c has_expressions members
    */
    static private hash<auto> getSerializedActionSummary(hash<auto> action) {
        # a typed hash cannot lose members
        hash<auto> rv = {} + action;
        rv -= ActionSummaryExcludedKeys;
        rv.option_names = action.options.typeCode() == NT_HASH ? keys action.options : ();
        rv.has_expressions = (action.data_provider_info.typeCode() == NT_HASH
            && action.data_provider_info.expressions) ? True : False;
        return rv;
    }

    #! Returns the given serialized action summaries as typed hashes
    /** @param serialized the serialized action summaries, keyed by app, then by action
        @param ctx the prepared deserialization context

        @return \c DataProvider::DataProviderIndexedActionSummaryInfo hashes keyed by app, then by action; a summary
        that cannot be read is logged and left out

        @throw PROVIDER-INDEX-ERROR the summaries of an app are not a hash
    */
    static private hash<string, hash<string, hash<auto>>> castActionSummaries(hash<auto> serialized,
            hash<MetadataDeserializeContext> ctx) {
        hash<string, hash<string, hash<auto>>> rv = {};
        foreach hash<auto> app in (serialized.pairIterator()) {
            if (app.value.typeCode() != NT_HASH) {
                throw "PROVIDER-INDEX-ERROR", sprintf("the action summaries of app %y must be a hash; got %s "
                    "instead", app.key, app.value.type());
            }
            hash<string, hash<auto>> actions = {};
            foreach hash<auto> action in (app.value.pairIterator()) {
                try {
                    if (action.value.typeCode() != NT_HASH) {
                        throw "PROVIDER-INDEX-ERROR", sprintf("the action summary must be a hash; got %s instead",
                            action.value.type());
                    }
                    actions{action.key} = castActionSummary(action.value, ctx);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::LevelError, "Omitting the summary of action %y of app %y from the data provider "
                        "index; it cannot be read: %s: %s", action.key, app.key, ex.err, ex.desc);
                }
            }
            if (actions) {
                rv{app.key} = actions;
            }
        }
        return rv;
    }

    #! Returns the given serialized action summary as a DataProviderIndexedActionSummaryInfo hash
    static private hash<auto> castActionSummary(hash<auto> summary, hash<MetadataDeserializeContext> ctx) {
        return ctx.action_summary_th.doCastWithOverrides(summary, {
            "path_vars": (summary.path_vars
                ? ctx.path_vars_th.doCastValues(summary.path_vars)
                : NOTHING),
        });
    }

    #! Packs the action summary file of a metadata cache
    /** @param summaries the serialized action summaries, keyed by app, then by action, in sorted order

        @return the compressed file contents

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available
    */
    static private binary packActionSummaryChunk(hash<auto> summaries) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot write data provider metadata";
%else
        return compress(msgpack_pack(summaries, MSGPACK_QORE_MODE));
%endif
    }

    #! Unpacks the action summary file of a metadata cache
    /** @param data the compressed file contents

        @return the serialized action summaries, keyed by app, then by action

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available, or the file does not hold a hash
    */
    static private hash<auto> unpackActionSummaryChunk(binary data) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        auto summaries = msgpack_unpack(uncompress_to_binary(data), MSGPACK_QORE_MODE);
        switch (summaries.typeCode()) {
            case NT_HASH:
                return summaries;
            case NT_NOTHING:
                return {};
        }
        throw "PROVIDER-INDEX-ERROR", sprintf("the action summary file must hold a hash; got %s instead",
            summaries.type());
%endif
    }

    #! Returns an empty data provider index with all map fields initialized
    static private hash<DataProviderIndexInfo> createEmptyDataProviderIndex() {
        return <DataProviderIndexInfo>{
            "factorymap": {},
            "appmap": {},
            "schememap": {},
            "altmap": {},
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

        @param maps the \c factorymap, \c appmap, \c schememap, and \c appinfomap members of the index, and
        optionally its \c factoryinfomap member; if \c factoryinfomap is not given, the index does not record factory
        info, and readers load the factory modules to get it; the info of a factory that is not in \c factorymap is
        not written, so an index read with @ref readSerializedDataProviderIndex() can be updated and written again;
        optionally also its \c failuremap member, the failures of the apps and actions that could not be initialized
        (see @ref DataProviderIndexInfo::failuremap), which are written to the metadata cache only; if it is not
        given, the index does not record failures
        @param action_apps the apps that have actions
        @param get_app_actions returns the actions of the given app, keyed by action name, in the serialized form
        stored in the index
        @param delta the time taken to create the index
        @param summary summary information about the index
        @param write_canonical_yaml if @ref True, the canonical index file is written too

        @return information about the written index

        @throw PROVIDER-INDEX-ERROR the index directory cannot be written to, or
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
        # the factory info is kept consistent with the factorymap, so that a caller that updates an index read
        # before, removing factories from its factorymap, never publishes the info of a factory that is not in it
        if (exists maps.factoryinfomap) {
            maps.factoryinfomap = (map {$1.key: $1.value}, maps.factoryinfomap.pairIterator(),
                exists maps.factorymap{$1.key}) ?? {};
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
            writeCanonicalMaps(write, maps, getCanonicalMapNames(exists maps.factoryinfomap));
        }

        bool actions_written;
        # the actions declaring an alternative to their app's scheme connection, recorded in the routing index so
        # that a consumer can find them without reading the metadata of every app
        hash<string, hash<string, bool>> altmap = {};
        # the action summaries of every app, written to a file of their own so that actions can be listed without
        # reading the metadata of any app
        hash<auto> action_summaries = {};
        foreach string app in (sort(keys app_set)) {
            *hash<auto> actions;
            if (action_app_set{app}) {
                actions = sortIndexMap(get_app_actions(app), False);
                foreach hash<auto> action in (actions.pairIterator()) {
                    if (declaresConnectionAlternative(action.value)) {
                        altmap{app}{action.key} = True;
                    }
                }
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
                    if (actions) {
                        action_summaries{app} = map {$1.key: getSerializedActionSummary($1.value)},
                            actions.pairIterator();
                    }
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
            # found while the actions were written, so it follows them
            writeCanonicalMaps(write, {"altmap": altmap}, ("altmap",));
            # ensure that the data is on disk before the file is renamed over the target
            f.sync();
            f.close();
            rename(tmp_path, path);
            remove tmp_path;
        }

        # the info of the indexed factories, if the index records it
        *hash<DataProviderMetadataCacheChunkInfo> factories;
%ifndef NoMsgPack
        if (write_cache && exists maps.factoryinfomap) {
            try {
                binary data = packFactoryInfoChunk(maps.factoryinfomap);
                string sha256 = digest("sha256", data).toHex();
                writeFileAtomic(getMetadataCacheChunkPath("", sha256, MetadataCacheVersion), data);
                factories = <DataProviderMetadataCacheChunkInfo>{
                    "size": data.size(),
                    "sha256": sha256,
                };
            } catch (hash<ExceptionInfo> ex) {
                if (!canonical) {
                    rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider factory info: %s: %s",
                        ex.err, ex.desc);
                }
                log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                    get_exception_string(ex));
                write_cache = False;
            }
        }
%endif

        # the failures of the apps and actions that could not be initialized, if the index records them
        *hash<DataProviderMetadataCacheChunkInfo> failures;
%ifndef NoMsgPack
        if (write_cache && exists maps.failuremap) {
            try {
                binary data = packFailureInfoChunk(maps.failuremap);
                string sha256 = digest("sha256", data).toHex();
                writeFileAtomic(getMetadataCacheChunkPath("", sha256, MetadataCacheVersion), data);
                failures = <DataProviderMetadataCacheChunkInfo>{
                    "size": data.size(),
                    "sha256": sha256,
                };
            } catch (hash<ExceptionInfo> ex) {
                if (!canonical) {
                    rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider failures: %s: %s",
                        ex.err, ex.desc);
                }
                log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                    get_exception_string(ex));
                write_cache = False;
            }
        }
%endif

        # the action summaries of every app
        *hash<DataProviderMetadataCacheChunkInfo> summaries;
%ifndef NoMsgPack
        if (write_cache) {
            try {
                binary data = packActionSummaryChunk(action_summaries);
                string sha256 = digest("sha256", data).toHex();
                writeFileAtomic(getMetadataCacheChunkPath("", sha256, MetadataCacheVersion), data);
                summaries = <DataProviderMetadataCacheChunkInfo>{
                    "size": data.size(),
                    "sha256": sha256,
                };
            } catch (hash<ExceptionInfo> ex) {
                if (!canonical) {
                    rethrow "PROVIDER-INDEX-ERROR", sprintf("cannot write the data provider action summaries: %s: "
                        "%s", ex.err, ex.desc);
                }
                log(LoggerLevel::LevelWarn, "Unable to write the optional data provider metadata cache: %s",
                    get_exception_string(ex));
                write_cache = False;
            }
        }
%endif
        remove action_summaries;

        *string cache_sha256;
        int cache_size = 0;
        if (write_cache) {
            try {
                binary manifest = packMetadataCacheManifest(chunks, factories, summaries, failures);
                cache_sha256 = digest("sha256", manifest).toHex();
                writeFileAtomic(getMetadataCachePath(cache_sha256), manifest);
                cache_size = manifest.size() + ((foldl $1 + $2, (map $1.size, chunks.iterator())) ?? 0)
                    + (factories.size ?? 0) + (summaries.size ?? 0) + (failures.size ?? 0);
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
        maps.altmap = altmap;
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

        removeUnreferencedIndexFiles(cache_sha256 ? chunks : NOTHING, cache_sha256, canonical,
            cache_sha256 ? factories.sha256 : NOTHING, cache_sha256 ? summaries.sha256 : NOTHING,
            cache_sha256 ? failures.sha256 : NOTHING);

        return <ProviderIndexInfo>{
            "path": canonical ? path : getIndexPath("routing"),
            "size": canonical ? size : routing_size + cache_size,
            "delta": delta,
            "summary": summary,
        };
    }

    #! Returns the names of the maps written first to the canonical index file, in the order written
    /** @param factory_info @ref True if the index records factory info
    */
    static private list<string> getCanonicalMapNames(bool factory_info) {
        list<string> rv = ("factorymap", "appmap", "schememap", "appinfomap");
        # an index that does not record factory info has no factoryinfomap member, as in earlier versions
        if (factory_info) {
            rv += "factoryinfomap";
        }
        return rv;
    }

    #! Writes the \c factorymap, \c appmap, \c schememap, and \c appinfomap members of the canonical index file
    /** @param write writes a string to the output
        @param maps the maps; every map is written, even if empty, so that the same index is always written
        identically
        @param names the maps to write, in the order written
    */
    static private writeCanonicalMaps(code<nothing(string)> write, hash<auto> maps,
            list<string> names = ("factorymap", "appmap", "schememap", "appinfomap")) {
        foreach string key in (names) {
            write(make_yaml({key: ((key == "appinfomap" || key == "factoryinfomap")
                ? sortIndexMap(maps{key}, False)
                : maps{key}) ?? {}}, BlockStyle));
        }
    }

    #! Writes the actions of one app to the \c actioninfomap member of the canonical index file
    /** @param write writes a string to the output
        @param app the app name
        @param actions the app's actions in the serialized form stored in the index, sorted by action name
        @param first @ref True if this is the first app written, in which case the \c actioninfomap key is written
        first
    */
    static private writeCanonicalAppActions(code<nothing(string)> write, string app, hash<auto> actions, bool first) {
        if (first) {
            write("actioninfomap:\n");
        }
        # each app's entry is indented under the top-level key; strings are written in quoted flow style, so
        # indenting every line leaves their values unchanged
        string yaml = make_yaml({app: actions}, BlockStyle);
        write("  " + replace(yaml.substr(0, -1), "\n", "\n  ") + "\n");
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
    /** The option type descriptions of the app's actions are stored in the chunk's tables of distinct type and
        field descriptions (see @ref IndexedTypeTableBuilder), and each option refers to its description by its
        position in the type table

        @param app the app name
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
        # the connection alternatives of all apps are recorded in the routing index
        remove app_index.altmap;
        if (exists app_info) {
            app_index.appinfomap{app} = app_info;
        }
        IndexedTypeTableBuilder types();
        if (exists actions) {
            hash<auto> app_actions = {};
            foreach hash<auto> action in (actions.pairIterator()) {
                app_actions{action.key} = types.addActionTypes(action.value);
            }
            app_index.actioninfomap{app} = app_actions;
        }
        # type descriptions repeat many keys and values, so chunks compress by about 10 times
        return compress(msgpack_pack({"index": app_index} + types.getTables(), MSGPACK_QORE_MODE));
%endif
    }

    #! Unpacks a metadata cache chunk
    /** @param data the compressed chunk
        @param version the metadata cache version of the chunk; by default, the current version, as for the chunks
        written by the workers of an index build
        @param restore @ref True if the actions will be restored by getIndexedAction(), in which case each action of
        a chunk that stores its type descriptions in tables refers to the tables with the @ref IndexedTypeTableKey
        key, so that each description is restored once; @ref False to return the actions in their serialized form,
        in which every option type description is a complete description

        @return the chunk's index, with the app's info and actions

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available, or a type description refers to a
        description that is not in the chunk

        @since ProviderIndexUtil 1.2
    */
    static private hash<DataProviderIndexInfo> unpackMetadataCacheChunk(binary data,
            int version = MetadataCacheVersion, bool restore = False) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        auto chunk = msgpack_unpack(uncompress_to_binary(data), MSGPACK_QORE_MODE);
        if (version < MetadataCacheTypeTableVersion) {
            return cast<hash<DataProviderIndexInfo>>(chunk);
        }
        if (chunk.typeCode() != NT_HASH || chunk.index.typeCode() != NT_HASH) {
            throw "PROVIDER-INDEX-ERROR", sprintf("a metadata cache chunk of version %d must be a hash with an "
                "\"index\" hash; got %s instead", version, chunk.typeCode() == NT_HASH ? chunk.index.type()
                : chunk.type());
        }
        hash<DataProviderIndexInfo> rv = cast<hash<DataProviderIndexInfo>>(chunk.index);
        if (!rv.actioninfomap) {
            return rv;
        }
        IndexedTypeTable types(chunk.types, chunk.fields);
        foreach hash<auto> app in (rv.actioninfomap.pairIterator()) {
            hash<auto> app_actions = {};
            foreach hash<auto> action in (app.value.pairIterator()) {
                if (restore) {
                    hash<auto> raw = action.value;
                    raw{IndexedTypeTableKey} = types;
                    app_actions{action.key} = raw;
                } else {
                    app_actions{action.key} = types.expandActionTypes(action.value);
                }
            }
            rv.actioninfomap{app.key} = app_actions;
        }
        return rv;
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
        @param factories the size and digest of the factory info file, if the index records factory info
        @param summaries the size and digest of the action summary file
        @param failures the size and digest of the failure file, if the index records failures

        @return the packed manifest
    */
    static private binary packMetadataCacheManifest(hash<string, DataProviderMetadataCacheChunkInfo> chunks,
            *hash<DataProviderMetadataCacheChunkInfo> factories, *hash<DataProviderMetadataCacheChunkInfo> summaries,
            *hash<DataProviderMetadataCacheChunkInfo> failures) {
        hash<DataProviderMetadataCacheManifest> manifest = <DataProviderMetadataCacheManifest>{
            "version": MetadataCacheVersion,
            "apps": chunks,
        };
        # a manifest without the member does not record factory info
        if (factories) {
            manifest.factories = factories;
        }
        if (summaries) {
            manifest.actions = summaries;
        }
        # a manifest without the member does not record failures
        if (failures) {
            manifest.failures = failures;
        }
        return packIndexData(manifest);
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
        @param factories_sha256 the digest of the factory info file that the manifest references, if any
        @param summaries_sha256 the digest of the action summary file that the manifest references, if any
        @param failures_sha256 the digest of the failure file that the manifest references, if any

        @par Example
        Uninstalling a provider module and rebuilding the index removes that app's chunk, so the index directory
        holds one chunk per indexed app and nothing else.

        @note Errors are logged and ignored: failing to remove a stale file must never fail an index write, because
        the index itself is already published at this point.
    */
    static private removeUnreferencedIndexFiles(*hash<string, DataProviderMetadataCacheChunkInfo> chunks,
            *string manifest_sha256, bool canonical, *string factories_sha256, *string summaries_sha256,
            *string failures_sha256) {
        *string dir = tryGetIndexDir();
        if (!dir.val()) {
            return;
        }
        hash<string, bool> keep = (map {basename(getMetadataCacheChunkPath($1.key, $1.value.sha256,
            MetadataCacheVersion)): True}, chunks.pairIterator()) ?? {};
        if (manifest_sha256) {
            keep{basename(getMetadataCachePath(manifest_sha256))} = True;
        }
        if (factories_sha256) {
            keep{basename(getMetadataCacheChunkPath("", factories_sha256, MetadataCacheVersion))} = True;
        }
        if (summaries_sha256) {
            keep{basename(getMetadataCacheChunkPath("", summaries_sha256, MetadataCacheVersion))} = True;
        }
        if (failures_sha256) {
            keep{basename(getMetadataCacheChunkPath("", failures_sha256, MetadataCacheVersion))} = True;
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
        each app's metadata, the factory info, the failures, and the action summaries against the sizes and digests
        recorded in the manifest.  The action summaries are not returned, since they are taken from the actions.

        @param dir the index directory
        @param per_app optional code called with each app's name, info, and actions, in app name order, as each
        app's metadata is read; the index then holds only its \c factorymap, \c appmap, \c schememap, and any
        \c altmap, \c factoryinfomap, and \c failuremap members, so the metadata of only one app is in memory at a
        time

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
        or the files are inconsistent, or the app is not in the index, or MessagePack support is not
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
            hash<auto> maps = {
                "factorymap": routing.factorymap,
                "appmap": routing.appmap,
                "schememap": routing.schememap,
                "appinfomap": (map {$1.key: $1.value.info}, apps.pairIterator(), exists $1.value.info),
            };
            if (compact.manifest.factories) {
                *hash<string, hash<auto>> factoryinfomap = readFactoryInfoChunk(compact.manifest.factories,
                    compact.manifest.version, dir);
                if (!exists factoryinfomap) {
                    throw "PROVIDER-INDEX-ERROR", sprintf("the data provider factory info in the data provider index "
                        "in %y is corrupt", dir);
                }
                maps.factoryinfomap = factoryinfomap;
            }
            writeCanonicalMaps(write, maps, getCanonicalMapNames(exists maps.factoryinfomap));
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
            *hash<auto> actions = unpackMetadataCacheChunk(data, compact.manifest.version, False)
                .actioninfomap{chunk_app};
            if (exists actions) {
                writeCanonicalAppActions(write, chunk_app, actions, !actions_written);
                actions_written = True;
            }
        }
        # the complete index records its connection alternatives after its actions, like the canonical index
        # file; a routing index published before version 4 does not record them
        if (!exists app && compact.routing.version >= 4) {
            writeCanonicalMaps(write, {"altmap": compact.routing.altmap}, ("altmap",));
        }
        if (f) {
            f.close();
        }
        return rv;
%endif
    }

    #! Reads and verifies a published index from its routing index and metadata cache
    /** @param dir the index directory
        @param unpack @ref True to unpack the metadata, @ref False to only verify it
        @param per_app optional code called with each app's metadata; see readSerializedDataProviderIndex()
    */
    static private *hash<SerializedDataProviderIndexInfo> readSerializedIndexIntern(string dir, bool unpack,
            *code<nothing(string, *hash<auto>, *hash<auto>)> per_app) {
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
            *hash<string, hash<auto>> factoryinfomap;
            if (*hash<auto> factories = compact.manifest.factories) {
                binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath("", factories.sha256,
                    compact.manifest.version, dir));
                if (data.size() != factories.size || digest("sha256", data).toHex() != factories.sha256) {
                    return;
                }
                size += data.size();
                if (unpack) {
                    factoryinfomap = unpackFactoryInfoChunk(data);
                }
            }
            *hash<string, list<hash<auto>>> failuremap;
            if (*hash<auto> failures = compact.manifest.failures) {
                binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath("", failures.sha256,
                    compact.manifest.version, dir));
                if (data.size() != failures.size || digest("sha256", data).toHex() != failures.sha256) {
                    return;
                }
                size += data.size();
                if (unpack) {
                    failuremap = unpackFailureInfoChunk(data);
                }
            }
            # the action summaries are not returned, since they are taken from the actions, but a copy of the index
            # without them is incomplete
            if (*hash<auto> summaries = compact.manifest.actions) {
                binary data = ReadOnlyFile::readBinaryFile(getMetadataCacheChunkPath("", summaries.sha256,
                    compact.manifest.version, dir));
                if (data.size() != summaries.size || digest("sha256", data).toHex() != summaries.sha256) {
                    return;
                }
                size += data.size();
            }
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
                    hash<DataProviderIndexInfo> chunk = unpackMetadataCacheChunk(data, compact.manifest.version,
                        False);
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
                # a routing index published before version 4 does not record the declarations
                if (compact.routing.version >= 4) {
                    rv.index.altmap = compact.routing.altmap ?? {};
                }
                # an index that does not record factory info has no member
                if (exists factoryinfomap) {
                    rv.index.factoryinfomap = factoryinfomap;
                }
                # an index that does not record failures has no member
                if (exists failuremap) {
                    rv.index.failuremap = failuremap;
                }
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
        return cast<*hash<DataProviderRoutingIndexInfo>>(parse_yaml(data.toString("UTF-8")));
    }

    #! Returns @ref True if the routing index has a supported version and matches its checksum
    static private bool checkRoutingIndex(hash<DataProviderRoutingIndexInfo> routing) {
        return routing.version >= MinRoutingIndexVersion && routing.version <= RoutingIndexVersion
            && routing.routing_sha256
                == digest("sha256", make_yaml(getRoutingIndexChecksumPayload(routing), BlockStyle)).toHex();
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
            if (manifest.factories) {
                cache.factoryinfomap = readFactoryInfoChunk(manifest.factories, manifest.version);
                if (!exists cache.factoryinfomap) {
                    failed = True;
                }
            }
            # the failures are only recorded in the metadata cache; if they cannot be read, the error is reported
            # by getIndexReadError(), and the index is returned without them
            if (manifest.failures) {
                cache.failuremap = readFailureInfoChunk(manifest.failures, manifest.version);
            }
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

    #! Reads the published index into a new index state, without changing the state of the process
    /** @param eager if @ref True, the metadata cache manifest is read and verified now; otherwise it is read when
        metadata is first needed

        @return the index state; the state of an index that is not published, or that cannot be read, has no apps,
        in which case the error is reported by @ref getIndexReadError()
    */
    static private hash<IndexStateInfo> readIndexState(bool eager) {
        # each read of the index reports its own errors
        clearIndexReadError();
        hash<IndexStateInfo> rv = getEmptyIndexState();
        *string sha256;
        *hash<DataProviderRoutingIndexInfo> routing = readValidRoutingIndex(\sha256);
        if (routing) {
            rv.index.factorymap = routing.factorymap ?? {};
            rv.index.appmap = routing.appmap ?? {};
            rv.index.schememap = routing.schememap ?? {};
            # a routing index published before version 4 does not record the declarations
            rv.index.altmap = routing.version >= 4 ? (routing.altmap ?? {}) : NOTHING;
            # the metadata is read from the metadata cache, or the canonical index file, when it is needed
            rv.deferred = True;
            rv.sha256 = sha256;
            if (routing.metadata_cache_sha256) {
                rv.metadata_cache_sha256 = routing.metadata_cache_sha256;
                rv.metadata_cache_version = routing.metadata_cache_version;
                if (eager) {
                    rv.manifest = readMetadataCacheManifestFile(routing.metadata_cache_sha256,
                        routing.metadata_cache_version);
                    rv.manifest_checked = True;
                }
            }
            return rv;
        }
        *hash<DataProviderIndexInfo> canonical = readCanonicalDataProviderIndex();
        if (canonical) {
            # the canonical index file holds every action, so the declarations are found in it
            if (!exists canonical.altmap) {
                canonical.altmap = getConnectionAlternativeMap(canonical.actioninfomap);
            }
            rv.raw_appinfomap = canonical.appinfomap ?? {};
            rv.raw_actioninfomap = canonical.actioninfomap ?? {};
            rv.canonical_apps = map {$1: True}, keys rv.raw_appinfomap;
            # the metadata maps are rebuilt from the typed metadata when it is restored
            canonical.appinfomap = {};
            canonical.actioninfomap = {};
            canonical.top_actioninfomap = {};
            rv.index = canonical;
        }
        return rv;
    }

    #! Returns the state of an index with no apps
    static private hash<IndexStateInfo> getEmptyIndexState() {
        return <IndexStateInfo>{
            "index": createEmptyDataProviderIndex(),
        };
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
            "altmap": h.altmap ?? {},
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
        sidecar.routing_sha256 =
            digest("sha256", make_yaml(getRoutingIndexChecksumPayload(sidecar), BlockStyle)).toHex();
        return writeIndex(sidecar, "routing", 0s, {}).size;
    }

    #! Records and logs an error reading a published index
    /** @param err the error code
        @param desc the error description
    */
    static private setIndexReadError(string err, string desc) {
        recordIndexReadError(sprintf("%s: %s", err, desc));
        log(LoggerLevel::LevelError, "Error reading the data provider index: %s: %s", err, desc);
    }

    #! Records an error reading a published index
    /** While the index is reloaded, an error reading it in the reloading thread is recorded for the reload, and
        only reported by @ref getIndexReadError() if the reload fails, since the process keeps using its index then

        @param err the error, as \c "<code>: <description>"
    */
    static private recordIndexReadError(string err) {
        if (reload_tid == gettid()) {
            reload_read_error = err;
        } else {
            index_read_error = err;
        }
    }

    #! Clears the error reading a published index; see recordIndexReadError()
    static private clearIndexReadError() {
        if (reload_tid == gettid()) {
            remove reload_read_error;
        } else {
            remove index_read_error;
        }
    }

    #! Reads and validates the routing index in the configured directory
    /** An index is published with a routing index and a metadata cache, and optionally with a canonical index file;
        this validates the routing index against its own checksum, and against the canonical index file if one was
        published with it.

        @param sha256 set to the digest of the routing index, if one is returned

        @return the routing index, or @ref nothing if it cannot be used, in which case the canonical index file is
        read if there is one; the returned routing index has a metadata cache with a supported version, or was
        published with a canonical index file; any error is reported by @ref getIndexReadError()
    */
    static private *hash<DataProviderRoutingIndexInfo> readValidRoutingIndex(*reference<*string> sha256) {
        # skip silently when the index env var is not configured, as in readIndex()
        if (!ENV{ProviderIndexEnvVar}.val()) {
            return;
        }
        string routing_path;
        *hash<StatInfo> routing_stat;
        *hash<DataProviderRoutingIndexInfo> routing;
        *string routing_sha256;
        try {
            routing_path = getIndexPath("routing");
            routing_stat = hstat(routing_path);
            if (!routing_stat) {
                return;
            }
            binary data = ReadOnlyFile::readBinaryFile(routing_path);
            routing = parseRoutingIndex(data);
            routing_sha256 = digest("sha256", data).toHex();
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
        sha256 = routing_sha256;
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
        if (exists h.altmap) {
            rv.altmap = h.altmap;
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
        @param report if @ref False, an error reading the manifest is not reported

        @return the manifest, or @ref nothing if it cannot be read, in which case the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<DataProviderMetadataCacheManifest> readMetadataCacheManifestFile(string sha256,
            int version, bool report = True) {
%ifdef NoMsgPack
        if (report) {
            setIndexReadError("PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read the "
                "metadata cache");
        }
        return;
%else
        *string path;
        string err;
        try {
            path = getMetadataCachePath(sha256, version);
            binary data = ReadOnlyFile::readBinaryFile(path);
            if (digest("sha256", data).toHex() != sha256) {
                err = sprintf("the metadata cache manifest %y does not match the digest recorded in the routing "
                    "index", path);
            } else {
                *hash<DataProviderMetadataCacheManifest> manifest =
                    cast<*hash<DataProviderMetadataCacheManifest>>(unpackIndexData(data));
                if (manifest && manifest.version == version) {
                    return manifest;
                }
                err = sprintf("the metadata cache manifest %y has version %y; expecting %y", path, manifest.version,
                    version);
            }
        } catch (hash<ExceptionInfo> ex) {
            err = sprintf("cannot read the metadata cache manifest %y: %s: %s", path ?? "<unknown>", ex.err,
                ex.desc);
        }
        if (report) {
            setIndexReadError("PROVIDER-INDEX-ERROR", err);
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
            return unpackMetadataCacheChunk(data, version, True);
        } catch (hash<ExceptionInfo> ex) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the metadata of app %y from %y: %s: %s",
                app, path ?? "<unknown>", ex.err, ex.desc));
        }
    }

    #! Reads and validates the factory info file of a metadata cache
    /** @param info the file's size and digest from the manifest
        @param version the metadata cache version
        @param dir the index directory; if not given, the configured index directory

        @return the factory info, or @ref nothing if it cannot be read, in which case the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<string, hash<auto>> readFactoryInfoChunk(hash<auto> info, int version, *string dir) {
        *string path;
        try {
            path = getMetadataCacheChunkPath("", info.sha256, version, dir);
            binary data = ReadOnlyFile::readBinaryFile(path);
            if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the data provider factory info in %y does not "
                    "match the size and digest recorded in the metadata cache manifest", path));
                return;
            }
            return unpackFactoryInfoChunk(data);
        } catch (hash<ExceptionInfo> ex) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the data provider factory info from %y: "
                "%s: %s", path ?? "<unknown>", ex.err, ex.desc));
        }
    }

    #! Packs the factory info of an index into a metadata cache file
    /** @param factoryinfomap the factory info, keyed by factory name

        @return the compressed file contents

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available
    */
    static private binary packFactoryInfoChunk(hash<auto> factoryinfomap) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot write data provider metadata";
%else
        # type descriptions repeat many keys and values, so the file compresses by about 10 times
        return compress(msgpack_pack(sortIndexMap(factoryinfomap, False), MSGPACK_QORE_MODE));
%endif
    }

    #! Unpacks the factory info file of a metadata cache
    /** @param data the compressed file contents

        @return the factory info, keyed by factory name

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available
    */
    static private hash<string, hash<auto>> unpackFactoryInfoChunk(binary data) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        return cast<hash<string, hash<auto>>>(msgpack_unpack(uncompress_to_binary(data), MSGPACK_QORE_MODE) ?? {});
%endif
    }

    #! Reads and validates the failure file of a metadata cache
    /** @param info the file's size and digest from the manifest
        @param version the metadata cache version
        @param dir the index directory; if not given, the configured index directory

        @return the failures, or @ref nothing if they cannot be read, in which case the error is reported by
        @ref getIndexReadError()
    */
    static private *hash<string, list<hash<auto>>> readFailureInfoChunk(hash<auto> info, int version, *string dir) {
        *string path;
        try {
            path = getMetadataCacheChunkPath("", info.sha256, version, dir);
            binary data = ReadOnlyFile::readBinaryFile(path);
            if (data.size() != info.size || digest("sha256", data).toHex() != info.sha256) {
                setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("the data provider failures in %y do not match the "
                    "size and digest recorded in the metadata cache manifest", path));
                return;
            }
            return unpackFailureInfoChunk(data);
        } catch (hash<ExceptionInfo> ex) {
            setIndexReadError("PROVIDER-INDEX-ERROR", sprintf("cannot read the data provider failures from %y: "
                "%s: %s", path ?? "<unknown>", ex.err, ex.desc));
        }
    }

    #! Packs the failures of an index into a metadata cache file
    /** @param failuremap the failures, keyed by app

        @return the compressed file contents

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available, or a failure is not a valid
        @ref ProviderIndexFailureInfo record
    */
    static private binary packFailureInfoChunk(hash<auto> failuremap) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot write data provider metadata";
%else
        hash<auto> sorted = {};
        foreach string app in (sort(keys failuremap)) {
            # only valid records are written, as plain hashes
            sorted{app} = map {} + cast<hash<ProviderIndexFailureInfo>>($1), failuremap{app};
        }
        return compress(msgpack_pack(sorted, MSGPACK_QORE_MODE));
%endif
    }

    #! Unpacks the failure file of a metadata cache
    /** @param data the compressed file contents

        @return the failures, keyed by app

        @throw PROVIDER-INDEX-ERROR MessagePack support is not available, or the file does not hold a hash of lists
        of valid @ref ProviderIndexFailureInfo records
    */
    static private hash<string, list<hash<auto>>> unpackFailureInfoChunk(binary data) {
%ifdef NoMsgPack
        throw "PROVIDER-INDEX-ERROR", "MessagePack support is not available; cannot read data provider metadata";
%else
        auto failures = msgpack_unpack(uncompress_to_binary(data), MSGPACK_QORE_MODE) ?? {};
        if (failures.typeCode() != NT_HASH) {
            throw "PROVIDER-INDEX-ERROR", sprintf("the failure file must hold a hash; got %s instead", failures.type());
        }
        hash<string, list<hash<auto>>> rv = {};
        foreach hash<auto> app in (failures.pairIterator()) {
            if (app.value.typeCode() != NT_LIST) {
                throw "PROVIDER-INDEX-ERROR", sprintf("the failures of app %y must be a list; got %s instead",
                    app.key, app.value.type());
            }
            list<hash<auto>> app_failures = ();
            foreach auto failure in (app.value) {
                if (failure.typeCode() != NT_HASH || failure.app !== app.key) {
                    throw "PROVIDER-INDEX-ERROR", sprintf("invalid failure of app %y: %y", app.key, failure);
                }
                # validates the record
                app_failures += {} + cast<hash<ProviderIndexFailureInfo>>(failure);
            }
            rv{app.key} = app_failures;
        }
        return rv;
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

        hash<auto> type_map = Constant::forName("DataProvider::AbstractDataProviderTypeMap").getValue();
        StaticMethod get_indexed_dp_info = Class::forName("DataProvider::DataProvider")
            .findStaticMethod("getIndexedInfo").method;
        return <MetadataDeserializeContext>{
            "app_info_th": TypedHash::forName("DataProvider::DataProviderAppInfo"),
            "conn_opt_th": TypedHash::forName("ConnectionProvider::ConnectionOptionInfo"),
            "action_info_th": TypedHash::forName("DataProvider::DataProviderIndexedActionInfo"),
            "action_summary_th": TypedHash::forName("DataProvider::DataProviderIndexedActionSummaryInfo"),
            "option_info_th": TypedHash::forName("DataProvider::IndexedActionOptionInfo"),
            "type_info_th": TypedHash::forName("DataProvider::DataTypeInfo"),
            "field_info_th": TypedHash::forName("DataProvider::DataFieldInfo"),
            "type_opt_info_th": TypedHash::forName("DataProvider::DataProviderTypeOptionInfo"),
            "constraint_info_th": TypedHash::forName("DataProvider::DataTypeConstraintInfo"),
            "path_vars_th": TypedHash::forName("DataProvider::DataProviderPathVarInfo"),
            "dp_info_th": TypedHash::forName("DataProvider::IndexedDataProviderInfo"),
            "dp_opt_info_th": TypedHash::forName("DataProvider::IndexedDataProviderOptionInfo"),
            "dp_exp_info_th": TypedHash::forName("DataProvider::IndexedDataProviderExpressionInfo"),
            "mapper_key_info_th": TypedHash::forName("Mapper::MapperRuntimeKeyInfo"),
            "dp_msg_info_th": TypedHash::forName("DataProvider::IndexedDataProviderMessageInfo"),
            "dp_sig_info_th": TypedHash::forName("DataProvider::IndexedDataProviderSignatureTypeInfo"),
            "dp_arg_info_th": TypedHash::forName("DataProvider::DataProviderArgInfo"),
            "type_map": type_map,
            "type_info_map": (map {$1.key: $1.value.getInfo()}, type_map.pairIterator()),
            "shape_map": getDataProviderShapeMap(),
            "ui_view_map": Constant::forName("DataProvider::UiOptionViewMap").getValue(),
            "get_indexed_dp_info": hash<auto> sub (hash<auto> info) { return get_indexed_dp_info.call(info); },
            "dp_option_members": Constant::forName("DataProvider::DataProviderInfoOptionMembers").getValue(),
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
        # the type description tables of the action's chunk, if it stores its type descriptions in tables; a
        # registered action is a typed hash without this key
        *IndexedTypeTable types = action.hasKey(IndexedTypeTableKey) ? action{IndexedTypeTableKey} : NOTHING;
        # dynamic callbacks and output types are not indexed; earlier indexes may contain output types
        action -= ("output_type", "get_output_type", "get_dynamic_options", IndexedTypeTableKey);
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
                    "type_info": getOptionTypeInfo(action, $1.key, $1.value, ctx, types),
                    "default_view": indexed ? ctx.ui_view_map{$1.value.default_view} : $1.value.default_view,
                }
            }, action.options.pairIterator()) : NOTHING),
            "data_provider_info": (action.data_provider_info
                ? (indexed
                    ? deserializeDataProviderInfo(action.data_provider_info, ctx)
                    : ctx.get_indexed_dp_info(action.data_provider_info))
                : NOTHING),
        });
    }

    #! Returns the type description of an action option
    /** @param action the action
        @param name the option name
        @param option the option as stored in the index, or an option of a registered action
        @param ctx the prepared deserialization context
        @param types the type description tables of the action's chunk, if it stores its type descriptions in tables

        @return a \c DataProvider::DataTypeInfo hash
    */
    static private hash<auto> getOptionTypeInfo(hash<auto> action, string name, hash<auto> option,
            hash<MetadataDeserializeContext> ctx, *IndexedTypeTable types) {
        # a registered action's option is a typed hash without this key; an option of a chunk that stores its type
        # descriptions in tables refers to its description by its position, which can be 0
        if (option.hasKey("type_info") && (option.type_info.typeCode() == NT_INT || option.type_info)) {
            return IndexedTypeTable::restoreTypeInfo(option.type_info, ctx, types);
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

    #! Returns the data provider info of an indexed action, with its types described by plain data
    /** The index holds the base type name of each type, which is described by the corresponding type in
        \c DataProvider::AbstractDataProviderTypeMap

        @param info the data provider info as stored in the index
        @param ctx the prepared deserialization context

        @return a \c DataProvider::IndexedDataProviderInfo hash
    */
    static private hash<auto> deserializeDataProviderInfo(hash<auto> info, hash<MetadataDeserializeContext> ctx) {
        hash<auto> rv;
        foreach hash<auto> i in (info.pairIterator()) {
            if (i.key == "bulk_read_shapes" || i.key == "bulk_create_shapes" || i.key == "bulk_upsert_shapes") {
                rv{i.key} = map deserializeDataProviderShape($1, ctx.shape_map), i.value;
            } else {
                rv{i.key} = i.value;
            }
        }
        foreach string key in (ctx.dp_option_members) {
            if (rv{key}) {
                rv{key} = ctx.dp_opt_info_th.doCastValuesWithOverrides(
                    (map {$1.key: $1.value - "type"}, rv{key}.pairIterator()), map {
                    $1.key: {
                        "allowed_values": (exists $1.value.allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values
                            : NOTHING),
                        "element_allowed_values": (exists $1.value.element_allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.value.element_allowed_values
                            : NOTHING),
                        "type_info": (map getTypeInfoForName($1, ctx), $1.value.type),
                    },
                }, rv{key}.pairIterator());
            }
        }
        return ctx.dp_info_th.doCastWithOverrides(rv, {
            "expressions": (exists rv.expressions ? ctx.dp_exp_info_th.doCastValuesWithOverrides(
                (map {$1.key: $1.value - "return_type"}, rv.expressions.pairIterator()), map {
                $1.key: {
                    "args": (exists $1.value.args ? map ctx.dp_sig_info_th.doCastWithOverrides($1 - "type", {
                        "type_info": getTypeInfoForName($1.type, ctx),
                    }), $1.value.args : NOTHING),
                    "arg_info": (exists $1.value.arg_info ? map ctx.dp_arg_info_th.doCastWithOverrides($1, {
                        "allowed_values": (exists $1.allowed_values
                            ? map cast<hash<AllowedValueInfo>>($1), $1.allowed_values
                            : NOTHING),
                    }), $1.value.arg_info : NOTHING),
                    "return_type_info": getTypeInfoForName($1.value.return_type, ctx),
                }
            }, rv.expressions.pairIterator()) : NOTHING),
            "events": (exists rv.events ? deserializeDataProviderMessages(rv.events, ctx) : NOTHING),
            "messages": (exists rv.messages ? deserializeDataProviderMessages(rv.messages, ctx) : NOTHING),
        } + (exists rv.mapper_keys
            ? {"mapper_keys": ctx.mapper_key_info_th.doCastValues(rv.mapper_keys)}
            # the member has a default value
            : {}));
    }

    #! Returns indexed event or message info, with the types described by plain data
    static private hash<auto> deserializeDataProviderMessages(hash<auto> messages,
            hash<MetadataDeserializeContext> ctx) {
        return map {
            $1.key: ctx.dp_msg_info_th.doCast({
                "desc": $1.value.desc,
                "type_info": getTypeInfoForName($1.value.type, ctx),
            }),
        }, messages.pairIterator();
    }

    #! Returns the description of the type with the given base type name from the index
    static private hash<auto> getTypeInfoForName(*string name, hash<MetadataDeserializeContext> ctx) {
        return ctx.type_info_map{name} ?? ctx.type_info_map."auto";
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
        string path = getIndexPath(type);
        string yaml = make_yaml(h, BlockStyle);
        writeFileAtomic(path, yaml);
        return <ProviderIndexInfo>{
            "path": path,
            "size": yaml.size(),
            "delta": delta,
            "summary": summary,
        };
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
                clearIndexReadError();
            }
            return rv;
        } catch (hash<ExceptionInfo> ex) {
            # an index that has not been created yet is not an error; anything else (ex: a permissions
            # problem or a corrupt file) is recorded so that callers can detect the degraded state
            bool missing = ex.arg == ENOENT;
            if (type == "data") {
                if (missing) {
                    clearIndexReadError();
                } else {
                    recordIndexReadError(sprintf("%s: %s", ex.err, ex.desc));
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

%ifndef NoMsgPack
#! Builds the tables of distinct type and field descriptions of a metadata cache chunk
/** The description of a type that is reachable by more than one path, as a record type that other record types
    refer to, repeats the descriptions of the types below it at each path, and the description of a recursive type
    repeats them at each path to each of its truncations, so the option type descriptions of some apps are many
    times larger than the descriptions of their distinct types.  In the tables, each distinct type description and
    each distinct field description is stored once: a type description refers to its field descriptions and to its
    nested type descriptions (its element type, its union types, and its default field type), and a field
    description to its type description, by their positions in the tables.

    Nested descriptions are added before the descriptions that refer to them.  Descriptions are compared by their
    serialized form, so descriptions are only shared if they are identical.

    @see IndexedTypeTable, which restores the descriptions

    @since ProviderIndexUtil 1.2
*/
class IndexedTypeTableBuilder {
    private {
        #! Distinct type descriptions, whose nested descriptions are positions
        list<hash<auto>> types = ();
        #! Distinct field descriptions, whose type descriptions are positions
        list<hash<auto>> fields = ();
        #! The positions of the type descriptions, by serialized form
        hash<string, int> type_pos = {};
        #! The positions of the field descriptions, by serialized form
        hash<string, int> field_pos = {};
    }

    #! Returns the given serialized action with each option type description replaced by its position
    /** @param action a serialized action

        @return the action; an option's \c type_info that is not a non-empty hash is left unchanged
    */
    hash<auto> addActionTypes(hash<auto> action) {
        if (action.options.typeCode() != NT_HASH) {
            return action;
        }
        # the action, its options, and the descriptions may be typed hashes, whose members cannot be positions
        hash<auto> options = {};
        foreach hash<auto> i in (action.options.pairIterator()) {
            if (i.value.typeCode() == NT_HASH && i.value.type_info.typeCode() == NT_HASH && i.value.type_info) {
                hash<auto> option = {} + i.value;
                option.type_info = addType(i.value.type_info);
                options{i.key} = option;
            } else {
                options{i.key} = i.value;
            }
        }
        hash<auto> rv = {} + action;
        rv.options = options;
        return rv;
    }

    #! Returns the tables as stored in a metadata cache chunk
    /** @return a hash with \c types and \c fields keys
    */
    hash<auto> getTables() {
        return {
            "types": types,
            "fields": fields,
        };
    }

    #! Adds the given type description and its nested descriptions, and returns its position
    private int addType(hash<auto> info) {
        hash<auto> node = {} + info;
        if (info.fields.typeCode() == NT_HASH) {
            hash<auto> node_fields = {};
            foreach hash<auto> i in (info.fields.pairIterator()) {
                node_fields{i.key} = i.value.typeCode() == NT_HASH ? addField(i.value) : i.value;
            }
            node.fields = node_fields;
        }
        if (info.element_type.typeCode() == NT_HASH) {
            node.element_type = addType(info.element_type);
        }
        if (info.union_types.typeCode() == NT_LIST) {
            list<auto> union_types = ();
            foreach auto union_type in (info.union_types) {
                push union_types, union_type.typeCode() == NT_HASH ? addType(union_type) : union_type;
            }
            node.union_types = union_types;
        }
        if (info.default_field_type_info.typeCode() == NT_HASH) {
            node.default_field_type_info = addType(info.default_field_type_info);
        }
        string key = msgpack_pack(node, MSGPACK_QORE_MODE).toBase64();
        *int pos = type_pos{key};
        if (exists pos) {
            return pos;
        }
        push types, node;
        return type_pos{key} = types.size() - 1;
    }

    #! Adds the given field description and its type description, and returns its position
    private int addField(hash<auto> field) {
        hash<auto> node = {} + field;
        if (field.type.typeCode() == NT_HASH) {
            node.type = addType(field.type);
        }
        string key = msgpack_pack(node, MSGPACK_QORE_MODE).toBase64();
        *int pos = field_pos{key};
        if (exists pos) {
            return pos;
        }
        push fields, node;
        return field_pos{key} = fields.size() - 1;
    }
}
%endif

#! The tables of distinct type and field descriptions of a metadata cache chunk
/** Restores the descriptions stored by @ref IndexedTypeTableBuilder: each description is restored once, when it is
    first needed, and every description that refers to it shares the restored description, so restoring the option
    types of an app's actions takes time and memory in proportion to its distinct descriptions, not to the size of
    their complete descriptions.

    A description that refers to a position that is not in its table, or to itself, cannot be restored.

    @since ProviderIndexUtil 1.2
*/
class IndexedTypeTable {
    private {
        #! Distinct type descriptions, whose nested descriptions are positions
        list<auto> types;
        #! Distinct field descriptions, whose type descriptions are positions
        list<auto> fields;
        #! Restored type descriptions by position
        list<auto> restored_types = ();
        #! Restored field descriptions by position
        list<auto> restored_fields = ();
        #! Complete serialized type descriptions by position
        list<auto> expanded_types = ();
        #! Complete serialized field descriptions by position
        list<auto> expanded_fields = ();
        #! The descriptions being restored or expanded by each thread, to reject a description that refers to itself
        hash<string, bool> pending = {};
    }

    #! Creates the object from the tables of a metadata cache chunk
    /** @param types the type description table
        @param fields the field description table

        @throw PROVIDER-INDEX-ERROR a table is not a list
    */
    constructor(auto types, auto fields) {
        self.types = getTable(types, "type");
        self.fields = getTable(fields, "field");
    }

    #! Returns the given type description, stored in a chunk's tables or as a complete description, as a DataTypeInfo hash
    /** @param info a position in the type table of @a types, or a complete type description, whose nested
        descriptions may be positions in the tables of @a types
        @param ctx the prepared deserialization context
        @param types the tables of the description's chunk, if it stores its type descriptions in tables

        @return a \c DataProvider::DataTypeInfo hash

        @throw PROVIDER-INDEX-ERROR the description refers to a position without tables, or to a position that is
        not in its table, or to itself
    */
    static hash<auto> restoreTypeInfo(auto info, hash<MetadataDeserializeContext> ctx, *IndexedTypeTable types) {
        if (info.typeCode() == NT_INT) {
            if (!types) {
                throw "PROVIDER-INDEX-ERROR", sprintf("a type description refers to position %d of a type "
                    "description table, but its metadata has none", info);
            }
            return types.restoreType(info, ctx);
        }
        return castTypeInfo(info, ctx, types);
    }

    #! Returns the given plain type description as a DataTypeInfo hash, including all nested descriptions
    /** @param info the type description as stored in the index
        @param ctx the prepared deserialization context
        @param types the tables of the description's chunk, if it stores its type descriptions in tables

        @return a \c DataProvider::DataTypeInfo hash
    */
    static hash<auto> castTypeInfo(hash<auto> info, hash<MetadataDeserializeContext> ctx, *IndexedTypeTable types) {
        return ctx.type_info_th.doCastWithOverrides(info, {
            "supported_options": (exists info.supported_options
                ? ctx.type_opt_info_th.doCastValues(info.supported_options)
                : NOTHING),
            "fields": exists info.fields ? castFields(info.fields, ctx, types) : NOTHING,
            "element_type": exists info.element_type ? restoreTypeInfo(info.element_type, ctx, types) : NOTHING,
            "union_types": (exists info.union_types
                ? (map restoreTypeInfo($1, ctx, types), info.union_types)
                : NOTHING),
            "default_field_type_info": (exists info.default_field_type_info
                ? restoreTypeInfo(info.default_field_type_info, ctx, types)
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

    #! Returns the given serialized action with each option type description that is a position replaced by the complete description
    /** @param action a serialized action from a chunk that stores its type descriptions in tables

        @return the action in the serialized form of a chunk without tables; descriptions are shared, not copied

        @throw PROVIDER-INDEX-ERROR a description refers to a position that is not in its table, or to itself
    */
    hash<auto> expandActionTypes(hash<auto> action) {
        if (action.options.typeCode() != NT_HASH) {
            return action;
        }
        hash<auto> options = {};
        foreach hash<auto> i in (action.options.pairIterator()) {
            if (i.value.typeCode() == NT_HASH && i.value.type_info.typeCode() == NT_INT) {
                hash<auto> option = {} + i.value;
                option.type_info = expandType(i.value.type_info);
                options{i.key} = option;
            } else {
                options{i.key} = i.value;
            }
        }
        hash<auto> rv = {} + action;
        rv.options = options;
        return rv;
    }

    #! Returns the type description at the given position as a DataTypeInfo hash
    private hash<auto> restoreType(int pos, hash<MetadataDeserializeContext> ctx) {
        if (exists (auto rv = restored_types[pos])) {
            return rv;
        }
        hash<auto> node = getEntry(types, pos, "type");
        string key = sprintf("%d:rt%d", gettid(), pos);
        checkPending(key, pos, "type");
        on_exit remove pending{key};
        return restored_types[pos] = castTypeInfo(node, ctx, self);
    }

    #! Returns the field description at the given position as a DataFieldInfo hash
    private hash<auto> restoreField(int pos, hash<MetadataDeserializeContext> ctx) {
        if (exists (auto rv = restored_fields[pos])) {
            return rv;
        }
        hash<auto> node = getEntry(fields, pos, "field");
        string key = sprintf("%d:rf%d", gettid(), pos);
        checkPending(key, pos, "field");
        on_exit remove pending{key};
        return restored_fields[pos] = castFieldInfo(node, ctx, self);
    }

    #! Returns the complete serialized type description at the given position
    private hash<auto> expandType(int pos) {
        if (exists (auto rv = expanded_types[pos])) {
            return rv;
        }
        hash<auto> rv = {} + getEntry(types, pos, "type");
        string key = sprintf("%d:et%d", gettid(), pos);
        checkPending(key, pos, "type");
        on_exit remove pending{key};
        if (rv.fields.typeCode() == NT_HASH) {
            hash<auto> node_fields = {};
            foreach hash<auto> i in (rv.fields.pairIterator()) {
                node_fields{i.key} = i.value.typeCode() == NT_INT ? expandField(i.value) : i.value;
            }
            rv.fields = node_fields;
        }
        if (rv.element_type.typeCode() == NT_INT) {
            rv.element_type = expandType(rv.element_type);
        }
        if (rv.union_types.typeCode() == NT_LIST) {
            list<auto> union_types = ();
            foreach auto union_type in (rv.union_types) {
                push union_types, union_type.typeCode() == NT_INT ? expandType(union_type) : union_type;
            }
            rv.union_types = union_types;
        }
        if (rv.default_field_type_info.typeCode() == NT_INT) {
            rv.default_field_type_info = expandType(rv.default_field_type_info);
        }
        return expanded_types[pos] = rv;
    }

    #! Returns the complete serialized field description at the given position
    private hash<auto> expandField(int pos) {
        if (exists (auto rv = expanded_fields[pos])) {
            return rv;
        }
        hash<auto> rv = {} + getEntry(fields, pos, "field");
        string key = sprintf("%d:ef%d", gettid(), pos);
        checkPending(key, pos, "field");
        on_exit remove pending{key};
        if (rv.type.typeCode() == NT_INT) {
            rv.type = expandType(rv.type);
        }
        return expanded_fields[pos] = rv;
    }

    #! Returns the given field descriptions as DataFieldInfo hashes
    private static hash<auto> castFields(hash<auto> info_fields, hash<MetadataDeserializeContext> ctx,
            *IndexedTypeTable types) {
        hash<auto> rv = {};
        foreach hash<auto> i in (info_fields.pairIterator()) {
            if (i.value.typeCode() == NT_INT) {
                if (!types) {
                    throw "PROVIDER-INDEX-ERROR", sprintf("field %y refers to position %d of a field description "
                        "table, but its metadata has none", i.key, i.value);
                }
                rv{i.key} = types.restoreField(i.value, ctx);
            } else {
                rv{i.key} = castFieldInfo(i.value, ctx, types);
            }
        }
        return rv;
    }

    #! Returns the given field description as a DataFieldInfo hash
    private static hash<auto> castFieldInfo(hash<auto> field, hash<MetadataDeserializeContext> ctx,
            *IndexedTypeTable types) {
        return ctx.field_info_th.doCastWithOverrides(field, {
            "type": restoreTypeInfo(field.type, ctx, types),
            "allowed_values": (exists field.allowed_values
                ? map cast<hash<AllowedValueInfo>>($1), field.allowed_values
                : NOTHING),
            "element_allowed_values": (exists field.element_allowed_values
                ? map cast<hash<AllowedValueInfo>>($1), field.element_allowed_values
                : NOTHING),
        });
    }

    #! Marks the given description as being restored or expanded by the current thread
    /** @throw PROVIDER-INDEX-ERROR the description is already being restored or expanded by the current thread,
        so it refers to itself
    */
    private checkPending(string key, int pos, string what) {
        if (pending{key}) {
            throw "PROVIDER-INDEX-ERROR", sprintf("%s description %d of a type description table refers to itself",
                what, pos);
        }
        pending{key} = True;
    }

    #! Returns the entry at the given position of the given table
    /** @throw PROVIDER-INDEX-ERROR the position is not in the table, or its entry is not a hash
    */
    private static hash<auto> getEntry(list<auto> table, int pos, string what) {
        if (pos < 0 || pos >= table.size() || table[pos].typeCode() != NT_HASH) {
            throw "PROVIDER-INDEX-ERROR", sprintf("a type description refers to position %d of the %s description "
                "table, which has %d description%s", pos, what, table.size(), table.size() == 1 ? "" : "s");
        }
        return table[pos];
    }

    #! Returns the given table of a metadata cache chunk
    /** @throw PROVIDER-INDEX-ERROR the table is neither a list nor missing
    */
    private static list<auto> getTable(auto table, string what) {
        switch (table.typeCode()) {
            case NT_LIST:
                return table;
            case NT_NOTHING:
                return ();
        }
        throw "PROVIDER-INDEX-ERROR", sprintf("the %s description table of a metadata cache chunk must be a list; "
            "got %s instead", what, table.type());
    }
}
}
