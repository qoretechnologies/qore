# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyOrganisationsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly organisations collection data provider
public class InsightlyOrganisationsDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "organisations",
            "desc": "Insightly organisations data provider for listing and searching organisations",
            "type": "InsightlyOrganisationsDataProvider",
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
            "id": <DataProviderOptionInfo>{
                "display_name": "Organisation ID",
                "short_desc": "Get a specific organisation by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The organisation ID to retrieve",
            },
            "skip": <DataProviderOptionInfo>{
                "display_name": "Skip",
                "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of records to skip for OData pagination",
                "preselected": True,
            },
            "top": <DataProviderOptionInfo>{
                "display_name": "Top",
                "short_desc": "Maximum number of records to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of records to return (max " + MaxPageSize + ")",
                "preselected": True,
            },
            "brief": <DataProviderOptionInfo>{
                "display_name": "Brief",
                "short_desc": "Return brief results",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, returns only a subset of fields for each record",
            },
            "count_total": <DataProviderOptionInfo>{
                "display_name": "Count Total",
                "short_desc": "Include total count in response",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, includes the total count of matching records",
            },
        };

        const ChildMap = {
            "create": Class::forName("InsightlyCreateOrganisationDataProvider"),
            "update": Class::forName("InsightlyUpdateOrganisationDataProvider"),
            "delete": Class::forName("InsightlyDeleteOrganisationDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() {
        return "organisations";
    }

    *string getDesc() {
        return "Insightly organisations data provider for listing and searching organisations";
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
        return new InsightlyOrganisationDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("Organisations/" + string(search_options.id));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.skip) {
            params.skip = search_options.skip;
        }
        if (search_options.top) {
            params.top = search_options.top;
        }
        if (search_options.brief) {
            params.brief = search_options.brief ? "true" : "false";
        }
        if (search_options.count_total) {
            params.count_total = search_options.count_total ? "true" : "false";
        }

        list<hash<auto>> records;
        if (search_options.top || search_options.skip) {
            records = doGetList("Organisations", params);
        } else {
            records = fetchAllPages("Organisations");
        }

        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InsightlyDataProvider
