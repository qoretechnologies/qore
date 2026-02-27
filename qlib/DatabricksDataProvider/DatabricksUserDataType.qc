# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider module definition

/*  DatabricksUserDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Data type for Databricks user records (SCIM)
public class DatabricksUserDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "User ID",
                "short_desc": "Unique identifier for the user",
                "desc": "The unique identifier of the Databricks user",
                "required": True,
            },
            "userName": {
                "type": StringOrNothingType,
                "display_name": "User Name",
                "short_desc": "Login name of the user",
                "desc": "The login user name, typically an email address",
            },
            "displayName": {
                "type": StringOrNothingType,
                "display_name": "Display Name",
                "short_desc": "Display name of the user",
                "desc": "The human-readable display name of the user",
            },
            "emails": {
                "type": AutoListOrNothingType,
                "display_name": "Emails",
                "short_desc": "User email addresses",
                "desc": "A list of email addresses associated with the user",
            },
            "active": {
                "type": BoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Whether the user is active",
                "desc": "Indicates whether the user account is active",
            },
            "groups": {
                "type": AutoListOrNothingType,
                "display_name": "Groups",
                "short_desc": "Groups the user belongs to",
                "desc": "A list of groups that the user is a member of",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace DatabricksDataProvider
