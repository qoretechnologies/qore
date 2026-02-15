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
    private {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "type": IntType,
                "desc": "The ID of the order to update",
                "required": True,
            },
            "status_id": {
                "display_name": "Status ID",
                "type": IntOrNothingType,
                "desc": "Numeric ID of the new order status (0=Incomplete, 1=Pending, "
                    "2=Shipped, 3=Partially Shipped, 4=Refunded, 5=Cancelled, "
                    "6=Declined, 7=Awaiting Payment, 8=Awaiting Pickup, "
                    "9=Awaiting Shipment, 10=Completed, 11=Awaiting Fulfillment, "
                    "12=Manual Verification Required, 13=Disputed, 14=Partially Refunded)",
            },
            "staff_notes": {
                "display_name": "Staff Notes",
                "type": StringOrNothingType,
                "desc": "Internal staff notes for the order",
            },
            "customer_message": {
                "display_name": "Customer Message",
                "type": StringOrNothingType,
                "desc": "Message from the customer",
            },
            "billing_address": {
                "display_name": "Billing Address",
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
