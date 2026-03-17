# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoTriggerBroadcastDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data type for Customer.io broadcast recipients filter
public class CustomerIoBroadcastRecipientsDataType inherits HashDataType {
    public {
        const Fields = {
            "segment": {
                "type": AutoHashOrNothingType,
                "display_name": "Segment",
                "short_desc": "Segment filter for recipients",
                "desc": "A segment filter hash with `id` key to target a specific segment",
            },
            "ids": {
                "type": AutoListOrNothingType,
                "display_name": "Customer IDs",
                "short_desc": "List of customer IDs",
                "desc": "A list of customer IDs to send the broadcast to",
            },
            "emails": {
                "type": AutoListOrNothingType,
                "display_name": "Email Addresses",
                "short_desc": "List of email addresses",
                "desc": "A list of email addresses to send the broadcast to",
            },
            "per_user_data": {
                "type": AutoListOrNothingType,
                "display_name": "Per-User Data",
                "short_desc": "Per-user data array",
                "desc": "An array of per-user data hashes, each containing `id` or `email` and custom data",
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

#! Request type for triggering a broadcast
public class CustomerIoTriggerBroadcastRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "broadcast_id": {
                "type": IntType,
                "display_name": "Broadcast ID",
                "short_desc": "The broadcast ID to trigger",
                "desc": "The unique Customer.io broadcast ID to trigger",
                "required": True,
                "ref_data": "broadcasts",
                "preselected": True,
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Data",
                "short_desc": "Data to pass to the broadcast",
                "desc": "Data to pass to the broadcast",
            },
            "recipients": {
                "type": new CustomerIoBroadcastRecipientsDataType(AutoHashOrNothingType),
                "display_name": "Recipients",
                "short_desc": "Recipients filter",
                "desc": "Recipients filter for the broadcast",
            },
            "emails": {
                "type": AutoListOrNothingType,
                "display_name": "Emails",
                "short_desc": "List of email addresses",
                "desc": "List of email addresses to send the broadcast to",
            },
            "ids": {
                "type": AutoListOrNothingType,
                "display_name": "IDs",
                "short_desc": "List of customer IDs",
                "desc": "List of customer IDs to send the broadcast to",
            },
            "per_user_data": {
                "type": AutoListOrNothingType,
                "display_name": "Per-User Data",
                "short_desc": "Per-user data array",
                "desc": "Per-user data array for the broadcast",
            },
            "id_ignore_missing": {
                "type": BoolOrNothingType,
                "display_name": "Ignore Missing IDs",
                "short_desc": "Ignore missing IDs",
                "desc": "Whether to ignore missing customer IDs",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for triggering a broadcast
public class CustomerIoTriggerBroadcastDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "trigger-broadcast",
            "desc": "Trigger a Customer.io broadcast",
            "type": "CustomerIoTriggerBroadcastDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoTriggerBroadcastRequestDataType RequestType();
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
        return "trigger-broadcast";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Trigger a Customer.io broadcast";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int broadcast_id = req.broadcast_id;
        hash<auto> body = req - "broadcast_id";
        return doPost(sprintf("/broadcasts/%d/triggers", broadcast_id), body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider
