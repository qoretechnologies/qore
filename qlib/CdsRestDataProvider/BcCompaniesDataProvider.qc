# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcCompaniesDataProvider class definition

/** BcCompaniesDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The Business Central companies data provider class
/** Lists available companies and provides access to company-scoped data
*/
public class BcCompaniesDataProvider inherits BcRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "companies",
            "type": "BcCompaniesDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": False,
            "children_can_support_records": True,
            "children_can_support_observers": False,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Companies list keyed by ID
        *hash<string, hash<auto>> companies;

        #! Companies list keyed by name
        *hash<string, hash<auto>> companies_by_name;

        #! Metadata mutex
        Qore::Thread::Mutex lck();
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::BusinessCentralRestClient rest) : BcRestDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        bool auto_config = DataProvider::getAutoConfig();
        if (auto_config) {
            map options{$1.key} = ENV{$1.value}, EnvOptions.pairIterator(),
                exists ENV{$1.value} && !exists options{$1.key};
        }
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = new CdsRestClient::BusinessCentralRestClient(getClientOptions(copts));
    }

    #! Returns the data provider name
    string getName() {
        return "companies";
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys getCompanies();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        # Get companies list
        *hash<string, hash<auto>> company_list = getCompanies();

        # Try to find by ID first
        *hash<auto> company = company_list{name};
        if (!company) {
            # Try to find by name
            company = companies_by_name{name};
        }

        if (company) {
            return new BcCompanyDataProvider(rest, company.id, company.displayName ?? company.name);
        }
    }

    #! Returns companies from the API
    private *hash<string, hash<auto>> getCompanies() {
        if (companies) {
            return companies;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (companies) {
            return companies;
        }

        hash<auto> info;
        try {
            hash<auto> resp = rest.get("companies", NOTHING, \info);
            list<hash<auto>> company_list = resp.body.value ?? ();
            companies = map {$1.id: $1}, company_list;
            companies_by_name = map {($1.displayName ?? $1.name): $1}, company_list;
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            rethrow ex.err, ex.desc, ex.arg + ex_arg;
        }

        return companies;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
