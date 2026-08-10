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
                "value_field": "slug",
                "display_field": "name",
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
    }
}
}
