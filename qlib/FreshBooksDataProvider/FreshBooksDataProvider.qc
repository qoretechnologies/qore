# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The main FreshBooks data provider class (root provider)
public class FreshBooksDataProvider inherits FreshBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "freshbooks",
            "desc": "FreshBooks data provider for accounting, invoicing, and time tracking",
            "type": "FreshBooksDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection providers
            "clients": Class::forName("FreshBooksClientsDataProvider"),
            "estimates": Class::forName("FreshBooksEstimatesDataProvider"),
            "expenses": Class::forName("FreshBooksExpensesDataProvider"),
            "invoices": Class::forName("FreshBooksInvoicesDataProvider"),
            "items": Class::forName("FreshBooksItemsDataProvider"),
            "payments": Class::forName("FreshBooksPaymentsDataProvider"),
            "services": Class::forName("FreshBooksServicesDataProvider"),
            # Action providers
            "create-client": Class::forName("FreshBooksCreateClientDataProvider"),
            "create-estimate": Class::forName("FreshBooksCreateEstimateDataProvider"),
            "create-expense": Class::forName("FreshBooksCreateExpenseDataProvider"),
            "create-invoice": Class::forName("FreshBooksCreateInvoiceDataProvider"),
            "create-item": Class::forName("FreshBooksCreateItemDataProvider"),
            "create-payment": Class::forName("FreshBooksCreatePaymentDataProvider"),
            "create-service": Class::forName("FreshBooksCreateServiceDataProvider"),
            "update-client": Class::forName("FreshBooksUpdateClientDataProvider"),
            "update-estimate": Class::forName("FreshBooksUpdateEstimateDataProvider"),
            "update-expense": Class::forName("FreshBooksUpdateExpenseDataProvider"),
            "update-invoice": Class::forName("FreshBooksUpdateInvoiceDataProvider"),
            "update-item": Class::forName("FreshBooksUpdateItemDataProvider"),
            "update-payment": Class::forName("FreshBooksUpdatePaymentDataProvider"),
            "update-service": Class::forName("FreshBooksUpdateServiceDataProvider"),
            "delete-client": Class::forName("FreshBooksDeleteClientDataProvider"),
            "delete-estimate": Class::forName("FreshBooksDeleteEstimateDataProvider"),
            "delete-expense": Class::forName("FreshBooksDeleteExpenseDataProvider"),
            "delete-invoice": Class::forName("FreshBooksDeleteInvoiceDataProvider"),
            "delete-item": Class::forName("FreshBooksDeleteItemDataProvider"),
            "delete-payment": Class::forName("FreshBooksDeletePaymentDataProvider"),
            "delete-service": Class::forName("FreshBooksDeleteServiceDataProvider"),
            "execute-api-call": Class::forName("FreshBooksExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("FreshBooksEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : FreshBooksDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest) : FreshBooksDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(FreshBooksRestClient::FreshBooksRestConnection conn) : FreshBooksDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "freshbooks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "FreshBooks data provider for accounting, invoicing, and time tracking";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshBooksDataProvider
