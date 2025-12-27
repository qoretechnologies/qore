# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcTablesDataProvider class definition

/** BcTablesDataProvider.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! The Business Central tables data provider class
/** Lists available entity tables for a company
*/
public class BcTablesDataProvider inherits CdsRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tables",
            "type": "BcTablesDataProvider",
            "supports_children": True,
            "constructor_options": BaseConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Standard Business Central entities
        const StandardEntities = (
            "accounts",
            "agedAccountsPayable",
            "agedAccountsReceivable",
            "balanceSheet",
            "bankAccounts",
            "cashFlowStatement",
            "companies",
            "companyInformation",
            "countriesRegions",
            "currencies",
            "customerPaymentJournals",
            "customerPayments",
            "customers",
            "defaultDimensions",
            "dimensionLines",
            "dimensions",
            "dimensionValues",
            "employees",
            "generalLedgerEntries",
            "incomeStatement",
            "itemCategories",
            "items",
            "journalLines",
            "journals",
            "paymentMethods",
            "paymentTerms",
            "picture",
            "purchaseInvoiceLines",
            "purchaseInvoices",
            "retainedEarningsStatement",
            "salesCreditMemoLines",
            "salesCreditMemos",
            "salesInvoiceLines",
            "salesInvoices",
            "salesOrderLines",
            "salesOrders",
            "salesQuoteLines",
            "salesQuotes",
            "shipmentMethods",
            "taxAreas",
            "taxGroups",
            "timeRegistrationEntries",
            "unitsOfMeasure",
            "vendors",
        );
    }

    private {
        #! The company ID
        string companyId;

        #! Entity metadata keyed by name
        *hash<string, bool> entities;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::CdsRestClient rest, string companyId) : CdsRestDataProviderBase(rest) {
        self.companyId = companyId;
    }

    #! Returns the data provider name
    string getName() {
        return "tables";
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getEntities();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        # For BC, we use standard entities or try to access the entity directly
        if (getEntities(){name}) {
            return new ODataEntityDataProvider(rest, companyId, name);
        }
    }

    #! Returns available entities
    private hash<string, bool> getEntities() {
        if (entities) {
            return entities;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (entities) {
            return entities;
        }

        # Use standard Business Central entities
        # Note: BC does not expose $metadata at the company level, so we use
        # the predefined list of standard entities
        entities = map {$1: True}, StandardEntities;

        return entities;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
