# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Root data provider for Unleashed
/** This is the main entry point for the Unleashed data provider, providing access to
    customers, products, sales orders, credit notes, and other Unleashed functionality.
*/
public class UnleashedDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "unleashed",
            "desc": "Unleashed data provider for inventory management, sales orders, and customer management",
            "type": "UnleashedDataProvider",
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
            # Collection/Search providers (DPAT_FIND)
            "customers": Class::forName("UnleashedCustomersDataProvider"),
            "products": Class::forName("UnleashedProductsDataProvider"),
            "sales-orders": Class::forName("UnleashedSalesOrdersDataProvider"),
            "credit-notes": Class::forName("UnleashedCreditNotesDataProvider"),

            # Customer action providers
            "create-customer": Class::forName("UnleashedCreateCustomerDataProvider"),
            "get-customer": Class::forName("UnleashedGetCustomerDataProvider"),
            "update-customer": Class::forName("UnleashedUpdateCustomerDataProvider"),

            # Product action providers
            "create-product": Class::forName("UnleashedCreateProductDataProvider"),
            "get-product": Class::forName("UnleashedGetProductDataProvider"),
            "update-product": Class::forName("UnleashedUpdateProductDataProvider"),
            "obsolete-product": Class::forName("UnleashedObsoleteProductDataProvider"),

            # Sales order action providers
            "create-sales-order": Class::forName("UnleashedCreateSalesOrderDataProvider"),
            "get-sales-order": Class::forName("UnleashedGetSalesOrderDataProvider"),
            "update-sales-order": Class::forName("UnleashedUpdateSalesOrderDataProvider"),
            "delete-sales-order": Class::forName("UnleashedDeleteSalesOrderDataProvider"),
            "complete-sales-order": Class::forName("UnleashedCompleteSalesOrderDataProvider"),

            # Credit note action providers
            "create-credit-note": Class::forName("UnleashedCreateCreditNoteDataProvider"),
            "get-credit-note": Class::forName("UnleashedGetCreditNoteDataProvider"),
            "update-credit-note": Class::forName("UnleashedUpdateCreditNoteDataProvider"),
            "delete-credit-note": Class::forName("UnleashedDeleteCreditNoteDataProvider"),
            "complete-credit-note": Class::forName("UnleashedCompleteCreditNoteDataProvider"),

            # Credit note line action providers
            "create-credit-note-line": Class::forName("UnleashedCreateCreditNoteLineDataProvider"),
            "update-credit-note-line": Class::forName("UnleashedUpdateCreditNoteLineDataProvider"),
            "delete-credit-note-line": Class::forName("UnleashedDeleteCreditNoteLineDataProvider"),

            # Utility providers
            "make-api-call": Class::forName("UnleashedMakeApiCallDataProvider"),

            # Events container
            "events": Class::forName("UnleashedEventsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "unleashed";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Unleashed data provider for inventory management, sales orders, and customer management";
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map cast<hash<DataProviderSummaryInfo>>({
            "name": $1,
            "desc": ChildMap{$1}.getConstant("ProviderInfo").getValue().desc,
            "type": ChildMap{$1}.getConstant("ProviderInfo").getValue().type,
            "supports_children": ChildMap{$1}.getConstant("ProviderInfo").getValue().supports_children ?? False,
            "children_can_support_apis": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_apis ?? False,
            "children_can_support_records": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_records ?? False,
            "children_can_support_observers": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_observers ?? False,
        }), keys ChildMap;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnleashedDataProvider
