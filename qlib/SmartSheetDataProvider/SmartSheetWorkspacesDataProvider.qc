# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWorkspacesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet workspaces collection data provider
public class SmartSheetWorkspacesDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "workspaces",
            "desc": "Smartsheet workspaces data provider for listing and managing workspaces",
            "type": "SmartSheetWorkspacesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
        };

        const ChildMap = {
            "create": Class::forName("SmartSheetCreateWorkspaceDataProvider"),
            "update": Class::forName("SmartSheetUpdateWorkspaceDataProvider"),
            "delete": Class::forName("SmartSheetDeleteWorkspaceDataProvider"),
        };

        const SearchOptions = {
            "workspaceId": <DataProviderOptionInfo>{
                "display_name": "Workspace ID",
                "short_desc": "Get a specific workspace by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The workspace ID to retrieve. When provided, returns a single workspace",
                "ref_data": "workspaces",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() {
        return "workspaces";
    }

    *string getDesc() {
        return ProviderInfo.desc;
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
        return new SmartSheetWorkspaceDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.workspaceId) {
            hash<auto> result = doGet("workspaces/" + search_options.workspaceId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        list<hash<auto>> records = fetchAllPages("workspaces", "data");
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Create workspace action
public class SmartSheetCreateWorkspaceDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetCreateWorkspaceRequestDataType RequestType();
        static SmartSheetWorkspaceDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new Smartsheet workspace",
            "type": "SmartSheetCreateWorkspaceDataProvider",
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
        return doPost("workspaces", req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Update workspace action
public class SmartSheetUpdateWorkspaceDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetUpdateWorkspaceRequestDataType RequestType();
        static SmartSheetWorkspaceDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update a Smartsheet workspace",
            "type": "SmartSheetUpdateWorkspaceDataProvider",
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
        string ws_id = string(remove req.workspaceId);
        return doPut("workspaces/" + ws_id, req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Delete workspace action
public class SmartSheetDeleteWorkspaceDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetDeleteWorkspaceRequestDataType RequestType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a Smartsheet workspace",
            "type": "SmartSheetDeleteWorkspaceDataProvider",
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
        return doDelete("workspaces/" + string(req.workspaceId));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
