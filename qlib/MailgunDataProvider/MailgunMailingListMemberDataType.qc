# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunMailingListMemberDataType.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Mailgun mailing list record type
public class MailgunMailingListDataType inherits HashDataType {
    public {
        #! Field definitions for Mailgun mailing lists
        const Fields = {
            "address": {
                "type": StringType,
                "display_name": "List Address",
                "short_desc": "The mailing list email address",
                "desc": "The email address of the mailing list",
                "example_value": "developers@lists.example.com",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The list name",
                "desc": "The display name of the mailing list",
                "example_value": "Developers",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "List description",
                "desc": "A description of the mailing list",
                "example_value": "All developers in the company",
            },
            "access_level": {
                "type": StringOrNothingType,
                "display_name": "Access Level",
                "short_desc": "Who can post to the list",
                "desc": "The access level for posting (`readonly`, `members`, `everyone`)",
                "example_value": "members",
            },
            "reply_preference": {
                "type": StringOrNothingType,
                "display_name": "Reply Preference",
                "short_desc": "Reply-to behavior",
                "desc": "How replies are handled (`list` or `sender`)",
                "example_value": "list",
            },
            "members_count": {
                "type": IntOrNothingType,
                "display_name": "Members Count",
                "short_desc": "Number of members",
                "desc": "The total number of members in the list",
                "example_value": 150,
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the list was created",
                "desc": "The date and time when the list was created",
                "example_value": "2025-01-01T09:00:00.000Z",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Mailgun mailing list member record type
public class MailgunMailingListMemberDataType inherits HashDataType {
    public {
        #! Field definitions for Mailgun mailing list members
        const Fields = {
            "address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "Member's email address",
                "desc": "The email address of the member",
                "example_value": "user@example.com",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Member's name",
                "desc": "The name of the member",
                "example_value": "John Doe",
            },
            "subscribed": {
                "type": BoolOrNothingType,
                "display_name": "Subscribed",
                "short_desc": "Subscription status",
                "desc": "Whether the member is subscribed",
                "example_value": True,
            },
            "vars": {
                "type": AutoHashOrNothingType,
                "display_name": "Variables",
                "short_desc": "Custom variables",
                "desc": "Custom JSON variables associated with the member",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace MailgunDataProvider
