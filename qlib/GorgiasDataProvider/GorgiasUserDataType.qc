# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasUserDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias user record type
public class GorgiasUserDataType inherits HashDataType {
    public {
        #! Field definitions for Gorgias users (agents)
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "User ID",
                "short_desc": "The user identifier",
                "desc": "The unique identifier of the user (agent)",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "User email address",
                "desc": "The email address of the user",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Full name",
                "desc": "The full name of the user",
            },
            "firstname": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "User first name",
                "desc": "The first name of the user",
            },
            "lastname": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "User last name",
                "desc": "The last name of the user",
            },
            "role": {
                "type": new GorgiasRoleDataType(AutoHashOrNothingType),
                "display_name": "Role",
                "short_desc": "User role",
                "desc": "The role assigned to the user in the helpdesk",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Preferred language",
                "desc": "The preferred language of the user",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "User timezone",
                "desc": "The timezone of the user",
            },
            "active": {
                "type": SoftBoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Whether active",
                "desc": "Whether the user account is active",
            },
            "created_datetime": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The ISO 8601 timestamp when the user was created",
            },
            "updated_datetime": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The ISO 8601 timestamp when the user was last updated",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace GorgiasDataProvider
