# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelCreateAdjustmentDataProvider.qc Deel create invoice adjustment action data provider

/*  DeelCreateAdjustmentDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an invoice adjustment
public class DeelCreateAdjustmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contract_id": {
                "type": StringType,
                "display_name": "Contract ID",
                "short_desc": "The associated contract ID",
                "desc": "The unique identifier of the contract this adjustment belongs to",
                "required": True,
            },
            "amount": {
                "type": NumberType,
                "display_name": "Amount",
                "short_desc": "The adjustment amount",
                "desc": "The monetary amount for this invoice adjustment",
                "required": True,
                "example_value": 500.00,
            },
            "currency": {
                "type": StringType,
                "display_name": "Currency",
                "short_desc": "The adjustment currency",
                "desc": "The ISO 4217 currency code for the adjustment (e.g., `USD`, `EUR`)",
                "required": True,
                "example_value": "USD",
            },
            "date_submitted": {
                "type": StringType,
                "display_name": "Date Submitted",
                "short_desc": "The adjustment date",
                "desc": "The date for this adjustment entry (YYYY-MM-DD format)",
                "required": True,
                "example_value": "2026-03-01",
            },
            "type": {
                "type": StringType,
                "display_name": "Adjustment Type",
                "short_desc": "The type of adjustment",
                "desc": "The type of invoice adjustment (e.g., `bonus`, `reimbursement`, "
                    "`deduction`)",
                "required": True,
                "example_value": "bonus",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "bonus", "desc": "Bonus payment"},
                    <AllowedValueInfo>{"value": "reimbursement", "desc": "Expense reimbursement"},
                    <AllowedValueInfo>{"value": "deduction", "desc": "Amount deduction"},
                ),
            },
            "description": {
                "type": StringType,
                "display_name": "Description",
                "short_desc": "The reason for the adjustment",
                "desc": "An explanation for why this invoice adjustment is being created",
                "required": True,
                "example_value": "Bonus for exceptional performance",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel create invoice adjustment action data provider
public class DeelCreateAdjustmentDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new invoice adjustment in Deel",
            "type": "DeelCreateAdjustmentDataProvider",
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
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new invoice adjustment in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelCreateAdjustmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelAdjustmentDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("invoice-adjustments", {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
