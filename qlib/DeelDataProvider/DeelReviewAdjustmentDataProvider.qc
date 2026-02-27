# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelReviewAdjustmentDataProvider.qc Deel review invoice adjustment action data provider

/*  DeelReviewAdjustmentDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Request type for reviewing an invoice adjustment
public class DeelReviewAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "adjustment_id": {
                "type": StringType,
                "display_name": "Adjustment ID",
                "short_desc": "The adjustment ID to review",
                "desc": "The unique identifier of the invoice adjustment to review",
                "required": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Review Status",
                "short_desc": "The review decision",
                "desc": "The review decision for the invoice adjustment: `approved` or `declined`",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "approved", "desc": "Approve the adjustment"},
                    <AllowedValueInfo>{"value": "declined", "desc": "Decline the adjustment"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel review invoice adjustment action data provider
public class DeelReviewAdjustmentDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "review",
            "desc": "Approve or decline an invoice adjustment in Deel",
            "type": "DeelReviewAdjustmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "review";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Approve or decline an invoice adjustment in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelReviewAdjustmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelAdjustmentDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string adjustment_id = remove req.adjustment_id;
        return doPost("invoice-adjustments/" + encode_url(adjustment_id, True) + "/review", req).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
