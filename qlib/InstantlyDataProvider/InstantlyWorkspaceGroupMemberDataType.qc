# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyWorkspaceGroupMemberDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly workspace group member data type
public class InstantlyWorkspaceGroupMemberDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Group Member ID",
                "short_desc": "Workspace group member UUID",
                "type": StringType,
                "desc": "The unique identifier (UUID) of the workspace group member",
                "required": True,
            },
            "user_id": {
                "display_name": "User ID",
                "short_desc": "User UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the user",
            },
            "group_id": {
                "display_name": "Group ID",
                "short_desc": "Group UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the group",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Member role in group",
                "type": StringOrNothingType,
                "desc": "The role of the member in the workspace group",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
