# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetGroupsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet groups collection data provider
public class SmartSheetGroupsDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "groups",
            "desc": "Smartsheet groups data provider for listing and managing groups",
            "type": "SmartSheetGroupsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
        };

        const ChildMap = {
            "create": Class::forName("SmartSheetCreateGroupDataProvider"),
            "update": Class::forName("SmartSheetUpdateGroupDataProvider"),
            "delete": Class::forName("SmartSheetDeleteGroupDataProvider"),
        };

        const SearchOptions = {
            "groupId": <DataProviderOptionInfo>{
                "display_name": "Group ID",
                "short_desc": "Get a specific group by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The group ID to retrieve. When provided, returns a single group",
                "ref_data": "groups",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "groups"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SmartSheetGroupDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.groupId) {
            hash<auto> result = doGet("groups/" + search_options.groupId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        hash<auto> response = doGet("groups");
        list<hash<auto>> records = response.data ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Create group action
public class SmartSheetCreateGroupDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetCreateGroupRequestDataType RequestType();
        static SmartSheetGroupDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new Smartsheet group",
            "type": "SmartSheetCreateGroupDataProvider",
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
        return doPost("groups", req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Update group action
public class SmartSheetUpdateGroupDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetUpdateGroupRequestDataType RequestType();
        static SmartSheetGroupDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update a Smartsheet group",
            "type": "SmartSheetUpdateGroupDataProvider",
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
        string group_id = string(remove req.groupId);
        return doPut("groups/" + group_id, req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Delete group action
public class SmartSheetDeleteGroupDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetDeleteGroupRequestDataType RequestType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a Smartsheet group",
            "type": "SmartSheetDeleteGroupDataProvider",
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
        return doDelete("groups/" + string(req.groupId));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
