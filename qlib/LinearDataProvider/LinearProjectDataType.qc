# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearProjectDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Linear project record type
public class LinearProjectDataType inherits HashDataType {
    private {
        #! Field definitions for Linear projects
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the project",
                "desc": "The unique identifier of the project",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The project name",
                "desc": "The name of the project",
                "example_value": "Q1 Product Launch",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The project description",
                "desc": "The description of the project",
                "example_value": "Launch new product features for Q1 release",
            },
            "slugId": {
                "type": StringOrNothingType,
                "display_name": "Slug ID",
                "short_desc": "The project slug ID",
                "desc": "The unique slug identifier for the project URL",
                "example_value": "q1-product-launch",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The project state",
                "desc": "The current state of the project (planned, started, paused, completed, canceled)",
                "example_value": "started",
                "allowed_values": ("planned", "backlog", "started", "paused", "completed", "canceled"),
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The project URL",
                "desc": "The URL to the project in Linear",
                "example_value": "https://linear.app/myteam/project/q1-product-launch",
            },
            "startDate": {
                "type": DateOrNothingType,
                "display_name": "Start Date",
                "short_desc": "The project start date",
                "desc": "The planned start date of the project",
            },
            "targetDate": {
                "type": DateOrNothingType,
                "display_name": "Target Date",
                "short_desc": "The project target date",
                "desc": "The target completion date of the project",
            },
            "lead": {
                "type": AutoHashOrNothingType,
                "display_name": "Lead",
                "short_desc": "The project lead",
                "desc": "The user leading the project",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the project was created",
                "desc": "The date and time when the project was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the project was last updated",
                "desc": "The date and time when the project was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
