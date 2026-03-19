# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoSendTransactionalEmailDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Data type for Customer.io customer identifiers
public class CustomerIoIdentifiersDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer's unique identifier",
                "desc": "The customer's unique identifier in your system",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer's email address",
                "desc": "The customer's email address",
            },
            "cio_id": {
                "type": StringOrNothingType,
                "display_name": "Customer.io ID",
                "short_desc": "The Customer.io immutable ID",
                "desc": "The immutable identifier assigned by Customer.io (prefixed with `cio_`)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Request type for sending a transactional email
public class CustomerIoSendTransactionalEmailRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "transactional_message_id": {
                "type": IntType,
                "display_name": "Transactional Message ID",
                "short_desc": "The transactional message template ID",
                "desc": "The transactional message template ID to use for the email",
                "required": True,
                "preselected": True,
            },
            "to": {
                "type": StringType,
                "display_name": "To",
                "short_desc": "Recipient email address",
                "desc": "The recipient email address",
                "required": True,
                "preselected": True,
            },
            "identifiers": {
                "type": new CustomerIoIdentifiersDataType(AutoHashOrNothingType),
                "display_name": "Identifiers",
                "short_desc": "Customer identifiers (id, email, or cio_id)",
                "desc": "Customer identifiers such as id, email, or cio_id",
            },
            "message_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Message Data",
                "short_desc": "Data for Liquid merge tags in the message",
                "desc": "Data for Liquid merge tags in the message template",
            },
            "from": {
                "type": StringOrNothingType,
                "display_name": "From",
                "short_desc": "Override sender email",
                "desc": "Override the sender email address",
            },
            "reply_to": {
                "type": StringOrNothingType,
                "display_name": "Reply To",
                "short_desc": "Override reply-to email",
                "desc": "Override the reply-to email address",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Override subject line",
                "desc": "Override the subject line of the email",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Override message body (HTML)",
                "desc": "Override the message body with HTML content",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for sending a transactional email
public class CustomerIoSendTransactionalEmailResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "delivery_id": {
                "type": StringOrNothingType,
                "display_name": "Delivery ID",
                "short_desc": "The unique delivery identifier",
                "desc": "The unique identifier for the email delivery",
            },
            "queued_at": {
                "type": IntOrNothingType,
                "display_name": "Queued At",
                "short_desc": "Unix timestamp when queued",
                "desc": "Unix timestamp when the email was queued for delivery",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for sending a transactional email
public class CustomerIoSendTransactionalEmailDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "send-transactional-email",
            "desc": "Send a transactional email via Customer.io",
            "type": "CustomerIoSendTransactionalEmailDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoSendTransactionalEmailRequestDataType RequestType();

        #! Response type
        static CustomerIoSendTransactionalEmailResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "send-transactional-email";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Send a transactional email via Customer.io";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        return doPost("/send/email", body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
