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

        #! All apps loaded flag
        static bool apps_loaded;
    }

    #! Returns a list of cached app info
    static *list<hash<auto>> getCachedApps() {
        checkDataIndex();
        return index.appinfomap.values();
    }

    #! Searches the app info cache for applications whose names or descriptions match a regular expression pattern
    /** @param regex_pattern A regular expression pattern to match in the \c name and \c desc fields
        @param regex_options Regular expression options; a case-insensitive match is performed by default

        @return All cached applications that match the regular expression pattern in the \c name or \c desc fields
    */
    static *list<hash<auto>> searchCachedAppRegex(string regex_pattern, int regex_options = RE_Caseless) {
        checkDataIndex();
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
        checkDataIndex();
        if (ignore_case) {
            substr = substr.upr();

            return map $1, index.appinfomap.values(),
                ($1.name.upr().find(substr) >= 0) || ($1.desc.upr().find(substr) >= 0);
        }
        return map $1, index.appinfomap.values(), ($1.name.find(substr) >= 0) || ($1.desc.find(substr) >= 0);
    }

    #! Try to return any cached app info for the given app
    static *hash<auto> tryGetAppInfo(string app) {
        checkDataIndex();
        return index.appinfomap{app};
    }

    #! Try to return any cached approximate app action list for the given app
    static *list<hash<auto>> tryGetApproximateActionsForApp(string app) {
        checkDataIndex();
        return index.actioninfomap{app}.values();
    }

    #! Try to return any cached approximate app action for the given app and action
    static *hash<auto> tryGetApproximateAppAction(string app, string action) {
        checkDataIndex();
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
        checkDataIndex();
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
        checkDataIndex();
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
        checkDataIndex();
        return map $1, index.top_actioninfomap.values(),
            regex($1.short_desc, regex_pattern, regex_options) || regex($1.desc, regex_pattern, regex_options);
    }

    #! Searches for cached actions whose descriptions match a substring
    /** @param substr A substring to match in the \c short_desc and \c desc fields
        @param ignore_case Ignore case when searching

        @return All cached actions that match the substring argument in the \c short_desc or \c desc fields
    */
    static *list<hash<auto>> searchCachedActionSubstr(string substr, bool ignore_case = True) {
        checkDataIndex();
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
        checkDataIndex();
        return index.top_actioninfomap.values();
    }

    #! Adds the given app and actions to the cache
    static addAppToCache(hash<auto> app) {
        checkDataIndex();
        if (!app.name) {
            throw "PROVIDER-INDEX-ERROR", sprintf("Cannot add app info to cache: app name is missing (%y)", app);
        }
        index.appinfomap{app.name} = app;
    }

    #! Adds the given app and actions to the cache
    static addActionToCache(hash<auto> action) {
        checkDataIndex();
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
        try {
            load_module(mod);
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
        m.lock();
        on_exit m.unlock();

        # check again in the lock
        if (apps_loaded) {
            return;
        }

        if (!initialized) {
            index = readDataProviderIndex();
            initialized = True;
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
        m.lock();
        on_exit m.unlock();

        # check again in the lock
        if (initialized) {
            return;
        }

        index = readDataProviderIndex();
        initialized = True;
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

        *hash<DataProviderIndexInfo> rv;
        try {
            rv = cast<*hash<DataProviderIndexInfo>>(readIndex("data"));
            if (rv.appinfomap) {
                rv.appinfomap = map {
                    $1.key: app_info_th.doCast($1.value),
                }, rv.appinfomap.pairIterator();
            }
            if (rv.actioninfomap) {
                rv.actioninfomap = deserializeActions(rv.actioninfomap, action_info_th, option_info_th, path_vars_th,
                    dp_info_th, dp_opt_info_th, dp_exp_info_th, mapper_key_info_th, dp_msg_info_th, dp_sig_info_th,
                    dp_arg_info_th, type_map);
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
            TypedHash dp_msg_info_th, TypedHash dp_sig_info_th, TypedHash dp_arg_info_th, hash<auto> type_map) {
        hash<auto> rv;
        foreach hash<auto> i in (actioninfomap.pairIterator()) {
            foreach hash<auto> j in (i.value.pairIterator()) {
                on_error rethrow $1.err, sprintf("%s (app %y action %y)", $1.desc, i.key, j.key);
                rv{i.key}{j.key} = action_info_th.doCast(j.value + {
                    "path_vars": (j.value.path_vars
                        ? map {$1.key: path_vars_th.doCast($1.value)}, j.value.path_vars.pairIterator(),
                        : NOTHING),
                    "options": (map {
                        $1.key: option_info_th.doCast($1.value + {
                            "allowed_values": (map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values),
                            "element_allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                                $1.value.element_allowed_values),
                            "type": type_map{$1.value.type} ?? type_map."auto",
                        })
                    }, j.value.options.pairIterator()),
                    "data_provider_info": deserializeDataProviderInfo(j.value.data_provider_info, dp_info_th,
                        dp_opt_info_th, dp_exp_info_th, mapper_key_info_th, dp_msg_info_th,
                        dp_sig_info_th, dp_arg_info_th, type_map),
                });
            }
        }
        return rv;
    }

    #! Deserialize data provider info
    static private *hash<auto> deserializeDataProviderInfo(*hash<auto> info, TypedHash dp_info_th,
            TypedHash dp_opt_info_th, TypedHash dp_exp_info_th, TypedHash mapper_key_info_th,
            TypedHash dp_msg_info_th, TypedHash dp_sig_info_th, TypedHash dp_arg_info_th, hash<auto> type_map) {
        if (!info) {
            return;
        }
        foreach string key in ("constructor", "create", "upsert", "search", "request", "child_create", "child_delete",
            "add_field", "update_field", "delete_field", "send_message") {
            key += "_options";
            if (info{key}) {
                info{key} = map {
                    $1.key: dp_opt_info_th.doCast($1.value + {
                        "allowed_values": (map cast<hash<AllowedValueInfo>>($1), $1.value.allowed_values),
                        "element_allowed_values": (map cast<hash<AllowedValueInfo>>($1),
                            $1.value.element_allowed_values),
                        "type": (map type_map{$1} ?? type_map."auto", $1.value.type),
                    }),
                }, info{key}.pairIterator();
            }
        }
        return dp_info_th.doCast(info + {
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
            }, info.expressions.pairIterator()),
            "events": (map {
                $1.key: dp_msg_info_th.doCast($1.value)
            }, info.events.pairIterator()),
            "mapper_keys": (map {
                $1.key: mapper_key_info_th.doCast($1.value)
            }, info.mapper_keys.pairIterator()),
            "messages": (map {
                $1.key: dp_msg_info_th.doCast($1.value)
            }, info.messages.pairIterator()),
        });
    }

    #! Writes the index to the configured directory
    static private hash<ProviderIndexInfo> writeIndex(*hash<auto> h, string type, date delta, hash<auto> summary) {
%ifdef NoYaml
        throw "PROVIDER-INDEX-ERROR", sprintf("YAML support is not available; cannot write %s provider index", type);
%else
        string path = getIndexPath(type);
        File f();
        f.open2(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        string yaml = make_yaml(h, BlockStyle);
        f.write(yaml);
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

    #! Log a message to the global logger if possible
    static logArgs(Qore::Logger::LoggerLevel level, string fmt, *softlist<auto> args) {
        if (logger) {
            logger.logArgs(level, fmt, args);
        }
    }
}
}
