# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MewsRestDataProvider module definition

/** MewsRestTablesDataProvider.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MewsRestDataProvider module
public namespace MewsRestDataProvider {
#! The MewsRest data provider class
public class MewsRestTablesDataProvider inherits MewsRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MewsRestTablesDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_records": True,
        };

        #! Constructor options
        const ConstructorOptions = MewsRestDataProvider::ConstructorOptions;
    }

    private {
        const ChildMap = {
            "accountnotes": Class::forName("MewsRestDataProvider::MewsRestAccountNotesTableDataProvider"),
            "accountingcategories":
                Class::forName("MewsRestDataProvider::MewsRestAccountingCategoriesTableDataProvider"),
            "addresses": Class::forName("MewsRestDataProvider::MewsRestAddressesTableDataProvider"),
            "agecategories": Class::forName("MewsRestDataProvider::MewsRestAgeCategoriesTableDataProvider"),
            "availabilityadjustments":
                Class::forName("MewsRestDataProvider::MewsRestAvailabilityAdjustmentsTableDataProvider"),
            "availabilityblocks":
                Class::forName("MewsRestDataProvider::MewsRestAvailabilityBlocksTableDataProvider"),
            "bills": Class::forName("MewsRestDataProvider::MewsRestBillsTableDataProvider"),
            "businesssegments":
                Class::forName("MewsRestDataProvider::MewsRestBusinessSegmentsTableDataProvider"),
            "cancellationpolicies":
                Class::forName("MewsRestDataProvider::MewsRestCancellationPoliciesTableDataProvider"),
            "companies":
                Class::forName("MewsRestDataProvider::MewsRestCompaniesTableDataProvider"),
            "countries":
                Class::forName("MewsRestDataProvider::MewsRestCountriesTableDataProvider"),
            "customers":
                Class::forName("MewsRestDataProvider::MewsRestCustomersTableDataProvider"),
            "enterprises":
                Class::forName("MewsRestDataProvider::MewsRestEnterprisesTableDataProvider"),
            "exchangerates":
                Class::forName("MewsRestDataProvider::MewsRestExchangeRatesTableDataProvider"),
            "languages":
                Class::forName("MewsRestDataProvider::MewsRestLanguagesTableDataProvider"),
            "orderitems": Class::forName("MewsRestDataProvider::MewsRestOrderItemsTableDataProvider"),
            "outletitems": Class::forName("MewsRestDataProvider::MewsRestOutletItemsTableDataProvider"),
            "outlets": Class::forName("MewsRestDataProvider::MewsRestOutletsTableDataProvider"),
            "payments": Class::forName("MewsRestDataProvider::MewsRestPaymentsTableDataProvider"),
            "preauthorizations": Class::forName("MewsRestDataProvider::MewsRestPreauthorizationsTableDataProvider"),
            "products": Class::forName("MewsRestDataProvider::MewsRestProductsTableDataProvider"),
            "rates": Class::forName("MewsRestDataProvider::MewsRestRatesTableDataProvider"),
            "reservations": Class::forName("MewsRestDataProvider::MewsRestReservationsTableDataProvider"),
            "resources": Class::forName("MewsRestDataProvider::MewsRestResourcesTableDataProvider"),
            "resourceblocks": Class::forName("MewsRestDataProvider::MewsRestResourceBlocksTableDataProvider"),
            "resourcefeatures": Class::forName("MewsRestDataProvider::MewsRestResourceFeaturesTableDataProvider"),
            "resourcecategories": Class::forName("MewsRestDataProvider::MewsRestResourceCategoriesTableDataProvider"),
            "restrictions": Class::forName("MewsRestDataProvider::MewsRestRestrictionsTableDataProvider"),
            "services": Class::forName("MewsRestDataProvider::MewsRestServicesTableDataProvider"),
            "taxations": Class::forName("MewsRestDataProvider::MewsRestTaxationsTableDataProvider"),
            "taxenvironments": Class::forName("MewsRestDataProvider::MewsRestTaxEnvironmentsTableDataProvider"),
            "vouchers": Class::forName("MewsRestDataProvider::MewsRestVouchersTableDataProvider"),
            "vouchercodes": Class::forName("MewsRestDataProvider::MewsRestVoucherCodesTableDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = MewsRestDataProvider::getRestConnection(copts);
    }

    #! Creates the object from the REST client
    constructor(AbstractRestClient mews) : MewsRestDataProviderBase(mews) {
    }

    #! Returns the data provider name
    string getName() {
        return "tables";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Mews REST tables data provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        AbstractDataProvider rv = cls.newObject(rest);
        if (logger) {
            rv.setLogger(logger);
        }
        return rv;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
