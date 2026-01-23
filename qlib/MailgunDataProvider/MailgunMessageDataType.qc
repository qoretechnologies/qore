# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunMessageDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Mailgun send email request type
public class MailgunSendEmailRequestDataType inherits HashDataType {
    private {
        #! Field definitions for sending emails
        const Fields = {
            "from": {
                "type": StringType,
                "display_name": "From",
                "short_desc": "Sender email address",
                "desc": "The sender email address. Can include a name: 'Name <email@domain.com>'",
                "example_value": "Sender <sender@example.com>",
                "required": True,
            },
            "to": {
                "type": new SoftListDataType(StringType),
                "display_name": "To",
                "short_desc": "Recipient email address(es)",
                "desc": "One or more recipient email addresses. Can include names.",
                "example_value": ("recipient@example.com",),
                "required": True,
            },
            "cc": {
                "type": new SoftListDataType(StringType, True),
                "display_name": "CC",
                "short_desc": "CC recipients",
                "desc": "Carbon copy recipients",
            },
            "bcc": {
                "type": new SoftListDataType(StringType, True),
                "display_name": "BCC",
                "short_desc": "BCC recipients",
                "desc": "Blind carbon copy recipients",
            },
            "subject": {
                "type": StringType,
                "display_name": "Subject",
                "short_desc": "Email subject",
                "desc": "The email subject line",
                "example_value": "Hello from Mailgun",
                "required": True,
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text Body",
                "short_desc": "Plain text body",
                "desc": "Plain text version of the email body",
                "example_value": "This is a test email.",
                "required_groups": ("body",),
            },
            "html": {
                "type": StringOrNothingType,
                "display_name": "HTML Body",
                "short_desc": "HTML body",
                "desc": "HTML version of the email body",
                "example_value": "<html><body><h1>Hello!</h1></body></html>",
                "required_groups": ("body",),
            },
            "o:tag": {
                "type": new SoftListDataType(StringType, True),
                "display_name": "Tags",
                "short_desc": "Message tags",
                "desc": "Tags for message categorization and tracking",
            },
            "o:tracking": {
                "type": BoolOrNothingType,
                "display_name": "Enable Tracking",
                "short_desc": "Enable click/open tracking",
                "desc": "Enable tracking for this message",
            },
            "o:tracking-clicks": {
                "type": BoolOrNothingType,
                "display_name": "Track Clicks",
                "short_desc": "Track link clicks",
                "desc": "Enable click tracking for this message",
            },
            "o:tracking-opens": {
                "type": BoolOrNothingType,
                "display_name": "Track Opens",
                "short_desc": "Track email opens",
                "desc": "Enable open tracking for this message",
            },
            "h:Reply-To": {
                "type": StringOrNothingType,
                "display_name": "Reply-To",
                "short_desc": "Reply-to address",
                "desc": "Email address for replies",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Mailgun send email response type
public class MailgunSendEmailResponseDataType inherits HashDataType {
    private {
        #! Field definitions for send email response
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "Message ID",
                "short_desc": "The message ID",
                "desc": "The unique identifier of the sent message",
                "example_value": "<20250101120000.1.ABC123@mg.example.com>",
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Status message",
                "desc": "A status message from the API",
                "example_value": "Queued. Thank you.",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Mailgun email validation request type
public class MailgunValidateEmailRequestDataType inherits HashDataType {
    private {
        #! Field definitions for email validation request
        const Fields = {
            "address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "Email address to validate",
                "desc": "The email address to validate",
                "example_value": "test@example.com",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Mailgun email validation response type
public class MailgunValidateEmailResponseDataType inherits HashDataType {
    private {
        #! Field definitions for email validation response
        const Fields = {
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The validated email address",
                "desc": "The email address that was validated",
            },
            "is_disposable_address": {
                "type": BoolOrNothingType,
                "display_name": "Is Disposable",
                "short_desc": "Whether address is disposable",
                "desc": "Whether the email is from a known disposable email provider",
            },
            "is_role_address": {
                "type": BoolOrNothingType,
                "display_name": "Is Role Address",
                "short_desc": "Whether address is a role",
                "desc": "Whether the email is a role-based address (e.g., info@, support@)",
            },
            "reason": {
                "type": new SoftListDataType(StringType, True),
                "display_name": "Reason",
                "short_desc": "Validation reasons",
                "desc": "Reasons for the validation result",
            },
            "result": {
                "type": StringOrNothingType,
                "display_name": "Result",
                "short_desc": "Validation result",
                "desc": "The validation result (deliverable, undeliverable, do_not_send, catch_all, unknown)",
                "example_value": "deliverable",
            },
            "risk": {
                "type": StringOrNothingType,
                "display_name": "Risk",
                "short_desc": "Risk level",
                "desc": "The risk level of the email address (low, medium, high, unknown)",
                "example_value": "low",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace MailgunDataProvider
