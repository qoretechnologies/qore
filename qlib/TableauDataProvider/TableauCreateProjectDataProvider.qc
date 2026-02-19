# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauCreateProjectDataProvider.qc Tableau create project action data provider

/*  TableauCreateProjectDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a project
public class TableauCreateProjectRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The name of the project",
                "desc": "The name of the project to create",
                "required": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The project description",
                "desc": "A description of the project",
            },
            "parent_project_id": {
                "type": StringOrNothingType,
                "display_name": "Parent Project ID",
                "short_desc": "The parent project ID",
                "desc": "The ID of the parent project, if creating a nested project",
            },
            "content_permissions": {
                "type": StringOrNothingType,
                "display_name": "Content Permissions",
                "short_desc": "The content permissions mode",
                "desc": "ManagedByOwner, LockedToProject, or LockedToProjectWithoutNested",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Tableau create project action data provider
public class TableauCreateProjectDataProvider inherits TableauDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new project in Tableau",
            "type": "TableauCreateProjectDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new project in Tableau";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new TableauCreateProjectRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new TableauProjectDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("projects", {"project": req}).project;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
