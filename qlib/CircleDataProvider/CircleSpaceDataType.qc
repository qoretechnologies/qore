# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleSpaceDataType.qc defines data types for Circle spaces and space groups

/*  CircleSpaceDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle add member to space request data type
public class CircleAddMemberToSpaceRequestDataType inherits HashDataType {
    public {
        #! Field definitions for adding a member to a space
        const Fields = {
            "space_id": {
                "type": IntType,
                "display_name": "Space ID",
                "short_desc": "Space to add the member to",
                "desc": "The ID of the space to add the member to",
                "required": True,
                "ref_data": "spaces",
                "example_value": 12345,
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Member email address",
                "desc": "The email address of the member to add to the space",
                "required": True,
                "example_value": "user@example.com",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle remove member from space request data type
public class CircleRemoveMemberFromSpaceRequestDataType inherits HashDataType {
    public {
        #! Field definitions for removing a member from a space
        const Fields = {
            "space_id": {
                "type": IntType,
                "display_name": "Space ID",
                "short_desc": "Space to remove the member from",
                "desc": "The ID of the space to remove the member from",
                "required": True,
                "ref_data": "spaces",
            },
            "member_id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "Member to remove",
                "desc": "The ID of the community member to remove from the space",
                "required": True,
                "ref_data": "members",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle add member to space group request data type
public class CircleAddMemberToSpaceGroupRequestDataType inherits HashDataType {
    public {
        #! Field definitions for adding a member to a space group
        const Fields = {
            "space_group_id": {
                "type": IntType,
                "display_name": "Space Group ID",
                "short_desc": "Space group to add the member to",
                "desc": "The ID of the space group to add the member to",
                "required": True,
                "ref_data": "space_groups",
                "example_value": 12345,
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Member email address",
                "desc": "The email address of the member to add to the space group",
                "required": True,
                "example_value": "user@example.com",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle remove member from space group request data type
public class CircleRemoveMemberFromSpaceGroupRequestDataType inherits HashDataType {
    public {
        #! Field definitions for removing a member from a space group
        const Fields = {
            "space_group_id": {
                "type": IntType,
                "display_name": "Space Group ID",
                "short_desc": "Space group to remove the member from",
                "desc": "The ID of the space group to remove the member from",
                "required": True,
                "ref_data": "space_groups",
            },
            "member_id": {
                "type": IntType,
                "display_name": "Member ID",
                "short_desc": "Member to remove",
                "desc": "The ID of the community member to remove from the space group",
                "required": True,
                "ref_data": "members",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CircleDataProvider
