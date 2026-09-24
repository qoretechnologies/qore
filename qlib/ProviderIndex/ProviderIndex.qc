# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProviderIndex.qc module for indexing data providers

/*  ProviderIndex.qc Copyright 2025 Qore Technologies, s.r.o.

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
}

#! The main API class
public class ProviderIndex inherits ProviderIndexUtil {
    public {}

    #! Create the data provider index
    /** @param index optional seeded factory, application and scheme dependencies; logical module names are
            preferred, and explicit artifact paths must match the loaded module registry
        @param dir optional output directory; otherwise the configured provider-index directory is used
        @param opts discovery sources, optional exact application/action inventory, and optional application selection
        @return the published index path, size, elapsed generation time, and summary whose \c qualification member is
            the complete @ref DataProvider::DataProviderDiscoveryReportInfo used to authorize publication
        @throw PROVIDER-INDEX-MODULE-ERROR an explicit dependency has no authoritative loaded module identity
        @throw DATA-PROVIDER-DISCOVERY-ERROR if discovery qualification is incomplete
        @throw DATA-PROVIDER-DISCOVERY-STALE if registration changes after qualification
        @throw PROVIDER-INDEX-ERROR the output directory is unavailable
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

        # Finish deferred registration, output-type resolution, and scheme loading before starting the authoritative
        # snapshot. These calls are allowed to advance the revision; the second getIndexApps() below is the actual
        # checked-point read.
        list<hash<DataProviderAppInfo>> apps = getIndexApps(opts.apps);
        DataProviderActionCatalog::getAllActions();
        apps = getIndexApps(opts.apps);
        foreach hash<DataProviderAppInfo> app in (apps) {
            if (app.scheme) {
                ConnectionSchemeCache::getScheme(app.scheme);
            }
        }
        DataProviderActionCatalog::getAllActions();

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
        apps = getIndexApps(opts.apps);
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
        index.factorymap = map {$1.key: moduleDependency($1.value, module_names)}, index.factorymap.pairIterator();
        foreach string section in (("appmap", "schememap")) {
            foreach string key in (keys index{section}) {
                index{section}{key} = map {moduleDependency($1, module_names): True}, keys index{section}{key};
            }
        }

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
            list<hash<DataProviderActionInfo>> app_actions = DataProviderActionCatalog::getActions(app.name, False);
            if (*hash<string, hash<auto>> actions = getSerializableAppActions(app.name, app_actions, discovery)) {
                index.actioninfomap{app.name} = actions;
            }
            if (*binary types = serializeAppActionTypes(app.name, app_actions, discovery)) {
                index.actiontypemap{app.name} = types;
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

        qualification = DataProviderActionCatalog::sealDiscovery(discovery);
        discovery = NOTHING;
        hash<DataProviderDiscoveryReportInfo> qualification_report
            = qualification.getReport();
        # The scheme registry and data-provider catalog hold their registration locks across the O(1) revision
        # checks and atomic file replacement, so neither can invalidate qualification between check and write.
        return ConnectionSchemeCache::publishStableRegistrySnapshot(
            scheme_registry_revision, hash<auto> sub () {
                return DataProviderActionCatalog::publishQualifiedDiscovery(
                    qualification, hash<auto> sub () {
                        return writeDataProviderIndex(index,
                            now_us() - start, {
                                "factories": index.factorymap.size(),
                                "apps": index.appmap.size(),
                                "schemes": index.schememap.size(),
                                "qualification": qualification_report,
                            });
                    });
                });
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
    /** Output types are not indexed, and option types are serialized separately by serializeAppActionTypes()
    */
    private static *hash<string, hash<auto>> getSerializableAppActions(string app,
            list<hash<DataProviderActionInfo>> actions, DataProviderDiscoverySession discovery) {
        hash<string, hash<auto>> rv;
        foreach hash<DataProviderActionInfo> action in (actions) {
            try {
                # dynamic callbacks cannot be serialized, and no index consumer uses output types
                rv{action.action} = (action - ("get_dynamic_options", "get_output_type", "output_type")) + {
                    "options": getSerializableActionOptions(action.options),
                    "data_provider_info": getSerializableDataProviderInfo(action.data_provider_info),
                };
            } catch (hash<ExceptionInfo> ex) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery, "action", "serialize-action-metadata",
                    app + "/" + action.action, ex.err, ex.desc, ex.arg, app, action.action);
            }
        }
        return rv;
    }

    #! Serializes the option types of all of an app's actions as one graph
    /** Types of one app commonly share objects, such as the %OpenAPI document that all schema-backed types of an
        app refer to.  Serializing each type separately repeated every shared object in every type; a single graph
        stores each shared object once, and restoring it shares them again.

        Native serialization retains nested fields, unions, binary types, and choice values.  It never executes
        dynamic type/default/example callbacks; unsupported state fails qualification.

        @param app the app name
        @param actions the app's actions
        @param discovery the discovery session that receives any serialization failure

        @return the zlib-compressed serialized hash of action names to option names to option types; @ref nothing
        if no action has options or the types cannot be serialized
    */
    private static *binary serializeAppActionTypes(string app, list<hash<DataProviderActionInfo>> actions,
            DataProviderDiscoverySession discovery) {
        hash<auto> types;
        foreach hash<DataProviderActionInfo> action in (actions) {
            if (action.options) {
                types{action.action} = map {$1.key: $1.value.type}, action.options.pairIterator();
            }
        }
        if (!types) {
            return;
        }
        try {
            # serialized type graphs compress by more than 10x, and decompressing costs a small fraction of
            # deserializing them
            return compress(Serializable::serialize(types));
        } catch (hash<ExceptionInfo> ex) {
            # attribute the failure to the actions whose types cannot be serialized
            bool reported;
            foreach hash<auto> i in (types.pairIterator()) {
                try {
                    Serializable::serialize(i.value);
                } catch (hash<ExceptionInfo> action_ex) {
                    DataProviderActionCatalog::reportDiscoveryFailure(discovery, "action", "serialize-action-types",
                        app + "/" + i.key, action_ex.err, action_ex.desc, action_ex.arg, app, i.key);
                    reported = True;
                }
            }
            if (!reported) {
                DataProviderActionCatalog::reportDiscoveryFailure(discovery, "app", "serialize-action-types", app,
                    ex.err, ex.desc, ex.arg, app);
            }
        }
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
    private static *hash<auto> getSerializableActionOptions(*hash<auto> options) {
        # option types are serialized per app by serializeAppActionTypes()
        return map {$1.key: $1.value - "type"}, options.pairIterator();
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
        foreach string key in ("constructor", "create", "upsert", "search", "request", "child_create", "child_delete",
            "add_field", "update_field", "delete_field", "send_message") {
            key += "_options";
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
        return rv;
    }

    #! Get YAML-serializable data provider options
    private static *hash<auto> getSerializableDataProviderOptions(*hash<auto> options) {
        return map {$1.key: $1.value + {
            "type": (map $1.getBaseTypeName(), $1.value.type),
        }}, options.pairIterator();
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
