# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapEmailDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap email record type
public class KeapEmailDataType inherits HashDataType {
    private {
        #! Field definitions for Keap emails
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The email ID",
                "desc": "The unique identifier of the email",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "The email subject",
                "desc": "The subject line of the email",
                "example_value": "Welcome to our service",
            },
            "sent_to_address": {
                "type": StringOrNothingType,
                "display_name": "Sent To",
                "short_desc": "The recipient email address",
                "desc": "The email address the email was sent to",
                "example_value": "customer@example.com",
            },
            "sent_from_address": {
                "type": StringOrNothingType,
                "display_name": "Sent From",
                "short_desc": "The sender email address",
                "desc": "The email address the email was sent from",
                "example_value": "support@company.com",
            },
            "sent_date": {
                "type": DateOrNothingType,
                "display_name": "Sent Date",
                "short_desc": "When the email was sent",
                "desc": "The timestamp when the email was sent",
            },
            "contact_id": {
                "type": IntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact ID",
                "desc": "The ID of the contact this email is associated with",
            },
            "headers": {
                "type": StringOrNothingType,
                "display_name": "Headers",
                "short_desc": "The email headers",
                "desc": "The email headers as a string",
            },
            "html_content": {
                "type": StringOrNothingType,
                "display_name": "HTML Content",
                "short_desc": "The email HTML content",
                "desc": "The HTML body content of the email",
            },
            "opened_date": {
                "type": DateOrNothingType,
                "display_name": "Opened Date",
                "short_desc": "When the email was opened",
                "desc": "The timestamp when the email was first opened",
            },
            "clicked_date": {
                "type": DateOrNothingType,
                "display_name": "Clicked Date",
                "short_desc": "When a link was clicked",
                "desc": "The timestamp when a link in the email was first clicked",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
