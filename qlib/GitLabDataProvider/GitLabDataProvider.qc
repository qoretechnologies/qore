# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitLabDataProvider module
public namespace GitLabDataProvider {
#! The root GitLab data provider
/** Exposes every action in @ref GitLabDataProvider::GitLabManifest "GitLabManifest" as a child provider,
    plus the one hand-written action and the polling event providers, so that each registered action path
    resolves.
*/
public class GitLabDataProvider inherits RestSchemaActions::RestSchemaActionSetDataProvider {
    public {
        #! The application name
        /** The name the superseded TypeScript application registered, which is what existing workflows
            reference; it is deliberately not restyled to \c "GitLab".
        */
        const AppName = "Gitlab";

        #! The name of the child provider holding the polling event providers
        const EventsChildName = "events";

        #! The action ID of the hand-written commit-creation action
        /** Hand-written because GitLab's own description declares this operation with a placeholder
            request body; see @ref GitLabDataProvider::GitLabSchema "GitLabSchema".
        */
        const CreateCommitAction = "postApiV4ProjectsIdRepositoryCommits";

        #! The action ID of the hand-written project-lookup action
        /** Hand-written because no GitLab endpoint takes a URL; see
            @ref GitLabDataProvider::GitLabProjectIdByUrlDataProvider "GitLabProjectIdByUrlDataProvider".

            The ID is the one the superseded TypeScript application registered.
        */
        const ProjectIdByUrlAction = "get_project_id_by_url";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gitlab",
            "type": "GitLabDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The GitLab REST client to use",
                "type": AbstractDataProviderType::get(new Type("GitLabRestClient::GitLabRestClientIo")),
                "desc": "The `GitLabRestClientIo` object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for the GitLab REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for the `GitLabRestClientIo` constructor; only used when a client is "
                    "created for a call",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "A GitLab personal, project or group access token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A GitLab access token with the `api` scope, sent as a bearer token",
                "sensitive": True,
            },
            "hostname": <DataProviderOptionInfo>{
                "display_name": "GitLab Hostname",
                "short_desc": "The GitLab instance to work against",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The base URL of the GitLab installation, ex: `https://gitlab.com` or "
                    "`https://gitlab.example.com`. The target URL is derived from this",
            },
        };
    }

    #! Creates the object from a REST client
    /** @param rest the GitLab REST client to use for API calls
    */
    constructor(*GitLabRestClient::GitLabRestClientIo rest)
            : RestSchemaActionSetDataProvider(GitLabSchema::getActionSet(), rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : RestSchemaActionSetDataProvider(GitLabSchema::getActionSet(),
            GitLabDataProvider::getRestClient(options)) {
    }

    #! Returns the data provider name
    string getName() {
        return "gitlab";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "GitLab data provider";
    }

    #! Returns the action IDs plus the hand-written actions and the events container
    private *list<string> getChildProviderNamesImpl() {
        return (RestSchemaActionSetDataProvider::getChildProviderNamesImpl() ?? ())
            + (CreateCommitAction, ProjectIdByUrlAction, EventsChildName);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == EventsChildName) {
            return new GitLabEventsDataProvider(cast<*GitLabRestClient::GitLabRestClientIo>(rest));
        }
        if (name == CreateCommitAction) {
            return new GitLabCreateCommitDataProvider(action_set,
                cast<*GitLabRestClient::GitLabRestClientIo>(rest));
        }
        if (name == ProjectIdByUrlAction) {
            return new GitLabProjectIdByUrlDataProvider(cast<*GitLabRestClient::GitLabRestClientIo>(rest));
        }
        return RestSchemaActionSetDataProvider::getChildProviderImpl(name);
    }

    #! Returns a REST client from the constructor options
    private static *GitLabRestClient::GitLabRestClientIo getRestClient(*hash<auto> options) {
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
        if (copts.hostname.val()) {
            ropts.hostname = copts.hostname;
        }
        if (ropts) {
            return new GitLabRestClientIo(ropts);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
