# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageSendDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
    constructor(GoogleRestClient::GoogleRestClient rest) : GoogleApiDataProvider(rest, GoogleDiscoveryGmailApiName,
            ResourceAction, {"userId": "me"}) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        Message msg(req.sender ?? "me@gmail.com", req.subject);
        map msg.addTO($1), req.to;
        map msg.addCC($1), req.cc;
        map msg.addBCC($1), req.bcc;
        msg.setBody(req.body, req."body-encoding", req."body-mime-type");
        map msg.attach($1."data".name, $1."data".mime_type, $1."data"."data", $1.encoding, $1.hdr), req.attachments;

        # throws a MESSAGE-ERROR exception if the message cannot be sent
        string mstr = make_base64_url_string(msg.serialize());
        hash<auto> greq = {
            "userId": "me",
            "raw": mstr,
        };
        return GoogleApiDataProvider::doRequestImpl(greq, request_options);
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
        "short_desc": "quoted-printable for string data, base64 for binary data",
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
