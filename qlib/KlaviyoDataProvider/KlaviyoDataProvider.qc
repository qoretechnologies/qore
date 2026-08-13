# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the KlaviyoDataProvider module
public namespace KlaviyoDataProvider {
#! The root Klaviyo data provider
/** Exposes every action in @ref KlaviyoDataProvider::KlaviyoManifest "KlaviyoManifest" as a child
    provider, so that each registered action path resolves, plus the nine hand-written actions and the
    \c events container holding the polling event providers.
*/
public class KlaviyoDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        const AppName = "Klaviyo";

        #! The name of the child provider holding the event providers
        const EventsChildName = "events";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "klaviyo",
            "type": "KlaviyoDataProvider",
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
                "short_desc": "The Klaviyo REST client to use",
                "type": AbstractDataProviderType::get(new Type("KlaviyoRestClient::KlaviyoRestClientIo")),
                "desc": "The `KlaviyoRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Klaviyo REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `KlaviyoRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "A Klaviyo access token or private API key",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A Klaviyo OAuth2 access token, or a private API key when `token_type` is set to "
                    "`Klaviyo-API-Key`",
                "sensitive": True,
            },
            "token_type": <DataProviderOptionInfo>{
                "display_name": "Token Type",
                "short_desc": "How the token is presented in the Authorization header",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "How the token is presented in the `Authorization` header: `Bearer` for an OAuth2 "
                    "access token, which is the default, or `Klaviyo-API-Key` for a private API key",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The Klaviyo API URL",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Klaviyo API URL; Klaviyo publishes one host, so this normally needs no "
                    "attention",
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Klaviyo REST client to use for API calls
    */
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest)
            : RestSchemaActionSetDataProvider(KlaviyoSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(KlaviyoSchema::getActionSet(),
            KlaviyoDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "klaviyo";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Klaviyo data provider";
    }

    #! Returns the action IDs plus the hand-written actions and the events container
    /** Event actions name a path under \c "/events", so that container has to resolve alongside the
        schema-derived actions.
    */
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + KlaviyoHandWrittenActions + (EventsChildName, TablesChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == TablesChildName) {
            return new RestSchemaTablesDataProvider(TablesChildName, KlaviyoSchema::getActionSet(),
                KlaviyoRecordTables::Tables, rest);
        }
        if (name == EventsChildName) {
            return new KlaviyoEventsDataProvider(cast<*KlaviyoRestClient::KlaviyoRestClientIo>(rest));
        }
        if (*AbstractDataProvider hand = KlaviyoDataProvider::getHandWrittenProvider(name,
                cast<*KlaviyoRestClient::KlaviyoRestClientIo>(rest))) {
            return hand;
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns the hand-written action with the given ID, if there is one
    /** @param name the action ID
        @param rest the REST client to use for API calls

        @return the provider, or @ref nothing if the ID does not name a hand-written action

        @see @ref klaviyomanifest_handwritten
    */
    static *AbstractDataProvider getHandWrittenProvider(string name,
            *KlaviyoRestClient::KlaviyoRestClientIo rest) {
        if (*hash<KlaviyoRelationshipActionInfo> ri = KlaviyoRelationshipActions{name}) {
            return new KlaviyoRelationshipDataProvider(ri, KlaviyoSchema::getActionSet(), rest);
        }
        switch (name) {
            case KlaviyoCreateEventDataProvider::ActionName:
                return new KlaviyoCreateEventDataProvider(KlaviyoSchema::getActionSet(), rest);
            case KlaviyoSubscribeProfileDataProvider::ActionName:
                return new KlaviyoSubscribeProfileDataProvider(KlaviyoSchema::getActionSet(), rest);
            case KlaviyoUnsubscribeProfileDataProvider::ActionName:
                return new KlaviyoUnsubscribeProfileDataProvider(KlaviyoSchema::getActionSet(), rest);
        }
    }

    #! Returns a REST client from the constructor options
    private static *KlaviyoRestClient::KlaviyoRestClientIo getRestClient(*hash<auto> options) {
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
        if (copts.token_type.val()) {
            ropts.token_type = copts.token_type;
        }
        if (copts.url.val()) {
            ropts.url = copts.url;
        }
        if (ropts) {
            return new KlaviyoRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
