# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyWorkspaceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Pending workspace invite data type
public class TallyWorkspaceInviteDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Invite ID",
                "short_desc": "Invite identifier",
                "desc": "The unique identifier of the pending invite",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Invited email address",
                "desc": "The email address that was invited",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "workspaceIds": {
                "display_name": "Workspace IDs",
                "short_desc": "Workspace IDs the invite applies to",
                "desc": "List of workspace IDs this invite grants access to",
                "type": new ListDataType(AbstractDataProviderTypeMap."string", True),
            },
        };
    }

    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Workspace response data type
public class TallyWorkspaceDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Workspace ID",
                "short_desc": "Unique workspace identifier",
                "desc": "The unique identifier for the workspace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Workspace name",
                "desc": "The display name of the workspace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "members": {
                "display_name": "Members",
                "short_desc": "Workspace members",
                "desc": "List of users that are members of this workspace",
                "type": new ListDataType("TallyWorkspaceMemberList", new TallyUserDataType(), True),
            },
            "invites": {
                "display_name": "Pending Invites",
                "short_desc": "Pending workspace invitations",
                "desc": "List of pending invitations for this workspace",
                "type": new ListDataType("TallyWorkspaceInviteList",
                    new TallyWorkspaceInviteDataType(), True),
            },
            "createdByUserId": {
                "display_name": "Created By User ID",
                "short_desc": "ID of the creating user",
                "desc": "The ID of the user that created the workspace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "Timestamp when the workspace was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "Timestamp when the workspace was last updated",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyWorkspaceDataType = new TallyWorkspaceDataType();
}
