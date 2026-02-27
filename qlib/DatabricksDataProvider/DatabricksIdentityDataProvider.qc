# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksIdentityDataProvider class definition

/*  DatabricksIdentityDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks identity collection data provider (SCIM API)
public class DatabricksIdentityDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "identity",
            "desc": "Databricks identity data provider; list users, groups, and service principals "
                "via the SCIM API",
            "type": "DatabricksIdentityDataProvider",
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
            "resource_type": <DataProviderOptionInfo>{
                "display_name": "Resource Type",
                "short_desc": "Type of identity resource to list",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The type of identity resource to list (required)",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "users",
                        "desc": "List workspace users",
                    },
                    <AllowedValueInfo>{
                        "value": "groups",
                        "desc": "List workspace groups",
                    },
                    <AllowedValueInfo>{
                        "value": "service-principals",
                        "desc": "List service principals",
                    },
                ),
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "SCIM filter expression",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "A SCIM filter expression to filter results "
                    "(e.g., `displayName co \"john\"`)",
            },
            "startIndex": <DataProviderOptionInfo>{
                "display_name": "Start Index",
                "short_desc": "1-indexed start position for pagination",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "The 1-indexed start position for SCIM pagination",
            },
            "count": <DataProviderOptionInfo>{
                "display_name": "Count",
                "short_desc": "Maximum results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return per page",
            },
        };

        #! SCIM resource type to API path mapping
        const ScimPathMap = {
            "users": "api/2.0/preview/scim/v2/Users",
            "groups": "api/2.0/preview/scim/v2/Groups",
            "service-principals": "api/2.0/preview/scim/v2/ServicePrincipals",
        };
    }

    private {
        #! Record type for users
        static DatabricksUserDataType userRecordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "identity";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks identity data provider; list users, groups, and service principals "
            "via the SCIM API";
    }

    #! Returns the record type
    /** @note Returns user record type fields for "users" resource_type, \c NOTHING for others
        since the schema varies
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        if (search_options.resource_type == "users") {
            return userRecordType.getFields();
        }
        return;
    }

    #! Searches for identity resources using the SCIM API
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        *string api_path = ScimPathMap{search_options.resource_type};
        if (!api_path) {
            throw "DATABRICKS-SEARCH-ERROR",
                sprintf("unknown resource_type %y; expected: users, groups, or service-principals",
                    search_options.resource_type);
        }

        # Build query parameters for SCIM pagination
        hash<auto> params;
        if (search_options.filter) {
            params.filter = search_options.filter;
        }
        if (search_options.startIndex) {
            params.startIndex = search_options.startIndex;
        }
        if (search_options.count) {
            params.count = search_options.count;
        }

        # SCIM uses startIndex + count + totalResults for pagination (1-indexed)
        list<auto> all_records = ();
        int start_index = search_options.startIndex ?? 1;
        # If user specified count, use it as total limit; otherwise fetch all
        *int user_limit = search_options.count;
        int page_size = user_limit ?? 100;

        while (True) {
            # Adjust page size if we're near the user limit
            int fetch_count = page_size;
            if (user_limit) {
                int remaining = user_limit - all_records.lsize();
                if (remaining <= 0) {
                    break;
                }
                if (fetch_count > remaining) {
                    fetch_count = remaining;
                }
            }
            params.startIndex = start_index;
            params.count = fetch_count;
            hash<auto> response = doGet(api_path, params);
            list<auto> resources = response.Resources ?? ();
            if (!resources) {
                break;
            }
            all_records += resources;
            # Stop if user limit reached
            if (user_limit && all_records.lsize() >= user_limit) {
                break;
            }
            int total_results = response.totalResults ?? 0;
            start_index += resources.size();
            if (start_index > total_results) {
                break;
            }
        }

        # Use typed record fields for users, generic for others
        *hash<string, AbstractDataField> fields;
        if (search_options.resource_type == "users") {
            fields = userRecordType.getFields();
        }
        return new DefaultRecordIterator(new ListHashIterator(all_records),
            where_cond, search_options, fields);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
