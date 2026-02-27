# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksWorkspaceDataProvider class definition

/*  DatabricksWorkspaceDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks workspace collection data provider
public class DatabricksWorkspaceDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "workspace",
            "desc": "Databricks workspace data provider; list workspace objects and get object status",
            "type": "DatabricksWorkspaceDataProvider",
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
            "path": <DataProviderOptionInfo>{
                "display_name": "Path",
                "short_desc": "Workspace path to list or get status of",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The workspace path to list objects from or get status of (required)",
                "required": True,
            },
            "get_status_only": <DataProviderOptionInfo>{
                "display_name": "Get Status Only",
                "short_desc": "Get status of a single path instead of listing",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "If `True`, returns the status of the path instead of listing directory contents",
            },
        };
    }

    private {
        #! Record type
        static DatabricksWorkspaceObjectDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "workspace";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks workspace data provider; list workspace objects and get object status";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for workspace objects
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If get_status_only, return single object status
        if (search_options.get_status_only) {
            hash<auto> result = doGet("api/2.0/workspace/get-status",
                {"path": search_options.path});
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options, recordType.getFields());
        }

        # List workspace objects
        hash<auto> response = doGet("api/2.0/workspace/list", {"path": search_options.path});
        list<auto> records = response.objects ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
