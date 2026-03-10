# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapFilesDataProvider.qc Keap files collection data provider

/*  KeapFilesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap files collection data provider
public class KeapFilesDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "files",
            "desc": "Keap files data provider for listing and searching files",
            "type": "KeapFilesDataProvider",
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
            "file_id": <DataProviderOptionInfo>{
                "display_name": "File ID",
                "short_desc": "Get a specific file by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by file ID for single record lookup",
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter files by associated contact ID",
            },
            "permission": <DataProviderOptionInfo>{
                "display_name": "Permission",
                "short_desc": "Filter by permission level",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter files by permission level",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "USER", "desc": "Files owned by the user"},
                    <AllowedValueInfo>{"value": "COMPANY", "desc": "Files owned by the company"},
                    <AllowedValueInfo>{"value": "BOTH", "desc": "Both user and company files"},
                ),
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
            "upload": Class::forName("KeapUploadFileDataProvider"),
            "get": Class::forName("KeapGetFileDataProvider"),
            "delete": Class::forName("KeapDeleteFileDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "files";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap files data provider for listing and searching files";
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
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new KeapFileDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If file_id provided, get single record
        if (search_options.file_id) {
            hash<auto> result = doGet("files/" + search_options.file_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.contact_id) {
            params.contact_id = search_options.contact_id;
        }
        if (search_options.permission) {
            params.permission = search_options.permission;
        }

        list<hash<auto>> all_records = fetchAllPages("files", "files", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
