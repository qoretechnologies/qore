# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SystemeIoMembershipsDataProvider.qc Systeme.io memberships collection data provider

/*  SystemeIoMembershipsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io memberships collection data provider
public class SystemeIoMembershipsDataProvider inherits SystemeIoDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "memberships",
            "desc": "Systeme.io memberships data provider for listing and searching memberships",
            "type": "SystemeIoMembershipsDataProvider",
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
                "display_name": "Membership ID",
                "short_desc": "Get a specific membership by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The membership ID to retrieve. When provided, returns a single record",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
                "preselected": True,
            },
            "order": <DataProviderOptionInfo>{
                "display_name": "Order",
                "short_desc": "Sort order",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
        };

        const ChildMap = {
            "create": Class::forName("SystemeIoCreateMembershipDataProvider"),
            "delete": Class::forName("SystemeIoDeleteMembershipDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SystemeIoDataProviderBase(rest) {
    }

    string getName() {
        return "memberships";
    }

    *string getDesc() {
        return "Systeme.io memberships data provider for listing and searching memberships";
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
        return new SystemeIoMembershipDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("community/memberships/" + string(search_options.id));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.order) {
            params.order = search_options.order;
        }
        list<auto> records = doPaginatedGet("community/memberships", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SystemeIoDataProvider
