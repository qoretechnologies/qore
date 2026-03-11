# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseEmailsDataProvider.qc Close emails collection data provider

/*  CloseEmailsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close emails collection data provider
public class CloseEmailsDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "emails",
            "desc": "Close emails data provider for listing and searching email activities",
            "type": "CloseEmailsDataProvider",
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
            "email_id": <DataProviderOptionInfo>{
                "display_name": "Email ID",
                "short_desc": "Get a specific email by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by email ID for single record lookup",
            },
            "lead_id": <DataProviderOptionInfo>{
                "display_name": "Lead ID",
                "short_desc": "Filter by lead ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter emails by lead ID",
            },
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Filter by user ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter emails by user ID",
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
            "create": Class::forName("CloseCreateEmailDataProvider"),
            "delete": Class::forName("CloseDeleteEmailDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "emails";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close emails data provider for listing and searching email activities";
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
        return new CloseEmailDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.email_id) {
            hash<auto> result = doGet("activity/email/" + search_options.email_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.lead_id) {
            params.lead_id = search_options.lead_id;
        }
        if (search_options.user_id) {
            params.user_id = search_options.user_id;
        }

        list<hash<auto>> all_records = fetchAllPages("activity/email", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
