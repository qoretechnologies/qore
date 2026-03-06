# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelDeleteAdjustmentDataProvider.qc Deel delete invoice adjustment action data provider

/*  DeelDeleteAdjustmentDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting an invoice adjustment
public class DeelDeleteAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "adjustment_id": {
                "type": StringType,
                "display_name": "Adjustment ID",
                "ref_data": "adjustments",
                "short_desc": "The adjustment ID to delete",
                "desc": "The unique identifier of the invoice adjustment to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for delete operations — Deel returns {"deleted": true}
public class DeelDeletedResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": SoftBoolOrNothingType,
                "display_name": "Deleted",
                "short_desc": "Whether the record was deleted",
                "desc": "`True` if the record was successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel delete invoice adjustment action data provider
public class DeelDeleteAdjustmentDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete an invoice adjustment in Deel",
            "type": "DeelDeleteAdjustmentDataProvider",
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
        return "delete";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete an invoice adjustment in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelDeleteAdjustmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelDeletedResponseDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string adjustment_id = req.adjustment_id;
        return doDelete("invoice-adjustments/" + encode_url(adjustment_id, True)).data ?? {};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
