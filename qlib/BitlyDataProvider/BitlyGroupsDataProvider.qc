# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyGroupsDataProvider.qc Bitly groups collection data provider

/*  BitlyGroupsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly groups collection data provider
public class BitlyGroupsDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "groups",
            "desc": "Bitly groups data provider for listing and retrieving groups",
            "type": "BitlyGroupsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "update": Class::forName("BitlyUpdateGroupDataProvider"),
        };

        #! Search options
        const SearchOptions = {
            "group_guid": <DataProviderOptionInfo>{
                "display_name": "Group GUID",
                "short_desc": "Get a specific group by GUID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The group GUID to retrieve. When provided, returns a single group",
                "ref_data": "groups",
            },
            "organization_guid": <DataProviderOptionInfo>{
                "display_name": "Organization GUID",
                "short_desc": "Filter by organization",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter groups by organization GUID",
                "ref_data": "organizations",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "groups";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly groups data provider for listing and retrieving groups";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new BitlyGroupDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If group_guid provided, get single record
        if (search_options.group_guid) {
            hash<auto> result = doGet("groups/" + search_options.group_guid);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.organization_guid) {
            params.organization_guid = search_options.organization_guid;
        }

        hash<auto> response = doGet("groups", params);
        list<hash<auto>> records = response.groups ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
