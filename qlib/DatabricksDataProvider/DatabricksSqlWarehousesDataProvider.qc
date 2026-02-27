# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksSqlWarehousesDataProvider class definition

/*  DatabricksSqlWarehousesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Databricks SQL warehouses collection data provider
public class DatabricksSqlWarehousesDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sql-warehouses",
            "desc": "Databricks SQL warehouses data provider; list and search SQL warehouses",
            "type": "DatabricksSqlWarehousesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Warehouse ID",
                "short_desc": "Get a specific SQL warehouse by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by warehouse ID for single record lookup",
            },
        };
    }

    private {
        #! Record type
        static DatabricksSqlWarehouseDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "sql-warehouses";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks SQL warehouses data provider; list and search SQL warehouses";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for SQL warehouses
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If id provided, get single record
        if (search_options.id) {
            hash<auto> result = doGet("api/2.0/sql/warehouses/" + search_options.id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options, recordType.getFields());
        }

        # List all SQL warehouses (no pagination)
        hash<auto> response = doGet("api/2.0/sql/warehouses");
        list<auto> records = response.warehouses ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
