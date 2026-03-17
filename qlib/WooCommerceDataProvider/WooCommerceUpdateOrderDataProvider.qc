# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceUpdateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Request type for updating an order
public class WooCommerceUpdateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Order ID",
                "type": IntType,
                "desc": "The unique identifier for the order to update",
                "required": True,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": IntOrNothingType,
                "desc": "The customer ID for the order",
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Order status",
                "allowed_values": OrderStatusAllowedValues,
            },
            "billing": {
                "display_name": "Billing",
                "type": AutoHashOrNothingType,
                "desc": "Billing address data",
            },
            "shipping": {
                "display_name": "Shipping",
                "type": AutoHashOrNothingType,
                "desc": "Shipping address data",
            },
            "line_items": {
                "display_name": "Line Items",
                "type": SoftListOrNothingType,
                "desc": "List of line items (products) for the order",
            },
            "payment_method": {
                "display_name": "Payment Method",
                "type": StringOrNothingType,
                "desc": "Payment method identifier",
            },
            "payment_method_title": {
                "display_name": "Payment Method Title",
                "type": StringOrNothingType,
                "desc": "Human-readable payment method title",
            },
            "set_paid": {
                "display_name": "Set Paid",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the order is marked as paid",
            },
            "currency": {
                "display_name": "Currency",
                "type": StringOrNothingType,
                "desc": "Three-letter ISO currency code",
            },
            "shipping_lines": {
                "display_name": "Shipping Lines",
                "type": SoftListOrNothingType,
                "desc": "List of shipping lines for the order",
            },
            "fee_lines": {
                "display_name": "Fee Lines",
                "type": SoftListOrNothingType,
                "desc": "List of fee lines for the order",
            },
            "coupon_lines": {
                "display_name": "Coupon Lines",
                "type": SoftListOrNothingType,
                "desc": "List of coupon lines for the order",
            },
            "customer_note": {
                "display_name": "Customer Note",
                "type": StringOrNothingType,
                "desc": "Note left by the customer during checkout",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "type": SoftListOrNothingType,
                "desc": "List of metadata key-value pairs for the order",
            },
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Data provider for updating a WooCommerce order
public class WooCommerceUpdateOrderDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Updates an existing WooCommerce order",
            "type": "WooCommerceUpdateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceUpdateOrderRequestDataType RequestType();

        #! Response type
        static WooCommerceOrderDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing WooCommerce order";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = remove req.id;
        return doPut(sprintf("orders/%d", id), {} + req);
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

} # namespace WooCommerceDataProvider
