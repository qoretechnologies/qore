# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TableauDataProvider module definition

/*  TableauUserDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau user record type
public class TableauUserDataType inherits HashDataType {
    public {
        #! Field definitions for Tableau users
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The username",
                "desc": "The username",
            },
            "fullName": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "The full name of the user",
                "desc": "The full name of the user",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The email address",
                "desc": "The email address of the user",
            },
            "siteRole": {
                "type": StringOrNothingType,
                "display_name": "Site Role",
                "short_desc": "The site role of the user",
                "desc": "The site role of the user",
            },
            "lastLogin": {
                "type": DateOrNothingType,
                "display_name": "Last Login",
                "short_desc": "When the user last logged in",
                "desc": "When the user last logged in",
            },
            "externalAuthUserId": {
                "type": StringOrNothingType,
                "display_name": "External Auth User ID",
                "short_desc": "External authentication user ID",
                "desc": "External authentication user ID",
            },
            "authSetting": {
                "type": StringOrNothingType,
                "display_name": "Auth Setting",
                "short_desc": "The authentication setting",
                "desc": "The authentication setting for the user",
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "The locale of the user",
                "desc": "The locale of the user",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "The language of the user",
                "desc": "The language of the user",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace TableauDataProvider
