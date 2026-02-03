# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The main Zoho Invoice data provider class (root provider)
public class ZohoInvoiceDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "zohoinvoice",
            "desc": "Zoho Invoice data provider for invoicing and billing",
            "type": "ZohoInvoiceDataProvider",
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
            # Collection providers (list/search)
            "invoices": Class::forName("ZohoInvoiceInvoicesDataProvider"),
            "recurring-invoices": Class::forName("ZohoInvoiceRecurringInvoicesDataProvider"),
            "contacts": Class::forName("ZohoInvoiceContactsDataProvider"),
            "expenses": Class::forName("ZohoInvoiceExpensesDataProvider"),
            "recurring-expenses": Class::forName("ZohoInvoiceRecurringExpensesDataProvider"),
            "payments": Class::forName("ZohoInvoicePaymentsDataProvider"),
            "estimates": Class::forName("ZohoInvoiceEstimatesDataProvider"),
            "projects": Class::forName("ZohoInvoiceProjectsDataProvider"),
            "items": Class::forName("ZohoInvoiceItemsDataProvider"),
            "events": Class::forName("ZohoInvoiceEventsDataProvider"),

            # Invoice action providers
            "create-invoice": Class::forName("ZohoInvoiceCreateInvoiceDataProvider"),
            "get-invoice": Class::forName("ZohoInvoiceGetInvoiceDataProvider"),
            "update-invoice": Class::forName("ZohoInvoiceUpdateInvoiceDataProvider"),
            "delete-invoice": Class::forName("ZohoInvoiceDeleteInvoiceDataProvider"),
            "email-invoice": Class::forName("ZohoInvoiceEmailInvoiceDataProvider"),
            "mark-invoice": Class::forName("ZohoInvoiceMarkInvoiceDataProvider"),

            # Recurring invoice action providers
            "create-recurring-invoice": Class::forName("ZohoInvoiceCreateRecurringInvoiceDataProvider"),
            "get-recurring-invoice": Class::forName("ZohoInvoiceGetRecurringInvoiceDataProvider"),
            "update-recurring-invoice": Class::forName("ZohoInvoiceUpdateRecurringInvoiceDataProvider"),
            "mark-recurring-invoice": Class::forName("ZohoInvoiceMarkRecurringInvoiceDataProvider"),

            # Contact action providers
            "create-contact": Class::forName("ZohoInvoiceCreateContactDataProvider"),
            "get-contact": Class::forName("ZohoInvoiceGetContactDataProvider"),
            "update-contact": Class::forName("ZohoInvoiceUpdateContactDataProvider"),
            "delete-contact": Class::forName("ZohoInvoiceDeleteContactDataProvider"),
            "mark-contact": Class::forName("ZohoInvoiceMarkContactDataProvider"),

            # Expense action providers
            "create-expense": Class::forName("ZohoInvoiceCreateExpenseDataProvider"),
            "get-expense": Class::forName("ZohoInvoiceGetExpenseDataProvider"),
            "update-expense": Class::forName("ZohoInvoiceUpdateExpenseDataProvider"),
            "delete-expense": Class::forName("ZohoInvoiceDeleteExpenseDataProvider"),

            # Recurring expense action providers
            "create-recurring-expense": Class::forName("ZohoInvoiceCreateRecurringExpenseDataProvider"),
            "get-recurring-expense": Class::forName("ZohoInvoiceGetRecurringExpenseDataProvider"),
            "update-recurring-expense": Class::forName("ZohoInvoiceUpdateRecurringExpenseDataProvider"),
            "delete-recurring-expense": Class::forName("ZohoInvoiceDeleteRecurringExpenseDataProvider"),

            # Payment action providers
            "create-payment": Class::forName("ZohoInvoiceCreatePaymentDataProvider"),
            "get-payment": Class::forName("ZohoInvoiceGetPaymentDataProvider"),
            "update-payment": Class::forName("ZohoInvoiceUpdatePaymentDataProvider"),
            "delete-payment": Class::forName("ZohoInvoiceDeletePaymentDataProvider"),
            "refund-payment": Class::forName("ZohoInvoiceRefundPaymentDataProvider"),

            # Estimate action providers
            "create-estimate": Class::forName("ZohoInvoiceCreateEstimateDataProvider"),
            "get-estimate": Class::forName("ZohoInvoiceGetEstimateDataProvider"),
            "update-estimate": Class::forName("ZohoInvoiceUpdateEstimateDataProvider"),
            "delete-estimate": Class::forName("ZohoInvoiceDeleteEstimateDataProvider"),
            "mark-estimate": Class::forName("ZohoInvoiceMarkEstimateDataProvider"),

            # Project action providers
            "create-project": Class::forName("ZohoInvoiceCreateProjectDataProvider"),
            "get-project": Class::forName("ZohoInvoiceGetProjectDataProvider"),
            "update-project": Class::forName("ZohoInvoiceUpdateProjectDataProvider"),
            "mark-project": Class::forName("ZohoInvoiceMarkProjectDataProvider"),

            # Item action providers
            "create-item": Class::forName("ZohoInvoiceCreateItemDataProvider"),
            "get-item": Class::forName("ZohoInvoiceGetItemDataProvider"),
            "update-item": Class::forName("ZohoInvoiceUpdateItemDataProvider"),
            "delete-item": Class::forName("ZohoInvoiceDeleteItemDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ZohoInvoiceDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestConnection conn) : ZohoInvoiceDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "zohoinvoice";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Invoice data provider for invoicing and billing";
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

} # namespace ZohoInvoiceDataProvider
