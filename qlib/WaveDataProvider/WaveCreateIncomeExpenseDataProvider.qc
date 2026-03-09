# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveCreateIncomeExpenseDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Request type for creating an income or expense entry
class WaveIncomeExpenseCreateDataType inherits HashDataType {
    private {
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the income/expense in",
                "required": True,
            },
            "externalId": {
                "type": StringType,
                "display_name": "External ID",
                "short_desc": "The external ID (required)",
                "desc": "A unique external identifier for the transaction (for idempotency)",
                "example_value": "ie-12345",
                "required": True,
            },
            "date": {
                "type": DateOrNothingType,
                "display_name": "Date",
                "short_desc": "The transaction date",
                "desc": "The date of the income or expense entry",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The transaction description",
                "desc": "The description of the income or expense entry",
                "example_value": "Office supplies purchase",
            },
            "lineItems": {
                "type": new ListDataType("TransactionLineItemInputList", new WaveTransactionLineItemInputDataType(), True),
                "display_name": "Line Items",
                "short_desc": "The transaction line items",
                "desc": "The line items for the transaction (accountId, amount, balance type)",
            },
            "anchor": {
                "type": new WaveTransactionAnchorInputDataType(),
                "display_name": "Anchor",
                "short_desc": "The anchor account",
                "desc": "The anchor account for the transaction (accountId, amount, direction)",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The entry type (income or expense)",
                "desc": "The type of the entry: income or expense",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "income",
                        "desc": "Income entry",
                    },
                    <AllowedValueInfo>{
                        "value": "expense",
                        "desc": "Expense entry",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating Wave income/expense entries via request API
public class WaveCreateIncomeExpenseDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-income-expense",
            "desc": "Create an income or expense entry in Wave",
            "type": "WaveCreateIncomeExpenseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the income/expense data to create
        static WaveIncomeExpenseCreateDataType RequestType();

        #! Response type - the created transaction
        static WaveTransactionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-income-expense";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create an income or expense entry in Wave";
    }

    #! Makes a request and returns the response
    /** @param req the request data (income/expense fields to create)
        @param request_options the request options

        @return the created transaction data
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.businessId.val(), "businessId is required");
        @assert(req.externalId.val(), "externalId is required");
        hash<auto> result = doMutation(MoneyTransactionCreateMutation, {"input": req},
            "moneyTransactionCreate", "transaction");
        return result;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
