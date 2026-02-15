# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Root data provider for Cin7Core
/** This is the main entry point for the Cin7Core data provider, providing access to
    customers, suppliers, products, sales, inventory, and other Cin7Core functionality.
*/
public class Cin7CoreDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "cin7core",
            "desc": "Cin7Core data provider for inventory management, sales, and order processing",
            "type": "Cin7CoreDataProvider",
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
            "customers": Class::forName("Cin7CoreCustomersDataProvider"),
            "suppliers": Class::forName("Cin7CoreSuppliersDataProvider"),
            "products": Class::forName("Cin7CoreProductsDataProvider"),
            "sales": Class::forName("Cin7CoreSalesDataProvider"),
            "sale-payments": Class::forName("Cin7CoreSalePaymentsDataProvider"),
            "sale-attachments": Class::forName("Cin7CoreSaleAttachmentsDataProvider"),
            "stock-adjustments": Class::forName("Cin7CoreStockAdjustmentsDataProvider"),

            # Customer action providers
            "create-customer": Class::forName("Cin7CoreCreateCustomerDataProvider"),
            "get-customer": Class::forName("Cin7CoreGetCustomerDataProvider"),
            "update-customer": Class::forName("Cin7CoreUpdateCustomerDataProvider"),

            # Supplier action providers
            "create-supplier": Class::forName("Cin7CoreCreateSupplierDataProvider"),
            "get-supplier": Class::forName("Cin7CoreGetSupplierDataProvider"),
            "update-supplier": Class::forName("Cin7CoreUpdateSupplierDataProvider"),

            # Product action providers
            "get-product": Class::forName("Cin7CoreGetProductDataProvider"),
            "find-product-availability": Class::forName("Cin7CoreFindProductAvailabilityDataProvider"),

            # Sale action providers
            "create-sale": Class::forName("Cin7CoreCreateSaleDataProvider"),
            "get-sale": Class::forName("Cin7CoreGetSaleDataProvider"),
            "update-sale": Class::forName("Cin7CoreUpdateSaleDataProvider"),
            "delete-sale": Class::forName("Cin7CoreDeleteSaleDataProvider"),

            # Sale attachment providers
            "add-sale-attachment": Class::forName("Cin7CoreAddSaleAttachmentDataProvider"),
            "delete-sale-attachment": Class::forName("Cin7CoreDeleteSaleAttachmentDataProvider"),

            # Sale pack/shipment providers
            "add-sale-pack": Class::forName("Cin7CoreAddSalePackDataProvider"),
            "add-sale-shipment": Class::forName("Cin7CoreAddSaleShipmentDataProvider"),
            "get-sale-pack": Class::forName("Cin7CoreGetSalePackDataProvider"),

            # Sale payment providers
            "add-sale-payment": Class::forName("Cin7CoreAddSalePaymentDataProvider"),
            "update-sale-payment": Class::forName("Cin7CoreUpdateSalePaymentDataProvider"),
            "delete-sale-payment": Class::forName("Cin7CoreDeleteSalePaymentDataProvider"),

            # Sale lookup providers
            "find-sale-credit-note": Class::forName("Cin7CoreFindSaleCreditNoteDataProvider"),
            "find-sale-fulfilment": Class::forName("Cin7CoreFindSaleFulfilmentDataProvider"),
            "find-sale-invoice": Class::forName("Cin7CoreFindSaleInvoiceDataProvider"),

            # Stock adjustment providers
            "create-stock-adjustment": Class::forName("Cin7CoreCreateStockAdjustmentDataProvider"),
            "get-stock-adjustment": Class::forName("Cin7CoreGetStockAdjustmentDataProvider"),
            "update-stock-adjustment": Class::forName("Cin7CoreUpdateStockAdjustmentDataProvider"),
            "delete-stock-adjustment": Class::forName("Cin7CoreDeleteStockAdjustmentDataProvider"),

            # Assembly providers
            "find-assembly": Class::forName("Cin7CoreFindAssemblyDataProvider"),

            # Utility providers
            "make-api-call": Class::forName("Cin7CoreMakeApiCallDataProvider"),

            # Events container
            "events": Class::forName("Cin7CoreEventsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "cin7core";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Cin7Core data provider for inventory management, sales, and order processing";
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

} # namespace Cin7CoreDataProvider
