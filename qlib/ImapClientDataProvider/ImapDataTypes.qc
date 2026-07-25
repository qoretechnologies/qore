# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ImapDataTypes.qc typed request and response data types for the IMAP data provider

/*  ImapDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ImapClientDataProvider {
#! Request type for searching a mailbox
public class ImapSearchRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox to search",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox to search; the mailbox is selected before the search runs",
                "default_value": "INBOX",
                "example_value": "INBOX",
            },
            "criteria": {
                "display_name": "Search Criteria",
                "short_desc": "The IMAP search criteria",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The IMAP search criteria as defined by "
                    "[RFC 3501 §6.4.4](https://datatracker.ietf.org/doc/html/rfc3501#section-6.4.4).\n\n"
                    "Searching happens on the server, so only matching messages are transferred. "
                    "Criteria can be combined freely, for example "
                    "`UNSEEN FROM \"boss@example.com\" SINCE 1-Jan-2026`",
                "default_value": "ALL",
                "example_value": "UNSEEN FROM \"boss@example.com\"",
            },
            "charset": {
                "display_name": "Charset",
                "short_desc": "An optional charset for the search criteria",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "An optional charset for the search criteria; needed only when the criteria "
                    "contain non-ASCII text",
                "example_value": "UTF-8",
            },
            "read_only": {
                "display_name": "Read Only?",
                "short_desc": "Open the mailbox read-only?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Open the mailbox with `EXAMINE` instead of `SELECT`, so that retrieving "
                    "messages cannot change any flags",
                "default_value": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for a mailbox search
public class ImapSearchResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "uids": {
                "display_name": "Message UIDs",
                "short_desc": "The UIDs of the matching messages",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."int", True),
                "desc": "The UIDs of the messages matching the search criteria.\n\n"
                    "UIDs are stable for the lifetime of the mailbox's `uidvalidity` value, so they can "
                    "be stored and used later, unlike message sequence numbers",
            },
            "count": {
                "display_name": "Match Count",
                "short_desc": "The number of matching messages",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The number of messages matching the search criteria",
            },
            "uidvalidity": {
                "display_name": "UID Validity",
                "short_desc": "The mailbox's UID validity value",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "The mailbox's `uidvalidity` value.\n\n"
                    "Store this alongside any UIDs that are retained; if it changes, the server has "
                    "reassigned UIDs and previously-stored UIDs no longer refer to the same messages",
            },
            "message_count": {
                "display_name": "Mailbox Message Count",
                "short_desc": "The total number of messages in the mailbox",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The total number of messages in the mailbox, matching or not",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for retrieving a message
public class ImapGetMessageRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox holding the message",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox holding the message",
                "default_value": "INBOX",
                "example_value": "INBOX",
            },
            "uid": {
                "display_name": "Message UID",
                "short_desc": "The UID of the message to retrieve",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The UID of the message to retrieve, as returned by a search",
                "required": True,
                "example_value": 4392,
            },
            "mark_seen": {
                "display_name": "Mark as Seen?",
                "short_desc": "Set the \\Seen flag after retrieving?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Set the `\\Seen` flag on the message after retrieving it.\n\n"
                    "Retrieval itself never changes flags, so leave this `False` to inspect a message "
                    "without the sender's mail client showing it as read",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Field definitions shared by the message response and message event types
/** Declared once so the event type does not have to reach into the response type's private field
    definitions.
*/
public const ImapMessageFields = {
            "uid": {
                "display_name": "Message UID",
                "short_desc": "The message UID",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The message UID",
            },
            "subject": {
                "display_name": "Subject",
                "short_desc": "The message subject",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The message subject, decoded from any MIME header encoding",
            },
            "from": {
                "display_name": "From",
                "short_desc": "The sender address",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The sender address as given in the `From` header",
            },
            "to": {
                "display_name": "To",
                "short_desc": "The recipient addresses",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "The recipient addresses as given in the `To` header",
            },
            "date": {
                "display_name": "Date",
                "short_desc": "The message date",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "The date the message was sent, from the `Date` header",
            },
            "message_id": {
                "display_name": "Message ID",
                "short_desc": "The Message-ID header value",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The `Message-ID` header value, which uniquely identifies the message",
            },
            "flags": {
                "display_name": "Flags",
                "short_desc": "The flags set on the message",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "The flags set on the message (ex: `\\Seen`, `\\Flagged`, `\\Answered`)",
            },
            "body": {
                "display_name": "Body",
                "short_desc": "The message body text",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The decoded text of the message body",
            },
            "attachments": {
                "display_name": "Attachments",
                "short_desc": "Information about the message's attachments",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."hash", True),
                "desc": "Information about the message's attachments; each entry has `name`, `mime` and "
                    "`size` keys.\n\n"
                    "Attachment content is not included here; retrieve it with the "
                    "`messages/attachment/get` action so that large attachments are only transferred "
                    "when they are actually needed",
            },
    "raw": {
        "display_name": "Raw Message",
        "short_desc": "The complete raw message",
        "type": AbstractDataProviderTypeMap."*data",
        "desc": "The complete message exactly as stored on the server, in RFC 822 form",
    },
};

#! Response type for a retrieved message
public class ImapMessageResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = ImapMessageFields;
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for retrieving a single MIME part
public class ImapGetAttachmentRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox holding the message",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox holding the message",
                "default_value": "INBOX",
                "example_value": "INBOX",
            },
            "uid": {
                "display_name": "Message UID",
                "short_desc": "The UID of the message holding the part",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The UID of the message holding the part",
                "required": True,
                "example_value": 4392,
            },
            "part": {
                "display_name": "MIME Part",
                "short_desc": "The MIME part specifier to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The MIME part specifier to retrieve (ex: `1`, `2.1`, `HEADER`, `TEXT`).\n\n"
                    "Retrieving a single part avoids transferring the whole message, which matters when "
                    "one attachment is needed from a message with several large ones",
                "required": True,
                "example_value": "2",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for a retrieved MIME part
public class ImapAttachmentResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "uid": {
                "display_name": "Message UID",
                "short_desc": "The message UID",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The UID of the message the part came from",
            },
            "part": {
                "display_name": "MIME Part",
                "short_desc": "The MIME part specifier retrieved",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The MIME part specifier that was retrieved",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size of the part in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The size of the retrieved part in bytes",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "The raw part content",
                "type": AbstractDataProviderTypeMap."data",
                "desc": "The part content exactly as sent by the server, with no decoding applied",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for listing mailboxes
public class ImapListMailboxesRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "pattern": {
                "display_name": "Pattern",
                "short_desc": "The mailbox name pattern to match",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox name pattern to match; `*` matches at any depth and `%` matches "
                    "within a single hierarchy level",
                "default_value": "*",
                "example_value": "*",
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "The reference name",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The reference name; normally left empty",
                "default_value": "",
            },
            "subscribed_only": {
                "display_name": "Subscribed Only?",
                "short_desc": "List only subscribed mailboxes?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "List only mailboxes the account is subscribed to, using `LSUB` instead of "
                    "`LIST`",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for a mailbox listing
public class ImapListMailboxesResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "mailboxes": {
                "display_name": "Mailboxes",
                "short_desc": "The matching mailboxes",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."hash", True),
                "desc": "The matching mailboxes; each entry has `name`, `delimiter` and `flags` keys.\n\n"
                    "A mailbox carrying the `\\Noselect` flag is a hierarchy node that cannot hold "
                    "messages",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "The number of mailboxes returned",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The number of mailboxes returned",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for changing message flags
public class ImapSetFlagsRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox holding the messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox holding the messages",
                "default_value": "INBOX",
                "example_value": "INBOX",
            },
            "uids": {
                "display_name": "Message UIDs",
                "short_desc": "The UIDs of the messages to change",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."int", False),
                "desc": "The UIDs of the messages whose flags should change",
                "required": True,
                "example_value": (4392,),
            },
            "flags": {
                "display_name": "Flags",
                "short_desc": "The flags to apply",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", False),
                "desc": "The flags to apply (ex: `\\Seen`, `\\Flagged`, `\\Answered`, `\\Deleted`)",
                "required": True,
                "example_value": ("\\Seen",),
            },
            "mode": {
                "display_name": "Mode",
                "short_desc": "How the flags should be applied",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "How the flags should be applied to the messages",
                "default_value": "add",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "add",
                        "display_name": "Add",
                        "desc": "Add the given flags, leaving any others in place",
                    }, <AllowedValueInfo>{
                        "value": "remove",
                        "display_name": "Remove",
                        "desc": "Remove the given flags, leaving any others in place",
                    }, <AllowedValueInfo>{
                        "value": "replace",
                        "display_name": "Replace",
                        "desc": "Replace all flags on the messages with the given flags",
                    },
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for moving messages
public class ImapMoveMessagesRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Source Mailbox",
                "short_desc": "The mailbox holding the messages",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox holding the messages to move",
                "default_value": "INBOX",
                "example_value": "INBOX",
            },
            "uids": {
                "display_name": "Message UIDs",
                "short_desc": "The UIDs of the messages to move",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."int", False),
                "desc": "The UIDs of the messages to move",
                "required": True,
                "example_value": (4392,),
            },
            "target": {
                "display_name": "Target Mailbox",
                "short_desc": "The mailbox to move the messages to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox to move the messages to; it must already exist",
                "required": True,
                "example_value": "Archive",
            },
            "copy_only": {
                "display_name": "Copy Only?",
                "short_desc": "Copy the messages instead of moving them?",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Copy the messages instead of moving them, leaving the originals in place",
                "default_value": False,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for operations that act on a set of messages
public class ImapMessageSetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "uids": {
                "display_name": "Message UIDs",
                "short_desc": "The UIDs the operation acted on",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."int", True),
                "desc": "The UIDs the operation acted on",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "The number of messages the operation acted on",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The number of messages the operation acted on",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type for appending a message to a mailbox
public class ImapAppendRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox to append the message to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox to append the message to; it must already exist",
                "required": True,
                "example_value": "Sent",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "The complete RFC 822 message to store",
                "type": AbstractDataProviderTypeMap."data",
                "desc": "The complete message to store, in RFC 822 form.\n\n"
                    "Use this to file a copy of a message that was sent by other means, or to import "
                    "mail from another system",
                "required": True,
            },
            "flags": {
                "display_name": "Flags",
                "short_desc": "Flags to set on the stored message",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "desc": "Flags to set on the stored message (ex: `\\Seen`)",
                "example_value": ("\\Seen",),
            },
            "internal_date": {
                "display_name": "Internal Date",
                "short_desc": "The arrival time to record for the message",
                "type": AbstractDataProviderTypeMap."*date",
                "desc": "The arrival time to record for the message; defaults to the time the server "
                    "stores it",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for appending a message
public class ImapAppendResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox the message was stored in",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox the message was stored in",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size of the stored message in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The size of the stored message in bytes",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Event type for a matched message
public class ImapMessageEventDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = ImapMessageFields + {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox the message arrived in",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox the message arrived in",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Event type for a matched attachment
public class ImapAttachmentEventDataType inherits DataProvider::HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "mailbox": {
                "display_name": "Mailbox",
                "short_desc": "The mailbox the message arrived in",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The mailbox the message arrived in",
            },
            "uid": {
                "display_name": "Message UID",
                "short_desc": "The UID of the message holding the attachment",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The UID of the message holding the attachment",
            },
            "subject": {
                "display_name": "Subject",
                "short_desc": "The subject of the message holding the attachment",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The subject of the message holding the attachment",
            },
            "from": {
                "display_name": "From",
                "short_desc": "The sender of the message holding the attachment",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The sender of the message holding the attachment",
            },
            "name": {
                "display_name": "Filename",
                "short_desc": "The attachment filename",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The attachment filename",
            },
            "mime": {
                "display_name": "MIME Type",
                "short_desc": "The attachment MIME type",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The attachment MIME type",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The attachment size in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The size of the attachment content in bytes",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "The decoded attachment content",
                "type": AbstractDataProviderTypeMap."*data",
                "desc": "The decoded attachment content; present only when the `retrieve_attachments` "
                    "option is enabled",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Search request type singleton
public const ImapSearchRequestType = new ImapSearchRequestDataType();
#! Search response type singleton
public const ImapSearchResponseType = new ImapSearchResponseDataType();
#! Get-message request type singleton
public const ImapGetMessageRequestType = new ImapGetMessageRequestDataType();
#! Message response type singleton
public const ImapMessageResponseType = new ImapMessageResponseDataType();
#! Get-attachment request type singleton
public const ImapGetAttachmentRequestType = new ImapGetAttachmentRequestDataType();
#! Attachment response type singleton
public const ImapAttachmentResponseType = new ImapAttachmentResponseDataType();
#! List-mailboxes request type singleton
public const ImapListMailboxesRequestType = new ImapListMailboxesRequestDataType();
#! List-mailboxes response type singleton
public const ImapListMailboxesResponseType = new ImapListMailboxesResponseDataType();
#! Set-flags request type singleton
public const ImapSetFlagsRequestType = new ImapSetFlagsRequestDataType();
#! Move-messages request type singleton
public const ImapMoveMessagesRequestType = new ImapMoveMessagesRequestDataType();
#! Message-set response type singleton
public const ImapMessageSetResponseType = new ImapMessageSetResponseDataType();
#! Append request type singleton
public const ImapAppendRequestType = new ImapAppendRequestDataType();
#! Append response type singleton
public const ImapAppendResponseType = new ImapAppendResponseDataType();
#! Message event type singleton
public const ImapMessageEventType = new ImapMessageEventDataType();
#! Attachment event type singleton
public const ImapAttachmentEventType = new ImapAttachmentEventDataType();
}
