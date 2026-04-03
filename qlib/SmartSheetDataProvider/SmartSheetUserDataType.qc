# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetUserDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for Smartsheet user
public class SmartSheetUserDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "User ID",
                "type": SoftIntType,
                "short_desc": "Unique user identifier",
                "desc": "The unique identifier for the user",
            },
            "email": {
                "display_name": "Email",
                "type": SoftStringType,
                "short_desc": "User email address",
                "desc": "The email address of the user",
            },
            "firstName": {
                "display_name": "First Name",
                "type": SoftStringOrNothingType,
                "short_desc": "User's first name",
                "desc": "The first name of the user",
            },
            "lastName": {
                "display_name": "Last Name",
                "type": SoftStringOrNothingType,
                "short_desc": "User's last name",
                "desc": "The last name of the user",
            },
            "admin": {
                "display_name": "Admin",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user is admin",
                "desc": "Whether the user is a system admin",
            },
            "licensedSheetCreator": {
                "display_name": "Licensed Sheet Creator",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user can create sheets",
                "desc": "Whether the user is a licensed sheet creator",
            },
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "short_desc": "User status",
                "desc": "The status of the user account (e.g., `ACTIVE`, `PENDING`, `DECLINED`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for creating a user
public class SmartSheetCreateUserRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "email": {
                "display_name": "Email",
                "type": SoftStringType,
                "short_desc": "Email address",
                "desc": "The email address for the new user",
                "required": True,
            },
            "admin": {
                "display_name": "Admin",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user is admin",
                "desc": "Whether the user should be a system admin",
            },
            "licensedSheetCreator": {
                "display_name": "Licensed Sheet Creator",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user can create sheets",
                "desc": "Whether the user should be a licensed sheet creator",
            },
            "firstName": {
                "display_name": "First Name",
                "type": SoftStringOrNothingType,
                "short_desc": "User's first name",
                "desc": "The first name of the new user",
            },
            "lastName": {
                "display_name": "Last Name",
                "type": SoftStringOrNothingType,
                "short_desc": "User's last name",
                "desc": "The last name of the new user",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for updating a user
public class SmartSheetUpdateUserRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "userId": {
                "display_name": "User ID",
                "type": SoftIntType,
                "short_desc": "ID of the user to update",
                "desc": "The unique identifier of the user to update",
                "required": True,
            },
            "admin": {
                "display_name": "Admin",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user is admin",
                "desc": "Whether the user should be a system admin",
            },
            "licensedSheetCreator": {
                "display_name": "Licensed Sheet Creator",
                "type": SoftBoolOrNothingType,
                "short_desc": "Whether user can create sheets",
                "desc": "Whether the user should be a licensed sheet creator",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for deleting a user
public class SmartSheetDeleteUserRequestDataType inherits DataProvider::HashDataType {
    public {
        const Fields = {
            "userId": {
                "display_name": "User ID",
                "type": SoftIntType,
                "short_desc": "ID of the user to delete",
                "desc": "The unique identifier of the user to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SmartSheetDataProvider
