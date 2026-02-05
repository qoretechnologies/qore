# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoCreateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for creating orders
public class ShippoCreateOrderDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-order",
            "desc": "Creates a new order in Shippo",
            "type": "ShippoCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoCreateOrderRequestDataType RequestType();

        #! Response type
        static ShippoOrderDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-order";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a new order in Shippo";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};

        # Required fields
        body.to_address = req.to_address;
        body.placed_at = format_date("YYYY-MM-DDTHH:mm:ssZ", req.placed_at);
        body.weight = req.weight;
        body.weight_unit = req.weight_unit;

        # Optional fields
        if (exists req.from_address) {
            body.from_address = req.from_address;
        }
        if (exists req.order_number) {
            body.order_number = req.order_number;
        }
        if (exists req.order_status) {
            body.order_status = req.order_status;
        }
        if (exists req.line_items) {
            body.line_items = req.line_items;
        }
        if (exists req.shipping_cost) {
            body.shipping_cost = req.shipping_cost;
        }
        if (exists req.shipping_cost_currency) {
            body.shipping_cost_currency = req.shipping_cost_currency;
        }
        if (exists req.shipping_method) {
            body.shipping_method = req.shipping_method;
        }
        if (exists req.subtotal_price) {
            body.subtotal_price = req.subtotal_price;
        }
        if (exists req.total_price) {
            body.total_price = req.total_price;
        }
        if (exists req.total_tax) {
            body.total_tax = req.total_tax;
        }
        if (exists req.currency) {
            body.currency = req.currency;
        }
        if (exists req.notes) {
            body.notes = req.notes;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating order with body: %y", body));
        hash<auto> response = doPost("/orders", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created order %y", response.object_id));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for creating orders
public class ShippoCreateOrderRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "to_address": {
                "type": new ShippoAddressInputDataType(),
                "display_name": "Shipping Address",
                "short_desc": "Customer shipping address",
                "desc": "The destination address for the order",
                "required": True,
            },
            "from_address": {
                "type": AutoHashOrNothingType,
                "display_name": "From Address",
                "short_desc": "Sender address",
                "desc": "The origin address for the order (optional)",
            },
            "order_number": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Your order number",
                "desc": "Your internal order number or reference",
            },
            "order_status": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Order status",
                "desc": "The current status of the order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "UNKNOWN", "desc": "Unknown status"},
                    <AllowedValueInfo>{"value": "PAID", "desc": "Order has been paid"},
                    <AllowedValueInfo>{"value": "SHIPPED", "desc": "Order has been shipped"},
                    <AllowedValueInfo>{"value": "DELIVERED", "desc": "Order has been delivered"},
                    <AllowedValueInfo>{"value": "CANCELLED", "desc": "Order was cancelled"},
                    <AllowedValueInfo>{"value": "RETURNED", "desc": "Order was returned"},
                ),
            },
            "placed_at": {
                "type": DateType,
                "display_name": "Placed At",
                "short_desc": "Order placement date",
                "desc": "The date/time when the order was placed",
                "required": True,
            },
            "line_items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order items",
                "desc": "Array of line items in the order",
            },
            "shipping_cost": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Cost",
                "short_desc": "Customer shipping cost",
                "desc": "The shipping cost charged to the customer",
            },
            "shipping_cost_currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Cost Currency",
                "short_desc": "Currency code",
                "desc": "Three-letter ISO currency code for shipping cost",
            },
            "shipping_method": {
                "type": SoftStringOrNothingType,
                "display_name": "Shipping Method",
                "short_desc": "Selected shipping method",
                "desc": "The shipping method selected by the customer",
            },
            "subtotal_price": {
                "type": SoftStringOrNothingType,
                "display_name": "Subtotal",
                "short_desc": "Order subtotal",
                "desc": "The subtotal price of the order (before shipping and tax)",
            },
            "total_price": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Price",
                "short_desc": "Order total",
                "desc": "The total price of the order",
            },
            "total_tax": {
                "type": SoftStringOrNothingType,
                "display_name": "Total Tax",
                "short_desc": "Tax amount",
                "desc": "The total tax amount for the order",
            },
            "currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Price currency",
                "desc": "Three-letter ISO currency code for prices",
            },
            "weight": {
                "type": SoftStringType,
                "display_name": "Weight",
                "short_desc": "Total weight",
                "desc": "The total weight of the order",
                "required": True,
            },
            "weight_unit": {
                "type": SoftStringType,
                "display_name": "Weight Unit",
                "short_desc": "Weight unit",
                "desc": "The unit of measurement for weight (g, oz, lb, kg)",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "g", "desc": "Grams"},
                    <AllowedValueInfo>{"value": "oz", "desc": "Ounces"},
                    <AllowedValueInfo>{"value": "lb", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "kg", "desc": "Kilograms"},
                ),
            },
            "notes": {
                "type": SoftStringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Any notes or special instructions for the order",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoCreateOrderRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
