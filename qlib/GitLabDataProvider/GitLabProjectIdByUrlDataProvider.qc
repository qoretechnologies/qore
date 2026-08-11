# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabProjectIdByUrlDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The request type of the hand-written project-lookup action
public class GitLabProjectIdByUrlRequestDataType inherits HashDataType {
    private {
        #! The request fields
        const Fields = {
            "project_url": {
                "type": StringType,
                "display_name": "Project URL",
                "short_desc": "The URL of the GitLab project",
                "desc": "The URL of the project, as copied from a browser.\n\n"
                    "Any URL **within** the project works, not only its front page: "
                    "`https://gitlab.com/group/project`, "
                    "`https://gitlab.com/group/project/-/issues/17` and "
                    "`https://gitlab.com/group/project.git` all identify the same project.",
                "example_value": "https://gitlab.com/group/project",
                "attr": {"preselected": True},
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! The response type of the hand-written project-lookup action
public class GitLabProjectIdByUrlResponseDataType inherits HashDataType {
    private {
        #! The response fields
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Project ID",
                "short_desc": "The numeric ID of the project",
                "desc": "The numeric ID of the project, which identifies it unambiguously and never "
                    "changes when the project is renamed or moved",
            },
            "path_with_namespace": {
                "type": StringType,
                "display_name": "Path With Namespace",
                "short_desc": "The project's full path, as it reads",
                "desc": "The project's full path including its groups, as it reads (`group/project`).\n\n"
                    "This is the form every other action in this application takes: give it to a "
                    "**Project** option directly, and the transport escapes it.",
            },
            "encoded_path_with_namespace": {
                "type": StringType,
                "display_name": "Encoded Path With Namespace",
                "short_desc": "The project's full path, percent-encoded",
                "desc": "The project's full path escaped into a single path segment "
                    "(`group%2Fproject`).\n\n"
                    "Provided because the application this one replaces returned it and existing mappers "
                    "read it. **Do not** feed it to a **Project** option: those escape what they are "
                    "given, so an already-escaped path would be escaped a second time and would not "
                    "resolve. Use `path_with_namespace` or `id` for that.",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Resolves a GitLab project URL to the project it names, as a hand-written action
/** Every other action in this application is generated from GitLab's own OpenAPI 3 description.  This one
    cannot be: no GitLab endpoint takes a URL.  What the action does before it calls GitLab - recovering a
    project path from a URL - is the whole point of it, and a schema cannot describe that.

    It exists because the application this module replaces exported it, under this same action ID, and
    workflows call it: it is the bridge from a URL a person pastes to the ID or path every other action
    wants.

    @section gitlabprojectidbyurl_forms The URL Forms It Accepts

    GitLab's web UI puts \c "/-/" between a project's path and whatever page is being viewed inside it, and
    reserves \c "-" so that no group or project can be named that.  That makes it an unambiguous terminator
    for the project path, so a URL copied from **any** page of a project resolves, not only the front page:

    |!URL|!Project path
    |\c https://gitlab.com/group/project|\c group/project
    |\c https://gitlab.com/group/sub/project/-/issues/17|\c group/sub/project
    |\c https://gitlab.com/group/project.git|\c group/project
    |\c https://gitlab.com/group/project/|\c group/project

    The superseded application took the whole URL path and stripped only the leading slash, so every one of
    those but the first resolved to a project that does not exist.  This is the defect the port closes.

    @section gitlabprojectidbyurl_escaping What It Returns

    \c path_with_namespace is the form the rest of this application takes, and
    \c encoded_path_with_namespace is that path escaped into one path segment.  Both are returned because
    the superseded application returned both and existing mappers read the names; only the unescaped one
    should be given to another action's **Project** option, which escapes what it receives.
*/
public class GitLabProjectIdByUrlDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The action ID
        const ActionName = GitLabDataProvider::ProjectIdByUrlAction;

        #! The request path, which takes the escaped project path
        const RequestPath = "/api/v4/projects/%s";

        #! The action display name
        const DisplayName = "Get Project ID by URL";

        #! The action short description
        const ShortDesc = "Resolve a GitLab project URL to the project's ID and path";

        #! The action description
        const Desc = "Resolves the URL of a GitLab project to the project's numeric ID and its full "
            "path.\n\n"
            "A URL copied from **any** page of the project works - its front page, an issue, a merge "
            "request or a clone URL - because GitLab separates a project's path from the page being "
            "viewed with `/-/`, and reserves `-` so no group or project can be named it.\n\n"
            "Use the returned `id` or `path_with_namespace` in any action that takes a **Project**.";

        #! The separator GitLab puts between a project path and a page within the project
        /** \c "-" is a reserved path segment, so no group or project can be named it and this cannot
            appear inside a project path.
        */
        const PageSeparator = "/-/";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ActionName,
            "type": "GitLabProjectIdByUrlDataProvider",
            "supports_request": True,
            "supports_children": False,
        };

        #! The request type
        static GitLabProjectIdByUrlRequestDataType RequestType();

        #! The response type
        static GitLabProjectIdByUrlResponseDataType ResponseType();
    }

    private {
        #! The REST client used for API calls
        *GitLabRestClient::GitLabRestClientIo rest;
    }

    #! Creates the object from the arguments
    /** @param rest the REST client to use for API calls
    */
    constructor(*GitLabRestClient::GitLabRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the action ID
    string getName() {
        return ActionName;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return DisplayName;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return ShortDesc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return Desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Resolves the URL and returns the project's ID and paths
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!rest) {
            throw "GITLAB-PROJECT-URL-ERROR", "no REST client is available; this data provider was "
                "created without one";
        }
        string url = req.project_url.toString();
        string path = GitLabProjectIdByUrlDataProvider::getProjectPath(url);
        string encoded = encode_url(path, True);
        on_error rethrow $1.err, sprintf("%s (while resolving GitLab project URL %y to the project %y)",
            $1.desc, url, path);
        hash<RestClient::RestResponseInfo> resp = rest.restGet(sprintf(RequestPath, encoded));
        # GitLab answers 404 for a project that does not exist, which arrives as an exception; a 200 whose
        # body carries no ID would mean the response is not a project at all
        *softint id = resp.body.id;
        if (!id) {
            throw "GITLAB-PROJECT-URL-ERROR", sprintf("GitLab returned no project ID for URL %y "
                "(project path %y); the response was: %y", url, path, resp.body);
        }
        return {
            "id": id,
            "path_with_namespace": path,
            "encoded_path_with_namespace": encoded,
        };
    }

    #! Returns the project path named by the given GitLab project URL
    /** @param url the URL of a GitLab project, or of any page within one

        @return the project's path with its namespace, as it reads and not escaped

        @throw GITLAB-PROJECT-URL-ERROR the URL cannot be parsed, or names no project
    */
    static string getProjectPath(string url) {
        hash<UrlInfo> info;
        try {
            info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "GITLAB-PROJECT-URL-ERROR", sprintf("cannot parse %y as a URL: %s: %s", url, ex.err,
                ex.desc);
        }
        *string path = info.path;
        if (!path.val()) {
            throw "GITLAB-PROJECT-URL-ERROR", sprintf("URL %y has no path, so it names no project; a "
                "project URL looks like \"https://gitlab.com/group/project\"", url);
        }
        # everything from "/-/" on is the page being viewed inside the project, not part of its path
        int sep = index(path, PageSeparator);
        if (sep >= 0) {
            path = substr(path, 0, sep);
        }
        # a clone URL ends in ".git"; the project's path does not
        if (path =~ /\.git$/) {
            path = substr(path, 0, -4);
        }
        # the path arrives percent-encoded in the URL and is returned as it reads
        path = decode_url(path);
        # trim the separators a copied URL carries at either end
        path =~ s/^\/+//;
        path =~ s/\/+$//;
        if (!path.val()) {
            throw "GITLAB-PROJECT-URL-ERROR", sprintf("URL %y names the GitLab instance but no project "
                "within it; a project URL looks like \"https://gitlab.com/group/project\"", url);
        }
        return path;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
