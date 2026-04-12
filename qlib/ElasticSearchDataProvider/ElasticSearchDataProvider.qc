# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchDataProvider.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! ElasticSearch app name
const AppName = "ElasticSearch";

#! The ElasticSearch data provider class
public class ElasticSearchDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! current URI path
        string uri_path = "/";

        #! The value to returns as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ElasticSearchDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": NOTHING,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "transaction_management": False,
            "supports_schema": False,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client for accessing the ElasticSearch server",
                "type": AbstractDataProviderType::get(new Type("AbstractRestClient")),
                "desc": "The REST client for accessing the ElasticSearch server; must be an "
                    "`AbstractRestClient` subclass — typically a `RestClientIo` (async, "
                    "multiplexed HTTP/1.1/2/3) or a `RestClient` (sync HTTP/1.x)",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the ElasticSearch server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the ElasticSearch REST server; overrides any URL in any REST client object "
                    "passed as an option",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options to any REST client created",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the `RestClientIo` constructor; only used if a `RestClientIo` object is created "
                    "for a call to the ElasticSearch server",
            },
        };
    }

    private {
        const ChildMap = {
            "document": Class::forName("ElasticSearchDataProvider::ElasticSearchDocumentDataProvider"),
            "index": Class::forName("ElasticSearchDataProvider::ElasticSearchIndexDataProvider"),
            "pipeline": Class::forName("ElasticSearchDataProvider::ElasticSearchPipelineDataProvider"),
            "indexes": Class::forName("ElasticSearchDataProvider::ElasticSearchIndexesDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(copts);
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    static AbstractRestClient getRestConnection(*hash<auto> options) {
        AbstractRestClient rest;
        if (options.restclient) {
            rest = options.restclient;
            if (options.url) {
                # Non-mutating URL override: build a sibling client via copyWithUrl()
                # — works for both sync RestClient and async RestClientIo
                rest = rest.copyWithUrl(options.url);
            }
        } else {
            if (!options.url && !options.restclient_options) {
                throw "CONSTRUCTOR-ERROR", "no 'restclient', 'url', or 'restclient_options' option passed; "
                    "cannot create REST client to ElasticSearch instance without a URL";
            }

            hash<auto> opts;
            opts += options.restclient_options;
            if (options.url) {
                opts.url = options.url;
            }
            # Default to the async multiplexed RestClientIo transport (HTTP/1.1/2/3)
            rest = new RestClientIo(opts);
        }
        return rest;
    }

    #! Returns the data provider name
    string getName() {
        return "elasticsearch";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Boolean string type for query parameters
public const SoftBoolStringType = new SoftBoolStringType();

#! Boolean data provider string type for query parameters
public const SoftBoolDataProviderStringType = AbstractDataProviderType::get(SoftBoolStringType);

#! Boolean string type
public class SoftBoolStringType inherits Qore::Reflection::Type {
    #! Creates the object
    constructor() : Type("*softstring") {
    }

    #! Returns the value after any conversions by the type
    auto acceptsValue(auto value) {
        return parse_boolean(value) ? "true" : "false";
    }

    #! Returns the default value for the type or NOTHING if the type has no default value
    auto getDefaultValue() {
        return "false";
    }
}
}
