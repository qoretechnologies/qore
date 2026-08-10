# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConfluenceDataProvider module definition

/*  ConfluenceDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ConfluenceDataProvider module
public namespace ConfluenceDataProvider {
#! The root Confluence data provider
/** Exposes every action in @ref ConfluenceDataProvider::ConfluenceManifest "ConfluenceManifest" as a child provider,
    plus the one hand-written action and the polling event providers, so that each registered action path
    resolves.
*/
public class ConfluenceDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        /** The name the superseded TypeScript application registered, which is what existing workflows
            reference; it is deliberately not restyled to \c "Confluence".
        */
        const AppName = "Confluence";

        #! The name of the child provider holding the polling event providers
        const EventsChildName = "events";


        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "confluence",
            "type": "ConfluenceDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The Confluence REST client to use",
                "type": AbstractDataProviderType::get(new Type("ConfluenceRestClient::ConfluenceRestClientIo")),
                "desc": "The `ConfluenceRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Confluence REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `ConfluenceRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "An OAuth2 access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An OAuth2 access token, sent as a bearer token",
                "sensitive": True,
            },
            "cloud_id": <DataProviderOptionInfo>{
                "display_name": "Cloud ID",
                "short_desc": "The Confluence site's cloud ID, used with the OAuth2 flow",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The site's cloud ID; the site is then reached through Atlassian's API gateway",
            },
            "site": <DataProviderOptionInfo>{
                "display_name": "Site",
                "short_desc": "The site host, ex: \"example.atlassian.net\"",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Confluence site host, used with an API token",
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "The Atlassian account email, used with an API token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Atlassian account email address to use with an API token",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "API Token",
                "short_desc": "An API token, used with the account email",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An API token created at **id.atlassian.com > Security > API tokens**",
                "sensitive": True,
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Confluence REST client to use for API calls
    */
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest)
            : RestSchemaActionSetDataProvider(ConfluenceSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(ConfluenceSchema::getActionSet(),
            ConfluenceDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "confluence";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Confluence data provider";
    }

    #! Returns the action IDs plus the events container
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ()) + (EventsChildName,);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == EventsChildName) {
            return new ConfluenceEventsDataProvider(cast<*ConfluenceRestClient::ConfluenceRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *ConfluenceRestClient::ConfluenceRestClientIo getRestClient(*hash<auto> options) {
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
        if (copts.cloud_id.val()) {
            ropts.cloud_id = copts.cloud_id;
        }
        if (copts.site.val()) {
            ropts.site = copts.site;
        }
        if (copts.username.val()) {
            ropts.username = copts.username;
        }
        if (copts.password.val()) {
            ropts.password = copts.password;
        }
        if (ropts) {
            return new ConfluenceRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
