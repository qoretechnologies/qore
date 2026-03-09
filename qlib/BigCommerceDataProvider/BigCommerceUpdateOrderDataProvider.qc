# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceUpdateOrderDataProvider.qc BigCommerce update order action data provider

/*  BigCommerceUpdateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Request type for updating an order
public class BigCommerceUpdateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "The order to update",
                "type": IntType,
                "desc": "The ID of the order to update",
                "required": True,
                "ref_data": "orders",
            },
            "status_id": {
                "display_name": "Status ID",
                "short_desc": "New order status ID",
                "type": IntOrNothingType,
                "desc": "The new order status ID",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Incomplete"},
                    <AllowedValueInfo>{"value": 1, "desc": "Pending"},
                    <AllowedValueInfo>{"value": 2, "desc": "Shipped"},
                    <AllowedValueInfo>{"value": 3, "desc": "Partially Shipped"},
                    <AllowedValueInfo>{"value": 4, "desc": "Refunded"},
                    <AllowedValueInfo>{"value": 5, "desc": "Cancelled"},
                    <AllowedValueInfo>{"value": 6, "desc": "Declined"},
                    <AllowedValueInfo>{"value": 7, "desc": "Awaiting Payment"},
                    <AllowedValueInfo>{"value": 8, "desc": "Awaiting Pickup"},
                    <AllowedValueInfo>{"value": 9, "desc": "Awaiting Shipment"},
                    <AllowedValueInfo>{"value": 10, "desc": "Completed"},
                    <AllowedValueInfo>{"value": 11, "desc": "Awaiting Fulfillment"},
                    <AllowedValueInfo>{"value": 12, "desc": "Manual Verification Required"},
                    <AllowedValueInfo>{"value": 13, "desc": "Disputed"},
                    <AllowedValueInfo>{"value": 14, "desc": "Partially Refunded"},
                ),
            },
            "staff_notes": {
                "display_name": "Staff Notes",
                "short_desc": "Internal staff notes",
                "type": StringOrNothingType,
                "desc": "Internal staff notes for the order",
            },
            "customer_message": {
                "display_name": "Customer Message",
                "short_desc": "Customer message",
                "type": StringOrNothingType,
                "desc": "Message from the customer",
            },
            "billing_address": {
                "display_name": "Billing Address",
                "short_desc": "Billing address",
                "type": AutoHashOrNothingType,
                "desc": "Billing address object for the order",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce update order action data provider
public class BigCommerceUpdateOrderDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing order in BigCommerce",
            "type": "BigCommerceUpdateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing order in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceUpdateOrderRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceOrderDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int order_id = remove req.order_id;
        return doPutV2("orders/" + order_id, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
