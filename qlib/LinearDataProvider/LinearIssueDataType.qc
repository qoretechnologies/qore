# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearIssueDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear issue record type
public class LinearIssueDataType inherits HashDataType {
    private {
        #! Field definitions for Linear issues
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the issue",
                "desc": "The unique identifier of the issue",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "identifier": {
                "type": StringType,
                "display_name": "Identifier",
                "short_desc": "The issue's human-readable identifier (e.g., ENG-123)",
                "desc": "The issue's human-readable identifier combining team key and issue number",
                "example_value": "ENG-123",
            },
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The issue title",
                "desc": "The title of the issue",
                "example_value": "Fix login button not responding",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The issue description in markdown format",
                "example_value": "The login button on the main page doesn't respond to clicks.",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The priority of the issue: 0 = No priority, 1 = Urgent, 2 = High, 3 = Normal, 4 = Low",
                "example_value": 2,
            },
            "estimate": {
                "type": FloatOrNothingType,
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The estimated complexity/effort for the issue",
                "example_value": 3.0,
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The issue URL",
                "desc": "The URL to the issue in Linear",
                "example_value": "https://linear.app/myteam/issue/ENG-123",
            },
            "state": {
                "type": new LinearStateRefDataType(),
                "display_name": "State",
                "short_desc": "The issue state/status",
                "desc": "The current workflow state of the issue (id, name, type)",
            },
            "assignee": {
                "type": new LinearUserRefDataType(),
                "display_name": "Assignee",
                "short_desc": "The assigned user",
                "desc": "The user assigned to the issue (id, name, email)",
            },
            "team": {
                "type": new LinearTeamRefDataType(),
                "display_name": "Team",
                "short_desc": "The team",
                "desc": "The team the issue belongs to (id, name, key)",
            },
            "project": {
                "type": new LinearProjectRefDataType(),
                "display_name": "Project",
                "short_desc": "The project",
                "desc": "The project the issue belongs to (id, name)",
            },
            "cycle": {
                "type": new LinearCycleRefDataType(),
                "display_name": "Cycle",
                "short_desc": "The cycle/sprint",
                "desc": "The cycle (sprint) the issue is assigned to (id, name, number)",
            },
            "labels": {
                "type": new ListDataType("LabelRefList", new LinearLabelRefDataType(), True),
                "display_name": "Labels",
                "short_desc": "The issue labels",
                "desc": "Labels attached to the issue (array of label objects with id, name, color)",
            },
            "parent": {
                "type": new LinearParentIssueRefDataType(),
                "display_name": "Parent Issue",
                "short_desc": "The parent issue",
                "desc": "The parent issue if this is a sub-issue (id, identifier)",
            },
            "number": {
                "type": IntOrNothingType,
                "display_name": "Number",
                "short_desc": "The issue number",
                "desc": "The issue number within the team (e.g., 123 in ENG-123)",
                "example_value": 123,
            },
            "sortOrder": {
                "type": FloatOrNothingType,
                "display_name": "Sort Order",
                "short_desc": "The sort order",
                "desc": "The order of the issue in relation to other issues",
                "example_value": 1234.56,
            },
            "startedAt": {
                "type": DateOrNothingType,
                "display_name": "Started At",
                "short_desc": "When work started on the issue",
                "desc": "The date and time when the issue was moved to a started state",
            },
            "completedAt": {
                "type": DateOrNothingType,
                "display_name": "Completed At",
                "short_desc": "When the issue was completed",
                "desc": "The date and time when the issue was completed",
            },
            "teamId": {
                "type": StringOrNothingType,
                "display_name": "Team ID",
                "short_desc": "The team ID",
                "desc": "The ID of the team the issue belongs to",
                "example_value": "72b2a2dc-6f4f-4423-9d34-24b5bd10634a",
            },
            "cycleId": {
                "type": StringOrNothingType,
                "display_name": "Cycle ID",
                "short_desc": "The cycle ID",
                "desc": "The ID of the cycle the issue is assigned to",
                "example_value": "a1b2c3d4-e5f6-7890-1234-567890abcdef",
            },
            "projectId": {
                "type": StringOrNothingType,
                "display_name": "Project ID",
                "short_desc": "The project ID",
                "desc": "The ID of the project the issue belongs to",
                "example_value": "9e8d7c6b-5a4f-3e2d-1c0b-9a8f7e6d5c4b",
            },
            "stateId": {
                "type": StringOrNothingType,
                "display_name": "State ID",
                "short_desc": "The state ID",
                "desc": "The ID of the current workflow state",
                "example_value": "e3d2c1b0-a987-6f5e-4d3c-2b1a09876543",
            },
            "assigneeId": {
                "type": StringOrNothingType,
                "display_name": "Assignee ID",
                "short_desc": "The assignee ID",
                "desc": "The ID of the user assigned to the issue",
                "example_value": "f1e2d3c4-b5a6-9870-1234-567890fedcba",
            },
            "creatorId": {
                "type": StringOrNothingType,
                "display_name": "Creator ID",
                "short_desc": "The creator ID",
                "desc": "The ID of the user who created the issue",
                "example_value": "b5ea5f1f-8adc-4f52-b4bd-ab4e84cf51ba",
            },
            "labelIds": {
                "type": SoftListOrNothingType,
                "display_name": "Label IDs",
                "short_desc": "The label IDs",
                "desc": "Array of IDs of labels attached to the issue",
            },
            "descriptionState": {
                "type": StringOrNothingType,
                "display_name": "Description State",
                "short_desc": "The description storage state",
                "desc": "The state of the description storage (e.g., 'stored')",
                "example_value": "stored",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the issue was created",
                "desc": "The date and time when the issue was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the issue was last updated",
                "desc": "The date and time when the issue was last updated",
            },
            "archivedAt": {
                "type": DateOrNothingType,
                "display_name": "Archived At",
                "short_desc": "When the issue was archived",
                "desc": "The date and time when the issue was archived (if archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Linear issue create input type
public class LinearIssueCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating issues
        const Fields = {
            "teamId": {
                "type": StringType,
                "display_name": "Team ID",
                "short_desc": "The team ID (required)",
                "desc": "The ID of the team to create the issue in",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
                "required": True,
            },
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The issue title (required)",
                "desc": "The title of the issue",
                "example_value": "Implement new feature",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The description of the issue in markdown format",
                "example_value": "This feature should allow users to...",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The priority: 0 = No priority, 1 = Urgent, 2 = High, 3 = Normal, 4 = Low",
                "example_value": 3,
            },
            "estimate": {
                "type": FloatOrNothingType,
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The estimated complexity/effort",
                "example_value": 2.0,
            },
            "assigneeId": {
                "type": StringOrNothingType,
                "display_name": "Assignee ID",
                "short_desc": "The assignee user ID",
                "desc": "The ID of the user to assign the issue to",
                "example_value": "user-uuid-here",
            },
            "stateId": {
                "type": StringOrNothingType,
                "display_name": "State ID",
                "short_desc": "The workflow state ID",
                "desc": "The ID of the workflow state to set",
                "example_value": "state-uuid-here",
            },
            "projectId": {
                "type": StringOrNothingType,
                "display_name": "Project ID",
                "short_desc": "The project ID",
                "desc": "The ID of the project to add the issue to",
                "example_value": "project-uuid-here",
            },
            "cycleId": {
                "type": StringOrNothingType,
                "display_name": "Cycle ID",
                "short_desc": "The cycle ID",
                "desc": "The ID of the cycle to add the issue to",
                "example_value": "cycle-uuid-here",
            },
            "labelIds": {
                "type": SoftListOrNothingType,
                "display_name": "Label IDs",
                "short_desc": "The label IDs",
                "desc": "Array of label IDs to attach to the issue",
            },
            "parentId": {
                "type": StringOrNothingType,
                "display_name": "Parent ID",
                "short_desc": "The parent issue ID",
                "desc": "The ID of the parent issue (for sub-issues)",
                "example_value": "parent-issue-uuid",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Linear issue update input type (for record-based updates)
public class LinearIssueUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating issues
        const Fields = {
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The issue title",
                "desc": "The new title of the issue",
                "example_value": "Updated issue title",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The new description of the issue",
                "example_value": "Updated description content",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The new priority",
                "example_value": 1,
            },
            "estimate": {
                "type": FloatOrNothingType,
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The new estimate",
                "example_value": 5.0,
            },
            "assigneeId": {
                "type": StringOrNothingType,
                "display_name": "Assignee ID",
                "short_desc": "The assignee user ID",
                "desc": "The ID of the user to assign",
                "example_value": "user-uuid-here",
            },
            "stateId": {
                "type": StringOrNothingType,
                "display_name": "State ID",
                "short_desc": "The workflow state ID",
                "desc": "The ID of the new workflow state",
                "example_value": "state-uuid-here",
            },
            "projectId": {
                "type": StringOrNothingType,
                "display_name": "Project ID",
                "short_desc": "The project ID",
                "desc": "The ID of the project",
                "example_value": "project-uuid-here",
            },
            "cycleId": {
                "type": StringOrNothingType,
                "display_name": "Cycle ID",
                "short_desc": "The cycle ID",
                "desc": "The ID of the cycle",
                "example_value": "cycle-uuid-here",
            },
            "labelIds": {
                "type": SoftListOrNothingType,
                "display_name": "Label IDs",
                "short_desc": "The label IDs",
                "desc": "Array of label IDs to set on the issue",
            },
            "parentId": {
                "type": StringOrNothingType,
                "display_name": "Parent ID",
                "short_desc": "The parent issue ID",
                "desc": "The ID of the parent issue",
                "example_value": "parent-issue-uuid",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Linear issue update request type (for request-based API with id)
public class LinearIssueUpdateRequestDataType inherits HashDataType {
    private {
        #! Field definitions for update request
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Issue",
                "short_desc": "The issue to update",
                "desc": "The ID of the issue to update",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The new issue title",
                "desc": "The new title of the issue",
                "example_value": "Updated issue title",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The new issue description",
                "desc": "The new description of the issue (supports markdown)",
                "example_value": "Updated description content",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "The new issue priority",
                "desc": "The new priority of the issue (0=None, 1=Urgent, 2=High, 3=Medium, 4=Low)",
                "example_value": 2,
            },
            "estimate": {
                "type": FloatOrNothingType,
                "display_name": "Estimate",
                "short_desc": "The new issue estimate",
                "desc": "The new estimated complexity/effort",
                "example_value": 3.0,
            },
            "assigneeId": {
                "type": StringOrNothingType,
                "display_name": "Assignee",
                "short_desc": "The new assignee",
                "desc": "The new user to assign the issue to",
                "example_value": "user-uuid-here",
            },
            "stateId": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The new workflow state",
                "desc": "The new workflow state for the issue",
                "example_value": "state-uuid-here",
            },
            "projectId": {
                "type": StringOrNothingType,
                "display_name": "Project",
                "short_desc": "The project to move the issue to",
                "desc": "The project to move the issue to",
                "example_value": "project-uuid-here",
            },
            "cycleId": {
                "type": StringOrNothingType,
                "display_name": "Cycle",
                "short_desc": "The cycle to move the issue to",
                "desc": "The cycle (sprint) to move the issue to",
                "example_value": "cycle-uuid-here",
            },
            "labelIds": {
                "type": SoftListOrNothingType,
                "display_name": "Labels",
                "short_desc": "The labels to set",
                "desc": "Array of label IDs to set on the issue (replaces existing labels)",
            },
            "parentId": {
                "type": StringOrNothingType,
                "display_name": "Parent Issue",
                "short_desc": "The parent issue",
                "desc": "The parent issue ID (to make this a sub-issue)",
                "example_value": "parent-issue-uuid",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
