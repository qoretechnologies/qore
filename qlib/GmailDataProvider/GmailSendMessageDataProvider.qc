# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageSendDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GmailDataProvider module
public namespace GmailDataProvider {
#! Data provider for sending gmail messages
public class GmailMessageSendDataProvider inherits GoogleDataProvider::GoogleApiDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "send-message",
            "desc": "Gmail send email message data provider",
            "type": "GmailMessageSendDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GmailSendMessageRequestDataType;

        #! Gmail message send resource action
        const ResourceAction = "users/messages/send";
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest) : GoogleApiDataProvider(rest, GoogleDiscoveryGmailApiName,
            ResourceAction, {"userId": "me"}) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> greq = {
            "userId": "me",
        } + GmailMessageSendDataProvider::makeMessageBody(req);
        return GoogleApiDataProvider::doRequestImpl(greq, request_options);
    }

    #! Returns the MIME message for a send or draft request, base64url-encoded as Gmail's API expects
    /** @param req the request; see @ref GmailSendMessageRequestDataType for the keys

        @return the serialized MIME message encoded with @ref Qore::make_base64_url_string()

        @throw MESSAGE-ERROR the message cannot be serialized (ex: no recipients)

        @note The threading headers follow RFC 5322 section 3.6.4: a reply carries the original's \c Message-ID
        in \c In-Reply-To and the conversation's IDs in \c References; when only \c in_reply_to is given, it
        also serves as \c References, because Gmail threads a message under the original only when both
        headers refer to it

        @since GmailDataProvider 1.2
    */
    static string makeRawMessage(hash<auto> req) {
        Message msg(req.sender ?? "me@gmail.com", req.subject);
        map msg.addTO($1), req.to;
        map msg.addCC($1), req.cc;
        map msg.addBCC($1), req.bcc;
        msg.setBody(req.body, req."body-encoding", req."body-mime-type");
        map msg.attach($1."data".name, $1."data".mime_type, parse_base64_string($1."data".content), $1.encoding,
            $1.hdr), req.attachments;
        if (req.in_reply_to.val()) {
            msg.addHeader({"In-Reply-To": req.in_reply_to});
        }
        if (*string references = req.references ?? req.in_reply_to) {
            msg.addHeader({"References": references});
        }

        # throws a MESSAGE-ERROR exception if the message cannot be sent
        return make_base64_url_string(msg.serialize());
    }

    #! Returns the Gmail API \c Message body for a send or draft request
    /** @param req the request; see @ref GmailSendMessageRequestDataType for the keys

        @return a hash with the \c raw MIME message (see makeRawMessage()) and, when the request names a thread
        in \c thread_id, the \c threadId the message belongs to

        @throw MESSAGE-ERROR the message cannot be serialized (ex: no recipients)

        @since GmailDataProvider 1.2
    */
    static hash<auto> makeMessageBody(hash<auto> req) {
        hash<auto> rv = {
            "raw": GmailMessageSendDataProvider::makeRawMessage(req),
        };
        if (req.thread_id.val()) {
            rv.threadId = req.thread_id;
        }
        return rv;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the Google request type for the operation
    private AbstractDataProviderType getGoogleRequestType() {
        return GoogleDataProviderBase::getRequestTypeForSchema(GoogleDiscoveryGmailApiName, ResourceAction);
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return GoogleDataProviderBase::getResponseTypeForSchema(GoogleDiscoveryGmailApiName, ResourceAction);
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

const BodyEncodingList = (
    <AllowedValueInfo>{
        "display_name": "Auto",
        "short_desc": "'quoted-printable' for string data, 'base64' for binary data",
        "desc": "`quoted-printable` for string data, `base64` for binary data",
        "value": "default",
    },
    <AllowedValueInfo>{
        "display_name": "None",
        "short_desc": "No content encoding (not recommended)",
        "desc": "No content encoding (not recommended)",
        "value": "none",
    },
    <AllowedValueInfo>{
        "display_name": "Quoted Printable",
        "short_desc": "Ideal for string data",
        "desc": "Ideal for string data",
        "value": "quoted-printable",
    },
    <AllowedValueInfo>{
        "display_name": "Base64",
        "short_desc": "Ideal for binary data",
        "desc": "Ideal for binary data",
        "value": "base64",
    },
);

const BodyMimeTypeList = (
    <AllowedValueInfo>{
        "display_name": "text/plain",
        "short_desc": "Plain text",
        "desc": "Plain text",
        "value": "text/plain",
    },
    <AllowedValueInfo>{
        "display_name": "text/html",
        "short_desc": "HTML text",
        "desc": "HTML text",
        "value": "text/html",
    },
);

#! Gmail attachment type
public class GmailSendMessageAttachmentDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "data": {
                "display_name": "Attachment Data",
                "type": FileDataType,
                "short_desc": "The data, file name, and MIME type for the attachment",
                "desc": "The data, file name, and MIME type for the attachment",
                "required": True,
            },
            "encoding": {
                "display_name": "Attachment Encoding",
                "type": StringType,
                "short_desc": "The encoding to use for the attachment",
                "desc": "The encoding to use for the attachment:\n"
                    "- `default`: `quoted-printable` for string data, `base64` for binary data\n"
                    "- `none`: no content encoding (not recommended)\n"
                    "- `quoted-printable`: quoted printable encoding \n"
                    "- `base64`: base64 encoding",
                "default_value": "default",
                "allowed_values": BodyEncodingList,
            },
            "hdr": {
                "display_name": "Headers",
                "type": HashOrNothingType,
                "short_desc": "Optional headers to send with the attachment",
                "desc": "Optional headers to send with the attachment",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Gmail attachment type
public const GmailSendMessageAttachmentDataType = new GmailSendMessageAttachmentDataType();

#! Gmail send message type
public class GmailSendMessageRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "to": {
                "display_name": "To",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "Message recipient addresses",
                "desc": "One or more `To:` addresses for the email",
                "required": True,
            },
            "cc": {
                "display_name": "CC",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "Carbon copy addresses",
                "desc": "One or more `Cc:` addresses for the email",
            },
            "bcc": {
                "display_name": "BCC",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "Blind carbon copy addresses",
                "desc": "One or more `Bcc:` addresses for the email",
            },
            "subject": {
                "display_name": "Subject",
                "type": StringType,
                "short_desc": "The subject for the email",
                "desc": "The subject for the email",
                "required": True,
            },
            "body": {
                "display_name": "Message Body",
                "type": StringType,
                "short_desc": "The message body",
                "desc": "The message body",
                "required": True,
            },
            "attachments": {
                "display_name": "Attachments",
                "type": new SoftListDataType(GmailSendMessageAttachmentDataType, True),
                "short_desc": "Message attachments",
                "desc": "Any attachments for the message",
            },
            "body-encoding": {
                "display_name": "Body Encoding",
                "type": StringType,
                "short_desc": "The encoding to use for the attachment",
                "desc": "The encoding to use for the attachment:\n"
                    "- `default`: `quoted-printable` for string data, `base64` for binary data\n"
                    "- `none`: no content encoding (not recommended)\n"
                    "- `quoted-printable`: quoted printable encoding \n"
                    "- `base64`: base64 encoding",
                "default_value": "default",
                "allowed_values": BodyEncodingList,
            },
            "body-mime-type": {
                "display_name": "Body Mime Type",
                "type": StringOrNothingType,
                "short_desc": "The MIME type of the message body",
                "desc": "The MIME type of the message body",
                "allowed_values": BodyMimeTypeList,
                "allowed_values_creatable": True,
            },
            "sender": {
                "display_name": "Sender",
                "type": StringOrNothingType,
                "short_desc": "The sender's address for the email",
                "desc": "The sender's address for the email",
            },
            "thread_id": {
                "display_name": "Thread ID",
                "type": StringOrNothingType,
                "short_desc": "The ID of the Gmail thread to add the message to",
                "desc": "The ID of the Gmail thread to add the message to; set this to reply within an existing "
                    "conversation. Gmail only honors it when `subject` matches the thread's subject and "
                    "`in_reply_to` (or `references`) names a message in the thread. Watch events carry this "
                    "value as `threadId`",
                "example_value": "18c6b13f059fc0bd",
            },
            "in_reply_to": {
                "display_name": "In Reply To",
                "type": StringOrNothingType,
                "short_desc": "The Message-ID header value of the message being answered",
                "desc": "The `Message-ID` header value of the message being answered, including the angle "
                    "brackets; sent as the `In-Reply-To` header and, unless `references` is set, also as the "
                    "`References` header, so that mail clients and Gmail thread the reply under the original "
                    "message. Watch events carry this value as `Message-ID`",
                "example_value": "<CAF1mXwUPjLb-Q3P2Ee5Ax9U1YrMU=ZxWq7MdX0KE_4Y5N6uD5Q@mail.gmail.com>",
            },
            "references": {
                "display_name": "References",
                "type": StringOrNothingType,
                "short_desc": "The Message-ID header values of the conversation being answered",
                "desc": "The `References` header value: the `Message-ID` values of the messages in the "
                    "conversation being answered, oldest first, separated by spaces and including the angle "
                    "brackets. If not set, `in_reply_to` is used. Watch events carry the original message's "
                    "`References` header",
                "example_value": "<original@mail.example.com> <first-reply@mail.example.com>",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Gmail send message type
public const GmailSendMessageRequestDataType = new GmailSendMessageRequestDataType();
}
