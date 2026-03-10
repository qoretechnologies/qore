# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapContactsDataProvider.qc Keap contacts collection data provider

/*  KeapContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Keap contacts collection data provider
public class KeapContactsDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Keap contacts data provider for listing and searching contacts",
            "type": "KeapContactsDataProvider",
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
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Get a specific contact by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by contact ID for single record lookup",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by email address",
            },
            "given_name": <DataProviderOptionInfo>{
                "display_name": "Given Name",
                "short_desc": "Filter by first name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by given (first) name",
            },
            "family_name": <DataProviderOptionInfo>{
                "display_name": "Family Name",
                "short_desc": "Filter by last name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by family (last) name",
            },
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Field to order results by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to order results by",
            },
            "order_direction": <DataProviderOptionInfo>{
                "display_name": "Order Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction for results",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "ASCENDING",
                        "desc": "Sort in ascending order",
                    },
                    <AllowedValueInfo>{
                        "value": "DESCENDING",
                        "desc": "Sort in descending order",
                    },
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
            "create": Class::forName("KeapCreateContactDataProvider"),
            "update": Class::forName("KeapUpdateContactDataProvider"),
            "delete": Class::forName("KeapDeleteContactDataProvider"),
            "merge": Class::forName("KeapMergeContactsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap contacts data provider for listing and searching contacts";
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
        return new KeapContactDataType();
    }

    public {
        #! Optional contact properties to request from the API
        /** These fields are not returned by default and must be explicitly requested
            via the `optional_properties` query parameter
        */
        const OptionalContactProperties = (
            "email_addresses", "phone_numbers",
            "job_title", "website", "spouse_name", "preferred_name",
            "preferred_locale", "source_type",
            "owner_id", "middle_name", "prefix", "suffix", "time_zone",
        );
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build optional_properties query string fragment
        string opt_props_qs = (map sprintf("optional_properties=%s", $1),
            OptionalContactProperties).join("&");

        # If contact_id provided, get single record
        if (search_options.contact_id) {
            hash<auto> result = doRestRequest("GET",
                "contacts/" + search_options.contact_id + "?" + opt_props_qs);
            result = transformContactResponse(result);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (search_options.given_name) {
            params.given_name = search_options.given_name;
        }
        if (search_options.family_name) {
            params.family_name = search_options.family_name;
        }
        if (search_options.order_by) {
            params.order_by = search_options.order_by;
        }
        if (search_options.order_direction) {
            params.order_direction = search_options.order_direction;
        }

        list<hash<auto>> all_records = fetchAllPages("contacts", "contacts", params,
            search_options.limit, opt_props_qs);
        # Transform each record to extract email and phone
        list<hash<auto>> transformed = map transformContactResponse($1), all_records;
        return new DefaultRecordIterator(new ListHashIterator(transformed), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
