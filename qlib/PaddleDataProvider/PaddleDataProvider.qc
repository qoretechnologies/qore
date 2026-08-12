# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! The root Paddle data provider
/** Exposes every action in @ref PaddleDataProvider::PaddleManifest "PaddleManifest" as a child provider, so
    that each registered action path resolves.
*/
public class PaddleDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        const AppName = "Paddle";

        #! The name of the child provider holding the webhook event providers
        const EventsChildName = "events";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "paddle",
            "type": "PaddleDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
            "children_can_support_records": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The Paddle REST client to use",
                "type": AbstractDataProviderType::get(new Type("PaddleRestClient::PaddleRestClientIo")),
                "desc": "The `PaddleRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Paddle REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `PaddleRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "A Paddle API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A Paddle API key (`pdl_live_apikey_…` or `pdl_sdbx_apikey_…`), sent as a bearer "
                    "token",
                "sensitive": True,
            },
            "instance_type": <DataProviderOptionInfo>{
                "display_name": "Instance Type",
                "short_desc": "The Paddle environment to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Paddle environment to work against: `api` for production, `sandbox-api` for "
                    "sandbox. This must match the key in `token`, as the two environments accept different "
                    "keys",
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Paddle REST client to use for API calls
    */
    constructor(*PaddleRestClient::PaddleRestClientIo rest)
            : RestSchemaActionSetDataProvider(PaddleSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(PaddleSchema::getActionSet(),
            PaddleDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "paddle";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Paddle data provider";
    }

    #! Returns the action IDs plus the events container
    /** Event actions name a path under \c "/events", so that container has to resolve alongside the
        schema-derived actions.
    */
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (EventsChildName, TablesChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == TablesChildName) {
            return new RestSchemaTablesDataProvider(TablesChildName, PaddleSchema::getActionSet(),
                PaddleRecordTables::Tables, rest);
        }
        if (name == EventsChildName) {
            return new PaddleEventsDataProvider(cast<*PaddleRestClient::PaddleRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *PaddleRestClient::PaddleRestClientIo getRestClient(*hash<auto> options) {
        # validated here rather than through checkOptions(), which is an instance method and so is not
        # available while resolving the arguments of the base class constructor
        list<string> unknown = (map $1, keys options, !exists ConstructorOptions{$1}) ?? ();
        if (unknown) {
            throw "CONSTRUCTOR-ERROR", sprintf("unknown option(s) %y; known options: %y", unknown,
                keys ConstructorOptions);
        }
        *hash<auto> copts = options;
        if (copts.restclient) {
            return copts.restclient;
        }
        hash<auto!> ropts = copts.restclient_options ?? {};
        if (copts.token.val()) {
            ropts.token = copts.token;
        }
        if (copts.instance_type.val()) {
            ropts.instance_type = copts.instance_type;
        }
        if (ropts) {
            return new PaddleRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
