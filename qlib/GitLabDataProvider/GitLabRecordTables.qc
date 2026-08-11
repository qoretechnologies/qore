# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitLabDataProvider module definition

/*  GitLabRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The GitLab record-based tables
/** Twelve of GitLab's collections are record-shaped and are exposed as tables.  Each is built from actions
    the manifest already exports, so the paths, the request contracts and the escaping are the pinned
    schema's.

    @section gitlabrecord_pushdown GitLab Filters More Than Anything Else Here

    GitLab's listings take by far the richest filter set of the applications in this family, and this module
    sends what a search asks for: an issue or merge request by state, labels, author, assignee, milestone or
    internal ID, a project or group by visibility, a merge request by source and target branch.

    The date bounds are exact.  GitLab documents \c created_after as "created on or after the given time" and
    \c created_before as "on or before", so the two carry \c >= and \c <= with their boundaries included, and
    the predicate is answered by GitLab rather than re-checked here.

    \c search and \c query are GitLab's own loose matches over a title or a name.  They are not predicates,
    so they stay ordinary search options rather than being wired to an operator; a caller who means
    "contains" gets exactly that, evaluated here.

    @section gitlabrecord_iid The Identity Is Usually the Internal ID

    An issue and a merge request each carry GitLab's global \c id and the \c iid that identifies it **within
    its project**, and every endpoint that reads or changes one takes the \c iid.  So that is what a record is
    addressed by here, and \c iids is the listing argument a search on it is sent to.

    A CI variable is addressed by its \c key, a branch by its name, a wiki page by its \c slug and a member by
    its \c id, because that is what each of those paths takes.

    @section gitlabrecord_type GitLab Declares a Collection as One Entity

    GitLab's published description declares the 200 response of every listing it exports as a **single**
    entity - \c APIEntitiesIssue rather than an array of it - though the body it returns is an array.  That
    is an upstream defect, and it is why these tables derive their record type from the response's own fields
    rather than from an element type there is none of.  Nothing else depends on it: the body really is an
    array at run time, and that is what the tables read.

    @section gitlabrecord_wikis Wiki Pages Are Not Paged

    GitLab's wiki listing takes no \c page or \c per_page: it answers with every page in the project's wiki.
    That table therefore declares @ref RSRP_NONE and reads it once, rather than sending arguments GitLab
    ignores and then waiting for a short page that never comes.
*/
public class GitLabRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! How a paged GitLab collection pages
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "per_page",
            "max_size": 100,
        };

        #! How a GitLab collection that takes no paging arguments is read
        const NoPaging = <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE};

        #! The date bounds an issue and a merge request share
        /** GitLab documents both bounds as inclusive, so each carries its operator exactly.
        */
        const DateFilters = (
            <RestSchemaRecordFilterInfo>{
                "field": "created_at", "operator": DP_SEARCH_OP_GE, "option": "created_after", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "created_at", "operator": DP_SEARCH_OP_LE, "option": "created_before",
                "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "updated_at", "operator": DP_SEARCH_OP_GE, "option": "updated_after", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "updated_at", "operator": DP_SEARCH_OP_LE, "option": "updated_before",
                "exact": True,
            },
        );

        #! The issues table
        const IssuesTable = <RestSchemaRecordTableInfo>{
            "name": "issues",
            "display_name": "Issues",
            "short_desc": "the issues in one project",
            "desc": "The issues in one project, as records",
            "list_action": "getApiV4ProjectsIdIssues",
            "get_action": "getApiV4ProjectsIdIssuesIssueIid",
            "create_action": "postApiV4ProjectsIdIssues",
            "update_action": "putApiV4ProjectsIdIssuesIssueIid",
            "delete_action": "deleteApiV4ProjectsIdIssuesIssueIid",
            "id_field": "iid",
            "id_option": "issue_iid",
            "scope_options": ("id",),
            "paging": Paging,
            "search_options": ("state", "search", "scope", "order_by", "sort", "confidential",
                "issue_type"),
            "filters": DateFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "state", "operator": DP_SEARCH_OP_EQ, "option": "state", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "iid", "operator": DP_SEARCH_OP_IN, "option": "iids", "multi": True,
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "labels", "operator": DP_SEARCH_OP_IN, "option": "labels", "multi": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "milestone", "operator": DP_SEARCH_OP_EQ, "option": "milestone", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "issue_type", "operator": DP_SEARCH_OP_EQ, "option": "issue_type",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "confidential", "operator": DP_SEARCH_OP_EQ, "option": "confidential",
                    "exact": True,
                },
            ),
        };

        #! The merge requests table
        const MergeRequestsTable = <RestSchemaRecordTableInfo>{
            "name": "merge-requests",
            "display_name": "Merge Requests",
            "short_desc": "the merge requests in one project",
            "desc": "The merge requests in one project, as records",
            "list_action": "getApiV4ProjectsIdMergeRequests",
            "get_action": "getApiV4ProjectsIdMergeRequestsMergeRequestIid",
            "create_action": "postApiV4ProjectsIdMergeRequests",
            "update_action": "putApiV4ProjectsIdMergeRequestsMergeRequestIid",
            "delete_action": "deleteApiV4ProjectsIdMergeRequestsMergeRequestIid",
            "id_field": "iid",
            "id_option": "merge_request_iid",
            "scope_options": ("id",),
            "paging": Paging,
            "search_options": ("state", "search", "scope", "view", "order_by", "sort", "wip"),
            "filters": DateFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "state", "operator": DP_SEARCH_OP_EQ, "option": "state", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "source_branch", "operator": DP_SEARCH_OP_EQ, "option": "source_branch",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "target_branch", "operator": DP_SEARCH_OP_EQ, "option": "target_branch",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "labels", "operator": DP_SEARCH_OP_IN, "option": "labels", "multi": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "milestone", "operator": DP_SEARCH_OP_EQ, "option": "milestone", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "draft", "operator": DP_SEARCH_OP_EQ, "option": "draft", "exact": True,
                },
            ),
        };

        #! The projects table
        const ProjectsTable = <RestSchemaRecordTableInfo>{
            "name": "projects",
            "display_name": "Projects",
            "short_desc": "the projects the credential can see",
            "desc": "The projects the credential can see, as records",
            "list_action": "getApiV4Projects",
            "get_action": "getApiV4ProjectsId",
            "create_action": "postApiV4Projects",
            "update_action": "putApiV4ProjectsId",
            "delete_action": "deleteApiV4ProjectsId",
            "id_field": "id",
            "paging": Paging,
            "search_options": ("membership", "owned", "starred", "search", "order_by", "sort", "archived",
                "active"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "visibility", "operator": DP_SEARCH_OP_EQ, "option": "visibility",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "archived", "operator": DP_SEARCH_OP_EQ, "option": "archived", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "last_activity_at", "operator": DP_SEARCH_OP_GE,
                    "option": "last_activity_after", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "last_activity_at", "operator": DP_SEARCH_OP_LE,
                    "option": "last_activity_before", "exact": True,
                },
            ),
        };

        #! The groups table
        const GroupsTable = <RestSchemaRecordTableInfo>{
            "name": "groups",
            "display_name": "Groups",
            "short_desc": "the groups the credential can see",
            "desc": "The groups the credential can see, as records",
            "list_action": "getApiV4Groups",
            "get_action": "getApiV4GroupsId",
            "create_action": "postApiV4Groups",
            "update_action": "putApiV4GroupsId",
            "delete_action": "deleteApiV4GroupsId",
            "id_field": "id",
            "paging": Paging,
            "search_options": ("search", "owned", "top_level_only", "all_available", "order_by", "sort",
                "archived", "active"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "visibility", "operator": DP_SEARCH_OP_EQ, "option": "visibility",
                    "exact": True,
                },
            ),
        };

        #! The project members table
        const ProjectMembersTable = <RestSchemaRecordTableInfo>{
            "name": "project-members",
            "display_name": "Project Members",
            "short_desc": "the members of one project",
            "desc": "The members of one project, as records",
            "list_action": "getApiV4ProjectsIdMembers",
            "get_action": "getApiV4ProjectsIdMembersUserId",
            "create_action": "postApiV4ProjectsIdMembers",
            "update_action": "putApiV4ProjectsIdMembersUserId",
            "delete_action": "deleteApiV4ProjectsIdMembersUserId",
            "id_field": "id",
            "id_option": "user_id",
            "scope_options": ("id",),
            "paging": Paging,
            "search_options": ("query",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_IN, "option": "user_ids", "multi": True,
                    "exact": True,
                },
            ),
        };

        #! The group members table
        const GroupMembersTable = <RestSchemaRecordTableInfo>{
            "name": "group-members",
            "display_name": "Group Members",
            "short_desc": "the members of one group",
            "desc": "The members of one group, as records",
            "list_action": "getApiV4GroupsIdMembers",
            "get_action": "getApiV4GroupsIdMembersUserId",
            "create_action": "postApiV4GroupsIdMembers",
            "update_action": "putApiV4GroupsIdMembersUserId",
            "delete_action": "deleteApiV4GroupsIdMembersUserId",
            "id_field": "id",
            "id_option": "user_id",
            "scope_options": ("id",),
            "paging": Paging,
            "search_options": ("query",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "id", "operator": DP_SEARCH_OP_IN, "option": "user_ids", "multi": True,
                    "exact": True,
                },
            ),
        };

        #! The project CI/CD variables table
        /** A CI variable is addressed by its \c key rather than by an ID, because that is what GitLab's
            paths take.
        */
        const ProjectVariablesTable = <RestSchemaRecordTableInfo>{
            "name": "project-variables",
            "display_name": "Project Variables",
            "short_desc": "the CI/CD variables of one project",
            "desc": "The CI/CD variables of one project, as records; a variable is addressed by its key",
            "list_action": "getApiV4ProjectsIdVariables",
            "get_action": "getApiV4ProjectsIdVariablesKey",
            "create_action": "postApiV4ProjectsIdVariables",
            "update_action": "putApiV4ProjectsIdVariablesKey",
            "delete_action": "deleteApiV4ProjectsIdVariablesKey",
            "id_field": "key",
            "scope_options": ("id",),
            "paging": Paging,
        };

        #! The group CI/CD variables table
        const GroupVariablesTable = <RestSchemaRecordTableInfo>{
            "name": "group-variables",
            "display_name": "Group Variables",
            "short_desc": "the CI/CD variables of one group",
            "desc": "The CI/CD variables of one group, as records; a variable is addressed by its key",
            "list_action": "getApiV4GroupsIdVariables",
            "get_action": "getApiV4GroupsIdVariablesKey",
            "create_action": "postApiV4GroupsIdVariables",
            "update_action": "putApiV4GroupsIdVariablesKey",
            "delete_action": "deleteApiV4GroupsIdVariablesKey",
            "id_field": "key",
            "scope_options": ("id",),
            "paging": Paging,
        };

        #! The branches table
        /** GitLab creates a branch and deletes one; it does not edit one, so there is no update.
        */
        const BranchesTable = <RestSchemaRecordTableInfo>{
            "name": "branches",
            "display_name": "Branches",
            "short_desc": "the branches of one project",
            "desc": "The branches of one project, as records; a branch is created and deleted rather than "
                "edited",
            "list_action": "getApiV4ProjectsIdRepositoryBranches",
            "get_action": "getApiV4ProjectsIdRepositoryBranchesBranch",
            "create_action": "postApiV4ProjectsIdRepositoryBranches",
            "delete_action": "deleteApiV4ProjectsIdRepositoryBranchesBranch",
            "id_field": "name",
            "id_option": "branch",
            "scope_options": ("id",),
            "paging": Paging,
            "search_options": ("search", "regex", "sort"),
        };

        #! The deploy keys table
        const DeployKeysTable = <RestSchemaRecordTableInfo>{
            "name": "deploy-keys",
            "display_name": "Deploy Keys",
            "short_desc": "the deploy keys of one project",
            "desc": "The deploy keys of one project, as records",
            "list_action": "getApiV4ProjectsIdDeployKeys",
            "get_action": "getApiV4ProjectsIdDeployKeysKeyId",
            "create_action": "postApiV4ProjectsIdDeployKeys",
            "update_action": "putApiV4ProjectsIdDeployKeysKeyId",
            "delete_action": "deleteApiV4ProjectsIdDeployKeysKeyId",
            "id_field": "id",
            "id_option": "key_id",
            "scope_options": ("id",),
            "paging": Paging,
        };

        #! The wiki pages table
        /** GitLab's wiki listing takes no paging arguments: it answers with every page in the wiki.
        */
        const WikisTable = <RestSchemaRecordTableInfo>{
            "name": "wikis",
            "display_name": "Wiki Pages",
            "short_desc": "the wiki pages of one project",
            "desc": "The wiki pages of one project, as records; GitLab returns them all in one response "
                "rather than paging them",
            "list_action": "getApiV4ProjectsIdWikis",
            "get_action": "getApiV4ProjectsIdWikisSlug",
            "create_action": "postApiV4ProjectsIdWikis",
            "update_action": "putApiV4ProjectsIdWikisSlug",
            "delete_action": "deleteApiV4ProjectsIdWikisSlug",
            "id_field": "slug",
            "scope_options": ("id",),
            "paging": NoPaging,
        };

        #! The commit comments table
        const CommitCommentsTable = <RestSchemaRecordTableInfo>{
            "name": "commit-comments",
            "display_name": "Commit Comments",
            "short_desc": "the comments on one commit",
            "desc": "The comments on one commit, as records",
            "list_action": "getApiV4ProjectsIdRepositoryCommitsShaComments",
            "create_action": "postApiV4ProjectsIdRepositoryCommitsShaComments",
            "id_field": "note",
            "scope_options": ("id", "sha"),
            "paging": Paging,
        };

        #! Every table, in presentation order
        const Tables = (ProjectsTable, GroupsTable, IssuesTable, MergeRequestsTable, BranchesTable,
            CommitCommentsTable, ProjectMembersTable, GroupMembersTable, ProjectVariablesTable,
            GroupVariablesTable, DeployKeysTable, WikisTable);

        #! The IDs the record-based actions are registered under
        /** The application this module replaces offered no record-based actions, so these are the
            conventional IDs.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{};
    }
}
}
