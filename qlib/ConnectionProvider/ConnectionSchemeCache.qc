# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectionSchemeCache.qc module for data access and introspection

/** ConnectionSchemeCache.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

%requires reflection

#! Contains all public definitions in the ConnectionProvider module
public namespace ConnectionProvider {
#! Connection provider information
public hashdecl ConnectionSchemeInfo inherits DataProvider::DisplayDescInfo {
    #! Connection scheme set; scheme -> True
    /** The first scheme is the "default" scheme for display
    */
    hash<string, bool> schemes;

    #! Base URL scheme map; base scheme -> connection scheme
    hash<string, string> base_scheme_map;

    #! The connection class
    Class cls;

    #! Connection options: name -> option info
    *hash<string, hash<ConnectionOptionInfo>> options;

    #! Set of sensitive options: name -> True
    *hash<string, bool> sensitive_options;

    #! URL provided automatically?
    bool auto_url = False;

    #! Required option string
    /** format is required option names separated by commas (\c ","); if there are multiple possibilities, they should
        be separated by pipe chars (\c "|""); ex: client_id,client_secret,tenant|token

        All strings must be valid option names in \a options
    */
    *string required_options;

    #! Free-form metadata for the connection
    *hash<auto> metadata;

    #! Messages to be displayed when creating a connection
    *list<hash<UiMessageInfo>> messages;

    #! Any icon info for the connection
    *string icon;

    #! Any image info for the connection
    *string image;
}

#! Connection provider type cache class
public class ConnectionSchemeCache {
    public {
        #! Map of known connection schemes to modules
        const SchemeMap = {
            "awsrests": "AwsRestClient",

            "bigcommerceres": "BigCommerceRestClient",

            "billwerkrest": "BillwerkRestClient",
            "billwerkrests": "BillwerkRestClient",

            # provided by the jni module
            "bltcp": "BusyLightDataProvider",
            "blssl": "BusyLightDataProvider",

            "bcrests": "CdsRestClient",
            "cdsrests": "CdsRestClient",
            "dynamics": "CdsRestClient",

            "chargebee": "ChargebeeRestClient",

            "cin7core": "Cin7CoreRestClient",

            "db": "DbDataProvider",

            "deel": "DeelRestClient",

            "discord": "DiscordRestClient",
            "discordws": "DiscordWebSocketClient",
            "discordwss": "DiscordWebSocketClient",

            "ds": "DataStreamClient",
            "dss": "DataStreamClient",

            "ebrests": "EmpathicBuildingDataProvider",

            "esrest": "ElasticSearchDataProvider",
            "esrests": "ElasticSearchDataProvider",

            "freshbooks": "FreshBooksRestClient",

            "gohighlevelres": "GoHighLevelRestClient",

            "qdrantrest": "QdrantDataProvider",
            "qdrantrests": "QdrantDataProvider",

            "gcal": "GoogleRestClient",
            "gmail": "GoogleRestClient",
            "grests": "GoogleRestClient",

            "hue": "HueRestClient",

            "jotform": "JotformRestClient",

            # provided by the json module
            "jsonrpc": "JsonRpcClient",
            "jsonrpcs": "JsonRpcClient",
            "mcp": "McpClient",
            "mcps": "McpClient",

            # provided by the jni module
            "kafka": "KafkaDataProvider",

            "linear": "LinearRestClient",

            "mailgun": "MailgunRestClient",

            "mewsrest": "MewsRestClient",

            "memcached": "MemcachedDataProvider",
            "memcacheds": "MemcachedDataProvider",
            "mongodb": "MongoDbDataProvider",

            # provided by the jni module
            "mqtttcp": "MqttDataProvider",
            "mqttssl": "MqttDataProvider",

            "netsuite": "NetSuiteRestClient",

            "openai": "OpenAiRestClient",

            "pdfco": "PdfCoRestClient",

            "pop3": "Pop3Client",
            "pop3s": "Pop3Client",

            "redis": "RedisDataProvider",
            "rediss": "RedisDataProvider",

            "rest": "RestClient",
            "rests": "RestClient",

            "sewiorest": "SewioRestClient",
            "sewiorests": "SewioRestClient",

            "sfrests": "SalesforceRestClient",

            # provided by the xml module
            "sfsoap": "SalesforceSoapClient",
            "sfsoaps": "SalesforceSoapClient",

            "sap4hanarests": "Sap4HanaRestClient",

            "snrests": "ServiceNowRestClient",

            "sewiows": "SewioWebSocketClient",
            "sewiowss": "SewioWebSocketClient",

            # provided by the xml module
            "soap": "SoapClient",
            "soaps": "SoapClient",

            "sftp": "Ssh2Connections",

            # provided by the ssh2 module
            "ssh": "Ssh2Connections",

            "smtp": "SmtpClient",
            "smtps": "SmtpClient",
            "smtptls": "SmtpClient",
            "esmtp": "SmtpClient",
            "esmtps": "SmtpClient",
            "esmtptls": "SmtpClient",

            "shippo": "ShippoRestClient",

            "shipstation": "ShipStationRestClient",

            "squareres": "SquareRestClient",
            "squarespaceres": "SquarespaceRestClient",

            "tableaures": "TableauRestClient",

            "sse": "ServerSentEventClient",
            "sses": "ServerSentEventClient",

            "telnet": "TelnetClient",

            "ws": "WebSocketClient",
            "wss": "WebSocketClient",

            # provided by the xml module
            "xmlrpc": "XmlRpcClient",
            "xmlrpcs": "XmlRpcClient",

            # provided by the yaml module
            "yamlrpc": "YamlRpcClient",
            "yamlrpcs": "YamlRpcClient",

            "zeyosrest": "ZeyosRestClient",
            "zeyosrests": "ZeyosRestClient",

            "unleashed": "UnleashedRestClient",

            "wave": "WaveRestClient",

            "zohobooks": "ZohoBooksRestClient",
            "zohoinventory": "ZohoInventoryRestClient",
            "zohoinvoice": "ZohoInvoiceRestClient",
        };

        #! Type alias map
        const TypeMap = {
            "int": Type::Int,
            "boolean": Type::Boolean,
            "file-as-string": Type::String,
        };
    }

    private {
        #! Connection provider cache
        static hash<string, hash<ConnectionSchemeInfo>> cache = {
            "ftp": FtpConnection::ConnectionScheme,
            "ftps": FtpConnection::ConnectionScheme,

            "dir": FilesystemConnection::ConnectionScheme,
            "file": FilesystemConnection::ConnectionScheme,

            "http": HttpConnection::ConnectionScheme,
            "https": HttpConnection::ConnectionScheme,
        };

        #! Pending connection provider init handler map: scheme -> init handler
        static hash<string, code> initmap;

        #! Connection provider cache lock
        static Mutex lck();

        #! Load all flag
        static bool load_all;
    }

    #! Registers a scheme for deferred initialization / initialization on demand
    /** @param scheme the scheme to register
        @param init the initialization code that will register the scheme
    */
    static registerPendingScheme(string scheme, code init) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        if (cache{scheme}) {
            throw "SCHEMA-ERROR", sprintf("Scheme %y is already registered", scheme);
        }
        if (initmap{scheme}) {
            throw "SCHEMA-ERROR", sprintf("Scheme %y is already pending", scheme);
        }

        initmap{scheme} = init;
    }

    #! Register a new connection provider in the cache
    /** @param scheme the scheme for the connection provider
        @param entry the new connection provider entry

        @return True if the connection was reigstered, False if there is already a registered connection with this
        scheme
    */
    static bool registerScheme(string scheme, hash<ConnectionSchemeInfo> entry) {
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        if (cache{scheme}) {
            return False;
        }
        remove initmap{scheme};

        # set sensitive option map
        entry.sensitive_options = map {$1.key: True}, entry.options.pairIterator(), $1.value.sensitive;

        cache{scheme} = entry;
        # sort connection options when caching
        cache{scheme}.options = cache{scheme}.options{sort(keys cache{scheme}.options)};

        # deregister the scheme automatically if module initialization fails
        ImplicitModuleTransaction::add(sub () { ConnectionSchemeCache::deregisterSchemeIntern(scheme); });

        return True;
    }

    #! Removes a scheme
    /** @param scheme the scheme name

        @return the scheme definition, if it was deregistered

        The caller must not be restricted with @ref Qore::PO_NO_PROCESS_CONTROL

        @throw PERMISSION-DENIED Caller is restricted with @ref Qore::PO_NO_PROCESS_CONTROL
    */
    static *hash<ConnectionSchemeInfo> deregisterScheme(string scheme) {
        if (Program::getCallerCapabilityMask() & PO_NO_PROCESS_CONTROL) {
            throw "PERMISSION-DENIED", sprintf("Cannot remove scheme %y from a context with restricted process control",
                scheme);
        }

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }

        remove initmap{scheme};

        return remove cache{scheme};
    }

    #! Loads all possible schemes and returns the entire cache
    static hash<string, hash<ConnectionSchemeInfo>> getLoadAll() {
        if (load_all) {
            return cache;
        }

        # Take a snapshot of what needs loading while holding the lock, then release the lock
        # before calling load_module() to avoid an ABBA deadlock with registerScheme().
        # See getSchemeImpl() for a detailed explanation of the lock ordering issue.
        *list<code> init_list;
        {
            bool needs_lock = !lck.lockOwner();
            if (needs_lock) {
                lck.lock();
            }
            on_exit if (needs_lock) {
                lck.unlock();
            }
            # check again in the lock
            if (load_all) {
                return cache;
            }
            # snapshot and clear pending init closures atomically under the lock
            init_list = initmap.values();
            initmap = {};
        }

        # Load all pending modules without holding the lock.
        # getSchemeImpl() acquires lck itself (needs_lock = True) when the caller does not hold it.
        # The init closures also register via registerScheme() which acquires lck.
        map $1(), init_list;
        map ConnectionSchemeCache::getSchemeImpl($1, False), keys SchemeMap;

        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }
        load_all = True;
        return cache;
    }

    #! Returns the entire cache
    static hash<string, hash<ConnectionSchemeInfo>> get() {
        return cache;
    }

    #! Returns the number of types in the cache
    static int size() {
        return cache.size();
    }

    #! Returns True if the cache is empty, False if not
    static bool empty() {
        return cache.empty();
    }

    #! Returns a list of all cached schemes
    /**
    */
    static *list<string> listSchemes() {
        return keys cache;
    }

    #! Returns info for the given connection scheme or @ref nothing if not present
    /** @param scheme the scheme for the connection

        @return info for the given connection scheme; @ref NOTHING is returned if the scheme is not
        registered
    */
    static *hash<ConnectionSchemeInfo> getScheme(string scheme) {
        return getSchemeImpl(scheme, False);
    }

    #! Returns info for the given connection scheme or throws an exception if not present
    /** @param scheme the scheme for the connection provider

        @return info for the given connection scheme; an exception is thrown if the scheme is unknown

        @throw SCHEME-ERROR the scheme is unknown
    */
    static hash<ConnectionSchemeInfo> getSchemeEx(string scheme) {
        return getSchemeImpl(scheme, True);
    }

    #! Returns info for the given connection scheme; either throws an exception if not present or returns @ref NOTHING
    /** @param scheme the scheme for the connection provider
        @param throw_exception if @ref True then an exception is thrown if the scheme cannot be resolved, otherwise
        @ref NOTHING is returned
        @param scheme_loader a closure taking a string scheme argument and returning
        <tt>*hash&lt;ConnectionSchemeInfo&gt;</tt>

        @return info for the given connection scheme or @ref NOTHING is returned if the scheme cannot be
        resolved and the \a throw_exception argument is @ref False

        @throw SCHEME-ERROR the scheme is unknown; only thrown if the \a throw_exception argument is @ref True
    */
    static *hash<ConnectionSchemeInfo> getSchemeImpl(string scheme, bool throw_exception, *code scheme_loader) {
        # we have to grab the write lock if there is a scheme_loader
        bool needs_lock = !lck.lockOwner();
        if (needs_lock) {
            lck.lock();
        }
        on_exit if (needs_lock) {
            lck.unlock();
        }
        if (*code init = remove initmap{scheme}) {
            init();
        }

        *hash<ConnectionSchemeInfo> rv = cache{scheme};
        if (rv) {
            return rv;
        }

        # Release the lock before calling load_module() to avoid an ABBA deadlock:
        #   Thread A: holds module_lock (loading a module whose init calls registerScheme())
        #             → waiting for lck (in registerScheme())
        #   Thread B: holds lck (here in getSchemeImpl())
        #             → waiting for module_lock (in checkSchemeModule()/tryLoadModule())
        # By releasing lck before calling load_module(), Thread B no longer holds lck
        # while waiting for the module lock, breaking the cycle.
        # SchemeMap is a const — safe to read without the lock.
        *string mod = SchemeMap{scheme};
        if (needs_lock) {
            lck.unlock();
        }
        bool loaded = ProviderIndexUtil::checkSchemeModule(scheme);
        if (!loaded && mod) {
            loaded = ConnectionSchemeCache::tryLoadModule(mod, throw_exception);
        }
        if (needs_lock) {
            lck.lock();
        }
        if (loaded && (rv = cache{scheme})) {
            return rv;
        }
        if (scheme_loader && (rv = cache{scheme} = scheme_loader(scheme))) {
            return rv;
        }
        if (throw_exception) {
            throw "SCHEME-ERROR", sprintf("Connection provider scheme %y is not registered", scheme);
        }
    }

    #! Returns default option values for the given connection scheme
    /** @param scheme The connection scheme name

        @return A hash of default options

        @throw SCHEME-ERROR unknown / unsupported scheme

        @since ConnectionProvider 2.0
    */
    static *hash<auto> getDefaultOptionValues(string scheme) {
        return getDefaultOptionValues(ConnectionSchemeCache::getSchemeEx(scheme));
    }

    #! Returns default option values for the given connection scheme
    /** @param scheme_info The connection scheme

        @return A hash of default options

        @since ConnectionProvider 2.0
    */
    static *hash<auto> getDefaultOptionValues(hash<ConnectionSchemeInfo> scheme_info) {
        return map {$1.key: $1.value.default_value}, scheme_info.options.pairIterator(),
            exists $1.value.default_value;
    }

    #! Checks and returns options
    /** @param conn_name The connection name, for error reporting only
        @param scheme The connection scheme name
        @param options The options to validate
        @param check_required Check required options?

        @return Options after validation

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option

        @since ConnectionProvider 2.0
    */
    static hash<auto> validateConnectionOptions(string conn_name, string scheme, hash<auto> options,
            bool check_required = True) {
        return ConnectionSchemeCache::validateConnectionOptions(conn_name, ConnectionSchemeCache::getSchemeEx(scheme),
            options, check_required);
    }

    #! Checks and returns options
    /** @param conn_name The connection name, for error reporting only
        @param scheme_info The connection scheme
        @param options The options to validate
        @param check_required Check required options?

        @return Options after validation

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option

        @since ConnectionProvider 2.0
    */
    static hash<auto> validateConnectionOptions(string conn_name, hash<ConnectionSchemeInfo> scheme_info,
            hash<auto> options, bool check_required = True) {
        hash<auto> orig_options = options;
        options = ConnectionSchemeCache::getDefaultOptionValues(scheme_info) + options;
        if (options) {
            # check for unknown options
            *hash<auto> unknown_opts = options - (keys scheme_info.options);
            if (unknown_opts) {
                throw "CONNECTION-OPTION-ERROR", sprintf("Connection %y (type %y) has unknown option%s %y (known "
                    "options: %y)", conn_name, scheme_info.display_name, unknown_opts.size() == 1 ? "" : "s",
                    unknown_opts, keys scheme_info.options);
            }
            foreach hash<auto> i in (options.pairIterator()) {
                hash<ConnectionOptionInfo> info = scheme_info.options{i.key};
                if (!exists i.value) {
                    # if an option has no value, then we assume that it should be set to no value and delete it
                    # not including the option key will allow it to be set with the default value
                    delete options{i.key};
                    continue;
                }
                string type = info.type;
                if (*string nt = TypeMap{type}) {
                    type = nt;
                }
                string vtype = i.value.type();
                if (type != "any" && type != "auto" && vtype != type) {
                    if (type == Type::Int && i.value.intp()) {
                        options{i.key} = i.value.toInt();
                        continue;
                    }
                    if (type == "bool" && vtype == "string") {
                        options{i.key} = parse_boolean(i.value);
                        continue;
                    }
                    if (type == "date" && vtype == "string") {
                        options{i.key} = date(i.value);
                        continue;
                    }
                    if (info.ignore_types{vtype}) {
                        delete options{i.key};
                        continue;
                    }
                    throw "CONNECTION-OPTION-ERROR",
                        sprintf("Connection %y (type %y) option %y was passed with type %s (%y); expecting %y",
                            conn_name, scheme_info.display_name, i.key, i.value.fullType(), i.value, type);
                }
                if (info.subst_env_vars) {
                    options{i.key} = substitute_env_vars(i.value);
                }
                # verify against allowed values
                if (info.allowed_values && !info.allowed_values_creatable) {
                    bool ok;
                    foreach hash<AllowedValueInfo> v in (info.allowed_values) {
                        if (v.value == i.value) {
                            ok = True;
                            break;
                        }
                    }
                    if (!ok) {
                        throw "CONNECTION-OPTION-ERROR",
                            sprintf("Connection %y (type %y) option %y was passed with value %y, but allowed "
                                "values are: %y", conn_name, scheme_info.display_name, i.key, i.value,
                                (map $1.value, info.allowed_values));
                    }
                }
                # verify against element allowed values
                if (info.element_allowed_values && !info.element_allowed_values_creatable) {
                    foreach auto val in (i.value) {
                        bool ok;
                        foreach hash<AllowedValueInfo> v in (info.element_allowed_values) {
                            if (v == val) {
                                ok = True;
                                break;
                            }
                        }
                        if (!ok) {
                            throw "CONNECTION-OPTION-ERROR",
                                sprintf("Connection %y (type %y) option %y was passed with element value %y, but "
                                    "allowed element values are: %y", conn_name, scheme_info.display_name, i.key, val,
                                    (map $1.value, info.element_allowed_values));
                        }
                    }
                }
            }
        }
        # check for required options
        if (check_required && scheme_info.required_options) {
            list<string> req_list = scheme_info.required_options.split("|");
            bool ok;
            list<string> missing;
            foreach string req in (req_list) {
                list<string> req_opt = req.split(",");
                # ignore "url" if present, as this is not an option
                if (req =~ /\burl\b/) {
                    req_opt = map $1, req_opt, $1 != "url";
                    if (!req_opt) {
                        continue;
                    }
                }
                # check which options are missing
                *hash<auto> have = options{req_opt};
                if (have.size() == req_opt.size()) {
                    ok = True;
                    break;
                }
                if (req_list.size() == 1) {
                    missing = keys ((map {$1: True}, req_opt) - keys have);
                }
            }
            if (!ok) {
                if (missing) {
                    throw "CONNECTION-OPTION-ERROR", sprintf("connection %y (type %y) missing required options %y; "
                        "options provided: %y", conn_name, scheme_info.display_name, missing, orig_options);
                } else {
                    throw "CONNECTION-OPTION-ERROR", sprintf("connection %y (type %y) missing required options %s; "
                        "options provided: %y", conn_name, scheme_info.display_name,
                        (foldl $1 + " OR " + $2, (map sprintf("%y", $1), req_list)), orig_options);
                }
            }
        }
        return options;
    }

    #! Try to load the given module
    private static bool tryLoadModule(string mod, bool throw_exception) {
        try {
            load_module(mod);
            return True;
        } catch (hash<ExceptionInfo> ex) {
            if (throw_exception) {
                rethrow;
            }
        }
        return False;
    }

    #! Deregisters a scheme in case module intiialization fails for the module that registered it
    /** @param scheme the scheme for the connection provider
    */
    private:internal static deregisterSchemeIntern(string scheme) {
        lck.lock();
        on_exit lck.unlock();

        remove cache{scheme};
    }
}
}
