# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoMembershipDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io membership record type
public class SystemeIoMembershipDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The membership ID",
                "desc": "The unique identifier of the membership",
            },
            "communityId": {
                "type": IntOrNothingType,
                "display_name": "Community ID",
                "short_desc": "The community ID",
                "desc": "The unique identifier of the community",
            },
            "contactId": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the member contact",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the membership was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io create membership request data type
public class SystemeIoCreateMembershipRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "communityId": {
                "type": IntType,
                "display_name": "Community ID",
                "short_desc": "The community to join",
                "desc": "The unique identifier of the community to add the contact to",
                "required": True,
                "ref_data": "communities",
                "example_value": 200,
            },
            "contactId": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact to add",
                "desc": "The unique identifier of the contact to add as a member",
                "required": True,
                "ref_data": "contacts",
                "example_value": 12345,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io delete membership request data type
public class SystemeIoDeleteMembershipRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Membership ID",
                "short_desc": "The membership ID to delete",
                "desc": "The unique identifier of the membership to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
