# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCompletePaymentDataProvider.qc Square complete payment action

/*  SquareCompletePaymentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for completing a payment
public class SquareCompletePaymentRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "payment_id": {
                "type": StringType,
                "display_name": "Payment ID",
                "short_desc": "The payment ID to complete",
                "desc": "The unique identifier of the payment to complete",
                "required": True,
            },
            "version_token": {
                "type": StringOrNothingType,
                "display_name": "Version Token",
                "short_desc": "Payment version token",
                "desc": "Used for optimistic concurrency. If provided, the request fails if "
                    "the current payment version doesn't match.",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square complete payment action data provider
public class SquareCompletePaymentDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "complete",
            "desc": "Complete a payment in Square (for delayed capture payments)",
            "type": "SquareCompletePaymentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "complete";
    }

    *string getDesc() {
        return "Complete a payment in Square (for delayed capture payments)";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareCompletePaymentRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquarePaymentDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string payment_id = remove req.payment_id;
        return doPost(sprintf("payments/%s/complete", payment_id), req).payment;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
