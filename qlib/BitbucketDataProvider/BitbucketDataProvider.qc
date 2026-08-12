# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BitbucketDataProvider module
public namespace BitbucketDataProvider {
#! The root Bitbucket data provider
/** Exposes every action in @ref BitbucketDataProvider::BitbucketManifest "BitbucketManifest" as a child provider,
    plus the one hand-written action and the polling event providers, so that each registered action path
    resolves.
*/
public class BitbucketDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        /** The name the superseded TypeScript application registered, which is what existing workflows
            reference; it is deliberately not restyled to \c "Bitbucket".
        */
        const AppName = "Bitbucket";

        #! The name of the child provider holding the polling event providers
        const EventsChildName = "events";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";


        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bitbucket",
            "type": "BitbucketDataProvider",
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
                "short_desc": "The Bitbucket REST client to use",
                "type": AbstractDataProviderType::get(new Type("BitbucketRestClient::BitbucketRestClientIo")),
                "desc": "The `BitbucketRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the Bitbucket REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `BitbucketRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "A Bitbucket repository, project or workspace access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A Bitbucket access token, sent as a bearer token",
                "sensitive": True,
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "The Atlassian account username, used with an app password",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Atlassian account username to use with an app password; **not** the email "
                    "address",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "App Password",
                "short_desc": "An app password, used with the username",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "An app password created under **Personal settings > App passwords**",
                "sensitive": True,
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the Bitbucket REST client to use for API calls
    */
    constructor(*BitbucketRestClient::BitbucketRestClientIo rest)
            : RestSchemaActionSetDataProvider(BitbucketSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(BitbucketSchema::getActionSet(),
            BitbucketDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "bitbucket";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Bitbucket data provider";
    }

    #! Returns the action IDs plus the events container
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (EventsChildName, TablesChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == TablesChildName) {
            return new RestSchemaTablesDataProvider(TablesChildName, BitbucketSchema::getActionSet(),
                BitbucketRecordTables::Tables, rest);
        }
        if (name == EventsChildName) {
            return new BitbucketEventsDataProvider(cast<*BitbucketRestClient::BitbucketRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *BitbucketRestClient::BitbucketRestClientIo getRestClient(*hash<auto> options) {
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
        if (copts.username.val()) {
            ropts.username = copts.username;
        }
        if (copts.password.val()) {
            ropts.password = copts.password;
        }
        if (ropts) {
            return new BitbucketRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
