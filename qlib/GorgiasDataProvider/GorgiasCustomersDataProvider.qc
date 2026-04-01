# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasCustomersDataProvider.qc Gorgias customers collection data provider

/*  GorgiasCustomersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias customers collection data provider
public class GorgiasCustomersDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Gorgias customers data provider for listing and searching customer profiles",
            "type": "GorgiasCustomersDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Get a specific customer by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The customer ID to retrieve. When provided, returns a single record",
                "ref_data": "customers",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter customers by email address",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        const ChildMap = {
            "create": Class::forName("GorgiasCreateCustomerDataProvider"),
            "update": Class::forName("GorgiasUpdateCustomerDataProvider"),
            "delete": Class::forName("GorgiasDeleteCustomerDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "customers";
    }

    *string getDesc() {
        return "Gorgias customers data provider for listing and searching customer profiles";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new GorgiasCustomerDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.customer_id) {
            hash<auto> result = doGet("customers/" + search_options.customer_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.email) {
            params.email = search_options.email;
        }

        list<hash<auto>> all_records = fetchAllPages("customers", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
