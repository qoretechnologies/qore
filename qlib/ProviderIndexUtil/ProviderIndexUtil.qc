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

        #! Metadata initialization state
        static bool metadata_initialized;
        static bool metadata_initializing;
        static *int metadata_initializing_tid;
        static Condition metadata_init_cond();

        #! All apps loaded flag
        static bool apps_loaded;

        #! Set of modules that have been successfully loaded; used to avoid calling load_module() which acquires
        #! the global module lock even when a module is already loaded
        static hash<string, bool> loaded_module_cache;
    }

    #! Returns a list of cached app info
    static *list<hash<auto>> getCachedApps() {
        checkMetadataIndex();
        return index.appinfomap.values();
    }

    #! Searches the app info cache for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached applications that match the regular expression pattern in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        checkMetadataIndex();
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
        checkMetadataIndex();
        if (ignore_case) {
            substr = substr.upr();

            return map $1, index.appinfomap.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, index.appinfomap.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Try to return any cached app info for the given app
    static *hash<auto> tryGetAppInfo(string app) {
        checkMetadataIndex();
        return index.appinfomap{app};
    }

    #! Try to return any cached approximate app action list for the given app
    static *list<hash<auto>> tryGetApproximateActionsForApp(string app) {
        checkMetadataIndex();
        return index.actioninfomap{app}.values();
    }

    #! Try to return any cached approximate app action for the given app and action
    static *hash<auto> tryGetApproximateAppAction(string app, string action) {
        checkMetadataIndex();
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
        checkMetadataIndex();
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
        checkMetadataIndex();
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
        checkMetadataIndex();
        return map $1, index.top_actioninfomap.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cached actions whose descriptions match a substring
    /** @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All cached actions that match the substring argument in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionSubstr(string substr, bool ignore_case = True) {
        checkMetadataIndex();
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
        checkMetadataIndex();
        return index.top_actioninfomap.values();
    }

    #! Adds the given app and actions to the cache
    static addAppToCache(hash<auto> app) {
        checkMetadataIndex();
        if (!app.name) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add app info to cache: app name is missing (%y)", app);
        }
        index.appinfomap{app.name} = app;
    }

    #! Adds the given app and actions to the cache
    static addActionToCache(hash<auto> action) {
        checkMetadataIndex();
        if (!index.appinfomap{action.app}) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action for unknown app %y to cache", action.app);
        }
        if (!action.action) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add action info to cache for app %y: action name is "
                "missing", action.app);
        }
        index.actioninfomap{action.app}{action.action} = action;
        index.top_actioninfomap{action.app + "/" + action.action} = action;
    }

    #! Removes the given app and all its actions from the cache
    static bool removeCachedApp(string app) {
        checkMetadataIndex();
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
            new_index = readRawDataProviderIndex();
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
        if (new_index) {
            index = new_index;
        }
        initialized = True;
        initializing = False;
        remove initializing_tid;
        init_cond.broadcast();
    }

    #! Ensures that cached app and action metadata has been deserialized
    static private checkMetadataIndex() {
        checkDataIndex();
        if (metadata_initialized) {
            return;
        }

        *hash<DataProviderIndexInfo> raw_index;
        {
            m.lock();
            on_exit m.unlock();
            while (metadata_initializing && metadata_initializing_tid != gettid()) {
                metadata_init_cond.wait(m);
            }

            if (metadata_initialized) {
                return;
            }

            if (metadata_initializing) {
                return;
            }

            metadata_initializing = True;
            metadata_initializing_tid = gettid();
            raw_index = index;
            index = <DataProviderIndexInfo>{
                "factorymap": raw_index.factorymap,
                "appmap": raw_index.appmap,
                "schememap": raw_index.schememap,
                "appinfomap": {},
                "actioninfomap": {},
                "top_actioninfomap": {},
            };
        }

        *hash<DataProviderIndexInfo> new_index;
        try {
            new_index = deserializeDataProviderIndex(raw_index);
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
        if (new_index) {
            index = new_index;
        }
        metadata_initialized = True;
        metadata_initializing = False;
        remove metadata_initializing_tid;
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
        return writeIndex(h, "data", delta, summary);
    }

    #! Reads the data provider index from the configured directory
    static *hash<DataProviderIndexInfo> readDataProviderIndex() {
        return deserializeDataProviderIndex(readRawDataProviderIndex());
    }

    #! Reads the serialized data provider index without reconstructing typed metadata
    static private *hash<DataProviderIndexInfo> readRawDataProviderIndex() {
        try {
            return cast<*hash<DataProviderIndexInfo>>(readIndex("data"));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
        }
    }

    #! Reconstructs typed app and action metadata in a serialized data provider index
    static private *hash<DataProviderIndexInfo> deserializeDataProviderIndex(*hash<DataProviderIndexInfo> rv) {
        if (!rv) {
            return;
        }

        # dynamically load the DataProvider module; cannot be required or it would cause a circular dependency
        load_module("DataProvider");
        load_module("MapperUtil");
        # get app info typed hash object
        TypedHash app_info_th = TypedHash::forName("DataProvider::DataProviderAppInfo");
        # get action typed hash objects
        TypedHash action_info_th = TypedHash::forName("DataProvider::DataProviderActionInfo");
        TypedHash option_info_th = TypedHash::forName("DataProvider::ActionOptionInfo");
        TypedHash path_vars_th = TypedHash::forName("DataProvider::DataProviderPathVarInfo");
        TypedHash dp_info_th = TypedHash::forName("DataProvider::DataProviderInfo");
        TypedHash dp_opt_info_th = TypedHash::forName("DataProvider::DataProviderOptionInfo");
        TypedHash dp_exp_info_th = TypedHash::forName("DataProvider::DataProviderExpressionInfo");
        TypedHash mapper_key_info_th = TypedHash::forName("Mapper::MapperRuntimeKeyInfo");
        TypedHash dp_msg_info_th = TypedHash::forName("DataProvider::DataProviderMessageInfo");
        TypedHash dp_sig_info_th = TypedHash::forName("DataProvider::DataProviderSignatureTypeInfo");
        TypedHash dp_arg_info_th = TypedHash::forName("DataProvider::DataProviderArgInfo");
        # get the AbstractDataProviderTypeMap value
        hash<auto> type_map = Constant::forName("DataProvider::AbstractDataProviderTypeMap").getValue();
        hash<auto> shape_map = getDataProviderShapeMap();
        # get the UiOptionViewMap for enum deserialization
        *hash<auto> ui_view_map = Constant::forName("DataProvider::UiOptionViewMap").getValue();

        try {
            if (rv.appinfomap) {
                TypedHash conn_opt_th = TypedHash::forName("ConnectionProvider::ConnectionOptionInfo");
                rv.appinfomap = map {
                    $1.key: app_info_th.doCast($1.value + (
                        $1.value.connection_options
                        ? {
                            "connection_options": map {
                                $1.key: conn_opt_th.doCast($1.value + {
                                    "allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                                        $1.value.allowed_values),
                                    "element_allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                                        $1.value.element_allowed_values),
                                }),
                            }, $1.value.connection_options.pairIterator(),
                        }
                        : {}
                    )),
                }, rv.appinfomap.pairIterator();
            }
            if (rv.actioninfomap) {
                rv.actioninfomap = deserializeActions(rv.actioninfomap, action_info_th, option_info_th, path_vars_th,
                    dp_info_th, dp_opt_info_th, dp_exp_info_th, mapper_key_info_th, dp_msg_info_th, dp_sig_info_th,
                    dp_arg_info_th, type_map, shape_map, ui_view_map);
                foreach hash<auto> i in (rv.actioninfomap.pairIterator()) {
                    map rv.top_actioninfomap{i.key + "/" + $1.key} = $1.value, i.value.pairIterator();
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s", get_exception_string(ex));
            #log(LoggerLevel::LevelError, "Ignoring error reading data provider index: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Deserialize action options
    static private hash<auto> deserializeActions(hash<string, hash<string, hash<auto>>> actioninfomap,
            TypedHash action_info_th, TypedHash option_info_th, TypedHash path_vars_th, TypedHash dp_info_th,
            TypedHash dp_opt_info_th, TypedHash dp_exp_info_th, TypedHash mapper_key_info_th,
            TypedHash dp_msg_info_th, TypedHash dp_sig_info_th, TypedHash dp_arg_info_th, hash<auto> type_map,
            hash<auto> shape_map, *hash<auto> ui_view_map) {
        hash<auto> rv;
        foreach hash<auto> i in (actioninfomap.pairIterator()) {
            foreach hash<auto> j in (i.value.pairIterator()) {
                on_error rethrow $1.err, sprintf("%s (app %y action %y)", $1.desc, i.key, j.key);
                rv{i.key}{j.key} = action_info_th.doCast(j.value + {
                    "path_vars": (j.value.path_vars
                        ? map {$1.key: path_vars_th.doCast($1.value)}, j.value.path_vars.pairIterator(),
                        : NOTHING),
                    "options": (map {
                        $1.key: option_info_th.doCast(($1.value - "default_view") + {
                            "allowed_values": (map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values),
                            "element_allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                                $1.value.element_allowed_values),
                            "type": type_map{$1.value.type} ?? type_map."auto",
                            "default_view": ui_view_map{$1.value.default_view},
                        })
                    }, j.value.options.pairIterator()),
                    "data_provider_info": deserializeDataProviderInfo(j.value.data_provider_info, dp_info_th,
                        dp_opt_info_th, dp_exp_info_th, mapper_key_info_th, dp_msg_info_th,
                        dp_sig_info_th, dp_arg_info_th, type_map, shape_map),
                });
            }
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
                rv{key} = map {
                    $1.key: dp_opt_info_th.doCast($1.value + {
                        "allowed_values": (map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values),
                        "element_allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                            $1.value.element_allowed_values),
                        "type": (map type_map{$1} ?? type_map."auto", $1.value.type),
                    }),
                }, rv{key}.pairIterator();
            }
        }
        return dp_info_th.doCast(rv + {
            "expressions": (map {
                $1.key: dp_exp_info_th.doCast($1.value + {
                    "args": (map dp_sig_info_th.doCast($1 + {
                        "type": type_map{$1.type} ?? type_map."auto",
                    }), $1.value.args),
                    "arg_info": (map dp_arg_info_th.doCast($1 + {
                        "allowed_values": (map cast<hash<AllowedValueInfo>>($1), $1.allowed_values),
                    }), $1.value.arg_info),
                    "return_type": type_map{$1.value.return_type} ?? type_map."auto",
                })
            }, rv.expressions.pairIterator()),
            "events": (map {
                $1.key: dp_msg_info_th.doCast($1.value)
            }, rv.events.pairIterator()),
            "mapper_keys": (map {
                $1.key: mapper_key_info_th.doCast($1.value)
            }, rv.mapper_keys.pairIterator()),
            "messages": (map {
                $1.key: dp_msg_info_th.doCast($1.value)
            }, rv.messages.pairIterator()),
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
        File f();
        f.open2(path: path, flags: O_CREAT | O_TRUNC | O_WRONLY, mode: 0644);
        string yaml = make_yaml(h, BlockStyle);
        f.write(data: yaml);
        return <ProviderIndexInfo>{
            "path": path,
            "size": yaml.size(),
            "delta": delta,
            "summary": summary,
        };
%endif
    }

    #! Reads the index from the configured directory
    static private *hash<auto> readIndex(string type) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", sprintf("YAML support is not available; cannot read %s provider index", type);
%else
        # Skip silently when the index env var is not configured — this is
        # the default in tests and non-Qorus deployments, not an error.
        # Check the env directly to avoid tryGetIndexDir()'s WARN log.
        if (!ENV{ProviderIndexEnvVar}.val()) {
            return;
        }
        try {
            string path = getIndexPath(type);
            return parse_yaml(File::readTextFile(path));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Ignoring error reading data provider %s index: %s: %s", type, ex.err, ex.desc);
        }
%endif
    }

    #! Returns the index path for the given type
    static string getIndexPath(string type) {
        return sprintf("%s%sqore-%s-index.yaml", tryGetIndexDir(True), DirSep, type);
    }

    #! Sets the environment variable
    static setEnvironmentVariable(string dir) {
        setenv(ProviderIndexEnvVar, dir);
        ENV{ProviderIndexEnvVar} = dir;
    }

    #! Try to get the index directory from the environment variable
    static *string tryGetIndexDir(*bool throw_exception) {
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
                mkdir_ex(dir, 0664, True);
            } catch (hash<ExceptionInfo> ex) {
                string err = sprintf("%s: %s", ex.err, ex.desc);
                if (throw_exception) {
                    rethrow "PROVIDER-INDEX-ERROR", err;
                }
                log(LoggerLevel::LevelWarn, "%s", err);
                return;
            }
        } else if (info.type != "DIRECTORY") {
            string err = sprintf("Environment variable %y is pointing to a %y; must be a directory",
                ProviderIndexEnvVar);
            if (throw_exception) {
                throw "PROVIDER-INDEX-ERROR", err;
            }
            log(LoggerLevel::LevelWarn, "%s", err);
            return;
        }
        return dir;
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
