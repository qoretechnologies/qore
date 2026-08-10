# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! The root GitHub data provider
/** Exposes every action in @ref GitHubDataProvider::GitHubManifest "GitHubManifest" as a child provider,
    plus the webhook event providers and the record-based tables, so that each registered action path
    resolves.
*/
public class GitHubDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        /** The name the superseded TypeScript application registered, which is what existing workflows
            reference; it is deliberately not restyled to \c "GitHub".
        */
        const AppName = "Github";

        #! The name of the child provider holding the webhook event providers
        const EventsChildName = "events";

        #! The name of the child provider holding the record-based tables
        const TablesChildName = "tables";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "github",
            "type": "GitHubDataProvider",
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
                "short_desc": "The GitHub REST client to use",
                "type": AbstractDataProviderType::get(new Type("GitHubRestClient::GitHubRestClientIo")),
                "desc": "The `GitHubRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the GitHub REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `GitHubRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "A GitHub personal access token or OAuth2 access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A GitHub personal access token or OAuth2 access token, sent as a bearer token",
                "sensitive": True,
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the GitHub REST client to use for API calls
    */
    constructor(*GitHubRestClient::GitHubRestClientIo rest)
            : RestSchemaActionSetDataProvider(GitHubSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(GitHubSchema::getActionSet(),
            GitHubDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "github";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitHub data provider";
    }

    #! Returns the action IDs plus the events and tables containers
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (EventsChildName, TablesChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EventsChildName:
                return new GitHubEventsDataProvider(cast<*GitHubRestClient::GitHubRestClientIo>(rest));
            case TablesChildName:
                return new GitHubTablesDataProvider(cast<*GitHubRestClient::GitHubRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *GitHubRestClient::GitHubRestClientIo getRestClient(*hash<auto> options) {
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
        if (ropts) {
            return new GitHubRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
