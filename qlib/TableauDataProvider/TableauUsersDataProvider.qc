# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauUsersDataProvider.qc Tableau users collection data provider

/*  TableauUsersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Tableau users collection data provider
public class TableauUsersDataProvider inherits TableauDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "users",
            "desc": "Tableau users data provider for listing and searching users",
            "type": "TableauUsersDataProvider",
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
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Get a specific user by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by user ID for single record lookup",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter users by name",
            },
            "site_role": <DataProviderOptionInfo>{
                "display_name": "Site Role",
                "short_desc": "Filter by site role",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter users by site role",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Creator", "display_name": "Creator"},
                    <AllowedValueInfo>{"value": "Explorer", "display_name": "Explorer"},
                    <AllowedValueInfo>{"value": "ExplorerCanPublish", "display_name": "Explorer (Can Publish)"},
                    <AllowedValueInfo>{"value": "SiteAdministratorCreator", "display_name": "Site Administrator Creator"},
                    <AllowedValueInfo>{"value": "SiteAdministratorExplorer", "display_name": "Site Administrator Explorer"},
                    <AllowedValueInfo>{"value": "Unlicensed", "display_name": "Unlicensed"},
                    <AllowedValueInfo>{"value": "Viewer", "display_name": "Viewer"},
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
            "create": Class::forName("TableauCreateUserDataProvider"),
            "update": Class::forName("TableauUpdateUserDataProvider"),
            "delete": Class::forName("TableauDeleteUserDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "users";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Tableau users data provider for listing and searching users";
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
        return new TableauUserDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If user_id provided, get single record
        if (search_options.user_id) {
            hash<auto> result = doGet("users/" + search_options.user_id);
            return new DefaultRecordIterator(new ListHashIterator((result.user,)),
                where_cond, search_options);
        }

        # Build query params with Tableau filter syntax
        hash<auto> params;
        list<string> filters;
        if (search_options.name) {
            filters += "name:eq:" + search_options.name;
        }
        if (search_options.site_role) {
            filters += "siteRole:eq:" + search_options.site_role;
        }
        if (filters) {
            params.filter = filters.join(",");
        }

        list<hash<auto>> all_records = fetchAllPages("users", "users.user",
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
