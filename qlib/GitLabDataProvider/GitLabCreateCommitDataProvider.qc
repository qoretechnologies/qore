# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabCreateCommitDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! One file change in a commit
/** The element type of @ref GitLabDataProvider::GitLabCreateCommitRequestDataType "the commit request"'s
    \c actions list.
*/
public class GitLabCommitActionDataType inherits HashDataType {
    private {
        #! The fields of one file change
        const Fields = {
            "action": {
                "type": StringType,
                "display_name": "Action",
                "short_desc": "What to do with the file",
                "desc": "What to do with the file: `create`, `update`, `move`, `delete` or `chmod`",
                "allowed_values": ("create", "update", "move", "delete", "chmod"),
                "example_value": "create",
            },
            "file_path": {
                "type": StringType,
                "display_name": "File Path",
                "short_desc": "The full path of the file, relative to the repository root",
                "desc": "The full path of the file, relative to the repository root",
                "example_value": "lib/example.rb",
            },
            "previous_path": {
                "type": StringOrNothingType,
                "display_name": "Previous Path",
                "short_desc": "The file's original path, for a \"move\" action",
                "desc": "The file's original path; required for a `move` action and ignored otherwise",
            },
            "content": {
                "type": StringOrNothingType,
                "display_name": "Content",
                "short_desc": "The file content, for \"create\" and \"update\"",
                "desc": "The new content of the file. Required for `create` and `update`, and optional "
                    "for `move` when the file is also being changed",
            },
            "encoding": {
                "type": StringOrNothingType,
                "display_name": "Encoding",
                "short_desc": "How \"content\" is encoded: \"text\" or \"base64\"",
                "desc": "How `content` is encoded: `text` (the default) or `base64`. Use `base64` for a "
                    "binary file",
                "allowed_values": ("text", "base64"),
            },
            "last_commit_id": {
                "type": StringOrNothingType,
                "display_name": "Last Commit ID",
                "short_desc": "Reject the change if the file has moved on since this commit",
                "desc": "The commit the file was last changed in. Supplying it makes the change "
                    "conditional: GitLab rejects the commit if somebody else has changed the file since, "
                    "instead of silently overwriting their work",
            },
            "execute_filemode": {
                "type": BoolOrNothingType,
                "display_name": "Executable",
                "short_desc": "Set or clear the file's execute bit",
                "desc": "Sets or clears the execute bit on the file; used with a `chmod` action",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! The request type of the hand-written commit-creation action
public class GitLabCreateCommitRequestDataType inherits HashDataType {
    private {
        #! The request fields
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Project",
                "short_desc": "The project to commit to",
                "desc": "The project to commit to, given as its numeric ID or as its path "
                    "(`group/project`).\n\n"
                    "Give the path as it reads; the transport escapes it into the single path segment "
                    "GitLab expects. A path escaped by hand would be escaped a second time and would not "
                    "resolve.",
                "attr": {
                    "ref_data": GitLabManifest::RefProject,
                    "supports_custom_values": True,
                    "preselected": True,
                    "has_dependents": True,
                    "on_change": ("refetch",),
                },
            },
            "branch": {
                "type": StringType,
                "display_name": "Branch",
                "short_desc": "The branch to commit to",
                "desc": "The name of the branch to commit to.\n\n"
                    "To commit to a **new** branch, name it here and also supply `start_branch` or "
                    "`start_sha` to say where it starts from",
                "example_value": "main",
                "attr": {
                    "ref_data": GitLabManifest::RefBranch,
                    "depends_on": ("id",),
                    "supports_custom_values": True,
                    "preselected": True,
                },
            },
            "commit_message": {
                "type": StringType,
                "display_name": "Commit Message",
                "short_desc": "The commit message",
                "desc": "The commit message",
                "attr": {"preselected": True},
            },
            "actions": {
                "type": new ListDataType(new GitLabCommitActionDataType(), False),
                "display_name": "File Changes",
                "short_desc": "The file changes making up the commit",
                "desc": "The file changes this commit makes. Every change is applied in one commit, so "
                    "either all of them land or none of them do",
                "attr": {"preselected": True},
            },
            "start_branch": {
                "type": StringOrNothingType,
                "display_name": "Start Branch",
                "short_desc": "Create the target branch from this branch",
                "desc": "The branch the target branch is created from, when `branch` does not exist yet",
                "attr": {
                    "ref_data": GitLabManifest::RefBranch,
                    "depends_on": ("id",),
                    "supports_custom_values": True,
                },
            },
            "start_sha": {
                "type": StringOrNothingType,
                "display_name": "Start Commit",
                "short_desc": "Create the target branch from this commit",
                "desc": "The commit the target branch is created from, when `branch` does not exist yet",
                "attr": {
                    "ref_data": GitLabManifest::RefCommit,
                    "depends_on": ("id",),
                    "supports_custom_values": True,
                },
            },
            "start_project": {
                "type": StringOrNothingType,
                "display_name": "Start Project",
                "short_desc": "Take the starting point from a different project",
                "desc": "The project `start_branch` or `start_sha` refers to, when it is not this one; "
                    "used to commit into a fork from its upstream",
                "attr": {
                    "ref_data": GitLabManifest::RefProject,
                    "supports_custom_values": True,
                },
            },
            "author_email": {
                "type": StringOrNothingType,
                "display_name": "Author Email",
                "short_desc": "Record a different author for the commit",
                "desc": "The email recorded as the commit author; defaults to the authenticated user",
            },
            "author_name": {
                "type": StringOrNothingType,
                "display_name": "Author Name",
                "short_desc": "Record a different author for the commit",
                "desc": "The name recorded as the commit author; defaults to the authenticated user",
            },
            "stats": {
                "type": BoolOrNothingType,
                "display_name": "Include Stats",
                "short_desc": "Include the lines added and removed in the response",
                "desc": "Include the lines added and removed by the commit in the response",
            },
            "force": {
                "type": BoolOrNothingType,
                "display_name": "Force",
                "short_desc": "Overwrite the target branch with a new commit based on the start point",
                "desc": "Overwrite the target branch with a new commit based on `start_branch` or "
                    "`start_sha`.\n\n"
                    "**This discards whatever was on the target branch**, so it is the one option here "
                    "that can lose work",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Creates a commit, as a hand-written action
/** Every other action in this application is generated from GitLab's own OpenAPI 3 description.  This one
    cannot be: the description declares \c "POST /api/v4/projects/{id}/repository/commits" with a
    \c multipart/form-data body whose only property is \c file, which is neither what GitLab documents nor
    what it accepts.  The real request is a JSON body carrying \c branch, \c commit_message and an
    \c actions array.

    Repairing that in the manifest's overlay was not an option - an overlay expresses presentation, and
    inventing a request contract there would hide an upstream defect behind something that looks like a
    display name.  Writing the one action by hand keeps the defect visible and keeps the action usable.

    The action ID is unchanged from the application this module replaces, so nothing calling it can tell
    that it is not schema-derived.

    @see @ref GitLabDataProvider::GitLabSchema "GitLabSchema" for how the defect was measured
*/
public class GitLabCreateCommitDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! The action ID
        const ActionName = GitLabDataProvider::CreateCommitAction;

        #! The request path
        const RequestPath = "/api/v4/projects/%s/repository/commits";

        #! The action display name
        const DisplayName = "Create Commit";

        #! The action short description
        const ShortDesc = "Commit one or more file changes";

        #! The action description
        const Desc = "Creates a commit with one or more file changes.\n\n"
            "Every change in `actions` is applied in a single commit, so either all of them land or none "
            "of them do. Set `last_commit_id` on a change to make it conditional on the file not having "
            "moved on since that commit, which is how a concurrent change is detected rather than "
            "overwritten.\n\n"
            "Committing to a branch that does not exist yet creates it, provided `start_branch` or "
            "`start_sha` says where it starts from.\n\n"
            "This action is hand-written rather than generated, because GitLab's own API description "
            "declares this one operation with a placeholder request body.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ActionName,
            "type": "GitLabCreateCommitDataProvider",
            "supports_request": True,
            "supports_children": False,
        };

        #! The request type
        static GitLabCreateCommitRequestDataType RequestType();
    }

    private {
        #! The action set, which supplies the reference data this action's dropdowns use
        RestSchemaActionSet action_set;

        #! The REST client used for API calls
        *GitLabRestClient::GitLabRestClientIo rest;
    }

    #! Creates the object from the arguments
    /** @param action_set the application's action set, which supplies the dropdowns this action's options
        declare
        @param rest the REST client to use for API calls
    */
    constructor(RestSchemaActionSet action_set, *GitLabRestClient::GitLabRestClientIo rest) {
        self.action_set = action_set;
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

    #! Creates the commit and returns the commit record
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (!rest) {
            throw "GITLAB-CREATE-COMMIT-ERROR", "no REST client is available; this data provider was "
                "created without one";
        }
        hash<auto> args = req;
        *string id = args.id.toString();
        if (!id.val()) {
            throw "GITLAB-CREATE-COMMIT-ERROR", "the \"id\" option is required; it names the project to "
                "commit to";
        }
        # the project is escaped here, exactly as the generated actions escape their path variables: GitLab
        # identifies a project by a numeric ID or by its path, and the path has to survive as one path
        # segment.  The option takes the path as it reads, so "group/project" has to become
        # "group%2Fproject" before it is substituted; substituting it verbatim would address
        # /api/v4/projects/group/project/repository/commits, which is a different route entirely
        string path = sprintf(RequestPath, encode_url(id, True));
        hash<auto> body = args - "id";
        on_error rethrow $1.err, sprintf("%s (while creating a commit in GitLab project %y)", $1.desc, id);
        hash<RestClient::RestResponseInfo> resp = rest.restPost(path, body);
        return resp.body;
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            GitLabManifest::RefProject: True,
            GitLabManifest::RefBranch: True,
            GitLabManifest::RefCommit: True,
        };
    }

    #! Populates a dropdown by delegating to the action set that declares it
    /** The dropdowns this action offers are the same ones the generated actions offer, so they are served
        by the same reference data rather than reimplemented here.
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        *hash<RestSchemaReferenceDataInfo> rd = action_set.getReferenceDataDecls(){type};
        if (!rd) {
            return;
        }
        return new RestSchemaActionDataProvider(action_set, action_set.getOperationEx(rd.action), rest)
            .getReferenceData(type, action_opts);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
