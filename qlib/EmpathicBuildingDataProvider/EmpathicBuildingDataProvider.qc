# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EmpathicBuildingDataProvider module definition

/** EmpathicBuildingDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EmpathicBuildingDataProvider module
public namespace EmpathicBuildingDataProvider {
#! Empathic building app name
public const AppName = "EmpathicBuilding";

#! The EmpathicBuilding data provider class
public class EmpathicBuildingDataProvider inherits EmpathicBuildingDataProviderBase {
    public {
        #! current URI path
        string uri_path = "/";

        #! The value to returns as the name of the object
        string display_name;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "EmpathicBuildingDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("AbstractRestClient")),
                "desc": "The REST client to use to communicate with the EmpathicBuilding server; must be an "
                    "`AbstractRestClient` subclass — typically a `RestClientIo` (async, multiplexed "
                    "HTTP/1.1/2/3) or a `RestClient` (sync HTTP/1.x)",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "EmpathicBuilding URL",
                "short_desc": "The URL to the EmpathicBuilding server",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL to the EmpathicBuilding server; overrides any URL in any REST client object passed "
                    "as an option",
                "default_value": "https://eu-api.empathicbuilding.com",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options to the REST client constructor",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the REST client constructor; only used if a REST client object is created for a "
                    "call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Sets a gateway authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Sets a gateway authorization token to be included in requests",
                "sensitive": True,
            },
        };
    }

    private {
        const ChildMap = {
            "measurements": Class::forName("EmpathicBuildingDataProvider::EmpathicBuildingMeasurementsDataProvider"),
            "organizations": Class::forName("EmpathicBuildingDataProvider::EmpathicBuildingOrganizationsDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = EmpathicBuildingDataProvider::getRestConnection(copts);
    }

    #! Create the object from the REST connection
    constructor(RestClient::AbstractRestClient rest, *Logger::LoggerInterface logger)
            : EmpathicBuildingDataProviderBase(rest, logger) {
    }

    static RestClient::AbstractRestClient getRestConnection(*hash<auto> options) {
        RestClient::AbstractRestClient rest;
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
                    "cannot create REST client to EmpathicBuilding instance without a URL";
            }
            if (!options.token) {
                throw "CONSTRUCTOR-ERROR", "no 'token' option passed; "
                    "cannot create REST client to EmpathicBuilding instance without an authorzation token";
            }

            hash<auto> opts;
            opts += options.restclient_options;
            if (options.url) {
                opts.url = options.url;
            }
            # Default to the async multiplexed RestClientIo transport (HTTP/1.1/2/3)
            rest = new RestClientIo(opts);
        }
        # add the gateway token
        if (options.token) {
            rest.addDefaultHeaders({"Authorization": "Bearer " + options.token});
        }
        return rest;
    }

    #! Returns the data provider name
    string getName() {
        return "empathicbuilding";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("EmpathicBuilding data provider for `%s`", rest.getSafeUrl());
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
        return cls.newObject(rest, logger);
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
