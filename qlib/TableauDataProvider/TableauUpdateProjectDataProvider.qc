# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauUpdateProjectDataProvider.qc Tableau update project action data provider

/*  TableauUpdateProjectDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a project
public class TableauUpdateProjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "project_id": {
                "type": StringType,
                "display_name": "Project ID",
                "short_desc": "The project ID to update",
                "desc": "The unique identifier of the project to update",
                "required": True,
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The new project name",
                "desc": "The new name of the project",
                "example_value": "Marketing Analytics",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The new project description",
                "desc": "The new description of the project",
                "example_value": "Updated dashboards for the marketing team",
            },
            "parent_project_id": {
                "type": StringOrNothingType,
                "display_name": "Parent Project ID",
                "short_desc": "The new parent project ID",
                "desc": "The new parent project ID for moving the project",
                "example_value": "f1e2d3c4-b5a6-7890-abcd-ef1234567890",
            },
            "content_permissions": {
                "type": StringOrNothingType,
                "display_name": "Content Permissions",
                "short_desc": "The content permissions mode",
                "desc": "The content permissions mode for the project",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ManagedByOwner", "display_name": "Managed By Owner"},
                    <AllowedValueInfo>{"value": "LockedToProject", "display_name": "Locked To Project"},
                    <AllowedValueInfo>{"value": "LockedToProjectWithoutNested", "display_name": "Locked To Project Without Nested"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau update project action data provider
public class TableauUpdateProjectDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing project in Tableau",
            "type": "TableauUpdateProjectDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! Response type for action registration
        static TableauProjectDataType ResponseType();
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing project in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauUpdateProjectRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string project_id = remove req.project_id;
        hash<auto> body;
        if (req.name) body.name = req.name;
        if (req.description) body.description = req.description;
        if (req.parent_project_id) body.parentProjectId = req.parent_project_id;
        if (req.content_permissions) body.contentPermissions = req.content_permissions;
        return doPut("projects/" + project_id, {"project": body}).project;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
