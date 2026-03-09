# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCreateOrderDataProvider.qc Square create order action data provider

/*  SquareCreateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Request type for creating an order
public class SquareCreateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "location_id": {
                "type": StringType,
                "display_name": "Location ID",
                "short_desc": "The location ID (required)",
                "desc": "The ID of the location where the order is created",
                "required": True,
            },
            "line_items": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "Order line items",
                "desc": "List of line items, each with: name (string), quantity (string, "
                    "e.g. '1'), base_price_money (hash with amount in cents and currency, "
                    "e.g. {amount: 1000, currency: 'USD'})",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer for this order",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID from an external system",
            },
            "taxes": {
                "type": AutoListOrNothingType,
                "display_name": "Taxes",
                "short_desc": "Order-level taxes",
                "desc": "List of taxes, each with: uid, name, percentage, scope "
                    "(ORDER or LINE_ITEM)",
            },
            "discounts": {
                "type": AutoListOrNothingType,
                "display_name": "Discounts",
                "short_desc": "Order-level discounts",
                "desc": "List of discounts, each with: uid, name, amount_money or percentage, "
                    "scope (ORDER or LINE_ITEM)",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "Initial order state",
                "desc": "The initial state of the order (OPEN or DRAFT, default: OPEN)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square create order action data provider
public class SquareCreateOrderDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new order in Square",
            "type": "SquareCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new order in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCreateOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareOrderDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "order": req,
        };
        return doPost("orders", body).order;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
