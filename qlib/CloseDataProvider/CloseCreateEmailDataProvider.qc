# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateEmailDataProvider.qc Close create email action data provider

/*  CloseCreateEmailDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Request type for creating an email
public class CloseCreateEmailRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an email
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead to associate with this email",
                "required": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Email status",
                "desc": "Email status: `draft`, `outbox`, or `sent`",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Draft",
                        "short_desc": "Draft email",
                        "desc": "Save the email as a draft",
                        "value": "draft",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outbox",
                        "short_desc": "Queued for sending",
                        "desc": "Queue the email for sending",
                        "value": "outbox",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sent",
                        "short_desc": "Already sent",
                        "desc": "Mark the email as already sent",
                        "value": "sent",
                    },
                ),
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Email subject",
                "desc": "The subject line of the email",
                "preselected": True,
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Email body as plain text",
                "desc": "The plain text body of the email",
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "Email body as HTML",
                "desc": "The HTML body of the email",
            },
            "sender": {
                "type": StringOrNothingType,
                "display_name": "Sender",
                "short_desc": "Sender email address",
                "desc": "The sender email address",
            },
            "to": {
                "type": SoftListOrNothingType,
                "display_name": "To",
                "short_desc": "To recipients",
                "desc": "A list of recipient email addresses as strings",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact to associate with this email",
            },
            "cc": {
                "type": SoftListOrNothingType,
                "display_name": "CC",
                "short_desc": "CC recipients",
                "desc": "A list of CC recipient email addresses as strings",
            },
            "bcc": {
                "type": SoftListOrNothingType,
                "display_name": "BCC",
                "short_desc": "BCC recipients",
                "desc": "A list of BCC recipient email addresses as strings",
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "Email direction",
                "desc": "The direction of the email: `incoming` or `outgoing`",
                "example_value": "outgoing",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Incoming",
                        "short_desc": "Received email",
                        "desc": "Email received from an external sender",
                        "value": "incoming",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outgoing",
                        "short_desc": "Sent email",
                        "desc": "Email sent to an external recipient",
                        "value": "outgoing",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create email action data provider
public class CloseCreateEmailDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new email activity in Close",
            "type": "CloseCreateEmailDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new email activity in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateEmailRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseEmailDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("activity/email", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
