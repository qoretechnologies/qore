# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProvider.qc module for data access and introspection

/** DataProvider.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
%requires Util

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
/** @defgroup DataProviderGlobalOptions Data Provider Global Options
    These are constants for global data provider behavior

    @see
    - DataProvider::addOptions()
    - DataProvider::getOptions()
    - DataProvider::setOptions()
*/
#/@{
#! Enable servers
/** If this is set, then external server threads or programs may be started for data provider functionality; in this
    case the user must call @ref DataProvider::shutdown() before exiting
*/
public const DPO_EnableServers                  = (1 << 0);

#! Disable on-demand initialization
/** This option ensures that all eligible data providers are initialized immediately instead of when they are used;
    this can be useful for testing
*/
public const DPO_DisableOnDemandInitialization  = (1 << 1);

#! Enable on-demand initialization of actions
/** This option ensures that data providers are initialized immediately but action initialization is deferred
*/
public const DPO_EnableOnDemandActions          = (1 << 2);

#! The mask of all data provider options
public const DPO_AllOptions = (
    DPO_EnableServers
    | DPO_DisableOnDemandInitialization
    | DPO_EnableOnDemandActions
);
#/@}

#! Data provider class
public class DataProvider {
    public {
        #! Map of known data provider factory names to modules
        const FactoryMap = {
            "asyncapi": "AsyncApiDataProvider",
            "awsrestclient": "AwsRestClientDataProvider",

            "bcrest": "CdsRestDataProvider",

            # provided by the jni module
            "busylight": "BusyLightDataProvider",

            "cdsrest": "CdsRestDataProvider",
            "cin7core": "Cin7CoreDataProvider",
            "csvread": "CsvUtil",
            "csvwrite": "CsvUtil",
            "db": "DbDataProvider",
            "discord": "DiscordDataProvider",
            "discordws": "DiscordDataProvider",
            "elasticsearch": "ElasticSearchDataProvider",
            "empathicbuilding": "EmpathicBuildingDataProvider",

            # provided by the jni module
            "excelread": "ExcelDataProvider",

            "file": "FileDataProvider",
            "filepoller": "FilePoller",
            "fixedlengthread": "FixedLengthUtil",
            "fixedlengthwrite": "FixedLengthUtil",
            "ftpclient": "FtpClientDataProvider",
            "ftppoller": "FtpPoller",
            "gcal": "GoogleCalendarDataProvider",
            "generator": "GeneratorDataProvider",
            "gmail": "GmailDataProvider",
            "httpclient": "HttpClientDataProvider",
            "jotform": "JotformDataProvider",

            # provided by the jni module
            "kafka": "KafkaDataProvider",

            "linear": "LinearDataProvider",

            # provided by the json module
            "mcpclient": "McpClientDataProvider",

            "mailgun": "MailgunDataProvider",
            "mewsrest": "MewsRestDataProvider",
            "memcached": "MemcachedDataProvider",
            "mongodb": "MongoDbDataProvider",

            # provided by the jni module
            "mqtt": "MqttDataProvider",

            "openai": "OpenAiDataProvider",
            "pop3": "Pop3ClientDataProvider",
            "redis": "RedisDataProvider",
            "restclient": "RestClientDataProvider",
            "restschema": "RestSchemaDataProvider",
            "salesforcerest": "SalesforceRestDataProvider",
            "sax": "SaxDataProvider",
            "servicenowrest": "ServiceNowRestDataProvider",
            "shippo": "ShippoDataProvider",
            "shipstation": "ShipStationDataProvider",

            # provided by the ssh2 module
            "sftpclient": "SftpClientDataProvider",
            "sftppoller": "SftpPoller",

            "smtpclient": "SmtpClient",

            # provided by the xml module
            "soap": "SoapDataProvider",

            "sseclient": "ServerSentEventClient",

            "swagger": "SwaggerDataProvider",

            # provided by the tar module
            "tar": "TarDataProvider",

            # provided by the xml module
            "webdavclient": "WebDavClientDataProvider",

            # provided by the ts-toolkit repo
            "ts-actions": "TypeScriptActionInterface",

            "wsclient": "WebSocketClient",

            # provided by the zip module
            "zip": "ZipDataProvider",

            "unleashed": "UnleashedDataProvider",

            "zohobooks": "ZohoBooksDataProvider",
            "zohoinvoice": "ZohoInvoiceDataProvider",
        };

        #! Map of known type path prefixes to modules
        const TypeMap = {
            "qore/ftp": "FtpPollerUtil",
            "qore/sftp": "SftpPollerUtil",
            "qore/fsevents": "FsEventPollerUtil",
        };

        #! List of data provider factory modules
        const FactoryModuleList = keys (map {$1: True}, FactoryMap.iterator());
        # "map" is used above to ensure that the values only appear once in the final list

        #! An invalid app name
        const NoApp = "$NO_APP";
    }

    private {
        #! data provider factory cache
        static hash<string, AbstractDataProviderFactory> factory_cache;

        #! data provider factory module map
        static hash<string, string> factory_module_map;

        #! data provider factory cache lock
        static Gate factory_cache_lock();

        #! data provider data type cache
        static DataProviderTypeCache type_cache();

        #! data provider type module map
        static hash<string, string> type_module_map;

        #! flag to allow automatic configuration from environment variables
        /** must be set before any requests for types or factories are made
        */
        static bool allow_env_config = False;

        #! flag that locks the \a allow_env_config var
        static bool env_config_locked = False;

        #! set of module already loaded
        static hash<string, bool> mod_map;

        #! Static initializer list
        static list<code> ilist;

        #! Deferred static initializers
        static list<*list<code>> ilist_save;

        #! Shutdown handlers; key -> handler
        static hash<string, code> smap;

        #! Global options
        static int options = 0;

        #! Global logger
        static *LoggerInterface logger;
    }

    #! Sets or clears the global logger
    /** @param logger the global data provider logger

        @since DataProvider 3.0
    */
    static setGlobalLogger(*LoggerInterface logger) {
        DataProvider::logger = logger;
    }

    #! Returns the global data provider logger
    /** @return the global data provider logger

        @since DataProvider 3.0
    */
    static *LoggerInterface getGlobalLogger() {
        return logger;
    }

    #! Adds the given global data provider options
    /** @param options a bitfield of global data provider options (DPO_*)

        @throw INVALID-OPTIONS Invalid options provided

        @since DataProvider 3.0
    */
    static addOptions(int options) {
        DataProvider::checkOptions(options);
        DataProvider::options |= options;
    }

    #! Removes the given global data provider options
    /** @param options a bitfield of global data provider options (DPO_*)

        @throw INVALID-OPTIONS Invalid options provided

        @since DataProvider 3.0
    */
    static removeOptions(int options) {
        DataProvider::checkOptions(options);
        DataProvider::options &= ~options;
    }

    #! Returns global data provider options
    /** @return global data provider options

        @since DataProvider 3.0
    */
    static int getOptions() {
        return options;
    }

    #! Replaces data provider global options with the value provided
    /** @param options a bitfield of global data provider options (DPO_*)

        @throw INVALID-OPTIONS Invalid options provided

        @since DataProvider 3.0
    */
    static setOptions(int options) {
        DataProvider::checkOptions(options);
        DataProvider::options = options;
    }

    #! Checks for valid global data provider options
    /** @param options a bitfield of global data provider options (DPO_*)

        @since DataProvider 3.0
    */
    static checkOptions(int options) {
        if (!(options & ~DPO_AllOptions)) {
            return;
        }
        throw "INVALID-OPTIONS", sprintf("Data provider option value %s has invalid bits set (mask: %s)",
            get_bit_string(options), get_bit_string(DPO_AllOptions));
    }

    #! Register a new data provider factory
    /** @param factory the new data provider factory

        @throw PROVIDER-ERROR the given provider has already been registered
    */
    static registerFactory(AbstractDataProviderFactory factory) {
        factory_cache_lock.enter();
        on_exit factory_cache_lock.exit();

        string name = factory.getName();
        if (factory_cache{name}) {
            # check if factory is being re-registered after module injection
            if (!DataProvider::checkInjection(factory, factory_module_map)) {
                throw "PROVIDER-ERROR", sprintf("provider %y has already been registered", name);
            }
        }
        {
            *string module_name = Class::getClass(factory).getModuleName();
            if (exists module_name) {
                factory_module_map{factory.className()} = module_name;
            }
        }
        factory_cache{name} = factory;

        # deregister the app automatically if module initialization fails
        ImplicitModuleTransaction::add(sub () { DataProvider::deregisterFactory(name); });
    }

    #! Returns the given data provider factory or @ref nothing if not present
    static *AbstractDataProviderFactory getFactory(string name) {
        DataProvider::checkRequest();
        if (!factory_cache{name} && !ProviderIndexUtil::checkFactoryModule(name)) {
            *string module_name = FactoryMap{name} ?? DataProvider::tryGetFactoryModuleName(name);
            if (!module_name) {
                return;
            }
            if (!DataProvider::tryLoad(module_name)) {
                return;
            }
        }
        return factory_cache{name};
    }

    #! Returns the given data provider factory or throws an exception if not present
    /** @param name the name of the data provider to retrieve

        @return the given data provider; an exception is thrown if the data provide is unknown

        @throw PROVIDER-ERROR the given provider is unknown
    */
    static AbstractDataProviderFactory getFactoryEx(string name) {
        *AbstractDataProviderFactory factory = DataProvider::getFactory(name);
        if (!factory) {
            throw "PROVIDER-ERROR", sprintf("data provider factory %y is unknown; known data provider factories: %y",
                name, keys factory_cache);
        }
        return factory;
    }

    #! Returns a data provider object from the given factory, created with the given constructor options
    /** @param path the \c "/"-separated path to the target data provider; the first element is the factory name
        @param template_options options with templated values; only the keys in this hash are relevant, as they
        indicate options that will be present at a later time when their values can be derived; the values provided in
        this call are assumed to be in an implementation-dependent templated format and should be ignored
        @param options factory creation options

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown

        @since DataProvider 2.7.1
    */
    static AbstractDataProvider getFactoryObjectWithTemplate(string path, *hash<auto> template_options,
            *hash<auto> options) {
        AbstractDataProvider provider;
        {
            list<string> path_list = path.split("/");
            provider = DataProvider::getFactoryEx(shift path_list).createWithTemplate(template_options, options);
            map provider = provider.getChildProviderEx($1), path_list;
        }
        return provider;
    }

    #! Returns a data provider object from the given factory, created with the given options and example data
    /** @param path the \c "/"-separated path to the target data provider; the first element is the factory name
        @param example example data to process
        @param options factory creation options

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown

        @since DataProvider 2.7.1
    */
    static AbstractDataProvider getFactoryObjectFromExample(string path, data example, *hash<auto> options) {
        AbstractDataProvider provider;
        {
            list<string> path_list = path.split("/");
            provider = DataProvider::getFactoryEx(shift path_list).getProviderFromExample(example, options);
            map provider = provider.getChildProviderEx($1), path_list;
        }
        return provider;
    }

    #! Returns a data provider object from the given factory, created with the given options and example data
    /** @param path the \c "/"-separated path to the target data provider; the first element is the factory name
        @param example example data to process
        @param options factory creation options

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown

        @since DataProvider 2.7.1
    */
    static AbstractDataProvider getFactoryObjectFromExample(string path, InputStream example, *hash<auto> options) {
        AbstractDataProvider provider;
        {
            list<string> path_list = path.split("/");
            provider = DataProvider::getFactoryEx(shift path_list).getProviderFromExample(example, options);
            map provider = provider.getChildProviderEx($1), path_list;
        }
        return provider;
    }

    #! Returns a data provider object from the given factory, created with the given constructor options
    /** @param path the \c "/"-separated path to the target data provider; the first element is the factory name
        @param options factory creation options

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown
    */
    static AbstractDataProvider getFactoryObject(string path, *hash<auto> options) {
        AbstractDataProvider provider;
        {
            list<string> path_list = path.split("/");
            provider = DataProvider::getFactoryEx(shift path_list).create(options);
            map provider = provider.getChildProviderEx($1), path_list;
        }
        return provider;
    }

    #! Returns a data provider object from the given factory string
    /** Factory options are given as string-formatted hash in curly brackets and child data providers are separated by
        forward slashes

        @par Example:
        @code{.py}
DbDataProvider db = DataProvider::getFactoryObjectFromString("db{datasource=oracle:user/password@db}/my_table");
        @endcode

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown
    */
    static AbstractDataProvider getFactoryObjectFromString(string name) {
        hash<FactoryInfo> info = DataProvider::getFactoryInfoFromString(name);
        AbstractDataProvider provider = DataProvider::getFactoryEx(info.name).create(info.options);
        map provider = provider.getChildProviderEx($1), info.path_list;
        return provider;
    }

    #! Returns a data provider object from the given factory string using environment variables to find the factory
    /** Factory options are given as string-formatted hash in curly brackets and child data providers are separated by
        forward slashes

        @par Example:
        @code{.py}
DbDataProvider db = DataProvider::getFactoryObjectFromStringUseEnv("db{datasource=oracle:user/password@db}/my_table");
        @endcode

        @throw PROVIDER-ERROR the given provider factory or child provider is unknown
    */


    static AbstractDataProvider getFactoryObjectFromStringUseEnv(string name) {
        hash<FactoryInfo> info = DataProvider::getFactoryInfoFromString(name);

        AbstractDataProviderFactory factory;
        if (*AbstractDataProviderFactory cached = DataProvider::getFactory(info.name)) {
            factory = cached;
        } else if ((*string factory_name = FactoryMap{info.name}) && DataProvider::tryLoad(factory_name)) {
            factory = DataProvider::getFactoryEx(info.name);
        } else {
            *object v = DataProvider::loadFromEnvironment("get_data_provider_map", "data provider", <SymbolInfo>{
                "type": "factory",
                "name": info.name,
            });
            if (v) {
                factory = cast<AbstractDataProviderFactory>(v);
            }
        }
        if (factory) {
            AbstractDataProvider provider = factory.create(info.options);
            map provider = provider.getChildProvider($1), info.path_list;
            return provider;
        }

        throw "FACTORY-ERROR", sprintf("data provider factory %y in %y is unknown", info.name, name);
    }

    #! Returns a hash of factory information from a string
    private static hash<FactoryInfo> getFactoryInfoFromString(string name) {
        # get options, respecting balanced curly brackets
        *string opts = (name =~ x/{((?:(?>[^{}]*)|(?0))*)}/)[0];
        # remove empty options; the above regex does not catch them :(
        if (!opts) {
            name =~ s/{}//;
        }
        auto options;
        if (opts) {
            name = replace(name, "{" + opts + "}", "");
            options = parse_to_qore_value(opts);
            if (exists options && options.typeCode() != NT_HASH) {
                throw "PROVIDER-ERROR", sprintf("option string in %y does not parse to type \"hash\"; got "
                    "type %y instead", name, options.type());
            }
        }
        list<string> path_list = name.split("/");
        return <FactoryInfo>{
            "name": shift path_list,
            "path_list": path_list,
            "options": options,
        };
    }

    #! Returns a list of registered data provider factories
    static *list<string> listFactories() {
        return keys factory_cache;
    }

    #! Returns the data provider cache
    static DataProviderTypeCache getTypeCache() {
        return type_cache;
    }

    #! Register a new data provider type
    /** @param path a \c "/" separated path for the type; this is the lookup index
        @param type the new data provider type

        @throw PROVIDER-ERROR the given provider has already been registered

        @note types are stored based on the path provided which may differ from their type names (which may be generic
        and therefore not suitable for indexing)
    */
    static registerType(string path, AbstractDataProviderType type) {
        try {
            type_cache.registerType(path, type, NOTHING, True);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "TYPE-ERROR") {
                # check if type is being re-registered after module injection
                if (!DataProvider::checkInjection(type, type_module_map)) {
                    throw "PROVIDER-ERROR", sprintf("provider type %y has already been registered", path);
                }
            } else {
                rethrow;
            }
        }

        {
            *string module_name = Class::getClass(type).getModuleName();
            if (exists module_name) {
                type_module_map{type.className()} = module_name;
            }
        }
    }

    #! Returns the given data provider type or @ref nothing if not present
    /** @param path a \c "/" separated path for the type; trailing path elements can be a path into fields

        @return the given data provider type; @ref NOTHING is returned if the type cannot be resolved

        @note types are stored based on the path provided which may differ from their type names (which may be generic
        and therefore not suitable for indexing)
    */
    static *AbstractDataProviderType getType(string path) {
        DataProvider::checkRequest();
        return type_cache.getTypeImpl(path, False, \DataProvider::tryLoadTypeFromPath());
    }

    #! Returns the given data provider type or throws an exception if not present
    /** @param path a \c "/" separated path for the type; trailing path elements can be a path into fields

        @return the given data provider type; an exception is thrown if the data provide is unknown

        @throw PROVIDER-ERROR the given data provider type path is unknown

        @note types are stored based on the path provided which may differ from their type names (which may be generic
        and therefore not suitable for indexing)
    */
    static AbstractDataProviderType getTypeEx(string path) {
        DataProvider::checkRequest();
        return type_cache.getTypeImpl(path, True, \DataProvider::tryLoadTypeFromPath());
    }

    #! Returns the root type entry
    static DataProviderTypeEntry getTypeRoot() {
        return type_cache.getRoot();
    }

    #! Locks all types
    static lockAllTypes() {
        type_cache.lockAll();
    }

    #! Returns a list of registered data provider type paths
    /**
        @note types are stored based on the path provided which may differ from their type names (which may be generic
        and therefore not suitable for indexing)
    */
    static *list<string> listTypes() {
        return type_cache.listTypes();
    }

    #! Registers all known data provider factories
    static registerKnownFactories() {
        # load known modules
        map DataProvider::tryLoad($1, True), FactoryModuleList;
    }

    #! Returns a descriptive string for the given expression
    /** @param exp the expression to describe
        @param emap the expression map to use; by default, @ref DataProviderGenericExpressions is used

        @return a descriptive string for the given expression

        @throw INVALID-EXPRESSION an expression was not found in the expression map

        @since DataProvider 3.3
    */
    static string renderExpression(hash<DataProviderExpression> exp,
            *hash<string, hash<DataProviderExpressionInfo>> emap = DataProviderGenericExpressions) {
        string str;
        *hash<DataProviderExpressionInfo> info = emap{exp.exp};
        if (!info) {
            throw "INVALID-EXPRESSION", sprintf("Expression %y not found in expression map", exp.exp);
        }
        if (info.type == DET_Function) {
            str = sprintf("%s(%s)", info.name, (foldl $1 + ", " + $2, (map renderExpressionArg($1, emap,
                info.arg_info[$1]), exp.args)));
        } else if (info.render_template) {
            str = renderExpressionTemplate(info, exp, emap);
        } else {
            if (exp.args.size() == 1) {
                str = sprintf("%s %s", info.symbol, renderExpressionArg(exp.args[0], emap, info.arg_info[0]));
            } else {
                str = foldl $1 + " " + info.symbol + " " + $2, (map renderExpressionArg($1, emap, info.arg_info[$1]),
                    exp.args);
            }
        }
        return str;
    }

    #! Parses a DPQL expression string into a DataProviderExpression
    /** @param text the DPQL expression to parse
        @param expressions optional map of expression names to their info for validation

        @return the parsed DataProviderExpression

        @throw DPQL-PARSE-ERROR if the expression contains syntax errors

        @par Example:
        @code{.py}
hash<DataProviderExpression> exp = DataProvider::parseDpqlExpression('@name == "John" && @age > 18');
        @endcode

        @since DataProvider 3.4
    */
    static hash<DataProviderExpression> parseDpqlExpression(string text,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        DpqlParser parser(expressions ?? DataProviderGenericExpressions);
        return parser.parse(text);
    }

    #! Parses a DPQL expression with full diagnostic information
    /** @param text the DPQL expression to parse
        @param expressions optional map of expression names to their info for validation

        @return a DpqlParseResult containing the expression, diagnostics, and tokens

        @par Example:
        @code{.py}
hash<DpqlParseResult> result = DataProvider::parseDpqlExpressionWithInfo('@name ==');
if (!result.success) {
    foreach hash<DpqlDiagnostic> diag in (result.diagnostics) {
        printf("Error at %d:%d: %s\n", diag.line, diag.column, diag.message);
    }
}
        @endcode

        @since DataProvider 3.4
    */
    static hash<DpqlParseResult> parseDpqlExpressionWithInfo(string text,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        DpqlParser parser(expressions ?? DataProviderGenericExpressions);
        return parser.parseWithInfo(text);
    }

    #! Serializes a DataProviderExpression to a DPQL string
    /** @param exp the expression to serialize
        @param expressions optional map of expression names to their info

        @return the DPQL string representation of the expression

        @par Example:
        @code{.py}
hash<DataProviderExpression> exp = <DataProviderExpression>{
    "exp": "==",
    "args": (<DataProviderFieldReference>{"field": "name"}, "John"),
};
string dpql = DataProvider::serializeDpqlExpression(exp);
# Result: @name == "John"
        @endcode

        @since DataProvider 3.4
    */
    static string serializeDpqlExpression(hash<DataProviderExpression> exp,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        DpqlSerializer serializer(expressions ?? DataProviderGenericExpressions);
        return serializer.serialize(exp);
    }

    #! Gets all tokens from a DPQL expression for syntax highlighting
    /** @param text the DPQL expression to tokenize

        @return a list of tokens with type and position information

        @par Example:
        @code{.py}
list<hash<DpqlTokenInfo>> tokens = DataProvider::getDpqlTokens('@name == "value"');
foreach hash<DpqlTokenInfo> tok in (tokens) {
    printf("Token: %s at %d:%d\n", DpqlTokenTypeNames{tok.type}, tok.line, tok.column);
}
        @endcode

        @since DataProvider 3.4
    */
    static list<hash<DpqlTokenInfo>> getDpqlTokens(string text) {
        DpqlTokenizer tokenizer(text);
        return tokenizer.getAllTokens();
    }

    #! Gets DPQL completions at the given cursor position
    /** @param text the DPQL expression text
        @param position cursor position (0-based character offset)
        @param fields available fields for completion
        @param expressions optional map of expression names to their info

        @return a list of completion items sorted by priority

        @par Example:
        @code{.py}
hash<string, AbstractDataField> fields = {
    "name": new QoreDataField("name", "Name", new QoreStringDataType()),
    "age": new QoreDataField("age", "Age", new QoreIntDataType()),
};
list<hash<DpqlCompletionItem>> items = DataProvider::getDpqlCompletions('@na', 3, fields);
# Returns completions for fields starting with "na"
        @endcode

        @since DataProvider 3.4
    */
    static list<hash<DpqlCompletionItem>> getDpqlCompletions(string text, int position,
            hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        DpqlCompletionProvider provider(expressions ?? DataProviderGenericExpressions);
        return provider.getCompletions(text, position, fields);
    }

    #! Validates a DPQL expression against available fields
    /** @param text the DPQL expression to validate
        @param fields available fields
        @param expressions optional map of expression names to their info

        @return a list of diagnostic messages for any validation errors

        @par Example:
        @code{.py}
list<hash<DpqlDiagnostic>> errors = DataProvider::validateDpqlExpression(
    '@unknown_field == "value"',
    my_fields
);
if (errors) {
    foreach hash<DpqlDiagnostic> err in (errors) {
        printf("Error: %s\n", err.message);
    }
}
        @endcode

        @since DataProvider 3.4
    */
    static list<hash<DpqlDiagnostic>> validateDpqlExpression(string text,
            hash<string, AbstractDataField> fields,
            *hash<string, hash<DataProviderExpressionInfo>> expressions) {
        DpqlParser parser(expressions ?? DataProviderGenericExpressions);
        hash<DpqlParseResult> result = parser.parseWithInfo(text);
        list<hash<DpqlDiagnostic>> diags = result.diagnostics;
        if (result.success && result.expression) {
            diags += parser.validate(result.expression, fields);
        }
        return diags;
    }

    #! Loads data providers from the environment
    static loadProvidersFromEnvironment(*LoggerInterface logger) {
        DataProvider::loadFromEnvironment("get_data_provider_map", "data provider", NOTHING, logger);
    }

    #! Tries to load a data provider from the environment from the connection name
    static *AbstractDataProvider tryLoadProviderForConnectionFromEnv(string name, *string subtype,
            *LoggerInterface logger) {
        load_module("ConnectionProvider");
        load_module("DatasourceProvider");
        *object v = DataProvider::loadFromEnvironment("get_data_provider_map", "data provider", <SymbolInfo>{
            "type": "connection",
            "name": name,
            "subtype": subtype,
        }, logger);
        if (v) {
            return cast<AbstractDataProvider>(v);
        }
    }

    #! Registers all known data provider types
    static registerKnownTypes() {
        # register basic qore types
        DataProvider::registerType("qore/hash", new HashDataType());
        DataProvider::registerType("qore/string", new QoreStringDataType());
        DataProvider::registerType("qore/int", new QoreIntDataType());
        DataProvider::registerType("qore/number", new QoreNumberDataType());
        DataProvider::registerType("qore/float", new QoreFloatDataType());
        DataProvider::registerType("qore/date", new QoreDateDataType());
        DataProvider::registerType("qore/bool", new QoreBoolDataType());
        DataProvider::registerType("qore/binary", new QoreBinaryDataType());

        map DataProvider::tryLoad($1, True), TypeMap.iterator();
    }

    #! Loads data types from the environment
    static loadTypesFromEnvironment(*LoggerInterface logger) {
        DataProvider::loadFromEnvironment("get_type_provider_map", "type", NOTHING, logger);
    }

    #! Converts an info hash with objects to a hash with string descriptions instead of the objects
    /** @param info0 the raw info with objects describing the types
        @param with_type_info if @ref True then more type information will be returned in the response
    */
    static hash<auto> getInfoAsData(hash<auto> info0, *bool with_type_info) {
        # strip types
        hash info = info0;
        foreach hash<auto> i0 in (info.pairIterator()) {
            if (i0.value.typeCode() == NT_HASH) {
                if (i0.key =~ /_options/ || i0.key == "events" || i0.key == "messages") {
                    # if the info key value is a hash with a "type" key, then return the type name instead of the type
                    # object
                    map info{i0.key}{$1.key}.type = (map $1.getName(), $1.value.type),
                        i0.value.pairIterator(),
                        $1.value.type;
                    if (with_type_info) {
                        map info{i0.key}{$1.key}.type_info = (map $1.getInfo(), $1.value.type),
                            i0.value.pairIterator(),
                            $1.value.type;
                    }
                } else if (i0.key == "expressions") {
                    foreach hash<auto> i1 in (i0.value.pairIterator()) {
                        info{i0.key}{i1.key}.args = map (
                            "type_code": SignatureTypeCodeMap{$1.type_code},
                            "type": $1.type.getName(),
                        ), i1.value.args;
                        info{i0.key}{i1.key}.return_type = i1.value.return_type.getName();
                        if (with_type_info) {
                            info{i0.key}{i1.key}.arg_type_info = map (
                                "type_code": SignatureTypeCodeMap{$1.type_code},
                                "type": $1.type.getInfo(),
                            ), i1.value.args;
                            info{i0.key}{i1.key}.return_type_info = i1.value.return_type.getInfo();
                        }
                    }
                }
            }
        }
        return info;
    }

    #! Sets the flag that allows for automatic configuration from environment variables
    /** Must be called before any requests for types or factories are made, or an exception is raised

        @throw DATA-PROVIDER-ERROR cannot call this method after requests for types or factories are made
    */
    static setAutoConfig() {
        if (env_config_locked) {
            throw "DATA-PROVIDER-ERROR", "cannot call DataProvider::setAutoConfigFromEnvironment() after type or "
                "factories have been loaded";
        }
        allow_env_config = True;
    }

    #! Returns the auto config flag
    static bool getAutoConfig() {
        return allow_env_config;
    }

    #! Returns a list of strings in a path separated by "/" characters
    static list<string> getPathList(string path) {
        return DataProvider::getPathList(\path);
    }

    #! Returns a list of strings in a path separated by "/" characters, normalizes the argument
    static list<string> getPathList(reference<string> path) {
        # remove leading "/" chars
        path =~ s/^\/+//;
        # combine multiple "/" chars into a single char
        path =~ s/\/+/\//g;
        list<string> rv = path.split("/");
        path = "/" + path;
        return rv;
    }

    #! Registers a static initializer
    /** @since DataProvider 3.0
    */
    static registerStaticInitializer(code init) {
        ilist += init;
    }

    #! Checks for static initialization
    /** @since DataProvider 3.0
    */
    static checkStaticInit() {
        if (ilist) {
            DataProvider::checkStaticInitIntern();
        }
    }

    #! Defers static initialization until restoreStaticInitialization() is called
    /** @since DataProvider 3.0
    */
    static deferStaticInitialization() {
        push ilist_save, remove ilist;
    }

    #! Restores static initialization deferred by deferStaticInitialization()
    /** @since DataProvider 3.0
    */
    static restoreStaticInitialization() {
        if (*list<code> l = pop ilist_save) {
            ilist += l;
        }
    }

    #! Checks for static initialization
    private static synchronized checkStaticInitIntern() {
        if (ilist) {
            map $1(), (remove ilist);
        }
    }

    #! Shuts down background services related to data providers
    /** @since DataProvider 3.0
    */
    static shutdown() {
        foreach hash<auto> i in ((remove smap).pairIterator()) {
            try {
                i.value();
            } catch (hash<ExceptionInfo> ex) {
                stderr.printf("Error in data provider shutdown handler %y: %s", i.key, get_exception_string(ex));
            }
        }
    }

    #! Registers a handler to run when shutting down data provider services
    /** @param key a unique key for the shutdown handler
        @param handler the shutdown handler

        @since DataProvider 3.0
    */
    static registerShutdownHandler(string key, code handler) {
        if (smap{key}) {
            throw "SHUTDOWN-HANDLER-ERROR", sprintf("A shutdown handler with key %y has already been registered",
                key);
        }
        smap{key} = handler;
    }

    #! Deregisters a shutdown handler
    /** @param key the key used to register the shutdown handler

        @return True if the handler was deregistered, False if not (no such key)

        @since DataProvider 3.0
    */
    static bool deregisterShutdownHandler(string key) {
        return exists (remove smap{key});
    }

    #! Sets the \a env_config_locked variable if not already set
    private static checkRequest() {
        if (!env_config_locked) {
            env_config_locked = True;
        }
    }

    #! Tries to load a module corresponding to the given factory
    private static *string tryGetFactoryModuleName(string name) {
        return DataProvider::tryGetModuleName(name, "get_data_provider_map", "data provider");
    }

    #! Tries to load a module corresponding to the given factory
    private static *string tryGetTypeModuleName(string name) {
        return DataProvider::tryGetModuleName(name, "get_type_provider_map", "type");
    }

    #! Tries to load a module corresponding to the given factory
    private static *string tryGetModuleName(string name, string func, string type) {
        *string prov = ENV.QORE_DATA_PROVIDERS;
        if (prov) {
            list<string> pl = ENV.QORE_DATA_PROVIDERS.split(PathSep);
            foreach string mod in (pl) {
                Program p(PO_NO_USER_API);
                try {
                    p.loadModule(mod);
                } catch (hash<ExceptionInfo> ex) {
                    stderr.printf("ERROR: invalid QORE_DATA_PROVIDERS module %y %s: %s: %s\n", mod, get_ex_pos(ex),
                        ex.err, ex.desc);
                    continue;
                }
                try {
                    hash<string, string> dmap = p.callFunction(func);
                    *string mod = dmap{name};
                    if (mod) {
                        return mod;
                    }
                } catch (hash<ExceptionInfo> ex) {
                    # ignore NO-FUNCTION error
                    if (ex.err != "NO-FUNCTION") {
                        stderr.printf("ERROR: error in QORE_DATA_PROVIDERS module %y %s registration function: %s: "
                            "%s\n", mod, type, ex.err, ex.desc);
                    }
                }
            }
        }
    }

    #! Tries to load the given type
    private static *DataProviderTypeEntry tryLoadTypeFromPath(list<string> type_path) {
        string key = type_path[0 .. (type_path.size() - 2)].join("/");
        *string module_name = TypeMap{key} ?? DataProvider::tryGetTypeModuleName(key);
        if (!module_name) {
            return;
        }
        if (!DataProvider::tryLoad(module_name)) {
            return;
        }
        # now that the module has been loaded, try to return the type
        *DataProviderTypeEntry type_entry = type_cache.getRoot();
        map type_entry = type_entry.getChild($1), type_path, type_entry;
        return type_entry;
    }

    #! Try to load the given module
    private static bool tryLoad(string module_str, *bool init) {
        try {
            if (logger) {
                logger.info("DataProvider loading module: %y", module_str);
            }
            load_module(module_str);
            return True;
        } catch (hash<ExceptionInfo> ex) {
            string err = sprintf("DataProvider load module %y: %s", module_str, get_exception_string(ex));
            if (logger) {
                if (init) {
                    logger.debug("INIT: %s", err);
                } else {
                    logger.error("%s", err);
                }
            } else {
                stderr.print(err + "\n");
            }
        }
        return False;
    }

    #! Loads modules from the QORE_DATA_PROVIDERS environment variable
    private static *object loadFromEnvironment(string func, string type, *hash<SymbolInfo> info,
            *LoggerInterface logger) {
        # load modules from the QORE_DATA_PROVIDERS environment variable
        *string prov = ENV.QORE_DATA_PROVIDERS;
        if (prov) {
            list<string> pl = ENV.QORE_DATA_PROVIDERS.split(PathSep);
            foreach string mod in (pl) {
                if (mod_map{mod}) {
                    continue;
                }
                mod_map{mod} = True;
                Program p(PO_NO_USER_API);
                try {
                    p.loadModule(mod);
                    if (info && (*object rv = DataProvider::checkSymbol(info, logger))) {
                        return rv;
                    }
                } catch (hash<ExceptionInfo> ex) {
                    stderr.printf("ERROR: invalid QORE_DATA_PROVIDERS module %y %s: %s: %s\n", mod, get_ex_pos(ex),
                        ex.err, ex.desc);
                }
                hash<string, string> dmap;
                try {
                    dmap = p.callFunction(func);
                } catch (hash<ExceptionInfo> ex) {
                    # ignore NO-FUNCTION error
                    if (ex.err != "NO-FUNCTION") {
                        stderr.printf("ERROR: error in QORE_DATA_PROVIDERS module %y %s registration function: %s: "
                            "%s\n", mod, type, ex.err, ex.desc);
                    }
                }
                foreach string mod_str in (dmap.iterator()) {
                    if (mod_map{mod_str}) {
                        continue;
                    }
                    mod_map{mod_str} = True;
                    if (DataProvider::tryLoad(mod_str)) {
                        if (info && (*object rv = DataProvider::checkSymbol(info, logger))) {
                            return rv;
                        }
                    }
                }
            }
        }
    }

    #! Returns True if an object of the given type has been loaded
    private static *object checkSymbol(hash<SymbolInfo> info, *LoggerInterface logger) {
        switch (info.type) {
            case "connection": {
                try {
                    object conn = call_function("get_connection", info.name);
                    if (logger) {
                        conn.setLogger(logger);
                    }
                    return info.subtype.val() ? conn.getDataProvider(info.subtype) : conn.getDataProvider();
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "CONNECTION-ERROR") {
                        rethrow;
                    }
                }

                # try to get a provider from a DB connection
                try {
                    Datasource ds(call_function("get_ds_string", info.name));
                    load_module("DbDataProvider");
                    return create_object("DbDataProvider", ds);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "NO-FUNCTION" && ex.err != "DATASOURCE-ERROR"
                        && ex.err != "DATASOURCE-PARSE-ERROR") {
                        rethrow;
                    }
                }
                break;
            }

            case "factory": {
                return DataProvider::getFactory(info.name);
            }

            default:
                throw "INVALID-SYMBOL-TYPE", sprintf("symbol type %y is unknown", info.type);
        }
    }

    #! Check if the object is from a module that has been subject to dependency injections
    private static bool checkInjection(object obj, hash<string, string> module_map) {
        *string module_name = Class::getClass(obj).getModuleName();
        return module_name && module_map{obj.className()} == module_name;
    }

    #! Returns a descriptive string for the given field reference argument
    /**
        @since DataProvider 3.3
    */
    static private string renderExpressionArg(hash<DataProviderFieldReference> ref,
            hash<string, hash<DataProviderExpressionInfo>> emap, *hash<DataProviderArgInfo> arg_info) {
        return sprintf("{%s}", ref.field);
    }

    #! Returns a descriptive string for the given expression argument
    /**
        @since DataProvider 3.3
    */
    static private string renderExpressionArg(hash<DataProviderExpression> exp,
            hash<string, hash<DataProviderExpressionInfo>> emap, *hash<DataProviderArgInfo> arg_info) {
        *hash<DataProviderExpressionInfo> info = emap{exp.exp};
        if (!info) {
            throw "INVALID-EXPRESSION", sprintf("Expression %y not found in expression map", info.name);
        }
        string str;
        if (info.type == DET_Operator) {
            str = "(";
        }
        str += renderExpression(exp, emap);
        if (info.type == DET_Operator) {
            str += ")";
        }
        return str;
    }

    #! Returns a descriptive string for the given argument
    /**
        @since DataProvider 3.3
    */
    static private string renderExpressionArg(auto v, hash<string, hash<DataProviderExpressionInfo>> emap,
            *hash<DataProviderArgInfo> arg_info) {
        if (!exists v) {
            if (!exists (v = arg_info.default_value)) {
                return "";
            }
        }
%ifndef NoYaml
        # return any template references as-is
        if (v.typeCode() == NT_STRING && (v =~ /^\$[a-z][-a-z]+:({.*}|[a-zA-Z0-9_\.]+|'.*')$/)) {
            return v;
        }
        # strip YAML header (e.g., "%YAML 1.2\n--- " or "%YAML 1.2\n---\n") for inline rendering
        return regex_subst(trim(make_yaml(v)), "^%YAML [0-9.]+\\n---\\s*", "");
%else
        throw "MISSING-YAML", "Missing YAML module";
%endif
    }

    #! Renders the expression from the template
    /**
        @since DataProvider 3.3
    */
    static private string renderExpressionTemplate(hash<DataProviderExpressionInfo> info,
            hash<DataProviderExpression> exp, hash<string, hash<DataProviderExpressionInfo>> emap) {
        string str = info.render_template;
        if (str =~ /\$symbol/) {
            str = replace(str, "$symbol", info.symbol);
        }
        if (str =~ /\$args/) {
            str = replace(str, "$args", (foldl $1 + ", " + $2, (map renderExpressionArg($1, emap, info.arg_info[$1]),
                exp.args)));
        }
        # make direct substitutions for plain argument references
        foreach string t in (str =~ x/\$(arg\[[0-9]+\])/g) {
            softint n = (t =~ x/arg\[([0-9]+)\]/)[0];
            string argstr = renderExpressionArg(exp.args[n], emap, info.arg_info[n]);
            str = replace(str, "$" + t, argstr);
        }
        # look for substitutions with curly bracks and optional ternary expressions
        foreach string t in (str =~ x/\$\{(arg\[[0-9]+\](?:\s*\?[^:\}]*:[^\}]*)?)\}/) {
            (softint n, *string t0, *string t1) = (t =~ x/arg\[([0-9]+)\](?:\s*\?([^:]*):(.*))?$/);
            if (exp.args[n].val() || info.arg_info[n].default_value.val()) {
                str = replace(str, "${" + t + "}", t0);
            } else {
                str = replace(str, "${" + t + "}", t1);
            }
        }
        return str;
    }

    #! Deregisters a factory if an exception occurs during the other module's initialization
    private static deregisterFactory(string name) {
        factory_cache_lock.enter();
        on_exit factory_cache_lock.exit();

        *AbstractDataProviderFactory factory = remove factory_cache{name};
        if (!factory) {
            return;
        }
        *string module_name = Class::getClass(factory).getModuleName();
        if (exists module_name) {
            remove factory_module_map{factory.className()};
        }
    }
}

# private hashdecls
hashdecl SymbolInfo {
    # symbol type
    string type;
    # symbol name
    string name;
    # subtype, if any
    *string subtype;
}
hashdecl FactoryInfo {
    # factory name
    string name;
    # path to final data provider in factory
    list<string> path_list;
    # factory options
    auto options;
}
}
