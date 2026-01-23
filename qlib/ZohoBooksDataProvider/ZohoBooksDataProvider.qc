# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! The main Zoho Books data provider class (root provider)
public class ZohoBooksDataProvider inherits ZohoBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "zohobooks",
            "desc": "Zoho Books data provider for accounting and invoicing",
            "type": "ZohoBooksDataProvider",
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
            "invoices": Class::forName("ZohoBooksInvoicesDataProvider"),
            "bills": Class::forName("ZohoBooksBillsDataProvider"),
            "payments": Class::forName("ZohoBooksPaymentsDataProvider"),
            "contacts": Class::forName("ZohoBooksContactsDataProvider"),
            "expenses": Class::forName("ZohoBooksExpensesDataProvider"),
            "items": Class::forName("ZohoBooksItemsDataProvider"),
            "estimates": Class::forName("ZohoBooksEstimatesDataProvider"),
            "salesorders": Class::forName("ZohoBooksSalesOrdersDataProvider"),
            "purchaseorders": Class::forName("ZohoBooksPurchaseOrdersDataProvider"),
            "events": Class::forName("ZohoBooksEventsDataProvider"),
            # CRUD action providers
            "create-invoice": Class::forName("ZohoBooksCreateInvoiceDataProvider"),
            "update-invoice": Class::forName("ZohoBooksUpdateInvoiceDataProvider"),
            "delete-invoice": Class::forName("ZohoBooksDeleteInvoiceDataProvider"),
            "get-invoice": Class::forName("ZohoBooksGetInvoiceDataProvider"),
            "email-invoice": Class::forName("ZohoBooksEmailInvoiceDataProvider"),
            "create-bill": Class::forName("ZohoBooksCreateBillDataProvider"),
            "update-bill": Class::forName("ZohoBooksUpdateBillDataProvider"),
            "delete-bill": Class::forName("ZohoBooksDeleteBillDataProvider"),
            "get-bill": Class::forName("ZohoBooksGetBillDataProvider"),
            "create-payment": Class::forName("ZohoBooksCreatePaymentDataProvider"),
            "update-payment": Class::forName("ZohoBooksUpdatePaymentDataProvider"),
            "delete-payment": Class::forName("ZohoBooksDeletePaymentDataProvider"),
            "get-payment": Class::forName("ZohoBooksGetPaymentDataProvider"),
            "create-contact": Class::forName("ZohoBooksCreateContactDataProvider"),
            "update-contact": Class::forName("ZohoBooksUpdateContactDataProvider"),
            "create-expense": Class::forName("ZohoBooksCreateExpenseDataProvider"),
            "update-expense": Class::forName("ZohoBooksUpdateExpenseDataProvider"),
            "create-item": Class::forName("ZohoBooksCreateItemDataProvider"),
            "create-estimate": Class::forName("ZohoBooksCreateEstimateDataProvider"),
            "update-estimate": Class::forName("ZohoBooksUpdateEstimateDataProvider"),
            "create-salesorder": Class::forName("ZohoBooksCreateSalesOrderDataProvider"),
            "update-salesorder": Class::forName("ZohoBooksUpdateSalesOrderDataProvider"),
            "delete-salesorder": Class::forName("ZohoBooksDeleteSalesOrderDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ZohoBooksDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ZohoBooksRestClient::ZohoBooksRestConnection conn) : ZohoBooksDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "zohobooks";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Books data provider for accounting and invoicing";
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

} # namespace ZohoBooksDataProvider
