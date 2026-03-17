# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoSendTransactionalPushDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for sending a transactional push notification
public class CustomerIoSendTransactionalPushRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "transactional_message_id": {
                "type": IntType,
                "display_name": "Transactional Message ID",
                "short_desc": "The transactional message template ID",
                "desc": "The transactional message template ID to use for the push notification",
                "required": True,
                "preselected": True,
            },
            "identifiers": {
                "type": new CustomerIoIdentifiersDataType(),
                "display_name": "Identifiers",
                "short_desc": "Customer identifiers",
                "desc": "Customer identifiers such as id, email, or cio_id",
                "required": True,
                "preselected": True,
            },
            "message_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Message Data",
                "short_desc": "Data for merge tags",
                "desc": "Data for merge tags in the push notification template",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Override push title",
                "desc": "Override the push notification title",
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Override push body text",
                "desc": "Override the push notification body text",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for sending a transactional push notification
public class CustomerIoSendTransactionalPushResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "delivery_id": {
                "type": StringOrNothingType,
                "display_name": "Delivery ID",
                "short_desc": "The unique delivery identifier",
                "desc": "The unique identifier for the push delivery",
            },
            "queued_at": {
                "type": IntOrNothingType,
                "display_name": "Queued At",
                "short_desc": "Unix timestamp when queued",
                "desc": "Unix timestamp when the push notification was queued for delivery",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for sending a transactional push notification
public class CustomerIoSendTransactionalPushDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "send-transactional-push",
            "desc": "Send a transactional push notification via Customer.io",
            "type": "CustomerIoSendTransactionalPushDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoSendTransactionalPushRequestDataType RequestType();

        #! Response type
        static CustomerIoSendTransactionalPushResponseDataType ResponseType();
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
        return "send-transactional-push";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Send a transactional push notification via Customer.io";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        return doPost("/send/push", body);
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
