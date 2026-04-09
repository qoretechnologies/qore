# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetFoldersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Smartsheet folders collection data provider
public class SmartSheetFoldersDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "folders",
            "desc": "Smartsheet folders data provider for listing and managing home folders",
            "type": "SmartSheetFoldersDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
        };

        const ChildMap = {
            "create": Class::forName("SmartSheetCreateFolderDataProvider"),
            "update": Class::forName("SmartSheetUpdateFolderDataProvider"),
            "delete": Class::forName("SmartSheetDeleteFolderDataProvider"),
        };

        const SearchOptions = {
            "folderId": <DataProviderOptionInfo>{
                "display_name": "Folder ID",
                "short_desc": "Get a specific folder by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The folder ID to retrieve. When provided, returns a single folder",
                "ref_data": "folders",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "folders"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SmartSheetFolderDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.folderId) {
            hash<auto> result = doGet("folders/" + search_options.folderId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        hash<auto> response = doGet("home/folders");
        list<hash<auto>> records = response.data ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Create folder action
public class SmartSheetCreateFolderDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetCreateFolderRequestDataType RequestType();
        static SmartSheetFolderDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new folder in the home location",
            "type": "SmartSheetCreateFolderDataProvider",
            "supports_request": True,
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "create"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("home/folders", req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Update folder action
public class SmartSheetUpdateFolderDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetUpdateFolderRequestDataType RequestType();
        static SmartSheetFolderDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update a Smartsheet folder",
            "type": "SmartSheetUpdateFolderDataProvider",
            "supports_request": True,
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "update"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string folder_id = string(remove req.folderId);
        return doPut("folders/" + folder_id, req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Delete folder action
public class SmartSheetDeleteFolderDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetDeleteFolderRequestDataType RequestType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a Smartsheet folder",
            "type": "SmartSheetDeleteFolderDataProvider",
            "supports_request": True,
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "delete"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doDelete("folders/" + string(req.folderId));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
