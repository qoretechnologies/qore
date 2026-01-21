# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearDataProviderDefs.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear app name constant
public const AppName = "Linear";

#! Default page size for pagination
public const DefaultPageSize = 50;

#! Issue fields fragment for GraphQL queries
public const IssueFieldsFragment = "
    id
    identifier
    title
    description
    priority
    estimate
    url
    state {
        id
        name
        type
    }
    assignee {
        id
        name
        email
    }
    team {
        id
        name
        key
    }
    project {
        id
        name
    }
    cycle {
        id
        name
        number
    }
    labels {
        nodes {
            id
            name
            color
        }
    }
    parent {
        id
        identifier
    }
    createdAt
    updatedAt
    archivedAt
";

#! Team fields fragment for GraphQL queries
public const TeamFieldsFragment = "
    id
    name
    key
    description
    private
    timezone
    createdAt
    updatedAt
";

#! Project fields fragment for GraphQL queries
public const ProjectFieldsFragment = "
    id
    name
    description
    slugId
    state
    url
    startDate
    targetDate
    lead {
        id
        name
    }
    createdAt
    updatedAt
";

#! Cycle fields fragment for GraphQL queries
public const CycleFieldsFragment = "
    id
    name
    number
    startsAt
    endsAt
    team {
        id
        name
    }
    createdAt
    updatedAt
";

#! Label fields fragment for GraphQL queries
public const LabelFieldsFragment = "
    id
    name
    description
    color
    team {
        id
        name
    }
    createdAt
    updatedAt
";

#! Comment fields fragment for GraphQL queries
public const CommentFieldsFragment = "
    id
    body
    url
    user {
        id
        name
        email
    }
    issue {
        id
        identifier
    }
    createdAt
    updatedAt
";

#! User fields fragment for GraphQL queries
public const UserFieldsFragment = "
    id
    name
    email
    displayName
    avatarUrl
    active
    createdAt
    updatedAt
";

#! GraphQL query for listing issues with pagination
public const ListIssuesQuery = "
query ListIssues($first: Int, $after: String, $filter: IssueFilter, $orderBy: PaginationOrderBy) {
    issues(first: $first, after: $after, filter: $filter, orderBy: $orderBy) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {" + IssueFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single issue
public const GetIssueQuery = "
query GetIssue($id: String!) {
    issue(id: $id) {" + IssueFieldsFragment + "
    }
}
";

#! GraphQL mutation for creating an issue
public const CreateIssueMutation = "
mutation CreateIssue($input: IssueCreateInput!) {
    issueCreate(input: $input) {
        success
        issue {" + IssueFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating an issue
public const UpdateIssueMutation = "
mutation UpdateIssue($id: String!, $input: IssueUpdateInput!) {
    issueUpdate(id: $id, input: $input) {
        success
        issue {" + IssueFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for archiving an issue
public const ArchiveIssueMutation = "
mutation ArchiveIssue($id: String!) {
    issueArchive(id: $id) {
        success
    }
}
";

#! GraphQL query for listing teams with pagination
public const ListTeamsQuery = "
query ListTeams($first: Int, $after: String) {
    teams(first: $first, after: $after) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {" + TeamFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single team
public const GetTeamQuery = "
query GetTeam($id: String!) {
    team(id: $id) {" + TeamFieldsFragment + "
    }
}
";

#! GraphQL query for listing projects with pagination
public const ListProjectsQuery = "
query ListProjects($first: Int, $after: String, $filter: ProjectFilter) {
    projects(first: $first, after: $after, filter: $filter) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {" + ProjectFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single project
public const GetProjectQuery = "
query GetProject($id: String!) {
    project(id: $id) {" + ProjectFieldsFragment + "
    }
}
";

#! GraphQL mutation for creating a project
public const CreateProjectMutation = "
mutation CreateProject($input: ProjectCreateInput!) {
    projectCreate(input: $input) {
        success
        project {" + ProjectFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a project
public const UpdateProjectMutation = "
mutation UpdateProject($id: String!, $input: ProjectUpdateInput!) {
    projectUpdate(id: $id, input: $input) {
        success
        project {" + ProjectFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for archiving a project
public const ArchiveProjectMutation = "
mutation ArchiveProject($id: String!) {
    projectArchive(id: $id) {
        success
    }
}
";

#! GraphQL query for listing cycles with pagination
public const ListCyclesQuery = "
query ListCycles($first: Int, $after: String, $filter: CycleFilter) {
    cycles(first: $first, after: $after, filter: $filter) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {" + CycleFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single cycle
public const GetCycleQuery = "
query GetCycle($id: String!) {
    cycle(id: $id) {" + CycleFieldsFragment + "
    }
}
";

#! GraphQL mutation for creating a cycle
public const CreateCycleMutation = "
mutation CreateCycle($input: CycleCreateInput!) {
    cycleCreate(input: $input) {
        success
        cycle {" + CycleFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a cycle
public const UpdateCycleMutation = "
mutation UpdateCycle($id: String!, $input: CycleUpdateInput!) {
    cycleUpdate(id: $id, input: $input) {
        success
        cycle {" + CycleFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for archiving a cycle
public const ArchiveCycleMutation = "
mutation ArchiveCycle($id: String!) {
    cycleArchive(id: $id) {
        success
    }
}
";

#! GraphQL query for listing labels with pagination
public const ListLabelsQuery = "
query ListLabels($first: Int, $after: String, $filter: IssueLabelFilter) {
    issueLabels(first: $first, after: $after, filter: $filter) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {" + LabelFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single label
public const GetLabelQuery = "
query GetLabel($id: String!) {
    issueLabel(id: $id) {" + LabelFieldsFragment + "
    }
}
";

#! GraphQL mutation for creating a label
public const CreateLabelMutation = "
mutation CreateLabel($input: IssueLabelCreateInput!) {
    issueLabelCreate(input: $input) {
        success
        issueLabel {" + LabelFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a label
public const UpdateLabelMutation = "
mutation UpdateLabel($id: String!, $input: IssueLabelUpdateInput!) {
    issueLabelUpdate(id: $id, input: $input) {
        success
        issueLabel {" + LabelFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for deleting a label
public const DeleteLabelMutation = "
mutation DeleteLabel($id: String!) {
    issueLabelDelete(id: $id) {
        success
    }
}
";

#! GraphQL query for listing comments on an issue
public const ListCommentsQuery = "
query ListComments($issueId: String!, $first: Int, $after: String) {
    issue(id: $issueId) {
        comments(first: $first, after: $after) {
            pageInfo {
                hasNextPage
                endCursor
            }
            nodes {" + CommentFieldsFragment + "
            }
        }
    }
}
";

#! GraphQL query for getting a single comment
public const GetCommentQuery = "
query GetComment($id: String!) {
    comment(id: $id) {" + CommentFieldsFragment + "
    }
}
";

#! GraphQL mutation for creating a comment
public const CreateCommentMutation = "
mutation CreateComment($input: CommentCreateInput!) {
    commentCreate(input: $input) {
        success
        comment {" + CommentFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a comment
public const UpdateCommentMutation = "
mutation UpdateComment($id: String!, $input: CommentUpdateInput!) {
    commentUpdate(id: $id, input: $input) {
        success
        comment {" + CommentFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for deleting a comment
public const DeleteCommentMutation = "
mutation DeleteComment($id: String!) {
    commentDelete(id: $id) {
        success
    }
}
";

#! GraphQL query for the current user (viewer)
public const ViewerQuery = "
query Viewer {
    viewer {" + UserFieldsFragment + "
    }
}
";

#! GraphQL query for listing users with pagination (for reference data)
public const ListUsersQuery = "
query ListUsers($first: Int, $after: String) {
    users(first: $first, after: $after) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {
            id
            name
            email
            displayName
            active
        }
    }
}
";

#! GraphQL query for listing workflow states with pagination (for reference data)
public const ListWorkflowStatesQuery = "
query ListWorkflowStates($first: Int, $after: String, $filter: WorkflowStateFilter) {
    workflowStates(first: $first, after: $after, filter: $filter) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {
            id
            name
            type
            team {
                id
                name
            }
        }
    }
}
";

#! GraphQL query for listing issues for reference data (lightweight)
public const ListIssuesRefQuery = "
query ListIssuesRef($first: Int, $after: String, $filter: IssueFilter) {
    issues(first: $first, after: $after, filter: $filter) {
        pageInfo {
            hasNextPage
            endCursor
        }
        nodes {
            id
            identifier
            title
        }
    }
}
";

#! Webhook fields fragment for GraphQL queries
public const WebhookFieldsFragment = "
    id
    label
    url
    enabled
    team {
        id
        name
    }
    resourceTypes
    createdAt
    updatedAt
";

#! GraphQL mutation for creating a webhook
public const CreateWebhookMutation = "
mutation CreateWebhook($input: WebhookCreateInput!) {
    webhookCreate(input: $input) {
        success
        webhook {" + WebhookFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for deleting a webhook
public const DeleteWebhookMutation = "
mutation DeleteWebhook($id: String!) {
    webhookDelete(id: $id) {
        success
    }
}
";

#! Linear logo in SVG format (loaded from file)
public const LinearLogo = File::readTextFile(get_script_dir() + "/linear-logo.svg");

} # namespace LinearDataProvider
