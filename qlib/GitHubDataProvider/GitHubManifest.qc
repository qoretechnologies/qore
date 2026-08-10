# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the GitHub application exports, and how they are presented
/** @section githubmanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, which derived them from the
    \c operationId of a vendored Swagger 2.0 document.  GitHub generates that document and the OpenAPI 3 one
    from the same source, so they agree and the IDs carry over unchanged.

    GitHub writes an \c operationId with a slash — \c "pulls/list" — and the action catalog accepts only
    \c A-Za-z0-9_- in an action name, so the superseded application registered them with the slash replaced
    by a hyphen: \c "pulls-list", \c "actions-create-or-update-repo-secret".  **That hyphenated form is the
    action ID**, because it is what existing workflows call; the \c operationId is recorded separately in
    \c operation_id, so an upstream rename surfaces as drift rather than silently renaming an action.

    @section githubmanifest_added What This Application Adds

    Three read-only listings are exported that the TypeScript application did not, all for the same reason:
    its dropdowns called endpoints it never exported, so they could not be filled from the application's own
    action surface.

    |!Action|!Why
    |\c orgs-list-for-authenticated-user|its organisation dropdown listed \c "/user/orgs", which it did not export
    |\c issues-list-labels-for-repo|its label dropdown listed a repository's labels, which it did not export
    |\c repos-list-for-user|its repository dropdown searched \c "user:<owner>" through the code search API, which is capped at 1000 results and rate-limited separately; listing a user's or organisation's repositories directly is exact and is not

    @section githubmanifest_scope Almost Every Dropdown Is Scoped

    A branch, an issue, a pull request, a label and a collaborator all belong to one repository, and a
    repository belongs to one owner.  Each of those dropdowns therefore declares
    @ref RestSchemaActions::RestSchemaReferenceDataInfo::options_from "options_from", so it forwards the
    \c owner and \c repo already chosen on the action being filled in, and stays empty until they have
    values — listing every branch the credential can see would offer values that do not belong to the
    chosen repository.

    \c owner is the one dropdown with no scope: it is read from the owners of the repositories the
    credential can see, which is the same set the superseded application derived it from.

    @section githubmanifest_assignees Assignees Come From Collaborators

    The superseded application filled its assignee dropdowns from \c "/contributors" — everyone who has ever
    committed.  GitHub only accepts an assignee with **push access** and silently drops the rest, so that
    dropdown offered values the API would ignore.  This application lists collaborators instead, which is
    exactly the set GitHub will accept.

    @section githubmanifest_paging Pagination

    Every collection GitHub returns is paged with \c page and \c per_page, and \c per_page is capped at 100.
    The next and last pages are reported in the \c Link **response header** rather than in the body, and the
    body is the bare array.

    The two search actions are different: they return \c {total_count, items} and are capped at 1000 results
    however far they are paged.
*/
public class GitHubManifest {
    public {
        #! Action group: repositories and their contents
        const GroupRepositories = "Repositories";

        #! Action group: issues
        const GroupIssues = "Issues";

        #! Action group: pull requests
        const GroupPullRequests = "Pull Requests";

        #! Action group: branches, commits and refs
        const GroupCode = "Branches and Commits";

        #! Action group: releases
        const GroupReleases = "Releases";

        #! Action group: organizations
        const GroupOrganizations = "Organizations";

        #! Action group: search
        const GroupSearch = "Search";

        #! Action group: GitHub Actions
        const GroupActions = "Actions";

        #! Reference data name: repository owners
        const RefOwner = "github-owner";

        #! Reference data name: repositories
        const RefRepository = "github-repository";

        #! Reference data name: organizations
        const RefOrganization = "github-organization";

        #! Reference data name: branches
        const RefBranch = "github-branch";

        #! Reference data name: issues
        const RefIssue = "github-issue";

        #! Reference data name: pull requests
        const RefPull = "github-pull";

        #! Reference data name: repository collaborators
        const RefCollaborator = "github-collaborator";

        #! Reference data name: issue labels
        const RefLabel = "github-label";

        #! GitHub's maximum page size for a listing
        const MaxPerPage = GitHubRestClient::GitHubRestClientBase::MaxPerPage;

        #! Fixed options passed to every listing that fills a dropdown
        const RefPaging = {"per_page": MaxPerPage};

        #! Forwards the repository a scoped dropdown belongs to
        const RefRepoScope = {
            "owner": "owner",
            "repo": "repo",
        };

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.  A GitHub collection is returned as
            a bare array, so none of these needs a records path.
        */
        const ReferenceData = {
            RefOwner: <RestSchemaReferenceDataInfo>{
                "name": RefOwner,
                "action": "repos-list-for-authenticated-user",
                "options": RefPaging,
                # the owner is nested inside each repository; the same owner appears on every repository it
                # owns, and duplicate values are dropped when the dropdown is built
                "value_field": "owner.login",
                "display_field": "owner.login",
                "desc": "the users and organizations owning repositories this credential can see",
            },
            RefRepository: <RestSchemaReferenceDataInfo>{
                "name": RefRepository,
                "action": "repos-list-for-user",
                "options": RefPaging,
                "options_from": {"username": "owner"},
                "value_field": "name",
                "display_field": "name",
                "desc": "the repositories belonging to the chosen owner",
            },
            RefOrganization: <RestSchemaReferenceDataInfo>{
                "name": RefOrganization,
                "action": "orgs-list-for-authenticated-user",
                "options": RefPaging,
                "value_field": "login",
                "display_field": "login",
                "desc": "the organizations the authenticated user belongs to",
            },
            RefBranch: <RestSchemaReferenceDataInfo>{
                "name": RefBranch,
                "action": "repos-list-branches",
                "options": RefPaging,
                "options_from": RefRepoScope,
                "value_field": "name",
                "display_field": "name",
                "desc": "the branches of the chosen repository",
            },
            RefIssue: <RestSchemaReferenceDataInfo>{
                "name": RefIssue,
                "action": "issues-list-for-repo",
                "options": RefPaging,
                "options_from": RefRepoScope,
                "value_field": "number",
                "display_field": "title",
                "desc": "the open issues in the chosen repository",
            },
            RefPull: <RestSchemaReferenceDataInfo>{
                "name": RefPull,
                "action": "pulls-list",
                "options": RefPaging,
                "options_from": RefRepoScope,
                "value_field": "number",
                "display_field": "title",
                "desc": "the open pull requests in the chosen repository",
            },
            RefCollaborator: <RestSchemaReferenceDataInfo>{
                "name": RefCollaborator,
                "action": "repos-list-collaborators",
                "options": RefPaging,
                "options_from": RefRepoScope,
                "value_field": "login",
                "display_field": "login",
                "desc": "the users with access to the chosen repository",
            },
            RefLabel: <RestSchemaReferenceDataInfo>{
                "name": RefLabel,
                "action": "issues-list-labels-for-repo",
                "options": RefPaging,
                "options_from": RefRepoScope,
                "value_field": "name",
                "display_field": "name",
                "desc": "the labels defined in the chosen repository",
            },
        };

        #! Overlay for the account that owns the repository an action works in
        const OwnerOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Owner",
            "short_desc": "The user or organization that owns the repository",
            "desc": "The account the repository belongs to — a user or an organization.\n\n"
                "For `qoretechnologies/qorus` this is `qoretechnologies`.",
            "ref_data": RefOwner,
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for the repository an action works in
        const RepoOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Repository",
            "short_desc": "The name of the repository, without the owner",
            "desc": "The repository name on its own, **not** `owner/name`.\n\n"
                "For `qoretechnologies/qorus` this is `qorus`.",
            "ref_data": RefRepository,
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "depends_on": ("owner",),
            "on_change": ("refetch",),
        };

        #! Overlay for the organization an action works in
        const OrgOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Organization",
            "short_desc": "The organization to work in",
            "ref_data": RefOrganization,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the branch an action works on
        const BranchOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Branch",
            "short_desc": "The branch name, without the \"refs/heads/\" prefix",
            "ref_data": RefBranch,
            "supports_custom_values": True,
            "preselected": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay for the issue an action works on
        const IssueOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Issue",
            "short_desc": "The issue number within the repository",
            "desc": "The issue **number** shown in the UI and the URL, not the internal `id`.",
            "ref_data": RefIssue,
            "supports_custom_values": True,
            "preselected": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay for the pull request an action works on
        const PullOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Pull Request",
            "short_desc": "The pull request number within the repository",
            "desc": "The pull request **number** shown in the UI and the URL, not the internal `id`.\n\n"
                "GitHub numbers issues and pull requests in one sequence, so a pull request number is never "
                "also an issue number.",
            "ref_data": RefPull,
            "supports_custom_values": True,
            "preselected": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay for a single assignee
        const AssigneeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Assignee",
            "short_desc": "The login of the user to assign",
            "desc": "GitHub only accepts an assignee with **push access** to the repository and silently "
                "ignores anyone else, so this dropdown lists the repository's collaborators.",
            "ref_data": RefCollaborator,
            "supports_custom_values": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay for a list of assignees
        const AssigneesOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Assignees",
            "short_desc": "The logins of the users to assign",
            "desc": "GitHub only accepts assignees with **push access** to the repository and silently "
                "ignores the rest, so this dropdown lists the repository's collaborators.",
            "element_ref_data": RefCollaborator,
            "supports_custom_values": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay for a list of labels
        const LabelsOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Labels",
            "short_desc": "The names of the labels to apply",
            "desc": "Label names. A label that does not exist in the repository is created by GitHub when it "
                "is applied.",
            "element_ref_data": RefLabel,
            "supports_custom_values": True,
            "depends_on": ("owner", "repo"),
        };

        #! Overlay shared by the \c per_page option of every listing action
        const PerPageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Size",
            "short_desc": "How many records to return in one page; at most 100",
            "desc": "How many records to return in one page. GitHub's maximum is **100** and its default is "
                "30.\n\n"
                "There is no total in the response: the `Link` response header carries the URLs of the next "
                "and last pages, and its absence is what says the collection is exhausted.",
            "example_value": MaxPerPage,
        };

        #! Overlay shared by the \c page option of every listing action
        const PageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "short_desc": "Which page to return, counting from 1",
            "example_value": 1,
        };

        #! Overlay applied to the paging options of every listing action
        const PagingOverlay = {
            "per_page": PerPageOverlay,
            "page": PageOverlay,
        };

        #! Overlay applied to the owner and repository of every repository-scoped action
        const RepoScopeOverlay = {
            "owner": OwnerOverlay,
            "repo": RepoOverlay,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Repositories ---
            <RestSchemaActionInfo>{
                "action": "repos-get",
                "method": "GET",
                "path": "/repos/{owner}/{repo}",
                "operation_id": "repos/get",
                "display_name": "Get Repository",
                "short_desc": "Retrieve one repository",
                "desc": "Retrieves a repository with its description, default branch, visibility, "
                    "permissions and counts.\n\n"
                    "This is also how to find the `default_branch` a later action needs.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo"),
                "fields": RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-update",
                "method": "PATCH",
                "path": "/repos/{owner}/{repo}",
                "operation_id": "repos/update",
                "display_name": "Update Repository",
                "short_desc": "Change a repository's settings",
                "desc": "Updates a repository's settings.\n\n"
                    "Only the properties supplied are changed, so this is not a read-modify-write. "
                    "`visibility` moves a repository between public, private and internal; `archived` can be "
                    "set to `true` to archive it, and GitHub does **not** allow it to be set back to "
                    "`false`.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo", "name", "description", "homepage", "private", "visibility",
                    "default_branch", "archived"),
                "fields": RepoScopeOverlay + {
                    "default_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Default Branch",
                        "short_desc": "The branch to make the default",
                        "preselected": False,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-delete",
                "method": "DELETE",
                "path": "/repos/{owner}/{repo}",
                "operation_id": "repos/delete",
                "display_name": "Delete Repository",
                "short_desc": "Delete a repository permanently",
                "desc": "Deletes a repository.\n\n"
                    "**This cannot be undone.** GitHub keeps no trash for repositories; recovering one means "
                    "asking GitHub Support within 90 days. A classic token needs the `delete_repo` scope, "
                    "which is not one of the scopes this connection requests by default.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo"),
                "fields": RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-for-authenticated-user",
                "method": "GET",
                "path": "/user/repos",
                "operation_id": "repos/list-for-authenticated-user",
                "display_name": "List My Repositories",
                "short_desc": "List the repositories the credential can see",
                "desc": "Lists the repositories the authenticated user owns, collaborates on or can see "
                    "through an organization.\n\n"
                    "`visibility` and `affiliation` narrow it; `type` is a shorthand for common combinations "
                    "of the two and cannot be given together with them.\n\n"
                    "This is also what fills the **Owner** dropdown on every repository-scoped action.",
                "groups": (GroupRepositories,),
                "field_order": ("visibility", "affiliation", "type", "sort", "direction", "since", "before",
                    "per_page", "page"),
                "fields": PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-for-user",
                "method": "GET",
                "path": "/users/{username}/repos",
                "operation_id": "repos/list-for-user",
                "display_name": "List Repositories for Owner",
                "short_desc": "List the public repositories of one user or organization",
                "desc": "Lists the repositories belonging to one account, which may be a user or an "
                    "organization.\n\n"
                    "This is what fills the **Repository** dropdown once an owner has been chosen. The "
                    "application this module replaces filled that dropdown by searching for "
                    "`user:<owner>` instead, which is capped at 1000 results and counts against a separate, "
                    "much smaller search rate limit.",
                "groups": (GroupRepositories,),
                "field_order": ("username", "type", "sort", "direction", "per_page", "page"),
                "fields": PagingOverlay + {
                    "username": OwnerOverlay + <RestSchemaFieldOverlayInfo>{
                        "has_dependents": False,
                        "on_change": NOTHING,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-create-for-authenticated-user",
                "method": "POST",
                "path": "/user/repos",
                "operation_id": "repos/create-for-authenticated-user",
                "display_name": "Create Repository",
                "short_desc": "Create a repository owned by the authenticated user",
                "desc": "Creates a repository owned by the authenticated user.\n\n"
                    "`auto_init` creates an initial commit with a README, which matters because a repository "
                    "with no commits has no branches — so **Create Branch** and the file actions have "
                    "nothing to work from until something is committed.",
                "groups": (GroupRepositories,),
                "field_order": ("name", "description", "homepage", "private", "auto_init",
                    "gitignore_template", "license_template", "has_issues", "has_wiki"),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the new repository",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-create-in-org",
                "method": "POST",
                "path": "/orgs/{org}/repos",
                "operation_id": "repos/create-in-org",
                "display_name": "Create Repository in Organization",
                "short_desc": "Create a repository owned by an organization",
                "desc": "Creates a repository owned by an organization.\n\n"
                    "The credential needs permission to create repositories in that organization; "
                    "`visibility` is subject to whatever the organization allows.",
                "groups": (GroupRepositories, GroupOrganizations),
                "field_order": ("org", "name", "description", "homepage", "visibility", "private",
                    "auto_init", "gitignore_template", "license_template"),
                "fields": {
                    "org": OrgOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the new repository",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-for-org",
                "method": "GET",
                "path": "/orgs/{org}/repos",
                "operation_id": "repos/list-for-org",
                "display_name": "List Organization Repositories",
                "short_desc": "List the repositories of one organization",
                "desc": "Lists an organization's repositories. `type` selects public, private, forked, "
                    "sourced or member repositories.",
                "groups": (GroupRepositories, GroupOrganizations),
                "field_order": ("org", "type", "sort", "direction", "per_page", "page"),
                "fields": PagingOverlay + {
                    "org": OrgOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-contributors",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/contributors",
                "operation_id": "repos/list-contributors",
                "display_name": "List Contributors",
                "short_desc": "List everyone who has committed to a repository",
                "desc": "Lists contributors with a commit count each, most commits first.\n\n"
                    "A contributor is not necessarily someone who can be assigned work: GitHub only accepts "
                    "an assignee with push access. Use **List Collaborators** for that.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo", "anon", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-collaborators",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/collaborators",
                "operation_id": "repos/list-collaborators",
                "display_name": "List Collaborators",
                "short_desc": "List the users with access to a repository",
                "desc": "Lists the users with access to a repository and the permission each has.\n\n"
                    "This is what fills the assignee and reviewer dropdowns, because those are exactly the "
                    "users GitHub will accept. The credential needs push access to call it.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo", "affiliation", "permission", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-get-content",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/contents/{path}",
                "operation_id": "repos/get-content",
                "display_name": "Get File or Directory",
                "short_desc": "Read a file's contents, or list a directory",
                "desc": "Reads a path in a repository.\n\n"
                    "For a **file** the response carries the content Base64-encoded in `content`, together "
                    "with the `sha` that **Update File** and **Delete File** require. For a **directory** "
                    "the response is a list of entries, and no content.\n\n"
                    "`ref` selects a branch, tag or commit; without it GitHub reads the default branch. "
                    "GitHub serves files up to 1 MB this way.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo", "path", "ref"),
                "fields": RepoScopeOverlay + {
                    "path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Path",
                        "short_desc": "The path within the repository; empty for the root directory",
                        "preselected": True,
                    },
                    "ref": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ref",
                        "short_desc": "The branch, tag or commit to read from",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-create-or-update-file-contents",
                "method": "PUT",
                "path": "/repos/{owner}/{repo}/contents/{path}",
                "operation_id": "repos/create-or-update-file-contents",
                "display_name": "Create or Update File",
                "short_desc": "Commit a file, creating it or replacing it",
                "desc": "Writes a file and commits the change.\n\n"
                    "`content` must be **Base64-encoded**. Creating a file takes no `sha`; replacing one "
                    "**requires** the `sha` of the blob being replaced, which **Get File or Directory** "
                    "returns — that is what makes the write safe against a concurrent change.\n\n"
                    "Without `branch` the commit goes to the default branch.",
                "groups": (GroupRepositories,),
                # GitHub answers 201 when the file was created and 200 when it was updated, and both carry
                # the same "file-commit" body; either would describe the output, and one has to be named
                # because two success responses with a body are otherwise an ambiguous contract
                "response_code": "200",
                "field_order": ("owner", "repo", "path", "message", "content", "sha", "branch", "committer",
                    "author"),
                "fields": RepoScopeOverlay + {
                    "path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Path",
                        "short_desc": "The path of the file within the repository",
                        "preselected": True,
                    },
                    "message": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Commit Message",
                        "preselected": True,
                    },
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Content",
                        "short_desc": "The new file content, Base64-encoded",
                        "preselected": True,
                    },
                    "sha": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Blob SHA",
                        "short_desc": "Required when replacing a file: the SHA of the blob being replaced",
                    },
                    "branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The branch to commit to; the default branch if unset",
                        "preselected": False,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-delete-file",
                "method": "DELETE",
                "path": "/repos/{owner}/{repo}/contents/{path}",
                "operation_id": "repos/delete-file",
                "display_name": "Delete File",
                "short_desc": "Delete a file and commit the change",
                "desc": "Deletes a file and commits the deletion.\n\n"
                    "`sha` is **required** and must be the SHA of the blob being deleted, which "
                    "**Get File or Directory** returns.",
                "groups": (GroupRepositories,),
                "field_order": ("owner", "repo", "path", "message", "sha", "branch", "committer", "author"),
                "fields": RepoScopeOverlay + {
                    "path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Path",
                        "short_desc": "The path of the file within the repository",
                        "preselected": True,
                    },
                    "message": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Commit Message",
                        "preselected": True,
                    },
                    "sha": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Blob SHA",
                        "short_desc": "The SHA of the blob being deleted",
                        "preselected": True,
                    },
                    "branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The branch to commit to; the default branch if unset",
                        "preselected": False,
                    },
                },
            },

            # --- Issues ---
            <RestSchemaActionInfo>{
                "action": "issues-list-for-repo",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/issues",
                "operation_id": "issues/list-for-repo",
                "display_name": "List Issues",
                "short_desc": "List the issues in a repository",
                "desc": "Lists a repository's issues.\n\n"
                    "**GitHub returns pull requests here too**, because every pull request is an issue; a "
                    "record carrying `pull_request` is one. Filter them out if only issues are wanted.\n\n"
                    "`state` defaults to `open`, and `since` returns only issues updated after a given "
                    "time — which is what makes an incremental sweep possible.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "state", "assignee", "creator", "labels", "milestone",
                    "since", "sort", "direction", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay + {
                    "assignee": AssigneeOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Return only issues assigned to this user, or \"none\" or \"*\"",
                    },
                    "creator": AssigneeOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Creator",
                        "short_desc": "Return only issues opened by this user",
                        "desc": NOTHING,
                    },
                    "labels": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Labels",
                        "short_desc": "A comma-separated list of label names",
                        "desc": "A **comma-separated** list of label names, not a list — an issue must carry "
                            "all of them to be returned.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-list",
                "method": "GET",
                "path": "/issues",
                "operation_id": "issues/list",
                "display_name": "List My Issues",
                "short_desc": "List the issues assigned to the authenticated user",
                "desc": "Lists issues across every repository the authenticated user can see, filtered by "
                    "their relationship to them — assigned, created, mentioned or subscribed.\n\n"
                    "`pulls` controls whether pull requests are included; unlike **List Issues** they are "
                    "excluded by default.",
                "groups": (GroupIssues,),
                "field_order": ("filter", "state", "labels", "since", "sort", "direction", "pulls",
                    "per_page", "page"),
                "fields": PagingOverlay + {
                    "labels": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Labels",
                        "short_desc": "A comma-separated list of label names",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-get",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/issues/{issue_number}",
                "operation_id": "issues/get",
                "display_name": "Get Issue",
                "short_desc": "Retrieve one issue",
                "desc": "Retrieves an issue by number.\n\n"
                    "Giving a pull request's number returns that pull request as an issue, which is how "
                    "GitHub models it.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "issue_number"),
                "fields": RepoScopeOverlay + {
                    "issue_number": IssueOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-create",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/issues",
                "operation_id": "issues/create",
                "display_name": "Create Issue",
                "short_desc": "Open an issue in a repository",
                "desc": "Opens an issue.\n\n"
                    "Only `title` is required. `assignees` and `labels` are applied as part of the same "
                    "call, so no follow-up action is needed for either.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "title", "body", "assignees", "labels", "milestone"),
                "fields": RepoScopeOverlay + {
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "The issue body, in GitHub-flavoured Markdown",
                        "preselected": True,
                    },
                    "assignee": AssigneeOverlay,
                    "assignees": AssigneesOverlay,
                    "labels": LabelsOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-update",
                "method": "PATCH",
                "path": "/repos/{owner}/{repo}/issues/{issue_number}",
                "operation_id": "issues/update",
                "display_name": "Update Issue",
                "short_desc": "Change an issue, or close or reopen it",
                "desc": "Updates an issue.\n\n"
                    "Only the properties supplied are changed. Setting `state` to `closed` closes it and "
                    "`open` reopens it; `state_reason` records **why** it was closed — `completed`, "
                    "`not_planned` or `duplicate`.\n\n"
                    "`labels` and `assignees` **replace** what is there rather than adding to it; use "
                    "**Add Assignees** to add without replacing.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "issue_number", "title", "body", "state", "state_reason",
                    "assignees", "labels", "milestone"),
                "fields": RepoScopeOverlay + {
                    "issue_number": IssueOverlay,
                    "assignee": AssigneeOverlay,
                    "assignees": AssigneesOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The logins to assign; this replaces the current assignees",
                    },
                    "labels": LabelsOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The labels to apply; this replaces the current labels",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-add-assignees",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/issues/{issue_number}/assignees",
                "operation_id": "issues/add-assignees",
                "display_name": "Add Assignees",
                "short_desc": "Add users to an issue's assignees",
                "desc": "Adds assignees to an issue, keeping the ones already there.\n\n"
                    "GitHub silently ignores a user without push access, and an issue can carry at most 10 "
                    "assignees.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "issue_number", "assignees"),
                "fields": RepoScopeOverlay + {
                    "issue_number": IssueOverlay,
                    "assignees": AssigneesOverlay + <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-remove-assignees",
                "method": "DELETE",
                "path": "/repos/{owner}/{repo}/issues/{issue_number}/assignees",
                "operation_id": "issues/remove-assignees",
                "display_name": "Remove Assignees",
                "short_desc": "Remove users from an issue's assignees",
                "desc": "Removes assignees from an issue, leaving the rest in place.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "issue_number", "assignees"),
                "fields": RepoScopeOverlay + {
                    "issue_number": IssueOverlay,
                    "assignees": AssigneesOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The logins to remove from the assignees",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "issues-list-labels-for-repo",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/labels",
                "operation_id": "issues/list-labels-for-repo",
                "display_name": "List Labels",
                "short_desc": "List the labels defined in a repository",
                "desc": "Lists a repository's labels with their colours and descriptions.\n\n"
                    "This is what backs the label dropdowns on the issue actions; the application this "
                    "module replaces offered those dropdowns without exporting the endpoint that fills "
                    "them.",
                "groups": (GroupIssues,),
                "field_order": ("owner", "repo", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },

            # --- Pull Requests ---
            <RestSchemaActionInfo>{
                "action": "pulls-list",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/pulls",
                "operation_id": "pulls/list",
                "display_name": "List Pull Requests",
                "short_desc": "List the pull requests in a repository",
                "desc": "Lists pull requests.\n\n"
                    "`state` defaults to `open`. `head` selects pull requests from one branch and must be "
                    "written as `owner:branch`; `base` selects the branch being merged into and is a plain "
                    "branch name.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "state", "base", "head", "sort", "direction", "per_page",
                    "page"),
                "fields": RepoScopeOverlay + PagingOverlay + {
                    "base": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Branch",
                        "short_desc": "Return only pull requests targeting this branch",
                        "preselected": False,
                    },
                    "head": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Head",
                        "short_desc": "Return only pull requests from this branch, as \"owner:branch\"",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-get",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/pulls/{pull_number}",
                "operation_id": "pulls/get",
                "display_name": "Get Pull Request",
                "short_desc": "Retrieve one pull request",
                "desc": "Retrieves a pull request, including the merge state and the file, commit and "
                    "review counts that the listing does not carry.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "pull_number"),
                "fields": RepoScopeOverlay + {
                    "pull_number": PullOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-create",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/pulls",
                "operation_id": "pulls/create",
                "display_name": "Create Pull Request",
                "short_desc": "Open a pull request between two branches",
                "desc": "Opens a pull request.\n\n"
                    "`head` is the branch the changes are on and `base` the branch they are for. For a pull "
                    "request **from a fork**, write `head` as `owner:branch`.\n\n"
                    "`draft` opens it as a draft. `issue` converts an existing issue into a pull request "
                    "instead of writing a title and body, and cannot be given together with `title`.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "title", "head", "base", "body", "draft",
                    "maintainer_can_modify"),
                "fields": RepoScopeOverlay + {
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                    "head": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Head Branch",
                        "short_desc": "The branch the changes are on; \"owner:branch\" from a fork",
                    },
                    "base": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Branch",
                        "short_desc": "The branch the changes are for",
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "The pull request body, in GitHub-flavoured Markdown",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-update",
                "method": "PATCH",
                "path": "/repos/{owner}/{repo}/pulls/{pull_number}",
                "operation_id": "pulls/update",
                "display_name": "Update Pull Request",
                "short_desc": "Change a pull request, or close or reopen it",
                "desc": "Updates a pull request.\n\n"
                    "Setting `state` to `closed` closes it without merging. `base` retargets it at a "
                    "different branch, which GitHub only accepts while it is open.\n\n"
                    "This does not merge: merging is a separate endpoint this application does not export.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "pull_number", "title", "body", "state", "base",
                    "maintainer_can_modify"),
                "fields": RepoScopeOverlay + {
                    "pull_number": PullOverlay,
                    "base": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Base Branch",
                        "short_desc": "Retarget the pull request at this branch",
                        "preselected": False,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-list-requested-reviewers",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/pulls/{pull_number}/requested_reviewers",
                "operation_id": "pulls/list-requested-reviewers",
                "display_name": "List Requested Reviewers",
                "short_desc": "List who has been asked to review a pull request",
                "desc": "Lists the users and teams whose review has been requested and **not yet given**: "
                    "GitHub removes a reviewer from this list once they review.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "pull_number"),
                "fields": RepoScopeOverlay + {
                    "pull_number": PullOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-request-reviewers",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/pulls/{pull_number}/requested_reviewers",
                "operation_id": "pulls/request-reviewers",
                "display_name": "Request Reviewers",
                "short_desc": "Ask users or teams to review a pull request",
                "desc": "Requests reviews on a pull request.\n\n"
                    "At least one of `reviewers` and `team_reviewers` must be given. A reviewer needs read "
                    "access to the repository, and the author of the pull request cannot be one.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "pull_number", "reviewers", "team_reviewers"),
                "fields": RepoScopeOverlay + {
                    "pull_number": PullOverlay,
                    "reviewers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reviewers",
                        "short_desc": "The logins of the users to ask",
                        "element_ref_data": RefCollaborator,
                        "supports_custom_values": True,
                        "depends_on": ("owner", "repo"),
                        "preselected": True,
                    },
                    "team_reviewers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Team Reviewers",
                        "short_desc": "The slugs of the teams to ask",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "pulls-remove-requested-reviewers",
                "method": "DELETE",
                "path": "/repos/{owner}/{repo}/pulls/{pull_number}/requested_reviewers",
                "operation_id": "pulls/remove-requested-reviewers",
                "display_name": "Remove Requested Reviewers",
                "short_desc": "Withdraw a review request",
                "desc": "Withdraws review requests from a pull request, leaving the rest in place.",
                "groups": (GroupPullRequests,),
                "field_order": ("owner", "repo", "pull_number", "reviewers", "team_reviewers"),
                "fields": RepoScopeOverlay + {
                    "pull_number": PullOverlay,
                    "reviewers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reviewers",
                        "short_desc": "The logins of the users whose request is withdrawn",
                        "element_ref_data": RefCollaborator,
                        "supports_custom_values": True,
                        "depends_on": ("owner", "repo"),
                        "preselected": True,
                    },
                    "team_reviewers": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Team Reviewers",
                        "short_desc": "The slugs of the teams whose request is withdrawn",
                    },
                },
            },

            # --- Branches and Commits ---
            <RestSchemaActionInfo>{
                "action": "repos-list-branches",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/branches",
                "operation_id": "repos/list-branches",
                "display_name": "List Branches",
                "short_desc": "List a repository's branches",
                "desc": "Lists branches with the commit each points at.\n\n"
                    "`protected` narrows the listing to protected branches, or to unprotected ones.",
                "groups": (GroupCode,),
                "field_order": ("owner", "repo", "protected", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-get-branch",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/branches/{branch}",
                "operation_id": "repos/get-branch",
                "display_name": "Get Branch",
                "short_desc": "Retrieve one branch",
                "desc": "Retrieves a branch with its head commit and its protection settings.\n\n"
                    "The head commit's `sha` is what **Create Branch** needs to branch from it.",
                "groups": (GroupCode,),
                "field_order": ("owner", "repo", "branch"),
                "fields": RepoScopeOverlay + {
                    "branch": BranchOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "git-create-ref",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/git/refs",
                "operation_id": "git/create-ref",
                "display_name": "Create Branch or Tag",
                "short_desc": "Create a git ref pointing at a commit",
                "desc": "Creates a git reference, which is how a branch or a tag is made.\n\n"
                    "`ref` is the **fully qualified** name — `refs/heads/my-branch` for a branch, "
                    "`refs/tags/v1.0` for a tag — and a bare branch name is rejected. `sha` is the commit "
                    "the new ref points at, which **Get Branch** returns for the branch being started from.",
                "groups": (GroupCode,),
                "field_order": ("owner", "repo", "ref", "sha"),
                "fields": RepoScopeOverlay + {
                    "ref": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ref",
                        "short_desc": "The fully qualified ref, ex: \"refs/heads/my-branch\"",
                        "example_value": "refs/heads/my-branch",
                        "preselected": True,
                    },
                    "sha": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Commit SHA",
                        "short_desc": "The commit the new ref points at",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repos-list-commits",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/commits",
                "operation_id": "repos/list-commits",
                "display_name": "List Commits",
                "short_desc": "List a repository's commits",
                "desc": "Lists commits, newest first.\n\n"
                    "`sha` selects the branch, tag or commit to start from — despite the name it accepts a "
                    "branch name — and `path` narrows the listing to commits touching one file or "
                    "directory. `since` and `until` bound it by commit date, which is what makes an "
                    "incremental sweep possible.",
                "groups": (GroupCode,),
                "field_order": ("owner", "repo", "sha", "path", "author", "committer", "since", "until",
                    "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay + {
                    "sha": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Branch or Commit",
                        "short_desc": "The branch, tag or commit to list from; the default branch if unset",
                        "ref_data": RefBranch,
                        "supports_custom_values": True,
                        "depends_on": ("owner", "repo"),
                    },
                },
            },

            # --- Releases ---
            <RestSchemaActionInfo>{
                "action": "repos-list-releases",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/releases",
                "operation_id": "repos/list-releases",
                "display_name": "List Releases",
                "short_desc": "List a repository's releases",
                "desc": "Lists releases, newest first.\n\n"
                    "Drafts are included only for a credential that can see them. A git tag with no release "
                    "attached does not appear here.",
                "groups": (GroupReleases,),
                "field_order": ("owner", "repo", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repos-create-release",
                "method": "POST",
                "path": "/repos/{owner}/{repo}/releases",
                "operation_id": "repos/create-release",
                "display_name": "Create Release",
                "short_desc": "Publish a release for a tag",
                "desc": "Creates a release.\n\n"
                    "`tag_name` is required. If the tag does not exist GitHub creates it, at "
                    "`target_commitish` — a branch name or commit SHA, defaulting to the default branch.\n\n"
                    "`generate_release_notes` writes the notes from the commits and pull requests since the "
                    "previous release, and `draft` keeps it unpublished until it is edited in the UI.",
                "groups": (GroupReleases,),
                "field_order": ("owner", "repo", "tag_name", "name", "body", "target_commitish", "draft",
                    "prerelease", "generate_release_notes", "make_latest"),
                "fields": RepoScopeOverlay + {
                    "tag_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tag",
                        "short_desc": "The git tag for the release, created if it does not exist",
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The release title",
                        "preselected": True,
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Release Notes",
                        "short_desc": "The release notes, in GitHub-flavoured Markdown",
                        "preselected": True,
                    },
                    "target_commitish": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target",
                        "short_desc": "The branch or commit to tag; only used when the tag does not exist",
                        "ref_data": RefBranch,
                        "supports_custom_values": True,
                        "depends_on": ("owner", "repo"),
                    },
                },
            },

            # --- Organizations ---
            <RestSchemaActionInfo>{
                "action": "orgs-list-for-authenticated-user",
                "method": "GET",
                "path": "/user/orgs",
                "operation_id": "orgs/list-for-authenticated-user",
                "display_name": "List My Organizations",
                "short_desc": "List the organizations the credential belongs to",
                "desc": "Lists the organizations the authenticated user is a member of.\n\n"
                    "This is what fills the **Organization** dropdown; the application this module replaces "
                    "offered that dropdown without exporting the endpoint that fills it.\n\n"
                    "An OAuth credential only sees organizations that have approved the OAuth app, and a "
                    "classic token needs the `read:org` scope to see them all.",
                "groups": (GroupOrganizations,),
                "field_order": ("per_page", "page"),
                "fields": PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "orgs-list-members",
                "method": "GET",
                "path": "/orgs/{org}/members",
                "operation_id": "orgs/list-members",
                "display_name": "List Organization Members",
                "short_desc": "List the members of an organization",
                "desc": "Lists an organization's members.\n\n"
                    "A credential that is not itself a member sees only public members. `filter` can "
                    "restrict the listing to members without two-factor authentication, and `role` to "
                    "administrators.",
                "groups": (GroupOrganizations,),
                "field_order": ("org", "filter", "role", "per_page", "page"),
                "fields": PagingOverlay + {
                    "org": OrgOverlay,
                },
            },

            # --- Search ---
            <RestSchemaActionInfo>{
                "action": "search-repos",
                "method": "GET",
                "path": "/search/repositories",
                "operation_id": "search/repos",
                "display_name": "Search Repositories",
                "short_desc": "Search public repositories with a query",
                "desc": "Searches repositories.\n\n"
                    "`q` takes GitHub's search syntax — `qore user:qoretechnologies`, "
                    "`language:python stars:>100`. The response is `{total_count, items}` rather than a bare "
                    "array.\n\n"
                    "Search is capped at **1000 results** however far it is paged, and is rate-limited "
                    "separately from the rest of the API: 30 requests a minute for an authenticated "
                    "credential. Use **List Repositories for Owner** when the owner is known.",
                "groups": (GroupSearch, GroupRepositories),
                "field_order": ("q", "sort", "order", "per_page", "page"),
                "fields": PagingOverlay + {
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "The search query, in GitHub's search syntax",
                        "example_value": "qore user:qoretechnologies",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-issues-and-pull-requests",
                "method": "GET",
                "path": "/search/issues",
                "operation_id": "search/issues-and-pull-requests",
                "display_name": "Search Issues and Pull Requests",
                "short_desc": "Search issues and pull requests with a query",
                "desc": "Searches issues and pull requests across repositories.\n\n"
                    "`q` takes GitHub's search syntax — `repo:owner/name is:open label:bug`. Add "
                    "`is:issue` or `is:pr` to separate the two, because both are returned. The response is "
                    "`{total_count, items}`.\n\n"
                    "As with repository search this is capped at **1000 results** and rate-limited "
                    "separately. `advanced_search` opts into GitHub's newer query engine.",
                "groups": (GroupSearch, GroupIssues),
                "field_order": ("q", "advanced_search", "sort", "order", "per_page", "page"),
                "fields": PagingOverlay + {
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "The search query, in GitHub's search syntax",
                        "example_value": "repo:qoretechnologies/qorus is:open label:bug",
                        "preselected": True,
                    },
                },
            },

            # --- Actions ---
            <RestSchemaActionInfo>{
                "action": "actions-list-repo-workflows",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/actions/workflows",
                "operation_id": "actions/list-repo-workflows",
                "display_name": "List Workflows",
                "short_desc": "List a repository's GitHub Actions workflows",
                "desc": "Lists the workflows defined in `.github/workflows`, with the state and file path of "
                    "each. The response is `{total_count, workflows}`.",
                "groups": (GroupActions,),
                "field_order": ("owner", "repo", "per_page", "page"),
                "fields": RepoScopeOverlay + PagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "actions-get-repo-public-key",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/actions/secrets/public-key",
                "operation_id": "actions/get-repo-public-key",
                "display_name": "Get Secrets Public Key",
                "short_desc": "Retrieve the key a secret must be encrypted with",
                "desc": "Retrieves the repository's public key for Actions secrets, as `{key_id, key}`.\n\n"
                    "This is the **first step of writing a secret**: GitHub never accepts a secret in the "
                    "clear, so the value has to be sealed with this key using libsodium's sealed-box "
                    "construction, and `key_id` sent alongside it.",
                "groups": (GroupActions,),
                "field_order": ("owner", "repo"),
                "fields": RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "actions-get-repo-secret",
                "method": "GET",
                "path": "/repos/{owner}/{repo}/actions/secrets/{secret_name}",
                "operation_id": "actions/get-repo-secret",
                "display_name": "Get Secret",
                "short_desc": "Check whether a secret exists, and when it changed",
                "desc": "Retrieves a secret's name and timestamps.\n\n"
                    "**The value is never returned** — no GitHub API reads a secret back — so this answers "
                    "whether the secret exists and when it last changed, and nothing else.",
                "groups": (GroupActions,),
                "field_order": ("owner", "repo", "secret_name"),
                "fields": RepoScopeOverlay + {
                    "secret_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Secret Name",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "actions-create-or-update-repo-secret",
                "method": "PUT",
                "path": "/repos/{owner}/{repo}/actions/secrets/{secret_name}",
                "operation_id": "actions/create-or-update-repo-secret",
                "display_name": "Create or Update Secret",
                "short_desc": "Write an encrypted Actions secret",
                "desc": "Creates or replaces a repository secret for GitHub Actions.\n\n"
                    "`encrypted_value` is **not the secret**: it is the secret sealed with the repository's "
                    "public key, which **Get Secrets Public Key** returns, and `key_id` names the key that "
                    "was used. Sending a plain value stores a secret whose contents are that ciphertext-"
                    "shaped string, and every workflow using it fails.",
                "groups": (GroupActions,),
                "field_order": ("owner", "repo", "secret_name", "encrypted_value", "key_id"),
                "fields": RepoScopeOverlay + {
                    "secret_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Secret Name",
                        "preselected": True,
                    },
                    "encrypted_value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Encrypted Value",
                        "short_desc": "The secret sealed with the repository's public key, Base64-encoded",
                        "sensitive": True,
                        "preselected": True,
                    },
                    "key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Key ID",
                        "short_desc": "The ID of the public key the value was sealed with",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "actions-delete-repo-secret",
                "method": "DELETE",
                "path": "/repos/{owner}/{repo}/actions/secrets/{secret_name}",
                "operation_id": "actions/delete-repo-secret",
                "display_name": "Delete Secret",
                "short_desc": "Remove an Actions secret",
                "desc": "Deletes a repository secret. Any workflow still referring to it sees an empty value.",
                "groups": (GroupActions,),
                "field_order": ("owner", "repo", "secret_name"),
                "fields": RepoScopeOverlay + {
                    "secret_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Secret Name",
                        "preselected": True,
                    },
                },
            },
        );
    }
}
}
