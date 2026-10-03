# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProviderIndex.qc module for indexing data providers

/*  ProviderIndex.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ProviderIndex module
public namespace ProviderIndex {
#! Options for creating data provider indices
public hashdecl ProviderIndexCreateOptions {
    #! Load all modules from DataProvider's built-in factory map
    bool load_known_factories = True;

    #! Load factories from environment variables such as QORE_DATA_PROVIDERS
    bool load_environment = True;

    #! Load TypeScriptActionInterface to register TypeScript-backed apps
    bool load_typescript_actions = True;

    #! Optional list of application names to index; if unset, all registered apps are indexed
    *list<string> apps;

    #! Optional exact application/action inventory fragments required for a complete discovery generation
    /** @since %ProviderIndex 1.1 */
    *list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected_inventory;

    #! The maximum number of worker processes run at the same time; 0 builds the index in the calling process
    /** Worker builds require the \c process module 2.2 or later.

        With workers, the calling process loads no provider modules: each worker is a new process that loads a
        batch of the provider modules, and writes the index metadata of their apps, and the calling process merges
        the results, qualifies them, and publishes the index.  The memory used is then bounded by the largest
        batches rather than growing with the number of providers.

        Apps and actions registered only in the calling process are not indexed by a worker build.

        Workers return their results with MessagePack; if it is not available (see
        @ref ProviderIndexUtil::ProviderIndexUtil::hasMessagePack() "ProviderIndexUtil::hasMessagePack()"), a warning
        is logged and the index is built in the calling process.

        @since %ProviderIndex 1.2
    */
    int workers = 0;

    #! The command that starts a worker process, followed by its arguments
    /** The first element is the executable, which must be given as an absolute path or be found in the \c PATH; a
        relative path is only searched for in the \c PATH and is not resolved against the working directory.

        The path of the worker's batch file is appended as the last argument; the command must pass it to
        @ref ProviderIndex::ProviderIndex::runIndexWorker() "ProviderIndex::runIndexWorker()" and exit with its
        return value.  A command that loads modules that the embedding application requires to be present while
        providers are indexed must first call
        @ref ProviderIndex::ProviderIndex::prepareIndexWorker() "ProviderIndex::prepareIndexWorker()".

        Workers inherit the environment of the calling process, with the
        @ref ProviderIndexUtil::ProviderIndexBuildEnvVar "QORE_PROVIDER_INDEX_BUILD" environment variable set and
        the directory of the ProviderIndex module first in \c QORE_MODULE_DIR.

        The default runs the current Qore interpreter with
        <tt>-l ProviderIndex -e "exit(ProviderIndex::ProviderIndex::runIndexWorker(ARGV[0]));"</tt>

        @since %ProviderIndex 1.2
    */
    *list<string> worker_command;

    #! The maximum number of provider modules loaded by one worker
    /** The TypeScript app catalogue and each module named in \c QORE_DATA_PROVIDERS always have a worker of their
        own.

        @since %ProviderIndex 1.2
    */
    int worker_batch_size = 4;

    #! The maximum time in milliseconds that a worker may run before it is terminated and the build fails
    /** @since %ProviderIndex 1.2 */
    int worker_timeout_ms = 3600000;

    #! Also write the canonical index file (\c qore-data-index.yaml)
    /** The published index is its routing index and metadata cache; the canonical index file holds the complete
        index as YAML, is many times larger, and is not needed by any reader, so it is only written if this is set,
        or if the @ref ProviderIndexUtil::ProviderIndexCanonicalYamlEnvVar "QORE_PROVIDER_INDEX_CANONICAL_YAML"
        environment variable is set, or if MessagePack support is not available.  Set it while a consumer that
        requires the file, such as an earlier version of ProviderIndexUtil sharing the index directory, is in use.

        A published index can be written as YAML on demand with
        @ref ProviderIndexUtil::ProviderIndexUtil::dumpIndexYaml() "ProviderIndexUtil::dumpIndexYaml()".

        @since %ProviderIndex 1.2
    */
    bool write_canonical_yaml = False;
}

#! The main API class
public class ProviderIndex inherits ProviderIndexUtil {
    public {}

    private {
        #! The batch of this process, if it is an index worker
        static *hash<auto> worker_batch;

        #! Protects the running worker processes and the cancellation generation
        static Mutex worker_lock();
        #! Incremented by cancelWorkers(); a build started in an earlier generation has been cancelled
        static int cancel_generation = 0;
%ifndef NoProcess
        #! The worker processes of the builds in progress, by batch file
        static hash<string, Process> worker_processes = {};
%endif
    }

    #! Stops the index worker processes of every build in progress in this process
    /** Each running worker is terminated and no further worker is started; the builds fail with
        \c PROVIDER-INDEX-CANCELLED and publish nothing.  Builds started afterwards are not affected.

        @return the number of worker processes that were terminated

        @par Example
        @code{.py}
# a builder that is asked to stop takes its workers with it
set_signal_handler(SIGTERM, sub () { ProviderIndex::cancelWorkers(); });
        @endcode

        @note Worker processes run in their own process groups, so they are not stopped by a signal sent to the
        process building the index; call this method when that process is asked to stop
    */
    static int cancelWorkers() {
%ifdef NoProcess
        {
            AutoLock al(worker_lock);
            ++cancel_generation;
        }
        return 0;
%else
        list<Process> running;
        {
            AutoLock al(worker_lock);
            ++cancel_generation;
            running = map $1, worker_processes.iterator();
        }
        foreach Process p in (running) {
            try {
                p.terminate();
            } catch (hash<ExceptionInfo> ex) {
                # the worker has already exited
            }
        }
        return running.size();
%endif
    }

    #! Returns the current cancellation generation
    private static int getCancelGeneration() {
        AutoLock al(worker_lock);
        return cancel_generation;
    }

    #! Returns True if builds of the given generation have been cancelled
    private static bool isCancelled(int generation) {
        AutoLock al(worker_lock);
        return cancel_generation != generation;
    }

    #! Create the data provider index
    /** @param index optional seeded factory, application and scheme dependencies; logical module names are
            preferred, and explicit artifact paths must match the loaded module registry
        @param dir optional output directory; otherwise the configured provider-index directory is used
        @param opts discovery sources, optional exact application/action inventory, and optional application selection
        @return the published index path, size, elapsed generation time, and summary whose \c qualification member is
            the complete @ref DataProvider::DataProviderDiscoveryReportInfo used to authorize publication, and whose
            \c failed_apps and \c failed_actions members give the number of apps and actions that could not be
            initialized and are recorded as unavailable in the index
        @throw PROVIDER-INDEX-MODULE-ERROR an explicit dependency has no authoritative loaded module identity
        @throw DATA-PROVIDER-DISCOVERY-ERROR if discovery fails other than by the failure of individual apps or actions:
            a discovery source, a factory, or a worker fails, or the catalog changes while it is indexed
        @throw DATA-PROVIDER-DISCOVERY-STALE if registration changes after qualification
        @throw PROVIDER-INDEX-ERROR the output directory is unavailable
        @throw PROVIDER-INDEX-WORKER-ERROR a worker build was requested, but worker processes cannot be started
        @throw PROVIDER-INDEX-CANCELLED a worker build was stopped by cancelWorkers()
        @note The index is built in index build mode (see
            @ref ProviderIndexUtil::ProviderIndexUtil::setBuildMode() "ProviderIndexUtil::setBuildMode()"), so the
            index being replaced is never read; when the build finishes, the next index lookup reads the new index.
        @note With @ref ProviderIndexCreateOptions::workers "workers" set, providers are loaded and indexed in
            worker processes; see @ref runIndexWorker()
        @note An app or action that cannot be initialized does not fail the build: it is recorded in the index with its
            error (see @ref ProviderIndexUtil::ProviderIndexUtil::getIndexedFailures()
            "ProviderIndexUtil::getIndexedFailures()"), and is unavailable in it, while the other apps and actions are
            indexed; an app that registered its info before failing is indexed with its info and connection scheme and
            without the actions that failed.  The failures are in the \c unavailable member of the \c qualification
            report, which is then not \c complete, and are logged; a caller that requires a complete catalog checks
            \c summary.qualification.complete (see @ref DataProvider::DataProviderActionCatalog::isAppDiscoveryFailure()
            "DataProviderActionCatalog::isAppDiscoveryFailure()")
        @note Existing explicit-path indexes remain readable. Rebuilding persists module names so the runtime
            can select source or compiled representations through its normal search path. This is a semantic
            catalog, not a binary digest or a lease over concurrently replaced module files.
        @par Example
        Rebuild after installing order-processing providers; equivalent source and AOT packaging retains the
        same dependencies, while a changed order-line action or option remains part of the new index content.
        @since %ProviderIndex 1.0
    */
    public static hash<auto> createDataProviderIndex(hash<DataProviderIndexInfo> index = <DataProviderIndexInfo>{},
            *string dir, hash<ProviderIndexCreateOptions> opts = <ProviderIndexCreateOptions>{}) {
        date start = now_us();

        if (dir.val()) {
            setEnvironmentVariable(dir);
        }

        # check that the environment variable is set and that the index directory can be written to before
        # starting the (potentially very long) index build; any existing index is left untouched until the new
        # index has been written completely (index files are written atomically)
        getWritableIndexDir();

        if (opts.workers > 0) {
            if (ProviderIndexUtil::hasMessagePack()) {
                return createDataProviderIndexWithWorkers(index, opts, start);
            }
            # workers return their results with MessagePack
            ProviderIndexUtil::log(LoggerLevel::LevelWarn, "MessagePack support is not available; the data "
                "provider index is built in the calling process instead of with %d worker process(es)", opts.workers);
        }

        # the index being replaced is never read while its replacement is built
        bool build_mode = setBuildMode(True);
        on_exit setBuildMode(build_mode);

        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected
            = opts.expected_inventory ?? ();
        foreach string app in (opts.apps) {
            expected += <DataProviderDiscoveryExpectedIdentityInfo>{
                "app": app,
            };
        }
        *DataProviderDiscoverySession discovery
            = DataProviderActionCatalog::beginDiscovery(expected, opts.apps);
        *DataProviderQualifiedDiscovery qualification;
        on_exit if (discovery) {
            DataProviderActionCatalog::abortDiscovery(discovery);
        }
        on_exit if (qualification) {
            DataProviderActionCatalog::discardQualifiedDiscovery(
                qualification);
        }

        # make sure that all data providers are initialized immediately
        DataProvider::setOptions(DPO_DisableOnDemandInitialization | DPO_EnableOnDemandActions);

        if (opts.load_known_factories) {
            # Load all known data provider factories. A missing optional module
            # is tolerated by the legacy registration API, but not by an
            # authoritative index build that explicitly requested this set.
            foreach hash<DataProviderFactoryModuleLoadFailureInfo> failure in
                    (DataProvider::registerKnownFactoriesWithReport()) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "factory", "load-known-factory", failure.module,
                    failure.error, failure.description, failure.argument,
                    NOTHING, NOTHING, failure.module);
            }
        }

        # start with the default factory map
        index.factorymap += DataProvider::FactoryMap;

        # Read the process environment at discovery time. The global ENV
        # snapshot can legitimately lack a variable added after startup (for
        # example by an embedding host or a qualification test), while the
        # environment loader itself uses getenv(). Gating both paths on the
        # same live source prevents a silent no-op discovery attempt.
        if (opts.load_environment && getenv("QORE_DATA_PROVIDERS")) {
            # load all data providers from the QORE_DATA_PROVIDERS environment variable
            try {
                foreach hash<DataProviderEnvironmentLoadFailureInfo> failure in
                        (DataProvider::loadProvidersFromEnvironmentWithReport()) {
                    DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                        "source", failure.phase, failure.module,
                        failure.error, failure.description, failure.argument,
                        NOTHING, NOTHING, failure.module);
                }
            } catch (hash<ExceptionInfo> ex) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "source", "load-environment", "QORE_DATA_PROVIDERS",
                    ex.err, ex.desc, ex.arg);
            }

            addDataProviderFactories(\index, discovery);
        }

        if (opts.load_typescript_actions) {
            # try to load and initialize TypeScriptActionInterface manually
            tryLoadTypeScriptActionInterface(discovery);
        }

        # Option types whose descriptions do not restore an equivalent type; see checkTypeInfo()
        list<hash<auto>> type_info_mismatches = ();
        int scheme_registry_revision = snapshotIndex(\index, discovery, opts.apps, \type_info_mismatches);

        # the failures of individual apps and actions are recorded in the index instead of failing the build
        qualification = DataProviderActionCatalog::sealDiscovery(discovery, True);
        discovery = NOTHING;
        hash<DataProviderDiscoveryReportInfo> qualification_report
            = qualification.getReport();
        index.failuremap = getFailureMap(qualification_report.unavailable, getModuleNames());
        # The scheme registry and data-provider catalog hold their registration locks across the O(1) revision
        # checks and atomic file replacement, so neither can invalidate qualification between check and write.
        hash<auto> rv = ConnectionSchemeCache::publishStableRegistrySnapshot(
            scheme_registry_revision, hash<auto> sub () {
                return DataProviderActionCatalog::publishQualifiedDiscovery(
                    qualification, hash<auto> sub () {
                        return writeDataProviderIndex(index,
                            now_us() - start, {
                                "factories": index.factorymap.size(),
                                "apps": index.appmap.size(),
                                "schemes": index.schememap.size(),
                                "qualification": qualification_report,
                                "type_info_mismatches": type_info_mismatches,
                            } + getFailureSummary(index.failuremap), opts.write_canonical_yaml);
                    });
                });
        logFailures(index.failuremap);
        return rv;
    }

    #! The maximum number of failed apps and actions named in the log message of an index build
    const MaxLoggedFailures = 20;

    #! The order of the failures recorded for the same app or action; the first is the cause of the others
    const FailurePhaseOrder = {
        "registration": 0,
        "publication": 1,
        "serialize-action-metadata": 2,
        "expected-inventory": 3,
    };

    #! Returns the failures of individual apps and actions to record in the index
    /** @param unavailable the failures of individual apps and actions (see
            @ref DataProvider::DataProviderActionCatalog::isAppDiscoveryFailure()
            "DataProviderActionCatalog::isAppDiscoveryFailure()")
        @param module_names maps the module paths of the process to module names

        @return the failures keyed by app, as
        @ref ProviderIndexUtil::ProviderIndexFailureInfo "ProviderIndexFailureInfo" records in plain hashes; each app's own failures are first, then those of its actions in action name order;
        a missing expected identity is only recorded if its app or action has no other failure, which caused it,
        and a failure reported more than once is recorded once
    */
    private static hash<string, list<hash<auto>>> getFailureMap(
            *list<hash<DataProviderDiscoveryFailureInfo>> unavailable, hash<string, string> module_names) {
        # app -> action ("" for the app itself) -> failures
        hash<string, hash<string, list<hash<auto>>>> identities = {};
        foreach hash<DataProviderDiscoveryFailureInfo> failure in (unavailable) {
            hash<auto> record = {} + <ProviderIndexFailureInfo>{
                "kind": failure.kind,
                "app": failure.app,
                "action": failure.action,
                "phase": failure.phase,
                "error": failure.error,
                "description": failure.description,
                "module": getFailureModule(failure.module_context, module_names),
            };
            string key = failure.action ?? "";
            if (!inlist(record, identities{failure.app}{key})) {
                identities{failure.app}{key} += record;
            }
        }
        hash<string, list<hash<auto>>> rv = {};
        foreach string app in (sort(keys identities)) {
            list<hash<auto>> app_failures = ();
            # the app's own failures ("") sort first
            foreach string key in (sort(keys identities{app})) {
                list<hash<auto>> records = identities{app}{key};
                if (records.size() > 1) {
                    records = map $1, records, $1.phase != "expected-inventory";
                    records = sort(records, int sub (hash<auto> l, hash<auto> r) {
                        return (FailurePhaseOrder{l.phase} ?? 99) <=> (FailurePhaseOrder{r.phase} ?? 99)
                            ?: l.error <=> r.error ?: l.description <=> r.description;
                    });
                }
                app_failures += records;
            }
            rv{app} = app_failures;
        }
        return rv;
    }

    #! Returns the module name to record for the owner of a failure
    /** @param module_context the owner's module path or name, if known
        @param module_names maps the module paths of the process to module names

        @return the owner's module name, or @ref nothing if it is not known
    */
    private static *string getFailureModule(*string module_context, hash<string, string> module_names) {
        if (!module_context.val()) {
            return NOTHING;
        }
        if (*string name = module_names{module_context}) {
            return name;
        }
        # a path that is not a loaded module does not identify one
        if (module_context =~ /[\/\\]/ || module_context =~ /\.(qm|qmod)$/i) {
            return NOTHING;
        }
        return module_context;
    }

    #! Returns the number of apps and actions recorded as unavailable, for the summary of an index build
    /** @param failuremap the failures recorded in the index

        @return \c failed_apps: the number of apps whose own initialization failed, and \c failed_actions: the
        number of actions that failed
    */
    private static hash<auto> getFailureSummary(*hash<string, list<hash<auto>>> failuremap) {
        int apps = 0;
        int actions = 0;
        foreach list<hash<auto>> failures in ((failuremap ?? {}).iterator()) {
            if (select failures, !exists $1.action) {
                ++apps;
            }
            actions += ((map {$1.action: True}, failures, exists $1.action) ?? {}).size();
        }
        return {
            "failed_apps": apps,
            "failed_actions": actions,
        };
    }

    #! Logs the apps and actions recorded as unavailable in a published index
    /** One warning names up to @ref MaxLoggedFailures failed apps and actions with their errors, and each failure
        is logged in full at debug level

        @param failuremap the failures recorded in the index
    */
    private static logFailures(*hash<string, list<hash<auto>>> failuremap) {
        if (!failuremap) {
            return;
        }
        hash<auto> summary = getFailureSummary(failuremap);
        list<string> names = ();
        int count = 0;
        foreach list<hash<auto>> failures in (failuremap.iterator()) {
            foreach hash<auto> failure in (failures) {
                ++count;
                if (names.size() < MaxLoggedFailures) {
                    names += sprintf("%s%s (%s)", failure.app, exists failure.action ? "/" + failure.action : "",
                        failure.error);
                }
                log(LoggerLevel::LevelDebug, "unavailable in the data provider index: %s%s: %s phase %s: %s: %s",
                    failure.app, exists failure.action ? "/" + failure.action : "", failure.kind, failure.phase,
                    failure.error, failure.description);
            }
        }
        log(LoggerLevel::LevelWarn, "the data provider index was published with %d app(s) and %d action(s) that could "
            "not be initialized and are unavailable in it; %d failure(s): %s%s", summary.failed_apps,
            summary.failed_actions, count, names.join(", "), count > names.size()
                ? sprintf(", and %d more", count - names.size())
                : "");
    }

    #! Materializes the loaded providers and copies the index candidate from them
    /** @param index the index to add the candidate to
        @param discovery the open discovery generation; the caller seals it
        @param app_filter optional apps to index
        @param type_info_mismatches option types whose descriptions do not restore an equivalent type are added here

        @return the connection scheme registry revision of the snapshot
    */
    private static int snapshotIndex(reference<hash<DataProviderIndexInfo>> index,
            DataProviderDiscoverySession discovery, *list<string> app_filter,
            reference<list<hash<auto>>> type_info_mismatches) {
        # Finish deferred registration, output-type resolution, and scheme loading before starting the authoritative
        # snapshot. These calls are allowed to advance the revision; the second getIndexApps() below is the actual
        # checked-point read.
        list<hash<DataProviderAppInfo>> apps = getIndexApps(app_filter);
        DataProviderActionCatalog::getAllActions();
        apps = getIndexApps(app_filter);
        foreach hash<DataProviderAppInfo> app in (apps) {
            if (app.scheme) {
                ConnectionSchemeCache::getScheme(app.scheme);
            }
        }
        DataProviderActionCatalog::getAllActions();

        # Reading factory info can resolve types, so it is read during the warm-up, like the apps and actions; the
        # factorymap keys are complete here, only their module names are resolved below
        hash<string, hash<auto>> factory_info = getSerializableFactoryInfo(keys index.factorymap, discovery);

        # Deferred initialization can load another producer or add exact identities to an existing producer's
        # inventory. Collect only after that intentional warm-up, otherwise the build invalidates its own inventory
        # snapshot. Changes during collection or after this point must still reject publication; do not retry or
        # recollect over a failed snapshot.
        DataProviderActionCatalog::collectDiscoveryExpectedInventory(discovery);

        # The warm-up above materializes every selected pending app/action and resolves deferred output types. Mark
        # the exact revision immediately before copying the authoritative index candidate. A concurrent registration
        # (including a DataProvider factory mutation) then makes sealDiscovery() reject this attempt.
        DataProviderActionCatalog::checkpointDiscoverySnapshot(discovery);
        int scheme_registry_revision
            = ConnectionSchemeCache::getRegistryRevision();
        apps = getIndexApps(app_filter);
        hash<string, string> module_names = getModuleNames();
        index.factorymap = map {$1.key: moduleDependency($1.value, module_names)}, index.factorymap.pairIterator();
        foreach string section in (("appmap", "schememap")) {
            foreach string key in (keys index{section}) {
                index{section}{key} = map {moduleDependency($1, module_names): True}, keys index{section}{key};
            }
        }
        # every factory registered here has its info in the index, so that listing factories never loads their
        # modules; seeded info takes precedence, like the seeded module of a factory
        index.factoryinfomap = factory_info + (index.factoryinfomap ?? {});

        # Dependencies are module identities; physical registration paths remain in the live catalog.
        foreach hash<DataProviderAppInfo> app in (apps) {
            *list<string> paths = DataProviderActionCatalog::getModulePathsForApp(app.name);
            if (!paths) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "app", "publication", app.name,
                    "DATA-PROVIDER-MODULE-PATH-MISSING",
                    sprintf("data provider app %y has no module path",
                        app.name), NOTHING, app.name);
                continue;
            }
            list<string> dependencies = map moduleDependency($1, module_names), paths;
            # add to app info map
            index.appinfomap{app.name} = app;
            # add to app action info map (skip apps with no actions)
            if (*hash<string, hash<auto>> actions = getSerializableAppActions(app.name, discovery,
                    \type_info_mismatches)) {
                index.actioninfomap{app.name} = actions;
            }
            if (!index.appmap{app.name}) {
                map index.appmap{app.name}{$1} = True, dependencies;
                map log(LoggerLevel::LevelDebug, "Registered data provider app %y -> %y", app.name, $1), dependencies;
            }
            if (app.scheme) {
                *hash<ConnectionSchemeInfo> info = ConnectionSchemeCache::getScheme(app.scheme);
                if (!info) {
                    DataProviderActionCatalog::reportDiscoveryFailure(
                        discovery, "scheme", "publication", app.scheme,
                        "DATA-PROVIDER-SCHEME-UNREGISTERED", sprintf(
                            "data provider app %y has unregistered scheme %y",
                            app.name, app.scheme), NOTHING, app.name);
                    continue;
                }
                foreach string dependency in (dependencies) {
                    map index.schememap{$1}{dependency} = True, keys info.schemes;
                    map log(LoggerLevel::LevelDebug, "Registered scheme %y -> %y", $1, dependency), keys info.schemes;
                }
            }
        }

        int current_scheme_registry_revision
            = ConnectionSchemeCache::getRegistryRevision();
        if (current_scheme_registry_revision != scheme_registry_revision) {
            DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                "scheme", "catalog-snapshot-stale",
                "connection-scheme-registry",
                "CONNECTION-SCHEME-SNAPSHOT-STALE", sprintf(
                    "connection scheme registry changed from revision %d to %d while the authoritative snapshot "
                        "was copied", scheme_registry_revision,
                    current_scheme_registry_revision), {
                    "snapshot_revision": scheme_registry_revision,
                    "current_revision": current_scheme_registry_revision,
                });
        }

        return scheme_registry_revision;
    }

    #! Resolves exact runtime ownership; never infers a module name from an artifact filename
    private static string moduleDependency(string value, hash<string, string> module_names) {
        if (exists module_names{value}) {
            return module_names{value};
        }
        # Optional known factories need not be installed. Keep their declared logical names;
        # an explicit path, however, needs a real registry record to establish its identity.
        if (!value.val() || value =~ /[\/\\\\]/ || value =~ /\.(qm|qmod)$/i) {
            throw "PROVIDER-INDEX-MODULE-ERROR", sprintf("cannot resolve indexed module dependency %y", value);
        }
        return value;
    }

    #! Get YAML-serializable hash of actions keyed by action name
    /** Option types are described by plain data, and output types are not indexed, so reading the index never
        requires the provider modules that define the types; dynamic type and default value callbacks are not run
        (see @ref DataProvider::DataProviderStaticMetadataHelper "DataProviderStaticMetadataHelper")
    */
    private static *hash<string, hash<auto>> getSerializableAppActions(string app,
            DataProviderDiscoverySession discovery, reference<list<hash<auto>>> type_info_mismatches) {
        hash<string, hash<auto>> rv;
        # the index is static metadata, so dynamic type and default value callbacks must not run
        DataProviderStaticMetadataHelper static_metadata();
        foreach hash<DataProviderActionInfo> action in (DataProviderActionCatalog::getActions(app, False)) {
            try {
                hash<auto> entry = (action - ("get_dynamic_options", "get_output_type", "output_type")) + {
                    "options": getSerializableActionOptions(app, action, \type_info_mismatches),
                    "data_provider_info": getSerializableDataProviderInfo(action.data_provider_info),
                };
                checkPlainData(entry, "");
                rv{action.action} = entry;
            } catch (hash<ExceptionInfo> ex) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery, "action", "serialize-action-metadata",
                    app + "/" + action.action, ex.err, ex.desc, ex.arg, app, action.action);
            }
        }
        return rv;
    }

    #! Returns the serializable info of the given data provider factories that are registered
    /** The info is that returned by
        @ref DataProvider::AbstractDataProviderFactory::getInfoAsData() "AbstractDataProviderFactory::getInfoAsData()"
        with type information, read as static metadata (see
        @ref DataProvider::DataProviderStaticMetadataHelper "DataProviderStaticMetadataHelper"), with the unique keys
        of its type descriptions replaced, so that the same index is always written identically.

        @param factories the names of the factories in the index
        @param discovery the open discovery generation; info that is not plain data is reported as a failure

        @return the info of each registered factory, keyed by factory name; a factory that is not registered has
        no info
    */
    private static hash<string, hash<auto>> getSerializableFactoryInfo(*list<string> factories,
            DataProviderDiscoverySession discovery) {
        hash<string, bool> registered = map {$1: True}, DataProvider::listFactories();
        hash<string, hash<auto>> rv = {};
        # the index is static metadata, so dynamic type and default value callbacks must not run
        DataProviderStaticMetadataHelper static_metadata();
        foreach string name in (factories) {
            # a factory that is not registered is never loaded here, which would change the registry
            if (!registered{name}) {
                continue;
            }
            try {
                # registered, so it is returned without loading a module
                *AbstractDataProviderFactory factory = DataProvider::getFactory(name);
                if (!factory) {
                    continue;
                }
                # unique keys identify objects by address, so they are replaced to make builds reproducible
                hash<string, string> key_map;
                hash<auto> info = replaceUniqueKeys(factory.getInfoAsData(True), \key_map);
                checkPlainData(info, "");
                rv{name} = info;
            } catch (hash<ExceptionInfo> ex) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery, "factory", "serialize-factory-info",
                    name, ex.err, ex.desc, ex.arg);
            }
        }
        return rv;
    }

    #! Return apps to include in the index
    private static list<hash<DataProviderAppInfo>> getIndexApps(*list<string> apps) {
        list<hash<DataProviderAppInfo>> all_apps = DataProviderActionCatalog::getAllApps();
        if (!apps) {
            return all_apps;
        }

        hash<string, bool> app_filter = map {$1: True}, apps.iterator();
        list<hash<DataProviderAppInfo>> rv;
        foreach hash<DataProviderAppInfo> app in (all_apps) {
            if (remove app_filter{app.name}) {
                push rv, app;
            }
        }
        return rv;
    }

    #! Get YAML-serializable action options
    private static *hash<auto> getSerializableActionOptions(string app, hash<DataProviderActionInfo> action,
            reference<list<hash<auto>>> type_info_mismatches) {
        hash<auto> rv;
        foreach hash<auto> i in (action.options.pairIterator()) {
            hash<DataTypeInfo> type_info = i.value.type.getInfo();
            # unique keys identify objects by address, so they are replaced to make builds reproducible
            hash<string, string> key_map;
            type_info = replaceUniqueKeys(type_info, \key_map);
            if (*hash<auto> mismatch = checkTypeInfo(type_info)) {
                push type_info_mismatches, {"app": app, "action": action.action, "option": i.key} + mismatch;
            }
            rv{i.key} = (i.value - "type") + {
                "type_info": type_info,
            };
        }
        return rv;
    }

    #! Replaces the unique keys in a type description with keys that do not depend on the process
    /** Unique keys are derived from object addresses; within one description they only need to identify the same
        type or field consistently, which a recursive type description relies on to refer to an enclosing type.

        @param value the description or part of it
        @param key_map maps each original key to its replacement

        @return the description with replaced unique keys
    */
    private static auto replaceUniqueKeys(auto value, reference<hash<string, string>> key_map) {
        switch (value.typeCode()) {
            case NT_HASH:
                foreach string key in (keys value) {
                    if (key == "unique_key" && value{key}.typeCode() == NT_STRING) {
                        value{key} = key_map{value{key}}
                            ?? (key_map{value{key}} = (key_map.size() + 1).toString());
                    } else {
                        value{key} = replaceUniqueKeys(value{key}, \key_map);
                    }
                }
                return value;
            case NT_LIST:
                return map replaceUniqueKeys($1, \key_map), value;
        }
        return value;
    }

    #! Checks that the given value contains only plain data that can be written to the index
    /** @param value the value to check
        @param path the path of the value in the action's index entry

        @throw PROVIDER-INDEX-ERROR the value contains an object or a call reference, such as a type option set to
        a closure
    */
    private static checkPlainData(auto value, string path) {
        switch (value.typeCode()) {
            case NT_HASH:
                map checkPlainData($1.value, path + "." + $1.key), value.pairIterator();
                return;
            case NT_LIST:
                for (int i = 0; i < value.size(); ++i) {
                    checkPlainData(value[i], sprintf("%s[%d]", path, i));
                }
                return;
            case NT_OBJECT:
                throw "PROVIDER-INDEX-ERROR", sprintf("%s: object of class %y cannot be indexed",
                    path.val() ? path.substr(1) : "<root>", value.className());
        }
        if (value.callp()) {
            throw "PROVIDER-INDEX-ERROR", sprintf("%s: a call reference cannot be indexed",
                path.val() ? path.substr(1) : "<root>");
        }
    }

    #! Checks that a type description restores an equivalent type
    /** Index consumers that need a type object create it from the indexed description with
        @ref DataProvider::AbstractDataProviderType::get() "AbstractDataProviderType::get()"; a
        description that does not restore an equivalent type is recorded in the index summary so that the loss of
        fidelity is visible rather than silent.

        @param type_info the type description

        @return @ref nothing if the restored type has the same description, otherwise a hash with a \c path key
        giving the first differing key of the description, or an \c error key if the type cannot be restored
    */
    private static *hash<auto> checkTypeInfo(hash<DataTypeInfo> type_info) {
        hash<DataTypeInfo> restored;
        try {
            restored = AbstractDataProviderType::get(type_info).getInfo();
        } catch (hash<ExceptionInfo> ex) {
            return {"error": sprintf("%s: %s", ex.err, ex.desc)};
        }
        if (*string path = getTypeInfoDifference(type_info, restored, "")) {
            return {"path": path};
        }
    }

    #! Returns the path of the first difference between two type descriptions, ignoring unique keys
    private static *string getTypeInfoDifference(auto a, auto b, string path) {
        if (a.typeCode() == NT_HASH && b.typeCode() == NT_HASH) {
            foreach string key in (keys (a + b)) {
                if (key == "unique_key") {
                    continue;
                }
                if (*string rv = getTypeInfoDifference(a{key}, b{key}, path + "." + key)) {
                    return rv;
                }
            }
            return;
        }
        if (a.typeCode() == NT_LIST && b.typeCode() == NT_LIST && a.size() == b.size()) {
            # accepted and returned types are sets; their order is that of the hash they are the keys of
            if (path =~ /\.types_(accepted|returned)$/) {
                return sort(a) == sort(b) ? NOTHING : path.substr(1);
            }
            for (int i = 0; i < a.size(); ++i) {
                if (*string rv = getTypeInfoDifference(a[i], b[i], sprintf("%s[%d]", path, i))) {
                    return rv;
                }
            }
            return;
        }
        return a === b ? NOTHING : (path.val() ? path.substr(1) : "<root>");
    }

    #! Get YAML-serializable data provider info
    private static *hash<auto> getSerializableDataProviderInfo(*hash<auto> info) {
        if (!info) {
            return;
        }
        hash<auto> rv;
        foreach hash<auto> i in (info.pairIterator()) {
            if (i.key == "bulk_read_shapes" || i.key == "bulk_create_shapes" || i.key == "bulk_upsert_shapes") {
                rv{i.key} = map AbstractDataProcessor::shapeName($1), i.value;
            } else {
                rv{i.key} = i.value;
            }
        }
        foreach string key in (DataProviderInfoOptionMembers) {
            if (rv{key}) {
                rv{key} = getSerializableDataProviderOptions(rv{key});
            }
        }
        if (info.expressions) {
            rv.expressions = map {
                $1.key: $1.value + {
                    "args": (map $1 + {
                        "type": $1.type.getBaseTypeName(),
                    }, $1.value.args),
                    "return_type": $1.value.return_type.getBaseTypeName(),
                }
            }, info.expressions.pairIterator();
        }
        foreach string key in ("events", "messages") {
            if (info{key}) {
                rv{key} = map {$1.key: {
                    "desc": $1.value.desc,
                    "type": $1.value.type.getBaseTypeName(),
                }}, info{key}.pairIterator();
            }
        }
        return rv;
    }

    #! Get YAML-serializable data provider options
    private static *hash<auto> getSerializableDataProviderOptions(*hash<auto> options) {
        return map {$1.key: $1.value + {
            "type": (map $1.getBaseTypeName(), $1.value.type),
        }}, options.pairIterator();
    }

    #! Prepares an index worker process before any provider module is loaded
    /** Enters index build mode and sets the environment for the worker's batch: \c QORE_DATA_PROVIDERS names only
        the batch's environment modules, and the TypeScript app catalogue settings
        (\c QORE_TYPESCRIPT_MASTER_ACTION_SCRIPT and \c QORE_TYPESCRIPT_ACTION_SCRIPTS) are removed unless the
        batch indexes the catalogue, so that a worker that loads the TypeScriptActionInterface module for another
        reason does not load the whole catalogue.

        @ref runIndexWorker() calls this method itself, before running its setup code; a worker command that
        loads modules of its own before calling @ref runIndexWorker() must call this method first.

        @param batch_path the path to the batch file, given to the worker command as its last argument

        @throw PROVIDER-INDEX-WORKER-ERROR the batch file cannot be read

        @par Example:
        A worker command for an application whose own modules load provider modules:
        @code{.py}
#!/usr/bin/env qore
%requires ProviderIndex

exit(ProviderIndex::runIndexWorker(ARGV[0], sub () {
    load_module("MyAppProviderSupport");
}));
        @endcode

        @since %ProviderIndex 1.2
    */
    static prepareIndexWorker(string batch_path) {
        getWorkerBatch(batch_path);
    }

    #! Runs one index worker
    /** A worker is a process started by @ref createDataProviderIndex() when
        @ref ProviderIndexCreateOptions::workers "workers" is set.  It enters index build mode, loads the provider
        modules of its batch, materializes their apps and actions, and writes their index metadata and its discovery
        report to the build's staging directory; it never writes the index itself.

        @param batch_path the path to the batch file, given to the worker command as its last argument
        @param setup optional code run after the worker process is prepared (see @ref prepareIndexWorker()) and
        before the batch is indexed, such as loading modules that the embedding application requires; an exception
        it raises is reported by the build as the worker's error

        @return the exit code for the worker process: 0 if the batch was indexed, including when some of its apps or
        actions could not be initialized, which the build records, 1 if its discovery failed otherwise, in which case
        the build reports the failures, or 2 if the worker failed

        @par Example:
        A worker command:
        @code{.py}
#!/usr/bin/env qore
%requires ProviderIndex

exit(ProviderIndex::runIndexWorker(ARGV[0]));
        @endcode

        @see prepareIndexWorker()

        @since %ProviderIndex 1.2
    */
    static int runIndexWorker(string batch_path, *code<nothing()> setup) {
        hash<auto> batch;
        try {
            batch = getWorkerBatch(batch_path);
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("ProviderIndex worker: %s\n", get_exception_string(ex));
            return 2;
        }

        hash<auto!> result = {"id": batch.id};
        int rc;
        try {
            if (setup) {
                setup();
            }
            result += indexWorkerBatch(batch);
            # the failures of individual apps and actions are recorded by the build
            rc = result.report.failures ? 1 : 0;
        } catch (hash<ExceptionInfo> ex) {
            # the error is reported by the build; the details are written to the worker's standard error
            result.error = sprintf("%s: %s", ex.err, ex.desc);
            result.error_detail = get_exception_string(ex);
            rc = 2;
        }
        try {
            writeFileAtomic(batch.result_path, packIndexData(result));
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("ProviderIndex worker: cannot write the result to %y: %s\n", batch.result_path,
                get_exception_string(ex));
            return 2;
        }
        if (result.error) {
            stderr.printf("ProviderIndex worker: %s\n", result.error_detail);
        }
        return rc;
    }

    #! Returns the worker's batch; the first call also prepares the worker process for it
    private static hash<auto> getWorkerBatch(string batch_path) {
        if (worker_batch) {
            return worker_batch;
        }
        # the index being replaced is never read while its replacement is built
        setBuildMode(True);

        hash<auto> batch;
        try {
            batch = unpackIndexData(ReadOnlyFile::readBinaryFile(batch_path));
        } catch (hash<ExceptionInfo> ex) {
            throw "PROVIDER-INDEX-WORKER-ERROR", sprintf("cannot read batch file %y: %s: %s", batch_path, ex.err,
                ex.desc);
        }

        # the worker loads only the environment modules of its batch; getenv() is read when they are loaded
        list<string> environment = map $1.name, batch.items, $1.type == "environment";
        if (environment) {
            setenv("QORE_DATA_PROVIDERS", environment.join(PathSep));
        } else {
            unsetenv("QORE_DATA_PROVIDERS");
        }
        # only the TypeScript worker loads the TypeScript app catalogue
        if (!(select batch.items, $1.type == "typescript" || $1.name == "TypeScriptActionInterface")) {
            unsetenv("QORE_TYPESCRIPT_MASTER_ACTION_SCRIPT");
            unsetenv("QORE_TYPESCRIPT_ACTION_SCRIPTS");
        }
        return worker_batch = batch;
    }

    #! Indexes the providers of one worker batch
    /** @param batch the batch

        @return the worker result, without its \c id
    */
    private static hash<auto> indexWorkerBatch(hash<auto> batch) {
        *list<string> app_filter = batch.apps;
        *DataProviderDiscoverySession discovery = DataProviderActionCatalog::beginDiscovery((), app_filter);
        *DataProviderQualifiedDiscovery qualification;
        on_exit if (discovery) {
            DataProviderActionCatalog::abortDiscovery(discovery);
        }
        on_exit if (qualification) {
            DataProviderActionCatalog::discardQualifiedDiscovery(qualification);
        }

        # make sure that all data providers are initialized immediately
        DataProvider::setOptions(DPO_DisableOnDemandInitialization | DPO_EnableOnDemandActions);

        # factories registered by the batch's modules are indexed only if the build does not know them already
        hash<DataProviderIndexInfo> index = <DataProviderIndexInfo>{
            "factorymap": DataProvider::FactoryMap,
        };
        bool environment;
        bool typescript;
        foreach hash<auto> item in (batch.items) {
            switch (item.type) {
                case "module": {
                    # the same load as DataProvider::registerKnownFactoriesWithReport()
                    int sp = ImplicitModuleTransaction::savepoint();
                    try {
                        load_module(item.name);
                        ImplicitModuleTransaction::commit(sp);
                    } catch (hash<ExceptionInfo> ex) {
                        ImplicitModuleTransaction::rollback(sp);
                        DataProviderActionCatalog::reportDiscoveryFailure(discovery, "factory", "load-known-factory",
                            item.name, ex.err, ex.desc, ex.arg, NOTHING, NOTHING, item.name);
                    }
                    break;
                }
                case "environment":
                    # QORE_DATA_PROVIDERS is set to the batch's environment modules by the build
                    environment = True;
                    break;
                case "typescript":
                    # loaded after the factories are collected, as in a single-process build
                    typescript = True;
                    break;
                default:
                    throw "PROVIDER-INDEX-WORKER-ERROR", sprintf("unknown index worker batch item type %y",
                        item.type);
            }
        }

        if (environment && getenv("QORE_DATA_PROVIDERS")) {
            try {
                foreach hash<DataProviderEnvironmentLoadFailureInfo> failure in
                        (DataProvider::loadProvidersFromEnvironmentWithReport()) {
                    DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                        "source", failure.phase, failure.module,
                        failure.error, failure.description, failure.argument,
                        NOTHING, NOTHING, failure.module);
                }
            } catch (hash<ExceptionInfo> ex) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "source", "load-environment", "QORE_DATA_PROVIDERS",
                    ex.err, ex.desc, ex.arg);
            }
        }

        # a single-process build indexes every factory registered by the time the environment modules have been
        # loaded, including factories registered by known factory modules that are not known factories
        # themselves, so every worker does the same
        if (batch.collect_factories) {
            addDataProviderFactories(\index, discovery);
        }

        if (typescript) {
            tryLoadTypeScriptActionInterface(discovery);
        }

        list<hash<auto>> type_info_mismatches = ();
        int scheme_registry_revision = snapshotIndex(\index, discovery, app_filter, \type_info_mismatches);

        hash<auto!> rv = {
            "type_info_mismatches": type_info_mismatches,
        };
        try {
            # the failures of individual apps and actions are recorded by the build
            qualification = DataProviderActionCatalog::sealDiscovery(discovery, True);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "DATA-PROVIDER-DISCOVERY-ERROR" && ex.arg.typeCode() == NT_HASH) {
                discovery = NOTHING;
                rv.report = toPlainData(ex.arg);
                return rv;
            }
            rethrow;
        }
        discovery = NOTHING;
        hash<DataProviderDiscoveryReportInfo> report = qualification.getReport();
        rv.report = toPlainData(report);
        # the build does not load the worker's modules, so the owners of the failures are resolved here
        hash<string, string> module_names = getModuleNames();
        rv.failure_modules = map {$1.module_context: getFailureModule($1.module_context, module_names)},
            report.unavailable, exists getFailureModule($1.module_context, module_names);
        return rv + ConnectionSchemeCache::publishStableRegistrySnapshot(
            scheme_registry_revision, hash<auto> sub () {
                return DataProviderActionCatalog::publishQualifiedDiscovery(
                    qualification, hash<auto> sub () {
                        return writeWorkerFragments(index, batch);
                    });
                });
    }

    #! Writes the index metadata of each app indexed by a worker to the build's staging directory
    /** @param index the worker's index
        @param batch the batch

        @return the maps of the worker's index, except its actions, and a description of each app's metadata
    */
    private static hash<auto> writeWorkerFragments(hash<DataProviderIndexInfo> index, hash<auto> batch) {
        hash<auto> apps = {};
        foreach string app in (keys index.appinfomap) {
            *hash<auto> actions = index.actioninfomap{app};
            # the fragment has the format of the app's metadata cache chunk
            binary data = packMetadataCacheChunk(app, index.appinfomap{app}, actions);
            string file = sprintf("%s%s.msgpack", batch.prefix, digest("sha256", app).toHex());
            writeFileAtomic(batch.staging_dir + DirSep + file, data);
            apps{app} = {
                "file": file,
                "size": data.size(),
                "sha256": digest("sha256", data).toHex(),
                "info_sha256": digest("sha256", packIndexData(index.appinfomap{app})).toHex(),
                "has_actions": exists actions,
                "action_sha256": (map {$1.key: digest("sha256", packIndexData($1.value)).toHex()},
                    actions.pairIterator()) ?? {},
            };
        }
        return {
            "factorymap": index.factorymap - keys DataProvider::FactoryMap,
            # the info of the known factories too, since the calling process does not load them
            "factoryinfomap": index.factoryinfomap ?? {},
            "appmap": index.appmap ?? {},
            "schememap": index.schememap ?? {},
            "appinfomap": index.appinfomap ?? {},
            "apps": apps,
        };
    }

    #! Returns the given value with all objects and call references replaced by descriptions
    private static auto toPlainData(auto value) {
        switch (value.typeCode()) {
            case NT_HASH:
                return map {$1.key: toPlainData($1.value)}, value.pairIterator();
            case NT_LIST:
                return map toPlainData($1), value;
            case NT_OBJECT:
                return sprintf("<object of class %s>", value.className());
        }
        if (value.callp()) {
            return "<call reference>";
        }
        return value;
    }

    #! Builds the index with worker processes
    private static hash<auto> createDataProviderIndexWithWorkers(hash<DataProviderIndexInfo> index,
            hash<ProviderIndexCreateOptions> opts, date start) {
%ifdef NoProcess
        throw "PROVIDER-INDEX-WORKER-ERROR", "the process module 2.2 or later is required; index worker processes "
            "cannot be started";
%else
        list<hash<auto>> batches = planWorkerBatches(opts);
        list<string> command = opts.worker_command ?? getDefaultWorkerCommand();
        if (!command) {
            throw "PROVIDER-INDEX-WORKER-ERROR", "the index worker command is empty";
        }

        # the staging directory is in the index directory, so worker output never crosses file systems
        string staging_dir = sprintf("%s%s.qore-index-build-%d-%d", getWritableIndexDir(), DirSep, getpid(),
            clock_getmicros());
        mkdir(staging_dir, 0700);
        on_exit removeStagingDir(staging_dir);

        # workers inherit this process's environment, which is changed only while they are started
        hash<string, *string> saved_env = setWorkerEnvironment();
        on_exit restoreWorkerEnvironment(saved_env);

        # a build is cancelled by cancelWorkers() while it runs
        int generation = getCancelGeneration();

        # run the workers
        int count = batches.size();
        int next = 0;
        Mutex lock();
        hash<string, hash<auto>> runs = {};
        Counter running();
        # a worker that fails outright (as opposed to reporting discovery failures) fails the build, so no further
        # workers are started; such failures usually have a common cause, such as a missing required module
        bool abort;
        int threads = opts.workers < count ? opts.workers : count;
        for (int i = 0; i < threads; ++i) {
            running.inc();
            background sub () {
                on_exit running.dec();
                while (True) {
                    hash<auto> batch;
                    {
                        lock.lock();
                        on_exit lock.unlock();
                        if (next == count || abort || isCancelled(generation)) {
                            return;
                        }
                        batch = batches[next++];
                    }
                    log(LoggerLevel::LevelInfo, "index worker %s started for %y", batch.id,
                        map $1.name, batch.items);
                    hash<auto> run = runWorker(batch, command, staging_dir, opts.worker_timeout_ms, generation);
                    log(LoggerLevel::LevelInfo, "index worker %s finished in %y with exit code %y", batch.id,
                        run.elapsed, run.exit_code);
                    lock.lock();
                    on_exit lock.unlock();
                    runs{batch.id} = run;
                    if (getWorkerRunError(run, opts.worker_timeout_ms)) {
                        abort = True;
                    }
                }
            }();
        }
        running.waitForZero();
        if (isCancelled(generation)) {
            throw "PROVIDER-INDEX-CANCELLED", "the index build was cancelled; no index was published";
        }
        restoreWorkerEnvironment(saved_env);
        saved_env = {};

        # resolve the seeded and known factories and seeded dependencies with this process's modules
        hash<string, string> module_names = getModuleNames();
        index.factorymap += DataProvider::FactoryMap;
        index.factorymap = map {$1.key: moduleDependency($1.value, module_names)}, index.factorymap.pairIterator();
        foreach string section in (("appmap", "schememap")) {
            foreach string key in (keys index{section}) {
                index{section}{key} = map {moduleDependency($1, module_names): True}, keys index{section}{key};
            }
        }
        hash<string, bool> seeded_apps = (map {$1: True}, keys index.appmap) ?? {};

        list<hash<DataProviderDiscoveryFailureInfo>> failures = ();
        # the failures of individual apps and actions, which are recorded in the index
        list<hash<DataProviderDiscoveryFailureInfo>> unavailable = ();
        # the module names of the owners of the failures, resolved by the workers
        hash<string, string> failure_modules = {};
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected = opts.expected_inventory ?? ();
        map expected += <DataProviderDiscoveryExpectedIdentityInfo>{"app": $1}, opts.apps;
        hash<string, bool> report_apps = {};
        hash<string, bool> report_actions = {};
        list<hash<auto>> type_info_mismatches = ();
        hash<string, bool> type_info_mismatch_keys = {};
        list<hash<auto>> worker_summary = ();

        # merged state: factories registered by workers, their info and its digest, app info, action digests, and the
        # fragments of each app
        hash<string, hash<auto>> worker_factories = {};
        hash<string, hash<auto>> worker_factory_info = {};
        hash<string, hash<auto>> worker_factory_info_sha256 = {};
        hash<string, hash<auto>> appinfomap = {};
        hash<string, string> app_info_sha256 = {};
        hash<string, hash<string, string>> action_sha256 = {};
        hash<string, list<hash<auto>>> app_sources = {};

        code conflict = sub (string kind, string identity, string desc, *string app, *string action) {
            failures += <DataProviderDiscoveryFailureInfo>{
                "generation": 0,
                "kind": kind,
                "phase": "worker-merge",
                "identity": identity,
                "app": app,
                "action": action,
                "error": "PROVIDER-INDEX-WORKER-CONFLICT",
                "description": desc,
            };
        };

        list<string> not_run = ();
        foreach hash<auto> batch in (batches) {
            *hash<auto> run = runs{batch.id};
            if (!run) {
                not_run += batch.id;
                continue;
            }
            *hash<auto> result = run.result;
            list<string> items = map $1.name, batch.items;
            worker_summary += {
                "id": batch.id,
                "items": items,
                "exit_code": run.exit_code,
                "elapsed": run.elapsed,
                "apps": result.apps.size(),
            };

            if (*string error = getWorkerRunError(run, opts.worker_timeout_ms)) {
                failures += <DataProviderDiscoveryFailureInfo>{
                    "generation": 0,
                    "kind": "source",
                    "phase": "worker",
                    "identity": batch.id,
                    "error": "PROVIDER-INDEX-WORKER-ERROR",
                    "description": sprintf("index worker %s for %y failed: %s", batch.id, items, error),
                    "argument": {
                        "items": items,
                        "exit_code": run.exit_code,
                        "stderr": run.stderr,
                    },
                };
                continue;
            }

            # the result is plain data, so its typed hashes are restored
            failures += map cast<hash<DataProviderDiscoveryFailureInfo>>($1), result.report.failures;
            unavailable += map cast<hash<DataProviderDiscoveryFailureInfo>>($1), result.report.unavailable;
            map failure_modules{$1.key} = $1.value, (result.failure_modules ?? {}).pairIterator();
            expected += map cast<hash<DataProviderDiscoveryExpectedIdentityInfo>>($1), result.report.expected;
            map report_apps{$1} = True, result.report.apps;
            map report_actions{$1} = True, result.report.actions;
            foreach hash<auto> mismatch in (result.type_info_mismatches) {
                string key = sprintf("%s/%s/%s", mismatch.app, mismatch.action, mismatch.option);
                if (!type_info_mismatch_keys{key}) {
                    type_info_mismatch_keys{key} = True;
                    type_info_mismatches += mismatch;
                }
            }

            foreach hash<auto> i in (result.factorymap.pairIterator()) {
                if (index.factorymap{i.key}) {
                    # seeded and known factories take precedence, as in a single-process build
                    continue;
                }
                if (*hash<auto> other = worker_factories{i.key}) {
                    if (other.module != i.value) {
                        conflict("factory", i.key, sprintf("data provider factory %y is registered by module %y "
                            "in worker %s and by module %y in worker %s", i.key, other.module, other.worker,
                            i.value, batch.id));
                    }
                    continue;
                }
                worker_factories{i.key} = {"module": i.value, "worker": batch.id};
            }
            foreach hash<auto> i in (result.factoryinfomap.pairIterator()) {
                string sha256 = digest("sha256", packIndexData(i.value)).toHex();
                if (*hash<auto> other = worker_factory_info_sha256{i.key}) {
                    if (other.sha256 != sha256) {
                        conflict("factory", i.key, sprintf("data provider factory %y is described differently by "
                            "worker %s and by worker %s; first difference: %s", i.key, other.worker, batch.id,
                            getTypeInfoDifference(worker_factory_info{i.key}, i.value, "")));
                    }
                    continue;
                }
                worker_factory_info_sha256{i.key} = {"sha256": sha256, "worker": batch.id};
                worker_factory_info{i.key} = i.value;
            }
            foreach hash<auto> i in (result.appmap.pairIterator()) {
                if (!seeded_apps{i.key}) {
                    map index.appmap{i.key}{$1} = True, keys i.value;
                }
            }
            foreach hash<auto> i in (result.schememap.pairIterator()) {
                map index.schememap{i.key}{$1} = True, keys i.value;
            }

            foreach hash<auto> i in (result.apps.pairIterator()) {
                string app = i.key;
                if (*string sha256 = app_info_sha256{app}) {
                    if (sha256 != i.value.info_sha256) {
                        # the supports_* flags describe the app's actions, of which each worker may have registered
                        # only some; the app has a capability if any of its actions has it
                        hash<auto> info = result.appinfomap{app};
                        list<string> flags = select keys (appinfomap{app} + info), $1 =~ /^supports_/;
                        if (packIndexData(appinfomap{app} - flags) == packIndexData(info - flags)) {
                            map appinfomap{app}{$1} = appinfomap{app}{$1} || info{$1}, flags;
                        } else {
                            conflict("app", app, sprintf("data provider app %y is described differently by two "
                                "index workers (the second is worker %s); first difference: %s", app, batch.id,
                                getTypeInfoDifference(appinfomap{app} - flags, info - flags, "")), app);
                        }
                    }
                } else {
                    app_info_sha256{app} = i.value.info_sha256;
                    appinfomap{app} = result.appinfomap{app};
                }
                foreach hash<auto> j in (i.value.action_sha256.pairIterator()) {
                    if (*string sha256 = action_sha256{app}{j.key}) {
                        if (sha256 != j.value) {
                            conflict("action", app + "/" + j.key, sprintf("data provider action %y of app %y is "
                                "described differently by two index workers (the second is worker %s)", j.key, app,
                                batch.id), app, j.key);
                        }
                    } else {
                        action_sha256{app}{j.key} = j.value;
                    }
                }
                if (i.value.has_actions) {
                    app_sources{app} += i.value{"file", "size", "sha256"};
                }
            }
        }
        map index.factorymap{$1.key} = $1.value.module, worker_factories.pairIterator();
        if (not_run) {
            failures += <DataProviderDiscoveryFailureInfo>{
                "generation": 0,
                "kind": "source",
                "phase": "worker",
                "identity": "index-build",
                "error": "PROVIDER-INDEX-WORKER-ERROR",
                "description": sprintf("%d index worker batch(es) were not run because an index worker failed",
                    not_run.size()),
                "argument": {
                    "batches": not_run,
                },
            };
        }

        # the expected inventory of the build and of every worker must be present in the merged index
        hash<string, bool> expected_keys = {};
        list<hash<DataProviderDiscoveryExpectedIdentityInfo>> merged_expected = ();
        foreach hash<DataProviderDiscoveryExpectedIdentityInfo> identity in (expected) {
            string key = identity.app + "/" + (identity.action ?? "");
            if (expected_keys{key}) {
                continue;
            }
            expected_keys{key} = True;
            merged_expected += identity;
            if (identity.action ? !exists action_sha256{identity.app}{identity.action}
                    : !exists appinfomap{identity.app}) {
                # a missing app or action is recorded in the index
                unavailable += <DataProviderDiscoveryFailureInfo>{
                    "generation": 0,
                    "kind": identity.action ? "action" : "app",
                    "phase": "expected-inventory",
                    "identity": identity.action ? key : identity.app,
                    "app": identity.app,
                    "action": identity.action,
                    "error": "DATA-PROVIDER-EXPECTED-IDENTITY-MISSING",
                    "description": sprintf("expected data provider identity %y is not registered",
                        identity.action ? key : identity.app),
                };
            }
        }

        # an identity missing in one worker's scope can be registered by another worker
        unavailable = map $1, unavailable, $1.phase != "expected-inventory"
            || (exists $1.action ? !exists action_sha256{$1.app}{$1.action} : !exists appinfomap{$1.app});
        hash<DataProviderDiscoveryReportInfo> report = <DataProviderDiscoveryReportInfo>{
            "generation": 0,
            "start_revision": 0,
            "sealed_revision": 0,
            "complete": !failures && !unavailable,
            "expected": merged_expected,
            "apps": sort(keys report_apps),
            "actions": sort(keys report_actions),
            "failures": failures,
            "unavailable": unavailable,
        };
        if (failures) {
            throw "DATA-PROVIDER-DISCOVERY-ERROR", sprintf("data provider index workers reported %d failure(s)",
                failures.size()), report;
        }

        # write the index one app at a time from the worker fragments; every factory registered by a worker has its
        # info in the index, and seeded info takes precedence, like the seeded module of a factory
        hash<auto> maps = {
            "factorymap": index.factorymap,
            "factoryinfomap": (map {$1.key: $1.value}, worker_factory_info.pairIterator(),
                exists index.factorymap{$1.key}) + (index.factoryinfomap ?? {}),
            "appmap": index.appmap ?? {},
            "schememap": index.schememap ?? {},
            "appinfomap": (index.appinfomap ?? {}) + appinfomap,
            "failuremap": getFailureMap(unavailable, module_names + failure_modules),
        };
        *hash<string, hash<string, hash<auto>>> seeded_actions = index.actioninfomap;
        hash<string, bool> action_apps = (map {$1: True}, keys seeded_actions) ?? {};
        map action_apps{$1} = True, keys app_sources;
        hash<ProviderIndexInfo> rv = writeDataProviderIndexByApp(maps, keys action_apps, *hash<auto> sub (string app) {
            *hash<auto> actions = seeded_actions{app};
            foreach hash<auto> source in (app_sources{app}) {
                binary data = ReadOnlyFile::readBinaryFile(staging_dir + DirSep + source.file);
                if (data.size() != source.size || digest("sha256", data).toHex() != source.sha256) {
                    throw "PROVIDER-INDEX-WORKER-ERROR", sprintf("index worker metadata for app %y in %y is "
                        "corrupt", app, source.file);
                }
                actions += unpackMetadataCacheChunk(data).actioninfomap{app};
            }
            return actions;
        }, now_us() - start, {
            "factories": index.factorymap.size(),
            "apps": index.appmap.size(),
            "schemes": index.schememap.size(),
            "qualification": report,
            "type_info_mismatches": type_info_mismatches,
            "workers": worker_summary,
        } + getFailureSummary(maps.failuremap), opts.write_canonical_yaml);
        logFailures(maps.failuremap);
        return rv;
%endif
    }

    #! Returns the error of a worker that failed outright, if any
    /** @param run the result of running the worker
        @param timeout_ms the worker timeout

        @return a description of the error, or @ref nothing if the worker reported a result, which can itself report
        discovery failures
    */
    private static *string getWorkerRunError(hash<auto> run, int timeout_ms) {
        *hash<auto> result = run.result;
        if (*string error = run.error ?? result.error) {
            return error;
        }
        if (run.timed_out) {
            return sprintf("the worker did not finish within %d ms and was terminated", timeout_ms);
        }
        if (!result.report) {
            return sprintf("the worker exited with code %y without a result", run.exit_code);
        }
        # a worker whose apps or actions failed indexes the others and exits with 0
        if (run.exit_code != (result.report.failures ? 1 : 0)) {
            return sprintf("the worker exited with code %y", run.exit_code);
        }
    }

    #! Returns the worker batches for a build
    private static list<hash<auto>> planWorkerBatches(hash<ProviderIndexCreateOptions> opts) {
        list<hash<auto>> batches = ();
        # as in a single-process build, registered factories are indexed only if environment modules are loaded
        bool collect_factories = opts.load_environment && getenv("QORE_DATA_PROVIDERS");
        code add = sub (list<hash<auto>> items) {
            batches += {
                "id": sprintf("w%d", batches.size() + 1),
                "items": items,
                "apps": opts.apps,
                "collect_factories": collect_factories,
            };
        };
        # the TypeScript app catalogue takes the longest to index, so its worker starts first; its module is also a
        # known factory module, and a worker that loads it loads the whole catalogue, so no other worker loads it
        list<string> modules = opts.load_known_factories ? sort(DataProvider::FactoryModuleList) : ();
        bool typescript_module = inlist("TypeScriptActionInterface", modules);
        if (typescript_module) {
            modules = select modules, $1 != "TypeScriptActionInterface";
        }
        if (opts.load_typescript_actions) {
            add(({"type": "typescript", "name": "TypeScriptActionInterface"},));
        } else if (typescript_module) {
            add(({"type": "module", "name": "TypeScriptActionInterface"},));
        }
        if (opts.load_environment && (*string providers = getenv("QORE_DATA_PROVIDERS"))) {
            map add(({"type": "environment", "name": $1},)), providers.split(PathSep), $1.val();
        }
        if (modules) {
            int size = opts.worker_batch_size > 0 ? opts.worker_batch_size : 1;
            list<hash<auto>> items = ();
            foreach string mod in (modules) {
                items += {"type": "module", "name": mod};
                if (items.size() == size) {
                    add(items);
                    items = ();
                }
            }
            if (items) {
                add(items);
            }
        }
        # apps registered by the modules that every worker loads are indexed even if no provider module is loaded
        if (!batches) {
            add(());
        }
        return batches;
    }

%ifndef NoProcess
    #! Runs one worker and returns the result of the run
    private static hash<auto> runWorker(hash<auto> batch, list<string> command, string staging_dir, int timeout_ms,
            int generation) {
        string base = staging_dir + DirSep + batch.id;
        batch += {
            "staging_dir": staging_dir,
            "prefix": batch.id + "-",
            "result_path": base + "-result.msgpack",
        };
        hash<auto> run = {};
        date start = now_us();
        string err_path = base + "-stderr.log";
        try {
            string batch_path = base + "-batch.msgpack";
            writeFileAtomic(batch_path, packIndexData(batch));


            File out();
            out.open2(base + "-stdout.log", O_CREAT | O_TRUNC | O_WRONLY, 0600);
            File err();
            err.open2(err_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
            # a worker is started and registered atomically, so cancelWorkers() cannot miss it
            Process p;
            {
                AutoLock al(worker_lock);
                if (cancel_generation != generation) {
                    throw "PROVIDER-INDEX-CANCELLED", "the index build was cancelled";
                }
                p = new Process(command[0], (command.size() > 1 ? command[1..] : ()) + batch_path, {
                    "stdout": out,
                    "stderr": err,
                    # The launching thread waits below for this worker. On Linux, bind its lifetime to that
                    # thread even when the builder crashes or is killed before cancelWorkers() can run.
                    "kill_on_parent_exit": HAVE_PROCESS_KILL_ON_PARENT_EXIT,
                });
                worker_processes{batch_path} = p;
            }
            on_exit {
                AutoLock al(worker_lock);
                remove worker_processes{batch_path};
            }
            if (!p.wait(timeout_ms)) {
                run.timed_out = True;
                p.terminate();
                p.wait();
            }
            run.exit_code = p.exitCode();
        } catch (hash<ExceptionInfo> ex) {
            run.error = sprintf("cannot run the worker: %s: %s", ex.err, ex.desc);
        }
        if (is_file(batch.result_path)) {
            try {
                run.result = unpackIndexData(ReadOnlyFile::readBinaryFile(batch.result_path));
            } catch (hash<ExceptionInfo> ex) {
                run.error = sprintf("cannot read the worker result: %s: %s", ex.err, ex.desc);
            }
        }
        if (run.error || run.exit_code || run.timed_out || run.result.error) {
            run.stderr = getFileTail(err_path);
        }
        run.elapsed = now_us() - start;
        return run;
    }
%endif

    #! Returns the end of the given file for error reports
    private static *string getFileTail(string path, int size = 8192) {
        try {
            ReadOnlyFile f(path);
            int file_size = f.hstat().size;
            if (file_size > size) {
                f.setPos(file_size - size);
            }
            return f.read(-1);
        } catch (hash<ExceptionInfo> ex) {
            # the report is useful without the output
        }
    }

    #! Returns the default worker command: the current Qore interpreter running runIndexWorker()
    private static list<string> getDefaultWorkerCommand() {
        *string qore;
%ifndef NoProcess
        if (HAVE_PROCESS_GETEXECUTABLEPATH) {
            try {
                string exe = Process::getExecutablePath();
                # a program embedding the Qore library is not a Qore interpreter
                if (basename(exe) =~ /^qore/) {
                    qore = exe;
                }
            } catch (hash<ExceptionInfo> ex) {
                # use the interpreter in the PATH
            }
        }
        qore = qore ?? Process::searchPath("qore");
%endif
        return (qore ?? "qore", "-l", "ProviderIndex", "-e",
            "exit(ProviderIndex::ProviderIndex::runIndexWorker(ARGV[0]));");
    }

    #! Sets the environment inherited by worker processes
    /** Workers start in index build mode, and the module path starts with the directory of the ProviderIndex
        module loaded here, so workers load the same modules

        @return the previous values of the environment variables set, to be restored with
        restoreWorkerEnvironment()
    */
    private static hash<string, *string> setWorkerEnvironment() {
        hash<string, *string> rv = {
            ProviderIndexBuildEnvVar: getenv(ProviderIndexBuildEnvVar),
        };
        setenv(ProviderIndexBuildEnvVar, "1");
        if ((*string filename = get_module_hash().ProviderIndex.filename).val()) {
            string dir = dirname(filename);
            if (basename(dir) == "ProviderIndex") {
                dir = dirname(dir);
            }
            *string module_dir = getenv("QORE_MODULE_DIR");
            rv.QORE_MODULE_DIR = module_dir;
            setenv("QORE_MODULE_DIR", dir + (module_dir.val() ? PathSep + module_dir : ""));
        }
        return rv;
    }

    #! Restores the environment changed by setWorkerEnvironment()
    private static restoreWorkerEnvironment(hash<string, *string> saved_env) {
        foreach hash<auto> i in (saved_env.pairIterator()) {
            if (exists i.value) {
                setenv(i.key, i.value);
            } else {
                unsetenv(i.key);
            }
        }
    }

    #! Returns a map of the names, paths, and file names of all loaded modules to their names
    private static hash<string, string> getModuleNames() {
        hash<string, string> module_names;
        foreach hash<auto> mod in (get_module_hash().values()) {
            module_names{mod.name} = mod.name;
            if (mod.path.val()) {
                module_names{mod.path} = mod.name;
            }
            if (mod.filename.val()) {
                module_names{mod.filename} = mod.name;
            }
        }
        return module_names;
    }

    #! Removes the staging directory of a worker build
    private static removeStagingDir(string dir) {
        try {
            Dir d();
            d.chdir(dir);
            map unlink(dir + DirSep + $1), d.listFiles();
            rmdir(dir);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelWarn, "Unable to remove the index build staging directory %y: %s: %s", dir,
                ex.err, ex.desc);
        }
    }

    #! Try to load and initialize TypeScriptActionInterface
    private static tryLoadTypeScriptActionInterface(
            DataProviderDiscoverySession discovery) {
        try {
            load_module("TypeScriptActionInterface");
        } catch (hash<ExceptionInfo> ex) {
            DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                "source", "load-typescript-actions",
                "TypeScriptActionInterface", ex.err, ex.desc, ex.arg,
                NOTHING, NOTHING, "TypeScriptActionInterface");
        }
    }

    #! Iterate all data provider factories and add them to the index
    private static addDataProviderFactories(
            reference<hash<DataProviderIndexInfo>> index,
            DataProviderDiscoverySession discovery) {
         # Iterate all factories and add any new data providers
        foreach string factory in (DataProvider::listFactories()) {
            # get the factory object
            AbstractDataProviderFactory f = DataProvider::getFactory(factory);
            if (!f) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "factory", "lookup", factory,
                    "DATA-PROVIDER-FACTORY-LOOKUP-ERROR", sprintf(
                        "data provider factory %y is registered but cannot be returned",
                        factory));
                continue;
            }

            string factory = f.getName();
            if (index.factorymap{factory}) {
                # already registered
                continue;
            }

            # get the module name for the factory
            *string mod = Class::getClass(f).getModuleName();
            if (!mod) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery,
                    "factory", "publication", factory,
                    "DATA-PROVIDER-FACTORY-MODULE-MISSING", sprintf(
                        "data provider factory %y has no module name", factory));
                continue;
            }

            index.factorymap{factory} = mod;
            log(LoggerLevel::LevelDebug, "Registered factory %y -> %y", factory, mod);
        }
    }
}
}
