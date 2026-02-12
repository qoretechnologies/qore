# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCreatePaidOrderDataProvider.qc Square create paid order action

/*  SquareCreatePaidOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a paid order
public class SquareCreatePaidOrderRequestDataType inherits HashDataType {
    private {
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
                "desc": "List of line items, each with: name (string), quantity (string), "
                    "base_price_money (hash with amount in cents and currency)",
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
            "payment_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Payment IDs",
                "short_desc": "Payment IDs to apply",
                "desc": "List of payment IDs to apply to the order. If empty, an external "
                    "payment source will be used.",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square create paid order action data provider (creates order then marks as paid)
public class SquareCreatePaidOrderDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-paid",
            "desc": "Create a new order and mark it as paid in Square",
            "type": "SquareCreatePaidOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "create-paid";
    }

    *string getDesc() {
        return "Create a new order and mark it as paid in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCreatePaidOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareOrderDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Extract payment_ids before creating order
        auto payment_ids = remove req.payment_ids;

        # Step 1: Create the order
        hash<auto> create_body = {
            "idempotency_key": generateIdempotencyKey(),
            "order": req,
        };
        hash<auto> result = doPost("orders", create_body);
        string order_id = result.order.id;

        # Step 2: Pay the order
        hash<auto> pay_body = {
            "idempotency_key": generateIdempotencyKey(),
        };
        if (payment_ids) {
            pay_body.payment_ids = payment_ids;
        }
        hash<auto> pay_result = doPost(sprintf("orders/%s/pay", order_id), pay_body);
        return pay_result.order;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
