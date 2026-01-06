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
    public {
        #! Field definitions for Linear issues
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the issue",
                "desc": "The unique identifier of the issue",
                "type": StringType,
            },
            "identifier": {
                "display_name": "Identifier",
                "short_desc": "The issue's human-readable identifier (e.g., ENG-123)",
                "desc": "The issue's human-readable identifier combining team key and issue number",
                "type": StringType,
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The issue title",
                "desc": "The title of the issue",
                "type": StringType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The issue description in markdown format",
                "type": StringOrNothingType,
            },
            "priority": {
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The priority of the issue: 0 = No priority, 1 = Urgent, 2 = High, 3 = Normal, 4 = Low",
                "type": IntOrNothingType,
            },
            "estimate": {
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The estimated complexity/effort for the issue",
                "type": FloatOrNothingType,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The issue URL",
                "desc": "The URL to the issue in Linear",
                "type": StringOrNothingType,
            },
            "state": {
                "display_name": "State",
                "short_desc": "The issue state/status",
                "desc": "The current workflow state of the issue",
                "type": AutoHashOrNothingType,
            },
            "assignee": {
                "display_name": "Assignee",
                "short_desc": "The assigned user",
                "desc": "The user assigned to the issue",
                "type": AutoHashOrNothingType,
            },
            "team": {
                "display_name": "Team",
                "short_desc": "The team",
                "desc": "The team the issue belongs to",
                "type": AutoHashOrNothingType,
            },
            "project": {
                "display_name": "Project",
                "short_desc": "The project",
                "desc": "The project the issue belongs to",
                "type": AutoHashOrNothingType,
            },
            "cycle": {
                "display_name": "Cycle",
                "short_desc": "The cycle/sprint",
                "desc": "The cycle (sprint) the issue is assigned to",
                "type": AutoHashOrNothingType,
            },
            "labels": {
                "display_name": "Labels",
                "short_desc": "The issue labels",
                "desc": "Labels attached to the issue",
                "type": AutoHashOrNothingType,
            },
            "parent": {
                "display_name": "Parent Issue",
                "short_desc": "The parent issue",
                "desc": "The parent issue if this is a sub-issue",
                "type": AutoHashOrNothingType,
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "When the issue was created",
                "desc": "The date and time when the issue was created",
                "type": DateOrNothingType,
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "When the issue was last updated",
                "desc": "The date and time when the issue was last updated",
                "type": DateOrNothingType,
            },
            "archivedAt": {
                "display_name": "Archived At",
                "short_desc": "When the issue was archived",
                "desc": "The date and time when the issue was archived (if archived)",
                "type": DateOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

#! Linear issue create input type
public class LinearIssueCreateDataType inherits HashDataType {
    public {
        #! Field definitions for creating issues
        const Fields = {
            "teamId": {
                "display_name": "Team ID",
                "short_desc": "The team ID (required)",
                "desc": "The ID of the team to create the issue in",
                "type": StringType,
                "required": True,
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The issue title (required)",
                "desc": "The title of the issue",
                "type": StringType,
                "required": True,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The description of the issue in markdown format",
                "type": StringOrNothingType,
            },
            "priority": {
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The priority: 0 = No priority, 1 = Urgent, 2 = High, 3 = Normal, 4 = Low",
                "type": IntOrNothingType,
            },
            "estimate": {
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The estimated complexity/effort",
                "type": FloatOrNothingType,
            },
            "assigneeId": {
                "display_name": "Assignee ID",
                "short_desc": "The assignee user ID",
                "desc": "The ID of the user to assign the issue to",
                "type": StringOrNothingType,
            },
            "stateId": {
                "display_name": "State ID",
                "short_desc": "The workflow state ID",
                "desc": "The ID of the workflow state to set",
                "type": StringOrNothingType,
            },
            "projectId": {
                "display_name": "Project ID",
                "short_desc": "The project ID",
                "desc": "The ID of the project to add the issue to",
                "type": StringOrNothingType,
            },
            "cycleId": {
                "display_name": "Cycle ID",
                "short_desc": "The cycle ID",
                "desc": "The ID of the cycle to add the issue to",
                "type": StringOrNothingType,
            },
            "labelIds": {
                "display_name": "Label IDs",
                "short_desc": "The label IDs",
                "desc": "Array of label IDs to attach to the issue",
                "type": new Type("softlist<string>"),
            },
            "parentId": {
                "display_name": "Parent ID",
                "short_desc": "The parent issue ID",
                "desc": "The ID of the parent issue (for sub-issues)",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

#! Linear issue update input type
public class LinearIssueUpdateDataType inherits HashDataType {
    public {
        #! Field definitions for updating issues
        const Fields = {
            "title": {
                "display_name": "Title",
                "short_desc": "The issue title",
                "desc": "The new title of the issue",
                "type": StringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The issue description",
                "desc": "The new description of the issue",
                "type": StringOrNothingType,
            },
            "priority": {
                "display_name": "Priority",
                "short_desc": "The issue priority (0-4)",
                "desc": "The new priority",
                "type": IntOrNothingType,
            },
            "estimate": {
                "display_name": "Estimate",
                "short_desc": "The issue estimate",
                "desc": "The new estimate",
                "type": FloatOrNothingType,
            },
            "assigneeId": {
                "display_name": "Assignee ID",
                "short_desc": "The assignee user ID",
                "desc": "The ID of the user to assign",
                "type": StringOrNothingType,
            },
            "stateId": {
                "display_name": "State ID",
                "short_desc": "The workflow state ID",
                "desc": "The ID of the new workflow state",
                "type": StringOrNothingType,
            },
            "projectId": {
                "display_name": "Project ID",
                "short_desc": "The project ID",
                "desc": "The ID of the project",
                "type": StringOrNothingType,
            },
            "cycleId": {
                "display_name": "Cycle ID",
                "short_desc": "The cycle ID",
                "desc": "The ID of the cycle",
                "type": StringOrNothingType,
            },
            "labelIds": {
                "display_name": "Label IDs",
                "short_desc": "The label IDs",
                "desc": "Array of label IDs to set on the issue",
                "type": new Type("softlist<string>"),
            },
            "parentId": {
                "display_name": "Parent ID",
                "short_desc": "The parent issue ID",
                "desc": "The ID of the parent issue",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

} # namespace LinearDataProvider
