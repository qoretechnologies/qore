# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyEmailDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyEmailDataType inherits HashDataType {
    public {
        const Fields = {
            "EMAIL_ID": {
                "type": SoftIntOrNothingType, "display_name": "Email ID",
                "short_desc": "The unique email identifier",
                "desc": "The unique identifier of the email",
            },
            "GMAIL_MESSAGE_ID": {
                "type": SoftStringOrNothingType, "display_name": "Gmail Message ID",
                "short_desc": "Gmail message identifier",
                "desc": "The Gmail message ID if the email was synced from Gmail",
            },
            "SUBJECT": {
                "type": SoftStringOrNothingType, "display_name": "Subject",
                "short_desc": "Email subject", "desc": "The subject line of the email",
            },
            "BODY": {
                "type": SoftStringOrNothingType, "display_name": "Body",
                "short_desc": "Email body content", "desc": "The body content of the email",
            },
            "FROM": {
                "type": SoftStringOrNothingType, "display_name": "From",
                "short_desc": "Sender address",
                "desc": "The email address of the sender",
            },
            "TO": {
                "type": SoftStringOrNothingType, "display_name": "To",
                "short_desc": "Recipient addresses",
                "desc": "The email addresses of the recipients",
            },
            "CC": {
                "type": SoftStringOrNothingType, "display_name": "CC",
                "short_desc": "CC addresses",
                "desc": "The email addresses in the CC field",
            },
            "BCC": {
                "type": SoftStringOrNothingType, "display_name": "BCC",
                "short_desc": "BCC addresses",
                "desc": "The email addresses in the BCC field",
            },
            "FORMAT": {
                "type": SoftStringOrNothingType, "display_name": "Format",
                "short_desc": "Email format",
                "desc": "The format of the email body",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "TEXT", "display_name": "Plain Text",
                        "desc": "Plain text format"},
                    <AllowedValueInfo>{"value": "HTML", "display_name": "HTML",
                        "desc": "HTML format"},
                ),
            },
            "SIZE": {
                "type": SoftIntOrNothingType, "display_name": "Size",
                "short_desc": "Email size in bytes",
                "desc": "The size of the email in bytes",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this email",
            },
            "EMAILLINKS": {
                "type": new ListDataType("InsightlyEmailRespLinkList",
                    new InsightlyEmailLinkDataType(), True),
                "display_name": "Email Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the email was created",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateEmailRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "SUBJECT": {
                "type": SoftStringOrNothingType, "display_name": "Subject",
                "short_desc": "Email subject", "desc": "The subject line of the email",
                "preselected": True, "example_value": "Follow-up on our meeting",
            },
            "BODY": {
                "type": SoftStringOrNothingType, "display_name": "Body",
                "short_desc": "Email body content", "desc": "The body content of the email",
                "preselected": True,
            },
            "FROM": {
                "type": SoftStringOrNothingType, "display_name": "From",
                "short_desc": "Sender address",
                "desc": "The email address of the sender",
                "preselected": True, "example_value": "user@example.com",
            },
            "TO": {
                "type": SoftStringOrNothingType, "display_name": "To",
                "short_desc": "Recipient addresses",
                "desc": "The email addresses of the recipients",
                "preselected": True, "example_value": "client@example.com",
            },
            "CC": {
                "type": SoftStringOrNothingType, "display_name": "CC",
                "short_desc": "CC addresses",
                "desc": "The email addresses in the CC field",
            },
            "BCC": {
                "type": SoftStringOrNothingType, "display_name": "BCC",
                "short_desc": "BCC addresses",
                "desc": "The email addresses in the BCC field",
            },
            "FORMAT": {
                "type": SoftStringOrNothingType, "display_name": "Format",
                "short_desc": "Email format",
                "desc": "The format of the email body",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "TEXT", "display_name": "Plain Text",
                        "desc": "Plain text format"},
                    <AllowedValueInfo>{"value": "HTML", "display_name": "HTML",
                        "desc": "HTML format"},
                ),
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType, "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this email",
                "ref_data": "users",
            },
            "EMAILLINKS": {
                "type": new ListDataType("InsightlyEmailLinkList",
                    new InsightlyEmailLinkDataType(), True),
                "display_name": "Email Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteEmailRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "EMAIL_ID": {
                "type": SoftIntType, "display_name": "Email ID",
                "required": True, "short_desc": "The email ID to delete",
                "desc": "The unique identifier of the email to delete",
                "ref_data": "emails",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
