# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetGroupDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Smartsheet group
public class SmartSheetGroupDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Group ID",
                "type": SoftIntType,
                "short_desc": "Unique group identifier",
                "desc": "The unique identifier for the group",
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringType,
                "short_desc": "Group name",
                "desc": "The name of the group",
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "short_desc": "Group description",
                "desc": "A description of the group",
            },
            "owner": {
                "display_name": "Owner",
                "type": SoftStringOrNothingType,
                "short_desc": "Group owner email",
                "desc": "The email address of the group owner",
            },
            "ownerId": {
                "display_name": "Owner ID",
                "type": SoftIntOrNothingType,
                "short_desc": "Group owner user ID",
                "desc": "The unique identifier of the group owner",
            },
            "createdAt": {
                "display_name": "Created At",
                "type": SoftDateOrNothingType,
                "short_desc": "Creation date",
                "desc": "The date and time the group was created",
            },
            "modifiedAt": {
                "display_name": "Modified At",
                "type": SoftDateOrNothingType,
                "short_desc": "Last modified date",
                "desc": "The date and time the group was last modified",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for creating a group
public class SmartSheetCreateGroupRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Group Name",
                "type": SoftStringType,
                "short_desc": "Name for the new group",
                "desc": "The name for the new group",
                "required": True,
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "short_desc": "Group description",
                "desc": "A description of the new group",
            },
            "members": {
                "display_name": "Members",
                "type": SoftAutoListOrNothingType,
                "short_desc": "Initial group members",
                "desc": "A list of member hashes. Each member hash should contain an "
                    "`email` field with the member's email address",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for updating a group
public class SmartSheetUpdateGroupRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "groupId": {
                "display_name": "Group ID",
                "type": SoftIntType,
                "short_desc": "ID of the group to update",
                "desc": "The unique identifier of the group to update",
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "type": SoftStringOrNothingType,
                "short_desc": "New group name",
                "desc": "The new name for the group",
            },
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "short_desc": "New group description",
                "desc": "The new description for the group",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for deleting a group
public class SmartSheetDeleteGroupRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "groupId": {
                "display_name": "Group ID",
                "type": SoftIntType,
                "short_desc": "ID of the group to delete",
                "desc": "The unique identifier of the group to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
