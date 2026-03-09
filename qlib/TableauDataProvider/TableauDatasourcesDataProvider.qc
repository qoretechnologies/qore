# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDatasourcesDataProvider.qc Tableau datasources collection data provider

/*  TableauDatasourcesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau datasources collection data provider
public class TableauDatasourcesDataProvider inherits TableauDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "datasources",
            "desc": "Tableau datasources data provider for listing and searching datasources",
            "type": "TableauDatasourcesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "datasource_id": <DataProviderOptionInfo>{
                "display_name": "Datasource ID",
                "short_desc": "Get a specific datasource by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by datasource ID for single record lookup",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter datasources by name",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "update": Class::forName("TableauUpdateDatasourceDataProvider"),
            "delete": Class::forName("TableauDeleteDatasourceDataProvider"),
            "download": Class::forName("TableauDownloadDatasourceDataProvider"),
            "add-tag": Class::forName("TableauAddTagToDatasourceDataProvider"),
            "delete-tag": Class::forName("TableauDeleteTagFromDatasourceDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "datasources";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Tableau datasources data provider for listing and searching datasources";
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

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new TableauDatasourceDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If datasource_id provided, get single record
        if (search_options.datasource_id) {
            hash<auto> result = doGet("datasources/" + search_options.datasource_id);
            return new DefaultRecordIterator(new ListHashIterator((result.datasource,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.name) {
            params.filter = "name:eq:" + search_options.name;
        }

        list<hash<auto>> all_records = fetchAllPages("datasources", "datasources.datasource",
            params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace TableauDataProvider
