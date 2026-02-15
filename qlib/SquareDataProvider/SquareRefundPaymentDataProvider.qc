# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareRefundPaymentDataProvider.qc Square refund payment action

/*  SquareRefundPaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for refunding a payment (user-friendly flattened fields)
public class SquareRefundPaymentRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "payment_id": {
                "type": StringType,
                "display_name": "Payment ID",
                "short_desc": "The payment ID to refund",
                "desc": "The unique identifier of the payment to refund",
                "required": True,
            },
            "amount": {
                "type": IntType,
                "display_name": "Amount (cents)",
                "short_desc": "Refund amount in cents",
                "desc": "The amount to refund in the smallest currency denomination "
                    "(e.g. cents for USD). For example, 1000 = $10.00",
                "required": True,
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code (default: USD)",
                "desc": "The ISO 4217 currency code (default: USD)",
                "default_value": "USD",
            },
            "reason": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Reason for the refund",
                "desc": "A description of the reason for the refund",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square refund payment action data provider
public class SquareRefundPaymentDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "refund",
            "desc": "Refund a payment in Square",
            "type": "SquareRefundPaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "refund";
    }

    *string getDesc() {
        return "Refund a payment in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareRefundPaymentRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareRefundDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Map user-friendly fields to Square API format
        hash<auto> body = {
            "idempotency_key": generateIdempotencyKey(),
            "payment_id": req.payment_id,
            "amount_money": {
                "amount": req.amount,
                "currency": req.currency ?? "USD",
            },
        };
        if (req.reason) {
            body.reason = req.reason;
        }
        return doPost("refunds", body).refund;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
