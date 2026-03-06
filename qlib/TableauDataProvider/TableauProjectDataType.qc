# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TableauDataProvider module definition

/*  TableauProjectDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau project record type
public class TableauProjectDataType inherits HashDataType {
    public {
        #! Field definitions for Tableau projects
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The project ID",
                "desc": "The unique identifier of the project",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The project name",
                "desc": "The name of the project",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The project description",
                "desc": "The description of the project",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the project was created",
                "desc": "When the project was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the project was last updated",
                "desc": "When the project was last updated",
            },
            "contentPermissions": {
                "type": StringOrNothingType,
                "display_name": "Content Permissions",
                "short_desc": "The content permissions mode",
                "desc": "The content permissions mode",
            },
            "parentProjectId": {
                "type": StringOrNothingType,
                "display_name": "Parent Project ID",
                "short_desc": "The ID of the parent project",
                "desc": "The ID of the parent project",
            },
            "topLevelProject": {
                "type": SoftBoolOrNothingType,
                "display_name": "Top Level Project",
                "short_desc": "Whether this is a top-level project",
                "desc": "Whether this is a top-level project",
            },
            "owner": {
                "type": AutoHashOrNothingType,
                "display_name": "Owner",
                "short_desc": "The owner of the project",
                "desc": "The owner of the project",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace TableauDataProvider
