# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWorkspaceDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Data type for Smartsheet workspace
public class SmartSheetWorkspaceDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Workspace ID",
                "type": SoftIntType,
                "short_desc": "Unique workspace identifier",
                "desc": "The unique identifier for the workspace",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringType,
                "short_desc": "Workspace name",
                "desc": "The name of the workspace",
            },
            "accessLevel": {
                "display_name": "Access Level",
                "type": SoftStringOrNothingType,
                "short_desc": "User's access level",
                "desc": "The access level the current user has on the workspace",
            },
            "permalink": {
                "display_name": "Permalink",
                "type": SoftStringOrNothingType,
                "short_desc": "Permanent URL",
                "desc": "URL that represents a direct link to the workspace in Smartsheet",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for creating a workspace
public class SmartSheetCreateWorkspaceRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Workspace Name",
                "type": SoftStringType,
                "short_desc": "Name for the new workspace",
                "desc": "The name for the new workspace",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for updating a workspace
public class SmartSheetUpdateWorkspaceRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "workspaceId": {
                "display_name": "Workspace ID",
                "type": SoftIntType,
                "short_desc": "ID of the workspace to update",
                "desc": "The unique identifier of the workspace to update",
                "required": True,
                "ref_data": "workspaces",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringOrNothingType,
                "short_desc": "New workspace name",
                "desc": "The new name for the workspace",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for deleting a workspace
public class SmartSheetDeleteWorkspaceRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "workspaceId": {
                "display_name": "Workspace ID",
                "type": SoftIntType,
                "short_desc": "ID of the workspace to delete",
                "desc": "The unique identifier of the workspace to delete",
                "required": True,
                "ref_data": "workspaces",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
