# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ServiceNowRestDataProvider module definition

/** ServiceNowRestDataProvider.qc Copyright 2020 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ServiceNowRestDataProvider module
public namespace ServiceNowRestDataProvider {
#! Application name
public const AppName = "ServiceNow";

#! The ServiceNowRest data provider class
public class ServiceNowRestDataProvider inherits ServiceNowRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ServiceNowRestDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The ServiceNow API version to use",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ServiceNow API version to use",
            },
            "snrestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The ServiceNowRestClient object",
                "type": AbstractDataProviderType::get(new Type("ServiceNowRestClient")),
                "desc": "The ServiceNowRestClient object",
            },
            "snrestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options to the ServiceNowRestClient constructor",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options to the `ServiceNowRestClient` constructor; only used if a "
                    "`ServiceNowRestClient` object is created for a call",
            },
            "client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The client ID (required if `client_secret` is provided)",
            },
            "client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The client secret (required if `client_id` is provided)",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The password (required if `username` is provided)",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "ServiceNow URL",
                "short_desc": "The URL to the ServiceNow instance",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the ServiceNow instance; overrides any URL in any REST client object "
                    "passed as an option",
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The username (required if `password` is provided)",
            },
        };

        #! Environment options
        const EnvOptions = {
            "url": "SERVICENOW_URL",
            "client_id": "SERVICENOW_CLIENT_ID",
            "client_secret": "SERVICENOW_CLIENT_SECRET",
            "username": "SERVICENOW_USERNAME",
            "password": "SERVICENOW_PASSWORD",
        };
    }

    #! Creates the object from the arguments
    constructor(ServiceNowRestClient::ServiceNowRestClient rest) : ServiceNowRestDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        bool auto_config = DataProvider::getAutoConfig();
        if (auto_config) {
            map options{$1.key} = ENV{$1.value}, EnvOptions.pairIterator(),
                exists ENV{$1.value} && !exists options{$1.key};
        }
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.snrestclient) {
            copts.snrestclient.checkClientOptions(copts);
            rest = copts.snrestclient;
        }

        if (!rest) {
            if (!options.url && !options.snrestclient) {
                throw "CONSTRUCTOR-ERROR", sprintf("missing 'url' option for connection to the ServiceNow instance");
            }
            rest = new ServiceNowRestClient(getClientOptions(copts));
        }
    }

    #! Returns the data provider name
    string getName() {
        return "/";
    }

    #! Returns the data provider description
    string getDesc() {
        return "ServiceNow top-level data provider";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return ("tables",);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "tables") {
            return new ServiceNowTablesDataProvider(rest);
        }
    }

    #! Returns options for the ServiceNowRestClient
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("snrestclient_options", "snrestclient")
            + copts.snrestclient_options;
    }
}
}
