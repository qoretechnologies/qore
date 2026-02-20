# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeUpdateSubscriptionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Request type for updating a subscription
public class ChargebeeUpdateSubscriptionRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Subscription ID",
                "short_desc": "The subscription ID to update",
                "desc": "The unique identifier of the subscription to update",
                "required": True,
            },
            "subscription_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Subscription Items",
                "short_desc": "List of item price objects",
                "desc": "List of subscription item objects containing item_price_id, quantity, "
                    "unit_price, etc.",
            },
            "billing_cycles": {
                "type": IntOrNothingType,
                "display_name": "Billing Cycles",
                "short_desc": "Number of billing cycles",
                "desc": "The number of billing cycles the subscription runs before cancelling",
            },
            "auto_collection": {
                "type": StringOrNothingType,
                "display_name": "Auto Collection",
                "short_desc": "Auto-collect payments",
                "desc": "Whether to automatically collect charges for this subscription",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "on", "desc": "Automatically collect charges"},
                    <AllowedValueInfo>{"value": "off", "desc": "Do not automatically collect charges"},
                ),
            },
            "po_number": {
                "type": StringOrNothingType,
                "display_name": "PO Number",
                "short_desc": "Purchase order number",
                "desc": "The purchase order number for this subscription",
            },
            "meta_data": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A set of key-value pairs for storing additional information about the subscription",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating an existing subscription
public class ChargebeeUpdateSubscriptionDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-subscription",
            "desc": "Update an existing subscription in Chargebee",
            "type": "ChargebeeUpdateSubscriptionDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ChargebeeUpdateSubscriptionRequestDataType RequestType();

        #! Response type
        static ChargebeeSubscriptionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-subscription";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing subscription in Chargebee";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating subscription %s", id));

        hash<auto> response = doPost("/subscriptions/" + id + "/update_for_items", body);
        return response.subscription;
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

} # namespace ChargebeeDataProvider
