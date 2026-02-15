# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! The main Wave data provider class (root provider)
public class WaveDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "wave",
            "desc": "Wave data provider for accounting and invoicing",
            "type": "WaveDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "customers": Class::forName("WaveCustomersDataProvider"),
            "invoices": Class::forName("WaveInvoicesDataProvider"),
            "products": Class::forName("WaveProductsDataProvider"),
            "accounts": Class::forName("WaveAccountsDataProvider"),
            "events": Class::forName("WaveEventsDataProvider"),
            "create-customer": Class::forName("WaveCreateCustomerDataProvider"),
            "create-product": Class::forName("WaveCreateProductDataProvider"),
            "create-sales-tax": Class::forName("WaveCreateSalesTaxDataProvider"),
            "create-transaction": Class::forName("WaveCreateTransactionDataProvider"),
            "create-account": Class::forName("WaveCreateAccountDataProvider"),
            "create-income-expense": Class::forName("WaveCreateIncomeExpenseDataProvider"),
            "create-invoice": Class::forName("WaveCreateInvoiceDataProvider"),
            "approve-invoice": Class::forName("WaveApproveInvoiceDataProvider"),
            "send-invoice": Class::forName("WaveSendInvoiceDataProvider"),
            "delete-customer": Class::forName("WaveDeleteCustomerDataProvider"),
            "update-customer": Class::forName("WaveUpdateCustomerDataProvider"),
            "update-product": Class::forName("WaveUpdateProductDataProvider"),
            "execute-graphql": Class::forName("WaveExecuteGraphQLDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WaveDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "wave";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Wave data provider for accounting and invoicing";
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

} # namespace WaveDataProvider
