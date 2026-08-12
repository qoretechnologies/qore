# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the GitLab application exports, and how they are presented
/** The manifest is deliberately explicit.  GitLab's schema declares 1284 paths and 1752 operations; this
    application exports a reviewed subset, and an endpoint appearing in a future schema does not become a
    public action until it is added here with a name, a description, a group and whatever presentation it
    needs.

    @section gitlabmanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, which derived them from the
    \c operationId of a vendored Swagger 2.0 document.  GitLab generates that document and its OpenAPI 3
    document from the same API source, so the two agree and the IDs carry over unchanged - \c
    "getApiV4Projects", not \c "list_projects".

    They are not pretty, and they are not restyled.  Action identity is what existing workflows call, so it
    is preserved across the replacement; the \c operation_id recorded beside each entry is checked for drift
    instead, so an upstream rename is reported rather than silently renaming an action.

    @section gitlabmanifest_added What This Application Adds

    Three read-only listings are exported that the TypeScript application did not:

    |!Action|!Why
    |\c getApiV4Users|its user dropdowns fell back to the authenticated user alone when no project was chosen
    |\c getApiV4ProjectsIdMilestones|its milestone dropdown called an endpoint the application did not export
    |\c getApiV4Topics|its topic dropdown called an endpoint the application did not export

    A dropdown here is produced by executing another action **in this manifest**, so one that names an
    unexported endpoint cannot exist: it fails when the action set is built.  Exporting these three is what
    makes those dropdowns work at all.

    @section gitlabmanifest_scoped Scoped Dropdowns

    Almost nothing in GitLab is addressable on its own.  A branch, an issue IID, a wiki slug, a CI/CD
    variable key and a deploy key are meaningful only inside a project, and a member only inside a project
    or group.  Every such dropdown declares
    @ref RestSchemaActions::RestSchemaReferenceDataInfo::options_from "options_from" naming the \c id option
    of the action being filled in, so the listing is made against the container the user chose; until one is
    chosen the dropdown is empty rather than offering values from somewhere else.

    The container option itself carries \c on_change, so that changing it re-reads everything scoped by it.

    @section gitlabmanifest_ids_vs_iids IDs and IIDs

    GitLab addresses an issue or a merge request inside a project by its **IID** - the number shown in the
    web UI and written \c "#42" - while the instance-wide \c /issues listing filters by \c id.  They are
    different numbers for the same object, and confusing them addresses a different issue without any error,
    so each dropdown supplies whichever the endpoint takes and says which it is.

    @section gitlabmanifest_pagination Pagination

    A GitLab listing takes \c page and \c per_page (maximum 100) and reports \c X-Total, \c X-Page and
    \c X-Next-Page in the **response headers**, not in the body; the body is the bare array.  An action
    therefore returns the records themselves, and paging means asking for the next page until fewer than
    \c per_page records come back.
*/
public class GitLabManifest {
    public {
        #! Action group: repository branches
        const GroupBranches = "Branches";

        #! Action group: repository commits and commit comments
        const GroupCommits = "Commits";

        #! Action group: deploy keys
        const GroupDeployKeys = "Deploy Keys";

        #! Action group: groups
        const GroupGroups = "Groups";

        #! Action group: issues and milestones
        const GroupIssues = "Issues";

        #! Action group: project and group membership, and users
        const GroupMembers = "Members";

        #! Action group: merge requests and their approvals
        const GroupMergeRequests = "Merge Requests";

        #! Action group: projects and topics
        const GroupProjects = "Projects";

        #! Action group: CI/CD variables
        const GroupVariables = "Variables";

        #! Action group: wiki pages
        const GroupWikis = "Wikis";

        #! Reference data name: projects
        const RefProject = "gitlab-project";

        #! Reference data name: groups
        const RefGroup = "gitlab-group";

        #! Reference data name: users
        const RefUser = "gitlab-user";

        #! Reference data name: project topics
        const RefTopic = "gitlab-topic";

        #! Reference data name: branches in the selected project
        const RefBranch = "gitlab-branch";

        #! Reference data name: commits in the selected project
        const RefCommit = "gitlab-commit";

        #! Reference data name: deploy keys enabled for the selected project
        const RefDeployKey = "gitlab-deploy-key";

        #! Reference data name: issues in the selected project, by project-relative IID
        const RefIssueIid = "gitlab-issue-iid";

        #! Reference data name: issues in the selected project, by instance-wide ID
        const RefIssueId = "gitlab-issue-id";

        #! Reference data name: merge requests in the selected project, by project-relative IID
        const RefMergeRequestIid = "gitlab-merge-request-iid";

        #! Reference data name: direct members of the selected project
        const RefProjectMember = "gitlab-project-member";

        #! Reference data name: direct members of the selected group
        const RefGroupMember = "gitlab-group-member";

        #! Reference data name: CI/CD variables on the selected project
        const RefProjectVariable = "gitlab-project-variable";

        #! Reference data name: CI/CD variables on the selected group
        const RefGroupVariable = "gitlab-group-variable";

        #! Reference data name: wiki pages in the selected project
        const RefWikiSlug = "gitlab-wiki-slug";

        #! Reference data name: milestones on the selected project
        const RefMilestone = "gitlab-milestone";

        #! GitLab's maximum page size for a listing
        const MaxPerPage = GitLabRestClient::GitLabRestClientBase::MaxPerPage;

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.

            \c per_page is set to GitLab's maximum, so that filling a dropdown is a single request rather
            than a walk through the whole collection.

            Everything that lives inside a project or a group declares \c options_from, so the listing is
            scoped to the container the user picked; see @ref gitlabmanifest_scoped.
        */
        const ReferenceData = {
            RefProject: <RestSchemaReferenceDataInfo>{
                "name": RefProject,
                "action": "getApiV4Projects",
                # "membership" restricts the listing to projects the credential can actually work with;
                # without it GitLab.com returns every public project in existence
                "options": {"membership": True, "per_page": MaxPerPage, "order_by": "path", "sort": "asc"},
                "value_field": "id",
                "display_field": "name_with_namespace",
                "desc": "projects the authenticated user is a member of",
            },
            RefGroup: <RestSchemaReferenceDataInfo>{
                "name": RefGroup,
                "action": "getApiV4Groups",
                "options": {"per_page": MaxPerPage, "order_by": "path", "sort": "asc"},
                "value_field": "id",
                # the full path is what identifies a group; two groups may share a name under different
                # parents
                "display_field": "full_path",
                "desc": "groups visible to the authenticated user",
            },
            RefUser: <RestSchemaReferenceDataInfo>{
                "name": RefUser,
                "action": "getApiV4Users",
                "options": {"per_page": MaxPerPage, "active": True},
                "value_field": "id",
                "display_field": "username",
                "desc": "active users on this GitLab instance",
            },
            RefTopic: <RestSchemaReferenceDataInfo>{
                "name": RefTopic,
                "action": "getApiV4Topics",
                "options": {"per_page": MaxPerPage},
                "value_field": "id",
                "display_field": "name",
                "desc": "project topics defined on this GitLab instance",
            },
            RefBranch: <RestSchemaReferenceDataInfo>{
                "name": RefBranch,
                "action": "getApiV4ProjectsIdRepositoryBranches",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "name",
                "desc": "branches in the selected project",
            },
            RefCommit: <RestSchemaReferenceDataInfo>{
                "name": RefCommit,
                "action": "getApiV4ProjectsIdRepositoryCommits",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "title",
                "desc": "the most recent commits in the selected project",
            },
            RefDeployKey: <RestSchemaReferenceDataInfo>{
                "name": RefDeployKey,
                "action": "getApiV4ProjectsIdDeployKeys",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "title",
                "desc": "deploy keys enabled for the selected project",
            },
            RefIssueIid: <RestSchemaReferenceDataInfo>{
                "name": RefIssueIid,
                "action": "getApiV4ProjectsIdIssues",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "iid",
                "display_field": "title",
                "desc": "issues in the selected project, by project-relative IID",
            },
            RefIssueId: <RestSchemaReferenceDataInfo>{
                "name": RefIssueId,
                "action": "getApiV4ProjectsIdIssues",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "title",
                "desc": "issues in the selected project, by instance-wide ID",
            },
            RefMergeRequestIid: <RestSchemaReferenceDataInfo>{
                "name": RefMergeRequestIid,
                "action": "getApiV4ProjectsIdMergeRequests",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "iid",
                "display_field": "title",
                "desc": "merge requests in the selected project, by project-relative IID",
            },
            RefProjectMember: <RestSchemaReferenceDataInfo>{
                "name": RefProjectMember,
                "action": "getApiV4ProjectsIdMembers",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "username",
                "desc": "direct members of the selected project",
            },
            RefGroupMember: <RestSchemaReferenceDataInfo>{
                "name": RefGroupMember,
                "action": "getApiV4GroupsIdMembers",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "username",
                "desc": "direct members of the selected group",
            },
            RefProjectVariable: <RestSchemaReferenceDataInfo>{
                "name": RefProjectVariable,
                "action": "getApiV4ProjectsIdVariables",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "key",
                "desc": "CI/CD variables defined on the selected project",
            },
            RefGroupVariable: <RestSchemaReferenceDataInfo>{
                "name": RefGroupVariable,
                "action": "getApiV4GroupsIdVariables",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "key",
                "desc": "CI/CD variables defined on the selected group",
            },
            RefWikiSlug: <RestSchemaReferenceDataInfo>{
                "name": RefWikiSlug,
                "action": "getApiV4ProjectsIdWikis",
                "options_from": {"id": "id"},
                "value_field": "slug",
                "display_field": "title",
                "desc": "wiki pages in the selected project",
            },
            RefMilestone: <RestSchemaReferenceDataInfo>{
                "name": RefMilestone,
                "action": "getApiV4ProjectsIdMilestones",
                "options": {"per_page": MaxPerPage},
                "options_from": {"id": "id"},
                "value_field": "id",
                "display_field": "title",
                "desc": "milestones defined on the selected project",
            },
        };

        #! Overlay for the \c id path variable when it identifies the project the action works on
        /** Everything else in a project action is scoped by this value, so it is preselected and declares
            \c on_change: changing the project has to re-read the branches, issues, members and variables
            that were listed for the previous one.
        */
        const ProjectOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Project",
            "short_desc": "The project to work on",
            "desc": "The project this action works on, given as its numeric ID or as its path "
                "(`group/project`).\n\n"
                "Give the path as it reads; the transport escapes it into the single path segment GitLab "
                "expects. A path escaped by hand would be escaped a second time and would not resolve.\n\n"
                "Every other option scoped by a project - branches, issues, members, variables - is listed "
                "from this project, so changing it re-reads them.",
            "ref_data": RefProject,
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for a project ID that is a secondary reference rather than the subject of the action
        const ProjectRefOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Project",
            "ref_data": RefProject,
            "supports_custom_values": True,
        };

        #! Overlay for the \c id path variable when it identifies the group the action works on
        const GroupOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Group",
            "short_desc": "The group to work on",
            "desc": "The group this action works on, given as its numeric ID or as its path "
                "(`parent/group`).\n\n"
                "Give the path as it reads; the transport escapes it into the single path segment GitLab "
                "expects. A path escaped by hand would be escaped a second time and would not resolve.\n\n"
                "Every other option scoped by a group - members, variables - is listed from this group, so "
                "changing it re-reads them.",
            "ref_data": RefGroup,
            "supports_custom_values": True,
            "preselected": True,
            "has_dependents": True,
            "on_change": ("refetch",),
        };

        #! Overlay for a branch name in the selected project
        const BranchOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Branch",
            "ref_data": RefBranch,
            "depends_on": ("id",),
            "supports_custom_values": True,
        };

        #! Overlay for a commit SHA in the selected project
        const CommitOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Commit",
            "short_desc": "A commit SHA, a branch name or a tag",
            "ref_data": RefCommit,
            "depends_on": ("id",),
            "supports_custom_values": True,
        };

        #! Overlay for a deploy key ID in the selected project
        const DeployKeyOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Deploy Key",
            "ref_data": RefDeployKey,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for an issue IID in the selected project
        const IssueIidOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Issue",
            "short_desc": "The project-relative issue IID, as shown in the web UI",
            "desc": "The issue's **IID**: the project-relative number shown in the web UI and written "
                "`#42`.\n\n"
                "This is not the instance-wide `id` that the global issue listing filters by; the two are "
                "different numbers for the same issue.",
            "ref_data": RefIssueIid,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a merge request IID in the selected project
        const MergeRequestIidOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Merge Request",
            "short_desc": "The project-relative merge request IID, as shown in the web UI",
            "desc": "The merge request's **IID**: the project-relative number shown in the web UI and "
                "written `!7`, not the instance-wide `id`.",
            "ref_data": RefMergeRequestIid,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a member of the selected project
        const ProjectMemberOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "User",
            "ref_data": RefProjectMember,
            "depends_on": ("id",),
            "supports_custom_values": True,
        };

        #! Overlay for the member of the selected project that the action works on
        const ProjectMemberSubjectOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "User",
            "short_desc": "The member to work on",
            "ref_data": RefProjectMember,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the member of the selected group that the action works on
        const GroupMemberSubjectOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "User",
            "short_desc": "The member to work on",
            "ref_data": RefGroupMember,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a user anywhere on the instance
        const UserRefOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "User",
            "ref_data": RefUser,
            "supports_custom_values": True,
        };

        #! Overlay for a milestone on the selected project
        const MilestoneOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Milestone",
            "ref_data": RefMilestone,
            "depends_on": ("id",),
            "supports_custom_values": True,
        };

        #! Overlay for a wiki page slug in the selected project
        const WikiSlugOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "short_desc": "The URL slug of the wiki page",
            "ref_data": RefWikiSlug,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a CI/CD variable key on the selected project
        const ProjectVariableKeyOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Key",
            "ref_data": RefProjectVariable,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a CI/CD variable key on the selected group
        const GroupVariableKeyOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Key",
            "ref_data": RefGroupVariable,
            "depends_on": ("id",),
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay shared by the \c per_page option of every listing action
        const PerPageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Per Page",
            "short_desc": "How many records to return, up to 100",
            "desc": "How many records to return in one request, up to GitLab's maximum of `100`.\n\n"
                "The total and the next page are reported in the `X-Total`, `X-Page` and `X-Next-Page` "
                "**response headers**, not in the body, so paging means asking for the next page until "
                "fewer than `per_page` records come back.",
            "preselected": True,
            "example_value": 50,
        };

        #! Overlay shared by the \c page option of every listing action
        const PageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "short_desc": "Which page of results to return, starting at 1",
            "preselected": True,
        };

        #! Overlay applied to every listing action's paging options
        const PagingOverlay = {
            "page": PageOverlay,
            "per_page": PerPageOverlay,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Branches ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdRepositoryBranches",
                "method": "GET",
                "path": "/api/v4/projects/{id}/repository/branches",
                "operation_id": "getApiV4ProjectsIdRepositoryBranches",
                "display_name": "List Branches",
                "short_desc": "List a project's repository branches",
                "desc": "Lists the branches of a project's repository, with the commit each one points at "
                    "and whether it is protected, merged or the default branch.\n\n"
                    "Use `search` for a prefix (`^feature`) or suffix (`fix$`) match, or `regex` for "
                    "anything else. A repository with thousands of branches is paged with `page_token` "
                    "rather than `page`.",
                "groups": (GroupBranches,),
                "field_order": ("id", "search", "regex", "sort", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdRepositoryBranches",
                "method": "POST",
                "path": "/api/v4/projects/{id}/repository/branches",
                "operation_id": "postApiV4ProjectsIdRepositoryBranches",
                "display_name": "Create Branch",
                "short_desc": "Create a branch in a project's repository",
                "desc": "Creates a branch from an existing commit, branch or tag.\n\n"
                    "`ref` is where the new branch starts; `branch` is the name to give it. Creating a "
                    "branch that already exists fails rather than moving it.",
                "groups": (GroupBranches,),
                "field_order": ("id", "branch", "ref"),
                "fields": {
                    "id": ProjectOverlay,
                    "branch": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Branch Name",
                        "short_desc": "The name to give the new branch",
                        "preselected": True,
                    },
                    "ref": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start From",
                        "short_desc": "The branch, tag or commit SHA the new branch starts from",
                        "ref_data": RefBranch,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdRepositoryBranchesBranch",
                "method": "GET",
                "path": "/api/v4/projects/{id}/repository/branches/{branch}",
                "operation_id": "getApiV4ProjectsIdRepositoryBranchesBranch",
                "display_name": "Get Branch",
                "short_desc": "Retrieve one repository branch",
                "desc": "Retrieves a single branch, including the commit it points at and its protection "
                    "status.",
                "groups": (GroupBranches,),
                "field_order": ("id", "branch"),
                "fields": {
                    "id": ProjectOverlay,
                    "branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdRepositoryBranchesBranch",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/repository/branches/{branch}",
                "operation_id": "deleteApiV4ProjectsIdRepositoryBranchesBranch",
                "display_name": "Delete Branch",
                "short_desc": "Delete a repository branch",
                "desc": "Deletes a branch.\n\n"
                    "The commits themselves are not deleted, but a branch is the only thing keeping them "
                    "reachable unless a tag or another branch also points at them.",
                "groups": (GroupBranches,),
                "field_order": ("id", "branch"),
                "fields": {
                    "id": ProjectOverlay,
                    "branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdRepositoryMergedBranches",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/repository/merged_branches",
                "operation_id": "deleteApiV4ProjectsIdRepositoryMergedBranches",
                "display_name": "Delete Merged Branches",
                "short_desc": "Delete every branch already merged into the default branch",
                "desc": "Deletes every branch that has already been merged into the project's default "
                    "branch, leaving protected branches alone.\n\n"
                    "This is asynchronous: it returns `202 Accepted` and the branches disappear shortly "
                    "afterwards.",
                "groups": (GroupBranches,),
                "fields": {
                    "id": ProjectOverlay,
                },
            },

            # --- Commits ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdRepositoryCommits",
                "method": "GET",
                "path": "/api/v4/projects/{id}/repository/commits",
                "operation_id": "getApiV4ProjectsIdRepositoryCommits",
                "display_name": "List Commits",
                "short_desc": "List a project's repository commits",
                "desc": "Lists commits, newest first.\n\n"
                    "`ref_name` selects the branch or tag to read (the default branch when unset), `path` "
                    "limits the listing to commits touching one file or directory, and `since`/`until` "
                    "bound it by date. Set `with_stats` to include the lines added and removed by each "
                    "commit.",
                "groups": (GroupCommits,),
                "field_order": ("id", "ref_name", "path", "since", "until", "author", "with_stats",
                    "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                    "ref_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Branch or Tag",
                        "short_desc": "The branch or tag to list commits from",
                        "ref_data": RefBranch,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdRepositoryCommitsSha",
                "method": "GET",
                "path": "/api/v4/projects/{id}/repository/commits/{sha}",
                "operation_id": "getApiV4ProjectsIdRepositoryCommitsSha",
                "display_name": "Get Commit",
                "short_desc": "Retrieve one commit",
                "desc": "Retrieves a commit by SHA, branch name or tag name.\n\n"
                    "Set `stats` to include the lines added and removed.",
                "groups": (GroupCommits,),
                "field_order": ("id", "sha", "stats"),
                "fields": {
                    "id": ProjectOverlay,
                    "sha": CommitOverlay + <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdRepositoryCommitsShaComments",
                "method": "GET",
                "path": "/api/v4/projects/{id}/repository/commits/{sha}/comments",
                "operation_id": "getApiV4ProjectsIdRepositoryCommitsShaComments",
                "display_name": "List Commit Comments",
                "short_desc": "List the comments on a commit",
                "desc": "Lists the comments made on a commit, both the standalone notes and the ones "
                    "attached to a specific line of a diff.",
                "groups": (GroupCommits,),
                "field_order": ("id", "sha", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                    "sha": CommitOverlay + <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdRepositoryCommitsShaComments",
                "method": "POST",
                "path": "/api/v4/projects/{id}/repository/commits/{sha}/comments",
                "operation_id": "postApiV4ProjectsIdRepositoryCommitsShaComments",
                "display_name": "Comment on Commit",
                "short_desc": "Add a comment to a commit",
                "desc": "Adds a comment to a commit.\n\n"
                    "Supplying `path`, `line` and `line_type` attaches the comment to that line of the "
                    "diff; without them it is a standalone note on the commit. `line_type` says whether "
                    "the line number refers to the old (`old`) or new (`new`) side of the diff.",
                "groups": (GroupCommits,),
                "field_order": ("id", "sha", "note", "path", "line", "line_type"),
                "fields": {
                    "id": ProjectOverlay,
                    "sha": CommitOverlay + <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "note": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Comment",
                        "short_desc": "The comment text, in GitLab-flavored Markdown",
                        "preselected": True,
                    },
                },
            },

            # --- Deploy Keys ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdDeployKeys",
                "method": "GET",
                "path": "/api/v4/projects/{id}/deploy_keys",
                "operation_id": "getApiV4ProjectsIdDeployKeys",
                "display_name": "List Deploy Keys",
                "short_desc": "List the deploy keys enabled for a project",
                "desc": "Lists the deploy keys enabled for a project, with whether each may push as well "
                    "as pull.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdDeployKeys",
                "method": "POST",
                "path": "/api/v4/projects/{id}/deploy_keys",
                "operation_id": "postApiV4ProjectsIdDeployKeys",
                "display_name": "Add Deploy Key",
                "short_desc": "Add a deploy key to a project",
                "desc": "Adds an SSH public key that can clone the repository without belonging to a "
                    "user.\n\n"
                    "`can_push` grants write access as well, so leave it unset for a key that only needs "
                    "to deploy. Adding a key GitLab already knows enables the existing key for this "
                    "project rather than creating a second one.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "title", "key", "can_push", "expires_at"),
                "fields": {
                    "id": ProjectOverlay,
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Public Key",
                        "short_desc": "The SSH public key, ex: \"ssh-ed25519 AAAA… deploy@example.com\"",
                        "preselected": True,
                    },
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "short_desc": "A name identifying what the key is for",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdDeployKeysKeyId",
                "method": "GET",
                "path": "/api/v4/projects/{id}/deploy_keys/{key_id}",
                "operation_id": "getApiV4ProjectsIdDeployKeysKeyId",
                "display_name": "Get Deploy Key",
                "short_desc": "Retrieve one deploy key",
                "desc": "Retrieves a deploy key enabled for a project.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "key_id"),
                "fields": {
                    "id": ProjectOverlay,
                    "key_id": DeployKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdDeployKeysKeyId",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/deploy_keys/{key_id}",
                "operation_id": "putApiV4ProjectsIdDeployKeysKeyId",
                "display_name": "Update Deploy Key",
                "short_desc": "Update a deploy key's title or push access",
                "desc": "Updates a deploy key's title, or grants and revokes its push access.\n\n"
                    "The key material itself cannot be changed; replace the key to do that.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "key_id", "title", "can_push"),
                "fields": {
                    "id": ProjectOverlay,
                    "key_id": DeployKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdDeployKeysKeyId",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/deploy_keys/{key_id}",
                "operation_id": "deleteApiV4ProjectsIdDeployKeysKeyId",
                "display_name": "Delete Deploy Key",
                "short_desc": "Remove a deploy key from a project",
                "desc": "Disables a deploy key for this project. A key still enabled for another project "
                    "continues to work there.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "key_id"),
                "fields": {
                    "id": ProjectOverlay,
                    "key_id": DeployKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdDeployKeysKeyIdEnable",
                "method": "POST",
                "path": "/api/v4/projects/{id}/deploy_keys/{key_id}/enable",
                "operation_id": "postApiV4ProjectsIdDeployKeysKeyIdEnable",
                "display_name": "Enable Deploy Key",
                "short_desc": "Enable an existing deploy key for another project",
                "desc": "Enables a deploy key that already exists elsewhere for this project as well, "
                    "without having to supply the key material again.",
                "groups": (GroupDeployKeys,),
                "field_order": ("id", "key_id"),
                "fields": {
                    "id": ProjectOverlay,
                    # the key being enabled is by definition not yet enabled here, so it cannot be listed
                    # from this project
                    "key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deploy Key",
                        "short_desc": "The ID of a deploy key already enabled for another project",
                        "preselected": True,
                    },
                },
            },

            # --- Groups ---
            <RestSchemaActionInfo>{
                "action": "getApiV4Groups",
                "method": "GET",
                "path": "/api/v4/groups",
                "operation_id": "getApiV4Groups",
                "display_name": "List Groups",
                "short_desc": "List the groups visible to the credential",
                "desc": "Lists groups.\n\n"
                    "By default this includes every group the credential can see, which on GitLab.com "
                    "means every public group. Set `owned` or `min_access_level` to restrict it to groups "
                    "the user actually works in, and `top_level_only` to leave out subgroups.",
                "groups": (GroupGroups,),
                "field_order": ("search", "owned", "min_access_level", "top_level_only", "page",
                    "per_page"),
                "fields": PagingOverlay + {
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the group name and path",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4Groups",
                "method": "POST",
                "path": "/api/v4/groups",
                "operation_id": "postApiV4Groups",
                "display_name": "Create Group",
                "short_desc": "Create a group or subgroup",
                "desc": "Creates a group.\n\n"
                    "Supplying `parent_id` creates a subgroup of that group; without it the group is "
                    "created at the top level, which a GitLab.com account may not be allowed to do. "
                    "`path` is the URL segment and cannot be changed as freely as `name` afterwards.",
                "groups": (GroupGroups,),
                "field_order": ("name", "path", "parent_id", "description", "visibility"),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                    "path": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Path",
                        "short_desc": "The URL segment for the group, ex: \"platform-team\"",
                        "preselected": True,
                    },
                    "parent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parent Group",
                        "short_desc": "Create the group under this group, making it a subgroup",
                        "ref_data": RefGroup,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4GroupsId",
                "method": "GET",
                "path": "/api/v4/groups/{id}",
                "operation_id": "getApiV4GroupsId",
                "display_name": "Get Group",
                "short_desc": "Retrieve one group",
                "desc": "Retrieves a group, optionally with the projects it contains.\n\n"
                    "`with_projects` is expensive on a large group; leave it unset and list the projects "
                    "separately when there are many.",
                "groups": (GroupGroups,),
                "field_order": ("id", "with_projects"),
                "fields": {
                    "id": GroupOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4GroupsId",
                "method": "PUT",
                "path": "/api/v4/groups/{id}",
                "operation_id": "putApiV4GroupsId",
                "display_name": "Update Group",
                "short_desc": "Update a group's attributes",
                "desc": "Updates a group's name, path, visibility and settings.\n\n"
                    "Only the fields supplied are changed. Narrowing `visibility` also narrows every "
                    "project in the group that was more visible than the new setting.",
                "groups": (GroupGroups,),
                "field_order": ("id", "name", "path", "description", "visibility"),
                "fields": {
                    "id": GroupOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4GroupsId",
                "method": "DELETE",
                "path": "/api/v4/groups/{id}",
                "operation_id": "deleteApiV4GroupsId",
                "display_name": "Delete Group",
                "short_desc": "Schedule a group for deletion",
                "desc": "Schedules a group for deletion, together with every project and subgroup in "
                    "it.\n\n"
                    "This is not immediate: GitLab marks the group for deletion and removes it after a "
                    "retention period, during which it can be restored.",
                "groups": (GroupGroups,),
                "fields": {
                    "id": GroupOverlay,
                },
            },

            # --- Issues ---
            <RestSchemaActionInfo>{
                "action": "getApiV4Issues",
                "method": "GET",
                "path": "/api/v4/issues",
                "operation_id": "getApiV4Issues",
                "display_name": "List Issues",
                "short_desc": "List issues across every project the user can see",
                "desc": "Lists issues across the whole instance rather than one project.\n\n"
                    "`scope` decides whose issues: `created_by_me`, `assigned_to_me`, or `all`. Every "
                    "filter has a `not[…]` counterpart that excludes instead of includes.\n\n"
                    "The `iids` filter here takes **instance-wide** issue IDs, not the project-relative "
                    "IIDs shown in the web UI.",
                "groups": (GroupIssues,),
                "field_order": ("scope", "state", "search", "labels", "assignee_id", "author_id",
                    "created_after", "updated_after", "page", "per_page"),
                "fields": PagingOverlay + {
                    "assignee_id": UserRefOverlay,
                    "author_id": UserRefOverlay,
                    "closed_by_id": UserRefOverlay,
                    "not[assignee_id]": UserRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Assigned To",
                    },
                    "not[author_id]": UserRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Authored By",
                    },
                    "scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Scope",
                        "short_desc": "Whose issues to return",
                        "preselected": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the issue title and description",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdIssues",
                "method": "GET",
                "path": "/api/v4/projects/{id}/issues",
                "operation_id": "getApiV4ProjectsIdIssues",
                "display_name": "List Project Issues",
                "short_desc": "List the issues in one project",
                "desc": "Lists the issues in a project.\n\n"
                    "The user filters here offer the project's own members, because an issue in a project "
                    "is normally assigned to somebody who works on it. Every filter has a `not[…]` "
                    "counterpart that excludes instead of includes.",
                "groups": (GroupIssues,),
                "field_order": ("id", "state", "search", "labels", "assignee_id", "author_id",
                    "milestone_id", "created_after", "updated_after", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "author_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Author",
                    },
                    "closed_by_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Closed By",
                    },
                    "milestone_id": MilestoneOverlay,
                    "not[assignee_id]": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Assigned To",
                    },
                    "not[author_id]": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Authored By",
                    },
                    "not[milestone_id]": MilestoneOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not In Milestone",
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the issue title and description",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdIssues",
                "method": "POST",
                "path": "/api/v4/projects/{id}/issues",
                "operation_id": "postApiV4ProjectsIdIssues",
                "display_name": "Create Issue",
                "short_desc": "Create an issue in a project",
                "desc": "Creates an issue.\n\n"
                    "`title` is the only required field. `labels` is a comma-separated list and creates "
                    "any label that does not exist yet, so a typo silently produces a new label rather "
                    "than an error.",
                "groups": (GroupIssues,),
                "field_order": ("id", "title", "description", "assignee_ids", "labels", "milestone_id",
                    "due_date", "confidential"),
                "fields": {
                    "id": ProjectOverlay,
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "assignee_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignees",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "milestone_id": MilestoneOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "The issue body, in GitLab-flavored Markdown",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdIssuesIssueIid",
                "method": "GET",
                "path": "/api/v4/projects/{id}/issues/{issue_iid}",
                "operation_id": "getApiV4ProjectsIdIssuesIssueIid",
                "display_name": "Get Issue",
                "short_desc": "Retrieve one issue",
                "desc": "Retrieves an issue by its project-relative IID.",
                "groups": (GroupIssues,),
                "field_order": ("id", "issue_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "issue_iid": IssueIidOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdIssuesIssueIid",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/issues/{issue_iid}",
                "operation_id": "putApiV4ProjectsIdIssuesIssueIid",
                "display_name": "Update Issue",
                "short_desc": "Update an issue, or close or reopen it",
                "desc": "Updates an issue.\n\n"
                    "`state_event` is how an issue is closed (`close`) or reopened (`reopen`); there is no "
                    "separate endpoint for it. `labels` **replaces** the whole label set, so use "
                    "`add_labels` and `remove_labels` to change one without disturbing the rest.",
                "groups": (GroupIssues,),
                "field_order": ("id", "issue_iid", "state_event", "title", "description",
                    "assignee_ids", "add_labels", "remove_labels", "labels", "milestone_id", "due_date"),
                "fields": {
                    "id": ProjectOverlay,
                    "issue_iid": IssueIidOverlay,
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "assignee_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignees",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                    },
                    "milestone_id": MilestoneOverlay,
                    "state_event": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State Change",
                        "short_desc": "\"close\" to close the issue, \"reopen\" to reopen it",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdIssuesIssueIid",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/issues/{issue_iid}",
                "operation_id": "deleteApiV4ProjectsIdIssuesIssueIid",
                "display_name": "Delete Issue",
                "short_desc": "Delete an issue permanently",
                "desc": "Deletes an issue.\n\n"
                    "This is permanent and needs Owner rights; closing an issue with **Update Issue** is "
                    "almost always what is wanted instead.",
                "groups": (GroupIssues,),
                "field_order": ("id", "issue_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "issue_iid": IssueIidOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdIssuesIssueIidTimeStats",
                "method": "GET",
                "path": "/api/v4/projects/{id}/issues/{issue_iid}/time_stats",
                "operation_id": "getApiV4ProjectsIdIssuesIssueIidTimeStats",
                "display_name": "Get Issue Time Stats",
                "short_desc": "Retrieve the time estimate and time spent on an issue",
                "desc": "Retrieves the time tracking figures for an issue: the estimate, the total spent, "
                    "and both as human-readable strings.",
                "groups": (GroupIssues,),
                "field_order": ("id", "issue_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "issue_iid": IssueIidOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdMilestones",
                "method": "GET",
                "path": "/api/v4/projects/{id}/milestones",
                "operation_id": "getApiV4ProjectsIdMilestones",
                "display_name": "List Milestones",
                "short_desc": "List a project's milestones",
                "desc": "Lists the milestones defined on a project.\n\n"
                    "This is what backs the milestone dropdowns on the issue and merge request actions; "
                    "the application this module replaces offered those dropdowns without exporting the "
                    "endpoint that fills them.",
                "groups": (GroupIssues,),
                "field_order": ("id", "state", "search", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },

            # --- Members ---
            <RestSchemaActionInfo>{
                "action": "getApiV4Users",
                "method": "GET",
                "path": "/api/v4/users",
                "operation_id": "getApiV4Users",
                "display_name": "List Users",
                "short_desc": "List the users on this GitLab instance",
                "desc": "Lists users.\n\n"
                    "This is what backs the user dropdowns on the instance-wide issue listing. The "
                    "application this module replaces could only offer the authenticated user there, "
                    "because it exported no endpoint that lists users.\n\n"
                    "A non-administrator sees a restricted view: `search` and `username` still work, but "
                    "the administrative filters are ignored.",
                "groups": (GroupMembers,),
                "field_order": ("search", "username", "active", "page", "per_page"),
                "fields": PagingOverlay + {
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the name, username and public email",
                        "preselected": True,
                    },
                    "username": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Username",
                        "short_desc": "Return the user with this exact username",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdUsers",
                "method": "GET",
                "path": "/api/v4/projects/{id}/users",
                "operation_id": "getApiV4ProjectsIdUsers",
                "display_name": "List Project Users",
                "short_desc": "List every user with access to a project",
                "desc": "Lists the users of a project, including those who reach it through a group or an "
                    "ancestor group.\n\n"
                    "This differs from **List Project Members**, which returns only the *direct* members "
                    "and their access levels.",
                "groups": (GroupMembers,),
                "field_order": ("id", "search", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdMembers",
                "method": "GET",
                "path": "/api/v4/projects/{id}/members",
                "operation_id": "getApiV4ProjectsIdMembers",
                "display_name": "List Project Members",
                "short_desc": "List the direct members of a project",
                "desc": "Lists the direct members of a project with their access levels.\n\n"
                    "Members who reach the project through a group are **not** included; use **List "
                    "Project Users** for everyone with access.",
                "groups": (GroupMembers,),
                "field_order": ("id", "query", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdMembers",
                "method": "POST",
                "path": "/api/v4/projects/{id}/members",
                "operation_id": "postApiV4ProjectsIdMembers",
                "display_name": "Add Project Member",
                "short_desc": "Add a user to a project",
                "desc": "Adds a user to a project at a given access level.\n\n"
                    "`access_level` is GitLab's numeric scale: 10 Guest, 20 Reporter, 30 Developer, "
                    "40 Maintainer, 50 Owner. Set `expires_at` for time-limited access.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "username", "access_level", "expires_at"),
                "fields": {
                    "id": ProjectOverlay,
                    "user_id": UserRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The user to add",
                        "preselected": True,
                    },
                    "access_level": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Access Level",
                        "short_desc": "10 Guest, 20 Reporter, 30 Developer, 40 Maintainer, 50 Owner",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdMembersUserId",
                "method": "GET",
                "path": "/api/v4/projects/{id}/members/{user_id}",
                "operation_id": "getApiV4ProjectsIdMembersUserId",
                "display_name": "Get Project Member",
                "short_desc": "Retrieve one direct member of a project",
                "desc": "Retrieves a direct member of a project with their access level and expiry.\n\n"
                    "A user who reaches the project only through a group is reported as not found.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id"),
                "fields": {
                    "id": ProjectOverlay,
                    "user_id": ProjectMemberSubjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdMembersUserId",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/members/{user_id}",
                "operation_id": "putApiV4ProjectsIdMembersUserId",
                "display_name": "Update Project Member",
                "short_desc": "Change a project member's access level or expiry",
                "desc": "Changes a member's access level or the date their access expires.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "access_level", "expires_at"),
                "fields": {
                    "id": ProjectOverlay,
                    "user_id": ProjectMemberSubjectOverlay,
                    "access_level": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Access Level",
                        "short_desc": "10 Guest, 20 Reporter, 30 Developer, 40 Maintainer, 50 Owner",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdMembersUserId",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/members/{user_id}",
                "operation_id": "deleteApiV4ProjectsIdMembersUserId",
                "display_name": "Remove Project Member",
                "short_desc": "Remove a user from a project",
                "desc": "Removes a direct member from a project.\n\n"
                    "Set `unassign_issuables` to take them off the issues and merge requests they were "
                    "assigned to; without it those assignments stay behind.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "unassign_issuables"),
                "fields": {
                    "id": ProjectOverlay,
                    "user_id": ProjectMemberSubjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4GroupsIdMembers",
                "method": "GET",
                "path": "/api/v4/groups/{id}/members",
                "operation_id": "getApiV4GroupsIdMembers",
                "display_name": "List Group Members",
                "short_desc": "List the direct members of a group",
                "desc": "Lists the direct members of a group with their access levels.\n\n"
                    "Members inherited from a parent group are not included.",
                "groups": (GroupMembers,),
                "field_order": ("id", "query", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": GroupOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4GroupsIdMembers",
                "method": "POST",
                "path": "/api/v4/groups/{id}/members",
                "operation_id": "postApiV4GroupsIdMembers",
                "display_name": "Add Group Member",
                "short_desc": "Add a user to a group",
                "desc": "Adds a user to a group at a given access level, which also grants that access to "
                    "every project in the group.\n\n"
                    "`access_level` is GitLab's numeric scale: 10 Guest, 20 Reporter, 30 Developer, "
                    "40 Maintainer, 50 Owner.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "username", "access_level", "expires_at"),
                "fields": {
                    "id": GroupOverlay,
                    "user_id": UserRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The user to add",
                        "preselected": True,
                    },
                    "access_level": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Access Level",
                        "short_desc": "10 Guest, 20 Reporter, 30 Developer, 40 Maintainer, 50 Owner",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4GroupsIdMembersUserId",
                "method": "GET",
                "path": "/api/v4/groups/{id}/members/{user_id}",
                "operation_id": "getApiV4GroupsIdMembersUserId",
                "display_name": "Get Group Member",
                "short_desc": "Retrieve one direct member of a group",
                "desc": "Retrieves a direct member of a group with their access level and expiry.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id"),
                "fields": {
                    "id": GroupOverlay,
                    "user_id": GroupMemberSubjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4GroupsIdMembersUserId",
                "method": "PUT",
                "path": "/api/v4/groups/{id}/members/{user_id}",
                "operation_id": "putApiV4GroupsIdMembersUserId",
                "display_name": "Update Group Member",
                "short_desc": "Change a group member's access level or expiry",
                "desc": "Changes a member's access level in a group, or the date their access expires.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "access_level", "expires_at"),
                "fields": {
                    "id": GroupOverlay,
                    "user_id": GroupMemberSubjectOverlay,
                    "access_level": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Access Level",
                        "short_desc": "10 Guest, 20 Reporter, 30 Developer, 40 Maintainer, 50 Owner",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4GroupsIdMembersUserId",
                "method": "DELETE",
                "path": "/api/v4/groups/{id}/members/{user_id}",
                "operation_id": "deleteApiV4GroupsIdMembersUserId",
                "display_name": "Remove Group Member",
                "short_desc": "Remove a user from a group",
                "desc": "Removes a member from a group, and with it their access to every project in the "
                    "group.\n\n"
                    "Set `skip_subresources` to leave their direct memberships of individual projects and "
                    "subgroups alone.",
                "groups": (GroupMembers,),
                "field_order": ("id", "user_id", "skip_subresources", "unassign_issuables"),
                "fields": {
                    "id": GroupOverlay,
                    "user_id": GroupMemberSubjectOverlay,
                },
            },

            # --- Merge Requests ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdMergeRequests",
                "method": "GET",
                "path": "/api/v4/projects/{id}/merge_requests",
                "operation_id": "getApiV4ProjectsIdMergeRequests",
                "display_name": "List Merge Requests",
                "short_desc": "List the merge requests in a project",
                "desc": "Lists merge requests.\n\n"
                    "`state` is `opened`, `closed`, `merged` or `locked`. Every user filter has a "
                    "`not[…]` counterpart that excludes instead of includes.\n\n"
                    "Set `with_merge_status_recheck` only when the mergeability flag matters: it asks "
                    "GitLab to recompute it, which is much slower.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "state", "search", "source_branch", "target_branch",
                    "assignee_id", "reviewer_id", "author_id", "labels", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "author_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Author",
                    },
                    "reviewer_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reviewer",
                    },
                    "merge_user_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Merged By",
                    },
                    "not[assignee_id]": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Assigned To",
                    },
                    "not[author_id]": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Authored By",
                    },
                    "not[reviewer_id]": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Not Reviewed By",
                    },
                    "source_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Branch",
                    },
                    "target_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Branch",
                    },
                    "source_project_id": ProjectRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Project",
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the title and description",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdMergeRequests",
                "method": "POST",
                "path": "/api/v4/projects/{id}/merge_requests",
                "operation_id": "postApiV4ProjectsIdMergeRequests",
                "display_name": "Create Merge Request",
                "short_desc": "Open a merge request",
                "desc": "Opens a merge request from one branch into another.\n\n"
                    "`source_branch` and `target_branch` are both in this project unless "
                    "`target_project_id` names a different one, which is how a fork proposes a change "
                    "upstream. A title beginning `Draft:` marks it as not ready to merge.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "source_branch", "target_branch", "title", "description",
                    "assignee_ids", "reviewer_ids", "labels", "milestone_id", "remove_source_branch",
                    "squash"),
                "fields": {
                    "id": ProjectOverlay,
                    "source_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Branch",
                        "short_desc": "The branch holding the changes",
                        "preselected": True,
                    },
                    "target_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Branch",
                        "short_desc": "The branch the changes are merged into",
                        "preselected": True,
                    },
                    "target_project_id": ProjectRefOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Project",
                        "short_desc": "Merge into a different project, ex: from a fork upstream",
                    },
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "assignee_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignees",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                    },
                    "reviewer_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reviewers",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "milestone_id": MilestoneOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "method": "GET",
                "path": "/api/v4/projects/{id}/merge_requests/{merge_request_iid}",
                "operation_id": "getApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "display_name": "Get Merge Request",
                "short_desc": "Retrieve one merge request",
                "desc": "Retrieves a merge request by its project-relative IID, with its state, its "
                    "mergeability and its pipeline.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "merge_request_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "merge_request_iid": MergeRequestIidOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/merge_requests/{merge_request_iid}",
                "operation_id": "putApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "display_name": "Update Merge Request",
                "short_desc": "Update a merge request, or close or reopen it",
                "desc": "Updates a merge request.\n\n"
                    "`state_event` closes (`close`) or reopens (`reopen`) it; this does **not** merge it. "
                    "`labels` replaces the whole label set, so use `add_labels` and `remove_labels` to "
                    "change one without disturbing the rest.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "merge_request_iid", "state_event", "title", "description",
                    "target_branch", "assignee_ids", "reviewer_ids", "add_labels", "remove_labels",
                    "milestone_id"),
                "fields": {
                    "id": ProjectOverlay,
                    "merge_request_iid": MergeRequestIidOverlay,
                    "target_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Branch",
                    },
                    "assignee_id": ProjectMemberOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignee",
                    },
                    "assignee_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Assignees",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                    },
                    "reviewer_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reviewers",
                        "element_ref_data": RefProjectMember,
                        "depends_on": ("id",),
                        "supports_custom_values": True,
                    },
                    "milestone_id": MilestoneOverlay,
                    "state_event": <RestSchemaFieldOverlayInfo>{
                        "display_name": "State Change",
                        "short_desc": "\"close\" to close the merge request, \"reopen\" to reopen it",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/merge_requests/{merge_request_iid}",
                "operation_id": "deleteApiV4ProjectsIdMergeRequestsMergeRequestIid",
                "display_name": "Delete Merge Request",
                "short_desc": "Delete a merge request permanently",
                "desc": "Deletes a merge request.\n\n"
                    "This is permanent and needs Owner rights; closing it with **Update Merge Request** is "
                    "almost always what is wanted instead.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "merge_request_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "merge_request_iid": MergeRequestIidOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdMergeRequestsMergeRequestIidApprove",
                "method": "POST",
                "path": "/api/v4/projects/{id}/merge_requests/{merge_request_iid}/approve",
                "operation_id": "postApiV4ProjectsIdMergeRequestsMergeRequestIidApprove",
                "display_name": "Approve Merge Request",
                "short_desc": "Approve a merge request as the authenticated user",
                "desc": "Approves a merge request on behalf of the authenticated user.\n\n"
                    "Supplying `sha` makes the approval conditional on the merge request still being at "
                    "that commit, so a push that lands between reading and approving is rejected rather "
                    "than silently approved.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "merge_request_iid", "sha"),
                "fields": {
                    "id": ProjectOverlay,
                    "merge_request_iid": MergeRequestIidOverlay,
                    "sha": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expected Head SHA",
                        "short_desc": "Approve only if the merge request is still at this commit",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdMergeRequestsMergeRequestIidUnapprove",
                "method": "POST",
                "path": "/api/v4/projects/{id}/merge_requests/{merge_request_iid}/unapprove",
                "operation_id": "postApiV4ProjectsIdMergeRequestsMergeRequestIidUnapprove",
                "display_name": "Unapprove Merge Request",
                "short_desc": "Withdraw the authenticated user's approval",
                "desc": "Withdraws the authenticated user's approval of a merge request. Another user's "
                    "approval is untouched.",
                "groups": (GroupMergeRequests,),
                "field_order": ("id", "merge_request_iid"),
                "fields": {
                    "id": ProjectOverlay,
                    "merge_request_iid": MergeRequestIidOverlay,
                },
            },

            # --- Projects ---
            <RestSchemaActionInfo>{
                "action": "getApiV4Projects",
                "method": "GET",
                "path": "/api/v4/projects",
                "operation_id": "getApiV4Projects",
                "display_name": "List Projects",
                "short_desc": "List the projects visible to the credential",
                "desc": "Lists projects.\n\n"
                    "Without a filter this returns every project the credential can see, which on "
                    "GitLab.com means every public project in existence. Set `membership`, `owned` or "
                    "`min_access_level` to restrict it to the projects the user actually works with - "
                    "that is what the project dropdowns in this application do.\n\n"
                    "`simple` returns a much smaller record per project and is worth setting when only "
                    "the ID and the name are needed.",
                "groups": (GroupProjects,),
                "field_order": ("membership", "search", "owned", "topic", "min_access_level", "simple",
                    "archived", "page", "per_page"),
                "fields": PagingOverlay + {
                    "membership": <RestSchemaFieldOverlayInfo>{
                        "display_name": "My Projects Only",
                        "short_desc": "Return only projects the authenticated user is a member of",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Match against the project name and path",
                        "preselected": True,
                    },
                    "topic_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Topic",
                        "short_desc": "Return only projects carrying this topic",
                        "ref_data": RefTopic,
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4Projects",
                "method": "POST",
                "path": "/api/v4/projects",
                "operation_id": "postApiV4Projects",
                "display_name": "Create Project",
                "short_desc": "Create a project",
                "desc": "Creates a project.\n\n"
                    "Without `namespace_id` the project is created under the authenticated user rather "
                    "than in a group. `path` is the URL segment and defaults to a slug of `name`. Set "
                    "`initialize_with_readme` to get a repository with a default branch, rather than an "
                    "empty one with no branches at all.",
                "groups": (GroupProjects,),
                "field_order": ("name", "path", "namespace_id", "description", "visibility",
                    "initialize_with_readme", "default_branch", "topics"),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                    "namespace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group",
                        "short_desc": "Create the project in this group; omit for the user's own namespace",
                        "ref_data": RefGroup,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "visibility": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Visibility",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsId",
                "method": "GET",
                "path": "/api/v4/projects/{id}",
                "operation_id": "getApiV4ProjectsId",
                "display_name": "Get Project",
                "short_desc": "Retrieve one project",
                "desc": "Retrieves a project, including its namespace, default branch and enabled "
                    "features.\n\n"
                    "Set `statistics` for repository and storage sizes; it is not returned by default "
                    "because it is expensive to compute.",
                "groups": (GroupProjects,),
                "field_order": ("id", "statistics", "license"),
                "fields": {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsId",
                "method": "PUT",
                "path": "/api/v4/projects/{id}",
                "operation_id": "putApiV4ProjectsId",
                "display_name": "Update Project",
                "short_desc": "Update a project's attributes",
                "desc": "Updates a project's name, description, visibility, default branch, merge "
                    "settings and feature access levels.\n\n"
                    "Only the fields supplied are changed. `topics` replaces the whole topic list.",
                "groups": (GroupProjects,),
                "field_order": ("id", "name", "description", "visibility", "default_branch", "topics"),
                "fields": {
                    "id": ProjectOverlay,
                    "default_branch": BranchOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Default Branch",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsId",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}",
                "operation_id": "deleteApiV4ProjectsId",
                "display_name": "Delete Project",
                "short_desc": "Schedule a project for deletion",
                "desc": "Schedules a project for deletion, together with its repository, issues, merge "
                    "requests and wiki.\n\n"
                    "This is not immediate: GitLab marks the project for deletion and removes it after a "
                    "retention period, during which it can be restored.",
                "groups": (GroupProjects,),
                "fields": {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4Topics",
                "method": "GET",
                "path": "/api/v4/topics",
                "operation_id": "getApiV4Topics",
                "display_name": "List Topics",
                "short_desc": "List the project topics defined on this instance",
                "desc": "Lists the topics projects can be tagged with.\n\n"
                    "This is what backs the topic filter on **List Projects**; the application this "
                    "module replaces offered that filter without exporting the endpoint that fills it.",
                "groups": (GroupProjects,),
                "field_order": ("search", "page", "per_page"),
                "fields": PagingOverlay + {
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "preselected": True,
                    },
                },
            },

            # --- Variables ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdVariables",
                "method": "GET",
                "path": "/api/v4/projects/{id}/variables",
                "operation_id": "getApiV4ProjectsIdVariables",
                "display_name": "List Project Variables",
                "short_desc": "List a project's CI/CD variables",
                "desc": "Lists the CI/CD variables defined on a project.\n\n"
                    "A masked variable's value is returned; a **hidden** variable's is not, and never can "
                    "be read back through the API.",
                "groups": (GroupVariables,),
                "field_order": ("id", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdVariables",
                "method": "POST",
                "path": "/api/v4/projects/{id}/variables",
                "operation_id": "postApiV4ProjectsIdVariables",
                "display_name": "Create Project Variable",
                "short_desc": "Create a CI/CD variable on a project",
                "desc": "Creates a CI/CD variable.\n\n"
                    "`protected` restricts it to pipelines on protected branches and tags; `masked` hides "
                    "it in job logs, which requires the value to meet GitLab's masking rules "
                    "(no newlines, minimum length). `masked_and_hidden` additionally makes the value "
                    "unreadable through the API afterwards - set it for a real secret, and store the "
                    "value somewhere else as well, because it cannot be retrieved.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "value", "protected", "masked", "masked_and_hidden",
                    "environment_scope", "variable_type", "description"),
                "fields": {
                    "id": ProjectOverlay,
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Key",
                        "short_desc": "The variable name, ex: \"DEPLOY_TOKEN\"",
                        "preselected": True,
                    },
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "sensitive": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdVariablesKey",
                "method": "GET",
                "path": "/api/v4/projects/{id}/variables/{key}",
                "operation_id": "getApiV4ProjectsIdVariablesKey",
                "display_name": "Get Project Variable",
                "short_desc": "Retrieve one of a project's CI/CD variables",
                "desc": "Retrieves a CI/CD variable by key.\n\n"
                    "When the same key exists for several environments, `filter[environment_scope]` "
                    "selects which one; without it GitLab returns whichever it finds first.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "filter[environment_scope]"),
                "fields": {
                    "id": ProjectOverlay,
                    "key": ProjectVariableKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdVariablesKey",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/variables/{key}",
                "operation_id": "putApiV4ProjectsIdVariablesKey",
                "display_name": "Update Project Variable",
                "short_desc": "Update one of a project's CI/CD variables",
                "desc": "Updates a CI/CD variable's value or flags.\n\n"
                    "When the same key exists for several environments, `filter` selects which one.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "value", "protected", "masked", "environment_scope"),
                "fields": {
                    "id": ProjectOverlay,
                    "key": ProjectVariableKeyOverlay,
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "sensitive": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdVariablesKey",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/variables/{key}",
                "operation_id": "deleteApiV4ProjectsIdVariablesKey",
                "display_name": "Delete Project Variable",
                "short_desc": "Delete one of a project's CI/CD variables",
                "desc": "Deletes a CI/CD variable. Pipelines already running keep the value they started "
                    "with.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "filter[environment_scope]"),
                "fields": {
                    "id": ProjectOverlay,
                    "key": ProjectVariableKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4GroupsIdVariables",
                "method": "GET",
                "path": "/api/v4/groups/{id}/variables",
                "operation_id": "getApiV4GroupsIdVariables",
                "display_name": "List Group Variables",
                "short_desc": "List a group's CI/CD variables",
                "desc": "Lists the CI/CD variables defined on a group; every project in the group "
                    "inherits them unless it defines a variable with the same key.",
                "groups": (GroupVariables,),
                "field_order": ("id", "page", "per_page"),
                "fields": PagingOverlay + {
                    "id": GroupOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4GroupsIdVariables",
                "method": "POST",
                "path": "/api/v4/groups/{id}/variables",
                "operation_id": "postApiV4GroupsIdVariables",
                "display_name": "Create Group Variable",
                "short_desc": "Create a CI/CD variable on a group",
                "desc": "Creates a CI/CD variable on a group, inherited by every project in it.\n\n"
                    "The flags mean the same as on a project variable: `masked_and_hidden` in particular "
                    "makes the value unreadable through the API afterwards.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "value", "protected", "masked", "masked_and_hidden",
                    "environment_scope", "variable_type", "description"),
                "fields": {
                    "id": GroupOverlay,
                    "key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Key",
                        "short_desc": "The variable name, ex: \"DEPLOY_TOKEN\"",
                        "preselected": True,
                    },
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "sensitive": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4GroupsIdVariablesKey",
                "method": "GET",
                "path": "/api/v4/groups/{id}/variables/{key}",
                "operation_id": "getApiV4GroupsIdVariablesKey",
                "display_name": "Get Group Variable",
                "short_desc": "Retrieve one of a group's CI/CD variables",
                "desc": "Retrieves a CI/CD variable defined on a group.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key"),
                "fields": {
                    "id": GroupOverlay,
                    "key": GroupVariableKeyOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4GroupsIdVariablesKey",
                "method": "PUT",
                "path": "/api/v4/groups/{id}/variables/{key}",
                "operation_id": "putApiV4GroupsIdVariablesKey",
                "display_name": "Update Group Variable",
                "short_desc": "Update one of a group's CI/CD variables",
                "desc": "Updates a group CI/CD variable's value or flags. Every project inheriting it "
                    "sees the new value on its next pipeline.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key", "value", "protected", "masked", "environment_scope"),
                "fields": {
                    "id": GroupOverlay,
                    "key": GroupVariableKeyOverlay,
                    "value": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Value",
                        "sensitive": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4GroupsIdVariablesKey",
                "method": "DELETE",
                "path": "/api/v4/groups/{id}/variables/{key}",
                "operation_id": "deleteApiV4GroupsIdVariablesKey",
                "display_name": "Delete Group Variable",
                "short_desc": "Delete one of a group's CI/CD variables",
                "desc": "Deletes a group CI/CD variable. Projects that inherited it lose the value; one "
                    "that defined its own variable with the same key is unaffected.",
                "groups": (GroupVariables,),
                "field_order": ("id", "key"),
                "fields": {
                    "id": GroupOverlay,
                    "key": GroupVariableKeyOverlay,
                },
            },

            # --- Wikis ---
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdWikis",
                "method": "GET",
                "path": "/api/v4/projects/{id}/wikis",
                "operation_id": "getApiV4ProjectsIdWikis",
                "display_name": "List Wiki Pages",
                "short_desc": "List a project's wiki pages",
                "desc": "Lists the wiki pages of a project.\n\n"
                    "The content is omitted unless `with_content` is set, which is worth leaving unset "
                    "when only the titles and slugs are needed.",
                "groups": (GroupWikis,),
                "field_order": ("id", "with_content"),
                "fields": {
                    "id": ProjectOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "postApiV4ProjectsIdWikis",
                "method": "POST",
                "path": "/api/v4/projects/{id}/wikis",
                "operation_id": "postApiV4ProjectsIdWikis",
                "display_name": "Create Wiki Page",
                "short_desc": "Create a wiki page in a project",
                "desc": "Creates a wiki page.\n\n"
                    "The slug is derived from the title; a title containing `/` creates the page in a "
                    "subdirectory of the wiki. `format` defaults to Markdown.",
                "groups": (GroupWikis,),
                "field_order": ("id", "title", "content", "format"),
                "fields": {
                    "id": ProjectOverlay,
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "short_desc": "The page title; a \"/\" creates the page in a subdirectory",
                        "preselected": True,
                    },
                    "content": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Content",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getApiV4ProjectsIdWikisSlug",
                "method": "GET",
                "path": "/api/v4/projects/{id}/wikis/{slug}",
                "operation_id": "getApiV4ProjectsIdWikisSlug",
                "display_name": "Get Wiki Page",
                "short_desc": "Retrieve one wiki page",
                "desc": "Retrieves a wiki page with its content.\n\n"
                    "`version` reads an earlier revision by commit SHA, and `render_html` returns the "
                    "rendered HTML alongside the source.",
                "groups": (GroupWikis,),
                "field_order": ("id", "slug", "version", "render_html"),
                "fields": {
                    "id": ProjectOverlay,
                    "slug": WikiSlugOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "putApiV4ProjectsIdWikisSlug",
                "method": "PUT",
                "path": "/api/v4/projects/{id}/wikis/{slug}",
                "operation_id": "putApiV4ProjectsIdWikisSlug",
                "display_name": "Update Wiki Page",
                "short_desc": "Update a wiki page's title or content",
                "desc": "Updates a wiki page.\n\n"
                    "Changing the title moves the page, which changes its slug and breaks links to the "
                    "old one.",
                "groups": (GroupWikis,),
                "field_order": ("id", "slug", "title", "content", "format"),
                "fields": {
                    "id": ProjectOverlay,
                    "slug": WikiSlugOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "deleteApiV4ProjectsIdWikisSlug",
                "method": "DELETE",
                "path": "/api/v4/projects/{id}/wikis/{slug}",
                "operation_id": "deleteApiV4ProjectsIdWikisSlug",
                "display_name": "Delete Wiki Page",
                "short_desc": "Delete a wiki page",
                "desc": "Deletes a wiki page. The wiki is a git repository, so the page remains in its "
                    "history.",
                "groups": (GroupWikis,),
                "field_order": ("id", "slug"),
                "fields": {
                    "id": ProjectOverlay,
                    "slug": WikiSlugOverlay,
                },
            },
        );
    }
}
}
