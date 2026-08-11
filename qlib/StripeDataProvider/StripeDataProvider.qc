# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the StripeDataProvider module
public namespace StripeDataProvider {
#! The root Stripe data provider
/** Exposes every action in @ref StripeDataProvider::StripeManifest "StripeManifest" as a child provider, so
    that each registered action path resolves.
*/
public class StripeDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        const AppName = "Stripe";

        #! The name of the child provider holding the webhook event providers
        const EventsChildName = "events";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stripe",
            "type": "StripeDataProvider",
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
                "short_desc": "The Stripe REST client to use",
                "type": AbstractDataProviderType::get(new Type("StripeRestClient::StripeRestClientIo")),
                "desc": "The `StripeRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Stripe REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `StripeRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "api_key": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "A Stripe secret or restricted key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A Stripe secret key (`sk_live_…` or `sk_test_…`) or restricted key "
                    "(`rk_live_…` or `rk_test_…`), sent as a bearer token",
                "sensitive": True,
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Stripe REST client to use for API calls
    */
    constructor(*StripeRestClient::StripeRestClientIo rest)
            : RestSchemaActionSetDataProvider(StripeSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(StripeSchema::getActionSet(),
            StripeDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "stripe";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Stripe data provider for API version `%s`", StripeSchema::Provenance.api_version);
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
            return new RestSchemaTablesDataProvider(TablesChildName, StripeSchema::getActionSet(),
                StripeRecordTables::Tables, rest);
        }
        if (name == EventsChildName) {
            return new StripeEventsDataProvider(cast<*StripeRestClient::StripeRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *StripeRestClient::StripeRestClientIo getRestClient(*hash<auto> options) {
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
        if (copts.api_key.val() || copts.restclient_options) {
            return new StripeRestClientIo((copts.restclient_options ?? {}) + (copts.api_key.val()
                ? {"api_key": copts.api_key}
                : {}));
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
