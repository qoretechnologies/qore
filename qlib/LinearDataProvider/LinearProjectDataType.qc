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
    public {
        #! Field definitions for Linear projects
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the project",
                "desc": "The unique identifier of the project",
                "type": StringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The project name",
                "desc": "The name of the project",
                "type": StringType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The project description",
                "desc": "The description of the project",
                "type": StringOrNothingType,
            },
            "slugId": {
                "display_name": "Slug ID",
                "short_desc": "The project slug ID",
                "desc": "The unique slug identifier for the project URL",
                "type": StringOrNothingType,
            },
            "state": {
                "display_name": "State",
                "short_desc": "The project state",
                "desc": "The current state of the project (planned, started, paused, completed, canceled)",
                "type": StringOrNothingType,
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The project URL",
                "desc": "The URL to the project in Linear",
                "type": StringOrNothingType,
            },
            "startDate": {
                "display_name": "Start Date",
                "short_desc": "The project start date",
                "desc": "The planned start date of the project",
                "type": DateOrNothingType,
            },
            "targetDate": {
                "display_name": "Target Date",
                "short_desc": "The project target date",
                "desc": "The target completion date of the project",
                "type": DateOrNothingType,
            },
            "lead": {
                "display_name": "Lead",
                "short_desc": "The project lead",
                "desc": "The user leading the project",
                "type": AutoHashOrNothingType,
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "When the project was created",
                "desc": "The date and time when the project was created",
                "type": DateOrNothingType,
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "When the project was last updated",
                "desc": "The date and time when the project was last updated",
                "type": DateOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        map addField(new QoreDataField({"name": $1.key} + $1.value)), Fields.pairIterator();
    }
}

} # namespace LinearDataProvider
