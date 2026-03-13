# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyWorkspaceMemberDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly workspace member data type
public class InstantlyWorkspaceMemberDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Member ID",
                "short_desc": "Workspace member UUID",
                "type": StringType,
                "desc": "The unique identifier (UUID) of the workspace member",
                "required": True,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Member email address",
                "type": StringOrNothingType,
                "desc": "The email address of the workspace member",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Member role",
                "type": StringOrNothingType,
                "desc": "The role of the workspace member",
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Member first name",
                "type": StringOrNothingType,
                "desc": "The first name of the workspace member",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Member last name",
                "type": StringOrNothingType,
                "desc": "The last name of the workspace member",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Member status",
                "type": StringOrNothingType,
                "desc": "The status of the workspace member",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
