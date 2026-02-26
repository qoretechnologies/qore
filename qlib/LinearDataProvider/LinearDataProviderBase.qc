# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearDataProviderBase.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Base class for Linear data providers
public class LinearDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Linear data providers
        const BaseConstructorOptions = {
            "linearrestclient": <DataProviderOptionInfo>{
                "display_name": "Linear REST Client",
                "short_desc": "The LinearRestClient object",
                "type": AbstractDataProviderType::get(new Type("LinearRestClient::LinearRestClient")),
                "desc": "The LinearRestClient object to use for API calls",
            },
            "linearrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Linear REST Client Options",
                "short_desc": "Options for creating a LinearRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a LinearRestClient object if one is not provided directly",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Token",
                "short_desc": "Linear API token",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Linear API token (API key or OAuth2 access token) for authentication",
                "sensitive": True,
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client ID for authentication",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client secret for authentication",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        LinearRestClient::LinearRestClient rest;

        #! The connection object, if applicable
        *LinearRestClient::LinearRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.linearrestclient) {
            rest = copts.linearrestclient;
        } else {
            hash<auto> client_opts = copts.linearrestclient_options ?? {};
            if (copts.token) {
                client_opts.token = copts.token;
            }
            if (copts.oauth2_client_id) {
                client_opts.oauth2_client_id = copts.oauth2_client_id;
            }
            if (copts.oauth2_client_secret) {
                client_opts.oauth2_client_secret = copts.oauth2_client_secret;
            }
            rest = new LinearRestClient::LinearRestClient(client_opts);
        }
    }

    #! Returns the REST client
    LinearRestClient::LinearRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *LinearRestClient::LinearRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Executes a GraphQL query
    /** @param query the GraphQL query string
        @param variables optional variables for the query
        @return the response data

        @throw GRAPHQL-ERROR if the response contains errors
    */
    hash<auto> doGraphQL(string query, *hash<auto> variables) {
        @assert(query.val(), "GraphQL query cannot be empty");
        @assert(rest, "REST client must be initialized before making GraphQL calls");
        @debug(log(LoggerLevel::DEBUG, "doGraphQL: executing query with variables: %y", variables));
        hash<auto> response = rest.graphql(query, variables);
        @assert(response.body, "GraphQL response body cannot be null");
        if (response.body.errors) {
            @debug(log(LoggerLevel::DEBUG, "doGraphQL: GraphQL error: %y", response.body.errors));
            string err_msg = response.body.errors[0].message ?? "Unknown GraphQL error";
            throw "GRAPHQL-ERROR", err_msg, response.body.errors;
        }
        @debug(log(LoggerLevel::DEBUG, "doGraphQL: success, response keys: %y", keys response.body.data));
        return response.body.data;
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.token) {
            rv.token = copts.token;
        }
        if (copts.oauth2_client_id) {
            rv.oauth2_client_id = copts.oauth2_client_id;
        }
        if (copts.oauth2_client_secret) {
            rv.oauth2_client_secret = copts.oauth2_client_secret;
        }
        foreach string key in (keys copts.linearrestclient_options) {
            rv{key} = copts.linearrestclient_options{key};
        }
        return rv;
    }

    #! Flattens GraphQL connection fields (those with 'nodes') to plain arrays
    /** GraphQL Relay-style connections return arrays as { nodes: [...] }. This method
        converts them to plain arrays for cleaner output.

        @param data the data hash to process
        @return the processed data with flattened connection fields
    */
    static hash<auto> flattenConnections(hash<auto> data) {
        hash<auto> rv = {};
        foreach hash<auto> i in (data.pairIterator()) {
            if (i.value.typeCode() == NT_HASH) {
                # Check if this is a connection object with 'nodes'
                if (i.value.hasKey("nodes") && i.value.nodes.typeCode() == NT_LIST) {
                    # Flatten: replace { nodes: [...] } with just [...]
                    rv{i.key} = map LinearDataProviderBase::flattenConnections($1), i.value.nodes;
                } else {
                    # Recursively process nested hashes
                    rv{i.key} = LinearDataProviderBase::flattenConnections(i.value);
                }
            } else if (i.value.typeCode() == NT_LIST) {
                # Recursively process list elements
                rv{i.key} = map $1.typeCode() == NT_HASH
                    ? LinearDataProviderBase::flattenConnections($1)
                    : $1, i.value;
            } else {
                rv{i.key} = i.value;
            }
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        \c getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "teams": True,
            "users": True,
            "projects": True,
            "cycles": True,
            "labels": True,
            "states": True,
            "issues": True,
        };
    }

    #! Returns information on supported element reference data (for list-type options)
    /** @return a hash of supported element reference data; keys in the hash returned are supported in calls to
        @ref getElementReferenceData()
    */
    *hash<string, bool> getSupportedElementReferenceData() {
        return {
            "teams": True,
            "labels": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "teams": return getReferenceTeams();
            case "users": return getReferenceUsers();
            case "projects": return getReferenceProjects();
            case "cycles": return getReferenceCycles(action_opts.teamId);
            case "labels": return getReferenceLabels(action_opts.teamId);
            case "states": return getReferenceStates(action_opts.teamId);
            case "issues": return getReferenceIssues(action_opts.teamId);
        }
    }

    #! Returns element reference data of the given kind if available (for list-type options)
    /** @param type the unique type name of the element reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for elements of this list type
    */
    private *list<hash<AllowedValueInfo>> getElementReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "teams": return getReferenceTeams();
            case "labels": return getReferenceLabels(action_opts.teamId);
        }
    }

    #! Returns available teams
    private *list<hash<AllowedValueInfo>> getReferenceTeams() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListTeamsQuery, {"first": 100});
        foreach hash<auto> team in (data.teams.nodes) {
            string desc = team.description ?? sprintf("Team %s (%s)", team.name, team.key);
            rv += <AllowedValueInfo>{
                "display_name": sprintf("%s (%s)", team.name, team.key),
                "short_desc": desc,
                "desc": desc,
                "value": team.id,
            };
        }
        return rv;
    }

    #! Returns available users
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListUsersQuery, {"first": 100});
        foreach hash<auto> user in (data.users.nodes) {
            if (!user.active) {
                continue;
            }
            string display = user.displayName ?? user.name;
            string desc = user.email ?? display;
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": desc,
                "desc": desc,
                "value": user.id,
            };
        }
        return rv;
    }

    #! Returns available projects
    private *list<hash<AllowedValueInfo>> getReferenceProjects() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> data = doGraphQL(ListProjectsQuery, {"first": 100});
        foreach hash<auto> project in (data.projects.nodes) {
            string desc = project.description ?? project.name;
            rv += <AllowedValueInfo>{
                "display_name": project.name,
                "short_desc": desc,
                "desc": desc,
                "value": project.id,
            };
        }
        return rv;
    }

    #! Returns available cycles for a team
    private *list<hash<AllowedValueInfo>> getReferenceCycles(*string teamId) {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> vars;
        vars.first = 100;
        if (teamId) {
            vars.filter = {"team": {"id": {"eq": teamId}}};
        }
        hash<auto> data = doGraphQL(ListCyclesQuery, vars);
        foreach hash<auto> cycle in (data.cycles.nodes) {
            string display = cycle.name ?? sprintf("Cycle %d", cycle.number);
            string desc = sprintf("%s (Team: %s)", display, cycle.team.name);
            rv += <AllowedValueInfo>{
                "display_name": display,
                "short_desc": desc,
                "desc": desc,
                "value": cycle.id,
            };
        }
        return rv;
    }

    #! Returns available labels for a team
    private *list<hash<AllowedValueInfo>> getReferenceLabels(*string teamId) {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> vars;
        vars.first = 100;
        if (teamId) {
            vars.filter = {"team": {"id": {"eq": teamId}}};
        }
        hash<auto> data = doGraphQL(ListLabelsQuery, vars);
        foreach hash<auto> label in (data.issueLabels.nodes) {
            string desc = label.description ?? label.name;
            rv += <AllowedValueInfo>{
                "display_name": label.name,
                "short_desc": desc,
                "desc": desc,
                "value": label.id,
            };
        }
        return rv;
    }

    #! Returns available workflow states for a team
    private *list<hash<AllowedValueInfo>> getReferenceStates(*string teamId) {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> vars;
        vars.first = 100;
        if (teamId) {
            vars.filter = {"team": {"id": {"eq": teamId}}};
        }
        hash<auto> data = doGraphQL(ListWorkflowStatesQuery, vars);
        foreach hash<auto> state in (data.workflowStates.nodes) {
            string desc = sprintf("%s (%s)", state.name, state.type);
            rv += <AllowedValueInfo>{
                "display_name": state.name,
                "short_desc": desc,
                "desc": desc,
                "value": state.id,
            };
        }
        return rv;
    }

    #! Returns available issues for reference data
    private *list<hash<AllowedValueInfo>> getReferenceIssues(*string teamId) {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> vars;
        vars.first = 100;
        if (teamId) {
            vars.filter = {"team": {"id": {"eq": teamId}}};
        }
        hash<auto> data = doGraphQL(ListIssuesRefQuery, vars);
        foreach hash<auto> issue in (data.issues.nodes) {
            string desc = issue.title;
            rv += <AllowedValueInfo>{
                "display_name": sprintf("%s: %s", issue.identifier, issue.title),
                "short_desc": desc,
                "desc": desc,
                "value": issue.id,
            };
        }
        return rv;
    }
}

} # namespace LinearDataProvider
