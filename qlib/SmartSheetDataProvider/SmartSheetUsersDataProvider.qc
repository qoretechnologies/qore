# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetUsersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet users collection data provider
public class SmartSheetUsersDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "users",
            "desc": "Smartsheet users data provider for listing and managing users",
            "type": "SmartSheetUsersDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
        };

        const ChildMap = {
            "create": Class::forName("SmartSheetCreateUserDataProvider"),
            "update": Class::forName("SmartSheetUpdateUserDataProvider"),
            "delete": Class::forName("SmartSheetDeleteUserDataProvider"),
        };

        const SearchOptions = {
            "userId": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Get a specific user by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The user ID to retrieve. When provided, returns a single user",
                "ref_data": "users",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter users by email address",
            },
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "users"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SmartSheetUserDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.userId) {
            hash<auto> result = doGet("users/" + search_options.userId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.email) {
            params.email = search_options.email;
        }
        list<hash<auto>> records = fetchAllPages("users", "data", params);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Create user action
public class SmartSheetCreateUserDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetCreateUserRequestDataType RequestType();
        static SmartSheetUserDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Add a new user to the Smartsheet organization",
            "type": "SmartSheetCreateUserDataProvider",
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
        return doPost("users", req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Update user action
public class SmartSheetUpdateUserDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetUpdateUserRequestDataType RequestType();
        static SmartSheetUserDataType ResponseType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update a Smartsheet user's account settings",
            "type": "SmartSheetUpdateUserDataProvider",
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
        string user_id = string(remove req.userId);
        return doPut("users/" + user_id, req).result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Delete user action
public class SmartSheetDeleteUserDataProvider inherits SmartSheetDataProviderBase {
    public {
        static SmartSheetDeleteUserRequestDataType RequestType();

        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Remove a user from the Smartsheet organization",
            "type": "SmartSheetDeleteUserDataProvider",
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
        return doDelete("users/" + string(req.userId));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
