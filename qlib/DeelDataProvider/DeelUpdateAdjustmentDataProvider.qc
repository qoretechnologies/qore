# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelUpdateAdjustmentDataProvider.qc Deel update invoice adjustment action data provider

/*  DeelUpdateAdjustmentDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating an invoice adjustment
public class DeelUpdateAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "adjustment_id": {
                "type": StringType,
                "display_name": "Adjustment ID",
                "ref_data": "adjustments",
                "short_desc": "The adjustment ID to update",
                "desc": "The unique identifier of the invoice adjustment to update",
                "required": True,
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "The updated adjustment amount",
                "desc": "The new monetary amount for this invoice adjustment",
                "example_value": 500.00,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "The updated description",
                "desc": "The updated explanation for this invoice adjustment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel update invoice adjustment action data provider
public class DeelUpdateAdjustmentDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing invoice adjustment in Deel",
            "type": "DeelUpdateAdjustmentDataProvider",
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing invoice adjustment in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelUpdateAdjustmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelAdjustmentDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string adjustment_id = remove req.adjustment_id;
        return doPatch("invoice-adjustments/" + encode_url(adjustment_id, True), {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
