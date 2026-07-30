# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipBuildDataType.qc Qlip build record data type

/*  QlipBuildDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! Qlip build record type
public class QlipBuildDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftStringType,
                "display_name": "ID",
                "short_desc": "The build ID",
                "desc": "The unique identifier of the build",
            },
            "projectName": {
                "type": StringOrNothingType,
                "display_name": "Project",
                "short_desc": "The project the build belongs to",
                "desc": "The name of the project the build belongs to; populated on list "
                    "responses, notably when a list spans multiple projects",
                "example_value": "reqore",
            },
            "buildNumber": {
                "type": SoftIntOrNothingType,
                "display_name": "Build Number",
                "short_desc": "The per-project sequential build number",
                "desc": "The per-project sequential build number (the \"#159\" label); "
                    "null only for legacy rows",
                "example_value": 159,
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The build status",
                "desc": "The current status of the build: `pending`, `approved`, `rejected`, "
                    "`mixed`, or `uploading`",
                "example_value": "pending",
            },
            "branch": {
                "type": StringOrNothingType,
                "display_name": "Branch",
                "short_desc": "The git branch",
                "desc": "The git branch the build was captured from",
                "example_value": "main",
            },
            "commit": {
                "type": StringOrNothingType,
                "display_name": "Commit",
                "short_desc": "The git commit SHA",
                "desc": "The git commit the build was captured from",
            },
            "pullRequestUrl": {
                "type": StringOrNothingType,
                "display_name": "Pull Request URL",
                "short_desc": "The originating pull-request URL",
                "desc": "The pull-request URL for the build when uploaded from a PR CI context; "
                    "omitted for local/push builds",
                "example_value": "https://github.com/owner/repo/pull/123",
            },
            "createdAt": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the build was created",
                "desc": "The timestamp when the build was created (ISO 8601 format)",
            },
            "updatedAt": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the build was last updated",
                "desc": "The timestamp when the build was last updated (ISO 8601 format)",
            },
            "statistics": {
                "type": AutoHashOrNothingType,
                "display_name": "Statistics",
                "short_desc": "Snapshot/review counts",
                "desc": "Roll-up counts for the build (total, changed, accepted, denied, "
                    "pending, etc.)",
            },
            "snapshotIds": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Snapshot IDs",
                "short_desc": "The build's snapshot IDs",
                "desc": "The list of snapshot identifiers captured in this build",
            },
            "toolName": {
                "type": StringOrNothingType,
                "display_name": "Tool Name",
                "short_desc": "The capture tool name",
                "desc": "The name of the tool that produced the build",
                "example_value": "qlip",
            },
            "toolVersion": {
                "type": StringOrNothingType,
                "display_name": "Tool Version",
                "short_desc": "The capture tool version",
                "desc": "The version of the tool that produced the build",
            },
            "agentReviewState": {
                "type": StringOrNothingType,
                "display_name": "Agent Review State",
                "short_desc": "Whether an automated reviewer has passed over the build",
                "desc": "Whether an automated reviewer has reviewed this build "
                    "(`none`, `in_progress`, or `reviewed`); separate from `status`",
                "example_value": "none",
            },
            "agentReviewSummary": {
                "type": StringOrNothingType,
                "display_name": "Agent Review Summary",
                "short_desc": "The automated reviewer's free-text summary",
                "desc": "The automated reviewer's free-text summary from its last finished pass; "
                    "present only when an agent has written one",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace QlipDataProvider
