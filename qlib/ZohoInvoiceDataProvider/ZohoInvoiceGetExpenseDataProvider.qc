# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceGetExpenseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice get expense data provider
public class ZohoInvoiceGetExpenseDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-expense",
            "desc": "Gets details of a specific expense from Zoho Invoice",
            "type": "ZohoInvoiceGetExpenseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "expense_id": <DataProviderOptionInfo>{
                "display_name": "Expense ID",
                "short_desc": "The expense to retrieve",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the expense to retrieve",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("GetExpenseRequest", {
            "expense_id": {
                "display_name": "Expense ID",
                "type": SoftStringType,
                "desc": "The ID of the expense to retrieve",
            },
        });
    }

    private {
        #! The expense ID
        string expense_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest, *hash<auto> options) : ZohoInvoiceDataProviderBase(rest) {
        if (options.expense_id) {
            expense_id = options.expense_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "get-expense";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gets details of a specific expense from Zoho Invoice";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInvoiceExpenseDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.expense_id ?? request_options.expense_id ?? expense_id;
        if (!id) {
            throw "ZOHOINVOICE-ERROR", "expense_id is required for get operation";
        }
        hash<auto> response = doGet(sprintf("/expenses/%s", id));
        return response.expense;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
