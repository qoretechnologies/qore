# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshBooksDeleteExpenseDataProvider.qc FreshBooks delete expense action data provider

/*  FreshBooksDeleteExpenseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks delete expense action data provider (soft delete via vis_state)
public class FreshBooksDeleteExpenseDataProvider inherits FreshBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-expense",
            "desc": "Delete (soft) an expense in FreshBooks by setting vis_state to 1",
            "type": "FreshBooksDeleteExpenseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - requires expense ID
        const RequestType = new HashDataType("FreshBooksDeleteExpenseRequest",
            FreshBooksDataProviderBase::AccountIdField + {
            "id": {
                "display_name": "Expense ID",
                "type": IntType,
                "desc": "The ID of the expense to delete",
                "required": True,
            },
        });

        #! Response type - returns the updated expense
        const ResponseType = AbstractDataProviderTypeMap."hash";
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest) : FreshBooksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-expense";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete (soft) an expense in FreshBooks by setting vis_state to 1";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FreshBooksExpenseDataType();
    }

    #! Makes a request and returns the response
    /** Performs a soft delete by setting vis_state to 1 (deleted)
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setAccountIdFromReq(req);
        if (!req.id) {
            throw "FRESHBOOKS-ERROR", "id is required for delete operation";
        }
        return doPut(accountPath(sprintf("/expenses/expenses/%d", req.id)),
            {"expense": {"vis_state": 1}}).expense;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshBooksDataProvider
