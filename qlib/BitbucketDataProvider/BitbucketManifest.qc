# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitbucketDataProvider module definition

/*  BitbucketManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the Bitbucket application exports, and how they are presented
/** @section bitbucketmanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, and they are **not**
    tidy.  That application derived each ID from the \c operationId of a vendored Swagger 2.0 document,
    falling back to a mangling of the path and method when the document declared none — which, for
    Bitbucket, is most of them.  So five actions are named \c "getReportsForCommit",
    \c "getDeploymentsForRepository", \c "searchTeam", \c "searchAccount" and \c "searchWorkspace", and the
    rest are named \c "repositories_workspace_repo_slug_pullrequests_get".

    Action identity is what existing workflows call, so these are preserved exactly rather than restyled.
    Where the current schema declares an \c operationId it is recorded and checked for drift; where it
    does not, there is nothing to check and the entry omits it.

    @section bitbucketmanifest_added What This Application Adds

    One read-only listing is exported that the TypeScript application did not:
    \c "repositories_workspace_repo_slug_refs_branches_get".  That application offered branch dropdowns on
    the pull request actions, filled by an endpoint it never exported; a dropdown here is produced by
    executing an action **in this manifest**, so exporting the listing is what makes those dropdowns work.

    @section bitbucketmanifest_withdrawn What Bitbucket Withdrew

    Bitbucket removed every cross-workspace listing — \c "GET /2.0/workspaces",
    \c "GET /2.0/repositories" and the \c "/2.0/user/permissions/..." family all answer **410 Gone**,
    \c "CHANGE-2770".  Verified against the live API, along with the fact that
    \c "GET /2.0/user/workspaces" is the one replacement.

    |!Action|!What happened
    |\c workspaces_get|repointed from \c "GET /workspaces" to \c "GET /user/workspaces"; the ID is unchanged and so is what it means
    |\c repositories_get|**withdrawn**: it listed public repositories across all of Bitbucket, and nothing replaces that

    Repointing rather than renaming is the whole point of an action ID being a product commitment: the
    operation behind \c workspaces_get changed, and the name workflows call did not.  The withdrawn one is
    a different case — the endpoint is gone and Atlassian's own guidance is to list the workspaces and ask
    each one, which \c repositories_workspace_get already does.

    A \c "/user/workspaces" record wraps its workspace and carries no display name, so the workspace
    dropdown reads \c "workspace.slug" for both the value and the label.

    @section bitbucketmanifest_nesting Branches and Reviewers Are Presented Flat

    A pull request declares its branches as \c "source.branch.name" and its reviewers as a list of objects
    each carrying a \c uuid.  Asking a user to fill in three levels of nesting to name a branch is not a
    usable form, so those values are presented as a plain branch name and a plain list of member IDs, and
    converted on the way to the wire by @ref BitbucketCodecs "codecs this application registers".

    A codec was used rather than a request transform because it is **declarative and reversible**: the
    manifest names it, the field is published with the codec's own type, and the value converts in both
    directions, so a response reads back as the same flat value the request took.

    @section bitbucketmanifest_envelope Every Collection Is the Same Envelope

    Bitbucket returns every collection as \c {values, pagelen, size, next, previous}.  A listing action
    returns that envelope whole, because \c next is the only way to reach the following page — Bitbucket
    does not always report \c size, so paging on a count does not work.  \c pagelen is capped at 100.
*/
public class BitbucketManifest {
    public {
        #! Action group: repository commits and commit comments
        const GroupCommits = "Commits";

        #! Action group: deployments
        const GroupDeployments = "Deployments";

        #! Action group: pull requests and their comments
        const GroupPullRequests = "Pull Requests";

        #! Action group: repositories and branches
        const GroupRepositories = "Repositories";

        #! Action group: code search
        const GroupSearch = "Search";

        #! Action group: workspaces, their members and projects
        const GroupWorkspaces = "Workspaces";

        #! Reference data name: workspaces
        const RefWorkspace = "bitbucket-workspace";

        #! Reference data name: repositories in the selected workspace
        const RefRepository = "bitbucket-repository";

        #! Reference data name: commits in the selected repository
        const RefCommit = "bitbucket-commit";

        #! Reference data name: branches in the selected repository
        const RefBranch = "bitbucket-branch";

        #! Reference data name: pull requests in the selected repository
        const RefPullRequest = "bitbucket-pull-request";

        #! Reference data name: comments on the selected pull request
        const RefPullRequestComment = "bitbucket-pull-request-comment";

        #! Reference data name: members of the selected workspace
        const RefWorkspaceMember = "bitbucket-workspace-member";

        #! Bitbucket's maximum page size for a collection
        const MaxPageLen = BitbucketRestClient::BitbucketRestClientBase::MaxPageLen;

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.

            Everything below a workspace or a repository declares \c options_from, so the listing is scoped
            to the container the user picked rather than offering values from somewhere else.  The records
            are read from Bitbucket's \c values envelope.
        */
        const ReferenceData = {
            RefWorkspace: <RestSchemaReferenceDataInfo>{
                "name": RefWorkspace,
                "action": "workspaces_get",
                "records_path": "values",
                # a record is a membership wrapping its workspace, and carries no display name of its own
                "value_field": "workspace.slug",
                "display_field": "workspace.slug",
                "desc": "workspaces the credential can see",
            },
            RefRepository: <RestSchemaReferenceDataInfo>{
                "name": RefRepository,
                "action": "repositories_workspace_get",
                "options_from": {"workspace": "workspace"},
                "records_path": "values",
                "value_field": "slug",
                "display_field": "full_name",
                "desc": "repositories in the selected workspace",
            },
            RefCommit: <RestSchemaReferenceDataInfo>{
                "name": RefCommit,
                "action": "repositories_workspace_repo_slug_commits_get",
                "options_from": {"workspace": "workspace", "repo_slug": "repo_slug"},
                "records_path": "values",
                "value_field": "hash",
                "display_field": "message",
                "desc": "the most recent commits in the selected repository",
            },
            RefBranch: <RestSchemaReferenceDataInfo>{
                "name": RefBranch,
                "action": "repositories_workspace_repo_slug_refs_branches_get",
                "options_from": {"workspace": "workspace", "repo_slug": "repo_slug"},
                "records_path": "values",
                "value_field": "name",
                "desc": "branches in the selected repository",
            },
            RefPullRequest: <RestSchemaReferenceDataInfo>{
                "name": RefPullRequest,
                "action": "repositories_workspace_repo_slug_pullrequests_get",
                "options_from": {"workspace": "workspace", "repo_slug": "repo_slug"},
                "records_path": "values",
                "value_field": "id",
                "display_field": "title",
                "desc": "pull requests in the selected repository",
            },
            RefPullRequestComment: <RestSchemaReferenceDataInfo>{
                "name": RefPullRequestComment,
                "action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_get",
                "options_from": {
                    "workspace": "workspace",
                    "repo_slug": "repo_slug",
                    "pull_request_id": "pull_request_id",
                },
                "records_path": "values",
                "value_field": "id",
                # a comment's text is nested under "content"; the raw Markdown identifies it best
                "display_field": "content.raw",
                "desc": "comments on the selected pull request",
            },
            RefWorkspaceMember: <RestSchemaReferenceDataInfo>{
                "name": RefWorkspaceMember,
                "action": "workspaces_workspace_members_get",
                "options_from": {"workspace": "workspace"},
                "records_path": "values",
                # a membership record carries the member below "user", not at the top level
                "value_field": "user.uuid",
                "display_field": "user.display_name",
                "desc": "members of the selected workspace",
            },
        };

        #! Overlay for the workspace the action works in
        const WorkspaceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Workspace",
            "short_desc": "The workspace to work in",
            "desc": "The workspace to work in, given as its slug or as its UUID in braces.\n\n"
                "Everything else scoped by a workspace — repositories, members — is listed from this "
                "workspace, so changing it re-reads them.",
            "ref_data": RefWorkspace,
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for the repository the action works on
        const RepositoryOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Repository",
            "short_desc": "The repository to work on",
            "desc": "The repository to work on, given as its slug or as its UUID in braces.\n\n"
                "Everything else scoped by a repository — commits, branches, pull requests — is listed "
                "from this repository, so changing it re-reads them.",
            "ref_data": RefRepository,
            "depends_on": ("workspace",),
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for a commit in the selected repository
        const CommitOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Commit",
            "short_desc": "A commit hash, a branch name or a tag",
            "ref_data": RefCommit,
            "depends_on": ("workspace", "repo_slug"),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a pull request in the selected repository
        const PullRequestOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Pull Request",
            "short_desc": "The pull request ID, as shown in the web UI",
            "ref_data": RefPullRequest,
            "depends_on": ("workspace", "repo_slug"),
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for a comment on the selected pull request
        const PullRequestCommentOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Comment",
            "ref_data": RefPullRequestComment,
            "depends_on": ("workspace", "repo_slug", "pull_request_id"),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the source branch of a pull request
        const SourceBranchOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Source Branch",
            "short_desc": "The branch holding the changes",
            "desc": "The branch holding the changes to merge.\n\n"
                "Bitbucket declares this as a nested object; it is presented here as the branch name "
                "alone and sent in the shape the API expects.",
            "ref_data": RefBranch,
            "depends_on": ("workspace", "repo_slug"),
            "supports_custom_values": True,
            "preselected": True,
            "codec": BitbucketCodecs::BranchRef,
        };

        #! Overlay for the destination branch of a pull request
        const DestinationBranchOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Destination Branch",
            "short_desc": "The branch the changes are merged into",
            "desc": "The branch the changes are merged into; the repository's main branch when unset.\n\n"
                "Bitbucket declares this as a nested object; it is presented here as the branch name "
                "alone and sent in the shape the API expects.",
            "ref_data": RefBranch,
            "depends_on": ("workspace", "repo_slug"),
            "supports_custom_values": True,
            "preselected": True,
            "codec": BitbucketCodecs::BranchRef,
        };

        #! Overlay for the reviewers of a pull request
        const ReviewersOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Reviewers",
            "short_desc": "The workspace members to request a review from",
            "desc": "The members to request a review from, as their account UUIDs.\n\n"
                "Bitbucket declares this as a list of user objects; it is presented here as a list of "
                "IDs and sent in the shape the API expects.",
            "element_ref_data": RefWorkspaceMember,
            "depends_on": ("workspace",),
            "supports_custom_values": True,
            "codec": BitbucketCodecs::ReviewerIds,
        };

        #! Fields every entity carries that a request must not set
        /** Bitbucket composes every entity on a shared base carrying a \c type discriminator, and its
            create and update bodies are declared as the **entity itself** rather than as a separate
            request shape.  So an action generated straight from the schema would offer the reader's view
            of the object — who created it, when, how many comments it has — as things to fill in.

            These are dropped from the request.  They are server-owned: sending them is at best ignored
            and at worst rejected.
        */
        const ServerOwnedOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
        };

        #! Overlay for an entity's \c type discriminator
        /** Bitbucket composes every entity on a base that declares \c type, and declares it **required**,
            so it cannot be dropped from a request the way the other server-owned fields are: the schema
            would reject the request before it was sent.  It is given the entity's own discriminator as a
            default instead, so it is always correct and nobody has to fill it in.
        */
        const PullRequestTypeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Type",
            "short_desc": "Bitbucket's object discriminator; always \"pullrequest\"",
            "default_value": "pullrequest",
            "allowed_values": ("pullrequest",),
        };

        #! Overlay for the \c type discriminator of a pull request comment
        const PullRequestCommentTypeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Type",
            "short_desc": "Bitbucket's object discriminator; always \"pullrequest_comment\"",
            "default_value": "pullrequest_comment",
            "allowed_values": ("pullrequest_comment",),
        };

        #! Overlay for the \c type discriminator of a commit comment
        const CommitCommentTypeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Type",
            "short_desc": "Bitbucket's object discriminator; always \"commit_comment\"",
            "default_value": "commit_comment",
            "allowed_values": ("commit_comment",),
        };

        #! Overlay for the \c type discriminator of a repository
        const RepositoryTypeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Type",
            "short_desc": "Bitbucket's object discriminator; always \"repository\"",
            "default_value": "repository",
            "allowed_values": ("repository",),
        };

        #! Overlay shared by the \c pagelen option of every listing action
        const PageLenOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Length",
            "short_desc": "How many records to return, up to 100",
            "desc": "How many records to return in one request, up to Bitbucket's maximum of `100`.\n\n"
                "`next` in the response is the URL of the following page; page on its presence rather "
                "than on `size`, which Bitbucket does not report for every collection.",
            "preselected": True,
            "example_value": 50,
        };

        #! Overlay shared by the \c page option of every listing action
        const PageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "short_desc": "Which page of results to return, starting at 1",
            "preselected": True,
        };

        #! Overlay shared by the \c q filter of every listing action that offers one
        const QueryOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Filter",
            "short_desc": "A Bitbucket filter expression, ex: 'state = \"OPEN\"'",
            "desc": "A Bitbucket [filter expression](https://developer.atlassian.com/cloud/bitbucket/"
                "rest/intro/#filtering), ex: `state = \"OPEN\"` or `updated_on > 2026-01-01`.\n\n"
                "The fields a filter may name differ per collection and are documented with the "
                "endpoint.",
        };

        #! Overlay shared by the \c sort option of every listing action that offers one
        const SortOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Sort By",
            "short_desc": "A field to sort by; prefix with \"-\" to reverse",
            "example_value": "-updated_on",
        };

        #! Overlay applied to the paging options of every listing action
        const PagingOverlay = {
            "page": PageOverlay,
            "pagelen": PageLenOverlay,
        };

        #! Overlay applied to the workspace and repository a repository action works on
        const RepoScopeOverlay = {
            "workspace": WorkspaceOverlay,
            "repo_slug": RepositoryOverlay,
        };

        #! The fields of an entity that a create or update request must not set
        /** Bitbucket declares its create and update bodies as the entity itself, so these would otherwise
            be offered as things to fill in.  @see @ref ServerOwnedOverlay
        */
        const ServerOwnedEntityFields = ("type", "links", "created_on", "updated_on", "id", "uuid");

        #! The operations the application exports
        const Manifest = (
            # --- Repositories ---
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_get",
                "method": "GET",
                "path": "/repositories/{workspace}",
                "display_name": "List Workspace Repositories",
                "short_desc": "List the repositories in a workspace",
                "desc": "Lists the repositories in a workspace.\n\n"
                    "`role` restricts the listing to repositories the credential has a given relationship "
                    "with — `member`, `contributor`, `admin` or `owner` — which is how a listing is kept "
                    "to what the user actually works on.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "q", "role", "sort", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "workspace": WorkspaceOverlay,
                    "q": QueryOverlay,
                    "sort": SortOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}",
                "display_name": "Get Repository",
                "short_desc": "Retrieve one repository",
                "desc": "Retrieves a repository, including its main branch, project, size and visibility.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "repo_slug"),
                "fields": RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_post",
                "method": "POST",
                "path": "/repositories/{workspace}/{repo_slug}",
                "display_name": "Create Repository",
                "short_desc": "Create a repository",
                "desc": "Creates a repository at the given workspace and slug.\n\n"
                    "Unusually, the **URL decides the name**: the repository is created at the slug in the "
                    "path, so there is no name to supply in the body. It is private unless `is_private` "
                    "says otherwise.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "repo_slug", "description", "is_private", "fork_policy",
                    "language", "has_issues", "has_wiki", "project"),
                "fields": {
                    "workspace": WorkspaceOverlay,
                    "repo_slug": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Repository Slug",
                        "short_desc": "The slug to create the repository at",
                        "desc": "The slug the repository is created at, which also decides its name",
                        "preselected": True,
                    },
                    "is_private": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Private",
                        "preselected": True,
                    },
                    "type": RepositoryTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "uuid": ServerOwnedOverlay,
                    "size": ServerOwnedOverlay,
                    "owner": ServerOwnedOverlay,
                    "full_name": ServerOwnedOverlay,
                    "parent": ServerOwnedOverlay,
                    "scm": ServerOwnedOverlay,
                    "mainbranch": ServerOwnedOverlay,
                    "name": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_put",
                "method": "PUT",
                "path": "/repositories/{workspace}/{repo_slug}",
                "display_name": "Update Repository",
                "short_desc": "Update a repository's settings",
                # Bitbucket declares both 200 and 201 with a body here, because the same endpoint creates
                # the repository when it does not exist; the updated repository is the output either way
                "response_code": "200",
                "desc": "Updates a repository's description, visibility, fork policy, language and the "
                    "features it has enabled.\n\n"
                    "Only the fields supplied are changed.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "repo_slug", "description", "is_private", "fork_policy",
                    "language", "has_issues", "has_wiki", "project"),
                "fields": RepoScopeOverlay + {
                    "type": RepositoryTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "uuid": ServerOwnedOverlay,
                    "size": ServerOwnedOverlay,
                    "owner": ServerOwnedOverlay,
                    "full_name": ServerOwnedOverlay,
                    "parent": ServerOwnedOverlay,
                    "scm": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_delete",
                "method": "DELETE",
                "path": "/repositories/{workspace}/{repo_slug}",
                "display_name": "Delete Repository",
                "short_desc": "Delete a repository permanently",
                "desc": "Deletes a repository, its pull requests and its issues.\n\n"
                    "**This is immediate and permanent**; Bitbucket has no retention window for a deleted "
                    "repository. `redirect_to` names a repository to redirect the old URL to.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "repo_slug", "redirect_to"),
                "fields": RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_refs_branches_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/refs/branches",
                "display_name": "List Branches",
                "short_desc": "List a repository's branches",
                "desc": "Lists the branches of a repository with the commit each one points at.\n\n"
                    "This is what backs the branch dropdowns on the pull request actions; the application "
                    "this module replaces offered those dropdowns without exporting the endpoint that "
                    "fills them.",
                "groups": (GroupRepositories,),
                "field_order": ("workspace", "repo_slug", "q", "sort", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay + {
                    "q": QueryOverlay,
                    "sort": SortOverlay,
                },
            },

            # --- Commits ---
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commits_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/commits",
                "display_name": "List Commits",
                "short_desc": "List a repository's commits",
                "desc": "Lists commits, newest first, across every branch of the repository.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commit_commit_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}",
                "display_name": "Get Commit",
                "short_desc": "Retrieve one commit",
                "desc": "Retrieves a commit by hash, with its message, author, parents and participants.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit"),
                "fields": RepoScopeOverlay + {
                    "commit": CommitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getReportsForCommit",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}/reports",
                "operation_id": "getReportsForCommit",
                "display_name": "List Commit Reports",
                "short_desc": "List the code insights reports on a commit",
                "desc": "Lists the Code Insights reports attached to a commit — the results a linter, a "
                    "test run or a security scan posted against it.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay + {
                    "commit": CommitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commit_commit_comments_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}/comments",
                "display_name": "List Commit Comments",
                "short_desc": "List the comments on a commit",
                "desc": "Lists the comments made on a commit, both standalone notes and those attached to "
                    "a line of a diff.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit", "q", "sort", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay + {
                    "commit": CommitOverlay,
                    "q": QueryOverlay,
                    "sort": SortOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commit_commit_comments_post",
                "method": "POST",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}/comments",
                "display_name": "Comment on Commit",
                "short_desc": "Add a comment to a commit",
                "desc": "Adds a comment to a commit.\n\n"
                    "`content.raw` is the comment text in Bitbucket-flavored Markdown. Supplying `inline` "
                    "attaches the comment to a line of the diff instead of the commit as a whole.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit", "content", "inline"),
                "fields": RepoScopeOverlay + {
                    # the path already carries the commit; the body's copy of it would collide with it
                    "body.commit": <RestSchemaFieldOverlayInfo>{"ignore": True},
                    "commit": CommitOverlay,
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "The comment; put the text in \"raw\"",
                        "preselected": True,
                    },
                    "type": CommitCommentTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "id": ServerOwnedOverlay,
                    "user": ServerOwnedOverlay,
                    "deleted": ServerOwnedOverlay,
                    "parent": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commit_commit_approve_post",
                "method": "POST",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}/approve",
                "display_name": "Approve Commit",
                "short_desc": "Approve a commit as the authenticated user",
                "desc": "Records the authenticated user's approval of a commit.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit"),
                "fields": RepoScopeOverlay + {
                    "commit": CommitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_commit_commit_approve_delete",
                "method": "DELETE",
                "path": "/repositories/{workspace}/{repo_slug}/commit/{commit}/approve",
                "display_name": "Unapprove Commit",
                "short_desc": "Withdraw the authenticated user's approval of a commit",
                "desc": "Withdraws the authenticated user's approval of a commit. Another user's approval "
                    "is untouched.",
                "groups": (GroupCommits,),
                "field_order": ("workspace", "repo_slug", "commit"),
                "fields": RepoScopeOverlay + {
                    "commit": CommitOverlay,
                },
            },

            # --- Deployments ---
            <RestSchemaActionInfo>{
                "action": "getDeploymentsForRepository",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/deployments",
                "operation_id": "getDeploymentsForRepository",
                "display_name": "List Deployments",
                "short_desc": "List a repository's deployments",
                "desc": "Lists deployments, newest first, with the environment each went to, the state it "
                    "reached and the commit it deployed.",
                "groups": (GroupDeployments,),
                "field_order": ("workspace", "repo_slug", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay,
            },

            # --- Pull Requests ---
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests",
                "display_name": "List Pull Requests",
                "short_desc": "List a repository's pull requests",
                "desc": "Lists pull requests.\n\n"
                    "`state` selects which — `OPEN`, `MERGED`, `DECLINED` or `SUPERSEDED` — and defaults "
                    "to `OPEN`. It may be given more than once to include several states.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "state", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay + {
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_post",
                "method": "POST",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests",
                "display_name": "Create Pull Request",
                "short_desc": "Open a pull request",
                "desc": "Opens a pull request from one branch into another.\n\n"
                    "The repository in the URL is the **destination**. `destination` defaults to the "
                    "repository's main branch when unset. Setting `close_source_branch` deletes the "
                    "source branch when the pull request is merged.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "title", "source", "destination",
                    "reviewers", "close_source_branch", "draft"),
                "fields": RepoScopeOverlay + {
                    "source": SourceBranchOverlay,
                    "destination": DestinationBranchOverlay,
                    "reviewers": ReviewersOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "required": True,
                        "preselected": True,
                    },
                    "type": PullRequestTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "id": ServerOwnedOverlay,
                    "author": ServerOwnedOverlay,
                    "closed_by": ServerOwnedOverlay,
                    "comment_count": ServerOwnedOverlay,
                    "task_count": ServerOwnedOverlay,
                    "merge_commit": ServerOwnedOverlay,
                    "participants": ServerOwnedOverlay,
                    "reason": ServerOwnedOverlay,
                    "rendered": ServerOwnedOverlay,
                    "state": ServerOwnedOverlay,
                    "summary": ServerOwnedOverlay,
                    "queued": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}",
                "display_name": "Get Pull Request",
                "short_desc": "Retrieve one pull request",
                "desc": "Retrieves a pull request with its state, participants and merge commit.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_put",
                "method": "PUT",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}",
                "display_name": "Update Pull Request",
                "short_desc": "Update a pull request",
                "desc": "Updates a pull request's title, description, destination branch or reviewers.\n\n"
                    "Only the fields supplied are changed. This does not merge, decline or approve the "
                    "pull request; those are separate operations.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "title", "destination",
                    "reviewers", "close_source_branch"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                    "source": SourceBranchOverlay,
                    "destination": DestinationBranchOverlay,
                    "reviewers": ReviewersOverlay,
                    "type": PullRequestTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "id": ServerOwnedOverlay,
                    "author": ServerOwnedOverlay,
                    "closed_by": ServerOwnedOverlay,
                    "comment_count": ServerOwnedOverlay,
                    "task_count": ServerOwnedOverlay,
                    "merge_commit": ServerOwnedOverlay,
                    "participants": ServerOwnedOverlay,
                    "reason": ServerOwnedOverlay,
                    "rendered": ServerOwnedOverlay,
                    "state": ServerOwnedOverlay,
                    "summary": ServerOwnedOverlay,
                    "queued": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_get",
                "method": "GET",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}/comments",
                "display_name": "List Pull Request Comments",
                "short_desc": "List the comments on a pull request",
                "desc": "Lists the comments on a pull request, including the ones attached to a line of "
                    "the diff and the ones Bitbucket marks as deleted.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "page", "pagelen"),
                "fields": PagingOverlay + RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_post",
                "method": "POST",
                "path": "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}/comments",
                "display_name": "Comment on Pull Request",
                "short_desc": "Add a comment to a pull request",
                "desc": "Adds a comment to a pull request.\n\n"
                    "`content.raw` is the comment text. Supplying `inline` with a `path` and a line number "
                    "attaches it to that line of the diff; supplying `parent` makes it a reply.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "content", "inline",
                    "parent"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "The comment; put the text in \"raw\"",
                        "preselected": True,
                    },
                    "type": PullRequestCommentTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "id": ServerOwnedOverlay,
                    "user": ServerOwnedOverlay,
                    "deleted": ServerOwnedOverlay,
                    "pending": ServerOwnedOverlay,
                    "pullrequest": ServerOwnedOverlay,
                    "resolution": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action":
                    "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_get",
                "method": "GET",
                "path":
                    "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}/comments/{comment_id}",
                "display_name": "Get Pull Request Comment",
                "short_desc": "Retrieve one comment on a pull request",
                "desc": "Retrieves a single comment on a pull request.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "comment_id"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                    "comment_id": PullRequestCommentOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action":
                    "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_put",
                "method": "PUT",
                "path":
                    "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}/comments/{comment_id}",
                "display_name": "Update Pull Request Comment",
                "short_desc": "Update a comment on a pull request",
                "desc": "Replaces the text of a comment. Only the comment's author may change it.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "comment_id", "content"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                    "comment_id": PullRequestCommentOverlay,
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "The replacement comment; put the text in \"raw\"",
                        "preselected": True,
                    },
                    "type": PullRequestCommentTypeOverlay,
                    "links": ServerOwnedOverlay,
                    "created_on": ServerOwnedOverlay,
                    "updated_on": ServerOwnedOverlay,
                    "id": ServerOwnedOverlay,
                    "user": ServerOwnedOverlay,
                    "deleted": ServerOwnedOverlay,
                    "pending": ServerOwnedOverlay,
                    "pullrequest": ServerOwnedOverlay,
                    "parent": ServerOwnedOverlay,
                    "resolution": ServerOwnedOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action":
                    "repositories_workspace_repo_slug_pullrequests_pull_request_id_comments_comment_id_delete",
                "method": "DELETE",
                "path":
                    "/repositories/{workspace}/{repo_slug}/pullrequests/{pull_request_id}/comments/{comment_id}",
                "display_name": "Delete Pull Request Comment",
                "short_desc": "Delete a comment on a pull request",
                "desc": "Deletes a comment. Bitbucket keeps the comment and marks it deleted rather than "
                    "removing it, so it still appears in a listing with `deleted` set.",
                "groups": (GroupPullRequests,),
                "field_order": ("workspace", "repo_slug", "pull_request_id", "comment_id"),
                "fields": RepoScopeOverlay + {
                    "pull_request_id": PullRequestOverlay,
                    "comment_id": PullRequestCommentOverlay,
                },
            },

            # --- Search ---
            <RestSchemaActionInfo>{
                "action": "searchWorkspace",
                "method": "GET",
                "path": "/workspaces/{workspace}/search/code",
                "operation_id": "searchWorkspace",
                "display_name": "Search Code in a Workspace",
                "short_desc": "Search the code in a workspace's repositories",
                "desc": "Searches the code in every repository of a workspace.\n\n"
                    "`search_query` takes Bitbucket's code search syntax, ex: `foo lang:python` or "
                    "`\"exact phrase\" repo:my-repo`.",
                "groups": (GroupSearch,),
                "field_order": ("workspace", "search_query", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "workspace": WorkspaceOverlay,
                    "search_query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Query",
                        "short_desc": "Bitbucket code search syntax, ex: \"foo lang:python\"",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "searchAccount",
                "method": "GET",
                "path": "/users/{selected_user}/search/code",
                "operation_id": "searchAccount",
                "display_name": "Search Code in a User's Repositories",
                "short_desc": "Search the code in one user's repositories",
                "desc": "Searches the code in the repositories of one user account.",
                "groups": (GroupSearch,),
                "field_order": ("selected_user", "search_query", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "selected_user": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User",
                        "short_desc": "The account UUID in braces, or the account ID",
                        "preselected": True,
                    },
                    "search_query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Query",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "searchTeam",
                "method": "GET",
                "path": "/teams/{username}/search/code",
                "operation_id": "searchTeam",
                "display_name": "Search Code in a Team's Repositories",
                "short_desc": "Search the code in a team's repositories (deprecated by Bitbucket)",
                "desc": "Searches the code in a team's repositories.\n\n"
                    "**Bitbucket has replaced teams with workspaces**, and this endpoint is deprecated; "
                    "use **Search Code in a Workspace** instead. It is exported because the application "
                    "this module replaces exported it, and removing an action silently breaks whatever "
                    "still calls it.",
                "groups": (GroupSearch,),
                "field_order": ("username", "search_query", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "username": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Team",
                        "short_desc": "The team name; teams are Bitbucket's superseded name for workspaces",
                        "preselected": True,
                    },
                    "search_query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Query",
                        "preselected": True,
                    },
                },
            },

            # --- Workspaces ---
            <RestSchemaActionInfo>{
                "action": "workspaces_get",
                "method": "GET",
                "path": "/user/workspaces",
                "display_name": "List Workspaces",
                "short_desc": "List the workspaces the credential can see",
                "desc": "Lists the workspaces the authenticated user can reach.\n\n"
                    "`administrator` narrows the listing to the ones they administer.\n\n"
                    "This is the **only** listing Bitbucket still serves across workspaces, and it is what "
                    "fills the workspace dropdown every other Bitbucket dropdown is scoped by. A record "
                    "wraps its workspace — `{administrator, workspace: {slug, uuid, links}}` — and carries "
                    "no display name, so a workspace is identified by its slug.",
                "groups": (GroupWorkspaces,),
                "field_order": ("administrator", "sort"),
                "fields": {
                    "sort": SortOverlay,
                    "administrator": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Administrator Only",
                        "short_desc": "Return only workspaces the credential administers",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "workspaces_workspace_get",
                "method": "GET",
                "path": "/workspaces/{workspace}",
                "display_name": "Get Workspace",
                "short_desc": "Retrieve one workspace",
                "desc": "Retrieves a workspace with its slug, name and whether it is private.",
                "groups": (GroupWorkspaces,),
                "field_order": ("workspace",),
                "fields": {
                    "workspace": WorkspaceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "workspaces_workspace_members_get",
                "method": "GET",
                "path": "/workspaces/{workspace}/members",
                "display_name": "List Workspace Members",
                "short_desc": "List the members of a workspace",
                "desc": "Lists the members of a workspace.\n\n"
                    "Each record is a **membership**, so the member is under `user` rather than at the top "
                    "level: the account UUID is `user.uuid`.",
                "groups": (GroupWorkspaces,),
                "field_order": ("workspace", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "workspace": WorkspaceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "workspaces_workspace_projects_get",
                "method": "GET",
                "path": "/workspaces/{workspace}/projects",
                "display_name": "List Workspace Projects",
                "short_desc": "List the projects in a workspace",
                "desc": "Lists the projects in a workspace. A project groups repositories; it is not a "
                    "Jira project.",
                "groups": (GroupWorkspaces,),
                "field_order": ("workspace", "page", "pagelen"),
                "fields": PagingOverlay + {
                    "workspace": WorkspaceOverlay,
                },
            },
        );
    }
}
}
