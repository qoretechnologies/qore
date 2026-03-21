# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDataProvider module definition

/** GoogleApiParentDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GoogleDataProvider module
public namespace GoogleDataProvider {
#! The Google data provider base class
public class GoogleApiParentDataProvider inherits GoogleDataProviderBase {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The Google REST client to use",
                "type": AbstractDataProviderType::get(new Type("GoogleRestClientIo")),
                "desc": "The `GoogleRestClientIo` object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Google REST client options to use when creating a new client",
                "type": AbstractDataProviderTypeMap."*hash",
                "desc": "Options to the `GoogleRestClientIo` constructor; only used if a `GoogleRestClientIo` object "
                    "is created for a call",
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Sets the OAuth2 client ID",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 client secret",
            },
            "oauth2_refresh_token": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Refresh Token",
                "short_desc": "Sets the OAuth2 refresh token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The OAuth2 refresh token, if known; complements option `token`",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Authorization Token",
                "short_desc": "Set an authorization token to be included in requests",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Any authorization token to use for the connection; will be passed as "
                    "`Authorization: Bearer ...` (assuming that `token_type` is `Bearer`) in request headers; "
                    "this option cannot be used with `username` and `password` options or authentication information "
                    "in the URL; if this option is set then most OAuth2 options are ignored for logins",
            },
        };
    }

    private {
        #! The Google API name (ex: "calendar")
        string api;

        #! The resource path name
        list<string> resource_path;

        #! Data provider description
        string desc;

        #! Data provider type name
        string type;

        #! Any path args associated with this data provider
        *hash<auto> path_args;

        #! Method hash
        hash<string, bool> methods;

        #! Path groups under this parent
        *hash<string, list<string>> subpaths;
    }

    #! Creates the object
    constructor(RestClientIo::RestClientIo rest, string api, string resource, *hash<auto> path_args,
            *string path_prefix, ...)
            : GoogleDataProviderBase(rest) {
        self.api = api;
        resource_path = resource.split("/");
        self.path_args = path_args;
        setSubpaths(path_prefix, argv);
        desc = sprintf("Google %s %s provider for `%s`", api, resource_path.last(), rest.getSafeUrl());
        type = sprintf("Google%s%sDataProvider", cap(api), cap(resource_path.last()));
        methods = getMethodsForResource(api, resource_path, path_args);
        check();
    }

    #! Creates the object
    constructor(string api, string resource, *hash<auto> path_args, *string path_prefix, ...) {
        self.api = api;
        resource_path = resource.split("/");
        self.path_args = path_args;
        setSubpaths(path_prefix, argv);
        desc = sprintf("Google %s %s provider", api, resource_path.last());
        type = sprintf("Google%s%sDataProvider", cap(api), cap(resource_path.last()));
        methods = getMethodsForResource(api, resource_path, path_args);
        check();
    }

    #! Returns the data provider name
    string getName() {
        return resource_path.last();
    }

    #! Returns the data provider description
    *string getDesc() {
        return desc;
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        string api = cap(self.api);
        string resource = cap(self.resource_path.last());

        *list<hash<DataProvider::DataProviderSummaryInfo>> rv = map <DataProviderSummaryInfo>{
            "name": $1,
            "desc": sprintf("Google %s events/%s provider%s", self.api, $1,
                rest ? sprintf(" for `%s`", rest.getSafeUrl()) : ""),
            "type": sprintf("Google%s%s%sDataProvider", api, resource_path.last(), cap($1)),
            "supports_request": True,
        }, keys methods;
        if (subpaths) {
            rv += map GoogleApiParentDataProvider::getSummaryInfo(self.api, $1.key, $1.value),
                subpaths.pairIterator();
        }
        return rv;
    }

    static hash<DataProvider::DataProviderSummaryInfo> getSummaryInfo(string api, string resource,
            list<string> resource_path) {
        string desc = sprintf("Google %s %s provider", api, resource);
        string type = sprintf("Google%s%sDataProvider", GoogleDataProviderBase::cap(api),
            GoogleDataProviderBase::cap(resource));
        return <DataProviderSummaryInfo>{
            "name": resource,
            "desc": desc,
            "type": type,
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Sets subpaths
    private setSubpaths(*string path_prefix, *list<auto> args) {
        if (path_prefix) {
            if (path_prefix !~ /\/$/) {
                path_prefix += "/";
            }
        }
        if (args) {
            foreach string arg in (args) {
                list<string> resource_path = (path_prefix + arg).split("/");
                checkResource(api, resource_path);
                subpaths{arg} = resource_path;
            }
        } else {
            GoogleDataProviderBase::getRequestInfo(api);
            *hash<auto> h = info_map{api}.resources;
            foreach string seg in (resource_path) {
                if (!h{seg}) {
                    throw "GOOGLE-API-ERROR", sprintf("API %y resource %y is invalid at %y; known resources at "
                        "this level: %y", api, resource_path.join("/"), seg, keys h);
                }
                h = h{seg}.resources;
            }
            map subpaths{$1} = resource_path + $1, keys h;
        }
    }

    #! Check the data provider
    private check() {
        if (!methods && !subpaths) {
            throw "GOOGLE-API-ERROR", sprintf("API %y resource %y has no methods and no subpaths", api,
                resource_path.join("/"));
        }
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        *list<string> rv = keys methods;
        if (subpaths) {
            rv += keys subpaths;
        }
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *GoogleDataProvider::GoogleDataProviderBase getChildProviderImpl(string name) {
        if (!rest) {
            throw "GOOGLE-API-ERROR", sprintf("Google API %y resource %y has no REST client set", api,
                resource_path.join("/"));
        }
        if (methods{name}) {
            return new GoogleApiDataProvider(rest, api, resource_path.join("/") + "/" + name, path_args);
        }
        if (exists (*list<string> resource_path = subpaths{name})) {
            return new GoogleApiParentDataProvider(rest, api, resource_path.join("/"), path_args);
        }
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": resource_path.last(),
            "desc": desc,
            "type": type,
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }
}
}