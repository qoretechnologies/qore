# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyUserDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! User response data type
public class TallyUserDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "User ID",
                "short_desc": "Unique user identifier",
                "desc": "The unique identifier for the authenticated user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "firstName": {
                "display_name": "First Name",
                "short_desc": "User first name",
                "desc": "The first name of the authenticated user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "lastName": {
                "display_name": "Last Name",
                "short_desc": "User last name",
                "desc": "The last name of the authenticated user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "fullName": {
                "display_name": "Full Name",
                "short_desc": "User full name",
                "desc": "The full display name of the authenticated user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "User email address",
                "desc": "The email address of the authenticated user",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "avatarUrl": {
                "display_name": "Avatar URL",
                "short_desc": "User avatar image URL",
                "desc": "The URL of the user's avatar image",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "organizationId": {
                "display_name": "Organization ID",
                "short_desc": "User organization identifier",
                "desc": "The ID of the organization the user belongs to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "isDeleted": {
                "display_name": "Is Deleted",
                "short_desc": "Whether the user is deleted",
                "desc": "`True` if the user account has been deleted",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "hasTwoFactorEnabled": {
                "display_name": "Two-Factor Enabled",
                "short_desc": "Whether 2FA is enabled",
                "desc": "`True` if the user has two-factor authentication enabled",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "createdAt": {
                "display_name": "Created At",
                "short_desc": "Account creation timestamp",
                "desc": "ISO 8601 timestamp when the user account was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "updatedAt": {
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "ISO 8601 timestamp when the user account was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "subscriptionPlan": {
                "display_name": "Subscription Plan",
                "short_desc": "User subscription plan",
                "desc": "The user's subscription plan",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FREE", "display_name": "Free"},
                    <AllowedValueInfo>{"value": "PRO", "display_name": "Pro"},
                    <AllowedValueInfo>{"value": "BUSINESS", "display_name": "Business"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const TallyUserDataType = new TallyUserDataType();
}
